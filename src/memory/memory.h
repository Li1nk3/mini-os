#ifndef MEMORY_H
#define MEMORY_H

#define MAX_BLOCKS 256
#define MAX_PAGES_REF 1024
#define MAX_FRAMES 64

typedef struct {
    int id;           /* 分区编号 */
    int start;        /* 起始地址 */
    int size;         /* 大小 */
    int free;         /* 1=空闲, 0=占用 */
    int pid;          /* 占用进程,-1表示空闲 */
} Block;

typedef struct {
    Block blocks[MAX_BLOCKS];
    int n;
    int total;
} MemoryPool;

void memory_menu(void);

/* 分区管理 */
void mem_init(MemoryPool *m, int total);
int  mem_alloc(MemoryPool *m, int pid, int size, int strategy); /* 0=FF 1=BF 2=WF */
int  mem_free(MemoryPool *m, int pid);
void mem_print(const MemoryPool *m);

/* 页面置换 */
int run_fifo(int *refs, int n, int frames, int verbose);
int run_lru (int *refs, int n, int frames, int verbose);
int run_opt (int *refs, int n, int frames, int verbose);

#endif
