#ifndef CYGONCE_LIBC_STDIO_STDIO_H
#define CYGONCE_LIBC_STDIO_STDIO_H
//========================================================================
//
//      stdio.h
//
//      ISO C standard I/O routines - with some POSIX 1003.1 extensions
//
//========================================================================
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
//========================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):     jlarmour
// Contributors:  
// Date:          2000-04-19
// Purpose:       ISO C standard I/O routines
// Description: 
// Usage:         Do not include this file directly - use #include <stdio.h>
//
//####DESCRIPTIONEND####
//
//========================================================================

// CONFIGURATION

#include <pkgconf/libc_stdio.h>   // Configuration header

// INCLUDES

#include <cyg/infra/cyg_type.h> // common type definitions and support
#include <stdarg.h>             // va_list from compiler

// CONSTANTS

// Some of these values are odd to ensure that asserts have better effect
// should spurious values be passed to functions expecting these constants.

// _IOFBF, _IOLBF, and _IONBF specify full, line or no buffering when used
// with setvbuf() - ISO C standard chap 7.9.1

#define _IOFBF      (-2)
#define _IOLBF      (-4)
#define _IONBF      (-8)

// EOF is a macro defined to any negative integer constant - ISO C standard
// chap. 7.9.1
#define EOF         (-1)

// SEEK_CUR, SEEK_END and SEEK_SET are used with fseek() as position
// anchors - ISO C standard chap. 7.9.1
#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2


// TYPE DEFINITIONS

// A type capable of specifying uniquely every file position - ISO C
// standard chap 7.9.1
typedef cyg_ucount32 fpos_t;


// FILE is just cast to an address here. It is uncast internally to the
// C library in stream.hxx  as the C++ Cyg_StdioStream class.
// Optional run-time checking can be enabled to ensure that the cast is
// valid, using the standard assertion functionality.
//
// The array size is irrelevant other than being more than 8, and is present
// to stop references to FILEs being marked as able to be put in the small
// data section. We can't just mark it as in the ".data" section as on some
// targets it may actually be ".common".
typedef CYG_ADDRESS FILE[9999];

// EXTERNAL VARIABLES

// Default file streams for input/output. These only need to be
// expressions, not l-values - ISO C standard chap. 7.9.1
//
// CYGPRI_LIBC_STDIO_NO_DEFAULT_STREAMS is used when initializing
// stdin/out/err privately inside the C library

#ifndef CYGPRI_LIBC_STDIO_NO_DEFAULT_STREAMS
__externC FILE *stdin, *stdout, *stderr;
#endif

// FUNCTION PROTOTYPES

//========================================================================

// ISO C 7.9.5 File access functions

externC int
fclose( FILE * /* stream */ );

externC int
fflush( FILE * /* stream */ );

externC FILE *
fopen( const char * /* filename */, const char * /* mode */ );

externC FILE *
freopen( const char * /* filename */, const char * /* mode */,
         FILE * /* stream */ );

externC void
setbuf( FILE * /* stream */, char * /* buffer */ );

externC int
setvbuf( FILE * /* stream */, char * /* buffer */, int /* mode */,
         size_t /* size */ );

//========================================================================

// ISO C 7.9.6 Formatted input/output functions

externC int
fprintf( FILE * /* stream */, const char * /* format */, ... );

externC int
fscanf( FILE * /* stream */, const char * /* format */, ... );

externC int
printf( const char * /* format */, ... );

externC int
scanf( const char * /* format */, ... );

externC int
sprintf( char * /* str */, const char * /* format */, ... );

externC int
sscanf( const char * /* str */, const char * /* format */, ... );

externC int
vfprintf( FILE * /* stream */, const char * /* format */,
          va_list /* args */ );

externC int
vprintf( const char * /* format */, va_list /* args */ );

externC int
vsprintf( char * /* str */, const char * /* format */,
          va_list /* args */ );

//========================================================================

// ISO C 7.9.7 Character input/output functions

externC int
fgetc( FILE * /* stream */ );

externC char *
fgets( char * /* str */, int /* length */, FILE * /* stream */ );

externC int
fputc( int /* c */, FILE * /* stream */ );

externC int
putc( int /* c */, FILE * /* stream */ );

externC int
putchar( int /* c */ );

externC int
fputs( const char * /* str */, FILE * /* stream */ );

externC char *
gets( char * );

externC int
getc( FILE * /* stream */ );

externC int
getchar( void );

externC int
puts( const char * /* str */ );

externC int
ungetc( int /* c */, FILE * /* stream */ );

// no function equivalent is required for getchar() or putchar(), so we can
// just #define them

#define getchar() fgetc( stdin )

#define putchar(__c) fputc(__c, stdout)

//========================================================================

// ISO C 7.9.8 Direct input/output functions

externC size_t
fread( void * /* ptr */, size_t /* object_size */,
       size_t /* num_objects */, FILE * /* stream */ );

externC size_t
fwrite( const void * /* ptr */, size_t /* object_size */,
        size_t /* num_objects */, FILE * /* stream */ );

//========================================================================

// ISO C 7.9.9 File positioning functions

externC int
fgetpos( FILE * /* stream */, fpos_t * /* pos */ );

externC int
fseek( FILE * /* stream */, long int /* offset */, int /* whence */ );

externC int
fsetpos( FILE * /* stream */, const fpos_t * /* pos */ );

externC long int
ftell( FILE * /* stream */ );

externC void
rewind( FILE * /* stream */ );

//========================================================================

// ISO C 7.9.10 Error-handling functions

externC void
clearerr( FILE * /* stream */ );

externC int
feof( FILE * /* stream */ );

externC int
ferror( FILE * /* stream */ );

externC void
perror( const char * /* prefix_str */ );

//========================================================================

// Other non-ISO C functions

externC int
fnprintf( FILE * /* stream */, size_t /* length */,
          const char * /* format */, ... ) CYGBLD_ATTRIB_PRINTF_FORMAT(3, 4);

externC int
snprintf( char * /* str */, size_t /* length */, const char * /* format */,
          ... ) CYGBLD_ATTRIB_PRINTF_FORMAT(3, 4);

externC int
vfnprintf( FILE * /* stream */, size_t /* length */,
           const char * /* format */, va_list /* args */ );

externC int
vsnprintf( char * /* str */, size_t /* length */,
           const char * /* format */, va_list /* args */ );

externC int
vscanf( const char * /* format */, va_list /* args */ );

externC int
vsscanf( const char * /* str */, const char * /* format */,
         va_list /* args */ );

externC int
vfscanf( FILE * /* stream */, const char * /* format */,
         va_list /* args */ );


// INLINE FUNCTIONS

#ifdef CYGIMP_LIBC_STDIO_INLINES
# include <cyg/libc/stdio/stdio.inl>
#endif

#endif // CYGONCE_LIBC_STDIO_STDIO_H multiple inclusion protection

// EOF stdio.h
