#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include "scheduling.h"
#include "../common/util.h"

static void gantt_add(GanttChart *g, int pid, int begin, int end) {
    if (begin >= end) return;
    /* 合并相邻相同pid的片段,便于显示 */
    if (g->n > 0 && g->slices[g->n - 1].pid == pid &&
        g->slices[g->n - 1].end == begin) {
        g->slices[g->n - 1].end = end;
        return;
    }
    if (g->n >= MAX_GANTT) return;
    g->slices[g->n].pid = pid;
    g->slices[g->n].begin = begin;
    g->slices[g->n].end = end;
    g->n++;
}

static void compute_metrics(Process *p, int n) {
    for (int i = 0; i < n; i++) {
        p[i].turnaround = p[i].finish - p[i].arrival;
        p[i].waiting = p[i].turnaround - p[i].burst;
        p[i].response = p[i].start - p[i].arrival;
    }
}

/* ---------------- FCFS ---------------- */
void run_fcfs(Process *p, int n, GanttChart *g) {
    /* 按到达时间排序 */
    int idx[MAX_PROCESSES];
    for (int i = 0; i < n; i++) idx[i] = i;
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - 1 - i; j++) {
            if (p[idx[j]].arrival > p[idx[j + 1]].arrival) {
                int t = idx[j]; idx[j] = idx[j + 1]; idx[j + 1] = t;
            }
        }
    }
    int t = 0;
    for (int k = 0; k < n; k++) {
        Process *cur = &p[idx[k]];
        if (t < cur->arrival) t = cur->arrival;
        cur->start = t;
        cur->started = 1;
        gantt_add(g, cur->pid, t, t + cur->burst);
        t += cur->burst;
        cur->finish = t;
    }
    compute_metrics(p, n);
}

/* ---------------- SJF (非抢占) ---------------- */
void run_sjf(Process *p, int n, GanttChart *g) {
    int done = 0, t = 0;
    int finished[MAX_PROCESSES] = {0};
    while (done < n) {
        int pick = -1, best = INT_MAX;
        for (int i = 0; i < n; i++) {
            if (!finished[i] && p[i].arrival <= t) {
                if (p[i].burst < best) {
                    best = p[i].burst;
                    pick = i;
                }
            }
        }
        if (pick < 0) {
            /* 没有就绪任务,推进时钟到最近到达 */
            int next = INT_MAX;
            for (int i = 0; i < n; i++)
                if (!finished[i] && p[i].arrival < next) next = p[i].arrival;
            if (next == INT_MAX) break;
            t = next;
            continue;
        }
        p[pick].start = t;
        p[pick].started = 1;
        gantt_add(g, p[pick].pid, t, t + p[pick].burst);
        t += p[pick].burst;
        p[pick].finish = t;
        finished[pick] = 1;
        done++;
    }
    compute_metrics(p, n);
}

/* ---------------- 时间片轮转 RR ---------------- */
void run_rr(Process *p, int n, int quantum, GanttChart *g) {
    if (quantum < 1) quantum = 1;
    int total_burst = 0;
    for (int i = 0; i < n; i++) total_burst += p[i].burst;
    int qsize = total_burst / quantum + n + 1;
    int *queue = malloc(qsize * sizeof(int));
    if (!queue) return;
    int head = 0, tail = 0;
    int in_queue[MAX_PROCESSES] = {0};
    int finished = 0;
    int t = 0;

    for (int i = 0; i < n; i++) p[i].remaining = p[i].burst;

    /* 找到第一个到达的进程,推进时钟 */
    int first = INT_MAX;
    for (int i = 0; i < n; i++) if (p[i].arrival < first) first = p[i].arrival;
    if (first == INT_MAX) return;
    t = first;
    /* 把所有到达时间为t的进程入队 */
    for (int i = 0; i < n; i++) {
        if (p[i].arrival <= t && !in_queue[i]) {
            queue[tail++] = i;
            in_queue[i] = 1;
        }
    }

    while (finished < n) {
        if (head == tail) {
            /* 队列为空,推进时钟 */
            int next = INT_MAX;
            for (int i = 0; i < n; i++)
                if (!in_queue[i] && p[i].arrival < next) next = p[i].arrival;
            if (next == INT_MAX) break;
            t = next;
            for (int i = 0; i < n; i++) {
                if (p[i].arrival <= t && !in_queue[i] && p[i].remaining > 0) {
                    queue[tail++] = i;
                    in_queue[i] = 1;
                }
            }
            continue;
        }
        int idx = queue[head++];
        if (!p[idx].started) {
            p[idx].start = t;
            p[idx].started = 1;
        }
        int run = (p[idx].remaining < quantum) ? p[idx].remaining : quantum;
        gantt_add(g, p[idx].pid, t, t + run);
        t += run;
        p[idx].remaining -= run;
        /* 把这段时间内新到达的进程入队 */
        for (int i = 0; i < n; i++) {
            if (!in_queue[i] && p[i].arrival <= t && p[i].remaining > 0) {
                queue[tail++] = i;
                in_queue[i] = 1;
            }
        }
        if (p[idx].remaining == 0) {
            p[idx].finish = t;
            finished++;
        } else {
            queue[tail++] = idx;  /* 重新入队 */
        }
    }
    free(queue);
    compute_metrics(p, n);
}

