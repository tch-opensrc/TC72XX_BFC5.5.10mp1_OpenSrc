//==========================================================================
//
//      io/serial/powerpc/quicc_smc_serial.c
//
//      PowerPC QUICC (SMC) Serial I/O Interface Module (interrupt driven)
//
//==========================================================================
//####ECOSGPLCOPYRIGHTBEGIN####
// -------------------------------------------
// This file is part of eCos, the Embedded Configurable Operating System.
// Copyright (C) 1998, 1999, 2000, 2001, 2002 Red Hat, Inc.
// Copyright (C) 2003 Gary Thomas
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
// Author(s):    gthomas
// Contributors: gthomas
// Date:         1999-06-20
// Purpose:      QUICC SMC Serial I/O module (interrupt driven version)
// Description: 
//
//####DESCRIPTIONEND####
//
//==========================================================================

#include <pkgconf/system.h>
#include <pkgconf/io_serial.h>
#include <pkgconf/io.h>
#include <cyg/io/io.h>
#include <cyg/hal/hal_intr.h>
#include <cyg/io/devtab.h>
#include <cyg/io/serial.h>
#include <cyg/infra/diag.h>
#include <cyg/hal/hal_cache.h>
#include <cyg/hal/quicc/ppc8xx.h>
#include CYGBLD_HAL_PLATFORM_H

#ifdef CYGPKG_IO_SERIAL_POWERPC_QUICC_SMC

// macro for aligning buffers to cache lines
#define ALIGN_TO_CACHELINES(b) ((cyg_uint8 *)(((CYG_ADDRESS)(b) + (HAL_DCACHE_LINE_SIZE-1)) & ~(HAL_DCACHE_LINE_SIZE-1)))

// Buffer descriptor control bits
#define QUICC_BD_CTL_Ready 0x8000  // Buffer contains data (tx) or is empty (rx)
#define QUICC_BD_CTL_Wrap  0x2000  // Last buffer in list
#define QUICC_BD_CTL_Int   0x1000  // Generate interrupt when empty (tx) or full (rx)
#define QUICC_BD_CTL_MASK  0xB000  // User settable bits

// SMC Mode Register
#define QUICC_SMCMR_CLEN(n)   ((n+1)<<11)   // Character length
#define QUICC_SMCMR_SB(n)     ((n-1)<<10)   // Stop bits (1 or 2)
#define QUICC_SMCMR_PE(n)     (n<<9)        // Parity enable (0=disable, 1=enable)
#define QUICC_SMCMR_PM(n)     (n<<8)        // Parity mode (0=odd, 1=even)
#define QUICC_SMCMR_UART      (2<<4)        // UART mode
#define QUICC_SMCMR_TEN       (1<<1)        // Enable transmitter
#define QUICC_SMCMR_REN       (1<<0)        // Enable receiver

// SMC Events (interrupts)
#define QUICC_SMCE_BRK 0x10  // Break received
#define QUICC_SMCE_BSY 0x04  // Busy - receive buffer overrun
#define QUICC_SMCE_TX  0x02  // Tx interrupt
#define QUICC_SMCE_RX  0x01  // Rx interrupt

// SMC Commands
#define QUICC_SMC_CMD_InitTxRx  (0<<8)
#define QUICC_SMC_CMD_InitTx    (1<<8)
#define QUICC_SMC_CMD_InitRx    (2<<8)
#define QUICC_SMC_CMD_StopTx    (4<<8)
#define QUICC_SMC_CMD_RestartTx (6<<8)
#define QUICC_SMC_CMD_Reset     0x8000
#define QUICC_SMC_CMD_Go        0x0001

#include "quicc_smc_serial.h"

typedef struct quicc_smc_serial_info {
    CYG_ADDRWORD          channel;                   // Which channel SMC1/SMC2
    CYG_WORD              int_num;                   // Interrupt number
    cyg_uint32            *brg;                      // Which baud rate generator
    volatile struct smc_uart_pram  *pram;            // Parameter RAM pointer
    volatile struct smc_regs       *ctl;             // SMC control registers
    volatile struct cp_bufdesc     *txbd, *rxbd;     // Next Tx,Rx descriptor to use
    struct cp_bufdesc     *tbase, *rbase;            // First Tx,Rx descriptor
    int                   txsize, rxsize;            // Length of individual buffers
    cyg_interrupt         serial_interrupt;
    cyg_handle_t          serial_interrupt_handle;
} quicc_smc_serial_info;

