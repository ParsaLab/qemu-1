//  DO-NOT-REMOVE begin-copyright-block
// QFlex consists of several software components that are governed by various
// licensing terms, in addition to software that was developed internally.
// Anyone interested in using QFlex needs to fully understand and abide by the
// licenses governing all the software components.
// 
// ### Software developed externally (not by the QFlex group)
// 
//     * [NS-3] (https://www.gnu.org/copyleft/gpl.html)
//     * [QEMU] (http://wiki.qemu.org/License)
//     * [SimFlex] (http://parsa.epfl.ch/simflex/)
//     * [GNU PTH] (https://www.gnu.org/software/pth/)
// 
// ### Software developed internally (by the QFlex group)
// **QFlex License**
// 
// QFlex
// Copyright (c) 2020, Parallel Systems Architecture Lab, EPFL
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
// 
//     * Redistributions of source code must retain the above copyright notice,
//       this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright notice,
//       this list of conditions and the following disclaimer in the documentation
//       and/or other materials provided with the distribution.
//     * Neither the name of the Parallel Systems Architecture Laboratory, EPFL,
//       nor the names of its contributors may be used to endorse or promote
//       products derived from this software without specific prior written
//       permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE PARALLEL SYSTEMS ARCHITECTURE LABORATORY,
// EPFL BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//  DO-NOT-REMOVE end-copyright-block
#ifndef QEMU_RCU_H
#define QEMU_RCU_H

/*
 * urcu-mb.h
 *
 * Userspace RCU header with explicit memory barrier.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * IBM's contributions to this file may be relicensed under LGPLv2 or later.
 */


#include "qemu/thread.h"
#include "qemu/queue.h"
#include "qemu/atomic.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Important !
 *
 * Each thread containing read-side critical sections must be registered
 * with rcu_register_thread() before calling rcu_read_lock().
 * rcu_unregister_thread() should be called before the thread exits.
 */

#ifdef DEBUG_RCU
#define rcu_assert(args...)    assert(args)
#else
#define rcu_assert(args...)
#endif

/*
 * Global quiescent period counter with low-order bits unused.
 * Using a int rather than a char to eliminate false register dependencies
 * causing stalls on some architectures.
 */
extern unsigned long rcu_gp_ctr;

extern QemuEvent rcu_gp_event;

#ifndef CONFIG_PTH
struct rcu_reader_data {
    /* Data used by both reader and synchronize_rcu() */
    unsigned long ctr;
    bool waiting;

    /* Data used by reader only */
    unsigned depth;

    /* Data used for registry, protected by rcu_registry_lock */
    QLIST_ENTRY(rcu_reader_data) node;
};
#else
#include "include/qemu/thread-pth.h"
#endif

#ifndef CONFIG_PTH
extern __thread struct rcu_reader_data rcu_reader;
#endif

static inline void rcu_read_lock(void)
{
    PTH_UPDATE_CONTEXT
    struct rcu_reader_data *p_rcu_reader = &PTH(rcu_reader);
    unsigned ctr;

    if (p_rcu_reader->depth++ > 0) {
        return;
    }

    ctr = atomic_read(&rcu_gp_ctr);
    atomic_xchg(&p_rcu_reader->ctr, ctr);
}

static inline void rcu_read_unlock(void)
{
    PTH_UPDATE_CONTEXT
    struct rcu_reader_data *p_rcu_reader = &PTH(rcu_reader);

    assert(p_rcu_reader->depth != 0);
    if (--p_rcu_reader->depth > 0) {
        return;
    }

    atomic_xchg(&p_rcu_reader->ctr, 0);
    if (unlikely(atomic_read(&p_rcu_reader->waiting))) {
        atomic_set(&p_rcu_reader->waiting, false);
        qemu_event_set(&rcu_gp_event);
    }
}

extern void synchronize_rcu(void);

/*
 * Reader thread registration.
 */
extern void rcu_register_thread(void);
extern void rcu_unregister_thread(void);

/*
 * Support for fork().  fork() support is enabled at startup.
 */
extern void rcu_enable_atfork(void);
extern void rcu_disable_atfork(void);

struct rcu_head;
typedef void RCUCBFunc(struct rcu_head *head);

struct rcu_head {
    struct rcu_head *next;
    RCUCBFunc *func;
};

extern void call_rcu1(struct rcu_head *head, RCUCBFunc *func);

/* The operands of the minus operator must have the same type,
 * which must be the one that we specify in the cast.
 */
#define call_rcu(head, func, field)                                      \
    call_rcu1(({                                                         \
         char __attribute__((unused))                                    \
            offset_must_be_zero[-offsetof(typeof(*(head)), field)],      \
            func_type_invalid = (func) - (void (*)(typeof(head)))(func); \
         &(head)->field;                                                 \
      }),                                                                \
      (RCUCBFunc *)(func))

#define g_free_rcu(obj, field) \
    call_rcu1(({                                                         \
        char __attribute__((unused))                                     \
            offset_must_be_zero[-offsetof(typeof(*(obj)), field)];       \
        &(obj)->field;                                                   \
      }),                                                                \
      (RCUCBFunc *)g_free);

#ifdef __cplusplus
}
#endif

#endif /* QEMU_RCU_H */
