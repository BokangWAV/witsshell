# Simple Shell (CLI) - witsshell

## Overview

This project implements a simple command-line interpreter (CLI), similar to Unix shells like bash. The shell executes commands entered by the user, creates a child process for each command, and then prompts for more input once the command has finished executing.

## Features

- Executes user commands in a child process.
- Prompts for input after each command execution.
- Supports basic command execution similar to Unix shells.

## Installation

### Prerequisites

- C compiler (e.g., `gcc`)
- Make (if using Makefile)

### Build Instructions

1. Clone the repository:
    ```sh
    git clone https://github.com/yourusername/shell-project.git
    cd shell-project
    ```

2. Compile the project:
    ```sh
    gcc -o witsshell witsshell.c
    ```

3. Run the shell:
    ```sh
    ./witsshell
    ```

## Usage

- Type a command and press Enter to execute it.
- The shell will create a child process for each command.
- The shell will prompt you again after the command completes.

## Example

```sh
$ ls
file1.txt  file2.txt
$ echo "Hello, world!"
Hello, world!
$ exit
```

## Contributing

1. Fork the repository on GitHub.
   
2. Create a new branch (git checkout -b feature-branch).
   
3. Commit your changes (git commit -am 'Add new feature').

4. Push to the branch (git push origin feature-branch).

5. Create a pull request on GitHub.
