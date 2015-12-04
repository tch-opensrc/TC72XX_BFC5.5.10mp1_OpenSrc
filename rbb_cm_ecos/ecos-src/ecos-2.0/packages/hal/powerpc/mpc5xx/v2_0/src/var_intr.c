//==========================================================================
//
//      var_intr.c
//
//      PowerPC variant interrupt handlers
//
//==========================================================================
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
//==========================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):    Bob Koninckx
// Contributors: Bob Koninckx
// Date:         2001-12-16
// Purpose:      PowerPC variant interrupt handlers
// Description:  This file contains code to handle interrupt related issues
//               on the PowerPC variant.
//
//####DESCRIPTIONEND####
//
//==========================================================================

#include <pkgconf/hal.h>
#include <cyg/hal/ppc_regs.h>
#include <cyg/hal/hal_arbiter.h>

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Since the interrupt sources do not have fixed vectors on the 5XX
// SIU, some arbitration is required.

// More than one interrupt source can be programmed to use the same
// vector, so all sources on the same vector have to be queried to
// find the one raising the interrupt. This functionality has not been
// implemented, but the arbiter functions for each of the SIU
// interrupt sources can be called in sequence without change.



// Timebase interrupt can be caused by match on either reference A
// or B.  
// Note: If only one interrupt source is assigned per vector, and only
// reference interrupt A or B is used, this ISR is not
// necessary. Attach the timerbase reference A or B ISR directly to
// the LVLx vector instead.
externC cyg_uint32
hal_arbitration_isr_tb (CYG_ADDRWORD vector, CYG_ADDRWORD data)
{
    cyg_uint32 isr_ret;
    cyg_uint16 tbscr;

    HAL_READ_UINT16 (CYGARC_REG_IMM_TBSCR, tbscr);
    if (tbscr & CYGARC_REG_IMM_TBSCR_REFA) {
        isr_ret = hal_call_isr (CYGNUM_HAL_INTERRUPT_SIU_TB_A);
#ifdef CYGIMP_HAL_COMMON_INTERRUPTS_CHAIN
        if (isr_ret & CYG_ISR_HANDLED)
#endif
            return isr_ret;
    }

    if (tbscr & CYGARC_REG_IMM_TBSCR_REFB) {
        isr_ret = hal_call_isr (CYGNUM_HAL_INTERRUPT_SIU_TB_B);
#ifdef CYGIMP_HAL_COMMON_INTERRUPTS_CHAIN
        if (isr_ret & CYG_ISR_HANDLED)
#endif
            return isr_ret;
    }

    return 0;
}

// Periodic interrupt.
// Note: If only one interrupt source is assigned per vector, this ISR
// is not necessary. Attach the periodic interrupt ISR directly to the
// LVLx vector instead.
externC cyg_uint32
hal_arbitration_isr_pit (CYG_ADDRWORD vector, CYG_ADDRWORD data)
{
    cyg_uint32 isr_ret;
    cyg_uint16 piscr;

    HAL_READ_UINT16 (CYGARC_REG_IMM_PISCR, piscr);
    if (piscr & CYGARC_REG_IMM_PISCR_PS) {
        isr_ret = hal_call_isr (CYGNUM_HAL_INTERRUPT_SIU_PIT);
#ifdef CYGIMP_HAL_COMMON_INTERRUPTS_CHAIN
        if (isr_ret & CYG_ISR_HANDLED)
#endif
            return isr_ret;
    }

    return 0;
}

// Real time clock interrupts can be caused by the alarm or
// once-per-second.
// Note: If only one interrupt source is assigned per vector, and only
// the alarm or once-per-second interrupt is used, this ISR is not
// necessary. Attach the alarm or once-per-second ISR directly to the
// LVLx vector instead.
externC cyg_uint32
hal_arbitration_isr_rtc (CYG_ADDRWORD vector, CYG_ADDRWORD data)
{
    cyg_uint32 isr_ret;
    cyg_uint16 rtcsc;

    HAL_READ_UINT16 (CYGARC_REG_IMM_RTCSC, rtcsc);
    if (rtcsc & CYGARC_REG_IMM_RTCSC_SEC) {
        isr_ret = hal_call_isr (CYGNUM_HAL_INTERRUPT_SIU_RTC_SEC);
#ifdef CYGIMP_HAL_COMMON_INTERRUPTS_CHAIN
        if (isr_ret & CYG_ISR_HANDLED)
#endif
            return isr_ret;
    }

    if (rtcsc & CYGARC_REG_IMM_RTCSC_ALR) {
        isr_ret = hal_call_isr (CYGNUM_HAL_INTERRUPT_SIU_RTC_ALR);
#ifdef CYGIMP_HAL_COMMON_INTERRUPTS_CHAIN
        if (isr_ret & CYG_ISR_HANDLED)
#endif
            return isr_ret;
    }

    return 0;
}

// -------------------------------------------------------------------------
// IMB3 interrupt decoding
//
// All interrupt priorities higher than 7 are mapped to SIU level 7. As much
// as 15 interrupting devices can be behind this. If more than one IMB3 
// device is to be used with priorites in the range 7-31, a special kind of
// arbitration isr needs to be set up on SIU level 7. As this is not allways
// necessary, it is provided as a configuration option.
#ifdef CYGSEM_HAL_POWERPC_MPC5XX_IMB3_ARBITER
static hal_mpc5xx_arbitration_data * imb3_data_head = 0;

