#include "shell.h"
#include "cfg.h"
#include "pipeline.h"

#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>

char *history[15];
int his_cnt = 0;

pid_t shell_pgid;

extern pid_t current_foreground_pid;
extern char current_foreground_command[256];

#define HISTORY_FILE ".shell_history"

char *shell_home_dir = NULL;
char *prev_dir = NULL;

void load_history() {
    char history_path[PATH_MAX];
    snprintf(history_path, sizeof(history_path), "%s/%s", shell_home_dir, HISTORY_FILE);
    
    FILE *file = fopen(history_path, "r");
    if (file == NULL) {
        return; // No history file exists yet
    }
    
    char line[1024];
    while (fgets(line, sizeof(line), file) != NULL && his_cnt < 15) {
        // Remove newline
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) > 0) {
            history[his_cnt++] = strdup(line);
        }
    }
    fclose(file);
}

void save_history() {
    char history_path[PATH_MAX];
    snprintf(history_path, sizeof(history_path), "%s/%s", shell_home_dir, HISTORY_FILE);
    
    FILE *file = fopen(history_path, "w");
    if (file == NULL) {
        return; // Can't save history
    }
    
    for (int i = 0; i < his_cnt; i++) {
        fprintf(file, "%s\n", history[i]);
    }
    fclose(file);
}

void init_builtin_state(char *home_path) {
    if (shell_home_dir == NULL) {
        shell_home_dir = strdup(home_path);
    }
    prev_dir = NULL;
}

void free_args(char **args) {
    if (args == NULL) {
        return;
    }
    for (int i = 0; args[i] != NULL; i++) {
        free(args[i]);
    }
    free(args);
}

void show_prompt() {
    struct passwd *pw = getpwuid(getuid());
    char *username = (pw != NULL) ? pw->pw_name : "unknown";

    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strcpy(hostname, "unknown");
    }

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        strcpy(cwd, "?");
    }

    char display_path[PATH_MAX];
    if (shell_home_dir != NULL && strncmp(cwd, shell_home_dir, strlen(shell_home_dir)) == 0) {
        snprintf(display_path, sizeof(display_path), "~%s", cwd + strlen(shell_home_dir));
    } else {
        strncpy(display_path, cwd, sizeof(display_path) - 1);
        display_path[sizeof(display_path) - 1] = '\0';
    }

    printf("<%s@%s:%s> ", username, hostname, display_path);
    fflush(stdout);
}

void handle_hop(char **args) {
    char *cwd_before_hop = getcwd(NULL, 0);
    if (cwd_before_hop == NULL) {
        perror("getcwd");
        return;
    }
    
    char *target_path;
    if (args[0] == NULL) {
        target_path = shell_home_dir;
    } else if (strcmp(args[0], "~") == 0) {
        target_path = shell_home_dir;
    } else if (strcmp(args[0], "-") == 0) {
        if (prev_dir == NULL) {
            fprintf(stderr, "No such directory!\n");
            free(cwd_before_hop);
            return;
        }
        target_path = prev_dir;
    } else {
        target_path = args[0];
    }
    
    if (chdir(target_path) != 0) {
        if (errno == ENOENT) {
            fprintf(stderr, "No such directory!\n");
        } else {
            perror("hop");
        }
        free(cwd_before_hop);
    } else {
        if (prev_dir != NULL) {
            free(prev_dir);
        }
        prev_dir = cwd_before_hop;
    }
}

