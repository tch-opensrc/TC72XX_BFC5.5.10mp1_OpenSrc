//==========================================================================
//
//      src/lwip/eth_drv.c
//
//      Hardware independent ethernet driver for lwIP
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
// Author(s):    Jani Monoses <jani@iv.ro>
// Contributors: 
// Date:         2002-04-05
// Purpose:      Hardware independent ethernet driver
// Description:  Based on the standalone driver for RedBoot.
//               
//      TODO:
//              support more than 1 lowlevel device
//              play nice with RedBoot too
//
//####DESCRIPTIONEND####
//
//==========================================================================

#include <pkgconf/system.h>
#include <pkgconf/io_eth_drivers.h>

#include <cyg/infra/cyg_type.h>
#include <cyg/hal/hal_arch.h>
#include <cyg/infra/diag.h>
#include <cyg/hal/drv_api.h>
#include <cyg/hal/hal_if.h>
#include <cyg/io/eth/eth_drv.h>
#include <cyg/io/eth/netdev.h>
#include <string.h>

#include <cyg/hal/hal_tables.h>
#include <cyg/kernel/kapi.h>

// Define table boundaries
CYG_HAL_TABLE_BEGIN( __NETDEVTAB__, netdev );
CYG_HAL_TABLE_END( __NETDEVTAB_END__, netdev );

// Interfaces exported to drivers

static void eth_drv_init(struct eth_drv_sc *sc, unsigned char *enaddr);
static void eth_drv_recv(struct eth_drv_sc *sc, int total_len);
static void eth_drv_tx_done(struct eth_drv_sc *sc, CYG_ADDRWORD key, int status);

struct eth_drv_funs eth_drv_funs = {eth_drv_init, eth_drv_recv, eth_drv_tx_done};

struct eth_drv_sc *__local_enet_sc;

//this is where lwIP keeps hw address
unsigned char *lwip_hw_addr; 

cyg_sem_t delivery;

//lwIP callback to pass received data to
typedef void (*lwip_input_t)(char *,int);
static lwip_input_t lwip_input;
void input_thread(void * arg)
{
	struct eth_drv_sc * sc;
	//sc = (struct eth_drv_sc *)arg;
	sc = __local_enet_sc;
	for(;;) {
	cyg_semaphore_wait(&delivery);
	(sc->funs->deliver)(sc);
	}	

}

void
eth_drv_dsr(cyg_vector_t vector,
            cyg_ucount32 count,
            cyg_addrword_t data)
{
	//  struct eth_drv_sc *sc = (struct eth_drv_sc *)data;
	//  sc->state |= ETH_DRV_NEEDS_DELIVERY;
	cyg_semaphore_post(&delivery);
}



//Called from lwIP init code to init the hw devices
//and pass the lwip input callback address 
void init_hw_drivers(unsigned char *hw_addr,lwip_input_t input)
{
    cyg_netdevtab_entry_t *t;
    
    lwip_hw_addr = hw_addr;
    lwip_input	 = input;
	
// Initialize all network devices
    for (t = &__NETDEVTAB__[0]; t != &__NETDEVTAB_END__; t++) {
        if (t->init(t)) {
            t->status = CYG_NETDEVTAB_STATUS_AVAIL;
        } else {
            // What to do if device init fails?
            t->status = 0;  // Device not [currently] available
        }
    }
}

//
// This function is called during system initialization to register a
// network interface with the system.
//
static void
eth_drv_init(struct eth_drv_sc *sc, unsigned char *enaddr)
{
    // enaddr == 0 -> hardware init was incomplete (no ESA)
    if (enaddr != 0) {
        // Set up hardware address
        memcpy(&sc->sc_arpcom.esa, enaddr, ETHER_ADDR_LEN);
        memcpy(lwip_hw_addr, enaddr, ETHER_ADDR_LEN);
        __local_enet_sc = sc;
    	// Perform any hardware initialization
    	(sc->funs->start)(sc, (unsigned char *)&sc->sc_arpcom.esa, 0);
    }
    cyg_semaphore_init(&delivery,0);
}

//
// Send a packet of data to the hardware
//
cyg_sem_t  packet_sent;

void
eth_drv_write(char *eth_hdr, char *buf, int len)
{
    struct eth_drv_sg sg_list[MAX_ETH_DRV_SG];
    struct eth_drv_sc *sc = __local_enet_sc;
    int sg_len = 1;

    while (!(sc->funs->can_send)(sc)) {
  	cyg_thread_delay(1);
    }
   
    sg_list[0].buf = (CYG_ADDRESS)buf;
    sg_list[0].len = len;
//    cyg_semaphore_init(&packet_sent,0);
    (sc->funs->send)(sc, sg_list, sg_len, len, (CYG_ADDRWORD)&packet_sent);
    
//    cyg_semaphore_wait(&packet_sent);
}

//
// This function is called from the hardware driver when an output operation
// has completed - i.e. the packet has been sent.
//
static void
eth_drv_tx_done(struct eth_drv_sc *sc, CYG_ADDRWORD key, int status)
{
#if 0	
    CYGARC_HAL_SAVE_GP();
    if (key == (CYG_ADDRWORD)&packet_sent) {
//	cyg_semaphore_post((cyg_sem_t *)&packet_sent);
    }
    CYGARC_HAL_RESTORE_GP();
#endif    
}


#define MAX_ETH_MSG 1540
//
// This function is called from a hardware driver to indicate that an input
// packet has arrived.  The routine will set up appropriate network resources
// to hold the data and call back into the driver to retrieve the data.
//
static void
eth_drv_recv(struct eth_drv_sc *sc, int total_len)
{
    struct eth_drv_sg sg_list[MAX_ETH_DRV_SG];
    int               sg_len = 0;
    unsigned char buf[MAX_ETH_MSG];
    CYGARC_HAL_SAVE_GP();

    if ((total_len > MAX_ETH_MSG) || (total_len < 0)) {        
        total_len = MAX_ETH_MSG;
    }

    sg_list[0].buf = (CYG_ADDRESS)buf;
    sg_list[0].len = total_len;
    sg_len = 1;

    (sc->funs->recv)(sc, sg_list, sg_len);
    (lwip_input)((char*)sg_list[0].buf,total_len);		
    CYGARC_HAL_RESTORE_GP();
}


// EOF src/lwip/eth_drv.c
