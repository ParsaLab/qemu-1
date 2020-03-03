/*
 * ucontext coroutine initialization code
 *
 * Copyright (C) 2006  Anthony Liguori <anthony@codemonkey.ws>
 * Copyright (C) 2011  Kevin Wolf <kwolf@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.0 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/* XXX Is there a nicer way to disable glibc's stack check for longjmp? */
#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif
#include "qemu/osdep.h"
#include <ucontext.h>
#include "qemu-common.h"
#include "qemu/coroutine_int.h"
#include "include/qemu/thread.h"

#ifndef CONFIG_PTH
#ifdef CONFIG_VALGRIND_H
#include <valgrind/valgrind.h>
#endif

typedef struct {
    Coroutine base;
    void *stack;
    size_t stack_size;
    sigjmp_buf env;

#ifdef CONFIG_VALGRIND_H
    unsigned int valgrind_stack_id;
#endif

} CoroutineUContext;
#else
#include "util/coroutine-ucontext-pth.h"
#endif

#if defined(__SANITIZE_ADDRESS__) || __has_feature(address_sanitizer)
#ifdef HAVE_ASAN_IFACE_FIBER
#define CONFIG_ASAN 1
#include <sanitizer/asan_interface.h>
#endif
#endif

/**
 * Per-thread coroutine bookkeeping
 */
#ifndef CONFIG_PTH
static __thread CoroutineUContext leader;
static __thread Coroutine *current;
#endif

/*
 * va_args to makecontext() must be type 'int', so passing
 * the pointer we need may require several int args. This
 * union is a quick hack to let us do that
 */
union cc_arg {
    void *p;
    int i[2];
};

static void finish_switch_fiber(void *fake_stack_save, bool update_leader_for_nest)
{
#ifdef CONFIG_ASAN
    const void *bottom_old;
    size_t size_old;

    __sanitizer_finish_switch_fiber(fake_stack_save, &bottom_old, &size_old);
    /* Gnu PTH */
    PTH_UPDATE_CONTEXT;
    if (update_leader_for_nest) {
        PTH(leader).stack = (void *)bottom_old;
        PTH(leader).stack_size = size_old;
    }
#endif
}

static void start_switch_fiber(void **fake_stack_save,
                               const void *bottom, size_t size)
{
#ifdef CONFIG_ASAN
#ifdef CONFIG_PTH
    /* Change the stack associated with this coroutine in the PTH library too */
    pth_wrapper *cur = pth_get_wrapper();
    pth_swap_cur_thread_sstate(cur->pth_thread,bottom,size); // from pth.h
#endif /* CONFIG_PTH */
    __sanitizer_start_switch_fiber(fake_stack_save, bottom, size);
#endif /* CONFIG_ASAN */
}

static void coroutine_trampoline(int i0, int i1)
{
    PTH_UPDATE_CONTEXT;
    union cc_arg arg;
    CoroutineUContext *self;
    Coroutine *co;
    void *fake_stack_save = NULL;

    finish_switch_fiber(NULL,true);

    arg.i[0] = i0;
    arg.i[1] = i1;
    self = arg.p;
    co = &self->base;

    /* Initialize longjmp environment and switch back the caller */
    if (!sigsetjmp(self->env, 0)) {
        start_switch_fiber(&fake_stack_save,
                           PTH(leader).stack, PTH(leader).stack_size);
        siglongjmp(*(sigjmp_buf *)co->entry_arg, 1);
    }

    finish_switch_fiber(fake_stack_save,false);

    while (true) {
        co->entry(co->entry_arg);
        qemu_coroutine_switch(co, co->caller, COROUTINE_TERMINATE);
    }
}