static bool quicc_smc_serial_init(struct cyg_devtab_entry *tab);
static bool quicc_smc_serial_putc(serial_channel *chan, unsigned char c);
static Cyg_ErrNo quicc_smc_serial_lookup(struct cyg_devtab_entry **tab, 
                                   struct cyg_devtab_entry *sub_tab,
                                   const char *name);
static unsigned char quicc_smc_serial_getc(serial_channel *chan);
static Cyg_ErrNo quicc_smc_serial_set_config(serial_channel *chan,
                                             cyg_uint32 key, const void *xbuf,
                                             cyg_uint32 *len);
static void quicc_smc_serial_start_xmit(serial_channel *chan);
static void quicc_smc_serial_stop_xmit(serial_channel *chan);

static cyg_uint32 quicc_smc_serial_ISR(cyg_vector_t vector, cyg_addrword_t data);
static void       quicc_smc_serial_DSR(cyg_vector_t vector, cyg_ucount32 count, cyg_addrword_t data);

static SERIAL_FUNS(quicc_smc_serial_funs, 
                   quicc_smc_serial_putc, 
                   quicc_smc_serial_getc,
                   quicc_smc_serial_set_config,
                   quicc_smc_serial_start_xmit,
                   quicc_smc_serial_stop_xmit
    );

#ifdef CYGPKG_IO_SERIAL_POWERPC_QUICC_SMC_SMC1
static quicc_smc_serial_info quicc_smc_serial_info1 = {
    0x90,                         // Channel indicator
    CYGNUM_HAL_INTERRUPT_CPM_SMC1 // interrupt
};
#if CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC1_BUFSIZE > 0
static unsigned char quicc_smc_serial_out_buf1[CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC1_BUFSIZE];
static unsigned char quicc_smc_serial_in_buf1[CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC1_BUFSIZE];

static SERIAL_CHANNEL_USING_INTERRUPTS(quicc_smc_serial_channel1,
                                       quicc_smc_serial_funs, 
                                       quicc_smc_serial_info1,
                                       CYG_SERIAL_BAUD_RATE(CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC1_BAUD),
                                       CYG_SERIAL_STOP_DEFAULT,
                                       CYG_SERIAL_PARITY_DEFAULT,
                                       CYG_SERIAL_WORD_LENGTH_DEFAULT,
                                       CYG_SERIAL_FLAGS_DEFAULT,
                                       &quicc_smc_serial_out_buf1[0], sizeof(quicc_smc_serial_out_buf1),
                                       &quicc_smc_serial_in_buf1[0], sizeof(quicc_smc_serial_in_buf1)
    );
#else
static SERIAL_CHANNEL(quicc_smc_serial_channel1,
                      quicc_smc_serial_funs, 
                      quicc_smc_serial_info1,
                      CYG_SERIAL_BAUD_RATE(CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC1_BAUD),
                      CYG_SERIAL_STOP_DEFAULT,
                      CYG_SERIAL_PARITY_DEFAULT,
                      CYG_SERIAL_WORD_LENGTH_DEFAULT,
                      CYG_SERIAL_FLAGS_DEFAULT
    );
#endif

static unsigned char quicc_smc1_txbuf[CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC1_TxNUM][CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC1_TxSIZE + HAL_DCACHE_LINE_SIZE-1];
static unsigned char quicc_smc1_rxbuf[CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC1_RxNUM][CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC1_RxSIZE + HAL_DCACHE_LINE_SIZE-1];

DEVTAB_ENTRY(quicc_smc_serial_io1, 
             CYGDAT_IO_SERIAL_POWERPC_QUICC_SMC_SMC1_NAME,
             0,                     // Does not depend on a lower level interface
             &cyg_io_serial_devio, 
             quicc_smc_serial_init, 
             quicc_smc_serial_lookup,     // Serial driver may need initializing
             &quicc_smc_serial_channel1
    );
#endif //  CYGPKG_IO_SERIAL_POWERPC_QUICC_SMC_SMC1

#ifdef CYGPKG_IO_SERIAL_POWERPC_QUICC_SMC_SMC2
static quicc_smc_serial_info quicc_smc_serial_info2 = {
    0xD0,                             // Channel indicator
    CYGNUM_HAL_INTERRUPT_CPM_SMC2_PIP // interrupt
};
#if CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC2_BUFSIZE > 0
static unsigned char quicc_smc_serial_out_buf2[CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC2_BUFSIZE];
static unsigned char quicc_smc_serial_in_buf2[CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC2_BUFSIZE];

