//==========================================================================
//
//      ftpclient.c
//
//      A simple FTP client
//
//==========================================================================
//####ECOSGPLCOPYRIGHTBEGIN####
// -------------------------------------------
// This file is part of eCos, the Embedded Configurable Operating System.
// Copyright (C) 1998, 1999, 2000, 2001, 2002 Red Hat, Inc.
// Copyright (C) 2002 Andrew Lunn.
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
// Author(s):    andrew.lunn@ascom.ch
// Contributors: andrew.lunn@ascom.ch
// Date:         2001-11-4
// Purpose:      
// Description:  
//              
//####DESCRIPTIONEND####
//
//==========================================================================

#include <pkgconf/system.h>

#include <network.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include "ftpclient.h"

/* Build one command to send to the FTP server */
static int 
build_cmd(char *buf,
          unsigned bufsize,
          char *cmd, 
          char *arg1)
{
  int cnt;
  
  if (arg1) {
    cnt = snprintf(buf,bufsize,"%s %s\r\n",cmd,arg1);
  } else {
    cnt = snprintf(buf,bufsize,"%s\r\n",cmd);
  }
  
  if (cnt < bufsize) 
    return 1;
  
  return 0;
}


/* Read one line from the server, being careful not to overrun the
   buffer. If we do reach the end of the buffer, discard the rest of
   the line. */
static int 
get_line(int s, char *buf, unsigned buf_size,ftp_printf_t ftp_printf) {
  
  int eol = 0;
  int cnt = 0;
  int ret;
  char c;
  
  while(!eol) {
    ret = read(s,&c,1);
    
    if (ret != 1) {
      ftp_printf(1,"read %s\n",strerror(errno));
      return FTP_BAD;
    }
    
    if (c == '\n') {
      eol = 1;
      continue;
    }

    if (cnt < buf_size) {
      buf[cnt++] = c;
    }   
  }
  if (cnt < buf_size) {
    buf[cnt++] = '\0';
  } else {
    buf[buf_size -1] = '\0';
  }
  return 0;
}  

/* Read the reply from the server and return the MSB from the return
   code. This gives us a basic idea if the command failed/worked. The
   reply can be spread over multiple lines. When this happens the line
   will start with a - to indicate there is more*/
static int 
get_reply(int s,ftp_printf_t ftp_printf) {

  char buf[BUFSIZ];
  int more = 0;
  int ret;
  int first_line=1;
  int code=0;
  
  do {
    
    if ((ret=get_line(s,buf,sizeof(buf),ftp_printf)) < 0) {
      return(ret);
    }
    
    ftp_printf(0,"FTP: %s\n",buf);
    
    if (first_line) {
      code = strtoul(buf,NULL,0);
      first_line=0;
      more = (buf[3] == '-');
    } else {
      if (isdigit(buf[0]) && isdigit(buf[1]) && isdigit(buf[2]) &&
          (code == strtoul(buf,NULL,0)) && 
          buf[3]==' ') {
        more=0;
      } else {
        more =1;
      }
    }
  } while (more);

  return (buf[0] - '0');
}

/* Send a command to the server */
static int 
send_cmd(int s,char * msgbuf,ftp_printf_t ftp_printf) {
  
  int len;
  int slen = strlen(msgbuf);
  
  if ((len = write(s,msgbuf,slen)) != slen) {
    if (slen < 0) {
      ftp_printf(1,"write %s\n",strerror(errno));
      return FTP_BAD;
    } else {
      ftp_printf(1,"write truncasted!\n");
      return FTP_BAD;
    }
  }
  return 1;
}

/* Send a complete command to the server and receive the reply. Return the 
   MSB of the reply code. */
static int 
command(char * cmd, 
        char * arg, 
        int s, 
        char *msgbuf, 
        int msgbuflen,
        ftp_printf_t ftp_printf) {

  int err;
  
  if (!build_cmd(msgbuf,msgbuflen,cmd,arg)) {
    ftp_printf(1,"FTP: %s command to long\n",cmd);
    return FTP_BAD;
  }

  ftp_printf(0,"FTP: Sending %s command\n",cmd);
  
  if ((err=send_cmd(s,msgbuf,ftp_printf)) < 0) {
    return(err);
  }

  return (get_reply(s,ftp_printf));
}

/* Open a socket and connect it to the server. Also print out the
   address of the server for debug purposes.*/

