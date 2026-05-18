#ifndef SCHEDULING_H
#define SCHEDULING_H

#define MAX_PROCESSES 64
#define MAX_GANTT     2048

typedef struct {
    int pid;
    int arrival;      /* 到达时间 */
    int burst;        /* CPU服务时间 */
    int priority;     /* 优先级,数值越小优先级越高 */
    int remaining;    /* 剩余服务时间(用于RR) */
    int start;        /* 首次开始时间 */
    int finish;       /* 完成时间 */
    int waiting;      /* 等待时间 */
    int turnaround;   /* 周转时间 */
    int response;     /* 响应时间 */
    int started;      /* 是否已首次执行过 */
} Process;

typedef struct {
    int pid;
    int begin;
    int end;
} GanttSlice;

typedef struct {
    GanttSlice slices[MAX_GANTT];
    int n;
} GanttChart;

void scheduling_menu(void);

/* 单独算法接口,便于测试 */
void run_fcfs(Process *p, int n, GanttChart *g);
void run_sjf(Process *p, int n, GanttChart *g);
void run_rr(Process *p, int n, int quantum, GanttChart *g);
void run_priority(Process *p, int n, GanttChart *g);

void print_gantt(const GanttChart *g);
void print_metrics(const Process *p, int n);

#endif
