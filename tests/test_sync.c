/* 自动化测试: 进程同步 */
#include <stdio.h>
#include <assert.h>
#include "../src/sync/sync.h"

int main(void) {
    int ret = sync_selftest();
    assert(ret == 0);
    printf("\n所有同步测试通过\n");
    return 0;
}
