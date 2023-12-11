# Minishell
## _Implemented in C_
*Created by myself and @DarkS34 for our 'Operating Systems' course at University using Ubuntu 22.04*
> [!NOTE]  
> This minishell is not natively executable in Windows. Is a simple UNIX command line shell implemented in C runnable in Linux systems.

This shell supports basic command execution, input/output redirection, piping, execution of multiple commands in a single line, background process handling, signal handling (including Ctrl+C interrupt), and job monitoring. It provides a foundational understanding of process management on UNIX-like systems.
| Command |  |
| ------ | ------ |
| cd | Changes the current working directory. |
| exit | Exits the shell. |
| umask | Sets or displays the default file permissions. |
| jobs | Lists background jobs. |
| fg | Brings a background job to the foreground. |
## Running the minishell

To execute the minishell, clone this repository in your UNIX-based system () and execute:

```sh
gcc -Wall -Wextra myshell.c libparser.a -o test
```

This will generate an executable file named 'test' that you have to run in your Linux terminal as:
```sh
./test
```

Minishell will let you know when is executing as it shows the next line:
```sh
msh:~<directories/actual_directory>$
```

[!TIP]
The code comments are in Spanish
## How does the code work?
#### Initialization
- Global variable declaration includes a structure for background processes with fields like process ID, job number, and command string.
- 'MAX_BACKGROUND_JOBS' limits simultaneous background processes.
- Main function declares 'buf' and dynamic variables based on input commands separated by pipes, saving memory.

#### Signal Management
- Custom SIGINT and SIGTSTP handling: SIGINT interrupts current execution and re-displays the command line, while SIGTSTP is ignored.

#### User Input
- Reads user input line by line and tokenizes commands using 'tokenize()' from 'parser.h'.

#### Command Interpretation
- Checks terminated background processes, handles Ctrl + C interruption, verifies command existence, and identifies internal commands like exit, cd, jobs, fg, and umask.

#### Command Execution
- Executes single commands, processes two commands separated by pipes, and handles compound commands (with pipes) by managing multiple child processes.

#### Background Implementation
- Utilizes a 'BackgroundJob' structure to manage background processes, maintaining integrity and allowing continued execution without waiting for completion.

#### Jobs and FG Implementation
- 'jobs' command displays running background processes.
- 'fg' command moves a background process to the foreground based on job number or process ID.

This shell supports basic command execution, input/output redirection, piping, and background process handling, providing a foundational understanding of UNIX-like systems' process management. 
