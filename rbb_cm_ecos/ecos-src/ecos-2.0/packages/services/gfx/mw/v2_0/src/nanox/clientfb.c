/*
 * Microwindows direct client-side framebuffer mapping routines
 *
 * Copyright (c) 2001, 2002 by Greg Haerr <greg@censoft.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef __ECOS
# include <pkgconf/system.h>
# ifdef CYGPKG_HAL_ARM
#  include <cyg/hal/lcd_support.h>
# endif
#else
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <asm/page.h>		/* For definition of PAGE_SIZE */
#include <linux/fb.h>
#endif
#include "nano-X.h"
#include "device.h"

/* globals: assumes use of non-shared libnano-X.a for now*/
static int 		frame_fd;	/* client side framebuffer fd*/
static unsigned char *	frame_map;	/* client side framebuffer mmap'd addr*/
static int 		frame_len;	/* client side framebuffer length*/
static unsigned char *	physpixels;	/* start address of pixels*/
static GR_SCREEN_INFO	sinfo;

/* map framebuffer address into client memory*/
unsigned char *
GrOpenClientFramebuffer(void)
{
#ifndef __ECOS
	int 	frame_offset;
	char *	fbdev;
	struct fb_fix_screeninfo finfo;
#endif
    
	/* if already open, return fb address*/
	if (physpixels)
		return physpixels;

	/*
	 * For now, we'll just check whether or not Microwindows
	 * is running its framebuffer driver to determine whether
	 * to allow direct client-side framebuffer mapping.  In
	 * the future, we could allow direct mapping for Microwindows
	 * running on top of X, and finding the address of the 
	 * window within the Microwindows X window.
	 */
	GrGetScreenInfo(&sinfo);
	if (!sinfo.fbdriver)
		return NULL;

#ifdef __ECOS
# ifdef CYGPKG_HAL_ARM
    {
        struct lcd_info li;
        lcd_getinfo(&li);
        physpixels = li.fb;
        return physpixels;
    }
# else
    /* This works for scr_ecospcsvga.c, at least    */
    {
        physpixels  = scrdev.addr;
        return physpixels;
    }
# endif        
#else
	/*
	 * Try to open the framebuffer directly.
	 */
	if (!(fbdev = getenv("FRAMEBUFFER")))
		fbdev = "/dev/fb0";
	frame_fd = open(fbdev, O_RDWR);
	if (frame_fd < 0) {
		printf("Can't open framebuffer device\n");
		return NULL;
	}

	/* Get the type of video hardware */
	if (ioctl(frame_fd, FBIOGET_FSCREENINFO, &finfo) < 0 ) {
		printf("Couldn't get fb hardware info\n");
		goto err;
	}

	// FIXME remove when mwin returns fb or X
	switch (finfo.visual) {
		case FB_VISUAL_TRUECOLOR:
		case FB_VISUAL_PSEUDOCOLOR:
		case FB_VISUAL_STATIC_PSEUDOCOLOR:
		case FB_VISUAL_DIRECTCOLOR:
			break;
		default:
			printf("Unsupported fb color map\n");
			goto err;
	}

	/* Memory map the device, compensating for buggy PPC mmap() */
	frame_offset = (((long)finfo.smem_start) -
		(((long)finfo.smem_start)&~(PAGE_SIZE-1)));
	frame_len = finfo.smem_len + frame_offset;
	frame_map = (unsigned char *)mmap(NULL, frame_len, PROT_READ|PROT_WRITE,
		MAP_SHARED, frame_fd, 0);
	if (frame_map == (unsigned char *)-1) {
		printf("Unable to memory map the video hardware\n");
		frame_map = NULL;
		goto err;
	}
	physpixels = frame_map + frame_offset;
	return physpixels;

err:
	close(frame_fd);
	return NULL;
#endif // __ECOS
}

void
GrCloseClientFramebuffer(void)
{
#ifndef __ECOS
	if (frame_fd >= 0) {
		if (frame_map) {
			munmap(frame_map, frame_len);
			frame_map = NULL;
			physpixels = NULL;
		}
		close(frame_fd);
		frame_fd = -1;

		/* reset sinfo struct*/
		sinfo.cols = 0;
	}
#endif
}

/*
 * Return client-side mapped framebuffer info for
 * passed window.  If not running framebuffer, the
 * physpixel and winpixel members will be NULL, and
 * everything else correct.
 */
void
GrGetWindowFBInfo(GR_WINDOW_ID wid, GR_WINDOW_FB_INFO *fbinfo)
{
	int			physoffset;
	GR_WINDOW_INFO		info;
	static int		last_portrait = -1;

	/* re-get screen info on auto-portrait switch*/
	if (sinfo.cols == 0 || last_portrait != sinfo.portrait)
		GrGetScreenInfo(&sinfo);
	last_portrait = sinfo.portrait;

	/* must get window position anew each time*/
	GrGetWindowInfo(wid, &info);

	fbinfo->bpp = sinfo.bpp;
	fbinfo->bytespp = (sinfo.bpp+7)/8;
	fbinfo->pixtype = sinfo.pixtype;
	fbinfo->x = info.x;
	fbinfo->y = info.y;
	fbinfo->portrait_mode = sinfo.portrait;

	switch (fbinfo->portrait_mode) {
	case MWPORTRAIT_RIGHT:
	case MWPORTRAIT_LEFT:
		/*
		 * We reverse coords since Microwindows reports
		 * back the virtual xres/yres, and we want
		 * the physical xres/yres.
		 */
		// FIXME return xres and xvirtres in SCREENINFO?
		fbinfo->xres = sinfo.rows;	/* reverse coords*/
		fbinfo->yres = sinfo.cols;
		break;
	default:
		fbinfo->xres = sinfo.cols;
		fbinfo->yres = sinfo.rows;
		break;
	}
	fbinfo->xvirtres = sinfo.cols;
	fbinfo->yvirtres = sinfo.rows;
	fbinfo->pitch = fbinfo->xres * fbinfo->bytespp;

	/* fill in memory mapped addresses*/
	fbinfo->physpixels = physpixels;

	/* winpixels only valid for non-portrait modes*/
	physoffset = info.y*fbinfo->pitch + info.x*fbinfo->bytespp;
	fbinfo->winpixels = physpixels? (physpixels + physoffset): NULL;
}
