#ifndef _PRODUCERCONSUMER_DRIVER_H_
#define _PRODUCERCONSUMER_DRIVER_H_

#define BUFFER_SIZE 10

/*
 * Data passed from a producer to a consumer.
 * producer_number == -1 is used as a "no more data" shutdown signal.
 */
struct pc_data {
        int item_number;
        int producer_number;
};

/* entry point registered in the kernel menu */
int producerconsumer(int nargs, char **args);

/*
 * Functions you implement in producerconsumer.c
 */
void producerconsumer_startup(void);
void producerconsumer_shutdown(void);
void producer_produce(struct pc_data data);
struct pc_data consumer_consume(void);

#endif /* _PRODUCERCONSUMER_DRIVER_H_ */
