#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <signal.h>

#define INITIAL_BUFFER_SIZE 1024
#define PATH_MAX 4096
#define MAX_PATHS 128

// Function declarations
void execute_command(char *command);
void execute_parallel_commands(char *command_line);
void print_error_and_exit();
void change_directory(char *path);
void update_paths(char **new_paths, int num_new_paths);
void handle_path(char **args, int num_args);
void handle_redirection(char **args, int num_args);
char **split_line(char *line, int *num_args);
void free_args(char **args, int num_args);
char* find_character(char **str, char target);
void interactive_mode();
void batch_mode(const char *filename);


char *directories[MAX_PATHS];
int num_paths = 0;

volatile sig_atomic_t stop;

void handle_sigint(int sig) {
    (void)sig;
    stop = 1;
}

void print_error_and_exit() {
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
    exit(0);
}

// cd command
void change_directory(char *path) {
    if(path == NULL) {
        print_error_and_exit();
    } else if (chdir(path) != 0) {
        print_error_and_exit();
    }
}

// path command, overites current path list with newly defined paths
void update_paths(char **new_paths, int num_new_paths) {
    // free the old paths
    for (int i = 0; i < num_paths; i++) {
        free(directories[i]);
    }
    num_paths = 0;
    directories[num_paths++] = strdup("/bin");
    if(directories[0] == NULL) {
        print_error_and_exit();
    }

    if(num_new_paths > 0) {
        // Add paths specified by user
        for(int i = 0; i < num_new_paths; i++) {
        directories[num_paths] = strdup(new_paths[i]);
        if(directories[num_paths] == NULL) {
            print_error_and_exit();
        }
        num_paths++;
    }
    }
}

//handle path command
void handle_path(char **args, int num_args) {
    if ( num_args == 1) {
        update_paths(NULL, 0);
    } else {
        //Overwrite the global paths list with new paths
        update_paths(args+1, num_args -1);
    }
}

// handle redirection
void handle_redirection(char **args, int num_args) {
    int fd;
    pid_t pid;
    int i;
    char *output_file = NULL;
    char **command_args;
    int command_len = 0;
    bool redirection_found = false;

    // Identify the redirection operator and the output files
    for(int i = 0; i < num_args; i++) {
        if(strcmp(args[i], ">") == 0) {
            if(redirection_found) {
                // Error: Multiple ">" found
                print_error_and_exit();
            }
            if(i + 1 < num_args) {
                output_file = args[i + 1];
            } else {
                print_error_and_exit();
            }

            if(i + 2 < num_args && args[i + 2] != NULL && strcmp(args[i + 2], ">") != 0) {
                print_error_and_exit();
            }
            
            redirection_found = true;
            command_len = i;
        }
    }

    if(!redirection_found || output_file == NULL || command_len == 0) {
        print_error_and_exit();
    }

    // Split command from redirection pat
    command_args = malloc(sizeof(char *) * (command_len + 1));
    if(command_args == NULL) {
        print_error_and_exit();
    }

    for(int i = 0; i < command_len; i++) {
        command_args[i] = args[i];
    }
    command_args[command_len] = NULL;

    // Fork and execute command
    pid = fork();
    if(pid == 0) {
        fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if(fd < 0) {
            print_error_and_exit();
        }

        // Redirect errors and such to this file
        if(dup2(fd, STDOUT_FILENO) < 0 ) {
            close(fd);
            print_error_and_exit();
        }

        close(fd);

        //find executable * execute
        char executable_path[PATH_MAX];
        bool found = false;

        for(int i = 0; i < num_args; i++) {
            snprintf(executable_path, sizeof(executable_path), "%s/%s", directories[i], command_args[0]);
            if(access(executable_path, X_OK) == 0) {
                found = true;
                break;
            }
        }

        if(!found) {
            print_error_and_exit();
        }

        execv(executable_path, command_args);
        print_error_and_exit();
    } else if(pid < 0) {
        // Fork failed
        print_error_and_exit();
    } else {
        int status;
        waitpid(pid, &status, 0);
    }

    free(command_args);

}

