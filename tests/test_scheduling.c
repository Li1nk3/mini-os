/* 自动化测试: 调度算法 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../src/scheduling/scheduling.h"

static int total_burst(Process *p, int n) {
    int t = 0;
    for (int i = 0; i < n; i++) t += p[i].burst;
    return t;
}

static int max_finish(Process *p, int n) {
    int m = 0;
    for (int i = 0; i < n; i++) if (p[i].finish > m) m = p[i].finish;
    return m;
}

static void reset(Process *p, int n) {
    for (int i = 0; i < n; i++) {
        p[i].start = p[i].finish = 0;
        p[i].waiting = p[i].turnaround = p[i].response = 0;
        p[i].started = 0;
        p[i].remaining = p[i].burst;
    }
}

int main(void) {
    /* 经典示例 */
    Process p[] = {
        {1, 0, 4, 3, 0,0,0,0,0,0,0},
        {2, 1, 3, 1, 0,0,0,0,0,0,0},
        {3, 2, 5, 4, 0,0,0,0,0,0,0},
        {4, 3, 2, 2, 0,0,0,0,0,0,0},
    };
    int n = 4;
    int tb = total_burst(p, n);

    GanttChart g;

    /* FCFS */
    memset(&g, 0, sizeof(g));
    reset(p, n);
    run_fcfs(p, n, &g);
    int mf = max_finish(p, n);
    printf("FCFS: makespan=%d (expected >= %d)\n", mf, tb);
    assert(mf == tb);
    /* P1 完成时间 = 4 */
    assert(p[0].finish == 4);

    /* SJF */
    memset(&g, 0, sizeof(g));
    reset(p, n);
    run_sjf(p, n, &g);
    mf = max_finish(p, n);
    printf("SJF : makespan=%d\n", mf);
    assert(mf == tb);

    /* RR 时间片=2 */
    memset(&g, 0, sizeof(g));
    reset(p, n);
    run_rr(p, n, 2, &g);
    mf = max_finish(p, n);
    printf("RR  : makespan=%d\n", mf);
    assert(mf == tb);

    /* Priority */
    memset(&g, 0, sizeof(g));
    reset(p, n);
    run_priority(p, n, &g);
    mf = max_finish(p, n);
    printf("PRI : makespan=%d\n", mf);
    assert(mf == tb);

    printf("\n所有调度测试通过\n");
    return 0;
}
