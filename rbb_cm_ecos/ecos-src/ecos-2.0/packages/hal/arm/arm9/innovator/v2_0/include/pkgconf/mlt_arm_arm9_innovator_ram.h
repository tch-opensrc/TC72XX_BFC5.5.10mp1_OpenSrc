// eCos memory layout - Tue Aug 07 11:15:51 2001

// This is a generated file - do not edit

#ifndef __ASSEMBLER__
#include <cyg/infra/cyg_type.h>
#include <stddef.h>

#endif
#define CYGMEM_REGION_ram         (0x00000000)
#define CYGMEM_REGION_ram_SIZE    (0x02000000)
#define CYGMEM_REGION_ram_ATTR    (CYGMEM_REGION_ATTR_R | CYGMEM_REGION_ATTR_W)
#ifndef __ASSEMBLER__
extern char CYG_LABEL_NAME (__heap1) [];
#endif
#define CYGMEM_SECTION_heap1      (CYG_LABEL_NAME (__heap1))
#define CYGMEM_SECTION_heap1_SIZE (0x02000000 - (size_t) CYG_LABEL_NAME (__heap1))
