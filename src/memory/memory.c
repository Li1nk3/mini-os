#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "memory.h"
#include "../common/util.h"

/* ====================== 动态分区管理 ====================== */

void mem_init(MemoryPool *m, int total) {
    m->n = 1;
    m->total = total;
    m->blocks[0].id = 1;
    m->blocks[0].start = 0;
    m->blocks[0].size = total;
    m->blocks[0].free = 1;
    m->blocks[0].pid = -1;
}

static void renumber(MemoryPool *m) {
    for (int i = 0; i < m->n; i++) m->blocks[i].id = i + 1;
}

/* 在第 i 个分区中切出 size 大小给 pid, 剩余作为新空闲分区 */
static void split_block(MemoryPool *m, int i, int pid, int size) {
    if (m->blocks[i].size == size) {
        m->blocks[i].free = 0;
        m->blocks[i].pid = pid;
        return;
    }
    /* 在 i 之后插入一个空闲分区 */
    if (m->n >= MAX_BLOCKS) return;
    for (int j = m->n; j > i + 1; j--) m->blocks[j] = m->blocks[j - 1];
    m->blocks[i + 1].start = m->blocks[i].start + size;
    m->blocks[i + 1].size  = m->blocks[i].size  - size;
    m->blocks[i + 1].free  = 1;
    m->blocks[i + 1].pid   = -1;

    m->blocks[i].size = size;
    m->blocks[i].free = 0;
    m->blocks[i].pid = pid;
    m->n++;
    renumber(m);
}

int mem_alloc(MemoryPool *m, int pid, int size, int strategy) {
    int pick = -1;
    if (strategy == 0) {
        /* First Fit */
        for (int i = 0; i < m->n; i++) {
            if (m->blocks[i].free && m->blocks[i].size >= size) {
                pick = i;
                break;
            }
        }
    } else if (strategy == 1) {
        /* Best Fit */
        int best = INT_MAX;
        for (int i = 0; i < m->n; i++) {
            if (m->blocks[i].free && m->blocks[i].size >= size && m->blocks[i].size < best) {
                best = m->blocks[i].size;
                pick = i;
            }
        }
    } else {
        /* Worst Fit */
        int worst = -1;
        for (int i = 0; i < m->n; i++) {
            if (m->blocks[i].free && m->blocks[i].size >= size && m->blocks[i].size > worst) {
                worst = m->blocks[i].size;
                pick = i;
            }
        }
    }
    if (pick < 0) return -1;
    split_block(m, pick, pid, size);
    return m->blocks[pick].start;
}

/* 与左右相邻空闲分区合并 */
static void merge_free(MemoryPool *m) {
    for (int i = 0; i < m->n - 1; ) {
        if (m->blocks[i].free && m->blocks[i + 1].free) {
            m->blocks[i].size += m->blocks[i + 1].size;
            for (int j = i + 1; j < m->n - 1; j++) m->blocks[j] = m->blocks[j + 1];
            m->n--;
        } else {
            i++;
        }
    }
    renumber(m);
}

int mem_free(MemoryPool *m, int pid) {
    int freed = 0;
    for (int i = 0; i < m->n; i++) {
        if (!m->blocks[i].free && m->blocks[i].pid == pid) {
            m->blocks[i].free = 1;
            m->blocks[i].pid = -1;
            freed++;
        }
    }
    if (freed) merge_free(m);
    return freed;
}

void mem_print(const MemoryPool *m) {
    int used = 0, free_size = 0;
    printf("\n %-4s %-10s %-10s %-8s %-8s\n", "ID", "起址", "大小", "状态", "PID");
    print_divider();
    for (int i = 0; i < m->n; i++) {
        printf(" %-4d %-10d %-10d %-8s %-8d\n",
               m->blocks[i].id, m->blocks[i].start, m->blocks[i].size,
               m->blocks[i].free ? "空闲" : "占用",
               m->blocks[i].pid);
        if (m->blocks[i].free) free_size += m->blocks[i].size;
        else used += m->blocks[i].size;
    }
    print_divider();
    printf(" 总内存: %d  已用: %d  空闲: %d  利用率: %.1f%%\n",
           m->total, used, free_size, 100.0 * used / m->total);
}

