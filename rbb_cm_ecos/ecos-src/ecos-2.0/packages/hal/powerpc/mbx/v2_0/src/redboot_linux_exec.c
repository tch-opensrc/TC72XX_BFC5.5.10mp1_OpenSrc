//==========================================================================
//
//      redboot_linux_boot.c
//
//      RedBoot command to boot Linux
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
//####OTHERCOPYRIGHTBEGIN####
//
//  The structure definitions below are taken from include/asm-ppc/mbx.h in
//  the Linux kernel, Copyright (c) 1997 Dan Malek. Their presence
//  here is for the express purpose of communication with the Linux kernel
//  being booted and is considered 'fair use' by the original author and
//  are included with his permission.
//
//####OTHERCOPYRIGHTEND####
//==========================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):    msalter
// Contributors: gthomas,msalter
// Date:         2002-01-14
// Purpose:      
// Description:  
//              
// This code is part of RedBoot (tm).
//
//####DESCRIPTIONEND####
//
//==========================================================================

#include <redboot.h>
#include CYGBLD_HAL_TARGET_H
#include CYGBLD_HAL_PLATFORM_H

#include <cyg/hal/hal_intr.h>
#include <cyg/hal/hal_cache.h>

#ifdef CYGSEM_REDBOOT_HAL_LINUX_BOOT

#include CYGHWR_MEMORY_LAYOUT_H

//=========================================================================

// Exported CLI function(s)
static void do_exec(int argc, char *argv[]);
RedBoot_cmd("exec", 
            "Execute an image - with MMU off", 
            "[-w timeout]\n"
            "        [-r <ramdisk addr> [-s <ramdisk length>]]\n"
            "        [-c \"kernel command line\"] [<entry_point>]",
            do_exec
    );

//=========================================================================
// Imported from Linux kernel include/asm-ppc/mbx.h
//   Copyright (c) 1997 Dan Malek (dmalek@jlc.net)
//   Used with permission of author.


/* A Board Information structure that is given to a program when
 * EPPC-Bug starts it up.
 */
typedef struct bd_info {
	unsigned int	bi_tag;		/* Should be 0x42444944 "BDID" */
	unsigned int	bi_size;	/* Size of this structure */
	unsigned int	bi_revision;	/* revision of this structure */
	unsigned int	bi_bdate;	/* EPPCbug date, i.e. 0x11061997 */
	unsigned int	bi_memstart;	/* Memory start address */
	unsigned int	bi_memsize;	/* Memory (end) size in bytes */
	unsigned int	bi_intfreq;	/* Internal Freq, in Hz */
	unsigned int	bi_busfreq;	/* Bus Freq, in Hz */
	unsigned int	bi_clun;	/* Boot device controller */
	unsigned int	bi_dlun;	/* Boot device logical dev */

	/* These fields are not part of the board information structure
	 * provided by the boot rom.  They are filled in by embed_config.c
	 * so we have the information consistent with other platforms.
	 */
	unsigned char	bi_enetaddr[6];
	unsigned int	bi_baudrate;
} bd_t;

/* Memory map for the MBX as configured by EPPC-Bug.  We could reprogram
 * The SIU and PCI bridge, and try to use larger MMU pages, but the
 * performance gain is not measureable and it certainly complicates the
 * generic MMU model.
 *
 * In a effort to minimize memory usage for embedded applications, any
 * PCI driver or ISA driver must request or map the region required by
 * the device.  For convenience (and since we can map up to 4 Mbytes with
 * a single page table page), the MMU initialization will map the
 * NVRAM, Status/Control registers, CPM Dual Port RAM, and the PCI
 * Bridge CSRs 1:1 into the kernel address space.
 */
