/** HELPER definitions for TCG code generation.
 */

DEF_HELPER_4(qflex_executed_instruction, void, env, i64, int, int)
DEF_HELPER_1(qflex_magic_insn, void, int)
DEF_HELPER_1(qflex_exception_return, void, env)

#ifdef CONFIG_FLEXUS
// maginc instruction operand
DEF_HELPER_1(flexus_magic_ins, void, int)
// No operands
DEF_HELPER_2(flexus_periodic, void, env, int)
// env, pc, target address, ins_size, is user, conditional or not, annulation or not (execute delay slot or not)
DEF_HELPER_7(flexus_insn_fetch, void, env, tl, tl, int, int, int, int)
// env, addr, size, is user, pc, is atomic
DEF_HELPER_6(flexus_ld, void, env, tl, int, int, tl, int)
// env, addr, size, is user, pc, is atomic
DEF_HELPER_6(flexus_st, void, env, tl, int, int, tl, int)
// specific versions for aarch32
// env, pc, target address, ins_size, is user, conditional or not, annulation or not (execute delay slot or not)
DEF_HELPER_7(flexus_insn_fetch_aa32, void, env, tl, i32, int, int, int, int)
// env, addr, size, is user, pc, is atomic
DEF_HELPER_6(flexus_ld_aa32, void, env, i32, int, int, tl, int)
// env, addr, size, is user, pc, is atomic
DEF_HELPER_6(flexus_st_aa32, void, env, i32, int, int, tl, int)
#endif /* CONFIG_FLEXUS */