static cyg_uint32
hal_arbitration_imb3(CYG_ADDRWORD vector, CYG_ADDRWORD data)
{
  hal_mpc5xx_arbitration_data * p = 
    *(hal_mpc5xx_arbitration_data **)data;

  // Try them all, highest priorities come first. An ISR should return
  // CYG_ISR_HANDLED or CYG_ISR_CALL_DSR. An arbitration ISR will 
  // strip the CYG_DSR_HANDLED from the ISR result, or returns 0 if
  // no ISR could be called. This means that CYG_ISR_HANDLED implies
  // that an ISR was called, 0 means that nothing was called.
  // Notice that our approach tries to be efficient. We return as soon
  // as the first interrupting source is found. This prevents from scanning 
  // the complete table for every interrupt. If more than one module 
  // requested at the same time, we will re-enter this procedure immediately
  // anyway.
  while(p)
  {
    if((p->arbiter(CYGNUM_HAL_INTERRUPT_SIU_LVL7, p->data))&CYG_ISR_HANDLED)
      break;
    else
      p = (hal_mpc5xx_arbitration_data *)(p->reserved);
  }

  return 0;
}

static hal_mpc5xx_arbitration_data *
mpc5xx_insert(hal_mpc5xx_arbitration_data * list,
              hal_mpc5xx_arbitration_data * data)
{
  hal_mpc5xx_arbitration_data    tmp;
  hal_mpc5xx_arbitration_data * ptmp = &tmp;
  tmp.reserved = list;

  while(ptmp->reserved)
  {
    if(((hal_mpc5xx_arbitration_data *)(ptmp->reserved))->priority > data->priority)
      break;
    ptmp = (hal_mpc5xx_arbitration_data *)(ptmp->reserved);
  }

  data->reserved = ptmp->reserved;
  ptmp->reserved = data;
  
  return (hal_mpc5xx_arbitration_data *)(tmp.reserved);
}

static hal_mpc5xx_arbitration_data *
mpc5xx_remove(hal_mpc5xx_arbitration_data * list,
              hal_mpc5xx_arbitration_data * data)
{
  hal_mpc5xx_arbitration_data    tmp;
  hal_mpc5xx_arbitration_data * ptmp = &tmp;
  tmp.reserved = list;

  while(ptmp->reserved)
  {
    if(ptmp->reserved == data)
      break;
      
    ptmp = (hal_mpc5xx_arbitration_data *)(ptmp->reserved);
  }

  if(ptmp->reserved)
    ptmp->reserved = ((hal_mpc5xx_arbitration_data *)(ptmp->reserved))->reserved;

  return (hal_mpc5xx_arbitration_data *)(tmp.reserved);
}
#endif

externC void 
hal_mpc5xx_install_arbitration_isr(hal_mpc5xx_arbitration_data * adata)
{
  CYG_ADDRWORD vector = 2*(1 + adata->priority);
  if(vector < CYGNUM_HAL_INTERRUPT_SIU_LVL7)
  {
    HAL_INTERRUPT_ATTACH(vector, adata->arbiter, adata->data, 0);
    HAL_INTERRUPT_UNMASK(vector);
  }
  else
  {
#ifdef CYGSEM_HAL_POWERPC_MPC5XX_IMB3_ARBITER  
    // Prevent anything from coming through while manipulating
    // the list
    HAL_INTERRUPT_MASK(CYGNUM_HAL_INTERRUPT_SIU_LVL7);
    imb3_data_head = mpc5xx_insert(imb3_data_head, adata);
    HAL_INTERRUPT_UNMASK(CYGNUM_HAL_INTERRUPT_SIU_LVL7);
#else
    HAL_INTERRUPT_ATTACH(CYGNUM_HAL_INTERRUPT_SIU_LVL7, adata->arbiter, adata->data, 0);
    HAL_INTERRUPT_UNMASK(CYGNUM_HAL_INTERRUPT_SIU_LVL7);
#endif
  }
}

externC void
hal_mpc5xx_remove_arbitration_isr(hal_mpc5xx_arbitration_data * adata)
{
#ifdef CYGSEM_HAL_POWERPC_MPC5XX_IMB3_ARBITER  
  // Prevent anything from coming through while manipulating the list
  HAL_INTERRUPT_MASK(CYGNUM_HAL_INTERRUPT_SIU_LVL7);
  imb3_data_head = mpc5xx_remove(imb3_data_head, adata);
  HAL_INTERRUPT_UNMASK(CYGNUM_HAL_INTERRUPT_SIU_LVL7);
#endif
}

// -------------------------------------------------------------------------
// Variant specific interrupt setup
externC void
hal_variant_IRQ_init(void)
{
#ifdef CYGSEM_HAL_POWERPC_MPC5XX_IMB3_ARBITER  
  HAL_INTERRUPT_ATTACH(CYGNUM_HAL_INTERRUPT_SIU_LVL7, hal_arbitration_imb3, &imb3_data_head, 0);
  HAL_INTERRUPT_UNMASK(CYGNUM_HAL_INTERRUPT_SIU_LVL7);
#endif
}

// -------------------------------------------------------------------------
// EOF var_intr.c
