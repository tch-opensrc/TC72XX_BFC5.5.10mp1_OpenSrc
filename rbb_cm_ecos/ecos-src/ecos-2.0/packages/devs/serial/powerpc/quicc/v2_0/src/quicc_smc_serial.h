#ifndef CYGONCE_POWERPC_QUICC_SMC_SERIAL_H
#define CYGONCE_POWERPC_QUICC_SMC_SERIAL_H

// ====================================================================
//
//      quicc_smc_serial.h
//
//      Device I/O - Description of PowerPC QUICC/SMC serial hardware
//
// ====================================================================
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
// ====================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):           gthomas
// Contributors:        gthomas
// Date:                1999-06-21
// Purpose:     Internal interfaces for serial I/O drivers
// Description:
//
//####DESCRIPTIONEND####
//
// ====================================================================

// Description of serial ports using QUICC/SMC

#include <cyg/hal/quicc/ppc8xx.h>                  // QUICC structure definitions

static unsigned int select_word_length[] = {
    QUICC_SMCMR_CLEN(5),  // 5 bits / word (char)
    QUICC_SMCMR_CLEN(6),
    QUICC_SMCMR_CLEN(7),
    QUICC_SMCMR_CLEN(8)
};

static unsigned int select_stop_bits[] = {
    0, 
    QUICC_SMCMR_SB(1),   // 1 stop bit
    QUICC_SMCMR_SB(1),   // 1.5 stop bit
    QUICC_SMCMR_SB(2)    // 2 stop bits
};

static unsigned int select_parity[] = {
    QUICC_SMCMR_PE(0),                     // No parity
    QUICC_SMCMR_PE(1)|QUICC_SMCMR_PM(1),   // Even parity
    QUICC_SMCMR_PE(1)|QUICC_SMCMR_PM(0),   // Odd parity
    0,                                     // Mark parity
    0,                                     // Space parity
};

// Baud rate values, based on board clock

static cyg_int32 select_baud[] = {
    0,      // Unused
    50,     // 50
    75,     // 75
    110,    // 110
    0,      // 134.5
    150,    // 150
    200,    // 200
    300,    // 300
    600,    // 600
    1200,   // 1200
    1800,   // 1800
    2400,   // 2400
    3600,   // 3600
    4800,   // 4800
    7200,   // 7200
    9600,   // 9600
    14400,  // 14400
    19200,  // 19200
    38400,  // 38400
    57600,  // 57600
    115200, // 115200
    0,      // 230400
};

#define UART_BITRATE(n) (((CYGHWR_HAL_POWERPC_BOARD_SPEED*1000000)/16)/n)
#define UART_SLOW_BITRATE(n) ((CYGHWR_HAL_POWERPC_BOARD_SPEED*1000000)/n))

#endif // CYGONCE_POWERPC_QUICC_SMC_SERIAL_H
