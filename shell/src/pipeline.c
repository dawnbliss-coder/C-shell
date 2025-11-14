// ############## LLM Generated Code Begins ##############
#include "pipeline.h"
#include "shell.h"
#include "cfg.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h> 
#include <Kernel/sys/fcntl.h>

void execute_builtin_in_pipeline(char **args) {
    if (strcmp(args[0], "reveal") == 0) {
        handle_reveal(args + 1);
    } else if (strcmp(args[0], "hop") == 0) {
        handle_hop(args + 1);
    } else if (strcmp(args[0], "log") == 0) {
        handle_log(args + 1);
    } else if (strcmp(args[0], "activities") == 0) {
        run_activities_builtin(processes, process_count);
    } else if (strcmp(args[0], "ping") == 0) {
        handle_ping(args + 1);
    } else if (strcmp(args[0], "true") == 0) {
        exit(0);
    } else if (strcmp(args[0], "false") == 0) {
        exit(1);
    } else {
        // Not a builtin, try execvp
        execvp(args[0], args);
        fprintf(stderr, "Command not found!\n");
        exit(EXIT_FAILURE);
    }
}

// Executes a command group with pipelines and redirection
void execute_command_group(char **args) {
    int pipe_fd[2];
    int input_fd = STDIN_FILENO;
    int output_fd = STDOUT_FILENO;
    int i, start_index = 0;
    pid_t pid;
    
    // Create a copy of args to modify for redirection parsing
    // Count total args first
    int arg_count = 0;
    while (args[arg_count] != NULL) arg_count++;
    
    // Create new args array
    char **new_args = malloc((arg_count + 1) * sizeof(char*));
    if (new_args == NULL) {
        perror("malloc");
        return;
    }
    
    // Copy all arguments initially
    for (i = 0; i <= arg_count; i++) {
        new_args[i] = args[i];
    }
    
    // Find I/O redirection points - scan from right to left for output redirection
    // This ensures the last redirection takes effect
    int last_output_redirect = -1;
    int last_input_redirect = -1;
    
    // First pass: find the last output and input redirections
    for (i = 0; new_args[i] != NULL; i++) {
        if (strcmp(new_args[i], ">") == 0 || strcmp(new_args[i], ">>") == 0) {
            last_output_redirect = i;
        } else if (strcmp(new_args[i], "<") == 0) {
            last_input_redirect = i;
        }
    }
    // Handle input redirection
    if (last_input_redirect != -1) {
        if (new_args[last_input_redirect + 1] == NULL) {
            printf("No such file or directory\n");
            free(new_args);
            return;
        }
        input_fd = open(new_args[last_input_redirect + 1], O_RDONLY);
        if (input_fd < 0) {
            printf("No such file or directory\n");
            free(new_args);
            return;
        }
        // Remove input redirection from args
        new_args[last_input_redirect] = NULL;
        new_args[last_input_redirect + 1] = NULL;
    }
    
    // Handle output redirection (only the last one)
    if (last_output_redirect != -1) {
        if (new_args[last_output_redirect + 1] == NULL) {
            printf("Unable to create file for writing\n");
            if (input_fd != STDIN_FILENO) close(input_fd);
            free(new_args);
            return;
        }
        
        if (strcmp(new_args[last_output_redirect], ">") == 0) {
            output_fd = open(new_args[last_output_redirect + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        } else if (strcmp(new_args[last_output_redirect], ">>") == 0) {
            output_fd = open(new_args[last_output_redirect + 1], O_WRONLY | O_CREAT | O_APPEND, 0644);
        }
        
        if (output_fd < 0) {
            printf("Unable to create file for writing\n");
            if (input_fd != STDIN_FILENO) close(input_fd);
            free(new_args);
            return;
        }
        
        // Remove output redirection from args
        new_args[last_output_redirect] = NULL;
        new_args[last_output_redirect + 1] = NULL;
    }
    
    // Now remove all other redirection operators and their arguments
    for (i = 0; new_args[i] != NULL; i++) {
        if (strcmp(new_args[i], ">") == 0 || strcmp(new_args[i], ">>") == 0 || strcmp(new_args[i], "<") == 0) {
            new_args[i] = NULL;
            if (new_args[i + 1] != NULL) {
                new_args[i + 1] = NULL;
                i++; // Skip the filename too
            }
        }
    }
    
    // Compact the args array by removing NULL entries
    int write_pos = 0;
    for (i = 0; i < arg_count; i++) {
        if (new_args[i] != NULL) {
            if (write_pos != i) {
                new_args[write_pos] = new_args[i];
            }
            write_pos++;
        }
    }
    new_args[write_pos] = NULL;
    
    // Handle pipeline execution
    start_index = 0;
    int prev_pipe_read = STDIN_FILENO;
    
    for (i = 0; new_args[i] != NULL; i++) {
        if (strcmp(new_args[i], "|") == 0) {
            new_args[i] = NULL;
            
            if (pipe(pipe_fd) < 0) {
                perror("pipe");
                if (input_fd != STDIN_FILENO) close(input_fd);
                if (output_fd != STDOUT_FILENO) close(output_fd);
                free(new_args);
                return;
            }
            
            pid = fork();
            if (pid < 0) {
                perror("fork");
                close(pipe_fd[0]);
                close(pipe_fd[1]);
                if (input_fd != STDIN_FILENO) close(input_fd);
                if (output_fd != STDOUT_FILENO) close(output_fd);
                free(new_args);
                return;
            }
            
            if (pid == 0) {
                // Child process
                //printf("..\n");
                close(pipe_fd[0]);
                if (prev_pipe_read != STDIN_FILENO) {
                    dup2(prev_pipe_read, STDIN_FILENO);
                    close(prev_pipe_read);
                } else if (input_fd != STDIN_FILENO) {
                    dup2(input_fd, STDIN_FILENO);
                    close(input_fd);
                }
                dup2(pipe_fd[1], STDOUT_FILENO);
                close(pipe_fd[1]);
                
                // Handle builtin commands
                execute_builtin_in_pipeline(new_args + start_index);
            } else {
                // Parent process
                close(pipe_fd[1]);
                if (prev_pipe_read != STDIN_FILENO) {
                    close(prev_pipe_read);
                }
                prev_pipe_read = pipe_fd[0];
                start_index = i + 1;
            }
        }
    }
    
    // Execute the last/only command
    pid = fork();
    if (pid < 0) {
        perror("fork");
        if (prev_pipe_read != STDIN_FILENO) close(prev_pipe_read);
        if (input_fd != STDIN_FILENO) close(input_fd);
        if (output_fd != STDOUT_FILENO) close(output_fd);
        free(new_args);
        return;
    }
    
    if (pid == 0) {
        // Child process - last command in pipeline
        if (prev_pipe_read != STDIN_FILENO) {
            dup2(prev_pipe_read, STDIN_FILENO);
            close(prev_pipe_read);
        } else if (input_fd != STDIN_FILENO) {
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);
        }
        
        if (output_fd != STDOUT_FILENO) {
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
        }
        
        execute_builtin_in_pipeline(new_args + start_index);
    } else {
        // Parent process - wait for completion
        if (prev_pipe_read != STDIN_FILENO) {
            close(prev_pipe_read);
        }
        if (input_fd != STDIN_FILENO) {
            close(input_fd);
        }
        if (output_fd != STDOUT_FILENO) {
            close(output_fd);
        }
        
        // Wait for all children
        int status;
        while (wait(&status) > 0);
    }
    
    free(new_args);
}

// ############## LLM Generated Code Ends ################