Coroutine *qemu_coroutine_new(void)
{
    CoroutineUContext *co;
    ucontext_t old_uc, uc;
    sigjmp_buf old_env;
    union cc_arg arg = {0};
    void *fake_stack_save = NULL;

    /* The ucontext functions preserve signal masks which incurs a
     * system call overhead.  sigsetjmp(buf, 0)/siglongjmp() does not
     * preserve signal masks but only works on the current stack.
     * Since we need a way to create and switch to a new stack, use
     * the ucontext functions for that but sigsetjmp()/siglongjmp() for
     * everything else.
     */

    if (getcontext(&uc) == -1) {
        abort();
    }

    co = g_malloc0(sizeof(*co));
    co->stack_size = COROUTINE_STACK_SIZE;
    co->stack = qemu_alloc_stack(&co->stack_size);
    co->base.entry_arg = &old_env; /* stash away our jmp_buf */

    uc.uc_link = &old_uc;
    uc.uc_stack.ss_sp = co->stack;
    uc.uc_stack.ss_size = co->stack_size;
    uc.uc_stack.ss_flags = 0;

#ifdef CONFIG_VALGRIND_H
    co->valgrind_stack_id =
        VALGRIND_STACK_REGISTER(co->stack, co->stack + co->stack_size);
#endif

    arg.p = co;

    makecontext(&uc, (void (*)(void))coroutine_trampoline,
                2, arg.i[0], arg.i[1]);

    /* swapcontext() in, siglongjmp() back out */
    if (!sigsetjmp(old_env, 0)) {
        start_switch_fiber(&fake_stack_save, co->stack, co->stack_size);
        swapcontext(&old_uc, &uc);
    }

    finish_switch_fiber(fake_stack_save,false);

    return &co->base;
}

#ifdef CONFIG_VALGRIND_H
#ifdef CONFIG_PRAGMA_DIAGNOSTIC_AVAILABLE
/* Work around an unused variable in the valgrind.h macro... */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
static inline void valgrind_stack_deregister(CoroutineUContext *co)
{
    VALGRIND_STACK_DEREGISTER(co->valgrind_stack_id);
}
#ifdef CONFIG_PRAGMA_DIAGNOSTIC_AVAILABLE
#pragma GCC diagnostic pop
#endif
#endif

void qemu_coroutine_delete(Coroutine *co_)
{
    CoroutineUContext *co = DO_UPCAST(CoroutineUContext, base, co_);

#ifdef CONFIG_VALGRIND_H
    valgrind_stack_deregister(co);
#endif

    qemu_free_stack(co->stack, co->stack_size);
    g_free(co);
}

/* This function is marked noinline to prevent GCC from inlining it
 * into coroutine_trampoline(). If we allow it to do that then it
 * hoists the code to get the address of the TLS variable "current"
 * out of the while() loop. This is an invalid transformation because
 * the sigsetjmp() call may be called when running thread A but
 * return in thread B, and so we might be in a different thread
 * context each time round the loop.
 */
CoroutineAction __attribute__((noinline))
qemu_coroutine_switch(Coroutine *from_, Coroutine *to_,
                      CoroutineAction action)
{
    PTH_UPDATE_CONTEXT;
    CoroutineUContext *from = DO_UPCAST(CoroutineUContext, base, from_);
    CoroutineUContext *to = DO_UPCAST(CoroutineUContext, base, to_);
    int ret;
    void *fake_stack_save = NULL;

    PTH(current) = to_;

    ret = sigsetjmp(from->env, 0);
    if (ret == 0) {
        start_switch_fiber(action == COROUTINE_TERMINATE ?
                           NULL : &fake_stack_save, to->stack, to->stack_size);
        siglongjmp(to->env, action);
    }

    finish_switch_fiber(fake_stack_save,false);

    return ret;
}

Coroutine *qemu_coroutine_self(void)
{
    PTH_UPDATE_CONTEXT;
    if (!PTH(current)) {
        PTH(current) = &PTH(leader).base;
    }
    return PTH(current);
}

bool qemu_in_coroutine(void)
{
    PTH_UPDATE_CONTEXT;
    return PTH(current) && PTH(current)->caller;
}