static SERIAL_CHANNEL_USING_INTERRUPTS(quicc_smc_serial_channel2,
                                       quicc_smc_serial_funs, 
                                       quicc_smc_serial_info2,
                                       CYG_SERIAL_BAUD_RATE(CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC2_BAUD),
                                       CYG_SERIAL_STOP_DEFAULT,
                                       CYG_SERIAL_PARITY_DEFAULT,
                                       CYG_SERIAL_WORD_LENGTH_DEFAULT,
                                       CYG_SERIAL_FLAGS_DEFAULT,
                                       &quicc_smc_serial_out_buf2[0], sizeof(quicc_smc_serial_out_buf2),
                                       &quicc_smc_serial_in_buf2[0], sizeof(quicc_smc_serial_in_buf2)
    );
#else
static SERIAL_CHANNEL(quicc_smc_serial_channel2,
                      quicc_smc_serial_funs, 
                      quicc_smc_serial_info2,
                      CYG_SERIAL_BAUD_RATE(CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC2_BAUD),
                      CYG_SERIAL_STOP_DEFAULT,
                      CYG_SERIAL_PARITY_DEFAULT,
                      CYG_SERIAL_WORD_LENGTH_DEFAULT,
                      CYG_SERIAL_FLAGS_DEFAULT
    );
#endif
static unsigned char quicc_smc2_txbuf[CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC2_TxNUM][CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC2_TxSIZE + HAL_DCACHE_LINE_SIZE-1];
static unsigned char quicc_smc2_rxbuf[CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC2_RxNUM][CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC2_RxSIZE + HAL_DCACHE_LINE_SIZE-1];

DEVTAB_ENTRY(quicc_smc_serial_io2, 
             CYGDAT_IO_SERIAL_POWERPC_QUICC_SMC_SMC2_NAME,
             0,                     // Does not depend on a lower level interface
             &cyg_io_serial_devio, 
             quicc_smc_serial_init, 
             quicc_smc_serial_lookup,     // Serial driver may need initializing
             &quicc_smc_serial_channel2
    );
#endif //  CYGPKG_IO_SERIAL_POWERPC_QUICC_SMC_SMC2

#ifdef CYGDBG_DIAG_BUF
extern int enable_diag_uart;
#endif // CYGDBG_DIAG_BUF

// Internal function to actually configure the hardware to desired baud rate, etc.
static bool
quicc_smc_serial_config_port(serial_channel *chan, cyg_serial_info_t *new_config, bool init)
{
    quicc_smc_serial_info *smc_chan = (quicc_smc_serial_info *)chan->dev_priv;
    unsigned int baud_divisor = select_baud[new_config->baud];
    cyg_uint32 _lcr;
    EPPC *eppc = eppc_base();
    if (baud_divisor == 0) return false;
    // Disable channel during setup
    smc_chan->ctl->smc_smcmr = QUICC_SMCMR_UART;  // Disabled, UART mode
    // Disable port interrupts while changing hardware
    _lcr = select_word_length[new_config->word_length - CYGNUM_SERIAL_WORD_LENGTH_5] | 
        select_stop_bits[new_config->stop] |
        select_parity[new_config->parity];
    // Stop transmitter while changing baud rate
    eppc->cp_cr = smc_chan->channel | QUICC_SMC_CMD_Go | QUICC_SMC_CMD_StopTx;
    // Set baud rate generator
    *smc_chan->brg = 0x10000 | (UART_BITRATE(baud_divisor)<<1);
#ifdef XX_CYGDBG_DIAG_BUF
        enable_diag_uart = 0;
        diag_printf("Set BAUD RATE[%x], %d = %x, tstate = %x\n", smc_chan->brg, baud_divisor, *smc_chan->brg, smc_chan->pram->tstate);
        enable_diag_uart = 1;
#endif // CYGDBG_DIAG_BUF

    // Enable channel with new configuration
    smc_chan->ctl->smc_smcmr = QUICC_SMCMR_UART|QUICC_SMCMR_TEN|QUICC_SMCMR_REN|_lcr;
    eppc->cp_cr = smc_chan->channel | QUICC_SMC_CMD_Go | QUICC_SMC_CMD_RestartTx;
    if (new_config != &chan->config) {
        chan->config = *new_config;
    }
    return true;
}

