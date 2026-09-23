#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <kern/fcntl.h>
#include <limits.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <synch.h>
#include <vfs.h>
#include <mips/trapframe.h>

#include "opt-A2.h"

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  as = curproc_setas(NULL);
  as_destroy(as);

  proc_remthread(curthread);

#if OPT_A2
  lock_acquire(p->p_waitlock);
  p->p_exitcode = _MKWAIT_EXIT(exitcode);
  p->p_exited = true;
  cv_broadcast(p->p_waitcv, p->p_waitlock);
  lock_release(p->p_waitlock);
#endif

  proc_destroy(p);

  thread_exit();
  panic("return from thread_exit in sys_exit\n");
}

int
sys_getpid(pid_t *retval)
{
#if OPT_A2
  *retval = curproc->p_pid;
  return(0);
#else
  *retval = 1;
  return(0);
#endif
}

int
sys_waitpid(pid_t pid,
            userptr_t status,
            int options,
            pid_t *retval)
{
  int exitstatus;
  int result;

  if (options != 0) {
    return(EINVAL);
  }

#if OPT_A2
  struct proc *child;

  child = proc_search_pid(pid);
  if (child == NULL) {
    return ESRCH;
  }
  if (child->p_parent_pid != curproc->p_pid) {
    return ECHILD;
  }

  lock_acquire(child->p_waitlock);
  while (!child->p_exited) {
    cv_wait(child->p_waitcv, child->p_waitlock);
  }
  exitstatus = child->p_exitcode;
  lock_release(child->p_waitlock);

  proc_reap(child);

  result = copyout((void *)&exitstatus, status, sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
#else
  exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
#endif
}

#if OPT_A2
int
sys_fork(struct trapframe *tf, pid_t *retval)
{
        struct proc *child;
        struct addrspace *child_as;
        struct trapframe *child_tf;
        int result;

        child = proc_create_runprogram(curproc->p_name);
        if (child == NULL) {
                return ENOMEM;
        }

        result = as_copy(curproc_getas(), &child_as);
        if (result) {
                proc_destroy_incomplete(child);
                return result;
        }
        child->p_addrspace = child_as;

        child->p_parent_pid = curproc->p_pid;

        child_tf = kmalloc(sizeof(struct trapframe));
        if (child_tf == NULL) {
                proc_destroy_incomplete(child);
                return ENOMEM;
        }
        *child_tf = *tf;

        result = thread_fork(curthread->t_name, child,
                              (void (*)(void *, unsigned long))enter_forked_process,
                              child_tf, 0);
        if (result) {
                kfree(child_tf);
                proc_destroy_incomplete(child);
                return result;
        }

        *retval = child->p_pid;
        return 0;
}

/*
 * Copy an argv-style array of user-space strings into kernel space.
 * ARGS is a user-space pointer to a NULL-terminated array of
 * user-space char* pointers. Returns a kernel array of kernel-space
 * strings (also NULL-terminated conceptually via *nargs_ret) that
 * the caller must free with free_kargs().
 */
#define MAX_ARGS 64

static int
copyin_args(userptr_t args, char ***kargs_ret, int *nargs_ret)
{
        char **kargs;
        int nargs;
        int result;
        int i;

        if (args == NULL) {
                *kargs_ret = NULL;
                *nargs_ret = 0;
                return 0;
        }

        kargs = kmalloc(MAX_ARGS * sizeof(char *));
        if (kargs == NULL) {
                return ENOMEM;
        }

        nargs = 0;
        for (i = 0; i < MAX_ARGS; i++) {
                userptr_t uarg;
                char *kbuf;
                size_t got;

                result = copyin((userptr_t)((char *)args + i * sizeof(userptr_t)),
                                 &uarg, sizeof(userptr_t));
                if (result) {
                        goto fail;
                }
                if (uarg == NULL) {
                        break;
                }

                kbuf = kmalloc(PATH_MAX);
                if (kbuf == NULL) {
                        result = ENOMEM;
                        goto fail;
                }
                result = copyinstr(uarg, kbuf, PATH_MAX, &got);
                if (result) {
                        kfree(kbuf);
                        goto fail;
                }
                kargs[nargs] = kbuf;
                nargs++;
        }

        *kargs_ret = kargs;
        *nargs_ret = nargs;
        return 0;

fail:
        for (i = 0; i < nargs; i++) {
                kfree(kargs[i]);
        }
        kfree(kargs);
        return result;
}

static void
free_kargs(char **kargs, int nargs)
{
        int i;

        if (kargs == NULL) {
                return;
        }
        for (i = 0; i < nargs; i++) {
                kfree(kargs[i]);
        }
        kfree(kargs);
}

/*
 * Copy NARGS kernel-space strings onto the new user stack, along with
 * a NULL-terminated argv pointer array. Updates *STACKPTR_INOUT to
 * the new stack pointer, and sets *ARGV_UPT to the user-space address
 * of the argv array.
 */
static int
build_arg_stack(char **kargs, int nargs, vaddr_t *stackptr_inout, userptr_t *argv_upt)
{
        vaddr_t sp = *stackptr_inout;
        userptr_t *uargv;
        int i, result;

        uargv = kmalloc((nargs + 1) * sizeof(userptr_t));
        if (uargv == NULL) {
                return ENOMEM;
        }

        /* copy strings onto the stack, last argument first */
        for (i = nargs - 1; i >= 0; i--) {
                size_t len = strlen(kargs[i]) + 1;
                sp -= len;
                result = copyoutstr(kargs[i], (userptr_t)sp, len, NULL);
                if (result) {
                        kfree(uargv);
                        return result;
                }
                uargv[i] = (userptr_t)sp;
        }

        /* align down to 4 bytes before the pointer array */
        sp -= (sp % 4);

        /* reserve space for the argv pointer array (nargs+1 entries) */
        sp -= (nargs + 1) * sizeof(userptr_t);
        /* keep final stack pointer 8-byte aligned */
        sp -= (sp % 8);

        for (i = 0; i <= nargs; i++) {
                userptr_t entry = (i == nargs) ? NULL : uargv[i];
                result = copyout(&entry, (userptr_t)(sp + i * sizeof(userptr_t)), sizeof(userptr_t));
                if (result) {
                        kfree(uargv);
                        return result;
                }
        }

        *argv_upt = (userptr_t)sp;
        *stackptr_inout = sp;

        kfree(uargv);
        return 0;
}

/*
 * execv() - replace the current address space with a new program,
 * passing along the given arguments.
 */
int
sys_execv(userptr_t progname, userptr_t args)
{
        char *kprogname;
        char **kargs;
        int nargs;
        struct addrspace *as_new;
        struct addrspace *as_old;
        struct vnode *v;
        vaddr_t entrypoint, stackptr;
        userptr_t argv_upt;
        int result;
        size_t got;

        /* copy the program name into kernel space */
        kprogname = kmalloc(PATH_MAX);
        if (kprogname == NULL) {
                return ENOMEM;
        }
        result = copyinstr(progname, kprogname, PATH_MAX, &got);
        if (result) {
                kfree(kprogname);
                return result;
        }

        /* copy the argument strings into kernel space (must happen
           while the OLD address space is still active) */
        result = copyin_args(args, &kargs, &nargs);
        if (result) {
                kfree(kprogname);
                return result;
        }

        /* open the executable */
        result = vfs_open(kprogname, O_RDONLY, 0, &v);
        if (result) {
                kfree(kprogname);
                free_kargs(kargs, nargs);
                return result;
        }
        kfree(kprogname);

        /* create a new address space, but don't discard the old one yet */
        as_new = as_create();
        if (as_new == NULL) {
                vfs_close(v);
                free_kargs(kargs, nargs);
                return ENOMEM;
        }

        as_old = curproc_setas(as_new);
        as_activate();

        result = load_elf(v, &entrypoint);
        if (result) {
                vfs_close(v);
                curproc_setas(as_old);
                as_activate();
                as_destroy(as_new);
                free_kargs(kargs, nargs);
                return result;
        }

        vfs_close(v);

        result = as_define_stack(as_new, &stackptr);
        if (result) {
                curproc_setas(as_old);
                as_activate();
                as_destroy(as_new);
                free_kargs(kargs, nargs);
                return result;
        }

        /* copy the arguments onto the new stack */
        result = build_arg_stack(kargs, nargs, &stackptr, &argv_upt);
        free_kargs(kargs, nargs);
        if (result) {
                curproc_setas(as_old);
                as_activate();
                as_destroy(as_new);
                return result;
        }

        /* success: permanently discard the old address space */
        as_destroy(as_old);

        enter_new_process(nargs, argv_upt, stackptr, entrypoint);

        panic("enter_new_process returned in sys_execv\n");
        return EINVAL;
}
#endif