static int
connect_to_server(char *hostname, 
                  struct sockaddr_in * local,
                  ftp_printf_t ftp_printf) 
{ 
  struct sockaddr_in host; 
  struct servent *sent;
#ifdef CYGPKG_NS_DNS   
  struct hostent *hp=NULL; 
#endif
  int s, len;

  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) {
    ftp_printf(1,"socket: %s\n",strerror(errno));
    return FTP_BAD;
  }
  
  sent = getservbyname("ftp", "tcp");
  if (sent == (struct servent *)0) {
    ftp_printf(1,"FTP: unknown serivice\n");
    close(s);
    return FTP_BAD;
  }

#ifdef CYGPKG_NS_DNS  
  hp = gethostbyname(hostname);


  if (hp) {           /* try name first */
    host.sin_family = hp->h_addrtype;
    bcopy(hp->h_addr, &host.sin_addr, hp->h_length);
  } else 
#endif
    {			/* maybe it's a numeric address ?*/
    host.sin_family = AF_INET;
    
    if (inet_aton(hostname,&host.sin_addr) == 0)  { 
      ftp_printf(1,"host not found: %s\n", hostname);
      close (s);
      return FTP_NOSUCHHOST;
    }
  }
  
  host.sin_port = sent->s_port;
  
  if (connect(s, (struct sockaddr *)&host, sizeof(host)) < 0) {
    ftp_printf(1,"FTP Connect failed: %s\n",strerror(errno));
    close (s);
    return FTP_NOSUCHHOST;
  }
  
  len = sizeof(struct sockaddr_in);
  if (getsockname(s, (struct sockaddr *)local, &len) < 0) {
    ftp_printf(1,"getsockname failed %s\n",strerror(errno));
    close(s);
    return FTP_BAD;
  }
  ftp_printf(0,"FTP: Connected to %s.%d\n",
             inet_ntoa(host.sin_addr), ntohs(host.sin_port));
  
  return (s);
}

/* Perform a login to the server. Pass the username and password and
   put the connection into binary mode. This assumes a passwd is
   always needed. Is this true? */

static int 
login(char * username, 
      char *passwd, 
      int s, 
      char *msgbuf, 
      unsigned msgbuflen,
      ftp_printf_t ftp_printf) {
  
  int ret;

  ret = command("USER",username,s,msgbuf,msgbuflen,ftp_printf);
  if (ret != 3) {
    ftp_printf(1,"FTP: User %s not accepted\n",username);
    return (FTP_BADUSER);
  }
  
  ret = command("PASS",passwd,s,msgbuf,msgbuflen,ftp_printf);
  if (ret < 0) {
    return (ret);
  }
  if (ret != 2) {
    ftp_printf(1,"FTP: Login failed for User %s\n",username);
    return (FTP_BADUSER);
  }
  
  ftp_printf(0,"FTP: Login sucessfull\n");
  
  ret = command("TYPE","I",s,msgbuf,msgbuflen,ftp_printf);
  if (ret < 0) {
    return (ret);
  }
  if (ret != 2) {
    ftp_printf(1,"FTP: TYPE failed!\n");
    return (FTP_BAD);
  }
  return (ret);
}


/* Open a data socket. This is a client socket, i.e. its listening
waiting for the FTP server to connect to it. Once the socket has been
opened send the port command to the server so the server knows which
port we are listening on.*/
static int 
opendatasock(int ctrl_s,
             struct sockaddr_in ctrl, 
             char *msgbuf, 
             unsigned msgbuflen,
             ftp_printf_t ftp_printf) {

  struct sockaddr_in local;
  socklen_t len;
  int on = 1;
  char buf[4*6+1];
  char *a, *p;
  int ret;
  int s;

  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) {
    ftp_printf(1,"socket: %s\n",strerror(errno));
    return FTP_BAD;
  }
  
  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof (on)) < 0) {
    ftp_printf(1,"setsockopt: %s\n",strerror(errno));
    close(s);
    return FTP_BAD;
  }

  local = ctrl;
  local.sin_family = AF_INET;
  local.sin_port = 0;
  if (bind(s,(struct sockaddr *)&local,sizeof(local)) < 0) {
    ftp_printf(1,"bind: %s\n",strerror(errno));
    close(s);
    return FTP_BAD;   
  }
  
  len = sizeof(local);
  if (getsockname(s,(struct sockaddr *)&local,&len) < 0) {
    ftp_printf(1,"getsockname: %s\n",strerror(errno));
    close(s);
    return FTP_BAD;   
  }
  
  if (listen(s, 1) < 0) {
    ftp_printf(1,"listen: %s\n",strerror(errno));
    close(s);
    return FTP_BAD;   
  }