// Function to set up internal tables for device.
static void
quicc_smc_serial_init_info(quicc_smc_serial_info *smc_chan,
                           volatile struct smc_uart_pram *uart_pram,
                           volatile struct smc_regs *ctl,
                           int TxBD, int TxNUM, int TxSIZE,
                           cyg_uint8 *TxBUF,
                           int RxBD, int RxNUM, int RxSIZE,
                           cyg_uint8 *RxBUF,
                           int portBmask,
                           int BRG, int SIpos)
{
    EPPC *eppc = eppc_base();
    struct cp_bufdesc *txbd, *rxbd;
    cyg_uint32 simode = 0;
    int i;

    // Disable channel during setup
    ctl->smc_smcmr = QUICC_SMCMR_UART;  // Disabled, UART mode
    smc_chan->pram = uart_pram;
    smc_chan->ctl = ctl;
    /*
     *  SDMA & LCD bus request level 5
     *  (Section 16.10.2.1)
     */
    eppc->dma_sdcr = 1;
    switch (BRG) {
    case 1:
        smc_chan->brg = (cyg_uint32 *)&eppc->brgc1;
        simode = 0;
        break;
    case 2:
        smc_chan->brg = (cyg_uint32 *)&eppc->brgc2;
        simode = 1;
        break;
    case 3:
        smc_chan->brg = (cyg_uint32 *)&eppc->brgc3;
        simode = 2;
        break;
    case 4:
        smc_chan->brg = (cyg_uint32 *)&eppc->brgc4;
        simode = 3;
        break;
    }
    // NMSI mode, BRGn to SMCm  (Section 16.12.5.2)
    eppc->si_simode = (eppc->si_simode & ~(0xF<<SIpos)) | (simode<<SIpos);
    /*
     *  Set up the PortB pins for UART operation.
     *  Set PAR and DIR to allow SMCTXDx and SMRXDx
     *  (Table 16-39)
     */
    eppc->pip_pbpar |= portBmask;
    eppc->pip_pbdir &= ~portBmask;
    /*
     *  SDMA & LCD bus request level 5
     *  (Section 16.10.2.1)
     */
    eppc->dma_sdcr = 1;
    /*
     *  Set Rx and Tx function code
     *  (Section 16.15.4.2)
     */
    uart_pram->rfcr = 0x18;
    uart_pram->tfcr = 0x18;
    /*
     *  Set pointers to buffer descriptors.
     *  (Sections 16.15.4.1, 16.15.7.12, and 16.15.7.13)
     */
    uart_pram->rbase = RxBD;
    uart_pram->tbase = TxBD;
    /* tx and rx buffer descriptors */
    txbd = (struct cp_bufdesc *)((char *)eppc + TxBD);
    rxbd = (struct cp_bufdesc *)((char *)eppc + RxBD);
    smc_chan->txbd = txbd;
    smc_chan->tbase = txbd;
    smc_chan->txsize = TxSIZE;
    smc_chan->rxbd = rxbd;
    smc_chan->rbase = rxbd;
    smc_chan->rxsize = RxSIZE;
    /* max receive buffer length */
    uart_pram->mrblr = RxSIZE;
    /* set max_idle feature - generate interrupt after 4 chars idle period */
    uart_pram->max_idl = 4;
    /* no last brk char received */
    uart_pram->brkln = 0;
    /* no break condition occurred */
    uart_pram->brkec = 0;
    /* 1 break char sent on top XMIT */
    uart_pram->brkcr = 1;
    /* setup RX buffer descriptors */
    for (i = 0;  i < RxNUM;  i++) {
        rxbd->length = 0;
        rxbd->buffer = RxBUF;
        rxbd->ctrl   = QUICC_BD_CTL_Ready | QUICC_BD_CTL_Int;
        if (i == (RxNUM-1)) rxbd->ctrl |= QUICC_BD_CTL_Wrap;  // Last buffer
        RxBUF += RxSIZE;
        rxbd++;
    }
    /* setup TX buffer descriptors */
    for (i = 0;  i < TxNUM;  i++) {
        txbd->length = 0;
        txbd->buffer = TxBUF;
        txbd->ctrl   = 0;
        if (i == (TxNUM-1)) txbd->ctrl |= QUICC_BD_CTL_Wrap;  // Last buffer
        TxBUF += TxSIZE;
        txbd++;
    }
    /*
     *  Reset Rx & Tx params
     */
    eppc->cp_cr = smc_chan->channel | QUICC_SMC_CMD_Go | QUICC_SMC_CMD_InitTxRx;
    /*
     *  Clear any previous events. Enable interrupts.
     *  (Section 16.15.7.14 and 16.15.7.15)
     */
    ctl->smc_smce = 0xFF;
    ctl->smc_smcm = QUICC_SMCE_BSY|QUICC_SMCE_TX|QUICC_SMCE_RX;
}

