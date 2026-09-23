#include <types.h>
#include <lib.h>
#include <test.h>
#include <thread.h>
#include <synch.h>
#include <current.h>
#include "producerconsumer_driver.h"

#define NUM_PRODUCER_THREADS   3
#define NUM_CONSUMER_THREADS   5
#define NUM_ITEMS_PER_PRODUCER 10

static struct semaphore *all_threads_done;
static struct semaphore *producers_done_sem;
static volatile int producers_remaining;
static struct lock *producers_remaining_lock;

static
void
producer_thread(void *unused1, unsigned long producer_number)
{
        int i;
        struct pc_data data;

        (void)unused1;

        kprintf("Producer started\n");

        for (i = 0; i < NUM_ITEMS_PER_PRODUCER; i++) {
                data.item_number = i;
                data.producer_number = (int)producer_number;
                producer_produce(data);
        }

        kprintf("Producer finished\n");

        lock_acquire(producers_remaining_lock);
        producers_remaining--;
        if (producers_remaining == 0) {
                V(producers_done_sem);
        }
        lock_release(producers_remaining_lock);

        V(all_threads_done);
        thread_exit();
}

static
void
consumer_thread(void *unused1, unsigned long consumer_number)
{
        struct pc_data data;
        int done = 0;

        (void)unused1;
        (void)consumer_number;

        kprintf("Consumer started\n");

        while (!done) {
                data = consumer_consume();
                if (data.producer_number == -1) {
                        /* shutdown signal - no more data coming */
                        done = 1;
                }
                /* otherwise: this is where a real consumer would process
                 * data.item_number / data.producer_number. Nothing to do
                 * in this simulation. */
        }

        kprintf("Consumer finished normally\n");

        V(all_threads_done);
        thread_exit();
}

/*
 * Waits until all producers have finished producing real data, then
 * feeds one shutdown signal per consumer thread so each consumer can
 * exit cleanly.
 */
static
void
feed_shutdown_signals(void *unused1, unsigned long unused2)
{
        struct pc_data shutdown_signal;
        int i;

        (void)unused1;
        (void)unused2;

        P(producers_done_sem);

        kprintf("All producer threads have exited.\n");

        shutdown_signal.item_number = 0;
        shutdown_signal.producer_number = -1;

        for (i = 0; i < NUM_CONSUMER_THREADS; i++) {
                producer_produce(shutdown_signal);
        }

        thread_exit();
}

int
producerconsumer(int nargs, char **args)
{
        int i, error;
        int total_threads;

        (void)nargs;
        (void)args;

        kprintf("run_producerconsumer: starting up\n");

        producerconsumer_startup();

        total_threads = NUM_PRODUCER_THREADS + NUM_CONSUMER_THREADS;

        all_threads_done = sem_create("all_threads_done", 0);
        if (all_threads_done == NULL) {
                panic("producerconsumer: sem_create failed\n");
        }

        producers_done_sem = sem_create("producers_done_sem", 0);
        if (producers_done_sem == NULL) {
                panic("producerconsumer: sem_create failed\n");
        }

        producers_remaining_lock = lock_create("producers_remaining_lock");
        if (producers_remaining_lock == NULL) {
                panic("producerconsumer: lock_create failed\n");
        }
        producers_remaining = NUM_PRODUCER_THREADS;

        for (i = 0; i < NUM_CONSUMER_THREADS; i++) {
                error = thread_fork("consumer thread", NULL, consumer_thread, NULL, i);
                if (error) {
                        panic("producerconsumer: thread_fork failed: %s\n", strerror(error));
                }
        }

        kprintf("Waiting for producer threads to exit...\n");

        for (i = 0; i < NUM_PRODUCER_THREADS; i++) {
                error = thread_fork("producer thread", NULL, producer_thread, NULL, i);
                if (error) {
                        panic("producerconsumer: thread_fork failed: %s\n", strerror(error));
                }
        }

        error = thread_fork("shutdown feeder", NULL, feed_shutdown_signals, NULL, 0);
        if (error) {
                panic("producerconsumer: thread_fork failed: %s\n", strerror(error));
        }

        for (i = 0; i < total_threads; i++) {
                P(all_threads_done);
        }

        sem_destroy(all_threads_done);
        sem_destroy(producers_done_sem);
        lock_destroy(producers_remaining_lock);

        producerconsumer_shutdown();

        return 0;
}
