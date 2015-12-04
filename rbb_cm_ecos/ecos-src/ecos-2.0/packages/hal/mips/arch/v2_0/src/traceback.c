//****************************************************************************
//
// Copyright (c) 2003-2010 Broadcom Corporation
//
// This file being released as part of eCos, the Embedded Configurable Operating System.
//
// eCos is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License version 2, available at 
// http://www.broadcom.com/licenses/GPLv2.php (the "GPL").
//
// As a special exception, if you compile this file and link it
// with other works to produce a work based on this file, this file does not
// by itself cause the resulting work to be covered by the GNU General Public
// License. However the source code for this file must still be made available
// in accordance with section (3) of the GNU General Public License.
//
//****************************************************************************

//  $Id$
//
//  Filename:       traceback.c
//  Author:         Mike Sieweke
//  Creation Date:  February 13, 2004
//
//****************************************************************************
//  Description:
//      Here we implement a call trace function for the MIPS.  It's pretty
//      basic, and there are cases where a function may be untraceable.
//
//****************************************************************************

//********************** Include Files ***************************************

#include <cyg/infra/cyg_type.h>
#include <cyg/infra/diag.h>

cyg_uint32 CalculateRamSize( void );

//********************** Local Constants *************************************

#define REG_RA      (31)
#define REG_SP      (29)
#define RSSHIFT     (21)
#define RTSHIFT     (16)
#define RDSHIFT     (11)
#define RSMASK      (0x1f<<RSSHIFT)
#define RTMASK      (0x1f<<RTSHIFT)
#define RDMASK      (0x1f<<RDSHIFT)
#define BMASK       (0xfc1f0000) // ((0x3f<<26) + (0x1f<<16))
#define BGEZAL      (0x04110000)
#define BGEZALL     (0x04130000)
#define BLTZAL      (0x04100000)
#define BLTZALL     (0x04120000)
#define JALMASK     (0xfc000000)
#define JAL         (0x0c000000)
#define JALRMASK    (0xfc1f003f)
#define JALR        (0x00000009)
#define ADDIUSPMASK (0xffff0000)
#define ADDIUSP     (0x27bd0000)
#define ADDISP      (0x23bd0000)
#define SWSPMASK    (0xffe00000)
#define SWSP        (0xafa00000)

extern cyg_uint32 _etext;

//********************** Local Functions *************************************

// GetfuncEntryFromRa() tries to find the current function's entry point by 
// looking at the calling instruction, which should be 2 instructions before 
// the return address.  If
static cyg_uint32 GetfuncEntryFromRa( cyg_uint32 ra )
{
    cyg_uint32  endText          = (cyg_uint32) &_etext;
    cyg_uint32  segment          = endText & 0xf0000000;
    cyg_uint32  functionEntry    = segment;

    if (( ra > segment ) &&
        ( ra < endText ))
    {
        // The calling instruction is 2 instructions (8 bytes) before the
        // return address.  We can use this instruction to find where the
        // current function starts.
        cyg_uint32 *callInstrPtr  = (cyg_uint32 *)( ra - 8 );
        cyg_uint32  callInstr     = *callInstrPtr;
        cyg_uint32  branchInstr   = callInstr & BMASK;
        cyg_uint32  jalInstr      = callInstr & JALMASK;
//        cyg_uint32  jalrInstr     = callInstr & JALRMASK;
        cyg_int32   instrOffset;

        // We hope the calling instruction is either "bal" or "jal", because
        // that gives us an absolute address.  If it's "jalr", it's most likely
        // a virtual function call, so we can't find the function entry.
        if (( branchInstr == BGEZAL  )  ||
            ( branchInstr == BGEZALL )  ||
            ( branchInstr == BLTZAL  )  ||
            ( branchInstr == BLTZALL ))
        {
            instrOffset = callInstr & 0xffff;
            // If the offset is a negative number, sign extend it.
            if (( instrOffset & 0x8000 ) != 0 )
            {
                instrOffset = (cyg_int32) ( (cyg_uint32) instrOffset | 0xffff0000 );
            }
            // The offset is a word (4 byte) offset.
            instrOffset = instrOffset * 4;
            functionEntry = ra + instrOffset;
        }
        else if ( jalInstr == JAL )
        {
            // The offset is a word (4 byte) offset.
            instrOffset = (callInstr & 0x03ffffff) * 4;
            functionEntry = (ra & 0xf0000000) | instrOffset;
        }
        else
        {
            diag_printf( "Current function called through jalr.  Entry not known.\n" );
        }
    }

    return functionEntry;
}


