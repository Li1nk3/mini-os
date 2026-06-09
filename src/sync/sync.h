#ifndef SYNC_H
#define SYNC_H

void sync_menu(void);

void run_producer_consumer(int producers, int consumers, int buf_size, int items_per_p, int delay_ms);
void run_reader_writer(int readers, int writers, int ops, int delay_ms, int writer_priority);
void run_dining_philosophers(int n, int rounds, int delay_ms, int strategy);

int  sync_selftest(void);  /* 非交互自动化测试,成功返回0 */

#endif
