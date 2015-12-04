/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: @(#)mips_opcode.h      7.1 (Berkeley) 3/19/92
 * via: mips_opcode.h,v 1.1 1994/03/10 16:15:10 (algorithmics)
 */

/*
 * Define the instruction formats and opcode values for the
 * MIPS instruction set.
 */

#ifndef _MIPS_OPCODE_H
#define _MIPS_OPCODE_H

/*
 * Define the instruction formats.
 */
typedef union {
    unsigned word;

#ifdef MIPSEL
    struct {
        unsigned imm: 16;
        unsigned rt: 5;
        unsigned rs: 5;
        unsigned op: 6;
    } IType;

    struct {
        unsigned target: 26;
        unsigned op: 6;
    } JType;

    struct {
        unsigned func: 6;
        unsigned shamt: 5;
        unsigned rd: 5;
        unsigned rt: 5;
        unsigned rs: 5;
        unsigned op: 6;
    } RType;

    struct {
        unsigned func: 6;
        unsigned fd: 5;
        unsigned fs: 5;
        unsigned ft: 5;
        unsigned fmt: 4;
        unsigned : 1;           /* always '1' */
        unsigned op: 6;         /* always '0x11' */
    } FRType;
#else
    struct {
        unsigned op: 6;
        unsigned rs: 5;
        unsigned rt: 5;
        unsigned imm: 16;
    } IType;

    struct {
        unsigned op: 6;
        unsigned target: 26;
    } JType;

    struct {
        unsigned op: 6;
        unsigned rs: 5;
        unsigned rt: 5;
        unsigned rd: 5;
        unsigned shamt: 5;
        unsigned func: 6;
    } RType;

    struct {
        unsigned op: 6;         /* always '0x11' */
        unsigned : 1;           /* always '1' */
        unsigned fmt: 4;
        unsigned func: 6;
        unsigned ft: 5;
        unsigned fs: 5;
        unsigned fd: 5;
    } FRType;
#endif
} InstFmt;

/*
 * Values for the 'op' field.
 */
#define OP_SPECIAL      000
#define OP_REGIMM       001
#define OP_J            002
#define OP_JAL          003
#define OP_BEQ          004
#define OP_BNE          005
#define OP_BLEZ         006
#define OP_BGTZ         007

#define OP_ADDI         010
#define OP_ADDIU        011
#define OP_SLTI         012
#define OP_SLTIU        013
#define OP_ANDI         014
#define OP_ORI          015
#define OP_XORI         016
#define OP_LUI          017

#define OP_COP0         020
#define OP_COP1         021
#define OP_COP2         022
#define OP_BEQL         024
#define OP_BNEL         025
#define OP_BLEZL        026
#define OP_BGTZL        027

#define OP_DADDI        030
#define OP_DADDIU       031
#define OP_LDL          032
#define OP_LDR          033

#define OP_LB           040
#define OP_LH           041
#define OP_LWL          042
#define OP_LW           043
#define OP_LBU          044
#define OP_LHU          045
#define OP_LWR          046
#define OP_LWU          047

#define OP_SB           050
#define OP_SH           051
#define OP_SWL          052
#define OP_SW           053
#define OP_SDL          054
#define OP_SDR          055
#define OP_SWR          056
#define OP_CACHE        057

#define OP_LL           060
#define OP_LWC1         061
#define OP_LWC2         062
#define OP_LLD          064
#define OP_LDC1         065
#define OP_LDC2         066
#define OP_LD           067

#define OP_SC           070
#define OP_SWC1         071
#define OP_SWC2         072
#define OP_SCD          074
#define OP_SDC1         075
#define OP_SDC2         076
#define OP_SD           077

/*
 * Values for the 'func' field when 'op' == OP_SPECIAL.
 */
#define OP_SLL          000
#define OP_SRL          002
#define OP_SRA          003
#define OP_SLLV         004
#define OP_SRLV         006
#define OP_SRAV         007

#define OP_JR           010
#define OP_JALR         011
#define OP_SYSCALL      014
#define OP_BREAK        015
#define OP_SYNC         017

#define OP_MFHI         020
#define OP_MTHI         021
#define OP_MFLO         022
#define OP_MTLO         023
#define OP_DSLLV        024
#define OP_DSRLV        026
#define OP_DSRAV        027

#define OP_MULT         030
#define OP_MULTU        031
#define OP_DIV          032
#define OP_DIVU         033
#define OP_DMULT        034
#define OP_DMULTU       035
#define OP_DDIV         036
#define OP_DDIVU        037

#define OP_ADD          040
#define OP_ADDU         041
#define OP_SUB          042
#define OP_SUBU         043
#define OP_AND          044
#define OP_OR           045
#define OP_XOR          046
#define OP_NOR          047

#define OP_SLT          052
#define OP_SLTU         053
#define OP_DADD         054
#define OP_DADDU        055
#define OP_DSUB         056
#define OP_DSUBU        057

#define OP_TGE          060
#define OP_TGEU         061
#define OP_TLT          062
#define OP_TLTU         063
#define OP_TEQ          064
#define OP_TNE          066

#define OP_DSLL         070
#define OP_DSRL         072
#define OP_DSRA         073
#define OP_DSLL32       074
#define OP_DSRL32       076
#define OP_DSRA32       077

/*
 * Values for the 'func' field when 'op' == OP_REGIMM.
 */
#define OP_BLTZ         000
#define OP_BGEZ         001
#define OP_BLTZL        002
#define OP_BGEZL        003

#define OP_TGEI         010
#define OP_TGEIU        011
#define OP_TLTI         012
#define OP_TLTIU        013
#define OP_TEQI         014
#define OP_TNEI         016

#define OP_BLTZAL       020
#define OP_BGEZAL       021
#define OP_BLTZALL      022
#define OP_BGEZALL      023

/*
 * Values for the 'rs' field when 'op' == OP_COPz.
 */
#define OP_MF           000
#define OP_DMF          001
#define OP_CF           002
#define OP_MT           004
#define OP_DMT          005
#define OP_CT           006
#define OP_BC           010

/*
 * Values for the 'rt' field when 'op' == OP_COPz and 'rt' == OP_BC.
 */
#define COPz_BCF        0x00
#define COPz_BCT        0x01
#define COPz_BCFL       0x02
#define COPz_BCTL       0x03

/*
 * Instructions with specal significance to debuggers.
 */
#define BREAK_INSTR     0x0005000d      /* instruction code for break */
#define NOP_INSTR       0x00000000      /* instruction code for no-op */

#endif  /* _MIPS_OPCODE_H */
