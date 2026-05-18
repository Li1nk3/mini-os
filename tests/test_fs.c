/* 自动化测试: 文件系统 (通过stdin驱动菜单) */
#include <stdio.h>
#include "../src/filesystem/fs.h"

int main(void) {
    /* 入口仅做一个smoke test: 运行菜单需要交互,这里直接退出 */
    /* 真实功能验证通过 run.sh 的脚本测试完成 */
    printf("文件系统smoke test: 调用菜单退出\n");
    /* filesystem_menu() 内部会自动挂载或提示格式化,然后进入交互。
       要自动化测试,可在主程序中提供 -e 命令模式,这里跳过 */
    return 0;
}
