#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "parser.h"

typedef struct {
  pid_t pid;
  char *buffLine;
  short jobNumber;
} BackgroundJob;

#define MAX_BACKGROUND_JOBS 10

tline *line;
BackgroundJob backgroundJobs[MAX_BACKGROUND_JOBS];
int numBackgroundJobs = 0;

// Funciones auxiliares

void checkCommand() { //Comprueba que el comando existe como ejecutable y no es un comando interno
  int k;
  for (k = 0; k < line->ncommands; k++) {
    char *comando = line->commands[k].argv[0];
    if ((line->commands[k].filename == NULL) && strcmp(comando, "exit") != 0 &&
        strcmp(comando, "cd") != 0 && strcmp(comando, "umask") != 0 &&
        strcmp(comando, "jobs") != 0 && strcmp(comando, "fg") != 0) {
      printf("%s : No se encuentra el mandato.\n", comando);
    }
  }
}

void redirectInput() { //Redirige la entrada estándar desde un archivo
  int file;
  file = open(line->redirect_input, O_RDONLY);

  if (file == -1) {
    fprintf(stderr, "Error al leer el archivo %s\n", line->redirect_input);
    exit(1);
  }
  dup2(file, STDIN_FILENO);
  close(file);
}

void redirectOutput() { //Redirige la salida estándar a un archivo (estableciendo permisos de lectura y escritura en caso de creación)
  int file;
  mode_t mode = S_IRUSR | S_IWUSR;

  file = open(line->redirect_output, O_CREAT | O_WRONLY | O_TRUNC, mode);

  if (file == -1) {
    fprintf(stderr, "Error al abrir el fichero de redirección de salida %s\n",
            line->redirect_output);
    exit(1);
  }
  if (chmod(line->redirect_output, mode) != 0) {
    fprintf(stderr,
            "Fallo al establecer permisos del fichero de redireccion de salida "
            "%s\n",
            line->redirect_output);
    exit(1);
  }
  dup2(file, STDOUT_FILENO);
  close(file);
}

void redirectError() { //Redirige la salida de error a un archivo (estableciendo permisos de lectura y escritura en caso de creación)
  int file;
  mode_t mode = S_IRUSR | S_IWUSR;

  file = open(line->redirect_error, O_CREAT | O_WRONLY | O_TRUNC, mode);

  if (file == -1) {
    fprintf(stderr, "Error al abrir el fichero de redirección de error %s\n",
            line->redirect_error);
    exit(1);
  }
  if (chmod(line->redirect_output, mode) != 0) {
    fprintf(stderr,
            "Fallo al establecer permisos del fichero de redireccion "
            "de salida %s\n",
            line->redirect_output);
    exit(1);
  }

  dup2(file, STDERR_FILENO);
  close(file);
}

void printMshandDir() { // Imprime por terminal prompt de comandos (msh:~<ruta>$)
  char cwd[1024];
  char *directory;
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    directory = malloc(strlen("\x1b[32mmsh:~\x1b[0m") + strlen("\x1b[34m") +
                       strlen(cwd) + strlen("\x1b[0m") + 3);

    if (directory != NULL) {
      strcpy(directory, "\x1b[32mmsh:~\x1b[0m");
      strcat(directory, "\x1b[34m");
      strcat(directory, cwd);
      strcat(directory, "\x1b[0m");
      strcat(directory, "$ ");

      printf("%s", directory);

      free(directory);
    } else {
      fprintf(stderr, "Error: No se pudo asignar memoria.\n");
    }
  } else {
    perror("Error al obtener el directorio actual");
  }
}

void rearrangeBgJobs(int pidTerm) { //Elimina proceso en segundo plano de estructura backgroundJobs y reordena la lista si es necesario
    int i,j;
    for (i = 0; i < numBackgroundJobs; i++) {
    if (backgroundJobs[i].pid == pidTerm) {
      free(backgroundJobs[i].buffLine);
      for (j = i; j < numBackgroundJobs - 1; j++) {
        backgroundJobs[j] = backgroundJobs[j + 1];
      }
      numBackgroundJobs--;
      break;
    }
  }
}