// Function to initialize the device.  Called at bootstrap time.
static bool 
quicc_smc_serial_init(struct cyg_devtab_entry *tab)
{
    serial_channel *chan = (serial_channel *)tab->priv;
    quicc_smc_serial_info *smc_chan = (quicc_smc_serial_info *)chan->dev_priv;
    volatile EPPC *eppc = (volatile EPPC *)eppc_base();
    int TxBD, RxBD;
    static int first_init = 1;
    int cache_state;

    HAL_DCACHE_IS_ENABLED(cache_state);
    HAL_DCACHE_SYNC();
    HAL_DCACHE_DISABLE();
#ifdef CYGDBG_IO_INIT
    diag_printf("QUICC_SMC SERIAL init - dev: %x.%d\n", smc_chan->channel, smc_chan->int_num);
#endif
    if (first_init) {
        // Set up tables since many fields are dynamic [computed at runtime]
        first_init = 0;
#ifdef CYGPKG_IO_SERIAL_POWERPC_QUICC_SMC_SMC1
        eppc->cp_cr = QUICC_SMC_CMD_Reset | QUICC_SMC_CMD_Go;  // Totally reset CP
        while (eppc->cp_cr & QUICC_SMC_CMD_Reset) ;
        TxBD = _mpc8xx_allocBd(sizeof(struct cp_bufdesc)*CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC1_TxNUM);
        RxBD = _mpc8xx_allocBd(sizeof(struct cp_bufdesc)*CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC1_RxNUM);
        quicc_smc_serial_init_info(&quicc_smc_serial_info1,
                                   &eppc->pram[2].scc.pothers.smc_modem.psmc.u, // PRAM
                                   &eppc->smc_regs[0], // Control registers
                                   TxBD, 
                                   CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC1_TxNUM,
                                   CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC1_TxSIZE,
                                   ALIGN_TO_CACHELINES(&quicc_smc1_txbuf[0][0]),
                                   RxBD, 
                                   CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC1_RxNUM,
                                   CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC1_RxSIZE,
                                   ALIGN_TO_CACHELINES(&quicc_smc1_rxbuf[0][0]),
                                   0xC0, // PortB mask
                                   CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC1_BRG,
                                   12  // SI mask position
            );
#else
#ifdef CYGPKG_HAL_POWERPC_MBX
        // Ensure the SMC1 side is initialized first and use shared mem
        // above where it plays:
        diag_init();    // (pull in constructor that inits diag channel)
        TxBD = 0x2830;  // Note: this should be inferred from the chip state
#else
        // there is no diag device wanting to use the QUICC, so prepare it
        // for SMC2 use only.
        eppc->cp_cr = QUICC_SMC_CMD_Reset | QUICC_SMC_CMD_Go;  // Totally reset CP
        while (eppc->cp_cr & QUICC_SMC_CMD_Reset) ;
        TxBD = 0x2800;  // Note: this should be configurable
#endif        
#endif
#ifdef CYGPKG_IO_SERIAL_POWERPC_QUICC_SMC_SMC2
        TxBD = _mpc8xx_allocBd(sizeof(struct cp_bufdesc)*CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC2_TxNUM);
        RxBD = _mpc8xx_allocBd(sizeof(struct cp_bufdesc)*CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC2_RxNUM);
        quicc_smc_serial_init_info(&quicc_smc_serial_info2,
                                   &eppc->pram[3].scc.pothers.smc_modem.psmc.u, // PRAM
                                   &eppc->smc_regs[1], // Control registers
                                   TxBD, 
                                   CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC2_TxNUM,
                                   CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC2_TxSIZE,
                                   ALIGN_TO_CACHELINES(&quicc_smc2_txbuf[0][0]),
                                   RxBD, 
                                   CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC2_RxNUM,
                                   CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC2_RxSIZE,
                                   ALIGN_TO_CACHELINES(&quicc_smc2_rxbuf[0][0]),
                                   0xC00, // PortB mask
                                   CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC2_BRG,
                                   28  // SI mask position
            );
#endif
    }
    (chan->callbacks->serial_init)(chan);  // Really only required for interrupt driven devices
    if (chan->out_cbuf.len != 0) {
        cyg_drv_interrupt_create(smc_chan->int_num,
                                 CYGARC_SIU_PRIORITY_HIGH, // Priority - unused (but asserted)
                                 (cyg_addrword_t)chan,   //  Data item passed to interrupt handler
                                 quicc_smc_serial_ISR,
                                 quicc_smc_serial_DSR,
                                 &smc_chan->serial_interrupt_handle,
                                 &smc_chan->serial_interrupt);
        cyg_drv_interrupt_attach(smc_chan->serial_interrupt_handle);
        cyg_drv_interrupt_unmask(smc_chan->int_num);
    }
    quicc_smc_serial_config_port(chan, &chan->config, true);
    if (cache_state)
        HAL_DCACHE_ENABLE();
    return true;
}

