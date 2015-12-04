//==========================================================================
//
//      decompress.c
//
//      RedBoot decompress support
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
// Author(s):    jskov
// Contributors: jskov, gthomas, tkoeller
// Date:         2001-03-08
// Purpose:      
// Description:  
//              
// This code is part of RedBoot (tm).
//
//####DESCRIPTIONEND####
//
//==========================================================================

#include <redboot.h>

#ifdef CYGPKG_COMPRESS_ZLIB
#include <cyg/compress/zlib.h>
static z_stream stream;
static bool stream_end;

#define __ZLIB_MAGIC__ 0x5A4C4942   // 'ZLIB'

//
// Free memory [blocks] are stored as a linked list of "struct _block"
// When free, 'size' is the size of the whole block
// When allocated, 'size' is the allocation size
// The 'magic' is kept to insure that the block being freed is reasonable
//
struct _block {
    int            size;
    long           magic;  // Must be __ZLIB_MAGIC__
    struct _block *next;
    struct _block *self;
};
static struct _block *memlist;

#ifdef CYGOPT_REDBOOT_FIS_ZLIB_COMMON_BUFFER
# define ZLIB_COMPRESSION_OVERHEAD CYGNUM_REDBOOT_FIS_ZLIB_COMMON_BUFFER_SIZE
#else
# define ZLIB_COMPRESSION_OVERHEAD 0xC000
#endif
static void *zlib_workspace;

//
// This function is run as part of RedBoot's initialization.
// It will allocate some memory from the "workspace" pool for
// use by the gzip/zlib routines.  This allows the memory usage
// of RedBoot to be more finely controlled than if we simply
// used the generic 'malloc() from the heap' functionality.
//
static void
_zlib_init(void)
{
#ifdef CYGOPT_REDBOOT_FIS_ZLIB_COMMON_BUFFER
    zlib_workspace = fis_zlib_common_buffer;
#else
    // Allocate some RAM for use by the gzip/zlib routines
    workspace_end -= ZLIB_COMPRESSION_OVERHEAD;
    zlib_workspace = workspace_end;
#endif
}

RedBoot_init(_zlib_init, RedBoot_INIT_FIRST);

// #define DEBUG_ZLIB_MALLOC
#ifdef DEBUG_ZLIB_MALLOC
static void
show_memlist(char *when)
{
    struct _block *bp = memlist;

    diag_printf("memory list after %s\n", when);
    while (bp != (struct _block *)0) {
        diag_printf("   %08p %5d %08p\n", bp, bp->size, bp->next);
        bp = bp->next;
    }
}
#endif

// Note: these have to be global and match the prototype used by the
// gzip/zlib package since we are exactly replacing them.

void 
*zcalloc(void *opaque, unsigned int items, unsigned int size)
{
    voidpf res;
    int len = (items*size);
    struct _block *bp = memlist;
    struct _block *nbp, *pbp;

    // Simple, first-fit algorithm
    pbp = (struct _block *)0;
    while (bp) {
        if (bp->size > len) {
            nbp = (struct _block *)((char *)bp + len + sizeof(struct _block));
            nbp->next = bp->next;
            nbp->size = bp->size - len;
            if (pbp) {
                pbp->next = nbp;
            } else {
                memlist = nbp;
            }
            res = bp;
            break;
        }
        pbp = bp;
        bp = bp->next;
    }
    if (bp) {
        bp->size = len;
        bp->magic = __ZLIB_MAGIC__;
        bp->self = bp;
        res = bp+1;
        memset(res, 0, len);
    } else {
        res = 0;  // No memory left
    }
#ifdef DEBUG_ZLIB_MALLOC
    diag_printf("%s(%d,%d) = %p\n", __FUNCTION__, items, size, res);
    show_memlist(__FUNCTION__);
#endif
    return res;
}

