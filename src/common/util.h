#ifndef OS_UTIL_H
#define OS_UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"

void clear_input_buffer(void);
int read_int(const char *prompt);
int read_int_default(const char *prompt, int def);
double read_double(const char *prompt);
void pause_screen(void);
void print_title(const char *title);
void print_divider(void);

#endif
