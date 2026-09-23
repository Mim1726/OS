#include <types.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vnode.h>
#include <vfs.h>
#include <synch.h>
#include <kern/fcntl.h>

#include "opt-A2.h"

#if OPT_A2
#define PROC_TABLE_SIZE 128
#define PROC_PID_MIN 2

static struct proc *proctable[PROC_TABLE_SIZE];
static struct lock *proctable_lock;

pid_t
proc_assign_pid(struct proc *proc)
{
        pid_t pid;

        lock_acquire(proctable_lock);
        for (pid = PROC_PID_MIN; pid < PROC_TABLE_SIZE; pid++) {
                if (proctable[pid] == NULL) {
                        proctable[pid] = proc;
                        lock_release(proctable_lock);
                        return pid;
                }
        }
        lock_release(proctable_lock);
        return -1;
}

struct proc *
proc_search_pid(pid_t pid)
{
        struct proc *proc;

        if (pid < PROC_PID_MIN || pid >= PROC_TABLE_SIZE) {
                return NULL;
        }
        lock_acquire(proctable_lock);
        proc = proctable[pid];
        lock_release(proctable_lock);
        return proc;
}
#endif /* OPT_A2 */

struct proc *kproc;

#ifdef UW
static volatile unsigned int proc_count;
static struct semaphore *proc_count_mutex;
struct semaphore *no_proc_sem;
#endif  // UW

static
struct proc *
proc_create(const char *name)
{
        struct proc *proc;

        proc = kmalloc(sizeof(*proc));
        if (proc == NULL) {
                return NULL;
        }
        proc->p_name = kstrdup(name);
        if (proc->p_name == NULL) {
                kfree(proc);
                return NULL;
        }

        threadarray_init(&proc->p_threads);
        spinlock_init(&proc->p_lock);

        proc->p_addrspace = NULL;

        proc->p_cwd = NULL;

#ifdef UW
        proc->console = NULL;
#endif // UW

        return proc;
}

void
proc_destroy(struct proc *proc)
{
        KASSERT(proc != NULL);
        KASSERT(proc != kproc);

        if (proc->p_cwd) {
                VOP_DECREF(proc->p_cwd);
                proc->p_cwd = NULL;
        }

#ifndef UW
        if (proc->p_addrspace) {
                struct addrspace *as;

                as_deactivate();
                as = curproc_setas(NULL);
                as_destroy(as);
        }
#endif // UW

#ifdef UW
        if (proc->console) {
          vfs_close(proc->console);
        }
#endif // UW

#if OPT_A2
        /* Under OPT_A2, don't free the proc struct  it mayhere 
           still be waited on. Reclamation happens in proc_reap(),
           called from sys_waitpid(). */
#else
        threadarray_cleanup(&proc->p_threads);
        spinlock_cleanup(&proc->p_lock);

        kfree(proc->p_name);
        kfree(proc);
#endif

#ifdef UW
        P(proc_count_mutex);
        KASSERT(proc_count > 0);
        proc_count--;
        if (proc_count == 0) {
          V(no_proc_sem);
        }
        V(proc_count_mutex);
#endif // UW

}

#if OPT_A2
void
proc_reap(struct proc *proc)
{
        KASSERT(proc != NULL);
        KASSERT(proc->p_exited);

        lock_acquire(proctable_lock);
        proctable[proc->p_pid] = NULL;
        lock_release(proctable_lock);

        lock_destroy(proc->p_waitlock);
        cv_destroy(proc->p_waitcv);
        threadarray_cleanup(&proc->p_threads);
        spinlock_cleanup(&proc->p_lock);
        kfree(proc->p_name);
        kfree(proc);
}

void
proc_destroy_incomplete(struct proc *proc)
{
        KASSERT(proc != NULL);

        lock_acquire(proctable_lock);
        proctable[proc->p_pid] = NULL;
        lock_release(proctable_lock);

        if (proc->p_addrspace != NULL) {
                as_destroy(proc->p_addrspace);
        }
        if (proc->p_cwd != NULL) {
                VOP_DECREF(proc->p_cwd);
        }
        if (proc->p_waitlock != NULL) {
                lock_destroy(proc->p_waitlock);
        }
        if (proc->p_waitcv != NULL) {
                cv_destroy(proc->p_waitcv);
        }
        threadarray_cleanup(&proc->p_threads);
        spinlock_cleanup(&proc->p_lock);
        kfree(proc->p_name);
        kfree(proc);
}
#endif

