#include "util.h"

void clear_input_buffer(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { }
}

int read_int(const char *prompt) {
    int v;
    for (;;) {
        printf("%s", prompt);
        if (scanf("%d", &v) == 1) {
            clear_input_buffer();
            return v;
        }
        printf(COLOR_RED "  输入无效,请输入整数\n" COLOR_RESET);
        clear_input_buffer();
    }
}

int read_int_default(const char *prompt, int def) {
    char line[64];
    printf("%s [默认 %d]: ", prompt, def);
    if (!fgets(line, sizeof(line), stdin)) return def;
    if (line[0] == '\n' || line[0] == '\0') return def;
    int v;
    if (sscanf(line, "%d", &v) == 1) return v;
    return def;
}

double read_double(const char *prompt) {
    double v;
    for (;;) {
        printf("%s", prompt);
        if (scanf("%lf", &v) == 1) {
            clear_input_buffer();
            return v;
        }
        printf(COLOR_RED "  输入无效,请输入数字\n" COLOR_RESET);
        clear_input_buffer();
    }
}

void pause_screen(void) {
    printf("\n" COLOR_CYAN "按回车继续..." COLOR_RESET);
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { }
}

void print_title(const char *title) {
    printf("\n" COLOR_BOLD COLOR_CYAN);
    printf("==================================================\n");
    printf("  %s\n", title);
    printf("==================================================\n");
    printf(COLOR_RESET);
}

void print_divider(void) {
    printf("--------------------------------------------------\n");
}