void bgHandler() { //Comprueba si existen procesos en segundo plano que hayan finalizado para mostrarlo por terminal
  pid_t pidTerminated;
  int status;

  while ((pidTerminated = waitpid(-1, &status, WNOHANG)) > 0) {
    rearrangeBgJobs(pidTerminated);

    if (WIFEXITED(status)) {
      printf("\nProceso con pid: [%d] terminado (Código de salida: %d)\n",
             pidTerminated, WEXITSTATUS(status));
      printMshandDir();
    } else if (WIFSIGNALED(status)) {
      printf("\nProceso con pid: [%d] terminado por señal %d\n", pidTerminated,
             WTERMSIG(status));
    }
  }
}

void addBackgroundJob(pid_t pid, char *bufferComando) { //Añade un proceso en segundo plano a la estructura backgroundJobs si esta tiene espacio suficiente
  if (numBackgroundJobs < MAX_BACKGROUND_JOBS) {
    backgroundJobs[numBackgroundJobs].pid = pid;
    backgroundJobs[numBackgroundJobs].buffLine =
        malloc(strlen(bufferComando) + 1);  // Asignación de memoria dinámica
    strcpy(backgroundJobs[numBackgroundJobs].buffLine, bufferComando);
    backgroundJobs[numBackgroundJobs].jobNumber = numBackgroundJobs+1;
    numBackgroundJobs++;
  } else {
    fprintf(stderr, "Número máximo de trabajos en segundo plano alcanzado.\n");
  }
}

void customSIHandler() { //Imprime un salto de línea, el prompt personalizado de comandos y limpia el bufer de salida para que se muestre inmediatamente
  printf("\n");
  printMshandDir();
  fflush(stdout);
}

// Cuerpo