// This routine is called when the device is "looked" up (i.e. attached)
static Cyg_ErrNo 
quicc_smc_serial_lookup(struct cyg_devtab_entry **tab, 
                  struct cyg_devtab_entry *sub_tab,
                  const char *name)
{
    serial_channel *chan = (serial_channel *)(*tab)->priv;
    (chan->callbacks->serial_init)(chan);  // Really only required for interrupt driven devices
    return ENOERR;
}

// Force the current transmit buffer to be sent
static void
quicc_smc_serial_flush(quicc_smc_serial_info *smc_chan)
{
    volatile struct cp_bufdesc *txbd = smc_chan->txbd;
    int cache_state;
                                       
    HAL_DCACHE_IS_ENABLED(cache_state);
    if (cache_state) {
      HAL_DCACHE_FLUSH(txbd->buffer, smc_chan->txsize);
    }

    if ((txbd->length > 0) && 
        ((txbd->ctrl & (QUICC_BD_CTL_Ready|QUICC_BD_CTL_Int)) == 0)) {
        txbd->ctrl |= QUICC_BD_CTL_Ready|QUICC_BD_CTL_Int;  // Signal buffer ready
        if (txbd->ctrl & QUICC_BD_CTL_Wrap) {
            txbd = smc_chan->tbase;
        } else {
            txbd++;
        }
        smc_chan->txbd = txbd;
    }
}

// Send a character to the device output buffer.
// Return 'true' if character is sent to device
static bool
quicc_smc_serial_putc(serial_channel *chan, unsigned char c)
{
    quicc_smc_serial_info *smc_chan = (quicc_smc_serial_info *)chan->dev_priv;
    volatile struct cp_bufdesc *txbd, *txfirst;
    EPPC *eppc = eppc_base();
    bool res;
    cyg_drv_dsr_lock();  // Avoid race condition testing pointers
    txbd = (struct cp_bufdesc *)((char *)eppc + smc_chan->pram->tbptr);
    txfirst = txbd;
    // Scan for a non-busy buffer
    while (txbd->ctrl & QUICC_BD_CTL_Ready) {
        // This buffer is busy, move to next one
        if (txbd->ctrl & QUICC_BD_CTL_Wrap) {
            txbd = smc_chan->tbase;
        } else {
            txbd++;
        }
        if (txbd == txfirst) break;  // Went all the way around
    }
    smc_chan->txbd = txbd;
    if ((txbd->ctrl & (QUICC_BD_CTL_Ready|QUICC_BD_CTL_Int)) == 0) {
        // Transmit buffer is not full/busy
        txbd->buffer[txbd->length++] = c;
        if (txbd->length == smc_chan->txsize) {
            // This buffer is now full, tell SMC to start processing it
            quicc_smc_serial_flush(smc_chan);
        }
        res = true;
    } else {
        // No space
        res = false;
    }
    cyg_drv_dsr_unlock();
    return res;
}

// Fetch a character from the device input buffer, waiting if necessary
static unsigned char 
quicc_smc_serial_getc(serial_channel *chan)
{
    unsigned char c;
    quicc_smc_serial_info *smc_chan = (quicc_smc_serial_info *)chan->dev_priv;
    volatile struct cp_bufdesc *rxbd = smc_chan->rxbd;
    while ((rxbd->ctrl & QUICC_BD_CTL_Ready) != 0) ;
    c = rxbd->buffer[0];
    rxbd->length = smc_chan->rxsize;
    rxbd->ctrl |= QUICC_BD_CTL_Ready;
    if (rxbd->ctrl & QUICC_BD_CTL_Wrap) {
        rxbd = smc_chan->rbase;
    } else {
        rxbd++;
    }
    smc_chan->rxbd = (struct cp_bufdesc *)rxbd;
    return c;
}

// Set up the device characteristics; baud rate, etc.
static Cyg_ErrNo
quicc_smc_serial_set_config(serial_channel *chan, cyg_uint32 key,
                            const void *xbuf, cyg_uint32 *len)
{
    switch (key) {
    case CYG_IO_SET_CONFIG_SERIAL_INFO:
      {
          // FIXME - The documentation says that you can't change the baud rate
          // again until at least two BRG input clocks have occurred.
        cyg_serial_info_t *config = (cyg_serial_info_t *)xbuf;
        if ( *len < sizeof(cyg_serial_info_t) ) {
            return -EINVAL;
        }
        *len = sizeof(cyg_serial_info_t);
        if ( true != quicc_smc_serial_config_port(chan, config, false) )
            return -EINVAL;
      }
      break;
    default:
        return -EINVAL;
    }
    return ENOERR;
}

