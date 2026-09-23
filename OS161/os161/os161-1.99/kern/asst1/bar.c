#include <types.h>
#include <lib.h>
#include <synch.h>
#include "bar_driver.h"

/*
 * ------------------------------------------------------------------
 * Order queue: customers enqueue a glass*, bartenders dequeue one.
 * Same bounded circular buffer + lock + 2 CVs pattern as the
 * producer/consumer problem, just carrying pointers instead of
 * pc_data by value. A NULL entry is used as a "go home" signal fed
 * to a single bartender.
 * ------------------------------------------------------------------
 */
#define ORDER_QUEUE_SIZE 32

static struct glass *order_queue[ORDER_QUEUE_SIZE];
static int oq_head;
static int oq_tail;
static int oq_count;

static struct lock *oq_lock;
static struct cv *oq_not_full;
static struct cv *oq_not_empty;

/*
 * ------------------------------------------------------------------
 * Per-bottle locks, so two bartenders mixing drinks that need
 * disjoint sets of bottles can proceed in parallel. To avoid
 * deadlock, mix() always acquires the bottles a drink needs in
 * ascending bottle-number order (never "acquire whatever's next in
 * the order the customer listed them").
 * ------------------------------------------------------------------
 */
static struct lock *bottle_lock[NBOTTLES + 1]; /* index 0 unused */

/*
 * ------------------------------------------------------------------
 * Customer-count tracking, so we know when to send bartenders home.
 * ------------------------------------------------------------------
 */
static struct lock *customers_lock;
static int customers_remaining;
static int num_bartenders;

static
void
enqueue_order(struct glass *glass)
{
        lock_acquire(oq_lock);

        while (oq_count == ORDER_QUEUE_SIZE) {
                cv_wait(oq_not_full, oq_lock);
        }

        order_queue[oq_tail] = glass;
        oq_tail = (oq_tail + 1) % ORDER_QUEUE_SIZE;
        oq_count++;

        cv_signal(oq_not_empty, oq_lock);

        lock_release(oq_lock);
}

static
struct glass *
dequeue_order(void)
{
        struct glass *glass;

        lock_acquire(oq_lock);

        while (oq_count == 0) {
                cv_wait(oq_not_empty, oq_lock);
        }

        glass = order_queue[oq_head];
        oq_head = (oq_head + 1) % ORDER_QUEUE_SIZE;
        oq_count--;

        cv_signal(oq_not_full, oq_lock);

        lock_release(oq_lock);

        return glass;
}

void
bar_open(int num_customers, int num_bartenders_in)
{
        int i;
        char name[16];

        oq_head = 0;
        oq_tail = 0;
        oq_count = 0;

        oq_lock = lock_create("oq_lock");
        oq_not_full = cv_create("oq_not_full");
        oq_not_empty = cv_create("oq_not_empty");
        if (oq_lock == NULL || oq_not_full == NULL || oq_not_empty == NULL) {
                panic("bar_open: order queue sync create failed\n");
        }

        for (i = 1; i <= NBOTTLES; i++) {
                snprintf(name, sizeof(name), "bottle_lock_%d", i);
                bottle_lock[i] = lock_create(name);
                if (bottle_lock[i] == NULL) {
                        panic("bar_open: bottle_lock create failed\n");
                }
        }

        customers_lock = lock_create("customers_lock");
        if (customers_lock == NULL) {
                panic("bar_open: customers_lock create failed\n");
        }
        customers_remaining = num_customers;
        num_bartenders = num_bartenders_in;
}

void
bar_close(void)
{
        int i;

        lock_destroy(oq_lock);
        cv_destroy(oq_not_full);
        cv_destroy(oq_not_empty);

        for (i = 1; i <= NBOTTLES; i++) {
                lock_destroy(bottle_lock[i]);
        }

        lock_destroy(customers_lock);
}

void
order(struct glass *glass)
{
        enqueue_order(glass);
        P(glass->ready);
}

void
customer_finished(void)
{
        int i;
        int send_shutdown;

        send_shutdown = 0;

        lock_acquire(customers_lock);
        customers_remaining--;
        if (customers_remaining == 0) {
                send_shutdown = 1;
        }
        lock_release(customers_lock);

        if (send_shutdown) {
                /* Every customer is gone: send one "go home" (NULL)
                 * signal per bartender so each one exits its loop. */
                for (i = 0; i < num_bartenders; i++) {
                        enqueue_order(NULL);
                }
        }
}

/*
 * Pours glass->requested[] into glass->contents[], locking each
 * distinct bottle needed (in ascending order, to avoid deadlock
 * against other concurrent mix() calls) only for as long as it takes
 * to "pour" and update that bottle's usage counter.
 */
static
void
mix(struct glass *glass)
{
        int i, j;
        int needed[DRINK_COMPLEXITY];
        int nneeded;
        int already;

        nneeded = 0;
        for (i = 0; i < DRINK_COMPLEXITY; i++) {
                if (glass->requested[i] == 0) {
                        continue;
                }
                already = 0;
                for (j = 0; j < nneeded; j++) {
                        if (needed[j] == glass->requested[i]) {
                                already = 1;
                                break;
                        }
                }
                if (!already) {
                        needed[nneeded++] = glass->requested[i];
                }
        }

        /* simple insertion sort, ascending, to fix lock order */
        for (i = 1; i < nneeded; i++) {
                int key = needed[i];
                j = i - 1;
                while (j >= 0 && needed[j] > key) {
                        needed[j + 1] = needed[j];
                        j--;
                }
                needed[j + 1] = key;
        }

        for (i = 0; i < nneeded; i++) {
                lock_acquire(bottle_lock[needed[i]]);
        }

        for (i = 0; i < DRINK_COMPLEXITY; i++) {
                glass->contents[i] = glass->requested[i];
                if (glass->requested[i] != 0) {
                        bottles_used[glass->requested[i]]++;
                }
        }

        for (i = nneeded - 1; i >= 0; i--) {
                lock_release(bottle_lock[needed[i]]);
        }
}

void
bartender_loop(unsigned long bartender_number)
{
        struct glass *glass;
        int done = 0;

        (void)bartender_number;

        while (!done) {
                glass = dequeue_order();
                if (glass == NULL) {
                        done = 1;
                } else {
                        mix(glass);
                        V(glass->ready);
                }
        }
}
