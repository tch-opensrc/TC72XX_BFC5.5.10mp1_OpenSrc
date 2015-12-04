//========================================================================
//
//      h8300_stub.c
//
//      Helper functions for H8/300H stub
//
//========================================================================
//####ECOSGPLCOPYRIGHTBEGIN####
// -------------------------------------------
// This file is part of eCos, the Embedded Configurable Operating System.
// Copyright (C) 1998, 1999, 2000, 2001, 2002 Red Hat, Inc.
//
// eCos is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 or (at your option) any later version.
//
// eCos is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
// for more details.
//
// You should have received a copy of the GNU General Public License along
// with eCos; if not, write to the Free Software Foundation, Inc.,
// 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
//
// As a special exception, if other files instantiate templates or use macros
// or inline functions from this file, or you compile this file and link it
// with other works to produce a work based on this file, this file does not
// by itself cause the resulting work to be covered by the GNU General Public
// License. However the source code for this file must still be made available
// in accordance with section (3) of the GNU General Public License.
//
// This exception does not invalidate any other reasons why a work based on
// this file might be covered by the GNU General Public License.
//
// Alternative licenses for eCos may be arranged by contacting Red Hat, Inc.
// at http://sources.redhat.com/ecos/ecos-license/
// -------------------------------------------
//####ECOSGPLCOPYRIGHTEND####
//========================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):     Red Hat, jskov
// Contributors:  Red Hat, jskov
// Date:          1998-11-06
// Purpose:       
// Description:   Helper functions for H8/300H stub
// Usage:         
//
//####DESCRIPTIONEND####
//
//========================================================================

#include <stddef.h>

#include <pkgconf/hal.h>

#ifdef CYGDBG_HAL_DEBUG_GDB_INCLUDE_STUBS

#include <cyg/hal/hal_stub.h>
#include <cyg/hal/hal_arch.h>
#include <cyg/hal/hal_intr.h>

#ifdef CYGDBG_HAL_DEBUG_GDB_THREAD_SUPPORT
#include <cyg/hal/dbg-threads-api.h>    // dbg_currthread_id
#endif

/*----------------------------------------------------------------------
 * Asynchronous interrupt support
 */

typedef unsigned short t_inst;

static struct
{
  t_inst *targetAddr;
  t_inst savedInstr;
} asyncBuffer;

/* Called to asynchronously interrupt a running program.
   Must be passed address of instruction interrupted.
   This is typically called in response to a debug port
   receive interrupt.
*/

void
install_async_breakpoint(void *pc)
{
  asyncBuffer.targetAddr = pc;
  asyncBuffer.savedInstr = *(t_inst *)pc;
  *(t_inst *)pc = (t_inst)HAL_BREAKINST;
  __instruction_cache(CACHE_FLUSH);
  __data_cache(CACHE_FLUSH);
}

/*--------------------------------------------------------------------*/
/* Given a trap value TRAP, return the corresponding signal. */

int __computeSignal (unsigned int trap_number)
{
    switch (trap_number) {
    case 11:
        return SIGTRAP;
    default:
        return SIGINT;
    }
}

/*--------------------------------------------------------------------*/
/* Return the trap number corresponding to the last-taken trap. */

int __get_trap_number (void)
{
    extern int CYG_LABEL_NAME(_intvector);
    // The vector is not not part of the GDB register set so get it
    // directly from the save context.
    return CYG_LABEL_NAME(_intvector);
}

/*--------------------------------------------------------------------*/
/* Set the currently-saved pc register value to PC. This also updates NPC
   as needed. */

void set_pc (target_register_t pc)
{
    put_register (PC, pc);
}


/*----------------------------------------------------------------------
 * Single-step support. Lifted from CygMon.
 */

#define NUM_SS_BPTS 2
static target_register_t break_mem [NUM_SS_BPTS] = {0, 0};
static unsigned char break_mem_data [NUM_SS_BPTS];

/* Set a single-step breakpoint at ADDR.  Up to two such breakpoints
   can be set; WHICH specifies which one to set (0 or 1).  */

static void
set_single_bp (int which, unsigned char *addr)
{
    if (break_mem[which] == 0) {
        break_mem[which] = (target_register_t) addr;
        break_mem_data[which] = *(unsigned short *)addr;
        *(unsigned short *)addr = HAL_BREAKINST;
    }
}

