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
/*
 *  AArch64 specific helpers
 *
 *  Copyright (c) 2013 Alexander Graf <agraf@suse.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/gdbstub.h"
#include "exec/helper-proto.h"
#include "qemu/host-utils.h"
#include "qemu/log.h"
#include "sysemu/sysemu.h"
#include "qemu/bitops.h"
#include "internals.h"
#include "qemu/crc32c.h"
#include "exec/exec-all.h"
#include "exec/cpu_ldst.h"
#include "qemu/int128.h"
#include "tcg.h"
#include <zlib.h> /* For crc32 */

#if defined(CONFIG_FLEXUS) && defined(CONFIG_EXTSNAP)
#include "include/sysemu/sysemu.h"
#include "qmp-commands.h"
static uint64_t num_inst;
static bool phases_init = false;
void helper_phases(CPUARMState *env)
{
    if (is_phases_enabled()) {
        if (!phases_init) {
            phases_init = true;
        }
        if (phase_is_valid()) {
            if (++num_inst == get_phase_value()) {
                vm_stop(RUN_STATE_PAUSED);
                save_phase();
                pop_phase();
                num_inst = 0;
            }
        } else if (!save_request_pending()) {
            fprintf(stderr, "done creating phases.");
            toggle_phases_creation();
            request_quit();
        }
    } else if (is_ckpt_enabled()) {
        if (++num_inst % get_ckpt_interval() == 0) {
            vm_stop(RUN_STATE_PAUSED);
            save_ckpt();
        }

        if (num_inst >= get_ckpt_end()) {
            toggle_ckpt_creation();
            fprintf(stderr, "done creating checkpoints.");
            request_quit();
        }


    }

}
#endif
#ifdef CONFIG_QUANTUM
static uint64_t *tni; // total num instructions
static uint64_t qv, qr, qn, qs; // total num instructions
const char* qf = NULL;
static FILE* qfile = NULL;
static bool qinit = false,start_record= false,cont_record= true;
static double start_time = 0.0f, diff_time = 0.0f, tmp = 0.0f;
static int qidx;

void helper_quantum(CPUARMState *env)
{
    tni = increment_total_num_instr();
    if (unlikely(!qinit)) {
        qv = query_quantum_core_value() ;
        qr = query_quantum_record_value();
        qn = query_quantum_node_value();
        qs = query_quantum_step_value();
        qf = query_quantum_file_value();
        if (qr > 0){
            qfile = fopen(qf, "w");
            if (qfile == NULL)
                assert(false);
        }
        qinit = true;
    }

    if (qv > 0){
        CPUState *cs = ENV_GET_CPU(env);
        if ((cs->nr_total_instr++) % qv == 0){
            cs->hasReachedInstrLimit = true;
            cs->nr_quantumHits++;
        }
    }
    if (qr > 0){
        if (!start_record){
            start_time = ((double)clock())/ CLOCKS_PER_SEC;
            start_record = true;
            fprintf(qfile, "#Recording %iM instrcutions\n", (int)(qr/1e6) );
            fprintf(qfile, "#Interval: %iM instrcution\n\n", (int)(qs/1e6) );
            fprintf(qfile, "#index  speed  time\n");
        } else {
            if(*tni % qs == 0 && cont_record){
                diff_time = (((double)clock())/ CLOCKS_PER_SEC);
                tmp = diff_time;
                diff_time -= start_time;
                start_time = tmp;
                fprintf(qfile, " %i  %i  %f\n",++qidx, (int)((qs / diff_time)/1e6), diff_time);
                if (*tni == qr ){
                    cont_record = false;
                    fclose(qfile);
                    fprintf(stdout, "'\e[1;31mDone writing a Quantum record file!\e[m");
                }
            }
        }
    }
    if (qn > 0){
        if (*tni % qn== 0){
            if (query_quantum_pause_state()){
                vm_stop(RUN_STATE_PAUSED);
            }else{
                raise(SIGSTOP);
            }
        }
    }
}
#endif