void 
zcfree(void *opaque, void *ptr)
{
    struct _block *bp, *pbp, *nbp;
    int size;

    if (!ptr) return;  // Safety
    bp = (struct _block *)((char *)ptr - sizeof(struct _block));
    if ((bp->magic != __ZLIB_MAGIC__) || (bp->self != bp)) {
        diag_printf("%s(%p) - invalid block\n", __FUNCTION__, ptr);
        return;
    }
    size = bp->size;
#ifdef DEBUG_ZLIB_MALLOC
    diag_printf("%s(%p) = %d bytes\n", __FUNCTION__, ptr, size);
#endif
    // See if this block is adjacent to a free block
    nbp = memlist;
    pbp = (struct _block *)0;
    while (nbp) {
        if ((char *)bp+size+sizeof(struct _block) == (char *)nbp) {
            // The block being freed fits just before
            bp->next = nbp->next;
            bp->size = nbp->size + size + sizeof(struct _block);
#ifdef DEBUG_ZLIB_MALLOC
            diag_printf("Free before\n");
#endif
            if (pbp) {
                pbp->next = bp;
                // See if this new block and the previous one can
                // be combined.
                if ((char *)pbp+pbp->size == (char *)bp) {
#ifdef DEBUG_ZLIB_MALLOC
                    diag_printf("Collapse [before] - p: %p/%d/%p, n: %p/%d/%p\n",
                                pbp, pbp->size, pbp->next,
                                bp, bp->size, bp->next);
#endif
                    pbp->size += bp->size;
                    pbp->next = bp->next;
                }
            } else {
                memlist = bp;
            }
#ifdef DEBUG_ZLIB_MALLOC
            show_memlist(__FUNCTION__);
#endif
            return;
        } else
        if ((char *)nbp+nbp->size == (char *)bp) {
            // The block being freed fits just after
            nbp->size += size + sizeof(struct _block);
            // See if it will now collapse with the following block
#ifdef DEBUG_ZLIB_MALLOC
            diag_printf("Free after\n");
#endif
            if (nbp->next != (struct _block *)0) {
                if ((char *)nbp+nbp->size == (char *)nbp->next) {
#ifdef DEBUG_ZLIB_MALLOC
                    diag_printf("Collapse [after] - p: %p/%d/%p, n: %p/%d/%p\n",
                                nbp, nbp->size, nbp->next,
                                nbp->next, nbp->next->size, nbp->next->next);
#endif
                    nbp->size += (nbp->next)->size;
                    nbp->next = (nbp->next)->next;
                }
            }
#ifdef DEBUG_ZLIB_MALLOC
            show_memlist(__FUNCTION__);
#endif
            return;
        } else {
            pbp = nbp;
            nbp = nbp->next;
        }
    }
}

//
// This function is called to initialize a gzip/zlib stream.
// Note that it also reinitializes the memory pool every time.
//
static int
gzip_init(_pipe_t* p)
{
    int err;
    struct _block *bp;

    bp = (struct _block *)zlib_workspace;
    memlist = bp;
    bp->next = 0;
    bp->size = ZLIB_COMPRESSION_OVERHEAD;
    stream.zalloc = zcalloc;
    stream.zfree = zcfree;
    stream.next_in = NULL;
    stream.avail_in = 0;
    stream.next_out = NULL;
    stream.avail_out = 0;
    err = inflateInit(&stream);
    stream_end = false;

    return err;
}

//
// This function is called during the decompression cycle to
// actually cause a buffer to be filled with uncompressed data.
//
static int
gzip_inflate(_pipe_t* p)
{
    int err, bytes_out;

    if (stream_end)
	return Z_STREAM_END;
	
    stream.next_in = p->in_buf;
    stream.avail_in = p->in_avail;
    stream.next_out = p->out_buf;
    stream.avail_out = p->out_max;
    err = inflate(&stream, Z_SYNC_FLUSH);
    bytes_out = stream.next_out - p->out_buf;
    p->out_size += bytes_out;
    p->out_buf = stream.next_out;
    p->msg = stream.msg;
    p->in_avail = stream.avail_in;
    p->in_buf = stream.next_in;

    // Let upper layers process any inflated bytes at
    // end of stream.
    if (err == Z_STREAM_END && bytes_out) {
	stream_end = true;
	err = Z_OK;
    }

    return err;
}

//
// Called when the input data is completed or an error has
// occured.  This allows for clean up as well as passing error
// information up.
//
static int
gzip_close(_pipe_t* p, int err)
{
    switch (err) {
    case Z_STREAM_END:
        err = 0;
        break;
    case Z_OK:
        if (stream_end) {
          break;
        }
        // Decompression didn't complete
        p->msg = "premature end of input";
        // fall-through
    default:
        err = -1;
        break;
    }

    inflateEnd(&stream);

    return err;
}

//
// Exported interfaces
//
_decompress_fun_init* _dc_init = gzip_init;
_decompress_fun_inflate* _dc_inflate = gzip_inflate;
_decompress_fun_close* _dc_close = gzip_close;
#endif // CYGPKG_COMPRESS_ZLIB
