#!/bin/bash
# 演示脚本: 通过stdin驱动主程序的文件系统菜单
set -e
cd "$(dirname "$0")/.."
rm -f fs.img

echo "=== 文件系统功能测试 ==="
./build/os_demo <<'EOF'
4
1
5
hello.txt
6
hello.txt
Hello, OS course design!
7
hello.txt
4
docs
9
docs
5
note.md
6
note.md
This is a note.
3
9
..
3
10
11
8
hello.txt
3
0
0
EOF
echo "=== 文件系统测试结束 ==="