/* C2.4.7 Multiply and divide */
/* special cases for 0 and LLONG_MIN are mandated by the standard */
uint64_t HELPER(udiv64)(uint64_t num, uint64_t den)
{
    if (den == 0) {
        return 0;
    }
    return num / den;
}

int64_t HELPER(sdiv64)(int64_t num, int64_t den)
{
    if (den == 0) {
        return 0;
    }
    if (num == LLONG_MIN && den == -1) {
        return LLONG_MIN;
    }
    return num / den;
}

uint64_t HELPER(rbit64)(uint64_t x)
{
    return revbit64(x);
}

/* Convert a softfloat float_relation_ (as returned by
 * the float*_compare functions) to the correct ARM
 * NZCV flag state.
 */
static inline uint32_t float_rel_to_flags(int res)
{
    uint64_t flags;
    switch (res) {
    case float_relation_equal:
        flags = PSTATE_Z | PSTATE_C;
        break;
    case float_relation_less:
        flags = PSTATE_N;
        break;
    case float_relation_greater:
        flags = PSTATE_C;
        break;
    case float_relation_unordered:
    default:
        flags = PSTATE_C | PSTATE_V;
        break;
    }
    return flags;
}

uint64_t HELPER(vfp_cmps_a64)(float32 x, float32 y, void *fp_status)
{
    return float_rel_to_flags(float32_compare_quiet(x, y, fp_status));
}

uint64_t HELPER(vfp_cmpes_a64)(float32 x, float32 y, void *fp_status)
{
    return float_rel_to_flags(float32_compare(x, y, fp_status));
}

uint64_t HELPER(vfp_cmpd_a64)(float64 x, float64 y, void *fp_status)
{
    return float_rel_to_flags(float64_compare_quiet(x, y, fp_status));
}

uint64_t HELPER(vfp_cmped_a64)(float64 x, float64 y, void *fp_status)
{
    return float_rel_to_flags(float64_compare(x, y, fp_status));
}

float32 HELPER(vfp_mulxs)(float32 a, float32 b, void *fpstp)
{
    float_status *fpst = fpstp;

    a = float32_squash_input_denormal(a, fpst);
    b = float32_squash_input_denormal(b, fpst);

    if ((float32_is_zero(a) && float32_is_infinity(b)) ||
        (float32_is_infinity(a) && float32_is_zero(b))) {
        /* 2.0 with the sign bit set to sign(A) XOR sign(B) */
        return make_float32((1U << 30) |
                            ((float32_val(a) ^ float32_val(b)) & (1U << 31)));
    }
    return float32_mul(a, b, fpst);
}

float64 HELPER(vfp_mulxd)(float64 a, float64 b, void *fpstp)
{
    float_status *fpst = fpstp;

    a = float64_squash_input_denormal(a, fpst);
    b = float64_squash_input_denormal(b, fpst);

    if ((float64_is_zero(a) && float64_is_infinity(b)) ||
        (float64_is_infinity(a) && float64_is_zero(b))) {
        /* 2.0 with the sign bit set to sign(A) XOR sign(B) */
        return make_float64((1ULL << 62) |
                            ((float64_val(a) ^ float64_val(b)) & (1ULL << 63)));
    }
    return float64_mul(a, b, fpst);
}

