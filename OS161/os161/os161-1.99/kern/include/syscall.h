#ifndef _SYSCALL_H_
#define _SYSCALL_H_

struct trapframe;

void syscall(struct trapframe *tf);

void enter_forked_process(struct trapframe *tf);

void enter_new_process(int argc, userptr_t argv, vaddr_t stackptr,
                       vaddr_t entrypoint);

int sys_reboot(int code);
int sys___time(userptr_t user_seconds, userptr_t user_nanoseconds);

#ifdef UW
int sys_write(int fdesc,userptr_t ubuf,unsigned int nbytes,int *retval);
void sys__exit(int exitcode);
int sys_getpid(pid_t *retval);
int sys_waitpid(pid_t pid, userptr_t status, int options, pid_t *retval);
int sys_fork(struct trapframe *tf, pid_t *retval);
int sys_execv(userptr_t progname, userptr_t args);

#endif // UW

#endif /* _SYSCALL_H_ */
