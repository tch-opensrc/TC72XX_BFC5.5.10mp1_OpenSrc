// eCos memory layout - Thu Mar 16 16:47:52 2000

// This is a generated file - do not edit

#ifndef __ASSEMBLER__
#include <cyg/infra/cyg_type.h>
#include <stddef.h>
#endif

#define CYGMEM_REGION_ram (0x80000200)
#define CYGMEM_REGION_ram_SIZE (0x1000000)
#define CYGMEM_REGION_ram_ATTR (CYGMEM_REGION_ATTR_R | CYGMEM_REGION_ATTR_W)
#define CYGMEM_REGION_rom (0x80200000)
#define CYGMEM_REGION_rom_SIZE (0x800000)
#define CYGMEM_REGION_rom_ATTR (CYGMEM_REGION_ATTR_R)
#ifndef __ASSEMBLER__
extern char CYG_LABEL_NAME (__heap1) [];
#endif
#define CYGMEM_SECTION_heap1 (CYG_LABEL_NAME (__heap1))
#define CYGMEM_SECTION_heap1_SIZE (0x81000200 - (size_t) CYG_LABEL_NAME (__heap1))