uint64_t HELPER(simd_tbl)(CPUARMState *env, uint64_t result, uint64_t indices,
                          uint32_t rn, uint32_t numregs)
{
    /* Helper function for SIMD TBL and TBX. We have to do the table
     * lookup part for the 64 bits worth of indices we're passed in.
     * result is the initial results vector (either zeroes for TBL
     * or some guest values for TBX), rn the register number where
     * the table starts, and numregs the number of registers in the table.
     * We return the results of the lookups.
     */
    int shift;

    for (shift = 0; shift < 64; shift += 8) {
        int index = extract64(indices, shift, 8);
        if (index < 16 * numregs) {
            /* Convert index (a byte offset into the virtual table
             * which is a series of 128-bit vectors concatenated)
             * into the correct vfp.regs[] element plus a bit offset
             * into that element, bearing in mind that the table
             * can wrap around from V31 to V0.
             */
            int elt = (rn * 2 + (index >> 3)) % 64;
            int bitidx = (index & 7) * 8;
            uint64_t val = extract64(env->vfp.regs[elt], bitidx, 8);

            result = deposit64(result, shift, 8, val);
        }
    }
    return result;
}

/* 64bit/double versions of the neon float compare functions */
uint64_t HELPER(neon_ceq_f64)(float64 a, float64 b, void *fpstp)
{
    float_status *fpst = fpstp;
    return -float64_eq_quiet(a, b, fpst);
}

uint64_t HELPER(neon_cge_f64)(float64 a, float64 b, void *fpstp)
{
    float_status *fpst = fpstp;
    return -float64_le(b, a, fpst);
}

uint64_t HELPER(neon_cgt_f64)(float64 a, float64 b, void *fpstp)
{
    float_status *fpst = fpstp;
    return -float64_lt(b, a, fpst);
}

/* Reciprocal step and sqrt step. Note that unlike the A32/T32
 * versions, these do a fully fused multiply-add or
 * multiply-add-and-halve.
 */
#define float32_two make_float32(0x40000000)
#define float32_three make_float32(0x40400000)
#define float32_one_point_five make_float32(0x3fc00000)

#define float64_two make_float64(0x4000000000000000ULL)
#define float64_three make_float64(0x4008000000000000ULL)
#define float64_one_point_five make_float64(0x3FF8000000000000ULL)

float32 HELPER(recpsf_f32)(float32 a, float32 b, void *fpstp)
{
    float_status *fpst = fpstp;

    a = float32_squash_input_denormal(a, fpst);
    b = float32_squash_input_denormal(b, fpst);

    a = float32_chs(a);
    if ((float32_is_infinity(a) && float32_is_zero(b)) ||
        (float32_is_infinity(b) && float32_is_zero(a))) {
        return float32_two;
    }
    return float32_muladd(a, b, float32_two, 0, fpst);
}

float64 HELPER(recpsf_f64)(float64 a, float64 b, void *fpstp)
{
    float_status *fpst = fpstp;

    a = float64_squash_input_denormal(a, fpst);
    b = float64_squash_input_denormal(b, fpst);

    a = float64_chs(a);
    if ((float64_is_infinity(a) && float64_is_zero(b)) ||
        (float64_is_infinity(b) && float64_is_zero(a))) {
        return float64_two;
    }
    return float64_muladd(a, b, float64_two, 0, fpst);
}

float32 HELPER(rsqrtsf_f32)(float32 a, float32 b, void *fpstp)
{
    float_status *fpst = fpstp;

    a = float32_squash_input_denormal(a, fpst);
    b = float32_squash_input_denormal(b, fpst);

    a = float32_chs(a);
    if ((float32_is_infinity(a) && float32_is_zero(b)) ||
        (float32_is_infinity(b) && float32_is_zero(a))) {
        return float32_one_point_five;
    }
    return float32_muladd(a, b, float32_three, float_muladd_halve_result, fpst);
}

float64 HELPER(rsqrtsf_f64)(float64 a, float64 b, void *fpstp)
{
    float_status *fpst = fpstp;

    a = float64_squash_input_denormal(a, fpst);
    b = float64_squash_input_denormal(b, fpst);

    a = float64_chs(a);
    if ((float64_is_infinity(a) && float64_is_zero(b)) ||
        (float64_is_infinity(b) && float64_is_zero(a))) {
        return float64_one_point_five;
    }
    return float64_muladd(a, b, float64_three, float_muladd_halve_result, fpst);
}