// Enable the transmitter (interrupt) on the device
static void
quicc_smc_serial_start_xmit(serial_channel *chan)
{
    quicc_smc_serial_info *smc_chan = (quicc_smc_serial_info *)chan->dev_priv;
    cyg_drv_dsr_lock();
    if (smc_chan->txbd->length == 0) {
        // See if there is anything to put in this buffer, just to get it going
        (chan->callbacks->xmt_char)(chan);
    }
    if (smc_chan->txbd->length != 0) {
        // Make sure it gets started
        quicc_smc_serial_flush(smc_chan);
    }
    cyg_drv_dsr_unlock();
}

// Disable the transmitter on the device
static void 
quicc_smc_serial_stop_xmit(serial_channel *chan)
{
    quicc_smc_serial_info *smc_chan = (quicc_smc_serial_info *)chan->dev_priv;
    // If anything is in the last buffer, need to get it started
    if (smc_chan->txbd->length != 0) {
        quicc_smc_serial_flush(smc_chan);
    }
}

// Serial I/O - low level interrupt handler (ISR)
static cyg_uint32 
quicc_smc_serial_ISR(cyg_vector_t vector, cyg_addrword_t data)
{
    serial_channel *chan = (serial_channel *)data;
    quicc_smc_serial_info *smc_chan = (quicc_smc_serial_info *)chan->dev_priv;
    cyg_drv_interrupt_mask(smc_chan->int_num);
    return (CYG_ISR_HANDLED|CYG_ISR_CALL_DSR);  // Cause DSR to be run
}