/* Clear any single-step breakpoint(s) that may have been set.  */

void __clear_single_step (void)
{
  int x;
  for (x = 0; x < NUM_SS_BPTS; x++)
    {
        unsigned char* addr = (unsigned char*) break_mem[x];
        if (addr) {
            *addr = break_mem_data[x];
            break_mem[x] = 0;
        }
    }
}

/* Set breakpoint(s) to simulate a single step from the current PC.  */

const static unsigned char opcode_length0[]={
  0x04,0x02,0x04,0x02,0x04,0x02,0x04,0x02,  /* 0x58 */
  0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,  /* 0x60 */
  0x02,0x02,0x11,0x11,0x02,0x02,0x04,0x04,  /* 0x68 */
  0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,  /* 0x70 */
  0x08,0x04,0x06,0x04,0x04,0x04,0x04,0x04   /* 0x78 */
};

const static unsigned char opcode_length1[]={
  0x10,0x00,0x00,0x00,0x11,0x00,0x00,0x00,
  0x02,0x00,0x00,0x00,0x04,0x04,0x00,0x04
};

static int insn_length(unsigned char *pc)
{
  if (*pc != 0x01 && (*pc < 0x58 || *pc>=0x80)) 
    return 2;
  else
    switch (*pc) {
      case 0x01:
	switch (*(pc+1) & 0xf0) {
	case 0x00:
	  if (*(pc+2)== 0x78) {
	    return 10;
	  } else if (*(pc+2)== 0x6b) {
	    return (*(pc+3) & 0x20)?8:6;
	  } else {
	    return (*(pc+2) & 0x02)?6:4;
	  }
	case 0x40:
	  return (*(pc+2) & 0x02)?8:6;
	default:
	  return opcode_length1[*(pc+1)>>4];
	}
      case 0x6a:
      case 0x6b:
	return (*(pc+1) & 0x20)?6:4;
      default:
	return opcode_length0[*pc-0x58];
    }
}

void __single_step (void)
{
  unsigned int pc = get_register (PC);
  unsigned int next;
  unsigned int opcode;

  opcode = *(unsigned short *)pc;
  next = pc + insn_length((unsigned char *)pc);
  if (opcode == 0x5470) {
    /* rts */ 
    unsigned long *sp;
    sp = (unsigned long *)get_register(SP);
    next = *sp & 0x00ffffff;
  } else if ((opcode & 0xfb00) != 0x5800) {
    /* jmp / jsr */
    int regs;
    const short reg_tbl[]={ER0,ER1,ER2,ER3,ER4,ER5,ER6,SP};
    switch(opcode & 0xfb00) {
    case 0x5900:
      regs = (opcode & 0x0070) >> 8;
      next = get_register(reg_tbl[regs]);
      break;
    case 0x5a00:
      next = *(unsigned long *)(pc+2) & 0x00ffffff;
      break;
    case 0x5b00:
      next = *(unsigned long *)(opcode & 0xff);
      break;
    }
  } else if (((opcode & 0xf000) == 0x4000) || ((opcode & 0xff00) == 0x5500)) { 
    /* b**:8 */
    unsigned long dsp;
    dsp = (long)(opcode && 0xff)+pc+2;
    set_single_bp(1,(unsigned char *)dsp);
  } else if (((opcode & 0xff00) == 0x5800) || ((opcode & 0xff00) == 0x5c00)) { 
    /* b**:16 */
    unsigned long dsp;
    dsp = *(unsigned short *)(pc+2)+pc+4;
    set_single_bp(1,(unsigned char *)dsp);
  }
  set_single_bp(0,(unsigned char *)next);
}

void __install_breakpoints (void)
{
    /* NOP since single-step HW exceptions are used instead of
       breakpoints. */
}

void __clear_breakpoints (void)
{

}


/* If the breakpoint we hit is in the breakpoint() instruction, return a
   non-zero value. */

externC void CYG_LABEL_NAME(breakinst)(void);
int
__is_breakpoint_function ()
{
    return get_register (PC) == (target_register_t)&CYG_LABEL_NAME(breakinst);
}


/* Skip the current instruction. */

void __skipinst (void)
{
    unsigned long pc = get_register (PC);

    pc+=insn_length((unsigned char *)pc);
    put_register (PC, (target_register_t) pc);
}

#endif // CYGDBG_HAL_DEBUG_GDB_INCLUDE_STUBS
