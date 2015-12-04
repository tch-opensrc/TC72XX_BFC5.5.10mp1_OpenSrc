//==========================================================================
//
//      ./lib/current/src/callback.c
//
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
//####UCDSNMPCOPYRIGHTBEGIN####
//
// -------------------------------------------
//
// Portions of this software may have been derived from the UCD-SNMP
// project,  <http://ucd-snmp.ucdavis.edu/>  from the University of
// California at Davis, which was originally based on the Carnegie Mellon
// University SNMP implementation.  Portions of this software are therefore
// covered by the appropriate copyright disclaimers included herein.
//
// The release used was version 4.1.2 of May 2000.  "ucd-snmp-4.1.2"
// -------------------------------------------
//
//####UCDSNMPCOPYRIGHTEND####
//==========================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):    hmt
// Contributors: hmt
// Date:         2000-05-30
// Purpose:      Port of UCD-SNMP distribution to eCos.
// Description:  
//              
//
//####DESCRIPTIONEND####
//
//==========================================================================
/********************************************************************
       Copyright 1989, 1991, 1992 by Carnegie Mellon University

			  Derivative Work -
Copyright 1996, 1998, 1999, 2000 The Regents of the University of California

			 All Rights Reserved

Permission to use, copy, modify and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appears in all copies and
that both that copyright notice and this permission notice appear in
supporting documentation, and that the name of CMU and The Regents of
the University of California not be used in advertising or publicity
pertaining to distribution of the software without specific written
permission.

CMU AND THE REGENTS OF THE UNIVERSITY OF CALIFORNIA DISCLAIM ALL
WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL CMU OR
THE REGENTS OF THE UNIVERSITY OF CALIFORNIA BE LIABLE FOR ANY SPECIAL,
INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
FROM THE LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*********************************************************************/
/* callback.c: A generic callback mechanism */

#include <config.h>
#include <sys/types.h>
#include <stdio.h>
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_WINSOCK_H
#include <winsock.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#if HAVE_DMALLOC_H
#include <dmalloc.h>
#endif

#include "tools.h"
#include "callback.h"
#include "asn1.h"
#include "snmp_api.h"
#include "snmp_debug.h"

static struct snmp_gen_callback *thecallbacks[MAX_CALLBACK_IDS][MAX_CALLBACK_SUBIDS];

/* the chicken. or the egg.  You pick. */
void
init_callbacks(void) {
  /* probably not needed? Should be full of 0's anyway? */
  /* (poses a problem if you put init_callbacks() inside of
     init_snmp() and then want the app to register a callback before
     init_snmp() is called in the first place.  -- Wes */
  /* memset(thecallbacks, 0, sizeof(thecallbacks)); */
}

int
snmp_register_callback(int major, int minor, SNMPCallback *new_callback,
                       void *arg) {

  struct snmp_gen_callback *scp;
  
  if (major >= MAX_CALLBACK_IDS || minor >= MAX_CALLBACK_SUBIDS) {
    return SNMPERR_GENERR;
  }
  
  if (thecallbacks[major][minor] != NULL) {
    /* get to the end of the list */
    for(scp = thecallbacks[major][minor]; scp->next != NULL; scp = scp->next);

    /* mallocate a new entry */
    scp->next = SNMP_MALLOC_STRUCT(snmp_gen_callback);
    scp = scp->next;
  } else {
    /* mallocate a new entry */
    scp = SNMP_MALLOC_STRUCT(snmp_gen_callback);

    /* make the new node the head */
    thecallbacks[major][minor] = scp;
  }

  if (scp == NULL)
    return SNMPERR_GENERR;

  scp->sc_client_arg = arg;
  scp->sc_callback = new_callback;

  DEBUGMSGTL(("callback","registered callback for maj=%d min=%d\n",
              major, minor));

  return SNMPERR_SUCCESS;
}

int
snmp_call_callbacks(int major, int minor, void *caller_arg) {
  struct snmp_gen_callback *scp;

  if (major >= MAX_CALLBACK_IDS || minor >= MAX_CALLBACK_SUBIDS) {
    return SNMPERR_GENERR;
  }

  DEBUGMSGTL(("callback","START calling callbacks for maj=%d min=%d\n",
              major, minor));

  /* for each registered callback of type major and minor */
  for(scp = thecallbacks[major][minor]; scp != NULL; scp = scp->next) {

    DEBUGMSGTL(("callback","calling a callback for maj=%d min=%d\n",
                major, minor));

    /* call them */
    (*(scp->sc_callback))(major, minor, caller_arg, scp->sc_client_arg);
  }
  
  DEBUGMSGTL(("callback","END calling callbacks for maj=%d min=%d\n",
              major, minor));

  return SNMPERR_SUCCESS;
}