// Serial I/O - high level interrupt handler (DSR)
static void       
quicc_smc_serial_DSR(cyg_vector_t vector, cyg_ucount32 count, cyg_addrword_t data)
{
    serial_channel *chan = (serial_channel *)data;
    quicc_smc_serial_info *smc_chan = (quicc_smc_serial_info *)chan->dev_priv;
    volatile struct smc_regs *ctl = smc_chan->ctl;
    volatile struct cp_bufdesc *txbd;
    volatile struct cp_bufdesc *rxbd = smc_chan->rxbd;
    struct cp_bufdesc *rxlast;
    int i, cache_state;
#ifdef CYGDBG_DIAG_BUF
    int _time, _stime;
    externC cyg_tick_count_t cyg_current_time(void);
    cyg_drv_isr_lock();
    enable_diag_uart = 0;
    HAL_CLOCK_READ(&_time);
    _stime = (int)cyg_current_time();
    diag_printf("DSR start - CE: %x, time: %x.%x\n", ctl->smc_smce, _stime, _time);
    enable_diag_uart = 1;
#endif // CYGDBG_DIAG_BUF
    if (ctl->smc_smce & QUICC_SMCE_TX) {
#ifdef XX_CYGDBG_DIAG_BUF
        enable_diag_uart = 0;
        txbd = smc_chan->tbase;
        for (i = 0;  i < CYGNUM_IO_SERIAL_POWERPC_QUICC_SMC_SMC1_TxNUM;  i++, txbd++) {
            diag_printf("Tx BD: %x, length: %d, ctl: %x\n", txbd, txbd->length, txbd->ctrl);
        }
        enable_diag_uart = 1;
#endif // CYGDBG_DIAG_BUF
        // Transmit interrupt
        ctl->smc_smce = QUICC_SMCE_TX;  // Reset interrupt state;
        txbd = smc_chan->tbase;  // First buffer
        while (true) {
            if ((txbd->ctrl & (QUICC_BD_CTL_Ready|QUICC_BD_CTL_Int)) == QUICC_BD_CTL_Int) {
#ifdef XX_CYGDBG_DIAG_BUF
                enable_diag_uart = 0;
                HAL_CLOCK_READ(&_time);
                _stime = (int)cyg_current_time();
                diag_printf("TX Done - Tx: %x, length: %d, time: %x.%x\n", txbd, txbd->length, _stime, _time);
                enable_diag_uart = 1;
#endif // CYGDBG_DIAG_BUF
                txbd->length = 0;
                txbd->ctrl &= ~QUICC_BD_CTL_Int;  // Reset interrupt bit
            }
            if (txbd->ctrl & QUICC_BD_CTL_Wrap) {
                txbd = smc_chan->tbase;
                break;
            } else {
                txbd++;
            }
        }
        (chan->callbacks->xmt_char)(chan);
    }
    while (ctl->smc_smce & QUICC_SMCE_RX) {
        // Receive interrupt
        ctl->smc_smce = QUICC_SMCE_RX;  // Reset interrupt state;
        rxlast = (struct cp_bufdesc *) (
            (char *)eppc_base() + smc_chan->pram->rbptr );
#ifdef CYGDBG_DIAG_BUF
        enable_diag_uart = 0;
        HAL_CLOCK_READ(&_time);
        _stime = (int)cyg_current_time();
        diag_printf("Scan RX - rxbd: %x, rbptr: %x, time: %x.%x\n", rxbd, rxlast, _stime, _time);
#endif // CYGDBG_DIAG_BUF
        while (rxbd != rxlast) {
            if ((rxbd->ctrl & QUICC_BD_CTL_Ready) == 0) {
#ifdef CYGDBG_DIAG_BUF
                diag_printf("rxbuf: %x, flags: %x, length: %d\n", rxbd, rxbd->ctrl, rxbd->length);
                diag_dump_buf(rxbd->buffer, rxbd->length);
#endif // CYGDBG_DIAG_BUF
                for (i = 0;  i < rxbd->length;  i++) {
                    (chan->callbacks->rcv_char)(chan, rxbd->buffer[i]);
                }
                // Note: the MBX860 does not seem to snoop/invalidate the data cache properly!
                HAL_DCACHE_IS_ENABLED(cache_state);
                if (cache_state) {
                    HAL_DCACHE_INVALIDATE(rxbd->buffer, smc_chan->rxsize);  // Make sure no stale data
                }
                rxbd->length = 0;
                rxbd->ctrl |= QUICC_BD_CTL_Ready;
            }
            if (rxbd->ctrl & QUICC_BD_CTL_Wrap) {
                rxbd = smc_chan->rbase;
            } else {
                rxbd++;
            }
        }
#ifdef CYGDBG_DIAG_BUF
        enable_diag_uart = 1;
#endif // CYGDBG_DIAG_BUF
        smc_chan->rxbd = (struct cp_bufdesc *)rxbd;
    }
    if (ctl->smc_smce & QUICC_SMCE_BSY) {
#ifdef CYGDBG_DIAG_BUF
        enable_diag_uart = 0;
        diag_printf("RX BUSY interrupt\n");
        enable_diag_uart = 1;
#endif // CYGDBG_DIAG_BUF
        ctl->smc_smce = QUICC_SMCE_BSY;  // Reset interrupt state;
    }
#ifdef CYGDBG_DIAG_BUF
    enable_diag_uart = 0;
    HAL_CLOCK_READ(&_time);
    _stime = (int)cyg_current_time();
    diag_printf("DSR done - CE: %x, time: %x.%x\n", ctl->smc_smce, _stime, _time);
    enable_diag_uart = 1;
#endif // CYGDBG_DIAG_BUF
    cyg_drv_interrupt_acknowledge(smc_chan->int_num);
    cyg_drv_interrupt_unmask(smc_chan->int_num);
#ifdef CYGDBG_DIAG_BUF
    cyg_drv_isr_unlock();
#endif // CYGDBG_DIAG_BUF
}

void
show_rxbd(int dump_all)
{
#ifdef CYGDBG_DIAG_BUF
    EPPC *eppc = eppc_base();
    struct smc_uart_pram *pram = &eppc->pram[2].scc.pothers.smc_modem.psmc.u;
    struct cp_bufdesc *rxbd = (struct cp_bufdesc *)((char *)eppc+pram->rbase);
    int _enable = enable_diag_uart;
    enable_diag_uart = 0;
#if 1
    diag_printf("SMC Mask: %x, Events: %x, Rbase: %x, Rbptr: %x\n", 
                eppc->smc_regs[0].smc_smcm, eppc->smc_regs[0].smc_smce,
                pram->rbase, pram->rbptr);
    while (true) {
        diag_printf("Rx BD: %x, ctl: %x, length: %d\n", rxbd, rxbd->ctrl, rxbd->length);
        if (rxbd->ctrl & QUICC_BD_CTL_Wrap) break;
        rxbd++;
    }
#endif
    enable_diag_uart = _enable;
    if (dump_all) dump_diag_buf();
#endif // CYGDBG_DIAG_BUF
}
#endif // CYGPKG_IO_SERIAL_POWERPC_QUICC_SMC

// ------------------------------------------------------------------------
// EOF powerpc/quicc_smc_serial.c
