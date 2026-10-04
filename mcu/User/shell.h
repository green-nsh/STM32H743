#ifndef SHELL_H
#define SHELL_H
#define SHELL_LINE_SIZE 128
#define SHELL_MAX_ARGS 8
/* RAM-only history; increase this value to retain more commands. */
#define SHELL_HISTORY_SIZE 16
#if SHELL_HISTORY_SIZE < 1
#error SHELL_HISTORY_SIZE must be at least 1
#endif
typedef void (*shell_handler_t)(int argc, char **argv);
typedef struct {
    const char *name;
    const char *help;
    shell_handler_t handler;
} shell_command_t;
/* Initialize the built-in command table and clear command history. */
void shell_init(void);
void shell_poll(void);
#endif
