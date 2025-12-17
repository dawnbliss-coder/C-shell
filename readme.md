# Custom Shell Implementation

A POSIX-compliant Unix shell implementation in C featuring advanced process management, I/O redirection, pipelines, job control, and signal handling.

## Technical Highlights
- Implemented process group management for signal isolation (SIGINT, SIGTSTP, SIGCHLD)
- Designed CFG-based parser tokenizing 7 operator types with syntax validation
- Built multi-stage pipeline executor with proper file descriptor management using pipe() and dup2()
- Developed job control system tracking PIDs, PGIDs, and process states (Running/Stopped)

## Features

### Built-in Commands
- **hop**: Change directory with support for `~` (home), `-` (previous directory), and relative/absolute paths
- **reveal**: List directory contents with flags `-a` (show hidden files) and `-l` (line-by-line output)
- **log**: Command history management supporting view, purge, and execute operations
- **ping**: Send signals to processes by PID
- **activities**: Display all background processes sorted by command name with their status (Running/Stopped)
- **fg**: Bring a background job to the foreground
- **bg**: Resume a stopped background job

### Advanced Features
- **Command Parsing**: Context-free grammar (CFG) based tokenizer and parser
- **Pipeline Execution**: Multi-stage pipelines with `|` operator
- **I/O Redirection**: Support for `<` (input), `>` (output/truncate), and `>>` (append)
- **Background Processes**: Run commands asynchronously with `&`
- **Command Chaining**: Execute multiple commands separated by `;`
- **Signal Handling**: Proper handling of Ctrl+C (SIGINT), Ctrl+Z (SIGTSTP), and Ctrl+D (EOF)
- **Job Control**: Process group management with foreground/background job tracking
- **Command History**: Store last 15 commands persistently in `.shell_history`

## Project Structure

```
shell/
├── include/
│   ├── cfg.h           # CFG parser declarations
│   ├── pipeline.h      # Pipeline execution declarations
│   └── shell.h         # Core shell function declarations
├── src/
│   ├── main.c          # Entry point and main loop
│   ├── shell.c         # Core shell functionality and built-in commands
│   ├── cfg.c           # CFG-based command parser and tokenizer
│   ├── pipeline.c      # Pipeline and I/O redirection handling
│   └── activities.c    # Background process tracking and management
└── Makefile            # Build configuration
```

## Compilation

```bash
make
```

This will compile all source files and create the `shell.out` executable.

To clean build artifacts:
```bash
make clean
```

## Usage

```bash
./shell.out
```

The shell displays a prompt in the format:
```
<username@hostname:current_directory> 
```

### Command Examples

#### Directory Navigation
```bash
<user@system:~> hop /path/to/directory
<user@system:~> hop ~
<user@system:~> hop -
```

#### File Listing
```bash
<user@system:~> reveal
<user@system:~> reveal -a
<user@system:~> reveal -l /path/to/dir
<user@system:~> reveal -al
```

#### Command History
```bash
<user@system:~> log                    # Display history
<user@system:~> log execute 1          # Execute most recent command
<user@system:~> log purge              # Clear history
```

#### Background Processes
```bash
<user@system:~> sleep 100 &
[1] 12345
<user@system:~> activities
[12345] : sleep - Running
```

#### Job Control
```bash
<user@system:~> fg 1                   # Bring job 1 to foreground
<user@system:~> bg 1                   # Resume stopped job 1 in background
```

#### Signal Management
```bash
<user@system:~> ping 12345 9           # Send SIGKILL (9) to process 12345
```

#### I/O Redirection
```bash
<user@system:~> echo "Hello" > output.txt
<user@system:~> cat < input.txt
<user@system:~> cat file.txt >> output.txt
```

#### Pipelines
```bash
<user@system:~> cat file.txt | grep "pattern" | wc -l
<user@system:~> reveal -a | grep ".txt"
```

#### Command Chaining
```bash
<user@system:~> echo "First" ; echo "Second" ; echo "Third"
```

## Implementation Details

### CFG-Based Parsing
The command parser uses a context-free grammar with tokenization that recognizes names, pipes (`|`), ampersands (`&`), input/output redirection (`<`, `>`, `>>`), and semicolons (`;`). This ensures syntactically valid commands are properly parsed before execution.

### Process Management
- Each process is assigned to its own process group for proper signal isolation
- Background processes are tracked with job numbers, PIDs, PGIDs, command names, and status (Running/Stopped)
- Foreground processes block the shell and receive terminal signals
- Completed background processes are detected and reported asynchronously

### Pipeline Execution
- Multi-stage pipelines are implemented using `pipe()` and `fork()`
- Each command in the pipeline runs in a separate child process
- I/O redirection is handled before pipeline execution, with proper precedence (last redirection wins)
- Built-in commands can be used within pipelines

### Signal Handling
- SIGINT (Ctrl+C) and SIGTSTP (Ctrl+Z) are forwarded only to foreground process groups
- The shell ignores SIGTTOU to prevent background job control issues
- When a foreground process is stopped (Ctrl+Z), it's automatically added to the background job list

### History Management
- Command history stores up to 15 unique commands
- Commands starting with "log" and duplicate consecutive commands are not stored
- History persists across shell sessions in `~/.shell_history`

## Build Configuration

The project uses a Makefile with the following compiler flags:
- `-std=c99`: C99 standard compliance
- `-D_POSIX_C_SOURCE=200809L`: POSIX.1-2008 features
- `-D_XOPEN_SOURCE=700`: X/Open 7 features
- `-Wall -Wextra -Werror`: Strict warning and error checking
- `-Iinclude`: Include directory for header files

## Requirements

- GCC compiler with C99 support
- POSIX-compliant Unix/Linux system or macOS
- Standard POSIX headers: `unistd.h`, `sys/wait.h`, `signal.h`, `dirent.h`, etc.
- Make build system

## Error Handling

- Invalid syntax errors are reported with descriptive messages
- File not found errors show "No such file or directory"
- Process errors (no such process) are handled gracefully
- Memory allocation failures are checked and reported

## Special Operators

Reserved operators with special meanings:
- `|` : Pipeline operator
- `&` : Background execution
- `;` : Command separator
- `<` : Input redirection
- `>` : Output redirection (truncate)
- `>>` : Output redirection (append)

These operators must be separated by spaces for proper parsing.

## Notes

- The `~` character expands to the shell's initial working directory
- The `-` directory reference refers to the previous working directory
- Process groups are used to properly isolate foreground and background jobs
- External commands are executed using `execvp()`
- Signal modulo 32 arithmetic is applied for the `ping` command
- The executable is named `shell.out` after compilation