static int string_compare(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

void handle_reveal(char **args) {
    int show_all = 0;
    int line_by_line = 0;
    char *path = ".";

    int i = 0;
    while (args[i] != NULL && args[i][0] == '-' && args[i][1]!='\0') {
        for (int j = 1; args[i][j] != '\0'; j++) {
            if (args[i][j] == 'a') {
                show_all = 1;
            } else if (args[i][j] == 'l') {
                line_by_line = 1;
            } else {
                fprintf(stderr, "reveal: Invalid Syntax!\n");
                return;
            }
        }
        i++;
    }

    if (args[i] != NULL) {
        path = args[i];
    }

    if (args[i+1] != NULL) {
        fprintf(stderr, "reveal: Invalid Syntax!\n");
        return;
    }

    // if(strcmp(args[0],"-")==0 && args[1]==NULL){
    //     path = args[0];
    // }
    char resolved_path[PATH_MAX];
    char *target_path = path;
    if (strcmp(path, "~") == 0) {
        target_path = shell_home_dir;
    } else if (strcmp(path, "-") == 0) {
        if (prev_dir == NULL) {
            fprintf(stderr, "No such directory!\n");
            return;
        }
        target_path = prev_dir;
    }

    if (realpath(target_path, resolved_path) == NULL) {
        fprintf(stderr, "No such directory!\n");
        return;
    }

    DIR *dir = opendir(resolved_path);
    if (dir == NULL) {
        fprintf(stderr, "No such directory!\n");
        return;
    }

    struct dirent *entry;
    char **entries = malloc(100 * sizeof(char *));
    if (entries == NULL) {
        perror("malloc");
        closedir(dir);
        return;
    }
    int entry_count = 0;
    int max_entries = 100;

    while ((entry = readdir(dir)) != NULL) {
        if (!show_all && entry->d_name[0] == '.') {
            continue;
        }
        if (entry_count >= max_entries) {
            max_entries *= 2;
            char **temp = realloc(entries, max_entries * sizeof(char *));
            if (temp == NULL) {
                perror("realloc");
                break;
            }
            entries = temp;
        }
        entries[entry_count] = strdup(entry->d_name);
        if (entries[entry_count] == NULL) {
            perror("strdup");
            break;
        }
        entry_count++;
    }
    closedir(dir);

    qsort(entries, entry_count, sizeof(char *), string_compare);

    for (i = 0; i < entry_count; i++) {
        printf("%s", entries[i]);
        if (line_by_line) {
            printf("\n");
        } else {
            printf(" ");
        }
        free(entries[i]);
    }

    if (!line_by_line && entry_count > 0) {
        printf("\n");
    }

    free(entries);
}

void update_history(char *cmd) {
    // Don't store if command starts with "log"
    if (strncmp(cmd, "log", 3) == 0 && (cmd[3] == '\0' || cmd[3] == ' ')) {
        return;
    }
    
    // Don't store if identical to previous command
    if (his_cnt > 0 && strcmp(history[his_cnt - 1], cmd) == 0) {
        return;
    }

    // If we have 15 commands, remove the oldest
    if (his_cnt == 15) {
        free(history[0]);
        for (int i = 0; i < 14; i++) {
            history[i] = history[i + 1];
        }
        his_cnt--;
    }

    // Add new command
    history[his_cnt++] = strdup(cmd);
}

void handle_log(char **args) {
    // Check for invalid syntax
    if (args[0] != NULL && args[1] != NULL && args[2] != NULL) {
        printf("log: Invalid Syntax!\n");
        return;
    }
    
    if (args[0] == NULL) {
        // No arguments: Print stored commands (oldest to newest)
        for (int i = 0; i < his_cnt; i++) {
            printf("%s\n", history[i]);
        }
    } else if (strcmp(args[0], "purge") == 0) {
        // Check for extra arguments after purge
        if (args[1] != NULL) {
            printf("log: Invalid Syntax!\n");
            return;
        }
        // Clear history
        for (int i = 0; i < his_cnt; i++) {
            free(history[i]);
        }
        his_cnt = 0;
    } else if (strcmp(args[0], "execute") == 0) {
        if (args[1] == NULL) {
            printf("log: Invalid Syntax!\n");
            return;
        }
        
        // Check for extra arguments after execute <index>
        if (args[2] != NULL) {
            printf("log: Invalid Syntax!\n");
            return;
        }

        int index = atoi(args[1]);
        // Index should be 1-indexed, newest to oldest
        if (index <= 0 || index > his_cnt) {
            printf("log: Invalid Syntax!\n");
            return;
        }

        // Convert to array index (newest to oldest means reverse order)
        char *command_to_execute = history[his_cnt - index];
        char **exec_args = parse_command(command_to_execute);
        if (exec_args != NULL) {
            run_builtin_or_external(exec_args, 0);
            free_args(exec_args);
        }
    } else {
        // Invalid first argument
        printf("log: Invalid Syntax!\n");
    }
}

void handle_ping(char **args) {
    if (args[0] == NULL || args[1] == NULL) {
        printf("Invalid syntax!\n");
        return;
    }

    pid_t pid = (pid_t)atoi(args[0]);
    int signal_num = atoi(args[1]);
    if(signal_num == 0){
        printf("Invalid syntax!\n");
        return ;
    }
    int actual_signal = signal_num % 32;

    if (kill(pid, actual_signal) == 0) {
        printf("Sent signal %d to process with pid %d\n", signal_num, pid);
    } else {
        if (errno == ESRCH) {
            fprintf(stderr, "No such process found\n");
        } 
    }
}

// Global variable to track the foreground process group
extern pid_t foreground_pgid ;

void handle_sigint(int signo) {
    if (foreground_pgid > 0) {
        kill(-foreground_pgid, SIGINT);
    }
}

void handle_sigtstp(int signo) {
    if (foreground_pgid > 0) {
        kill(-foreground_pgid, SIGTSTP);

        if (current_foreground_pid > 0) {
            add_child_process(current_foreground_pid, current_foreground_command, 1);
            // Find the process we just added and mark it as stopped
            for (int i = 0; i < process_count; i++) {
                if (processes[i].pid == current_foreground_pid) {
                    processes[i].status = STOPPED;
                    printf("\n[%d] Stopped %s\n", processes[i].job_number, processes[i].command);
                    break;
                }
            }
        }
        
        // Reset foreground tracking
        foreground_pgid = 0;
        current_foreground_pid = 0;
        current_foreground_command[0] = '\0';
    }
}

void setup_signal_handlers() {
    struct sigaction sa;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);

    // Handle SIGINT (Ctrl+C)
    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);

    // Handle SIGTSTP (Ctrl+Z) 
    sa.sa_handler = handle_sigtstp;
    sigaction(SIGTSTP, &sa, NULL);
    
    // Ignore SIGTTOU to prevent background job control issues
    signal(SIGTTOU, SIG_IGN);
}