/* ====================== 页面置换 ====================== */

static void print_frames(int *frames, int k, int ref, int fault) {
    printf("  访问 %2d %s | 帧: [", ref, fault ? "缺页" : "命中");
    for (int i = 0; i < k; i++) {
        if (frames[i] == -1) printf(" - ");
        else printf("%2d ", frames[i]);
    }
    printf("]\n");
}

int run_fifo(int *refs, int n, int frames, int verbose) {
    int frame[MAX_FRAMES];
    for (int i = 0; i < frames; i++) frame[i] = -1;
    int next = 0;     /* 下一个被替换的位置 */
    int count = 0;    /* 当前已装入 */
    int faults = 0;
    for (int i = 0; i < n; i++) {
        int hit = 0;
        for (int k = 0; k < count; k++) if (frame[k] == refs[i]) { hit = 1; break; }
        if (!hit) {
            faults++;
            if (count < frames) {
                frame[count++] = refs[i];
            } else {
                frame[next] = refs[i];
                next = (next + 1) % frames;
            }
        }
        if (verbose) print_frames(frame, frames, refs[i], !hit);
    }
    return faults;
}

int run_lru(int *refs, int n, int frames, int verbose) {
    int frame[MAX_FRAMES];
    int last_used[MAX_FRAMES];
    for (int i = 0; i < frames; i++) { frame[i] = -1; last_used[i] = -1; }
    int count = 0, faults = 0;
    for (int i = 0; i < n; i++) {
        int hit = -1;
        for (int k = 0; k < count; k++) if (frame[k] == refs[i]) { hit = k; break; }
        if (hit >= 0) {
            last_used[hit] = i;
            if (verbose) print_frames(frame, frames, refs[i], 0);
        } else {
            faults++;
            if (count < frames) {
                frame[count] = refs[i];
                last_used[count] = i;
                count++;
            } else {
                /* 找最久未使用 */
                int victim = 0;
                for (int k = 1; k < frames; k++)
                    if (last_used[k] < last_used[victim]) victim = k;
                frame[victim] = refs[i];
                last_used[victim] = i;
            }
            if (verbose) print_frames(frame, frames, refs[i], 1);
        }
    }
    return faults;
}

int run_opt(int *refs, int n, int frames, int verbose) {
    int frame[MAX_FRAMES];
    for (int i = 0; i < frames; i++) frame[i] = -1;
    int count = 0, faults = 0;
    for (int i = 0; i < n; i++) {
        int hit = 0;
        for (int k = 0; k < count; k++) if (frame[k] == refs[i]) { hit = 1; break; }
        if (!hit) {
            faults++;
            if (count < frames) {
                frame[count++] = refs[i];
            } else {
                /* 选取未来最远才用到(或不再使用)的页面替换 */
                int victim = 0, farthest = -1;
                for (int k = 0; k < frames; k++) {
                    int next_use = INT_MAX;
                    for (int j = i + 1; j < n; j++) {
                        if (refs[j] == frame[k]) { next_use = j; break; }
                    }
                    if (next_use > farthest) {
                        farthest = next_use;
                        victim = k;
                    }
                }
                frame[victim] = refs[i];
            }
        }
        if (verbose) print_frames(frame, frames, refs[i], !hit);
    }
    return faults;
}

/* ====================== 菜单 ====================== */