/* Pairwise long add: add pairs of adjacent elements into
 * double-width elements in the result (eg _s8 is an 8x8->16 op)
 */
uint64_t HELPER(neon_addlp_s8)(uint64_t a)
{
    uint64_t nsignmask = 0x0080008000800080ULL;
    uint64_t wsignmask = 0x8000800080008000ULL;
    uint64_t elementmask = 0x00ff00ff00ff00ffULL;
    uint64_t tmp1, tmp2;
    uint64_t res, signres;

    /* Extract odd elements, sign extend each to a 16 bit field */
    tmp1 = a & elementmask;
    tmp1 ^= nsignmask;
    tmp1 |= wsignmask;
    tmp1 = (tmp1 - nsignmask) ^ wsignmask;
    /* Ditto for the even elements */
    tmp2 = (a >> 8) & elementmask;
    tmp2 ^= nsignmask;
    tmp2 |= wsignmask;
    tmp2 = (tmp2 - nsignmask) ^ wsignmask;

    /* calculate the result by summing bits 0..14, 16..22, etc,
     * and then adjusting the sign bits 15, 23, etc manually.
     * This ensures the addition can't overflow the 16 bit field.
     */
    signres = (tmp1 ^ tmp2) & wsignmask;
    res = (tmp1 & ~wsignmask) + (tmp2 & ~wsignmask);
    res ^= signres;

    return res;
}

uint64_t HELPER(neon_addlp_u8)(uint64_t a)
{
    uint64_t tmp;

    tmp = a & 0x00ff00ff00ff00ffULL;
    tmp += (a >> 8) & 0x00ff00ff00ff00ffULL;
    return tmp;
}

uint64_t HELPER(neon_addlp_s16)(uint64_t a)
{
    int32_t reslo, reshi;

    reslo = (int32_t)(int16_t)a + (int32_t)(int16_t)(a >> 16);
    reshi = (int32_t)(int16_t)(a >> 32) + (int32_t)(int16_t)(a >> 48);

    return (uint32_t)reslo | (((uint64_t)reshi) << 32);
}

uint64_t HELPER(neon_addlp_u16)(uint64_t a)
{
    uint64_t tmp;

    tmp = a & 0x0000ffff0000ffffULL;
    tmp += (a >> 16) & 0x0000ffff0000ffffULL;
    return tmp;
}

/* Floating-point reciprocal exponent - see FPRecpX in ARM ARM */
float32 HELPER(frecpx_f32)(float32 a, void *fpstp)
{
    float_status *fpst = fpstp;
    uint32_t val32, sbit;
    int32_t exp;

    if (float32_is_any_nan(a)) {
        float32 nan = a;
        if (float32_is_signaling_nan(a, fpst)) {
            float_raise(float_flag_invalid, fpst);
            nan = float32_maybe_silence_nan(a, fpst);
        }
        if (fpst->default_nan_mode) {
            nan = float32_default_nan(fpst);
        }
        return nan;
    }

    val32 = float32_val(a);
    sbit = 0x80000000ULL & val32;
    exp = extract32(val32, 23, 8);

    if (exp == 0) {
        return make_float32(sbit | (0xfe << 23));
    } else {
        return make_float32(sbit | (~exp & 0xff) << 23);
    }
}

float64 HELPER(frecpx_f64)(float64 a, void *fpstp)
{
    float_status *fpst = fpstp;
    uint64_t val64, sbit;
    int64_t exp;

    if (float64_is_any_nan(a)) {
        float64 nan = a;
        if (float64_is_signaling_nan(a, fpst)) {
            float_raise(float_flag_invalid, fpst);
            nan = float64_maybe_silence_nan(a, fpst);
        }
        if (fpst->default_nan_mode) {
            nan = float64_default_nan(fpst);
        }
        return nan;
    }

    val64 = float64_val(a);
    sbit = 0x8000000000000000ULL & val64;
    exp = extract64(float64_val(a), 52, 11);

    if (exp == 0) {
        return make_float64(sbit | (0x7feULL << 52));
    } else {
        return make_float64(sbit | (~exp & 0x7ffULL) << 52);
    }
}