process* find_job_by_number(int job_number) {
    for (int i = 0; i < process_count; i++) {
        if (processes[i].job_number == job_number) {
            return &processes[i];
        }
    }
    return NULL;
}

process* find_most_recent_job() {
    for (int i = process_count - 1; i >= 0; i--) {
        if (processes[i].is_background) {
            return &processes[i];
        }
    }
    return NULL;
}

void handle_fg(char **args) {
    process *job = NULL;
    if (args[0] == NULL) {
        job = find_most_recent_job();
    } else {
        int job_num = atoi(args[0]);
        job = find_job_by_number(job_num);
    }

    if (job == NULL) {
        fprintf(stderr, "No such job\n");
        return;
    }

    printf("%s\n", job->command);

    // Send SIGCONT if the job is stopped
    if (job->status == STOPPED) {
        if (kill(-job->pgid, SIGCONT) < 0) {
            perror("fg: Failed to send SIGCONT");
            return;
        }
        job->status = RUNNING;
    }

    // Move job to foreground
    job->is_background = 0;
    foreground_pgid = job->pgid;
    current_foreground_pid = job->pid;
    strncpy(current_foreground_command, job->command, sizeof(current_foreground_command) - 1);
    current_foreground_command[sizeof(current_foreground_command) - 1] = '\0';

    // Wait for the job
    int status;
    pid_t result = waitpid(-job->pgid, &status, WUNTRACED);
    
    if (result > 0) {
        if (WIFSTOPPED(status)) {
            // Process was stopped again (Ctrl+Z while in foreground)
            // The signal handler will take care of this
            return;
        } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
            // Process completed - remove from job list
            remove_process_by_pid(job->pid);
        }
    }

    // Reset foreground tracking
    foreground_pgid = 0;
    current_foreground_pid = 0;
    current_foreground_command[0] = '\0';
}

void handle_bg(char **args) {
    process *job = NULL;
    if (args[0] == NULL) {
        job = find_most_recent_job();
    } else {
        int job_num = atoi(args[0]);
        job = find_job_by_number(job_num);
    }

    if (job == NULL) {
        fprintf(stderr, "No such job\n");
        return;
    }

    if (job->status == RUNNING) {
        fprintf(stderr, "Job already running\n");
        return;
    }

    if (kill(-job->pgid, SIGCONT) < 0) {
        //perror("bg: Failed to send SIGCONT");
        return;
    }

    printf("[%d] %s &\n", job->job_number, job->command);
    job->status = RUNNING;
}

