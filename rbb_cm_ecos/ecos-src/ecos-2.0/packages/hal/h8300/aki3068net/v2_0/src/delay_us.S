;;delay_us

#include <pkgconf/hal.h>	

#include <cyg/hal/arch.inc>	
#include <cyg/hal/basetype.h>
#include <cyg/hal/mod_regs_tmr.h>
	
#define DELAY_COUNT (CYGHWR_HAL_H8300_PROCESSOR_SPEED/1000000)*16/8
	
//	.file	"delay_us.S"
	.h8300h
	.text

	.global	CYG_LABEL_DEFN(hal_delay_us)
CYG_LABEL_DEFN(hal_delay_us):
	mov.b	#0,r1l
	mov.b	r1l,@CYGARC_8TCNT2:8
	mov.b	r1l,@CYGARC_8TCSR2:8
	mov.b	#DELAY_COUNT+1,r1l
	mov.b	r1l,@CYGARC_TCORA2:8
	mov.b	#0x09,r1l
	mov.b	r1l,@CYGARC_8TCR2:8
	mov.b	#0,r2h
	sub.w	e1,e1
1:
	mov.l	er0,er0
	ble	3f
2:
	btst	#6,@CYGARC_8TCSR2:8
	beq	2b
	bclr	#6,@CYGARC_8TCSR2:8
	dec.l	#1,er0
	bra	1b
3:	
	mov.b	#0,r0l
	mov.b	r0l,@CYGARC_8TCR2
	rts

