#include "shell.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

//process processes[100];
extern int process_count;
extern int next_job_number;
extern pid_t foreground_pgid ;

void add_child_process(pid_t pid, const char *command, int bg) {
    if (process_count < 100) {
        processes[process_count].pid = pid;
        processes[process_count].pgid = getpgid(pid);
        processes[process_count].command = strdup(command);
        processes[process_count].job_number = next_job_number++;
        processes[process_count].is_background = bg;
        processes[process_count].status = RUNNING;

        process_count++;
    } 
}

void remove_process_by_pid(pid_t pid) {
    for (int i = 0; i < process_count; i++) {
        if (processes[i].pid == pid) {
            free(processes[i].command);
            for (int j = i; j < process_count - 1; j++) {
                processes[j] = processes[j + 1];
            }
            process_count--;
            return;
        }
    }
}

// completed_processes is implemented in shell.c

int compare_commands(const void *a, const void *b) {
    process *proc_a = (process *)a;
    process *proc_b = (process *)b;
    return strcmp(proc_a->command, proc_b->command);
}

void run_activities_builtin(process *processes, int count) {
    qsort(processes, count, sizeof(process), compare_commands);

    for (int i = 0; i < count; i++) {
        char *state = (processes[i].status == STOPPED) ? "Stopped" : "Running";
        printf("[%d] : %s - %s\n", processes[i].pid, processes[i].command, state);
    }
}
