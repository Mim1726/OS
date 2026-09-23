#include <types.h>
#include <lib.h>
#include <test.h>
#include <thread.h>
#include <synch.h>
#include <current.h>
#include "bar_driver.h"

#define NUM_CUSTOMER_THREADS   10
#define NUM_BARTENDER_THREADS  3
#define ORDERS_PER_CUSTOMER    10

volatile unsigned long int bottles_used[NBOTTLES + 1];

static struct semaphore *all_threads_done;

/*
 * Cheap deterministic-ish pseudo-random generator so we don't need a
 * real RNG here; reproducible across runs for a given seed elsewhere
 * doesn't matter for this driver, only that orders vary.
 */
static
unsigned long
next_rand(unsigned long *state)
{
        *state = (*state * 1103515245 + 12345) & 0x7fffffff;
        return *state;
}

static
void
fill_random_order(struct glass *glass, unsigned long *rand_state)
{
        int i;
        int complexity;

        complexity = 1 + (next_rand(rand_state) % DRINK_COMPLEXITY);

        for (i = 0; i < DRINK_COMPLEXITY; i++) {
                glass->requested[i] = 0;
                glass->contents[i] = 0;
        }

        for (i = 0; i < complexity; i++) {
                glass->requested[i] = 1 + (next_rand(rand_state) % NBOTTLES);
        }
}

static
void
customer_thread(void *unused1, unsigned long customer_number)
{
        int i, j;
        struct glass glass;
        unsigned long rand_state;

        (void)unused1;

        rand_state = customer_number * 7919 + 1;

        for (i = 0; i < ORDERS_PER_CUSTOMER; i++) {
                fill_random_order(&glass, &rand_state);

                glass.ready = sem_create("glass_ready", 0);
                if (glass.ready == NULL) {
                        panic("customer_thread: sem_create failed\n");
                }

                order(&glass);

                /* Sanity-check what we got back. */
                for (j = 0; j < DRINK_COMPLEXITY; j++) {
                        if (glass.contents[j] != glass.requested[j]) {
                                kprintf("*** Error! Customer %lu got wrong drink "
                                        "(slot %d: requested %d, got %d)\n",
                                        customer_number, j,
                                        glass.requested[j], glass.contents[j]);
                        }
                }

                sem_destroy(glass.ready);
        }

        customer_finished();

        V(all_threads_done);
        thread_exit();
}

static
void
bartender_thread(void *unused1, unsigned long bartender_number)
{
        (void)unused1;

        bartender_loop(bartender_number);

        V(all_threads_done);
        thread_exit();
}

int
runbar(int nargs, char **args)
{
        int i, error;
        int total_threads;

        (void)nargs;
        (void)args;

        for (i = 0; i <= NBOTTLES; i++) {
                bottles_used[i] = 0;
        }

        bar_open(NUM_CUSTOMER_THREADS, NUM_BARTENDER_THREADS);

        total_threads = NUM_CUSTOMER_THREADS + NUM_BARTENDER_THREADS;

        all_threads_done = sem_create("all_threads_done", 0);
        if (all_threads_done == NULL) {
                panic("runbar: sem_create failed\n");
        }

        for (i = 0; i < NUM_BARTENDER_THREADS; i++) {
                error = thread_fork("bartender thread", NULL,
                                     bartender_thread, NULL, i);
                if (error) {
                        panic("runbar: thread_fork failed: %s\n",
                              strerror(error));
                }
        }

        for (i = 0; i < NUM_CUSTOMER_THREADS; i++) {
                error = thread_fork("customer thread", NULL,
                                     customer_thread, NULL, i);
                if (error) {
                        panic("runbar: thread_fork failed: %s\n",
                              strerror(error));
                }
        }

        for (i = 0; i < total_threads; i++) {
                P(all_threads_done);
        }

        sem_destroy(all_threads_done);

        for (i = 1; i <= NBOTTLES; i++) {
                kprintf("Bottle %d used for %lu doses\n", i, bottles_used[i]);
        }

        bar_close();

        kprintf("The bar is closed, bye!!!\n");

        return 0;
}
