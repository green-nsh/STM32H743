#include "shell.h"
#include "mcu_temp.h"
#include "app_led.h"
#include "./SYSTEM/usart/usart.h"
#include <string.h>

static const shell_command_t *table;
static unsigned int count, length;
static char line[SHELL_LINE_SIZE], draft[SHELL_LINE_SIZE];
static char history[SHELL_HISTORY_SIZE][SHELL_LINE_SIZE];
/* head is the next write slot; position 0 is the draft, 1 is newest. */
static unsigned int history_head, history_count, history_position;
static unsigned char escape_state, previous_cr, discard;

/*CMD÷’∂À√¸¡Ó◊¢≤·*/
static void cmd_info(int argc, char **argv);
static void cmd_uptime(int argc, char **argv);
static void cmd_temp(int argc, char **argv);
static void cmd_led(int argc, char **argv);
static void cmd_echo(int argc, char **argv);
static void cmd_clear(int argc, char **argv);

static const shell_command_t commands[] = {
    {"help",    "List commands and editing keys",           0},
    {"info",    "Show MCU and UART information",            cmd_info},
    {"uptime",  "Show elapsed time since boot",             cmd_uptime},
    {"temp",    "Read internal MCU temperature and VREF+",  cmd_temp},
    {"led",     "led [on|off|breathe] - control PB0 LED",   cmd_led},
    {"echo",    "echo [text ...]",                          cmd_echo},
    {"clear",   "Clear ANSI terminal screen",               cmd_clear}
};

static void cmd_info(int argc, char **argv)
{
    (void)argv;
    if (argc != 1) { printf("Usage: info\r\n"); return; }
    printf("STM32H743 | CPU %lu MHz | USART1 PA9/PA10 | 115200 8N1\r\n",
           (unsigned long)SystemCoreClock/1000000);
}

static void cmd_uptime(int argc, char **argv)
{
    uint32_t ms = HAL_GetTick();
    (void)argv;
    if (argc != 1) { printf("Usage: uptime\r\n"); return; }
    printf("%lu.%03lu seconds (32-bit tick)\r\n",
           (unsigned long)(ms / 1000U), (unsigned long)(ms % 1000U));
}

static void cmd_led(int argc, char **argv)
{
    if (argc == 1) {
        app_led_mode_t mode = app_led_get_mode();
        printf("LED PB0: %s\r\n", mode == APP_LED_BREATHE ? "breathing" :
               (mode == APP_LED_ON ? "on" : "off"));
        return;
    }
    if (argc != 2) { printf("Usage: led [on|off|breathe]\r\n"); return; }
    if (strcmp(argv[1], "breathe") == 0) app_led_set_mode(APP_LED_BREATHE);
    else if (strcmp(argv[1], "on") == 0) app_led_set_mode(APP_LED_ON);
    else if (strcmp(argv[1], "off") == 0) app_led_set_mode(APP_LED_OFF);
    else { printf("Usage: led [on|off|breathe]\r\n"); return; }
    printf("OK\r\n");
}

static void cmd_echo(int argc, char **argv)
{
    int i;
    for (i = 1; i < argc; ++i) printf("%s%s", i == 1 ? "" : " ", argv[i]);
    printf("\r\n");
}

static void cmd_clear(int argc, char **argv)
{
    (void)argv;
    if (argc != 1) { printf("Usage: clear\r\n"); return; }
    printf("\033[2J\033[H");
}

static void cmd_temp(int argc, char **argv)
{
    int32_t temperature;
    uint32_t voltage;
    HAL_StatusTypeDef status;
    (void)argv;
    if (argc != 1) { printf("Usage: temp\r\n"); return; }
    status = mcu_temp_read(&temperature, &voltage);
    if (status != HAL_OK) {
        const mcu_temp_diag_t *d = mcu_temp_diagnostics();
        printf("Temperature read failed: stage=%s channel=%s HAL=%u ADC-error=0x%08lX\r\n",
               d->stage, d->channel, (unsigned int)status, (unsigned long)d->adc_error);
        printf("kernel=%lu Hz raw-vref=%lu raw-temp=%lu VREF+=%lu mV temp=%ld C\r\n",
               (unsigned long)d->kernel_hz, (unsigned long)d->vref_raw,
               (unsigned long)d->temp_raw, (unsigned long)d->vdda_mv, (long)d->temperature_c);
        printf("cal-vref=%lu cal-t1=%lu cal-t2=%lu IDCODE=0x%08lX\r\n",
               (unsigned long)d->cal_vref, (unsigned long)d->cal_t1,
               (unsigned long)d->cal_t2, (unsigned long)d->revision);
        return;
    }
    printf("MCU temperature: %ld C | VREF+: %lu mV\r\n",
           (long)temperature, (unsigned long)voltage);
}



static void prompt(void) { printf("stm32>> "); }

static void replace_line(const char *text)
{
    while (length) { printf("\b \b"); --length; }
    strcpy(line, text);
    length = (unsigned int)strlen(line);
    printf("%s", line);
}

static unsigned int history_index(unsigned int age)
{
    return (history_head + SHELL_HISTORY_SIZE - age) % SHELL_HISTORY_SIZE;
}