int main(void) {
  char buf[1024];
  printMshandDir();

  //Manejo de señales inicial
  signal(SIGINT, customSIHandler);
  signal(SIGTSTP, SIG_IGN);

  //Bucle principal de la minishell
  while (fgets(buf, 1024, stdin)) {
    // COMPROBAR LINEA VACIA
    if (strcmp(buf, "\n") == 0) {  
      printMshandDir();
      bgHandler();
      continue;
    }

    line = tokenize(buf);

    // COMPROBACION DE PROCESOS EN SEGUNDO PLANO
    bgHandler();

    // GESTION DE LA SEÑAL SIGINT (CTRL+C)
    signal(SIGINT, customSIHandler);

    // COMPROBAR EXISTENCIA MANDATO
    checkCommand();

    // COMANDO EXIT
    if (strcmp(line->commands[0].argv[0], "exit") == 0) {
      exit(0);
    }

    // COMANDO CD
    if (strcmp(line->commands[0].argv[0], "cd") == 0) {
      char *directorio = line->commands[0].argv[1];
      if (line->ncommands == 1){
        if (line->commands[0].argc == 1) {
          chdir(getenv("HOME"));
        } else if (line->commands[0].argc == 2) {
          if (chdir(directorio) != 0) {
            perror("Error al cambiar de directorio");
          }
        } else {
          printf("Numero de parametros incorrecto");
        }
      } else {
        printf("Comando cd no se puede ejecutar con otros comandos\n");
        printMshandDir();
        continue;
      }
    }

    // COMANDO JOBS
    if (strcmp(line->commands[0].argv[0], "jobs") == 0) {
      for (int i = 0; i < numBackgroundJobs; i++) {
        printf("[%d] \t Running \t %s", backgroundJobs[i].jobNumber, backgroundJobs[i].buffLine);
      }
    }

    // COMANDO FG
    if (strcmp(line->commands[0].argv[0], "fg") == 0) {
      int i, pidObjective, secondArg;
      if (numBackgroundJobs > 0) {
        if (line->commands->argc==1){ // FG SIN ARGUMENTOS
          pidObjective = backgroundJobs[0].pid;
          printf("%s\n",backgroundJobs[0].buffLine);
          waitpid(pidObjective,NULL,0);
          rearrangeBgJobs(pidObjective);
        }else if (line->commands->argc==2){ // FG CON 1 ARGUMENTO
          secondArg = atoi(line->commands->argv[1]);
          for (i = 0; i < numBackgroundJobs; i++){
            if(backgroundJobs[i].jobNumber==secondArg){
              pidObjective = backgroundJobs[i].pid;
              printf("%s\n",backgroundJobs[i].buffLine);
            }
          }
          waitpid(pidObjective,NULL,0);
          rearrangeBgJobs(pidObjective);
        }else{
          perror("Numero de argumentos incorrecto\n");
        }
      } else {
        perror("No hay ningun proceso en segundo plano\n");
      }
    }

    // COMANDO UMASK
    if (strcmp(line->commands[0].argv[0], "umask") == 0) {
      if (line->commands[0].argc == 1) {
        mode_t current_mask = umask(0);
        umask(current_mask);
        printf("%04o\n", current_mask);
      } else if (line->commands[0].argc == 2) {
        int octal = strtol(line->commands[0].argv[1], NULL, 8);
        mode_t mask = (mode_t)octal;
        umask(mask);
      } else {
        perror("Numero de parametros incorrecto");
      }
    }

    if (line->ncommands == 1) {  // SI SOLO HAY UN (1) COMANDO
      pid_t pid;
      
      pid = fork();
      if (pid == -1) {
        fprintf(stderr, "El fork ha fallado %s\n", strerror(errno));
        exit(EXIT_FAILURE);
      } else if (pid == 0) {
        signal(SIGINT, SIG_DFL);

        // REDIRECCION DE ENTRADA
        if (line->redirect_input != NULL) {
          redirectInput();
        }
        // REDIRECCION DE SALIDA
        if (line->redirect_output != NULL) {
          redirectOutput();
        }
        // REDIRECCION DE ERROR
        if (line->redirect_error != NULL) {
          redirectError();
        }
        
        execvp(line->commands[0].filename, line->commands[0].argv);
        perror("Error al ejecutar el comando");
        exit(EXIT_FAILURE);
      } else {
        if (!line->background) {
          waitpid(pid, NULL, 0);
          signal(SIGINT, customSIHandler);
        } else {
          signal(SIGINT, customSIHandler);
          addBackgroundJob(pid, buf);
          printf("[%d] %d\n", numBackgroundJobs, pid);
        }
      }
    } else if (line->ncommands == 2) {  // SI HAY DOS (2) COMANDOS
      int pipes[2];
      pid_t pidPrimerComando;
      pid_t pidSegundoComando;

      if (pipe(pipes) == -1) {
        fprintf(stderr, "Fallo al crear el pipe: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
      }

      pidPrimerComando = fork();

      if (pidPrimerComando == -1) {
        fprintf(stderr, "El fork ha fallado %s\n", strerror(errno));
        exit(EXIT_FAILURE);
      } else if (pidPrimerComando == 0) {
        // REDIRECCION DE ENTRADA
        if (line->redirect_input != NULL) {
          redirectInput();
        }

        close(pipes[0]);
        dup2(pipes[1], STDOUT_FILENO);
        close(pipes[1]);

        execvp(line->commands[0].filename, line->commands[0].argv);

        perror("Error al ejecutar el primer comando");
        exit(EXIT_FAILURE);
      } else {
        close(pipes[1]);
        
        pidSegundoComando = fork();

        if (pidSegundoComando == -1) {
          fprintf(stderr, "El fork ha fallado %s\n", strerror(errno));
          exit(EXIT_FAILURE);
        } else if (pidSegundoComando == 0) {
          // REDIRECCION DE SALIDA
          if (line->redirect_output != NULL) {
            redirectOutput();
          }
          // REDIRECCION DE ERROR
          if (line->redirect_error != NULL) {
            redirectError();
          }

          dup2(pipes[0], STDIN_FILENO);
          close(pipes[0]);

          execvp(line->commands[1].filename, line->commands[1].argv);

          perror("Error al ejecutar el segundo comando");
          exit(EXIT_FAILURE);
        } else {
          close(pipes[0]);

          if (!line->background) {  // SIN BACKGROUND
            waitpid(pidPrimerComando, NULL, 0);
            waitpid(pidSegundoComando, NULL, 0);
          } else {  // CON BACKGROUND
            waitpid(pidPrimerComando, NULL, 0);
            addBackgroundJob(pidSegundoComando, buf);
            printf("[%d] %d\n", numBackgroundJobs, pidSegundoComando);
          }
        }
      }
    } else {  // SI HAY MÁS DE 2 COMANDOS
      pid_t pidComando;
      int i, j;

      // RESERVA DE MEMORIA PARA UN ARRAY DE PIPES
      int **pipes = (int **)malloc((sizeof(int *) * line->ncommands-1));

      // RESERVA DE MEMORIA PARA EXTREMOS DE LECTURA Y ESCRITURA PARA CADA PIPE
      for (j = 0; j < line->ncommands - 1; j++) {
        // Reserva de memoria para la entrada y salida de cada pipe.
        pipes[j] = (int *)malloc(sizeof(int) * 2);
        // Creacion de pipes.
        if (pipe(pipes[j]) == -1) {
          fprintf(stderr, "Fallo al crear el pipe: %s\n", strerror(errno));
          exit(EXIT_FAILURE);
        }
      }

      //CREACIÓN DE PROCESOS HIJO PARA CADA COMANDO
      for (i = 0; i < line->ncommands; i++) {
        pidComando = fork();

        if (pidComando == -1) {
          fprintf(stderr, "Fallo al hacer fork: %s\n", strerror(errno));
          exit(EXIT_FAILURE);
        } else if (pidComando == 0) {
          if (i == 0) {  // PRIMER COMANDO
            if (line->redirect_input != NULL) {// Si hay redirección de entrada
              redirectInput();
            }

            close(pipes[i][0]); // Cerrar extremo de lectura del siguiente pipe
            dup2(pipes[i][1], STDOUT_FILENO); // Redirigir la salida al extremo de escritura del siguiente pipe
            close(pipes[i][1]); // Cerrar extremo de escritura del siguiente pipe

            execvp(line->commands[i].filename, line->commands[i].argv); //Ejecutar comando

            fprintf(stderr, "Error al ejecutar el primer comando\n");
            exit(EXIT_FAILURE);
          } else if (i != 0 && i != line->ncommands - 1) {    // COMANDO INTERMEDIO
            close(pipes[i - 1][1]); // Cerrar extremo de escritura del anterior pipe
            close(pipes[i][0]); // Cerrar extremo de lectura del siguiente pipe

            dup2(pipes[i - 1][0], STDIN_FILENO); // Redirigir la entrada desde el extremo de lectura del anterior pipe
            close(pipes[i - 1][0]); // Cerrar extremo de lectura del anterior pipe

            dup2(pipes[i][1], STDOUT_FILENO); // Redirigir la salida al extremo de escritura del siguiente pipe
            close(pipes[i][1]); // Cerrar extremo de escritura del siguiente pipe

            execvp(line->commands[i].filename, line->commands[i].argv); //Ejecutar comando

            fprintf(stderr, "Error al ejecutar el comando numero %d\n", i);
            exit(EXIT_FAILURE);
          } else {  // ULTIMO COMANDO
            if (line->redirect_output != NULL) { // Si hay redirección de salida
              redirectOutput();
            }
            if (line->redirect_error != NULL) { // Si hay redirección de salida de error
              redirectError();
            }

            close(pipes[i - 1][1]); // Cerrar extremo de escritura del siguiente pipe

            dup2(pipes[i - 1][0], STDIN_FILENO); // Redirigir la entrada desde el extremo de lectura del anterior pipe
            close(pipes[i - 1][0]); //Cerrar extremo de lectura del anterior pipe

            execvp(line->commands[i].filename, line->commands[i].argv); // Ejecutar comando

            fprintf(stderr, "Error al ejecutar el ultimo comando\n");
            exit(EXIT_FAILURE);
          }
        } else {
          if (i != line->ncommands - 1) { //Si no es el último comando
            close(pipes[i][1]); //Cerrar extremo de escritura del siguiente pipe
          }
          if (!line->background) { // Si NO es un comando a ejecutar en segundo plano
            waitpid(pidComando, NULL, 0); 
          } else { // Si es un comando a ejecutar en segundo plano
            if (i != line->ncommands - 1) { //Si no es el último comando
              waitpid(pidComando, NULL, 0);
            } else { //Si es el último comando
              addBackgroundJob(pidComando, buf);
              printf("[%d] %d\n", numBackgroundJobs, pidComando);
            }
          }
        }
      }
      free(pipes); //Liberar memoria del array de pipes
    }

    printMshandDir();
  }

  return 0;
}