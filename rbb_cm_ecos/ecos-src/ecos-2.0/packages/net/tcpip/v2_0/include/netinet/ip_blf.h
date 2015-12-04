//==========================================================================
//
//      include/netinet/ip_blf.h
//
//      
//
//==========================================================================
//####BSDCOPYRIGHTBEGIN####
//
// -------------------------------------------
//
// Portions of this software may have been derived from OpenBSD or other sources,
// and are covered by the appropriate copyright disclaimers included herein.
//
// -------------------------------------------
//
//####BSDCOPYRIGHTEND####
//==========================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):    gthomas
// Contributors: gthomas
// Date:         2000-01-10
// Purpose:      
// Description:  
//              
//
//####DESCRIPTIONEND####
//
//==========================================================================


/* $OpenBSD: ip_blf.h,v 1.2 1999/02/23 05:15:09 angelos Exp $ */
/*
 * Blowfish - a fast block cipher designed by Bruce Schneier
 *
 * Copyright 1997 Niels Provos <provos@physnet.uni-hamburg.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _NETINET_IP_BLF_H_
#define _NETINET_IP_BLF_H_

/* Schneier states the maximum key length to be 56 bytes.
 * The way how the subkeys are initalized by the key up
 * to (N+2)*4 i.e. 72 bytes are utilized.
 * Warning: For normal blowfish encryption only 56 bytes
 * of the key affect all cipherbits.
 */

#define BLF_N	16			/* Number of Subkeys */
#define BLF_MAXKEYLEN ((BLF_N-2)*4)	/* 448 bits */

/* Blowfish context */
typedef struct BlowfishContext {
	u_int32_t S[4][256];	/* S-Boxes */
	u_int32_t P[BLF_N + 2];	/* Subkeys */
} blf_ctx;

/* Raw access to customized Blowfish
 *	blf_key is just:
 *	Blowfish_initstate( state )
 *	Blowfish_expand0state( state, key, keylen )
 */

void Blowfish_encipher __P((blf_ctx *, u_int32_t *, u_int32_t *));
void Blowfish_decipher __P((blf_ctx *, u_int32_t *, u_int32_t *));
void Blowfish_initstate __P((blf_ctx *));
void Blowfish_expand0state __P((blf_ctx *, const u_int8_t *, u_int16_t));
void Blowfish_expandstate
    __P((blf_ctx *, const u_int8_t *, u_int16_t, const u_int8_t *, u_int16_t));

/* Standard Blowfish */

void blf_key __P((blf_ctx *, const u_int8_t *, u_int16_t));
void blf_enc __P((blf_ctx *, u_int32_t *, u_int16_t));
void blf_dec __P((blf_ctx *, u_int32_t *, u_int16_t));

/* Converts u_int8_t to u_int32_t */
u_int32_t Blowfish_stream2word __P((const u_int8_t *, u_int16_t ,
				    u_int16_t *));

void blf_ecb_encrypt __P((blf_ctx *, u_int8_t *, u_int32_t));
void blf_ecb_decrypt __P((blf_ctx *, u_int8_t *, u_int32_t));

#endif // _NETINET_IP_BLF_H_