// Test whether the current instruction is a stack decrement (ADDI or ADDIU sp,#).
static bool IsStackDecrement( cyg_uint32 currentInstruction )
{
    if ((( currentInstruction & ADDIUSPMASK ) == ADDIUSP ) ||
        (( currentInstruction & ADDIUSPMASK ) == ADDISP ))
    {
        return true;
    }
    else
    {
        return false;
    }
}


// Test whether the current instruction is a stack store (SW reg,offset(sp))
// and "reg" is the return address register.
static bool IsStoreRa( cyg_uint32 currentInstruction )
{
    if ((( currentInstruction & SWSPMASK ) == SWSP ) &&
        (( currentInstruction & RTMASK ) == ( REG_RA << RTSHIFT )))
        {
            return true;
        }
        else
        {
            return false;
        }
}


// This is our exported interface for generating a stack trace.  We use the
// current PC, stack pointer, and return address register to look through the
// code and stack to glean some useful info.  When the compiler is in "full
// optimize" mode, the frame pointer isn't used, so we have to look for stack
// manipulation instructions.
void MipsStackTrace( cyg_uint32 pc, cyg_uint32 sp, cyg_uint32 ra )
{
    cyg_uint32  currentRa        = ra;
    cyg_uint32  currentPc        = pc;
    cyg_uint32  functionEntry;
    cyg_uint32  currentStack     = sp;
    cyg_uint32  endText          = (cyg_uint32) &_etext;
    int         i;
    cyg_uint32  memSize          = CalculateRamSize();

    // Try to find our entry point from the call instruction at the return
    // address.  If the current function called another function, the call
    // won't be for our function.  But that doesn't cause a problem.
    // After this point we know every function in the chain has called another
    // function, so we know the return address will be on the stack.  Only
    // the top (current) function is in question.
    functionEntry = GetfuncEntryFromRa( ra );
//    diag_printf( "Computed function entry is %08x\n", functionEntry );
    if ( functionEntry > currentPc )
    {
        // Place the entry point at the start of the current segment.
        // We won't back up farther than this.
        functionEntry = currentPc & 0xf0000000;
    }

    // Limit the number of function calls.  This protects us from an error
    // which could put us into an infinite loop.
    for ( i = 0; i < 32; i++ )
    {
        cyg_uint32  currentInstruction;
        cyg_int16   stackSize               = 0;
        cyg_int16   stackOffsetToRa         = -1;

        // We have an idea where the current function may start, so start
        // looking backward from the PC to find stack size and where the RA
        // register was stored on the stack.
        while ( currentPc >= functionEntry )
        {
            // Look at the instruction to see if it's a stack decrement.
            currentInstruction = *(cyg_uint32*)currentPc;
            if ( IsStackDecrement( currentInstruction ) )
            {
                // We only really care about stack decrement (add a negative
                // number).  If we find one, we're at the beginning of the
                // function.  Ignore a stack increment, because our starting
                // point could be after a function return.
                // We're assuming there won't be multiple stack decrement
                // opcodes in a single function.
                stackSize = currentInstruction & 0xffff;
                if ( stackSize < 0 )
                {
                    stackSize = - stackSize;
                    break;
                }
            }
            // If the return address was stored on the stack, remember where.
            // We'll pull the value off later.
            else if ( IsStoreRa( currentInstruction ) )
            {
                stackOffsetToRa = ( currentInstruction & 0xffff );
            }
            currentPc -= 4;
        }
        // It's possible for us to back up past the entry point if the current
        // function doesn't use the stack (no stack decrement).
        if ( currentPc < functionEntry )
        {
            currentPc = functionEntry;
        }

        // We've reached what we think is a function entry point.
        diag_printf( "entry %08x  ", currentPc );

        // The stack should be in the same segment as the PC, and less than
        // memSize from the base of the segment.
        if ((( currentStack & 0x0fffffff ) > memSize ) ||
            (( currentStack & 0xf0000000 ) != ( currentPc & 0xf0000000 )))
        {
            diag_printf( "Stack pointer invalid.  Trace stops.\n" );
            break;
        }
        // If we found where the return address was stored on the stack, use it.
        if ( stackOffsetToRa != -1 )
        {
            currentRa = *((cyg_uint32*)( currentStack + stackOffsetToRa ));
        }

        // Make sure the return address is within the code.
        if (( currentRa > endText ) ||
            (( currentRa & 0xf0000000 ) != ( currentPc & 0xf0000000 )))
        {
            diag_printf( "Return address (%08x) invalid or not found.  Trace stops.\n", currentRa );
            break;
        }
        diag_printf( "  called from %08x\n", currentRa - 8 );
        if ( stackSize > 0 )
        {
            currentStack += stackSize;
        }
        
        functionEntry = currentPc & 0xf0000000;
        currentPc     = currentRa;
        currentRa     = 0;
    }
}