#define PCI_ISA_IO_ADDR		((unsigned)0x80000000)
#define PCI_ISA_IO_SIZE		((unsigned int)(512 * 1024 * 1024))
#define PCI_IDE_ADDR		((unsigned)0x81000000)
#define PCI_ISA_MEM_ADDR	((unsigned)0xc0000000)
#define PCI_ISA_MEM_SIZE	((unsigned int)(512 * 1024 * 1024))
#define PCMCIA_MEM_ADDR		((unsigned int)0xe0000000)
#define PCMCIA_MEM_SIZE		((unsigned int)(64 * 1024 * 1024))
#define PCMCIA_DMA_ADDR		((unsigned int)0xe4000000)
#define PCMCIA_DMA_SIZE		((unsigned int)(64 * 1024 * 1024))
#define PCMCIA_ATTRB_ADDR	((unsigned int)0xe8000000)
#define PCMCIA_ATTRB_SIZE	((unsigned int)(64 * 1024 * 1024))
#define PCMCIA_IO_ADDR		((unsigned int)0xec000000)
#define PCMCIA_IO_SIZE		((unsigned int)(64 * 1024 * 1024))
#define NVRAM_ADDR		((unsigned int)0xfa000000)
#define NVRAM_SIZE		((unsigned int)(1 * 1024 * 1024))
#define MBX_CSR_ADDR		((unsigned int)0xfa100000)
#define MBX_CSR_SIZE		((unsigned int)(1 * 1024 * 1024))
#define IMAP_ADDR		((unsigned int)0xfa200000)
#define IMAP_SIZE		((unsigned int)(64 * 1024))
#define PCI_CSR_ADDR		((unsigned int)0xfa210000)
#define PCI_CSR_SIZE		((unsigned int)(64 * 1024))

/* Map additional physical space into well known virtual addresses.  Due
 * to virtual address mapping, these physical addresses are not accessible
 * in a 1:1 virtual to physical mapping.
 */
#define ISA_IO_VIRT_ADDR	((unsigned int)0xfa220000)
#define ISA_IO_VIRT_SIZE	((unsigned int)64 * 1024)

/* Interrupt assignments.
 * These are defined (and fixed) by the MBX hardware implementation.
 */
#define POWER_FAIL_INT	SIU_IRQ0	/* Power fail */
#define TEMP_HILO_INT	SIU_IRQ1	/* Temperature sensor */
#define QSPAN_INT	SIU_IRQ2	/* PCI Bridge (DMA CTLR?) */
#define ISA_BRIDGE_INT	SIU_IRQ3	/* All those PC things */
#define COMM_L_INT	SIU_IRQ6	/* MBX Comm expansion connector pin */
#define STOP_ABRT_INT	SIU_IRQ7	/* Stop/Abort header pin */

/* The MBX uses the 8259.
*/
#define NR_8259_INTS	16

// End of imported data/structures
//=========================================================================

#define MBX_CSR_COM1            0x02  // COM1 enabled

