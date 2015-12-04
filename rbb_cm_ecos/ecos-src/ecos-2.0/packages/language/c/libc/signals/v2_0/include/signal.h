#ifndef CYGONCE_LIBC_SIGNALS_SIGNAL_H
#define CYGONCE_LIBC_SIGNALS_SIGNAL_H
//========================================================================
//
//      signal.h
//
//      Definitions for ISO C and POSIX 1003.1 signals
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
// ----------------------------------------------------------------------
// Portions of this software Copyright (c) 2003-2011 Broadcom Corporation
// ----------------------------------------------------------------------
//========================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):     jlarmour
// Contributors:  
// Date:          2000-04-18
// Purpose:       Provides description of interface to ISO C and POSIX 1003.1
//                signal functionality
// Description:   
// Usage:         Do not include this file directly - use #include <signal.h>
//
//####DESCRIPTIONEND####
//
//========================================================================

// CONFIGURATION

#include <pkgconf/libc_signals.h>  // libc signals configuration

// INCLUDES

#include <cyg/infra/cyg_type.h>    // Common type definitions and support

#define CYGSEM_LIBC_SIGNALS_POSIX
#ifdef CYGSEM_LIBC_SIGNALS_POSIX
# include <unistd.h>               // _POSIX_* macros
#endif


// TYPE DEFINITIONS

// Integral type that can be accessed atomically - from ISO C 7.7
typedef cyg_atomic sig_atomic_t;

// Type of signal handler functions
typedef void (*__sighandler_t)(int);

#ifdef CYGSEM_LIBC_SIGNALS_POSIX

// Signal sets from POSIX 1003.1 chap 3.3.3 and other chaps
typedef cyg_uint32 sigset_t;

// POSIX 1003.1 chap 3.3.1.2
union sigval {
    int   sival_int;    // used when application-defined value is an int
    void *sival_ptr;    // used when application-defined value is a pointer
};

// Signal information structure passed to a __siginfoaction_t style handler
// from POSIX 1003.1 chap 3.3.1.3
typedef struct {
    int si_signo;           // signal number
    int si_code;            // cause of signal
    union sigval si_value;  // signal value
} siginfo_t;


// Type of signal handler used if SA_SIGINFO is set in flags to sigaction
// from POSIX 1003.1 chap 3.3.1.3
typedef void (*__siginfoaction_t)(int __signo, siginfo_t *__info,
                                  void *__context);

// struct sigaction describes the action to be taken when we get a signal
// From POSIX 1003.1 chap. 3.3.4.2
struct sigaction {
    sigset_t sa_mask;                   // Additional signals to be blocked
    int sa_flags;                       // Special flags
    union {
        __sighandler_t sa_handler;      // signal handler
        __siginfoaction_t sa_sigaction; // Function to call instead of
                                        // sa_handler if SA_SIGINFO is
                                        // set in sa_flags
    } __sigactionhandler;
#define sa_handler   __sigactionhandler.sa_handler
#define sa_sigaction __sigactionhandler.sa_sigaction
};


#endif // ifdef CYGSEM_LIBC_SIGNALS_POSIX


// CONSTANTS

// Signal handlers for use with signal(). We avoid 0 because in an embedded
// system this may be e.g. start of ROM and thus a possible function pointer
// for reset!

#define SIG_DFL ((__sighandler_t) 1)      // Default action
#define SIG_IGN ((__sighandler_t) 2)      // Ignore action
#define SIG_ERR ((__sighandler_t)-1)      // Error return

// NB. We do not need to restrict SIG* definitions (e.g. by eliminating
// POSIX signals) when using strict ISO C (permitted in 7.7)

#define SIGNULL   0    // Reserved signal - do not use (POSIX 3.3.1.1)
#define SIGHUP    1    // Hangup on controlling terminal (POSIX)
#define SIGINT    2    // Interactive attention (ISO C)
#define SIGQUIT   3    // Interactive termination (POSIX)
#define SIGILL    4    // Illegal instruction (not reset when caught) (ISO C)
#define SIGTRAP   5    // Trace trap (not reset when caught)
#define SIGIOT    6    // IOT instruction
#define SIGABRT   6    // Abnormal termination - used by abort() (ISO C)
#define SIGEMT    7    // EMT instruction
#define SIGFPE    8    // Floating Point Exception e.g. div by 0 (ISO C)
#define SIGKILL   9    // Kill (cannot be caught or ignored) (POSIX)
#if 1 // FIXME - should be #ifdef _POSIX_MEMORY_PROTECTION?
# define SIGBUS   10   // Bus error (POSIX)
#endif
#define SIGSEGV   11   // Invalid memory reference (ISO C)
#define SIGSYS    12   // Bad argument to system call (used by anything?)
#define SIGPIPE   13   // Write on a pipe with no one to read it (POSIX)
#define SIGALRM   14   // Alarm timeout (POSIX)
#define SIGTERM   15   // Software termination request (ISO C)
#define SIGUSR1   16   // Application-defined signal 1 (POSIX)
#define SIGUSR2   17   // Application-defined signal 2 (POSIX)

#define CYGNUM_LIBC_SIGNALS 18  // Maximum signal number + 1

#ifdef _POSIX_JOB_CONTROL
# define SIGCHLD  18   // Child process terminated or stopped (POSIX)
# define SIGCONT  19   // Continue if stopped (POSIX)
# define SIGSTOP  20   // Stop (cannot be caught or ignored) (POSIX)
# define SIGTSTP  21   // Interactive stop (POSIX)
# define SIGTTIN  22   // Terminal read attempted by backgrounded
                       // process (POSIX)
# define SIGTTOU  23   // Terminal write attempted by backgrounded
                       // process (POSIX)
#undef CYGNUM_LIBC_SIGNALS
#define CYGNUM_LIBC_SIGNALS 24
#endif

// FIXME - strictly by POSIX in 3.3.1.1 we should define SIGRTMIN/SIGRTMAX
// But if so, to what value if it _isn't_ supported?!

#ifdef CYGSEM_LIBC_SIGNALS_POSIX
// sa_flag bits in struct sigaction
#define SA_NOCLDSTOP 1   // Don't generate SIGCHLD when children stop
#define SA_SIGINFO   2   // Use the __siginfoaction_t style signal
                         // handler, instead of the single argument handler
#endif // ifdef CYGSEM_LIBC_SIGNALS_POSIX


// FUNCTION PROTOTYPES

#ifdef __cplusplus
extern "C" {
#endif

//===========================================================================

// ISO C functions

//////////////////////////////
// signal() - ISO C 7.7.1   //
//////////////////////////////
//
// Installs a new signal handler for the specified signal, and returns
// the old handler
//

extern __sighandler_t
signal(int __sig, __sighandler_t __handler);

///////////////////////////
// raise() - ISO C 7.7.2 //
///////////////////////////
//
// Raises the signal, which will cause the current signal handler for
// that signal to be called
//

extern int
raise(int __sig);


#ifdef __cplusplus
} // extern "C"
#endif 

#include <cyg/libc/signals/signal.inl>

#endif // CYGONCE_LIBC_SIGNALS_SIGNAL_H multiple inclusion protection

// EOF signal.h
