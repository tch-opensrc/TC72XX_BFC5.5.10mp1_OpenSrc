// eCos memory layout - Thu May 31 15:00:18 2001

// This is a generated file - do not edit

#ifndef __ASSEMBLER__
#include <cyg/infra/cyg_type.h>
#include <stddef.h>

#endif
#define CYGMEM_REGION_sram1 (0x82000000)
#define CYGMEM_REGION_sram1_SIZE (0x100000)
#define CYGMEM_REGION_sram1_ATTR (CYGMEM_REGION_ATTR_R | CYGMEM_REGION_ATTR_W)
#define CYGMEM_REGION_sram2 (0x89000000)
#define CYGMEM_REGION_sram2_SIZE (0x100000)
#define CYGMEM_REGION_sram2_ATTR (CYGMEM_REGION_ATTR_R | CYGMEM_REGION_ATTR_W)
#define CYGMEM_REGION_ram (0x8c020000)
#define CYGMEM_REGION_ram_SIZE (0x3fe0000)
#define CYGMEM_REGION_ram_ATTR (CYGMEM_REGION_ATTR_R | CYGMEM_REGION_ATTR_W)
#ifndef __ASSEMBLER__
extern char CYG_LABEL_NAME (__heap2) [];
#endif
#define CYGMEM_SECTION_heap2 (CYG_LABEL_NAME (__heap2))
#define CYGMEM_SECTION_heap2_SIZE (0x82100000 - (size_t) CYG_LABEL_NAME (__heap2))
#ifndef __ASSEMBLER__
extern char CYG_LABEL_NAME (__heap3) [];
#endif
#define CYGMEM_SECTION_heap3 (CYG_LABEL_NAME (__heap3))
#define CYGMEM_SECTION_heap3_SIZE (0x89100000 - (size_t) CYG_LABEL_NAME (__heap3))
#ifndef __ASSEMBLER__
extern char CYG_LABEL_NAME (__heap1) [];
#endif
#define CYGMEM_SECTION_heap1 (CYG_LABEL_NAME (__heap1))
#define CYGMEM_SECTION_heap1_SIZE (0x8ff00000 - (size_t) CYG_LABEL_NAME (__heap1))
#ifndef __ASSEMBLER__
extern char CYG_LABEL_NAME (__pci_window) [];
#endif
#define CYGMEM_SECTION_pci_window (CYG_LABEL_NAME (__pci_window))
#define CYGMEM_SECTION_pci_window_SIZE (0x100000)