//
// Execute a Linux kernel - this is a RedBoot CLI command
//
static void 
do_exec(int argc, char *argv[])
{
    unsigned long entry;
    bool wait_time_set;
    int  wait_time;
    bool cmd_line_set, ramdisk_addr_set, ramdisk_size_set;
    unsigned long ramdisk_addr, ramdisk_size;
    char *cmd_line;
    char *cline;
    struct option_info opts[6];
    
    bd_t *board_info;

    CYG_INTERRUPT_STATE oldints;
    unsigned long sp = CYGMEM_REGION_ram+CYGMEM_REGION_ram_SIZE;
    
    init_opts(&opts[0], 'w', true, OPTION_ARG_TYPE_NUM, 
              (void **)&wait_time, (bool *)&wait_time_set, "wait timeout");
    init_opts(&opts[1], 'c', true, OPTION_ARG_TYPE_STR, 
              (void **)&cmd_line, (bool *)&cmd_line_set, "kernel command line");
    init_opts(&opts[2], 'r', true, OPTION_ARG_TYPE_NUM, 
              (void **)&ramdisk_addr, (bool *)&ramdisk_addr_set, "ramdisk_addr");
    init_opts(&opts[3], 's', true, OPTION_ARG_TYPE_NUM, 
              (void **)&ramdisk_size, (bool *)&ramdisk_size_set, "ramdisk_size");
    if (!scan_opts(argc, argv, 1, opts, 4, (void *)&entry, OPTION_ARG_TYPE_NUM, 
                   "[physical] starting address"))
    {
        return;
    }

    // Make a little space at the top of the stack, and align to
    // 64-bit boundary.
    sp = (sp-8) & ~7;
    
    // Set up parameter struct at top of stack

    sp = sp-sizeof(bd_t);
    
    board_info = (bd_t *)sp;
    
    board_info->bi_tag		= 0x42444944;
    board_info->bi_size		= sizeof(board_info);
    board_info->bi_revision	= 1;
    board_info->bi_bdate	= 0x11061997;
    board_info->bi_memstart	= CYGMEM_REGION_ram;
    board_info->bi_memsize	= CYGMEM_REGION_ram_SIZE;
    board_info->bi_intfreq	= CYGHWR_HAL_POWERPC_BOARD_SPEED;
    board_info->bi_busfreq	= 66; // ????
    board_info->bi_clun		= 0;  // ????
    board_info->bi_dlun		= 0;  // ????

    // Copy the commandline onto the stack, and set the SP to just below it.

    // If no cmd_line set in args, set it to empty string.
    if( !cmd_line_set )
	cmd_line = "";
    
    {
	int len,i;

	// get length of string
	for( len = 0; cmd_line[len] != '\0'; len++ );

	// decrement sp by length of string and align to
	// word boundary.
	sp = (sp-(len+1)) & ~3;

	// assign this SP value to command line start
	cline = (char *)sp;

	// copy command line over.
	for( i = 0; i < len; i++ )
	    cline[i] = cmd_line[i];
	cline[len] = '\0';

	// adjust SP to 64 bit boundary, and leave a little space
	// between it and the commandline for PowerPC calling
	// conventions.
	
	sp = (sp-32)&~7;
    
    }

    // Disable interrupts
    HAL_DISABLE_INTERRUPTS(oldints);

    // Put the caches to sleep.
    HAL_DCACHE_SYNC();
    HAL_ICACHE_DISABLE();
    HAL_DCACHE_DISABLE();
    HAL_DCACHE_SYNC();
    HAL_ICACHE_INVALIDATE_ALL();
    HAL_DCACHE_INVALIDATE_ALL();

//    diag_printf("entry %08x sp %08x bi %08x cl %08x\n",
//		entry,sp,board_info,cline);
//    breakpoint();
    
    // Call into Linux
    *(volatile unsigned char *)MBX_CSR_ADDR |= MBX_CSR_COM1;  // Magic that says COM1 enabled
    __asm__ volatile (        
                       // Linux seems to want the I/O mapped at 0xFA200000
                       "lis     3,0xFA20\n"
                       "mtspr   638,3\n"

	               // Start by disabling MMU - the mappings are
	               // 1-1 so this should not cause any problems
	               "mfmsr	3\n"
		       "li      4,0xFFFFFFCF\n"
		       "and	3,3,4\n"
		       "sync\n"
		       "mtmsr	3\n"
		       "sync\n"

		       // Now set up parameters to jump into linux

		       "mtlr	%0\n"		// set entry address in LR
		       "mr	1,%1\n"		// set stack pointer
		       "mr	3,%2\n"		// set board info in R3
		       "mr	4,%3\n"		// set command line in R4
		       "blr          \n"	// jump into linux
		       :
		       : "r"(entry),"r"(sp),"r"(board_info),"r"(cline)
		       : "r3", "r4"
	             
	             );
}

#endif // CYGSEM_REDBOOT_HAL_LINUX_BOOT

//=========================================================================
// EOF redboot_linux_exec.c