float32 HELPER(fcvtx_f64_to_f32)(float64 a, CPUARMState *env)
{
    /* Von Neumann rounding is implemented by using round-to-zero
     * and then setting the LSB of the result if Inexact was raised.
     */
    float32 r;
    float_status *fpst = &env->vfp.fp_status;
    float_status tstat = *fpst;
    int exflags;

    set_float_rounding_mode(float_round_to_zero, &tstat);
    set_float_exception_flags(0, &tstat);
    r = float64_to_float32(a, &tstat);
    r = float32_maybe_silence_nan(r, &tstat);
    exflags = get_float_exception_flags(&tstat);
    if (exflags & float_flag_inexact) {
        r = make_float32(float32_val(r) | 1);
    }
    exflags |= get_float_exception_flags(fpst);
    set_float_exception_flags(exflags, fpst);
    return r;
}

/* 64-bit versions of the CRC helpers. Note that although the operation
 * (and the prototypes of crc32c() and crc32() mean that only the bottom
 * 32 bits of the accumulator and result are used, we pass and return
 * uint64_t for convenience of the generated code. Unlike the 32-bit
 * instruction set versions, val may genuinely have 64 bits of data in it.
 * The upper bytes of val (above the number specified by 'bytes') must have
 * been zeroed out by the caller.
 */
uint64_t HELPER(crc32_64)(uint64_t acc, uint64_t val, uint32_t bytes)
{
    uint8_t buf[8];

    stq_le_p(buf, val);

    /* zlib crc32 converts the accumulator and output to one's complement.  */
    return crc32(acc ^ 0xffffffff, buf, bytes) ^ 0xffffffff;
}

uint64_t HELPER(crc32c_64)(uint64_t acc, uint64_t val, uint32_t bytes)
{
    uint8_t buf[8];

    stq_le_p(buf, val);

    /* Linux crc32c converts the output to one's complement.  */
    return crc32c(acc, buf, bytes) ^ 0xffffffff;
}

#ifdef CONFIG_FLEXUS
/* Aarch 64 helpers */
void helper_flexus_insn_fetch_aa64( CPUARMState *env,
			       target_ulong pc,
			       uint64_t targ_addr,
			       int ins_size,
			       int is_user,
			       int cond,
			       int annul ) {
  helper_flexus_insn_fetch(env, pc, targ_addr, ins_size, is_user, cond, annul);
}
void helper_flexus_ld_aa64( CPUARMState *env,
		       uint64_t addr,
		       int size,
		       int is_user,
		       target_ulong pc,
		       int is_atomic ) {
  helper_flexus_ld(env, addr, size, is_user, pc, is_atomic);
}
void helper_flexus_st_aa64(
		      CPUARMState *env,
		      uint64_t addr,
		      int size,
		      int is_user,
		      target_ulong pc,
		      int is_atomic) {
  helper_flexus_st(env, addr, size, is_user, pc, is_atomic);
}
#endif
/* Returns 0 on success; 1 otherwise.  */
uint64_t HELPER(paired_cmpxchg64_le)(CPUARMState *env, uint64_t addr,
                                     uint64_t new_lo, uint64_t new_hi)
{
    uintptr_t ra = GETPC();
    Int128 oldv, cmpv, newv;
    bool success;

    cmpv = int128_make128(env->exclusive_val, env->exclusive_high);
    newv = int128_make128(new_lo, new_hi);

    if (parallel_cpus) {
#ifndef CONFIG_ATOMIC128
        cpu_loop_exit_atomic(ENV_GET_CPU(env), ra);
#else
        int mem_idx = cpu_mmu_index(env, false);
        TCGMemOpIdx oi = make_memop_idx(MO_LEQ | MO_ALIGN_16, mem_idx);
        oldv = helper_atomic_cmpxchgo_le_mmu(env, addr, cmpv, newv, oi, ra);
        success = int128_eq(oldv, cmpv);
#endif
    } else {
        uint64_t o0, o1;

#ifdef CONFIG_USER_ONLY
        /* ??? Enforce alignment.  */
        uint64_t *haddr = g2h(addr);
        o0 = ldq_le_p(haddr + 0);
        o1 = ldq_le_p(haddr + 1);
        oldv = int128_make128(o0, o1);

        success = int128_eq(oldv, cmpv);
        if (success) {
            stq_le_p(haddr + 0, int128_getlo(newv));
            stq_le_p(haddr + 1, int128_gethi(newv));
        }
#else
        int mem_idx = cpu_mmu_index(env, false);
        TCGMemOpIdx oi0 = make_memop_idx(MO_LEQ | MO_ALIGN_16, mem_idx);
        TCGMemOpIdx oi1 = make_memop_idx(MO_LEQ, mem_idx);

        o0 = helper_le_ldq_mmu(env, addr + 0, oi0, ra);
        o1 = helper_le_ldq_mmu(env, addr + 8, oi1, ra);
        oldv = int128_make128(o0, o1);

        success = int128_eq(oldv, cmpv);
        if (success) {
            helper_le_stq_mmu(env, addr + 0, int128_getlo(newv), oi1, ra);
            helper_le_stq_mmu(env, addr + 8, int128_gethi(newv), oi1, ra);
        }
#endif
    }

    return !success;
}

