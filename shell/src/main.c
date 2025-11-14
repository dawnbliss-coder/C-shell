#include "shell.h"
#include "cfg.h"
#include "pipeline.h"

#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <ctype.h>
#include <signal.h>

// Globals used to track background processes
process processes[100];
int process_count = 0;
int next_job_number = 1;
pid_t foreground_pgid = 0;

pid_t current_foreground_pid = 0;
char current_foreground_command[256] = "";

int main() {
    char home_dir[PATH_MAX];
    if (getcwd(home_dir, sizeof(home_dir)) == NULL) {
        perror("getcwd");
        return 1;
    }

    setup_signal_handlers();
    init_builtin_state(home_dir);

    load_history();

    char input[1024];
    while (1) {
        // Check for completed background processes and update their status
        completed_processes();

        // Display the shell prompt
        show_prompt();

        // Read user input
        if (fgets(input, sizeof(input), stdin) == NULL) {
            save_history();
            // End of file (Ctrl-D) handling
            for (int i = 0; i < process_count; i++) {
                kill(processes[i].pid, SIGKILL);
            }
            printf("\nlogout\n");
            break;
        }

        // Remove the newline character from the input
        input[strcspn(input, "\n")] = '\0';

        if (strlen(input) == 0) {
            continue;
        }

        char *command_start = input;
        char *command_end = input;

        // Loop to handle multiple commands separated by ';'
        while (*command_end != '\0') {
            while (*command_end != ';' && *command_end != '\0') {
                command_end++;
            }

            int bg = 0;
            // Check for background command ('&')
            if (*(command_end - 1) == '&') {
                bg = 1;
                *(command_end - 1) = '\0'; // Remove '&'
            }

            char temp_char = *command_end;
            *command_end = '\0';

            // Trim leading whitespace
            while (isspace((unsigned char)*command_start)) {
                command_start++;
            }
            // Trim trailing whitespace
            char *temp = command_start + strlen(command_start) - 1;
            while (isspace((unsigned char)*temp) && temp >= command_start) {
                *temp = '\0';
                temp--;
            }

            if (strlen(command_start) > 0) {
                update_history(command_start);
                // Parse the command and arguments
                char **args = parse_command(command_start);
                if (args == NULL) {
                    fprintf(stderr, "Invalid Syntax!\n");
                } else {
                    if (args[0] != NULL) {
                        run_builtin_or_external(args, bg);
                    }
                    free_args(args);
                }
            }
            *command_end = temp_char;
            command_start = command_end + 1;
            command_end = command_start;
        }
    }

    save_history();

    return 0;
}
