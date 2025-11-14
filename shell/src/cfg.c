#include "cfg.h" 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> 
#include <stdbool.h>

typedef enum {
    TOK_NAME,
    TOK_PIPE,     // |
    TOK_AND,      // && (logical AND for command separation)
    TOK_AMP,      // & (background execution)
    TOK_INPUT,    // <
    TOK_OUTPUT,   // > or >>
    TOK_SEMI,     // ; (command separator, though not explicitly in the given CFG for shell_cmd's structure)
    TOK_END       // Marks the end of the token stream
} TokenType;


typedef struct {
    TokenType type;
    char text[256]; 
} Token;

static bool parse_name();
static bool parse_input();
static bool parse_output();
static bool parse_atomic();
static bool parse_cmd_group();
static bool parse_shell_cmd_segment();
static bool parse_shell_cmd();

#define MAX_TOKENS 1024
static Token tokens[MAX_TOKENS];
static int tok_pos = 0;         
static int tok_len = 0;        


static void tokenize(char *input) {
    tok_pos = 0; 
    tok_len = 0; 

    while (*input) {
        while (isspace((unsigned char)*input)) input++; 
        if (!*input) break; 

        if (*input == '|') {
            tokens[tok_len++] = (Token){TOK_PIPE, "|"};
            input++;
        } 
        else if (*input == '&') {
                tokens[tok_len++] = (Token){TOK_AMP, "&"};
                input++;
        } 
        else if (*input == '<') {
            tokens[tok_len++] = (Token){TOK_INPUT, "<"};
            input++;
        } 
        else if (*input == '>') {
            if (*(input + 1) == '>') {
                tokens[tok_len++] = (Token){TOK_OUTPUT, ">>"};
                input += 2;
            } else {
                tokens[tok_len++] = (Token){TOK_OUTPUT, ">"};
                input++;
            }
        } 
        else if (*input == ';') {
            tokens[tok_len++] = (Token){TOK_SEMI, ";"};
            input++;
        } 
        else {
            char buf[256]; // Temporary buffer for collecting name characters
            size_t len = 0;

            while (*input && !isspace((unsigned char)*input) && *input != '|' && *input != '&' && *input != '<' && *input != '>' && *input != ';') {
                if (len < sizeof(buf) - 1) { 
                    buf[len++] = *input;
                } else {
                    fprintf(stderr, "Warning: Name token exceeds buffer size, truncating.\n");
                    break;
                }
                input++;
            }
            buf[len] = '\0'; 

            if (len > 0) { 
                tokens[tok_len++] = (Token){TOK_NAME, ""};
                strncpy(tokens[tok_len - 1].text, buf, sizeof(tokens[tok_len - 1].text) - 1);
                tokens[tok_len - 1].text[sizeof(tokens[tok_len - 1].text) - 1] = '\0'; 
            } else {
                input++;
            }
        }
    }
    tokens[tok_len++] = (Token){TOK_END, ""}; 
}


static TokenType current_token_type() {
    if (tok_pos >= tok_len) {
        return TOK_END;
    }
    return tokens[tok_pos].type;
}

static void consume_token() {
    if (tok_pos < tok_len) {
        tok_pos++;
    }
}

static char **collect_atomic_args() {
    char **args = (char **)malloc((MAX_TOKENS + 1) * sizeof(char *));
    if (args == NULL) {
        perror("malloc");
        return NULL;
    }
    int arg_count = 0;

    int temp_pos = tok_pos;

    while(temp_pos < tok_len && tokens[temp_pos].type != TOK_END && 
        tokens[temp_pos].type != TOK_SEMI && tokens[temp_pos].type != TOK_AMP) {
        
        // Collect ALL token types (names, operators, filenames)
        args[arg_count] = strdup(tokens[temp_pos].text);
        if (args[arg_count] == NULL) {
            perror("strdup");
            for (int i = 0; i < arg_count; i++) {
                free(args[i]);
            }
            free(args);
            return NULL;
        }
        arg_count++;
        temp_pos++;
    }
    args[arg_count] = NULL; 
    return args;
}

static bool parse_name() {
    if (current_token_type() == TOK_NAME) {
        consume_token(); 
        return true;
    }
    return false; 
}

static bool parse_input() {
    if (current_token_type() == TOK_INPUT) {
        consume_token(); 
        if (parse_name()) { 
            return true;
        }
    }
    return false; // Expected '< name'
}

static bool parse_output() {
    if (current_token_type() == TOK_OUTPUT) {
        consume_token(); 
        if (parse_name()) { 
            return true;
        }
    }
    return false; 
}

static bool parse_atomic() {
    if (!parse_name()) { 
        return false;
    }

    while (true) {
        TokenType next_type = current_token_type();
        if (next_type == TOK_NAME) { 
            if (!parse_name()) return false; 
        } else if (next_type == TOK_INPUT) { 
            if (!parse_input()) return false;
        } else if (next_type == TOK_OUTPUT) { 
            if (!parse_output()) return false;
        } else {
            break; 
        }
    }
    return true; // Successfully parsed an atomic command
}

static bool parse_cmd_group() {
    if (!parse_atomic()) { 
        return false;
    }

    while (current_token_type() == TOK_PIPE) {
        consume_token(); 
        if (!parse_atomic()) { 
            return false;
        }
    }
    return true; 
}

static bool parse_shell_cmd_segment() {
    while (true) {
        TokenType current_t = current_token_type();
        if (current_t == TOK_AMP || current_t == TOK_AND) {
            consume_token(); 
            if (!parse_cmd_group()) { 
                return false;
            }
        } else {
            break;
        }
    }
    return true;
}

static bool parse_shell_cmd() {
    if (!parse_cmd_group()) {
        return false;
    }

    if (!parse_shell_cmd_segment()) {
        return false;
    }

    if (current_token_type() == TOK_AMP) {
        consume_token(); 
    }

    if (current_token_type() == TOK_SEMI) {
        return false; 
    }

    return current_token_type() == TOK_END; 
}

char** parse_command(char *input) {
    tokenize(input);      
    tok_pos = 0;         
    
    char** args = collect_atomic_args();

    if (!parse_shell_cmd()) {
        if (args != NULL) {
            for (int i = 0; args[i] != NULL; i++) {
                free(args[i]);
            }
            free(args);
        }
        return NULL;
    }
    
    return args;
}