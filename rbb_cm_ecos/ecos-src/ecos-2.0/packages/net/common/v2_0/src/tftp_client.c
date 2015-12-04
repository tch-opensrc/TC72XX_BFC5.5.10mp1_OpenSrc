//==========================================================================
//
//      lib/tftp_client.c
//
//      TFTP client support
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
// Contributors: gthomas
// Date:         2000-04-06
// Purpose:      
// Description:  
//              
//
//####DESCRIPTIONEND####
//
//==========================================================================

// TFTP client support

#include <network.h>
#include <arpa/tftp.h>
#include <tftp_support.h>

#define min(x,y) (x<y ? x : y)

//
// Read a file from a host into a local buffer.  Returns the
// number of bytes actually read, or (-1) if an error occurs.
// On error, *err will hold the reason.
//
int
tftp_get(char *filename,
         struct sockaddr_in *server,
         char *buf,
         int len,
         int mode,
         int *err)
{
    int res = 0;
    int s, actual_len, data_len, recv_len, from_len;
    static int get_port = 7700;
    struct sockaddr_in local_addr, server_addr, from_addr;
    char data[SEGSIZE+sizeof(struct tftphdr)];
    struct tftphdr *hdr = (struct tftphdr *)data;
    char *cp, *fp;
    struct timeval timeout;
    unsigned short last_good_block = 0;
    struct servent *server_info;
    fd_set fds;
    int total_timeouts = 0;

    *err = 0;  // Just in case

    // Create initial request
    hdr->th_opcode = htons(RRQ);  // Read file
    cp = (char *)&hdr->th_stuff;
    fp = filename;
    while (*fp) *cp++ = *fp++;
    *cp++ = '\0';
    if (mode == TFTP_NETASCII) {
        fp = "NETASCII";
    } else if (mode == TFTP_OCTET) {
        fp = "OCTET";
    } else {
        *err = TFTP_INVALID;
        return -1;
    }
    while (*fp) *cp++ = *fp++;
    *cp++ = '\0';
    server_info = getservbyname("tftp", "udp");
    if (server_info == (struct servent *)0) {
        *err = TFTP_NETERR;
        return -1;
    }

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        // Couldn't open a communications channel
        *err = TFTP_NETERR;
        return -1;
    }
    memset((char *)&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_len = sizeof(local_addr);
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(get_port++);
    if (bind(s, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        // Problem setting up my end
        *err = TFTP_NETERR;
        close(s);
        return -1;
    }
    memset((char *)&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_len = sizeof(server_addr);
    server_addr.sin_addr = server->sin_addr;
    if (server->sin_port == 0) {
        server_addr.sin_port = server_info->s_port; // Network order already
    } else {
        server_addr.sin_port = server->sin_port;
    }

    // Send request
    if (sendto(s, data, sizeof(data), 0, 
               (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        // Problem sending request
        *err = TFTP_NETERR;
        close(s);
        return -1;
    }

    // Read data
    fp = buf;
    while (true) {
        timeout.tv_sec = TFTP_TIMEOUT_PERIOD;
        timeout.tv_usec = 0;
        FD_ZERO(&fds);
        FD_SET(s, &fds);
        if (select(s+1, &fds, 0, 0, &timeout) <= 0) {
            if ((++total_timeouts > TFTP_TIMEOUT_MAX) || (last_good_block == 0)) {
                // Timeout - no data received
                *err = TFTP_TIMEOUT;
                close(s);
                return -1;
            }
            // Try resending last ACK
            hdr->th_opcode = htons(ACK);
            hdr->th_block = htons(last_good_block);
            if (sendto(s, data, 4 /* FIXME */, 0, 
                       (struct sockaddr *)&from_addr, from_len) < 0) {
                // Problem sending request
                *err = TFTP_NETERR;
                close(s);
                return -1;
            }
        } else {
            recv_len = sizeof(data);
            from_len = sizeof(from_addr);
            if ((data_len = recvfrom(s, &data, recv_len, 0, 
                                     (struct sockaddr *)&from_addr, &from_len)) < 0) {
                // What happened?
                *err = TFTP_NETERR;
                close(s);
                return -1;
            }
            if (ntohs(hdr->th_opcode) == DATA) {
                actual_len = 0;
                if (ntohs(hdr->th_block) == (last_good_block+1)) {
                    // Consume this data
                    cp = hdr->th_data;
                    data_len -= 4;  /* Sizeof TFTP header */
                    actual_len = data_len;
                    res += actual_len;
                    while (data_len-- > 0) {
                        if (len-- > 0) {
                            *fp++ = *cp++;
                        } else {
                            // Buffer overflow
                            *err = TFTP_TOOLARGE;
                            close(s);
                            return -1;
                        }
                    }
                    last_good_block++;
                }
                // Send out the ACK
                hdr->th_opcode = htons(ACK);
                hdr->th_block = htons(last_good_block);
                if (sendto(s, data, 4 /* FIXME */, 0, 
                           (struct sockaddr *)&from_addr, from_len) < 0) {
                    // Problem sending request
                    *err = TFTP_NETERR;
                    close(s);
                    return -1;
                }
                if ((actual_len >= 0) && (actual_len < SEGSIZE)) {
                    // End of data
                    close(s);
                    return res;
                }
            } else 
            if (ntohs(hdr->th_opcode) == ERROR) {
                *err = ntohs(hdr->th_code);
                close(s);
                return -1;
            } else {
                // What kind of packet is this?
                *err = TFTP_PROTOCOL;
                close(s);
                return -1;
            }
        }
    }
}

//
// Send data to a file on a server via TFTP.
//
int
tftp_put(char *filename,
         struct sockaddr_in *server,
         char *buf,
         int len,
         int mode,
         int *err)
{
    int res = 0;
    int s, actual_len, data_len, recv_len, from_len;
    static int put_port = 7800;
    struct sockaddr_in local_addr, server_addr, from_addr;
    char data[SEGSIZE+sizeof(struct tftphdr)];
    struct tftphdr *hdr = (struct tftphdr *)data;
    char *cp, *fp, *sfp;
    struct timeval timeout;
    unsigned short last_good_block = 0;
    struct servent *server_info;
    fd_set fds;
    int total_timeouts = 0;

    *err = 0;  // Just in case

    server_info = getservbyname("tftp", "udp");
    if (server_info == (struct servent *)0) {
        *err = TFTP_NETERR;
        return -1;
    }

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        // Couldn't open a communications channel
        *err = TFTP_NETERR;
        return -1;
    }
    memset((char *)&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_len = sizeof(local_addr);
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(put_port++);
    if (bind(s, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        // Problem setting up my end
        *err = TFTP_NETERR;
        close(s);
        return -1;
    }
    memset((char *)&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_len = sizeof(server_addr);
    server_addr.sin_addr = server->sin_addr;
    if (server->sin_port == 0) {
        server_addr.sin_port = server_info->s_port; // Network order already
    } else {
        server_addr.sin_port = server->sin_port;
    }

    while (1) {
        // Create initial request
        hdr->th_opcode = htons(WRQ);  // Create/write file
        cp = (char *)&hdr->th_stuff;
        fp = filename;
        while (*fp) *cp++ = *fp++;
        *cp++ = '\0';
        if (mode == TFTP_NETASCII) {
            fp = "NETASCII";
        } else if (mode == TFTP_OCTET) {
            fp = "OCTET";
        } else {
            *err = TFTP_INVALID;
            return -1;
        }
        while (*fp) *cp++ = *fp++;
        *cp++ = '\0';
        // Send request
        if (sendto(s, data, sizeof(data), 0, 
                   (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            // Problem sending request
            *err = TFTP_NETERR;
            close(s);
            return -1;
        }
        // Wait for ACK
        timeout.tv_sec = TFTP_TIMEOUT_PERIOD;
        timeout.tv_usec = 0;
        FD_ZERO(&fds);
        FD_SET(s, &fds);
        if (select(s+1, &fds, 0, 0, &timeout) <= 0) {
            if (++total_timeouts > TFTP_TIMEOUT_MAX) {
                // Timeout - no ACK received
                *err = TFTP_TIMEOUT;
                close(s);
                return -1;
            }
        } else {
            recv_len = sizeof(data);
            from_len = sizeof(from_addr);
            if ((data_len = recvfrom(s, &data, recv_len, 0, 
                                     (struct sockaddr *)&from_addr, &from_len)) < 0) {
                // What happened?
                *err = TFTP_NETERR;
                close(s);
                return -1;
            }
            if (ntohs(hdr->th_opcode) == ACK) {
                // Write request accepted - start sending data
                break;
            } else 
            if (ntohs(hdr->th_opcode) == ERROR) {
                *err = ntohs(hdr->th_code);
                close(s);
                return -1;
            } else {
                // What kind of packet is this?
                *err = TFTP_PROTOCOL;
                close(s);
                return -1;
            }
        }
    }

    // Send data
    sfp = buf;
    last_good_block = 1;
    while (res < len) {
        // Build packet of data to send
        data_len = min(SEGSIZE, len-res);
        hdr->th_opcode = htons(DATA);
        hdr->th_block = htons(last_good_block);
        cp = hdr->th_data;
        fp = sfp;
        actual_len = data_len + 4;
        // FIXME - what about "netascii" data?
        while (data_len-- > 0) *cp++ = *fp++;
        // Send data packet
        if (sendto(s, data, actual_len, 0, 
                   (struct sockaddr *)&from_addr, from_len) < 0) {
            // Problem sending request
            *err = TFTP_NETERR;
            close(s);
            return -1;
        }
        // Wait for ACK
        timeout.tv_sec = TFTP_TIMEOUT_PERIOD;
        timeout.tv_usec = 0;
        FD_ZERO(&fds);
        FD_SET(s, &fds);
        if (select(s+1, &fds, 0, 0, &timeout) <= 0) {
            if (++total_timeouts > TFTP_TIMEOUT_MAX) {
                // Timeout - no data received
                *err = TFTP_TIMEOUT;
                close(s);
                return -1;
            }
        } else {
            recv_len = sizeof(data);
            from_len = sizeof(from_addr);
            if ((data_len = recvfrom(s, &data, recv_len, 0, 
                                     (struct sockaddr *)&from_addr, &from_len)) < 0) {
                // What happened?
                *err = TFTP_NETERR;
                close(s);
                return -1;
            }
            if (ntohs(hdr->th_opcode) == ACK) {
                if (ntohs(hdr->th_block) == last_good_block) {
                    // Advance pointers, etc
                    sfp = fp;
                    res += (actual_len - 4);
                    last_good_block++;
                } else {
                    diag_printf("Send block #%d, got ACK for #%d\n", 
                                last_good_block, ntohs(hdr->th_block));
                }
            } else 
            if (ntohs(hdr->th_opcode) == ERROR) {
                *err = ntohs(hdr->th_code);
                close(s);
                return -1;
            } else {
                // What kind of packet is this?
                *err = TFTP_PROTOCOL;
                close(s);
                return -1;
            }
        }
    }
    close(s);
    return res;
}

// EOF tftp_client.c
