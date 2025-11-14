#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>

#include "cfg.h"
#include "pipeline.h"

extern pid_t current_foreground_pid;
extern char current_foreground_command[256];

typedef enum {
    RUNNING,
    STOPPED
} job_status;

typedef struct {
    pid_t pid;
    pid_t pgid;
    char *command;
    int job_number;
    int is_background; // 1 for background, 0 for sequential
    job_status status;
} process;

extern process processes[100];
extern int process_count;
extern int next_job_number;
extern pid_t foreground_pgid;

#define MAX_INPUT_SIZE 1024
#define MAX_NAME_SIZE 256
#define MAX_ARGS 10

extern char *history[15];
extern int his_cnt;

void load_history();
void save_history();
void init_builtin_state(char *home_path);
void show_prompt();
void handle_hop(char **args);
void handle_reveal(char **args);
void update_history(char *cmd);
void handle_log(char **args);
void run_builtin_or_external(char **args, int is_background);
void handle_ping(char **args);
void run_activities_builtin(process *processes, int count);

void add_child_process(pid_t pid, const char *command, int bg);
void remove_process_by_pid(pid_t pid);
void completed_processes(void);

void setup_signal_handlers();
void handle_sigint(int signo);
void handle_sigtstp(int signo);

void handle_fg(char **args);
void handle_bg(char **args);


void free_args(char **args);

#endif