/* ---------------- 优先级调度 (非抢占,数值小优先) ---------------- */
void run_priority(Process *p, int n, GanttChart *g) {
    int done = 0, t = 0;
    int finished[MAX_PROCESSES] = {0};
    while (done < n) {
        int pick = -1, best = INT_MAX;
        for (int i = 0; i < n; i++) {
            if (!finished[i] && p[i].arrival <= t && p[i].priority < best) {
                best = p[i].priority;
                pick = i;
            }
        }
        if (pick < 0) {
            int next = INT_MAX;
            for (int i = 0; i < n; i++)
                if (!finished[i] && p[i].arrival < next) next = p[i].arrival;
            if (next == INT_MAX) break;
            t = next;
            continue;
        }
        p[pick].start = t;
        p[pick].started = 1;
        gantt_add(g, p[pick].pid, t, t + p[pick].burst);
        t += p[pick].burst;
        p[pick].finish = t;
        finished[pick] = 1;
        done++;
    }
    compute_metrics(p, n);
}

/* ---------------- 输出 ---------------- */
void print_gantt(const GanttChart *g) {
    if (g->n == 0) return;
    printf("\n甘特图:\n");
    /* 上沿 */
    for (int i = 0; i < g->n; i++) {
        int len = g->slices[i].end - g->slices[i].begin;
        if (len < 1) len = 1;
        printf("+");
        for (int k = 0; k < len * 3; k++) printf("-");
    }
    printf("+\n");
    /* 中间 pid 行 */
    for (int i = 0; i < g->n; i++) {
        int len = g->slices[i].end - g->slices[i].begin;
        if (len < 1) len = 1;
        int width = len * 3;
        printf("|");
        char buf[16];
        snprintf(buf, sizeof(buf), "P%d", g->slices[i].pid);
        int pad = (width - (int)strlen(buf)) / 2;
        for (int k = 0; k < pad; k++) printf(" ");
        printf("%s", buf);
        for (int k = pad + (int)strlen(buf); k < width; k++) printf(" ");
    }
    printf("|\n");
    /* 下沿 */
    for (int i = 0; i < g->n; i++) {
        int len = g->slices[i].end - g->slices[i].begin;
        if (len < 1) len = 1;
        printf("+");
        for (int k = 0; k < len * 3; k++) printf("-");
    }
    printf("+\n");
    /* 时间轴 */
    printf("%-3d", g->slices[0].begin);
    for (int i = 0; i < g->n; i++) {
        int len = g->slices[i].end - g->slices[i].begin;
        if (len < 1) len = 1;
        int width = len * 3;
        for (int k = 0; k < width - 3; k++) printf(" ");
        printf("%-3d", g->slices[i].end);
    }
    printf("\n");
}

void print_metrics(const Process *p, int n) {
    printf("\n%-6s %-10s %-10s %-10s %-12s %-12s %-12s %-12s\n",
           "PID", "到达", "服务", "完成", "周转时间", "等待时间", "响应时间", "带权周转");
    print_divider();
    double sum_tat = 0, sum_wait = 0, sum_resp = 0, sum_wtat = 0;
    for (int i = 0; i < n; i++) {
        double wtat = p[i].burst > 0 ? (double)p[i].turnaround / p[i].burst : 0.0;
        printf("%-6d %-10d %-10d %-10d %-12d %-12d %-12d %-12.2f\n",
               p[i].pid, p[i].arrival, p[i].burst, p[i].finish,
               p[i].turnaround, p[i].waiting, p[i].response, wtat);
        sum_tat += p[i].turnaround;
        sum_wait += p[i].waiting;
        sum_resp += p[i].response;
        sum_wtat += wtat;
    }
    print_divider();
    printf("平均周转时间 : %.2f\n", sum_tat / n);
    printf("平均等待时间 : %.2f\n", sum_wait / n);
    printf("平均响应时间 : %.2f\n", sum_resp / n);
    printf("平均带权周转 : %.2f\n", sum_wtat / n);
}

