#include <stdio.h>
#include <stdlib.h>
#include "common/util.h"
#include "scheduling/scheduling.h"
#include "memory/memory.h"
#include "sync/sync.h"
#include "filesystem/fs.h"

static void banner(void) {
    printf(COLOR_BOLD COLOR_CYAN);
    printf("\n");
    printf("==================================================\n");
    printf("        操作系统课程设计 - 综合演示系统\n");
    printf("        OS Course Design - Demo System\n");
    printf("==================================================\n");
    printf(COLOR_RESET);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    setvbuf(stdout, NULL, _IONBF, 0);
    for (;;) {
        banner();
        printf("  1) 处理机调度  (FCFS / SJF / RR / Priority)\n");
        printf("  2) 内存管理    (动态分区 + 页面置换)\n");
        printf("  3) 进程同步    (PC / RW / 哲学家)\n");
        printf("  4) 文件系统    (简易磁盘镜像 FS)\n");
        printf("  0) 退出\n");
        int op = read_int("请选择: ");
        switch (op) {
        case 0: printf("再见\n"); return 0;
        case 1: scheduling_menu(); break;
        case 2: memory_menu(); break;
        case 3: sync_menu(); break;
        case 4: filesystem_menu(); break;
        default: printf(COLOR_RED "无效选项\n" COLOR_RESET);
        }
    }
}
