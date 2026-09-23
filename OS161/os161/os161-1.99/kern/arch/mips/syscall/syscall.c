#include <types.h>
#include <kern/errno.h>
#include <kern/syscall.h>
#include <lib.h>
#include <mips/trapframe.h>
#include <thread.h>
#include <current.h>
#include <syscall.h>

void
syscall(struct trapframe *tf)
{
        int callno;
        int32_t retval;
        int err;

        KASSERT(curthread != NULL);
        KASSERT(curthread->t_curspl == 0);
        KASSERT(curthread->t_iplhigh_count == 0);

        callno = tf->tf_v0;

        retval = 0;

        switch (callno) {
            case SYS_reboot:
                err = sys_reboot(tf->tf_a0);
                break;

            case SYS___time:
                err = sys___time((userptr_t)tf->tf_a0,
                                 (userptr_t)tf->tf_a1);
                break;
#ifdef UW
        case SYS_write:
          err = sys_write((int)tf->tf_a0,
                          (userptr_t)tf->tf_a1,
                          (int)tf->tf_a2,
                          (int *)(&retval));
          break;
        case SYS__exit:
          sys__exit((int)tf->tf_a0);
          panic("unexpected return from sys__exit");
          break;
        case SYS_getpid:
          err = sys_getpid((pid_t *)&retval);
          break;
        case SYS_waitpid:
          err = sys_waitpid((pid_t)tf->tf_a0,
                            (userptr_t)tf->tf_a1,
                            (int)tf->tf_a2,
                            (pid_t *)&retval);
          break;
        case SYS_fork:
          err = sys_fork(tf, (pid_t *)&retval);
          break;
        case SYS_execv:
          err = sys_execv((userptr_t)tf->tf_a0, (userptr_t)tf->tf_a1);
          break;
#endif // UW

        default:
          kprintf("Unknown syscall %d\n", callno);
          err = ENOSYS;
          break;
        }


        if (err) {
                tf->tf_v0 = err;
                tf->tf_a3 = 1;
        }
        else {
                tf->tf_v0 = retval;
                tf->tf_a3 = 0;
        }

        tf->tf_epc += 4;

        KASSERT(curthread->t_curspl == 0);
        KASSERT(curthread->t_iplhigh_count == 0);
}

void
enter_forked_process(struct trapframe *tf)
{
        struct trapframe mytf;

        mytf = *tf;
        kfree(tf);

        mytf.tf_v0 = 0;
        mytf.tf_a3 = 0;
        mytf.tf_epc += 4;

        mips_usermode(&mytf);
}
