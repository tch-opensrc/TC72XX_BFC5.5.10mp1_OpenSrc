// ----------------------------------------------------------------------
// Portions of this software Copyright (c) 2003-2011 Broadcom Corporation
// ----------------------------------------------------------------------
//  Filename:   missing.c
//  Author:     Mike Sieweke
//  Created:    August 28, 2003
//
//  Description:
//      This module defines functions and data structures which were included
//      in newlib (used to build Gnu C++ libraries), but missining in the eCos
//      standard C library.

#include <string.h> 
#include <malloc.h>
#include <ctype.h>
#include <sys/reent.h>


//--------------------------------------------------------------------------
// The newlib library uses an impure_data structure to keep track of global
// variables which need to be unique for each thread.
/* Note that there is a copy of this in sys/reent.h.  */
#ifndef __ATTRIBUTE_IMPURE_PTR__
#define __ATTRIBUTE_IMPURE_PTR__
#endif

#ifndef __ATTRIBUTE_IMPURE_DATA__
#define __ATTRIBUTE_IMPURE_DATA__
#endif

static struct _reent __ATTRIBUTE_IMPURE_DATA__ impure_data = _REENT_INIT (impure_data);
struct _reent *__ATTRIBUTE_IMPURE_PTR__ _impure_ptr = &impure_data;


//--------------------------------------------------------------------------
// Define peripheral clock frequency.  This can't be compiled into the library
// because we want to use a single library for multiple 33xx targets with
// potentially different frequencies.
// extern unsigned int PeripheralClockFrequency = 50 * 1000 * 1000;

#include <errno.h>
int __errno()
{
    return errno;
}


//--------------------------------------------------------------------------
// For some unknown reason these string functions are missing the the eCos C
// library.
char *strdup(const char *oldString)
{
    char * newString;
    newString = (char *) malloc( strlen( oldString ) + 1 );
    if (newString != NULL)
    {
        strcpy( newString, oldString );
    }
    return newString;
}

long long strtoll(const char *s, char **endp, int base)
{
    return (long long) strtol( s, endp, base );
}

long long strtoull(const char *s, char **endp, int base)
{
    return (unsigned long long) strtoul( s, endp, base );
}

int finitef ( float x)
{
    return 1;
}


//=========================================================================
// OBSOLETE:
// The _ctype_ array is defined statically in ctype_.c, so we don't need to
// initialize it here.
#if 0
char           _ctype_[257];

#define	_U	01      //upper
#define	_L	02      //lower
#define	_N	04      //number
#define	_S	010     //space
#define _P	020     //punctuation
#define _C	040     //control
#define _X	0100    //hex
#define	_B	0200    //blank

void init_ctype_()
{
    char i;

    // First clear all values...
    for ( i = 0; i < 257; i++ )
    {
        _ctype_[i] = 0;
    }
    // Punctuation.  This should be first so all non-punct characters have
    // their values replaced.
    for ( i = '!'; i <= '~'; i++ )
    {
        (_ctype_+1)[i] = _P;
    }
    // Uppercase characters
    for ( i = 'A'; i <= 'Z'; i++ )
    {
        (_ctype_+1)[i] = _U;
    }
    // Lowercase characters
    for ( i = 'a'; i <= 'z'; i++ )
    {
        (_ctype_+1)[i] = _L;
    }
    // Numbers
    for ( i = '0'; i <= '9'; i++ )
    {
        (_ctype_+1)[i] = _N;
    }
    // Hex letters from a through f
    for ( i = 'A'; i <= 'F'; i++ )
    {
        (_ctype_+1)[i] |= _X;
    }
    for ( i = 'a'; i <= 'f'; i++ )
    {
        (_ctype_+1)[i] |= _X;
    }
    // Space characters
    (_ctype_+1)[' ']  = _S | _B;
    (_ctype_+1)['\t'] = _S;
    (_ctype_+1)['\n'] = _S;
    (_ctype_+1)['\r'] = _S;
    (_ctype_+1)['\f'] = _S;
    (_ctype_+1)['\v'] = _S;
    // Control characters, including escape
    for ( i = 0; i <= 31; i++ )
    {
        (_ctype_+1)[i] = _C;
    }
    (_ctype_+1)[127] = _C;
}
#endif


