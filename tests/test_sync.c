/* 自动化测试: 同步 */
#include <stdio.h>
#include "../src/sync/sync.h"

int main(void) {
    printf(">>> 生产者-消费者 (2P,2C,buf=4,每P 3个)\n");
    run_producer_consumer(2, 2, 4, 3, 10);

    printf("\n>>> 读者-写者 (3R,2W,各2次,写者优先)\n");
    run_reader_writer(3, 2, 2, 10, 1);

    printf("\n>>> 哲学家就餐 (5人,2轮,资源分级)\n");
    run_dining_philosophers(5, 2, 30, 0);

    printf("\n同步测试运行完毕(目视检查)\n");
    return 0;
}
