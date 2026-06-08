#ifndef FS_H
#define FS_H

#include <stdint.h>

#define FS_BLOCK_SIZE   512
#define FS_TOTAL_BLOCKS 1024
#define FS_MAX_INODES   128
#define FS_MAX_NAME     28
#define FS_MAX_DENTRY   128  /* 8块 × 16条/块 */
#define FS_DIRECT_BLOCKS 8

#define FS_T_FILE 1
#define FS_T_DIR  2

typedef struct {
    uint32_t magic;
    uint32_t total_blocks;
    uint32_t block_size;
    uint32_t inode_count;
    uint32_t bitmap_block;    /* 位图所在块 */
    uint32_t inode_block;     /* inode表起始块 */
    uint32_t data_block;      /* 数据区起始块 */
    uint32_t root_inode;
} SuperBlock;

typedef struct {
    uint32_t type;       /* 0=空闲 1=文件 2=目录 */
    uint32_t size;       /* 字节数 */
    uint32_t blocks[FS_DIRECT_BLOCKS];
    uint32_t block_count;
    uint32_t parent;     /* 父目录inode */
    char     name[FS_MAX_NAME];
} Inode;

/* 目录项放在目录的数据块中 */
typedef struct {
    uint32_t inode;
    char     name[FS_MAX_NAME];
} DirEntry;

void filesystem_menu(void);

#endif
