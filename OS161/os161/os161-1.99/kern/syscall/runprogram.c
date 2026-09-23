#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <syscall.h>
#include <copyinout.h>
#include <test.h>

static int
build_runprogram_stack(char **args, unsigned long nargs,
                        vaddr_t *stackptr_inout, userptr_t *argv_upt)
{
        vaddr_t sp = *stackptr_inout;
        userptr_t *uargv;
        long i;
        int result;

        uargv = kmalloc((nargs + 1) * sizeof(userptr_t));
        if (uargv == NULL) {
                return ENOMEM;
        }

        for (i = (long)nargs - 1; i >= 0; i--) {
                size_t len = strlen(args[i]) + 1;
                sp -= len;
                result = copyoutstr(args[i], (userptr_t)sp, len, NULL);
                if (result) {
                        kfree(uargv);
                        return result;
                }
                uargv[i] = (userptr_t)sp;
        }

        sp -= (sp % 4);
        sp -= (nargs + 1) * sizeof(userptr_t);
        sp -= (sp % 8);

        for (i = 0; i <= (long)nargs; i++) {
                userptr_t entry = (i == (long)nargs) ? NULL : uargv[i];
                result = copyout(&entry, (userptr_t)(sp + i * sizeof(userptr_t)),
                                  sizeof(userptr_t));
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
 * Load program "progname" and start running it in usermode, passing
 * along the given argument array.
 * Does not return except on error.
 */
int
runprogram(char *progname, char **args, unsigned long nargs)
{
        struct addrspace *as;
        struct vnode *v;
        vaddr_t entrypoint, stackptr;
        userptr_t argv_upt;
        int result;

        result = vfs_open(progname, O_RDONLY, 0, &v);
        if (result) {
                return result;
        }

        KASSERT(curproc_getas() == NULL);

        as = as_create();
        if (as == NULL) {
                vfs_close(v);
                return ENOMEM;
        }

        curproc_setas(as);
        as_activate();

        result = load_elf(v, &entrypoint);
        if (result) {
                vfs_close(v);
                return result;
        }

        vfs_close(v);

        result = as_define_stack(as, &stackptr);
        if (result) {
                return result;
        }

        result = build_runprogram_stack(args, nargs, &stackptr, &argv_upt);
        if (result) {
                return result;
        }

        enter_new_process(nargs, argv_upt, stackptr, entrypoint);

        panic("enter_new_process returned\n");
        return EINVAL;
}