uint64_t HELPER(paired_cmpxchg64_be)(CPUARMState *env, uint64_t addr,
                                     uint64_t new_lo, uint64_t new_hi)
{
    uintptr_t ra = GETPC();
    Int128 oldv, cmpv, newv;
    bool success;

    cmpv = int128_make128(env->exclusive_val, env->exclusive_high);
    newv = int128_make128(new_lo, new_hi);

    if (parallel_cpus) {
#ifndef CONFIG_ATOMIC128
        cpu_loop_exit_atomic(ENV_GET_CPU(env), ra);
#else
        int mem_idx = cpu_mmu_index(env, false);
        TCGMemOpIdx oi = make_memop_idx(MO_BEQ | MO_ALIGN_16, mem_idx);
        oldv = helper_atomic_cmpxchgo_be_mmu(env, addr, cmpv, newv, oi, ra);
        success = int128_eq(oldv, cmpv);
#endif
    } else {
        uint64_t o0, o1;

#ifdef CONFIG_USER_ONLY
        /* ??? Enforce alignment.  */
        uint64_t *haddr = g2h(addr);
        o1 = ldq_be_p(haddr + 0);
        o0 = ldq_be_p(haddr + 1);
        oldv = int128_make128(o0, o1);

        success = int128_eq(oldv, cmpv);
        if (success) {
            stq_be_p(haddr + 0, int128_gethi(newv));
            stq_be_p(haddr + 1, int128_getlo(newv));
        }
#else
        int mem_idx = cpu_mmu_index(env, false);
        TCGMemOpIdx oi0 = make_memop_idx(MO_BEQ | MO_ALIGN_16, mem_idx);
        TCGMemOpIdx oi1 = make_memop_idx(MO_BEQ, mem_idx);

        o1 = helper_be_ldq_mmu(env, addr + 0, oi0, ra);
        o0 = helper_be_ldq_mmu(env, addr + 8, oi1, ra);
        oldv = int128_make128(o0, o1);

        success = int128_eq(oldv, cmpv);
        if (success) {
            helper_be_stq_mmu(env, addr + 0, int128_gethi(newv), oi1, ra);
            helper_be_stq_mmu(env, addr + 8, int128_getlo(newv), oi1, ra);
        }
#endif
    }

    return !success;
}
