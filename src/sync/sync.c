#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include "sync.h"
#include "../common/util.h"

/* ====================== 生产者-消费者 ====================== */

#define MAX_BUF 128

typedef struct {
    int buffer[MAX_BUF];
    int buf_size;
    int in;
    int out;
    sem_t empty;       /* 空槽数 */
    sem_t full;        /* 满槽数 */
    pthread_mutex_t mu;
    int items_per_producer;
    int total_to_consume;  /* 消费者需要消费的总数 */
    int produced;
    int reserved;          /* 已被某个消费者认领待消费的额度,用于退出判定 */
    int consumed;          /* 实际完成的消费次数 */
    pthread_mutex_t stat_mu;
    int delay_ms;
} PCContext;

static void msleep(int ms) {
    if (ms <= 0) return;
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

typedef struct {
    PCContext *ctx;
    int id;
} ThreadArg;

static void *producer_fn(void *arg) {
    ThreadArg *t = (ThreadArg *)arg;
    PCContext *c = t->ctx;
    for (int i = 0; i < c->items_per_producer; i++) {
        int item = t->id * 1000 + i;
        sem_wait(&c->empty);
        pthread_mutex_lock(&c->mu);
        c->buffer[c->in] = item;
        printf(COLOR_GREEN "  [生产者%d] 放入 %d 到槽 %d\n" COLOR_RESET, t->id, item, c->in);
        c->in = (c->in + 1) % c->buf_size;
        pthread_mutex_unlock(&c->mu);
        sem_post(&c->full);

        pthread_mutex_lock(&c->stat_mu);
        c->produced++;
        pthread_mutex_unlock(&c->stat_mu);

        msleep(c->delay_ms);
    }
    return NULL;
}

static void *consumer_fn(void *arg) {
    ThreadArg *t = (ThreadArg *)arg;
    PCContext *c = t->ctx;
    for (;;) {
        pthread_mutex_lock(&c->stat_mu);
        if (c->reserved >= c->total_to_consume) {
            pthread_mutex_unlock(&c->stat_mu);
            break;
        }
        c->reserved++;
        pthread_mutex_unlock(&c->stat_mu);

        sem_wait(&c->full);
        pthread_mutex_lock(&c->mu);
        int item = c->buffer[c->out];
        printf(COLOR_BLUE "  [消费者%d] 取出 %d 从槽 %d\n" COLOR_RESET, t->id, item, c->out);
        c->out = (c->out + 1) % c->buf_size;
        pthread_mutex_unlock(&c->mu);
        sem_post(&c->empty);

        pthread_mutex_lock(&c->stat_mu);
        c->consumed++;
        pthread_mutex_unlock(&c->stat_mu);

        msleep(c->delay_ms);
    }
    return NULL;
}

void run_producer_consumer(int producers, int consumers, int buf_size, int items_per_p, int delay_ms) {
    if (buf_size < 1 || buf_size > MAX_BUF) buf_size = 8;
    PCContext c;
    memset(&c, 0, sizeof(c));
    c.buf_size = buf_size;
    c.in = c.out = 0;
    c.items_per_producer = items_per_p;
    c.total_to_consume = producers * items_per_p;
    c.delay_ms = delay_ms;
    sem_init(&c.empty, 0, buf_size);
    sem_init(&c.full, 0, 0);
    pthread_mutex_init(&c.mu, NULL);
    pthread_mutex_init(&c.stat_mu, NULL);

    pthread_t ptid[64], ctid[64];
    ThreadArg pargs[64], cargs[64];

    for (int i = 0; i < producers; i++) {
        pargs[i].ctx = &c;
        pargs[i].id = i + 1;
        pthread_create(&ptid[i], NULL, producer_fn, &pargs[i]);
    }
    for (int i = 0; i < consumers; i++) {
        cargs[i].ctx = &c;
        cargs[i].id = i + 1;
        pthread_create(&ctid[i], NULL, consumer_fn, &cargs[i]);
    }

    for (int i = 0; i < producers; i++) pthread_join(ptid[i], NULL);
    for (int i = 0; i < consumers; i++) pthread_join(ctid[i], NULL);

    sem_destroy(&c.empty);
    sem_destroy(&c.full);
    pthread_mutex_destroy(&c.mu);
    pthread_mutex_destroy(&c.stat_mu);

    printf("\n生产 %d 项,消费 %d 项,完成\n", c.produced, c.consumed);
}

/* ====================== 读者-写者 ====================== */
/* 写者优先版本 + 读者优先版本 */

typedef struct {
    pthread_mutex_t rc_mu;     /* 保护 readers 计数 */
    pthread_mutex_t wc_mu;     /* 保护 writers 计数(写优先用) */
    pthread_mutex_t rw_mu;     /* 临界资源 */
    pthread_mutex_t wq_mu;     /* 写者优先时排在前 */
    pthread_mutex_t r_try;     /* 写者优先时阻止新读 */
    int readers;
    int writers_waiting;
    int ops;
    int delay_ms;
    int priority;              /* 0=读者优先, 1=写者优先 */
    int data;                  /* 共享数据 */
} RWContext;

typedef struct {
    RWContext *ctx;
    int id;
    int is_writer;
} RWArg;

static void *rw_thread(void *arg) {
    RWArg *a = (RWArg *)arg;
    RWContext *c = a->ctx;
    for (int i = 0; i < c->ops; i++) {
        msleep(c->delay_ms);
        if (a->is_writer) {
            if (c->priority == 1) {
                pthread_mutex_lock(&c->wc_mu);
                c->writers_waiting++;
                if (c->writers_waiting == 1) pthread_mutex_lock(&c->r_try);
                pthread_mutex_unlock(&c->wc_mu);
            }
            pthread_mutex_lock(&c->rw_mu);
            c->data++;
            printf(COLOR_YELLOW "  [写者%d] 写: data=%d\n" COLOR_RESET, a->id, c->data);
            msleep(c->delay_ms);
            pthread_mutex_unlock(&c->rw_mu);
            if (c->priority == 1) {
                pthread_mutex_lock(&c->wc_mu);
                c->writers_waiting--;
                if (c->writers_waiting == 0) pthread_mutex_unlock(&c->r_try);
                pthread_mutex_unlock(&c->wc_mu);
            }
        } else {
            if (c->priority == 1) {
                pthread_mutex_lock(&c->r_try);
                pthread_mutex_unlock(&c->r_try);
            }
            pthread_mutex_lock(&c->rc_mu);
            c->readers++;
            if (c->readers == 1) pthread_mutex_lock(&c->rw_mu);
            pthread_mutex_unlock(&c->rc_mu);

            printf(COLOR_GREEN "  [读者%d] 读: data=%d (并发读者=%d)\n" COLOR_RESET,
                   a->id, c->data, c->readers);
            msleep(c->delay_ms);

            pthread_mutex_lock(&c->rc_mu);
            c->readers--;
            if (c->readers == 0) pthread_mutex_unlock(&c->rw_mu);
            pthread_mutex_unlock(&c->rc_mu);
        }
    }
    return NULL;
}

void run_reader_writer(int readers, int writers, int ops, int delay_ms, int writer_priority) {
    RWContext c;
    memset(&c, 0, sizeof(c));
    pthread_mutex_init(&c.rc_mu, NULL);
    pthread_mutex_init(&c.wc_mu, NULL);
    pthread_mutex_init(&c.rw_mu, NULL);
    pthread_mutex_init(&c.r_try, NULL);
    c.readers = 0;
    c.writers_waiting = 0;
    c.ops = ops;
    c.delay_ms = delay_ms;
    c.priority = writer_priority;
    c.data = 0;

    pthread_t rtid[64], wtid[64];
    RWArg rargs[64], wargs[64];
    for (int i = 0; i < readers; i++) {
        rargs[i].ctx = &c; rargs[i].id = i + 1; rargs[i].is_writer = 0;
        pthread_create(&rtid[i], NULL, rw_thread, &rargs[i]);
    }
    for (int i = 0; i < writers; i++) {
        wargs[i].ctx = &c; wargs[i].id = i + 1; wargs[i].is_writer = 1;
        pthread_create(&wtid[i], NULL, rw_thread, &wargs[i]);
    }
    for (int i = 0; i < readers; i++) pthread_join(rtid[i], NULL);
    for (int i = 0; i < writers; i++) pthread_join(wtid[i], NULL);
    pthread_mutex_destroy(&c.rc_mu);
    pthread_mutex_destroy(&c.wc_mu);
    pthread_mutex_destroy(&c.rw_mu);
    pthread_mutex_destroy(&c.r_try);
    printf("\n最终 data=%d\n", c.data);
}

/* ====================== 哲学家就餐 ====================== */
/* 策略: 0=资源分级(奇偶顺序拿筷),避免循环等待; 1=信号量限制最多 n-1 人尝试 */

#define MAX_PHIL 16

typedef struct {
    int n;
    int rounds;
    int delay_ms;
    int strategy;
    pthread_mutex_t fork[MAX_PHIL];
    sem_t room;        /* strategy=1 时用,允许 n-1 人就餐 */
    int ate[MAX_PHIL]; /* 每位哲学家吃饭次数 */
    pthread_mutex_t stat_mu;
} DPContext;

typedef struct {
    DPContext *ctx;
    int id;
} DPArg;

static void think(int id, int ms) {
    printf("  哲学家%d 思考...\n", id);
    msleep(ms);
}
static void eat(int id, int ms) {
    printf(COLOR_GREEN "  哲学家%d 进餐...\n" COLOR_RESET, id);
    msleep(ms);
}

static void *philosopher(void *arg) {
    DPArg *a = (DPArg *)arg;
    DPContext *c = a->ctx;
    int id = a->id;
    int left = id;
    int right = (id + 1) % c->n;
    for (int r = 0; r < c->rounds; r++) {
        think(id, c->delay_ms);
        if (c->strategy == 0) {
            /* 资源分级: 总是先拿编号小的 */
            int first = left < right ? left : right;
            int second = left < right ? right : left;
            pthread_mutex_lock(&c->fork[first]);
            printf("  哲学家%d 拿起筷子%d\n", id, first);
            pthread_mutex_lock(&c->fork[second]);
            printf("  哲学家%d 拿起筷子%d\n", id, second);
            eat(id, c->delay_ms);
            pthread_mutex_unlock(&c->fork[second]);
            pthread_mutex_unlock(&c->fork[first]);
        } else {
            sem_wait(&c->room);
            pthread_mutex_lock(&c->fork[left]);
            printf("  哲学家%d 拿起筷子%d\n", id, left);
            pthread_mutex_lock(&c->fork[right]);
            printf("  哲学家%d 拿起筷子%d\n", id, right);
            eat(id, c->delay_ms);
            pthread_mutex_unlock(&c->fork[right]);
            pthread_mutex_unlock(&c->fork[left]);
            sem_post(&c->room);
        }
        pthread_mutex_lock(&c->stat_mu);
        c->ate[id]++;
        pthread_mutex_unlock(&c->stat_mu);
    }
    return NULL;
}

void run_dining_philosophers(int n, int rounds, int delay_ms, int strategy) {
    if (n < 2 || n > MAX_PHIL) n = 5;
    DPContext c;
    memset(&c, 0, sizeof(c));
    c.n = n; c.rounds = rounds; c.delay_ms = delay_ms; c.strategy = strategy;
    for (int i = 0; i < n; i++) pthread_mutex_init(&c.fork[i], NULL);
    pthread_mutex_init(&c.stat_mu, NULL);
    if (strategy == 1) sem_init(&c.room, 0, n - 1);

    pthread_t tid[MAX_PHIL];
    DPArg args[MAX_PHIL];
    for (int i = 0; i < n; i++) {
        args[i].ctx = &c; args[i].id = i;
        pthread_create(&tid[i], NULL, philosopher, &args[i]);
    }
    for (int i = 0; i < n; i++) pthread_join(tid[i], NULL);
    for (int i = 0; i < n; i++) pthread_mutex_destroy(&c.fork[i]);
    pthread_mutex_destroy(&c.stat_mu);
    if (strategy == 1) sem_destroy(&c.room);

    printf("\n进餐统计:\n");
    for (int i = 0; i < n; i++)
        printf("  哲学家%d 进餐 %d 次\n", i, c.ate[i]);
}

/* ====================== 菜单 ====================== */
void sync_menu(void) {
    for (;;) {
        print_title("进程同步与并发控制");
        printf("  1) 生产者-消费者\n");
        printf("  2) 读者-写者\n");
        printf("  3) 哲学家就餐\n");
        printf("  0) 返回主菜单\n");
        int op = read_int("选择: ");
        if (op == 0) return;
        if (op == 1) {
            int p = read_int_default("生产者数", 2);
            int cc = read_int_default("消费者数", 2);
            int b = read_int_default("缓冲区大小", 5);
            int it = read_int_default("每个生产者生产数", 5);
            int d = read_int_default("操作延迟(ms)", 100);
            run_producer_consumer(p, cc, b, it, d);
        } else if (op == 2) {
            int r = read_int_default("读者数", 3);
            int w = read_int_default("写者数", 2);
            int ops = read_int_default("每个线程操作次数", 3);
            int d = read_int_default("操作延迟(ms)", 100);
            int prio = read_int_default("0=读者优先 1=写者优先", 1);
            run_reader_writer(r, w, ops, d, prio);
        } else if (op == 3) {
            int n = read_int_default("哲学家数量", 5);
            int rounds = read_int_default("每人进餐轮次", 3);
            int d = read_int_default("操作延迟(ms)", 200);
            int s = read_int_default("0=资源分级 1=房间信号量", 0);
            run_dining_philosophers(n, rounds, d, s);
        } else {
            printf(COLOR_RED "无效\n" COLOR_RESET);
            continue;
        }
        pause_screen();
    }
}