void
proc_bootstrap(void)
{
  kproc = proc_create("[kernel]");
  if (kproc == NULL) {
    panic("proc_create for kproc failed\n");
  }
#ifdef UW
  proc_count = 0;
  proc_count_mutex = sem_create("proc_count_mutex",1);
  if (proc_count_mutex == NULL) {
    panic("could not create proc_count_mutex semaphore\n");
  }
  no_proc_sem = sem_create("no_proc_sem",0);
  if (no_proc_sem == NULL) {
    panic("could not create no_proc_sem semaphore\n");
  }
#endif // UW

#if OPT_A2
  proctable_lock = lock_create("proctable_lock");
  if (proctable_lock == NULL) {
    panic("could not create proctable_lock\n");
  }
#endif
}

struct proc *
proc_create_runprogram(const char *name)
{
        struct proc *proc;
        char *console_path;

        proc = proc_create(name);
        if (proc == NULL) {
                return NULL;
        }

#ifdef UW
        console_path = kstrdup("con:");
        if (console_path == NULL) {
          panic("unable to copy console path name during process creation\n");
        }
        if (vfs_open(console_path,O_WRONLY,0,&(proc->console))) {
          panic("unable to open the console during process creation\n");
        }
        kfree(console_path);
#endif // UW

        proc->p_addrspace = NULL;

#ifdef UW
        if (curproc->p_cwd != NULL) {
                VOP_INCREF(curproc->p_cwd);
                proc->p_cwd = curproc->p_cwd;
        }
#else // UW
        spinlock_acquire(&curproc->p_lock);
        if (curproc->p_cwd != NULL) {
                VOP_INCREF(curproc->p_cwd);
                proc->p_cwd = curproc->p_cwd;
        }
        spinlock_release(&curproc->p_lock);
#endif // UW

#ifdef UW
        P(proc_count_mutex);
        proc_count++;
        V(proc_count_mutex);
#endif // UW

#if OPT_A2
        proc->p_pid = proc_assign_pid(proc);
        proc->p_parent_pid = 0;
        proc->p_exitcode = 0;
        proc->p_exited = false;
        proc->p_waitlock = lock_create("p_waitlock");
        proc->p_waitcv = cv_create("p_waitcv");
        if (proc->p_waitlock == NULL || proc->p_waitcv == NULL) {
                panic("could not create wait lock/cv for process\n");
        }
#endif

        return proc;
}

int
proc_addthread(struct proc *proc, struct thread *t)
{
        int result;

        KASSERT(t->t_proc == NULL);

        spinlock_acquire(&proc->p_lock);
        result = threadarray_add(&proc->p_threads, t, NULL);
        spinlock_release(&proc->p_lock);
        if (result) {
                return result;
        }
        t->t_proc = proc;
        return 0;
}

void
proc_remthread(struct thread *t)
{
        struct proc *proc;
        unsigned i, num;

        proc = t->t_proc;
        KASSERT(proc != NULL);

        spinlock_acquire(&proc->p_lock);
        num = threadarray_num(&proc->p_threads);
        for (i=0; i<num; i++) {
                if (threadarray_get(&proc->p_threads, i) == t) {
                        threadarray_remove(&proc->p_threads, i);
                        spinlock_release(&proc->p_lock);
                        t->t_proc = NULL;
                        return;
                }
        }
        spinlock_release(&proc->p_lock);
        panic("Thread (%p) has escaped from its process (%p)\n", t, proc);
}

struct addrspace *
curproc_getas(void)
{
        struct addrspace *as;
#ifdef UW
        if (curproc == NULL) {
                return NULL;
        }
#endif

        spinlock_acquire(&curproc->p_lock);
        as = curproc->p_addrspace;
        spinlock_release(&curproc->p_lock);
        return as;
}

struct addrspace *
curproc_setas(struct addrspace *newas)
{
        struct addrspace *oldas;
        struct proc *proc = curproc;

        spinlock_acquire(&proc->p_lock);
        oldas = proc->p_addrspace;
        proc->p_addrspace = newas;
        spinlock_release(&proc->p_lock);
        return oldas;
}
