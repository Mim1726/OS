#ifndef _BAR_DRIVER_H_
#define _BAR_DRIVER_H_

#include <synch.h>

/*
 * Number of distinct bottles behind the bar, numbered 1..NBOTTLES.
 * Index 0 is reserved to mean "no ingredient" / unused slot.
 */
#define NBOTTLES 10

#define BEER     1
#define WINE     2
#define VODKA    3
#define GIN      4
#define WHISKEY  5
#define RUM      6
#define TEQUILA  7
#define BRANDY   8
#define COLA     9
#define TONIC    10

/* Max number of ingredients that make up a single drink. */
#define DRINK_COMPLEXITY 3

/*
 * A customer's order and the resulting drink.
 *
 * requested[]: bottle numbers the customer asked for (0 = unused slot).
 * contents[]:  bottle numbers actually poured. A correct solution has
 *              contents[i] == requested[i] for all i once "ready" has
 *              been posted.
 * ready:       semaphore the bartender V()'s once contents[] is filled;
 *              the customer P()'s this to know their drink is done.
 */
struct glass {
        int requested[DRINK_COMPLEXITY];
        int contents[DRINK_COMPLEXITY];
        struct semaphore *ready;
};

/*
 * Per-bottle usage counters (in "doses" poured), indices 1..NBOTTLES.
 * Index 0 is unused. Updated by mix() inside bar.c.
 */
extern volatile unsigned long int bottles_used[NBOTTLES + 1];

/* entry point registered in the kernel menu */
int runbar(int nargs, char **args);

/*
 * Functions you implement in bar.c
 */

/*
 * Called once before any customer/bartender threads start.
 * num_customers/num_bartenders tell bar.c how many of each thread to
 * expect, so it knows when the last customer has left and can release
 * exactly that many bartenders from their serving loop.
 */
void bar_open(int num_customers, int num_bartenders);

/* Called once after all customer/bartender threads have finished. */
void bar_close(void);

/*
 * Called by a customer thread. Fill in glass->requested before calling.
 * Blocks until a bartender has mixed the drink (glass->contents filled
 * and glass->ready posted).
 */
void order(struct glass *glass);

/*
 * Called once by a customer thread after it has placed all of its
 * orders and is done drinking for the night.
 */
void customer_finished(void);

/*
 * Called by a bartender thread. Loops serving customer orders (mixing
 * drinks and waking the corresponding customer) until told the bar is
 * closing, at which point it returns so the thread can exit.
 */
void bartender_loop(unsigned long bartender_number);

#endif /* _BAR_DRIVER_H_ */
