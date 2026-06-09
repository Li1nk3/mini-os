/* 自动化测试: 文件系统 */
#include <stdio.h>
#include <assert.h>
#include "../src/filesystem/fs.h"

int main(void) {
    int ret = filesystem_selftest();
    assert(ret == 0);
    printf("\n所有文件系统测试通过\n");
    return 0;
}