#define	BtoI(b)	(((int)b)&0xff)
  a = (char *)&local.sin_addr;
  p = (char *)&local.sin_port;
  sprintf(buf,"%d,%d,%d,%d,%d,%d",
          BtoI(a[0]),BtoI(a[1]),BtoI(a[2]),BtoI(a[3]),
          BtoI(p[0]),BtoI(p[1]));

  ret = command("PORT",buf,ctrl_s,msgbuf,msgbuflen,ftp_printf);
  if (ret < 0) {
    close(s);
    return (ret);
  }

  if (ret != 2) {
    ftp_printf(1,"FTP: PORT failed!\n");
    close(s);
    return (FTP_BAD);
  }
  return (s);
}

/* Receive the file into the buffer and close the data socket
   afterwards */
static int 
receive_file(int data_s, char *buf, int buf_size,ftp_printf_t ftp_printf)
{
  int remaining = buf_size;
  int finished = 0;
  int total_size=0;
  char *bufp = buf;
  int len;
  int s;
  
  s = accept(data_s,NULL,0);
  if (s<0) {
    ftp_printf(1,"listen: %s\n",strerror(errno));
    return FTP_BAD;   
  }
  
  do {
    len = read(s,bufp,remaining);
    if (len < 0) {
      ftp_printf(1,"read: %s\n",strerror(errno));
      close(s);
      return FTP_BAD;   
    }

    if (len == 0) {
      finished = 1;
    } else {
      total_size += len;
      remaining -= len;
      bufp += len;
    
      if (total_size == buf_size) {
        ftp_printf(1,"FTP: File too big!\n");
        close(s);
        return FTP_TOOBIG;
      }
    }
  } while (!finished);
  
  close(s);
  return total_size;
}

/* Receive the file into the buffer and close the socket afterwards*/
static int 
send_file(int data_s, char *buf, int buf_size,ftp_printf_t ftp_printf)
{
  int remaining=buf_size;
  int finished = 0;
  char * bufp = buf;
  int len;
  int s;
  
  s = accept(data_s,NULL,0);
  if (s<0) {
    ftp_printf(1,"listen: %s\n",strerror(errno));
    return FTP_BAD;   
  }
  
  do { 
    len = write(s,bufp,remaining);
    if (len < 0) {
      ftp_printf(1,"write: %s\n",strerror(errno));
      close(s);
      return FTP_BAD;   
    }
    
    if (len == remaining) {
      finished = 1;
    } else {
      remaining -= len;
      bufp += len;
    }
  } while (!finished);
  
  close(s);
  return 0;
}

/* All done, say bye, bye */
static int quit(int s, 
                char *msgbuf, 
                unsigned msgbuflen,
                ftp_printf_t ftp_printf) {
  
  int ret;
  
  ret = command("QUIT",NULL,s,msgbuf,msgbuflen,ftp_printf);
  if (ret < 0) {
    return (ret);
  }
  if (ret != 2) {
    ftp_printf(1,"FTP: Quit failed!\n");
    return (FTP_BAD);
  }
  
  ftp_printf(0,"FTP: Connection closed\n");
  return (0);
}

/* Get a file from an FTP server. Hostname is the name/IP address of
   the server. username is the username used to connect to the server
   with. Passwd is the password used to authentificate the
   username. filename is the name of the file to receive. It should be
   the full pathname of the file. buf is a pointer to a buffer the
   contents of the file should be placed in and buf_size is the size
   of the buffer. If the file is bigger than the buffer, buf_size
   bytes will be retrieved and an error code returned. ftp_printf is a
   function to be called to perform printing. On success the number of
   bytes received is returned. On error a negative value is returned
   indicating the type of error. */

