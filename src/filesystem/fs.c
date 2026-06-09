#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include "fs.h"
#include "../common/util.h"

#define FS_MAGIC 0x4F534653  /* "OSFS" */
#define DEFAULT_IMG "fs.img"

/* 安全复制到固定长度字段,保证终止符 */
static void name_copy(char *dst, size_t cap, const char *src) {
    if (cap == 0) return;
    size_t i = 0;
    for (; i + 1 < cap && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
}

static FILE *g_disk = NULL;
static SuperBlock g_sb;
static uint8_t   *g_bitmap = NULL;
static Inode     *g_inodes = NULL;
static uint32_t   g_cwd = 0;          /* 当前目录inode号 */
static char       g_cwd_path[512] = "/";

/* ------- 工具 ------- */
static void disk_read_block(uint32_t blk, void *buf) {
    fseek(g_disk, blk * FS_BLOCK_SIZE, SEEK_SET);
    if (fread(buf, FS_BLOCK_SIZE, 1, g_disk) != 1) {
        memset(buf, 0, FS_BLOCK_SIZE);
    }
}
static void disk_write_block(uint32_t blk, const void *buf) {
    fseek(g_disk, blk * FS_BLOCK_SIZE, SEEK_SET);
    fwrite(buf, FS_BLOCK_SIZE, 1, g_disk);
    fflush(g_disk);
}

static int bitmap_blocks_needed(void) {
    int bits = FS_TOTAL_BLOCKS;
    int bytes = (bits + 7) / 8;
    int blks = (bytes + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;
    return blks;
}
static int inode_blocks_needed(void) {
    int bytes = FS_MAX_INODES * (int)sizeof(Inode);
    return (bytes + FS_BLOCK_SIZE - 1) / FS_BLOCK_SIZE;
}

static void bitmap_set(uint32_t blk, int v) {
    if (v) g_bitmap[blk / 8] |=  (1 << (blk % 8));
    else   g_bitmap[blk / 8] &= ~(1 << (blk % 8));
}
static int bitmap_get(uint32_t blk) {
    return (g_bitmap[blk / 8] >> (blk % 8)) & 1;
}

static int alloc_block(void) {
    for (uint32_t b = g_sb.data_block; b < g_sb.total_blocks; b++) {
        if (!bitmap_get(b)) { bitmap_set(b, 1); return (int)b; }
    }
    return -1;
}
static void free_block(uint32_t b) { bitmap_set(b, 0); }

static int alloc_inode(void) {
    for (int i = 0; i < FS_MAX_INODES; i++) {
        if (g_inodes[i].type == 0) return i;
    }
    return -1;
}

static void persist_meta(void) {
    /* 写超级块 */
    uint8_t buf[FS_BLOCK_SIZE];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, &g_sb, sizeof(g_sb));
    disk_write_block(0, buf);
    /* 写位图 */
    int nb = bitmap_blocks_needed();
    uint8_t *p = g_bitmap;
    for (int i = 0; i < nb; i++) {
        memset(buf, 0, sizeof(buf));
        memcpy(buf, p + i * FS_BLOCK_SIZE,
               (i == nb - 1) ? ((FS_TOTAL_BLOCKS + 7) / 8 - i * FS_BLOCK_SIZE) : FS_BLOCK_SIZE);
        disk_write_block(g_sb.bitmap_block + i, buf);
    }
    /* 写inode表 */
    int ib = inode_blocks_needed();
    for (int i = 0; i < ib; i++) {
        memset(buf, 0, sizeof(buf));
        int remain = (int)sizeof(Inode) * FS_MAX_INODES - i * FS_BLOCK_SIZE;
        int copy = remain > FS_BLOCK_SIZE ? FS_BLOCK_SIZE : remain;
        memcpy(buf, (uint8_t *)g_inodes + i * FS_BLOCK_SIZE, copy);
        disk_write_block(g_sb.inode_block + i, buf);
    }
}

static void load_meta(void) {
    uint8_t buf[FS_BLOCK_SIZE];
    disk_read_block(0, buf);
    memcpy(&g_sb, buf, sizeof(g_sb));
    int nb = bitmap_blocks_needed();
    int total_bytes = (FS_TOTAL_BLOCKS + 7) / 8;
    g_bitmap = realloc(g_bitmap, nb * FS_BLOCK_SIZE);
    for (int i = 0; i < nb; i++) {
        disk_read_block(g_sb.bitmap_block + i, g_bitmap + i * FS_BLOCK_SIZE);
    }
    (void)total_bytes;

    int ib = inode_blocks_needed();
    g_inodes = realloc(g_inodes, ib * FS_BLOCK_SIZE);
    for (int i = 0; i < ib; i++) {
        disk_read_block(g_sb.inode_block + i, (uint8_t *)g_inodes + i * FS_BLOCK_SIZE);
    }
}

static int fs_format(const char *path) {
    FILE *f = fopen(path, "wb+");
    if (!f) { perror("fopen"); return -1; }
    /* 创建空镜像 */
    uint8_t z[FS_BLOCK_SIZE];
    memset(z, 0, sizeof(z));
    for (int i = 0; i < FS_TOTAL_BLOCKS; i++) fwrite(z, sizeof(z), 1, f);
    if (g_disk) fclose(g_disk);
    g_disk = f;

    memset(&g_sb, 0, sizeof(g_sb));
    g_sb.magic = FS_MAGIC;
    g_sb.total_blocks = FS_TOTAL_BLOCKS;
    g_sb.block_size = FS_BLOCK_SIZE;
    g_sb.inode_count = FS_MAX_INODES;
    g_sb.bitmap_block = 1;
    int bm = bitmap_blocks_needed();
    g_sb.inode_block = g_sb.bitmap_block + bm;
    int ib = inode_blocks_needed();
    g_sb.data_block = g_sb.inode_block + ib;
    g_sb.root_inode = 0;

    int total_bytes = (FS_TOTAL_BLOCKS + 7) / 8;
    g_bitmap = realloc(g_bitmap, bm * FS_BLOCK_SIZE);
    memset(g_bitmap, 0, bm * FS_BLOCK_SIZE);
    /* 标记元数据区已用 */
    for (uint32_t b = 0; b < g_sb.data_block; b++) bitmap_set(b, 1);
    (void)total_bytes;

    g_inodes = realloc(g_inodes, ib * FS_BLOCK_SIZE);
    memset(g_inodes, 0, ib * FS_BLOCK_SIZE);

    /* 创建根目录 inode 0 */
    Inode *root = &g_inodes[0];
    root->type = FS_T_DIR;
    root->size = 0;
    root->parent = 0;
    name_copy(root->name, FS_MAX_NAME, "/");

    g_cwd = 0;
    strcpy(g_cwd_path, "/");

    persist_meta();
    printf(COLOR_GREEN "已格式化 %s\n" COLOR_RESET, path);
    printf("  总块数 %u  块大小 %u  inode数 %u  数据起始块 %u\n",
           g_sb.total_blocks, g_sb.block_size, g_sb.inode_count, g_sb.data_block);
    return 0;
}

static int fs_mount(const char *path) {
    FILE *f = fopen(path, "rb+");
    if (!f) return -1;
    if (g_disk) fclose(g_disk);
    g_disk = f;
    load_meta();
    if (g_sb.magic != FS_MAGIC) {
        printf(COLOR_RED "镜像magic不匹配,请先格式化\n" COLOR_RESET);
        fclose(g_disk); g_disk = NULL;
        return -1;
    }
    g_cwd = g_sb.root_inode;
    strcpy(g_cwd_path, "/");
    printf(COLOR_GREEN "已挂载 %s\n" COLOR_RESET, path);
    return 0;
}

/* ----- 目录条目读写 ----- */
static int dir_read(uint32_t ino, DirEntry *entries, int *count) {
    Inode *d = &g_inodes[ino];
    *count = 0;
    int per_block = FS_BLOCK_SIZE / (int)sizeof(DirEntry);
    uint8_t buf[FS_BLOCK_SIZE];
    for (uint32_t i = 0; i < d->block_count; i++) {
        disk_read_block(d->blocks[i], buf);
        DirEntry *ent = (DirEntry *)buf;
        for (int j = 0; j < per_block; j++) {
            if (ent[j].name[0] != '\0') {
                entries[(*count)++] = ent[j];
                if (*count >= FS_MAX_DENTRY) return 0;
            }
        }
    }
    return 0;
}

static int dir_add_entry(uint32_t dir_ino, uint32_t child_ino, const char *name) {
    Inode *d = &g_inodes[dir_ino];
    int per_block = FS_BLOCK_SIZE / (int)sizeof(DirEntry);
    uint8_t buf[FS_BLOCK_SIZE];
    for (uint32_t i = 0; i < d->block_count; i++) {
        disk_read_block(d->blocks[i], buf);
        DirEntry *ent = (DirEntry *)buf;
        for (int j = 0; j < per_block; j++) {
            if (ent[j].name[0] == '\0') {
                ent[j].inode = child_ino;
                name_copy(ent[j].name, FS_MAX_NAME, name);
                disk_write_block(d->blocks[i], buf);
                d->size += sizeof(DirEntry);
                return 0;
            }
        }
    }
    /* 需要分配新块 */
    if (d->block_count >= FS_DIRECT_BLOCKS) return -1;
    int nb = alloc_block();
    if (nb < 0) return -1;
    d->blocks[d->block_count++] = nb;
    memset(buf, 0, sizeof(buf));
    DirEntry *ent = (DirEntry *)buf;
    ent[0].inode = child_ino;
    name_copy(ent[0].name, FS_MAX_NAME, name);
    disk_write_block(nb, buf);
    d->size += sizeof(DirEntry);
    return 0;
}

static int dir_remove_entry(uint32_t dir_ino, const char *name) {
    Inode *d = &g_inodes[dir_ino];
    int per_block = FS_BLOCK_SIZE / (int)sizeof(DirEntry);
    uint8_t buf[FS_BLOCK_SIZE];
    for (uint32_t i = 0; i < d->block_count; i++) {
        disk_read_block(d->blocks[i], buf);
        DirEntry *ent = (DirEntry *)buf;
        for (int j = 0; j < per_block; j++) {
            if (strcmp(ent[j].name, name) == 0) {
                memset(&ent[j], 0, sizeof(DirEntry));
                disk_write_block(d->blocks[i], buf);
                if (d->size >= sizeof(DirEntry)) d->size -= sizeof(DirEntry);
                return 0;
            }
        }
    }
    return -1;
}

static int dir_lookup(uint32_t dir_ino, const char *name, uint32_t *out) {
    DirEntry entries[FS_MAX_DENTRY];
    int cnt = 0;
    dir_read(dir_ino, entries, &cnt);
    for (int i = 0; i < cnt; i++) {
        if (strcmp(entries[i].name, name) == 0) {
            *out = entries[i].inode;
            return 0;
        }
    }
    return -1;
}

/* ----- 命令实现 ----- */
static void cmd_ls(void) {
    DirEntry entries[FS_MAX_DENTRY];
    int cnt = 0;
    dir_read(g_cwd, entries, &cnt);
    printf("%-6s %-10s %-10s %s\n", "INODE", "TYPE", "SIZE", "NAME");
    print_divider();
    for (int i = 0; i < cnt; i++) {
        Inode *in = &g_inodes[entries[i].inode];
        printf("%-6u %-10s %-10u %s\n",
               entries[i].inode,
               in->type == FS_T_DIR ? "DIR" : "FILE",
               in->size, entries[i].name);
    }
}

static void cmd_mkdir(const char *name) {
    uint32_t dummy;
    if (dir_lookup(g_cwd, name, &dummy) == 0) {
        printf(COLOR_RED "已存在\n" COLOR_RESET); return;
    }
    int ino = alloc_inode();
    if (ino < 0) { printf(COLOR_RED "无空闲inode\n" COLOR_RESET); return; }
    Inode *n = &g_inodes[ino];
    memset(n, 0, sizeof(*n));
    n->type = FS_T_DIR;
    n->parent = g_cwd;
    name_copy(n->name, FS_MAX_NAME, name);
    if (dir_add_entry(g_cwd, ino, name) < 0) {
        printf(COLOR_RED "添加目录项失败\n" COLOR_RESET);
        n->type = 0;
        return;
    }
    persist_meta();
    printf(COLOR_GREEN "已创建目录 %s (inode=%d)\n" COLOR_RESET, name, ino);
}

static void cmd_create(const char *name) {
    uint32_t dummy;
    if (dir_lookup(g_cwd, name, &dummy) == 0) {
        printf(COLOR_RED "已存在\n" COLOR_RESET); return;
    }
    int ino = alloc_inode();
    if (ino < 0) { printf(COLOR_RED "无空闲inode\n" COLOR_RESET); return; }
    Inode *n = &g_inodes[ino];
    memset(n, 0, sizeof(*n));
    n->type = FS_T_FILE;
    n->parent = g_cwd;
    name_copy(n->name, FS_MAX_NAME, name);
    if (dir_add_entry(g_cwd, ino, name) < 0) {
        printf(COLOR_RED "添加目录项失败\n" COLOR_RESET);
        n->type = 0; return;
    }
    persist_meta();
    printf(COLOR_GREEN "已创建文件 %s (inode=%d)\n" COLOR_RESET, name, ino);
}

static void cmd_write(const char *name, const char *content) {
    uint32_t ino;
    if (dir_lookup(g_cwd, name, &ino) < 0) {
        printf(COLOR_RED "未找到\n" COLOR_RESET); return;
    }
    Inode *n = &g_inodes[ino];
    if (n->type != FS_T_FILE) { printf(COLOR_RED "不是文件\n" COLOR_RESET); return; }
    /* 释放旧数据块 */
    for (uint32_t i = 0; i < n->block_count; i++) free_block(n->blocks[i]);
    n->block_count = 0; n->size = 0;
    int len = (int)strlen(content);
    int written = 0;
    uint8_t buf[FS_BLOCK_SIZE];
    while (written < len) {
        if (n->block_count >= FS_DIRECT_BLOCKS) {
            printf(COLOR_YELLOW "文件过大,截断\n" COLOR_RESET);
            break;
        }
        int blk = alloc_block();
        if (blk < 0) { printf(COLOR_RED "无空闲块\n" COLOR_RESET); break; }
        int chunk = len - written;
        if (chunk > FS_BLOCK_SIZE) chunk = FS_BLOCK_SIZE;
        memset(buf, 0, sizeof(buf));
        memcpy(buf, content + written, chunk);
        disk_write_block(blk, buf);
        n->blocks[n->block_count++] = blk;
        written += chunk;
    }
    n->size = written;
    persist_meta();
    printf(COLOR_GREEN "写入 %d 字节,占用 %u 块\n" COLOR_RESET, written, n->block_count);
}

static void cmd_cat(const char *name) {
    uint32_t ino;
    if (dir_lookup(g_cwd, name, &ino) < 0) {
        printf(COLOR_RED "未找到\n" COLOR_RESET); return;
    }
    Inode *n = &g_inodes[ino];
    if (n->type != FS_T_FILE) { printf(COLOR_RED "不是文件\n" COLOR_RESET); return; }
    uint8_t buf[FS_BLOCK_SIZE + 1];
    int remain = (int)n->size;
    printf("--- %s (%u bytes) ---\n", name, n->size);
    for (uint32_t i = 0; i < n->block_count && remain > 0; i++) {
        disk_read_block(n->blocks[i], buf);
        int chunk = remain > FS_BLOCK_SIZE ? FS_BLOCK_SIZE : remain;
        buf[chunk] = '\0';
        fwrite(buf, 1, chunk, stdout);
        remain -= chunk;
    }
    printf("\n--- end ---\n");
}

static void cmd_rm(const char *name) {
    uint32_t ino;
    if (dir_lookup(g_cwd, name, &ino) < 0) {
        printf(COLOR_RED "未找到\n" COLOR_RESET); return;
    }
    Inode *n = &g_inodes[ino];
    if (n->type == FS_T_DIR) {
        DirEntry entries[FS_MAX_DENTRY];
        int cnt = 0;
        dir_read(ino, entries, &cnt);
        if (cnt > 0) { printf(COLOR_RED "目录非空\n" COLOR_RESET); return; }
        for (uint32_t i = 0; i < n->block_count; i++) free_block(n->blocks[i]);
    } else {
        for (uint32_t i = 0; i < n->block_count; i++) free_block(n->blocks[i]);
    }
    memset(n, 0, sizeof(*n));
    dir_remove_entry(g_cwd, name);
    persist_meta();
    printf(COLOR_GREEN "已删除 %s\n" COLOR_RESET, name);
}

static void cmd_cd(const char *name) {
    if (strcmp(name, "/") == 0) {
        g_cwd = g_sb.root_inode;
        strcpy(g_cwd_path, "/");
        return;
    }
    if (strcmp(name, "..") == 0) {
        if (g_cwd == g_sb.root_inode) return;
        g_cwd = g_inodes[g_cwd].parent;
        /* 重建路径 */
        char path[512] = "/";
        uint32_t cur = g_cwd;
        char stack[16][FS_MAX_NAME];
        int sp = 0;
        while (cur != g_sb.root_inode && sp < 16) {
            name_copy(stack[sp++], FS_MAX_NAME, g_inodes[cur].name);
            cur = g_inodes[cur].parent;
        }
        for (int i = sp - 1; i >= 0; i--) {
            strncat(path, stack[i], sizeof(path) - strlen(path) - 1);
            if (i > 0) strncat(path, "/", sizeof(path) - strlen(path) - 1);
        }
        snprintf(g_cwd_path, sizeof(g_cwd_path), "%s", path);
        return;
    }
    uint32_t ino;
    if (dir_lookup(g_cwd, name, &ino) < 0) { printf(COLOR_RED "未找到\n" COLOR_RESET); return; }
    if (g_inodes[ino].type != FS_T_DIR) { printf(COLOR_RED "不是目录\n" COLOR_RESET); return; }
    g_cwd = ino;
    if (strcmp(g_cwd_path, "/") != 0) strncat(g_cwd_path, "/", sizeof(g_cwd_path) - strlen(g_cwd_path) - 1);
    strncat(g_cwd_path, name, sizeof(g_cwd_path) - strlen(g_cwd_path) - 1);
}

static void cmd_stat(void) {
    int used = 0;
    for (uint32_t b = 0; b < g_sb.total_blocks; b++) if (bitmap_get(b)) used++;
    int used_ino = 0;
    for (int i = 0; i < FS_MAX_INODES; i++) if (g_inodes[i].type) used_ino++;
    printf("镜像统计:\n");
    printf("  总块数 %u, 已用 %d, 空闲 %d\n", g_sb.total_blocks, used, (int)g_sb.total_blocks - used);
    printf("  总inode %d, 已用 %d, 空闲 %d\n", FS_MAX_INODES, used_ino, FS_MAX_INODES - used_ino);
    printf("  当前目录: %s\n", g_cwd_path);
}

static void cmd_bitmap(void) {
    printf("位图(每64块一行, 1=已用):\n");
    for (uint32_t b = 0; b < g_sb.total_blocks; b++) {
        if (b % 64 == 0) printf("%4u: ", b);
        printf("%c", bitmap_get(b) ? '1' : '.');
        if ((b + 1) % 64 == 0) printf("\n");
    }
    printf("\n");
}

/* ----- 自动化测试 (非交互) ----- */
int filesystem_selftest(void) {
    const char *img = "/tmp/_fs_selftest.img";
    int pass = 1;

    if (fs_format(img) != 0) { printf("FAIL: format\n"); return -1; }

    cmd_create("hello.txt");
    uint32_t ino;
    if (dir_lookup(g_cwd, "hello.txt", &ino) != 0) {
        printf("FAIL: create\n"); pass = 0;
    }

    cmd_write("hello.txt", "hello world");
    if (pass && g_inodes[ino].size != 11) {
        printf("FAIL: write size expected 11 got %u\n", g_inodes[ino].size); pass = 0;
    }

    cmd_mkdir("subdir");
    uint32_t dino;
    if (dir_lookup(g_cwd, "subdir", &dino) != 0) {
        printf("FAIL: mkdir\n"); pass = 0;
    }

    /* 持久化后重新挂载,验证数据仍存在 */
    if (g_disk) { fclose(g_disk); g_disk = NULL; }
    if (fs_mount(img) != 0) { printf("FAIL: remount\n"); pass = 0; }
    if (pass && dir_lookup(g_cwd, "hello.txt", &ino) != 0) {
        printf("FAIL: file missing after remount\n"); pass = 0;
    }
    if (pass && dir_lookup(g_cwd, "subdir", &dino) != 0) {
        printf("FAIL: dir missing after remount\n"); pass = 0;
    }

    cmd_rm("hello.txt");
    if (dir_lookup(g_cwd, "hello.txt", &ino) == 0) {
        printf("FAIL: file still exists after rm\n"); pass = 0;
    }

    cmd_rm("subdir");
    if (dir_lookup(g_cwd, "subdir", &dino) == 0) {
        printf("FAIL: dir still exists after rm\n"); pass = 0;
    }

    if (g_disk) { fclose(g_disk); g_disk = NULL; }
    remove(img);

    if (pass) printf("文件系统自检通过\n");
    return pass ? 0 : -1;
}

/* ----- 菜单 ----- */
void filesystem_menu(void) {
    char img[256] = DEFAULT_IMG;
    /* 自动尝试挂载 */
    struct stat st;
    if (stat(img, &st) == 0) {
        fs_mount(img);
    } else {
        printf("未发现镜像 %s,执行 format 创建\n", img);
    }

    for (;;) {
        print_title("简易文件系统");
        printf(" 当前路径: %s%s%s\n", COLOR_CYAN, g_cwd_path, COLOR_RESET);
        printf("  1) format    格式化镜像\n");
        printf("  2) mount     挂载镜像\n");
        printf("  3) ls        列出当前目录\n");
        printf("  4) mkdir     创建目录\n");
        printf("  5) create    创建文件\n");
        printf("  6) write     写入文件\n");
        printf("  7) cat       读取文件\n");
        printf("  8) rm        删除文件/空目录\n");
        printf("  9) cd        切换目录\n");
        printf(" 10) stat      镜像统计\n");
        printf(" 11) bitmap    显示位图\n");
        printf("  0) 返回主菜单\n");
        int op = read_int("选择: ");
        char name[128], content[4096];
        switch (op) {
        case 0:
            if (g_disk) { fclose(g_disk); g_disk = NULL; }
            return;
        case 1: fs_format(img); break;
        case 2: fs_mount(img); break;
        case 3:
            if (!g_disk) { printf(COLOR_RED "请先挂载\n" COLOR_RESET); break; }
            cmd_ls(); break;
        case 4:
            if (!g_disk) { printf(COLOR_RED "请先挂载\n" COLOR_RESET); break; }
            printf("目录名: "); if(!fgets(name,sizeof(name),stdin))break;
            name[strcspn(name,"\n")] = 0;
            if (name[0]) cmd_mkdir(name);
            break;
        case 5:
            if (!g_disk) { printf(COLOR_RED "请先挂载\n" COLOR_RESET); break; }
            printf("文件名: "); if(!fgets(name,sizeof(name),stdin))break;
            name[strcspn(name,"\n")] = 0;
            if (name[0]) cmd_create(name);
            break;
        case 6:
            if (!g_disk) { printf(COLOR_RED "请先挂载\n" COLOR_RESET); break; }
            printf("文件名: "); if(!fgets(name,sizeof(name),stdin))break;
            name[strcspn(name,"\n")] = 0;
            printf("内容(单行,以回车结束): ");
            if(!fgets(content,sizeof(content),stdin))break;
            content[strcspn(content,"\n")] = 0;
            if (name[0]) cmd_write(name, content);
            break;
        case 7:
            if (!g_disk) { printf(COLOR_RED "请先挂载\n" COLOR_RESET); break; }
            printf("文件名: "); if(!fgets(name,sizeof(name),stdin))break;
            name[strcspn(name,"\n")] = 0;
            if (name[0]) cmd_cat(name);
            break;
        case 8:
            if (!g_disk) { printf(COLOR_RED "请先挂载\n" COLOR_RESET); break; }
            printf("名称: "); if(!fgets(name,sizeof(name),stdin))break;
            name[strcspn(name,"\n")] = 0;
            if (name[0]) cmd_rm(name);
            break;
        case 9:
            if (!g_disk) { printf(COLOR_RED "请先挂载\n" COLOR_RESET); break; }
            printf("目录名(.. / 名称): "); if(!fgets(name,sizeof(name),stdin))break;
            name[strcspn(name,"\n")] = 0;
            if (name[0]) cmd_cd(name);
            break;
        case 10:
            if (!g_disk) { printf(COLOR_RED "请先挂载\n" COLOR_RESET); break; }
            cmd_stat(); break;
        case 11:
            if (!g_disk) { printf(COLOR_RED "请先挂载\n" COLOR_RESET); break; }
            cmd_bitmap(); break;
        default:
            printf(COLOR_RED "无效\n" COLOR_RESET);
        }
    }
}
