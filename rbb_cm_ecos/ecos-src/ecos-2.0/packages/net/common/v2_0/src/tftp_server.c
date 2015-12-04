//==========================================================================
//
//      lib/tftp_server.c
//
//      TFTP server support
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
// Author(s):    gthomas
// Contributors: gthomas, hmt, andrew.lunn@ascom.ch (Andrew Lunn)
// Date:         2000-04-06
// Purpose:      
// Description:  
//              
//
//####DESCRIPTIONEND####
//
//==========================================================================

// TFTP server support

#include <network.h>          // Basic networking support
#include <arpa/tftp.h>        // TFTP protocol definitions
#include <tftp_support.h>     // TFTPD structures
#include <cyg/kernel/kapi.h>
#include <stdlib.h>           // For malloc

#define nCYGOPT_NET_TFTP_SERVER_INSTRUMENT
#ifdef CYGOPT_NET_TFTP_SERVER_INSTRUMENT

struct subinfo {
    int rx;
    int rx_repeat;
    int rx_skip;
    int send;
    int resend;
};

struct info {
    struct subinfo ack, data;
    int err_send;
    int total_transactions;
};


static struct info tftp_server_instrument = {
    { 0,0,0,0,0 },
    { 0,0,0,0,0 },
    0, 0,
};

#endif // CYGOPT_NET_TFTP_SERVER_INSTRUMENT

#define STACK_SIZE (((CYGNUM_HAL_STACK_SIZE_TYPICAL+(3*(SEGSIZE+sizeof(struct tftphdr)))) + CYGARC_ALIGNMENT-1) & ~(CYGARC_ALIGNMENT-1))
static char *TFTP_tag = "TFTPD";
struct tftp_server {
    char                 *tag;
    char                  stack[STACK_SIZE];
    cyg_thread            thread_data;
    cyg_handle_t          thread_handle;
    int                   port;
    struct tftpd_fileops *ops;
};

static char * errmsg[] = {
  "Undefined error code",               // 0 nothing defined
  "File not found",                     // 1 TFTP_ENOTFOUND 
  "Access violation",                   // 2 TFTP_EACCESS   
  "Disk full or allocation exceeded",   // 3 TFTP_ENOSPACE  
  "Illegal TFTP operation",             // 4 TFTP_EBADOP    
  "Unknown transfer ID",                // 5 TFTP_EBADID    
  "File already exists",                // 6 TFTP_EEXISTS   
  "No such user",                       // 7 TFTP_ENOUSER   
};

/* Send an error packet to the client */
static void 
tftpd_send_error(int s, struct tftphdr * reply, int err,
		 struct sockaddr_in *from_addr, int from_len)
{
    CYG_ASSERT( 0 <= err, "err underflow" );
    CYG_ASSERT( sizeof(errmsg)/sizeof(errmsg[0]) > err, "err overflow" );

    reply->th_opcode = htons(ERROR);
    reply->th_code = htons(err);
    if ( (0 > err) || (sizeof(errmsg)/sizeof(errmsg[0]) <= err) )
        err = 0; // Do not copy a random string from hyperspace
    strcpy(reply->th_msg, errmsg[err]);
    sendto(s, reply, 4+strlen(reply->th_msg)+1, 0, 
	   (struct sockaddr *)from_addr, from_len);
}