static void history_save(void)
{
    const char *p = line;
    while (*p == ' ' || *p == '\t') ++p;
    if (!*p) return; /* Ignore empty and whitespace-only commands. */
    if (history_count && strcmp(history[history_index(1)], line) == 0)
        return; /* Repeated temp commands do not consume all history slots. */
    strcpy(history[history_head], line);
    history_head = (history_head + 1U) % SHELL_HISTORY_SIZE;
    if (history_count < SHELL_HISTORY_SIZE) ++history_count;
}

static void history_previous(void)
{
    if (history_position >= history_count) { printf("\a"); return; }
    if (history_position == 0) {
        line[length] = '\0';
        strcpy(draft, line);
    }
    ++history_position;
    replace_line(history[history_index(history_position)]);
}

static void history_next(void)
{
    if (history_position == 0) { printf("\a"); return; }
    --history_position;
    replace_line(history_position ? history[history_index(history_position)] : draft);
}

static void complete(void)
{
    unsigned int i, matches = 0, match = 0;
    line[length] = '\0';
    if (!length || strchr(line, ' ')) return;
    for (i = 0; i < count; ++i) {
        if (strncmp(line, table[i].name, length) == 0) { match = i; ++matches; }
    }
    if (matches == 1 && strlen(table[match].name) < SHELL_LINE_SIZE - 1) {
        printf("%s ", table[match].name + length);
        strcpy(line, table[match].name);
        length = (unsigned int)strlen(line);
        line[length++] = ' ';
        line[length] = '\0';
    } else if (matches > 1) {
        printf("\r\n");
        for (i = 0; i < count; ++i)
            if (strncmp(line, table[i].name, length) == 0) printf("%s  ", table[i].name);
        printf("\r\n"); prompt(); printf("%s", line);
    } else printf("\a");
}

static void execute(void)
{
    char *argv[SHELL_MAX_ARGS], *p = line;
    int argc = 0;
    unsigned int i;
    line[length] = '\0';
    history_save();
    while (*p) {
        while (*p == ' ' || *p == '\t') ++p;
        if (!*p) break;
        if (argc == SHELL_MAX_ARGS) { printf("Too many arguments (max %d).\r\n", SHELL_MAX_ARGS); return; }
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') ++p;
        if (*p) *p++ = '\0';
    }
    if (!argc) return;
    if (strcmp(argv[0], "help") == 0) {
        for (i = 0; i < count; ++i) printf("  %-10s %s\r\n", table[i].name, table[i].help);
        printf("Backspace: erase; Up/Down: browse history/restore draft; Tab: complete\r\n"
               "Ctrl+C/Ctrl+U: cancel line; Ctrl+L: clear screen\r\n");
        return;
    }
    for (i = 0; i < count; ++i) {
        if (strcmp(argv[0], table[i].name) == 0) {
            if (table[i].handler) table[i].handler(argc, argv);
            return;
        }
    }
    printf("Unknown command: %s. Type help.\r\n", argv[0]);
}

void shell_init(void)
{
    table = commands; 
    count = sizeof(commands) / sizeof(commands[0]);
    length = escape_state = previous_cr = discard = 0;
    history_head = history_count = history_position = 0;
    line[0] = draft[0] = '\0';
    printf("\r\nSTM32H743 serial shell. Type help.\r\n"); 
    prompt();
}

void shell_poll(void)
{
    int value;
    unsigned int budget = 64;
    /* Bound each poll so other main-loop tasks can run during continuous input. */
    while (budget-- && (value = usart_read_char()) != -1) {
        unsigned char c;
        if (value == -2) {
            discard = 1; length = escape_state = previous_cr = 0;
            history_position = 0;
            printf("\r\nUART input lost; line cancelled. Press Enter.\r\n");
            continue;
        }
        c = (unsigned char)value;
        if (c == '\n' && previous_cr) { previous_cr = 0; continue; }
        previous_cr = (c == '\r');
        if (c == '\r' || c == '\n') {
            printf("\r\n");
            if (!discard) execute();
            length = escape_state = discard = 0;
            history_position = 0;
            line[0] = '\0'; prompt(); continue;
        }
        if (c == 3 || c == 21) {
            length = escape_state = discard = 0;
            history_position = 0;
            line[0] = '\0'; printf("^C\r\n"); prompt(); continue;
        }
        if (discard) continue;
        if (escape_state == 1) { escape_state = (c == '[' || c == 'O') ? 2 : 0; continue; }
        if (escape_state == 2) {
            if (c < 0x40 || c > 0x7e) continue;
            escape_state = 0;
            if (c == 'A') history_previous();
            else if (c == 'B') history_next();
            continue;
        }
        if (c == 27) { escape_state = 1; continue; }
        if (c == 8 || c == 127) {
            if (length) { line[--length] = '\0'; printf("\b \b"); }
        } else if (c == '\t') complete();
        else if (c == 12) {
            line[length] = '\0'; printf("\033[2J\033[H"); prompt(); printf("%s", line);
        } else if (c >= 32 && c <= 126) {
            if (length == SHELL_LINE_SIZE - 1) {
                discard = 1; printf("\r\nLine too long; command cancelled. Press Enter.\r\n");
            } else { line[length++] = (char)c; line[length] = '\0'; printf("%c", c); }
        }
    }
}
