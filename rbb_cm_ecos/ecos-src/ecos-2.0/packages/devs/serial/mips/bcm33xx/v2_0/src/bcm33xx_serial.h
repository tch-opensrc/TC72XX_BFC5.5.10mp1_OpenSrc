//==========================================================================
//
//      io/serial/mips/bcm33xx/bcm33xx_serial.h
//
//      Broadcom 33xx Serial I/O definitions.
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
// Author(s):    msieweke, based on MIPS IDT 16550 driver by tmichals, based on driver by dmoseley, based on POWERPC driver by jskov
// Contributors: gthomas, jskov, dmoseley, tmichals
// Date:         2003-07-02
// Purpose:      Broadcom 33xx reference platform serial device driver definitions.
// Description:  Broadcom 33xx serial device driver definitions.
//####DESCRIPTIONEND####
//==========================================================================
//==========================================================================
// Portions of this software Copyright (c) 2003-2010 Broadcom Corporation
//==========================================================================

#include <cyg/hal/bcm33xx_regs.h>

// Description of serial ports on Broadcom 33xx board

static unsigned int select_baud[] = {
    0,    // Unused
    50,
    75,
    110,
    134,
    150,
    200,
    300,
    600,
    1200,
    1800,
    2400,
    3600,
    4800,
    7200,
    9600,
    14400,
    19200,
    38400,
    57600,
    115200,
    230400
};

static unsigned char select_word_length[] = {
    0, // 0
    0, // 1
    0, // 2
    0, // 3
    0, // 4
    BITS5SYM,
    BITS6SYM,
    BITS7SYM,
    BITS8SYM
};

static unsigned char select_stop_bits[] = {
    0,
    ONESTOP,
    ONEPTFIVESTOP,
    TWOSTOP
};

static unsigned char select_parity[] = {
    0,     // No parity
    TXPARITYEN | TXPARITYEVEN | RXPARITYEN | RXPARITYEVEN,     // Even parity
    TXPARITYEN | RXPARITYEN,     // Odd parity
    0,     // Mark parity not supported
    0,     // Space parity not supported
};


// For Broadcom reference designs, the peripheral clock should be 50 MHz,
// which makes the baudword for 115.2kps = 13.  And we only use 115.2kbps
#define BAUD_115200 13

// EOF bcm33xx_serial.h