void run_builtin_or_external(char **args, int is_background) {
    if (args == NULL || args[0] == NULL) {
        return;
    }

    int has_pipe = 0;
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) {
            has_pipe = 1;
            break;
        }
    }

    // If it's a pipeline, handle it in execute_command_group
    if (has_pipe) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
        } else if (pid == 0) {
            // Child process
            pid_t pgid = getpid();
            setpgid(0, pgid);
            
            // Restore default signal handlers in child
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);

            execute_command_group(args);
            exit(EXIT_SUCCESS);
        } else {
            // Parent process
            setpgid(pid, pid); // Set process group ID in parent too
            
            if (is_background) {
                add_child_process(pid, args[0], 1);
                printf("[%d] %d\n", processes[process_count-1].job_number, pid);
            } else {
                // Track foreground process for signal handling
                foreground_pgid = pid;
                current_foreground_pid = pid;
                strncpy(current_foreground_command, args[0], sizeof(current_foreground_command) - 1);
                current_foreground_command[sizeof(current_foreground_command) - 1] = '\0';
                
                int status;
                pid_t result = waitpid(pid, &status, WUNTRACED);
                
                if (result == pid) {
                    if (WIFSTOPPED(status)) {
                        // Process was stopped by signal (Ctrl+Z)
                        // The signal handler already dealt with this
                        // Don't reset foreground tracking here
                        return;
                    } else {
                        // Process completed normally or was terminated
                        foreground_pgid = 0;
                        current_foreground_pid = 0;
                        current_foreground_command[0] = '\0';
                    }
                } else {
                    // waitpid was interrupted or failed
                    foreground_pgid = 0;
                    current_foreground_pid = 0;
                    current_foreground_command[0] = '\0';
                }
            }
        }
        return;
    }

    if (strcmp(args[0], "hop") == 0) {
        handle_hop(args + 1);
    } else if (strcmp(args[0], "reveal") == 0) {
        handle_reveal(args + 1);
    } else if (strcmp(args[0], "log") == 0) {
        handle_log(args + 1);
    } else if (strcmp(args[0], "activities") == 0) {
        run_activities_builtin(processes, process_count);
    } else if (strcmp(args[0], "ping") == 0) {
        handle_ping(args + 1);
    } else if (strcmp(args[0], "fg") == 0) {
        handle_fg(args + 1);
    } else if (strcmp(args[0], "bg") == 0) {
        handle_bg(args + 1);
    } else {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
        } else if (pid == 0) {
            // Child process
            pid_t pgid = getpid();
            setpgid(0, pgid);
            
            // Restore default signal handlers in child
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);

            execute_command_group(args);
            exit(EXIT_SUCCESS);
        } else {
            // Parent process
            setpgid(pid, pid); // Set process group ID in parent too
            
            if (is_background) {
                add_child_process(pid, args[0], 1);
                printf("[%d] %d\n", processes[process_count-1].job_number, pid);
            } else {
                // Track foreground process for signal handling
                foreground_pgid = pid;
                current_foreground_pid = pid;
                strncpy(current_foreground_command, args[0], sizeof(current_foreground_command) - 1);
                current_foreground_command[sizeof(current_foreground_command) - 1] = '\0';
                
                int status;
                pid_t result = waitpid(pid, &status, WUNTRACED);
                
                if (result == pid) {
                    if (WIFSTOPPED(status)) {
                        // Process was stopped by signal (Ctrl+Z)
                        // The signal handler already dealt with this
                        // Don't reset foreground tracking here
                        return;
                    } else {
                        // Process completed normally or was terminated
                        foreground_pgid = 0;
                        current_foreground_pid = 0;
                        current_foreground_command[0] = '\0';
                    }
                } else {
                    // waitpid was interrupted or failed
                    foreground_pgid = 0;
                    current_foreground_pid = 0;
                    current_foreground_command[0] = '\0';
                }
            }
        }
    }
}


void completed_processes(void) {
    int status;
    pid_t completed_pid;
    
    while ((completed_pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        int found_index = -1;
        for (int i = 0; i < process_count; i++) {
            if (processes[i].pid == completed_pid) {
                found_index = i;
                break;
            }
        }

        if (found_index == -1) {
            continue;
        }

        process *job = &processes[found_index];

        if (WIFEXITED(status)) {
            printf("%s with pid %d exited normally\n", job->command, completed_pid);
            remove_process_by_pid(completed_pid);
        } else if (WIFSIGNALED(status)) {
            printf("%s with pid %d exited abnormally\n", job->command, completed_pid);
            remove_process_by_pid(completed_pid);
        } else if (WIFSTOPPED(status)) {
            // Process was stopped
            if (job->status != STOPPED) {
                job->status = STOPPED;
                // Only print if this wasn't already handled by signal handler
                if (job->pid != current_foreground_pid) {
                    printf("[%d] Stopped %s\n", job->job_number, job->command);
                }
            }
        } else if (WIFCONTINUED(status)) {
            job->status = RUNNING;
            if (!job->is_background) {
                printf("[%d] Running %s\n", job->job_number, job->command);
            }
        }
    }
}
