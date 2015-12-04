//****************************************************************************
//
// Copyright (c) 2003-2010 Broadcom Corporation
//
// This file being released as part of eCos, the Embedded Configurable Operating System.
//
// eCos is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License version 2, available at 
// http://www.broadcom.com/licenses/GPLv2.php (the "GPL").
//
// As a special exception, if you compile this file and link it
// with other works to produce a work based on this file, this file does not
// by itself cause the resulting work to be covered by the GNU General Public
// License. However the source code for this file must still be made available
// in accordance with section (3) of the GNU General Public License.
//
//****************************************************************************

//  $Id$
//
//  Filename:       taskinfo.c
//  Author:         Mike Sieweke (originally in C++ by Dannie Gay)
//  Creation Date:  February 13, 2004
//
//****************************************************************************
//  Description:
//      Define support functions to print basic task info.
//
//****************************************************************************

#include <cyg/kernel/kapi.h>
#include <stdio.h>                 // All the stdio functions
#include <stdlib.h>

void eCosTaskShow(void);
void eCosTaskInfo(int taskIdSel);
void eCosStackShow(void);
void hal_print_saved_info( void *exc_regs );

/// void eCosTaskShow()
///
/// Displays system task information
///
/// Parameters:
///      none.
///
/// Returns:  Nothing.
///
void eCosTaskShow(void)
{
   cyg_handle_t thread  = 0;
   cyg_uint16   id      = 0;
   char         taskName[32];

   printf( "  TaskId               TaskName              Priority   State\n" );
   printf( "---------- --------------------------------  --------  --------\n");

   while (cyg_thread_get_next( &thread, &id ))
   {
      cyg_thread_info info;

      if (!cyg_thread_get_info( thread, id, &info ))
         break;

      if (info.name)
      {
         strcpy(taskName, info.name);
      }
      else
      {
         strcpy(taskName, "----");
      }

      // Translate the state into a string.
      char * state_string;

      if ( info.state == 0 )
         state_string = "RUN";
      else if ( info.state & 0x04 )
         state_string = "SUSP";
      else switch ( info.state & 0x1b )
         {
            case 0x01: 
               state_string = "SLEEP"; 
               break;
            case 0x02: 
               state_string = "CNTSLEEP"; 
               break;
            case 0x08: 
               state_string = "CREATE"; 
               break;
            case 0x10: 
               state_string = "EXIT"; 
               break;
            default: 
               state_string = "????"; 
         }

      printf( "%08x %32s %8d    %s\n", thread, taskName, info.cur_pri, state_string );
   }
}


/// void eCosTaskInfo()
///
/// Displays specific system task information
///
/// Parameters:
///      int taskIdSel - ID of task (from taskShow output)
///  
/// Returns:  Nothing.
///
void eCosTaskInfo(int taskIdSel)
{
   cyg_handle_t thread  = 0;
   cyg_uint16   id      = 0;
   cyg_uint32   taskId;
   char         taskName[32];

   while (cyg_thread_get_next( &thread, &id ))
   {

      // found our selected task, get the info
      if (taskIdSel == (int) thread)
      {
         cyg_thread_info  info;
         char            *state_string;

         if (!cyg_thread_get_info( thread, id, &info ))
            break;

         hal_print_saved_info( (void *) info.current_stack );
         taskId = info.id;

         if (info.name)
         {
            strcpy(taskName, info.name);
         }
         else
         {
            strcpy(taskName, "----");
         }

         // Translate the state into a string.

         if ( info.state == 0 )
            state_string = "RUN";
         else if ( info.state & 0x04 )
            state_string = "SUSP";
         else switch ( info.state & 0x1b )
            {
               case 0x01: 
                  state_string = "SLEEP"; 
                  break;
               case 0x02: 
                  state_string = "CNTSLEEP"; 
                  break;
               case 0x08: 
                  state_string = "CREATE"; 
                  break;
               case 0x10: 
                  state_string = "EXIT"; 
                  break;
               default: 
                  state_string = "????"; 
            }

         printf( "\nTask: %s", taskName );
         printf( "\n---------------------------------------------------" );
         printf( "\nID:               0x%04x",  taskId );
         printf( "\nHandle:           0x%08x",  thread );
         printf( "\nSet Priority:     %d",      info.set_pri );
         printf( "\nCurrent Priority: %d",      info.cur_pri );
         printf( "\nState:            %s",      state_string);
         printf( "\nStack Base:       0x%08x",  info.stack_base );
         printf( "\nStack Size:       %d bytes", info.stack_size );

#ifdef CYGFUN_KERNEL_THREADS_STACK_MEASUREMENT
         printf( "\nStack Used:       %d bytes", info.stack_used );
#endif

         printf( "\n" );

      }
   }
}


/// void eCosStackShow()
///
/// Displays specific system stack information
///
/// Parameters: None
///        
/// Returns:  Nothing.
///
void eCosStackShow(void)
{
   cyg_handle_t thread  = 0;
   cyg_uint16   id      = 0;
   char         taskName[32];
   char        *highFlag;
      
   printf( "                                                                   Stack     Stack    Stack\n" );
   printf( "  TaskId               TaskName              Priority   State      Size      Used     Margin\n" );
   printf( "---------- --------------------------------  --------  --------  --------  --------  --------\n" );

   while (cyg_thread_get_next( &thread, &id ))
   {
      cyg_thread_info   info;
      char             *state_string;

      if (!cyg_thread_get_info( thread, id, &info ))
         break;

      if (info.name)
      {
         strcpy(taskName, info.name);
      }
      else
      {
         strcpy(taskName, "----");
      }

      // Translate the state into a string.

      if ( info.state == 0 )
         state_string = "RUN";
      else if ( info.state & 0x04 )
         state_string = "SUSP";
      else switch ( info.state & 0x1b )
         {
            case 0x01: 
               state_string = "SLEEP"; 
               break;
            case 0x02: 
               state_string = "CNTSLEEP"; 
               break;
            case 0x08: 
               state_string = "CREATE"; 
               break;
            case 0x10: 
               state_string = "EXIT"; 
               break;
            default: 
               state_string = "????"; 
         }
      
      highFlag = "";
#ifdef CYGFUN_KERNEL_THREADS_STACK_MEASUREMENT
      if (info.stack_used + 16 >= info.stack_size)
          highFlag = " OVERFLOW";   
#endif

      printf( "0x%08x %32s %8d     %8s %9d", thread, taskName, info.cur_pri, state_string, info.stack_size );
#ifdef CYGFUN_KERNEL_THREADS_STACK_MEASUREMENT
      printf( "%8d %10d %s",  info.stack_used, info.stack_size - info.stack_used, highFlag );

#endif

      printf( "\n" );
   }

   printf( "Done!\n" );
}


