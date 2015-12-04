//==========================================================================
//
//      plf_clock.c
//
//      HAL platform peripheral clock definition.
//
//==========================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):    msieweke
// Contributors: 
// Date:         2003-09-12
// Purpose:      define default timer/UART clock frequency
// Description:  The value PeripheralClockFrequency is used to set the
//               baud rate generator in the UART, so it must be defined
//               at init time.  This value is placed in a separate module
//               in the library so it won't be linked in if the application
//               overrides this definition.
//
//####DESCRIPTIONEND####

// ====================================================================
// Portions of this software Copyright (c) 2003-2010 Broadcom Corporation
// ====================================================================
//========================================================================*/

#include <cyg/infra/cyg_type.h>         // Base types

// Most Broadcom reference designs run the peripheral clock at 50 MHz.
cyg_uint32 PeripheralClockFrequency = 50000000;