// Function to split a line into arguments
char **split_line(char *line, int *num_args) {
    char **args = NULL;
    char *token;
    int size = 0;
    int buffer_size = INITIAL_BUFFER_SIZE;

    args = malloc(sizeof(char *) * buffer_size);
    if (args == NULL) {
        print_error_and_exit();
    }

    char *line_copy = strdup(line);
    if (line_copy == NULL) {
        free(args);
        print_error_and_exit();
    }
    char *line_ptr = line_copy;

    while ((token = strsep(&line_ptr, " \t\r\n")) != NULL) {
        // Check for redirection symbols (">") without spaces and split them accordingly
        if (strchr(token, '>') != NULL && strlen(token) > 1) {
            char *redir = strchr(token, '>');
            
            if (redir == token) {
                // ">" is at the start, split it off
                args[size++] = strdup(">");
                if (redir + 1 != '\0') {
                    args[size++] = strdup(redir + 1);
                }
            } else {
                // ">" is in the middle or end
                char *before = strndup(token, redir - token);
                args[size++] = before;
                args[size++] = strdup(">");
                if (*(redir + 1) != '\0') {
                    args[size++] = strdup(redir + 1);
                }
            }
        } else if (token[0] != '\0') {
            // Regular token
            if (size >= buffer_size) {
                buffer_size *= 2;
                char **temp = realloc(args, sizeof(char *) * buffer_size);
                if (temp == NULL) {
                    free(args);
                    free(line_copy);
                    print_error_and_exit();
                }
                args = temp;
            }
            args[size] = strdup(token);
            if (args[size] == NULL) {
                free(args);
                free(line_copy);
                print_error_and_exit();
            }
            size++;
        }
    }

    char **temp = realloc(args, sizeof(char *) * (size + 1));
    if (temp == NULL) {
        free(args);
        free(line_copy);
        print_error_and_exit();
    }
    args = temp;
    args[size] = NULL;
    *num_args = size;

    free(line_copy);
    return args;
}


// Split by delimiter, used to split by '&' in command line
char **split_by_delimeter(char *line, char delimeter, int *num_tokens) {
    char **tokens = NULL;
    char *token;
    int size = 0;
    int buffer_size = INITIAL_BUFFER_SIZE;

    tokens = malloc(sizeof(char *) * buffer_size);
    if (tokens == NULL) {
        print_error_and_exit();
    }

    char *line_copy = strdup(line);
    if (line_copy == NULL) {
        free(tokens);
        print_error_and_exit();
    }

    char *line_ptr = line_copy;

    // Tokenize the line using the delimiter
    while ((token = strsep(&line_ptr, &delimeter)) != NULL) {
        // Handle multiple delimiters in a row
        if (token[0] != '\0') {
            // Trim leading and trailing whitespaces
            while (*token == ' ' || *token == '\t') token++;
            char *end = token + strlen(token) - 1;
            while (end > token && (*end == ' ' || *end == '\t')) {
                *end = '\0';
                end--;
            }
            if (size >= buffer_size) {
                buffer_size *= 2;
                char **temp = realloc(tokens, sizeof(char *) * buffer_size);
                if (temp == NULL) {
                    free_args(tokens, size);
                    free(line_copy);
                    print_error_and_exit();
                }
                tokens = temp;
            }
            tokens[size] = strdup(token);
            if (tokens[size] == NULL) {
                free_args(tokens, size);
                free(line_copy);
                print_error_and_exit();
            }
            size++;
        }
    }

    char **temp = realloc(tokens, sizeof(char *) * (size + 1));
    if (temp == NULL) {
        free_args(tokens, size);
        free(line_copy);
        print_error_and_exit();
    }
    tokens = temp;
    tokens[size] = NULL;
    *num_tokens = size;

    free(line_copy);
    return tokens;
}

// Function to free the allocated arguments
void free_args(char **args, int num_args) {
    for (int i = 0; i < num_args; i++) {
        free(args[i]);
    }
    free(args);
}

// Find character function
char* find_character(char **str, char target) {
    if (str == NULL) {
        return NULL;  // Handle NULL input
    }

    int i = 0;

    // Loop through the array of strings
    while (str[i] != NULL) {  // Continue until a NULL pointer is found in the array
        char *current_str = str[i];
        
        // Loop through each character in the current string
        while (*current_str != '\0') {
            if (*current_str == target) {
                return current_str;  // Return pointer to the found character
            }
            current_str++;
        }
        i++;
    }

    return NULL;  // Return NULL if the character is not found in any string
}