static void partition_menu(void) {
    int total = read_int_default("内存总大小", 640);
    int strat_op = 0;
    print_title("动态分区分配演示");
    printf("  1) 首次适应 First-Fit\n");
    printf("  2) 最佳适应 Best-Fit\n");
    printf("  3) 最坏适应 Worst-Fit\n");
    printf("  4) 三种策略对比\n");
    strat_op = read_int("选择策略: ");
    if (strat_op < 1 || strat_op > 4) { printf(COLOR_RED "无效\n" COLOR_RESET); return; }

    /* 收集操作序列 */
    typedef struct { int op; int pid; int size; } Req;
    Req reqs[128];
    int rn = 0;
    printf("请输入请求序列(op:1=申请,2=释放; 输入0结束):\n");
    while (rn < 128) {
        int op = read_int("op (1申请/2释放/0结束): ");
        if (op == 0) break;
        if (op != 1 && op != 2) continue;
        reqs[rn].op = op;
        reqs[rn].pid = read_int("  PID: ");
        if (op == 1) reqs[rn].size = read_int("  申请大小: ");
        else reqs[rn].size = 0;
        rn++;
    }

    int strategies[3];
    int sn = 0;
    if (strat_op == 4) {
        strategies[sn++] = 0; strategies[sn++] = 1; strategies[sn++] = 2;
    } else {
        strategies[sn++] = strat_op - 1;
    }
    const char *names[] = {"First-Fit", "Best-Fit", "Worst-Fit"};
    for (int s = 0; s < sn; s++) {
        MemoryPool m;
        mem_init(&m, total);
        printf("\n");
        print_divider();
        printf(COLOR_BOLD "策略: %s\n" COLOR_RESET, names[strategies[s]]);
        print_divider();
        for (int i = 0; i < rn; i++) {
            if (reqs[i].op == 1) {
                int addr = mem_alloc(&m, reqs[i].pid, reqs[i].size, strategies[s]);
                if (addr < 0)
                    printf("申请: PID=%d size=%d --> " COLOR_RED "失败\n" COLOR_RESET, reqs[i].pid, reqs[i].size);
                else
                    printf("申请: PID=%d size=%d --> 起址=%d\n", reqs[i].pid, reqs[i].size, addr);
            } else {
                int k = mem_free(&m, reqs[i].pid);
                printf("释放: PID=%d --> 释放%d个分区\n", reqs[i].pid, k);
            }
            mem_print(&m);
        }
    }
}

static void page_replace_menu(void) {
    print_title("页面置换演示");
    int n = read_int("访问串长度: ");
    if (n < 1 || n > MAX_PAGES_REF) { printf(COLOR_RED "无效\n" COLOR_RESET); return; }
    int refs[MAX_PAGES_REF];
    printf("请输入 %d 个页面号(空格或换行分隔):\n", n);
    for (int i = 0; i < n; i++) {
        if (scanf("%d", &refs[i]) != 1) { i--; clear_input_buffer(); }
    }
    clear_input_buffer();
    int frames = read_int("物理块数: ");
    if (frames < 1 || frames > MAX_FRAMES) { printf(COLOR_RED "无效\n" COLOR_RESET); return; }
    int verbose = read_int_default("是否输出每步过程(1是/0否)", 1);

    printf("\n");
    print_divider();
    printf(COLOR_BOLD "FIFO\n" COLOR_RESET);
    print_divider();
    int f_fifo = run_fifo(refs, n, frames, verbose);
    printf("\n");
    print_divider();
    printf(COLOR_BOLD "LRU\n" COLOR_RESET);
    print_divider();
    int f_lru = run_lru(refs, n, frames, verbose);
    printf("\n");
    print_divider();
    printf(COLOR_BOLD "OPT (最佳)\n" COLOR_RESET);
    print_divider();
    int f_opt = run_opt(refs, n, frames, verbose);

    printf("\n=== 缺页统计 ===\n");
    printf("访问 %d 次,物理块 %d\n", n, frames);
    printf("FIFO: 缺页 %d 次, 缺页率 %.2f%%\n", f_fifo, 100.0 * f_fifo / n);
    printf("LRU : 缺页 %d 次, 缺页率 %.2f%%\n", f_lru,  100.0 * f_lru  / n);
    printf("OPT : 缺页 %d 次, 缺页率 %.2f%%\n", f_opt,  100.0 * f_opt  / n);
}

void memory_menu(void) {
    for (;;) {
        print_title("内存管理模拟");
        printf("  1) 动态分区分配 (FF/BF/WF)\n");
        printf("  2) 页面置换 (FIFO/LRU/OPT)\n");
        printf("  0) 返回主菜单\n");
        int op = read_int("选择: ");
        if (op == 0) return;
        if (op == 1) partition_menu();
        else if (op == 2) page_replace_menu();
        else { printf(COLOR_RED "无效\n" COLOR_RESET); continue; }
        pause_screen();
    }
}