/* ---------------- 输入 ---------------- */
static int input_processes(Process *p, int need_priority) {
    int n = read_int("请输入进程数量 (1-" "64" "): ");
    if (n < 1) n = 1;
    if (n > MAX_PROCESSES) n = MAX_PROCESSES;
    for (int i = 0; i < n; i++) {
        printf("--- 进程 %d ---\n", i + 1);
        p[i].pid = read_int_default("  PID", i + 1);
        p[i].arrival = read_int("  到达时间: ");
        p[i].burst = read_int("  服务时间(CPU): ");
        while (p[i].burst < 1) {
            printf(COLOR_RED "  服务时间必须 >= 1\n" COLOR_RESET);
            p[i].burst = read_int("  服务时间(CPU): ");
        }
        if (need_priority)
            p[i].priority = read_int("  优先级(数值越小越高): ");
        else
            p[i].priority = 0;
        p[i].started = 0;
    }
    return n;
}

static void reset_processes(Process *p, int n) {
    for (int i = 0; i < n; i++) {
        p[i].start = p[i].finish = 0;
        p[i].waiting = p[i].turnaround = p[i].response = 0;
        p[i].started = 0;
        p[i].remaining = p[i].burst;
    }
}

static void copy_processes(const Process *src, Process *dst, int n) {
    for (int i = 0; i < n; i++) dst[i] = src[i];
}

static void run_compare_all(Process *p, int n, int quantum) {
    Process work[MAX_PROCESSES];
    GanttChart g;
    const char *names[] = {"FCFS", "SJF (非抢占)", "RR", "优先级 (非抢占)"};
    for (int alg = 0; alg < 4; alg++) {
        copy_processes(p, work, n);
        reset_processes(work, n);
        memset(&g, 0, sizeof(g));
        printf("\n");
        print_divider();
        printf(COLOR_BOLD "算法: %s\n" COLOR_RESET, names[alg]);
        print_divider();
        switch (alg) {
        case 0: run_fcfs(work, n, &g); break;
        case 1: run_sjf(work, n, &g); break;
        case 2: run_rr(work, n, quantum, &g); break;
        case 3: run_priority(work, n, &g); break;
        }
        print_gantt(&g);
        print_metrics(work, n);
    }
}

void scheduling_menu(void) {
    for (;;) {
        print_title("处理机调度模拟");
        printf("  1) 先来先服务 FCFS\n");
        printf("  2) 短作业优先 SJF (非抢占)\n");
        printf("  3) 时间片轮转 RR\n");
        printf("  4) 优先级调度 (非抢占)\n");
        printf("  5) 全部算法对比 (使用同一组进程)\n");
        printf("  0) 返回主菜单\n");
        int op = read_int("选择: ");
        if (op == 0) return;
        if (op < 1 || op > 5) { printf(COLOR_RED "无效\n" COLOR_RESET); continue; }

        Process p[MAX_PROCESSES];
        int need_priority = (op == 4 || op == 5);
        int n = input_processes(p, need_priority);
        int quantum = 2;
        if (op == 3 || op == 5) {
            quantum = read_int("请输入时间片大小: ");
            if (quantum < 1) quantum = 1;
        }

        if (op == 5) {
            run_compare_all(p, n, quantum);
            pause_screen();
            continue;
        }

        GanttChart g;
        memset(&g, 0, sizeof(g));
        reset_processes(p, n);
        switch (op) {
        case 1: run_fcfs(p, n, &g); break;
        case 2: run_sjf(p, n, &g); break;
        case 3: run_rr(p, n, quantum, &g); break;
        case 4: run_priority(p, n, &g); break;
        }
        print_gantt(&g);
        print_metrics(p, n);
        pause_screen();
    }
}
