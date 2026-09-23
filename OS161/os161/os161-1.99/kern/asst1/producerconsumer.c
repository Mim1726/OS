#include <types.h>
#include <lib.h>
#include <synch.h>
#include "producerconsumer_driver.h"

/*
 * Fixed-size circular buffer, FIFO order.
 *
 * Shared state: buffer[], buf_head, buf_tail, buf_count.
 * All access to this state is protected by buf_lock. The buffer being
 * full/empty are the two conditions producers/consumers wait on,
 * represented by the two condition variables below.
 */
static struct pc_data buffer[BUFFER_SIZE];
static int buf_head;   /* index of the next item to remove */
static int buf_tail;   /* index of the next free slot to fill */
static int buf_count;  /* number of items currently held */

static struct lock *buf_lock;
static struct cv *buf_not_full;
static struct cv *buf_not_empty;

void
producerconsumer_startup(void)
{
        buf_head = 0;
        buf_tail = 0;
        buf_count = 0;

        buf_lock = lock_create("pc_buf_lock");
        if (buf_lock == NULL) {
                panic("producerconsumer_startup: lock_create failed\n");
        }

        buf_not_full = cv_create("pc_buf_not_full");
        if (buf_not_full == NULL) {
                panic("producerconsumer_startup: cv_create failed\n");
        }

        buf_not_empty = cv_create("pc_buf_not_empty");
        if (buf_not_empty == NULL) {
                panic("producerconsumer_startup: cv_create failed\n");
        }
}

void
producerconsumer_shutdown(void)
{
        lock_destroy(buf_lock);
        cv_destroy(buf_not_full);
        cv_destroy(buf_not_empty);
}

void
producer_produce(struct pc_data data)
{
        lock_acquire(buf_lock);

        while (buf_count == BUFFER_SIZE) {
                cv_wait(buf_not_full, buf_lock);
        }

        buffer[buf_tail] = data;
        buf_tail = (buf_tail + 1) % BUFFER_SIZE;
        buf_count++;

        cv_signal(buf_not_empty, buf_lock);

        lock_release(buf_lock);
}

struct pc_data
consumer_consume(void)
{
        struct pc_data data;

        lock_acquire(buf_lock);

        while (buf_count == 0) {
                cv_wait(buf_not_empty, buf_lock);
        }

        data = buffer[buf_head];
        buf_head = (buf_head + 1) % BUFFER_SIZE;
        buf_count--;

        cv_signal(buf_not_full, buf_lock);

        lock_release(buf_lock);

        return data;
}