int ftp_get(char * hostname, 
            char * username, 
            char * passwd, 
            char * filename, 
            char * buf, 
            unsigned buf_size,
            ftp_printf_t ftp_printf)
{

  struct sockaddr_in local;
  char msgbuf[256];
  int s,data_s;
  int bytes;
  int ret;
  
  s = connect_to_server(hostname,&local,ftp_printf);
  if (s < 0) {
    return (s);
  }
  
  /* Read the welcome message from the server */
  if (get_reply(s,ftp_printf) != 2) {
    ftp_printf(0,"FTP: Server refused connection\n");
    close(s);
    return FTP_BAD;
  }

  ret = login(username,passwd,s,msgbuf,sizeof(msgbuf),ftp_printf);
  if (ret < 0) {
    close(s);
    return (ret);
  }

  /* We are now logged in and ready to transfer the file. Open the
     data socket ready to receive the file. It also build the PORT
     command ready to send */
  data_s = opendatasock(s,local,msgbuf,sizeof(msgbuf),ftp_printf);
  if (data_s < 0) {
    close (s);
    return (data_s);
  }

  /* Ask for the file */
  ret = command("RETR",filename,s,msgbuf,sizeof(msgbuf),ftp_printf);
  if (ret < 0) {
    close(s);
    close(data_s);
    return (ret);
  }
  
  if (ret != 1) {
    ftp_printf(0,"FTP: RETR failed!\n");
    close (data_s);
    close(s);
    return (FTP_BADFILENAME);
  }
  
  if ((bytes=receive_file(data_s,buf,buf_size,ftp_printf)) < 0) {
    ftp_printf(0,"FTP: Receiving file failed\n");
    close (data_s);
    close(s);
    return (bytes);
  }
  
  if (get_reply(s,ftp_printf) != 2) {
    ftp_printf(0,"FTP: Transfer failed!\n");
    close (data_s);
    close(s);
    return (FTP_BAD);
  }

  ret = quit(s,msgbuf,sizeof(msgbuf),ftp_printf);
  if (ret < 0) {
    close(s);
    close(data_s);
    return (ret);
  }
            
  close (data_s);
  close(s);
  return bytes;
}
  
/* Put a file on an FTP server. Hostname is the name/IP address of the
   server. username is the username used to connect to the server
   with. Passwd is the password used to authentificate the
   username. filename is the name of the file to receive. It should be
   the full pathname of the file. buf is a pointer to a buffer the
   contents of the file should be placed in and buf_size is the size
   of the buffer. ftp_printf is a function to be called to perform
   printing. On success 0 is returned. On error a negative value is
   returned indicating the type of error. */

int ftp_put(char * hostname, 
            char * username, 
            char * passwd, 
            char * filename, 
            char * buf, 
            unsigned buf_size,
            ftp_printf_t ftp_printf)
{

  struct sockaddr_in local;
  char msgbuf[256];
  int s,data_s;
  int ret;
  
  s = connect_to_server(hostname,&local,ftp_printf);
  if (s < 0) {
    return (s);
  }
  
  /* Read the welcome message from the server */
  if (get_reply(s,ftp_printf) != 2) {
    ftp_printf(1,"FTP: Server refused connection\n");
    close(s);
    return FTP_BAD;
  }

  ret = login(username,passwd,s,msgbuf,sizeof(msgbuf),ftp_printf);
  if (ret < 0) {
    close(s);
    return (ret);
  }

  /* We are now logged in and ready to transfer the file. Open the
     data socket ready to receive the file. It also build the PORT
     command ready to send */
  data_s = opendatasock(s,local,msgbuf,sizeof(msgbuf),ftp_printf);
  if (data_s < 0) {
    close (s);
    return (data_s);
  }

  /* Ask for the file */
  ret = command("STOR",filename,s,msgbuf,sizeof(msgbuf),ftp_printf);
  if (ret < 0) {
    close(s);
    close(data_s);
    return (ret);
  }
  
  if (ret != 1) {
    ftp_printf(1,"FTP: STOR failed!\n");
    close (data_s);
    close(s);
    return (FTP_BADFILENAME);
  }
  
  if ((ret = send_file(data_s,buf,buf_size,ftp_printf)) < 0) {
    ftp_printf(1,"FTP: Sending file failed\n");
    close (data_s);
    close(s);
    return (ret);
  }
  
  if (get_reply(s,ftp_printf) != 2) {
    ftp_printf(1,"FTP: Transfer failed!\n");
    close (data_s);
    close(s);
    return (FTP_BAD);
  }

  ret = quit(s,msgbuf,sizeof(msgbuf),ftp_printf);
  if (ret < 0) {
    close(s);
    close(data_s);
    return (ret);
  }
            
  close (data_s);
  close(s);
  return 0;
}

/* An example ftp_printf function. This uses the standard eCos diag
output device for outputting error and diagnostic messages. The
function take one addition parameter to the normal printf function. The
first parameter indicates when the message is an error message when
true. This can be used to filter errors from diagnostic output. In
this example the error parameter is ignored and everything printed. */

void ftpclient_printf(unsigned error, const char *fmt, ...) 
{
  va_list ap;
  
  va_start(ap, fmt);
  diag_vprintf( fmt, ap);
  va_end(ap);
}