//
// Receive a file from the client
//
static void
tftpd_write_file(struct tftp_server *server,
                 struct tftphdr *hdr, 
                 struct sockaddr_in *from_addr, int from_len)
{
    char data_out[SEGSIZE+sizeof(struct tftphdr)];
    char data_in[SEGSIZE+sizeof(struct tftphdr)];
    struct tftphdr *reply = (struct tftphdr *)data_out;
    struct tftphdr *response = (struct tftphdr *)data_in;
    int fd, len, ok, tries, closed, data_len, s;
    unsigned short block;
    struct timeval timeout;
    fd_set fds;
    int total_timeouts = 0;
    struct sockaddr_in client_addr, local_addr;
    int client_len;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        diag_printf("TFTPD: can't open socket for 'write_file'\n");
        return;
    }
    memset((char *)&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_len = sizeof(local_addr);
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(INADDR_ANY);
    if (bind(s, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        // Problem setting up my end
        diag_printf("TFTPD: can't bind to reply port for 'write_file'\n");
        close(s);
        return;
    }
    if ((fd = (server->ops->open)(hdr->th_stuff, O_WRONLY)) < 0) {
        tftpd_send_error(s,reply,TFTP_ENOTFOUND,from_addr, from_len);
        close(s);
        return;
    }
    ok = true;
    closed = false;
    block = 0;
    while (ok) {
        // Send ACK telling client he can send data
        reply->th_opcode = htons(ACK);
        reply->th_block = htons(block++); // postincrement
        for (tries = 0;  tries < TFTP_RETRIES_MAX;  tries++) {
#ifdef CYGOPT_NET_TFTP_SERVER_INSTRUMENT
            tftp_server_instrument.ack.send++;
#endif
            sendto(s, reply, 4, 0, (struct sockaddr *)from_addr, from_len);
        repeat_select:
            timeout.tv_sec = TFTP_TIMEOUT_PERIOD;
            timeout.tv_usec = 0;
            FD_ZERO(&fds);
            FD_SET(s, &fds);
            if (select(s+1, &fds, 0, 0, &timeout) <= 0) {
                if (++total_timeouts > TFTP_TIMEOUT_MAX) {
#ifdef CYGOPT_NET_TFTP_SERVER_INSTRUMENT
                    tftp_server_instrument.err_send++;
#endif
                    tftpd_send_error(s,reply,TFTP_EBADOP,from_addr, from_len);
                    ok = false;
                    break;
                }
#ifdef CYGOPT_NET_TFTP_SERVER_INSTRUMENT
                tftp_server_instrument.ack.resend++;
#endif
                continue; // retry the send, using up one retry.
            }
            // Some data has arrived
            data_len = sizeof(data_in);
            client_len = sizeof(client_addr);
            if ((data_len = recvfrom(s, data_in, data_len, 0, 
                      (struct sockaddr *)&client_addr, &client_len)) < 0) {
                // What happened?  No data here!
#ifdef CYGOPT_NET_TFTP_SERVER_INSTRUMENT
                tftp_server_instrument.ack.resend++;
#endif
                continue; // retry the send, using up one retry.
            }
            if (ntohs(response->th_opcode) == DATA &&
                ntohs(response->th_block) < block) {
#ifdef CYGOPT_NET_TFTP_SERVER_INSTRUMENT
                tftp_server_instrument.data.rx_repeat++;
#endif
                // Then it is repeat DATA with an old block; listen again,
                // but do not repeat sending the current ack, and do not
                // use up a retry count.  (we do re-send the ack if
                // subsequently we time out)
                goto repeat_select;
            }
            if (ntohs(response->th_opcode) == DATA &&
                ntohs(response->th_block) == block) {
#ifdef CYGOPT_NET_TFTP_SERVER_INSTRUMENT
                tftp_server_instrument.data.rx++;
#endif
                // Good data - write to file
                len = (server->ops->write)(fd, response->th_data, data_len-4);
                if (len < (data_len-4)) {
                    // File is "full"
                    tftpd_send_error(s,reply,TFTP_ENOSPACE,
                                     from_addr, from_len);     
                    ok = false;  // Give up
                    break; // out of the retries loop
                }
                if (data_len < (SEGSIZE+4)) {
                    // End of file
                    closed = true;
                    ok = false;
                    if ((server->ops->close)(fd) == -1) {
                        tftpd_send_error(s,reply,TFTP_EACCESS,
                                         from_addr, from_len);
                        break;  // out of the retries loop
                    }
                    // Exception to the loop structure: we must ACK the last
                    // packet, the one that implied EOF:
#ifdef CYGOPT_NET_TFTP_SERVER_INSTRUMENT
                    tftp_server_instrument.ack.send++;
#endif
                    reply->th_opcode = htons(ACK);
                    reply->th_block = htons(block++); // postincrement
                    sendto(s, reply, 4, 0, (struct sockaddr *)from_addr, from_len);
                    break; // out of the retries loop
                }
                // Happy!  Break out of the retries loop.
                break;
            }
            // Otherwise, we got something we do not understand!  So repeat
            // sending the current ACK, and use up a retry count.
#ifdef CYGOPT_NET_TFTP_SERVER_INSTRUMENT
            if ( (ntohs(response->th_opcode) == DATA) )
                tftp_server_instrument.data.rx_skip++;
            tftp_server_instrument.ack.resend++;
#endif
        } // End of the retries loop.
        if (TFTP_RETRIES_MAX <= tries) {
#ifdef CYGOPT_NET_TFTP_SERVER_INSTRUMENT
            tftp_server_instrument.err_send++;
#endif
            tftpd_send_error(s,reply,TFTP_EBADOP,from_addr, from_len);
            ok = false;
        }
    }
    close(s);
    if (!closed) {
      (server->ops->close)(fd);
    }
}

//
// Send a file to the client
//
static void
tftpd_read_file(struct tftp_server *server,
                struct tftphdr *hdr, 
                struct sockaddr_in *from_addr, int from_len)
{
    char data_out[SEGSIZE+sizeof(struct tftphdr)];
    char data_in[SEGSIZE+sizeof(struct tftphdr)];
    struct tftphdr *reply = (struct tftphdr *)data_out;
    struct tftphdr *response = (struct tftphdr *)data_in;
    int fd, len, tries, ok, data_len, s;
    unsigned short block;
    struct timeval timeout;
    fd_set fds;
    int total_timeouts = 0;
    struct sockaddr_in client_addr, local_addr;
    int client_len;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        diag_printf("TFTPD: can't open socket for 'read_file'\n");
        return;
    }
    memset((char *)&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_len = sizeof(local_addr);
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(INADDR_ANY);
    if (bind(s, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        // Problem setting up my end
        diag_printf("TFTPD: can't bind to reply port for 'read_file'\n");
        close(s);
        return;
    }
    if ((fd = (server->ops->open)(hdr->th_stuff, O_RDONLY)) < 0) {
        tftpd_send_error(s,reply,TFTP_ENOTFOUND,from_addr, from_len);
        close(s);
        return;
    }
    block = 0;
    ok = true;
    while (ok) {
        // Read next chunk of file
        len = (server->ops->read)(fd, reply->th_data, SEGSIZE);
        reply->th_block = htons(++block); // preincrement
        reply->th_opcode = htons(DATA);
        for (tries = 0;  tries < TFTP_RETRIES_MAX;  tries++) {
#ifdef CYGOPT_NET_TFTP_SERVER_INSTRUMENT
            tftp_server_instrument.data.send++;
#endif
            if (sendto(s, reply, 4+len, 0,
                       (struct sockaddr *)from_addr, from_len) < 0) {
                // Something went wrong with the network!
                ok = false;
                break;
            }
        repeat_select:
            timeout.tv_sec = TFTP_TIMEOUT_PERIOD;
            timeout.tv_usec = 0;
            FD_ZERO(&fds);
            FD_SET(s, &fds);
            if (select(s+1, &fds, 0, 0, &timeout) <= 0) {
                if (++total_timeouts > TFTP_TIMEOUT_MAX) {
#ifdef CYGOPT_NET_TFTP_SERVER_INSTRUMENT
                    tftp_server_instrument.err_send++;
#endif
 		    tftpd_send_error(s,reply,TFTP_EBADOP,from_addr, from_len);
                    ok = false;
                    break;
                }
#ifdef CYGOPT_NET_TFTP_SERVER_INSTRUMENT
                tftp_server_instrument.data.resend++;
#endif
                continue; // retry the send, using up one retry.
            }
            data_len = sizeof(data_in);
            client_len = sizeof(client_addr);
            if ((data_len = recvfrom(s, data_in, data_len, 0, 
                                     (struct sockaddr *)&client_addr,
                                     &client_len)) < 0) {
                // What happened?  Maybe someone lied to us...
#ifdef CYGOPT_NET_TFTP_SERVER_INSTRUMENT
                tftp_server_instrument.data.resend++;
#endif
                continue; // retry the send, using up one retry.
            }
            if ((ntohs(response->th_opcode) == ACK) &&
                (ntohs(response->th_block) < block)) {
#ifdef CYGOPT_NET_TFTP_SERVER_INSTRUMENT
                tftp_server_instrument.ack.rx_repeat++;
#endif
                // Then it is a repeat ACK for an old block; listen again,
                // but do not repeat sending the current block, and do not
                // use up a retry count.  (we do re-send the data if
                // subsequently we time out)
                goto repeat_select;
            }
            if ((ntohs(response->th_opcode) == ACK) &&
                (ntohs(response->th_block) == block)) {
#ifdef CYGOPT_NET_TFTP_SERVER_INSTRUMENT
                tftp_server_instrument.ack.rx++;
#endif
                // Happy!  Break out of the retries loop.
                break;
            }
            // Otherwise, we got something we do not understand!  So repeat
            // sending the current block, and use up a retry count.
#ifdef CYGOPT_NET_TFTP_SERVER_INSTRUMENT
            if ( (ntohs(response->th_opcode) == ACK) )
                tftp_server_instrument.ack.rx_skip++;
            tftp_server_instrument.data.resend++;
#endif
        } // End of the retries loop.
        if (TFTP_RETRIES_MAX <= tries) {
#ifdef CYGOPT_NET_TFTP_SERVER_INSTRUMENT
            tftp_server_instrument.err_send++;
#endif
            tftpd_send_error(s,reply,TFTP_EBADOP,from_addr, from_len);
            ok = false;
        }
        if (len < SEGSIZE) {
            break; // That's end of file then.
        }
    }
    close(s);
    (server->ops->close)(fd);
}

//
// Actual TFTP server
//
#define CYGSEM_TFTP_SERVER_MULTITHREADED
#ifdef CYGSEM_TFTP_SERVER_MULTITHREADED
static cyg_sem_t tftp_server_sem;
#endif

static void
tftpd_server(cyg_addrword_t p)
{
    struct tftp_server *server = (struct tftp_server *)p;
    int s;
    int data_len, recv_len, from_len;
    struct sockaddr_in local_addr, from_addr;
    struct servent *server_info;
    char data[SEGSIZE+sizeof(struct tftphdr)];
    struct tftphdr *hdr = (struct tftphdr *)data;

#ifndef CYGPKG_NET_TESTS_USE_RT_TEST_HARNESS
    // Otherwise routine printfs fail the test - interrupts disabled too long.
    diag_printf("TFTPD [%x]: port %d\n", p, server->port);
#endif

    // Set up port
    if (server->port == 0) {
        server_info = getservbyname("tftp", "udp");
        if (server_info == (struct servent *)0) {
            diag_printf("TFTPD: can't get TFTP service information\n");
            return;
        }
        // Network order in info; host order in server:
        server->port = ntohs( server_info->s_port );
    }

    while (true) {
#ifdef CYGOPT_NET_TFTP_SERVER_INSTRUMENT
        struct info o = tftp_server_instrument;
#endif
        // Create socket
        s = socket(AF_INET, SOCK_DGRAM, 0);
        if (s < 0) {
            diag_printf("TFTPD [%x]: can't open socket\n", p);
            return;
        }
        memset((char *)&local_addr, 0, sizeof(local_addr));
        local_addr.sin_family = AF_INET;
        local_addr.sin_len = sizeof(local_addr);
        local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        local_addr.sin_port = htons(server->port);
        if (bind(s, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
            // Problem setting up my end
            close(s);
#ifdef CYGSEM_TFTP_SERVER_MULTITHREADED
#ifndef CYGPKG_NET_TESTS_USE_RT_TEST_HARNESS
            diag_printf("TFTPD [%x]: waiting to bind to service port\n", p);
#endif
            // Wait until the socket is free...
            cyg_semaphore_wait( &tftp_server_sem );
            continue; // try re-opening and rebinding the socket.
#else
            diag_printf("TFTPD [%x]: can't bind to service port\n", p);
            return;
#endif
        }

        recv_len = sizeof(data);
        from_len = sizeof(from_addr);
        data_len = recvfrom(s, hdr, recv_len, 0,
                            (struct sockaddr *)&from_addr, &from_len);
        close(s); // so that other servers can bind to the TFTP socket
#ifdef CYGSEM_TFTP_SERVER_MULTITHREADED
        // The socket is free...
        cyg_semaphore_post( &tftp_server_sem );
#endif

        if ( data_len < 0) {
            diag_printf("TFTPD [%x]: can't read request\n", p);
        } else {
#ifndef CYGPKG_NET_TESTS_USE_RT_TEST_HARNESS
            diag_printf("TFTPD [%x]: received %x from %s:%d\n", p,
                        ntohs(hdr->th_opcode), inet_ntoa(from_addr.sin_addr),
                        from_addr.sin_port);
#endif
            switch (ntohs(hdr->th_opcode)) {
            case WRQ:
                tftpd_write_file(server, hdr, &from_addr, from_len);
                break;
            case RRQ:
                tftpd_read_file(server, hdr, &from_addr, from_len);
                break;
            case ACK:
            case DATA:
            case ERROR:
                // Ignore
                break;
            default:
                diag_printf("TFTPD [%x]: bogus request %x from %s:%d\n", p,
                            ntohs(hdr->th_opcode),
                            inet_ntoa(from_addr.sin_addr),
                            from_addr.sin_port );
                tftpd_send_error(s,hdr,TFTP_EBADOP,&from_addr,from_len);
            }
        }
#ifdef CYGOPT_NET_TFTP_SERVER_INSTRUMENT
        tftp_server_instrument.total_transactions++;

        o.data.rx        -= tftp_server_instrument.data.rx       ;
        o.data.rx_repeat -= tftp_server_instrument.data.rx_repeat;
        o.data.rx_skip   -= tftp_server_instrument.data.rx_skip  ;
        o.data.send      -= tftp_server_instrument.data.send     ;
        o.data.resend    -= tftp_server_instrument.data.resend   ;

        o.ack.rx         -= tftp_server_instrument.ack.rx        ;
        o.ack.rx_repeat  -= tftp_server_instrument.ack.rx_repeat ;
        o.ack.rx_skip    -= tftp_server_instrument.ack.rx_skip   ;
        o.ack.send       -= tftp_server_instrument.ack.send      ;
        o.ack.resend     -= tftp_server_instrument.ack.resend    ;

        o.err_send       -= tftp_server_instrument.err_send      ;

#ifndef CYGPKG_NET_TESTS_USE_RT_TEST_HARNESS
        if ( o.data.rx        ) diag_printf( "data rx       %4d\n", -o.data.rx        );
        if ( o.data.rx_repeat ) diag_printf( "data rx_repeat%4d\n", -o.data.rx_repeat );
        if ( o.data.rx_skip   ) diag_printf( "data rx_skip  %4d\n", -o.data.rx_skip   );
        if ( o.data.send      ) diag_printf( "data send     %4d\n", -o.data.send      );
        if ( o.data.resend    ) diag_printf( "data resend   %4d\n", -o.data.resend    );

        if ( o.ack.rx        ) diag_printf( " ack rx       %4d\n", -o.ack.rx        );
        if ( o.ack.rx_repeat ) diag_printf( " ack rx_repeat%4d\n", -o.ack.rx_repeat );
        if ( o.ack.rx_skip   ) diag_printf( " ack rx_skip  %4d\n", -o.ack.rx_skip   );
        if ( o.ack.send      ) diag_printf( " ack send     %4d\n", -o.ack.send      );
        if ( o.ack.resend    ) diag_printf( " ack resend   %4d\n", -o.ack.resend    );

        if ( o.err_send      ) diag_printf( "*error sends  %4d\n", -o.err_send      );
#endif // CYGPKG_NET_TESTS_USE_RT_TEST_HARNESS
#endif // CYGOPT_NET_TFTP_SERVER_INSTRUMENT
    }
}

//
// This function is used to create a new server [thread] which supports
// the TFTP protocol on the given port.  A server 'id' will be returned
// which can later be used to destroy the server.  
//
// Note: all [memory] resources for the server thread will be allocated
// dynamically.  If there are insufficient resources, an error will be
// returned.
//
int 
tftpd_start(int port, struct tftpd_fileops *ops)
{
    struct tftp_server *server;
#ifdef CYGSEM_TFTP_SERVER_MULTITHREADED
    static char init = 0;
    if ( 0 == init ) {
        init++;
        cyg_semaphore_init( &tftp_server_sem, 0 );
    }
#endif

    if ((server = malloc(sizeof(struct tftp_server)))) {
        server->tag = TFTP_tag;
        server->port = port;
        server->ops = ops;
        cyg_thread_create(CYGPKG_NET_TFTPD_THREAD_PRIORITY, // Priority
                          tftpd_server,              // entry
                          (cyg_addrword_t)server,    // entry parameter
                          "TFTP server",             // Name
                          &server->stack[0],         // Stack
                          STACK_SIZE,                // Size
                          &server->thread_handle,    // Handle
                          &server->thread_data       // Thread data structure
            );
        cyg_thread_resume(server->thread_handle);  // Start it

    }
    return (int)server;
}

//
// Destroy a TFTP server, using a previously created server 'id'.
//
int 
tftpd_stop(int p)
{
    struct tftp_server *server = (struct tftp_server *)p;
    // Simple sanity check
    if (server->tag == TFTP_tag) {
        cyg_thread_kill(server->thread_handle);
        cyg_thread_set_priority(server->thread_handle, 0);
        cyg_thread_delay(1);  // Make sure it gets to die...
        if (cyg_thread_delete(server->thread_handle)) {
            // Success shutting down the thread
            free(server);  // Give up memory
            return 1;
        }
    }
    return 0;
}

// EOF tftp_server.c
