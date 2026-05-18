/* 自动化测试: 内存管理 */
#include <stdio.h>
#include <assert.h>
#include "../src/memory/memory.h"

int main(void) {
    /* 分区: 总640 */
    MemoryPool m;
    mem_init(&m, 640);
    int a1 = mem_alloc(&m, 1, 130, 0);  /* FF: start=0 */
    int a2 = mem_alloc(&m, 2, 60, 0);   /* start=130 */
    int a3 = mem_alloc(&m, 3, 100, 0);  /* start=190 */
    assert(a1 == 0);
    assert(a2 == 130);
    assert(a3 == 190);
    assert(mem_free(&m, 2) == 1);
    /* 现在有空洞 [130,190) */
    int a4 = mem_alloc(&m, 4, 50, 0);   /* FF优先选第一个能放下的 */
    assert(a4 == 130);

    /* Best-Fit 测试 */
    MemoryPool m2;
    mem_init(&m2, 1000);
    mem_alloc(&m2, 1, 100, 0); /* 0..100 */
    mem_alloc(&m2, 2, 50, 0);  /* 100..150 */
    mem_alloc(&m2, 3, 200, 0); /* 150..350 */
    mem_free(&m2, 1);          /* 释放 0..100 (100B空闲) */
    mem_free(&m2, 3);          /* 释放 150..350 (200B空闲) */
    int b = mem_alloc(&m2, 4, 80, 1); /* BF应选100B那块,start=0 */
    assert(b == 0);
    int w = mem_alloc(&m2, 5, 80, 2); /* WF应选最大那块,start=150 */
    assert(w == 150);

    /* 页面置换 */
    int refs[] = {7,0,1,2,0,3,0,4,2,3,0,3,2,1,2,0,1,7,0,1};
    int n = sizeof(refs)/sizeof(refs[0]);
    int f_fifo = run_fifo(refs, n, 3, 0);
    int f_lru  = run_lru(refs, n, 3, 0);
    int f_opt  = run_opt(refs, n, 3, 0);
    printf("FIFO faults=%d  LRU faults=%d  OPT faults=%d\n", f_fifo, f_lru, f_opt);
    /* 经典样例: FIFO=15, LRU=12, OPT=9 */
    assert(f_fifo == 15);
    assert(f_lru  == 12);
    assert(f_opt  ==  9);

    printf("\n所有内存测试通过\n");
    return 0;
}