// Function to execute a command
void execute_command(char *command) {
    int num_args;
    char **args = split_line(command, &num_args);

    if (args == NULL || args[0] == NULL) {
        free_args(args, num_args);
        print_error_and_exit();
    }

    // find '&'
    char *and_pos = find_character(args, '&');
    if (and_pos != NULL) {
        char *command_line = strdup(command);
        if(command_line == NULL) {
            free_args(args, num_args);
            print_error_and_exit();
        }
        execute_parallel_commands(command_line);
        free(command_line);
        free_args(args, num_args);
        return;
    }

    //run through arguments list to check if ">" character was used and process this
    for(int i =0; i < num_args; i++) {
        if(strcmp(args[i], ">") == 0) {
            handle_redirection(args, num_args);
            free_args(args, num_args);
            return;
        }
    }

    //handle "cd" command
    if(strcmp(args[0], "cd") == 0) {
        change_directory(args[1]);
        free_args(args, num_args);
        return;
    }

    //handle "path" command
    if(strcmp(args[0], "path") == 0) {
        handle_path(args, num_args);
        free_args(args, num_args);
        return;
    }

    // execute normal command without redirection
    char executable_path[PATH_MAX];
    bool found = false;

    for (int i =0; i < num_paths; i++) {
        snprintf(executable_path, sizeof(executable_path), "%s/%s", directories[i], args[0]);
        if (access(executable_path, X_OK) == 0) {
            found = true;
            break;
        }
    }

    if (!found) {
        free_args(args, num_args);
        print_error_and_exit();
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        if (execv(executable_path, args) == -1) {
            // Specific command error handling
            if (strcmp(args[0], "ls") == 0) {
                // Print specific error message for ls
                perror("ls");
            } else {
                // Print generic error message for other commands
                print_error_and_exit();
            }
            exit(EXIT_FAILURE); // Exit child process
        }
    } else if (pid < 0) {
        // Fork failed
        print_error_and_exit();
    } else {
        // Parent process: wait for the child process to finish
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            // Check if the command was `ls` and if so, do not print generic error
            if (strcmp(args[0], "ls") != 0) {
                print_error_and_exit(); // Print the generic error message for non-'ls' commands
            }
        }
    }

    free_args(args, num_args);
}

// Function to execute a command in parallel
void execute_parallel_commands(char *command_line) {
    int num_commands = 0;
    char **commands =split_by_delimeter(command_line, '&', &num_commands);
    pid_t *pids = malloc(sizeof(pid_t) * num_commands);
    if(pids == NULL) {
        free_args(commands, num_commands);
        print_error_and_exit();
    }

    for( int i = 0; i < num_commands; i++) {
        if(commands[i] == NULL || strlen(commands[i]) == 0) continue; //Skip empty commands

        pids[i] = fork();
        if(pids[i] == 0) {
            // Child process executes the command
            execute_command(commands[i]);
            exit(0);
        } else if(pids[i] < 0) {
            free(pids);
            free_args(commands, num_commands);
            print_error_and_exit();
        }
    }

    // Parent process: Wait for all children to complete
    for (int i = 0; i < num_commands; i++) {
        if(pids[i] > 0) {
            waitpid(pids[i], NULL, 0);
        }
    }

    free(pids);
    free_args(commands, num_commands);
}


void interactive_mode() {
    char *line = NULL;
    size_t len = 0;

    while (true) {
        printf("witshell> ");
        fflush(stdout);

        if (getline(&line, &len, stdin) == -1) {
            break; // Exit on EOF
        }

        line[strcspn(line, "\n")] = '\0';

        // Skip lines with only "&"
        if (strcmp(line, "&") == 0) {
            continue;
        }

        if (strcmp(line, "exit") == 0) {
            free(line);
            exit(0);
        }

        execute_command(line);
    }

    free(line);
}

void sigchld_handler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void batch_mode(const char *filename) {
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        char error_message[30] = "An error has occurred\n";
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }

    FILE *file = fopen(filename, "r");
    if (!file) {
        char error_message[30] = "An error has occurred\n";
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }

    char *line = NULL;
    size_t len = 0;
    int only_ampersand = 1;
    while (getline(&line, &len, file) != -1) {
        // Remove trailing newline character
        size_t length = strlen(line);
        if (length > 0 && line[length - 1] == '\n') {
            line[length - 1] = '\0';
        }

        // Skip empty lines or lines with only whitespace
        if (strlen(line) == 0 || strspn(line, " \t\r\n") == strlen(line)) {
            continue;
        }

        // Check if the line is only "&"
        if (strcmp(line, "&") == 0 || strspn(line, " \t\r\n") == strlen(line)) {
            continue; // Skip this line
        }

        only_ampersand = 0; // Found a valid command

        // Check if the user typed "exit"
        if (strcmp(line, "exit") == 0) {
            free(line);
            fclose(file);
            exit(0);
        }
        
        execute_command(line);
    }

    if (stop) {
        printf("\nBatch mode interrupted.\n");
    }

    free(line);
    fclose(file);
}


int main(int MainArgc, char *MainArgv[]) {
    //Initialize with a default path
    directories[0] = strdup("/bin");
    num_paths = 1;


    if (MainArgc > 2) {
        print_error_and_exit();
    }

    if (MainArgc == 2) {
        batch_mode(MainArgv[1]);
    } else {
        interactive_mode();
    }

    return EXIT_SUCCESS;
}