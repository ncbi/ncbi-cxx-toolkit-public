/*
 * BSD-style socket emulation library for the Mac
 * Original author: Tom Milligan
 * Current author: Charlie Reiman - creiman@ncsa.uiuc.edu
 *
 * This source file is placed in the public domian.
 * Any resemblance to NCSA Telnet, living or dead, is purely coincidental.
 *
 *      National Center for Supercomputing Applications
 *      152 Computing Applications Building
 *      605 E. Springfield Ave.
 *      Champaign, IL  61820
 */

/*
 * External definitions for socket users.
 * Charlie Reiman
 * Friday, August 3, 1990 2:09:45 PM
 */
 
#ifndef __socket_ext__
#define __socket_ext__

#ifndef __MEMORY__
#include <Memory.h>
#endif  /* __MEMORY__ */

#ifndef _TYPES_
#include <s_types.h>
#endif  /*_TYPES_ */

#ifndef ipBadLapErr
#include <MacTCP.h>
#endif


#ifndef SOCK_STATE_NO_STREAM

typedef int (*SpinFn)();


typedef enum spin_msg
	{
	SP_MISC,			/* some weird thing, usually just return immediately if you get this */
	SP_SELECT,			/* in a select call */
	SP_NAME,			/* getting a host by name */
	SP_ADDR,			/* getting a host by address */
	SP_TCP_READ,		/* TCP read call */
	SP_TCP_WRITE,		/* TCP write call */
	SP_UDP_READ,		/* UDP read call */
	SP_UDP_WRITE,		/* UDP write call */
	SP_SLEEP			/* sleeping, passes ticks left to sleep */
	} spin_msg;
	
	
/*
 * You spin routine should look like this:
 *
 * void myspin(spin_msg aMsg,long data)
 *	{
 *	switch (aMsg)
 *    ...
 *	}
 *
 * The data param is only defined for TCP read and write. 
 * For TCP reads:
 *  Nubmer of bytes left to read in.
 * For TCP writes:
 *  Number of bytes left to write. Note that this may be 0 for writes once all
 *  the necessary writes are queued up and the library is simply waiting for
 *  them to finish.
 */
 
 /*
  * High level prototypes.
  */
  
#ifdef __cplusplus
extern "C" {
#endif

/* Socket.c */
 
int s_socket(Int4 domain, Int4 type, short protocol);
int s_bind(Int4 s, struct sockaddr *name, Int4 namelen);
int s_connect(Int4 s, struct sockaddr *addr, Int4 addrlen);
int s_listen(Int4 s, Int4 qlen);
int s_accept(Int4 s, struct sockaddr *addr, Int4 *addrlen);
int s_accept_once(Int4 s, struct sockaddr *addr, Int4 *addrlen);
int s_close(Int4 s);
int s_read(Int4 s, void *buffer, Int4 buflen);
int s_recv(Int4 s, void *buffer, Int4 buflen, Int4 flags);
int s_recvfrom(Int4 s, void *buffer, Int4 buflen, Int4 flags, struct sockaddr *from, int *fromlen);
int s_write(Int4 s, void *buffer, Int4 buflen);
int s_writev(Int4 s, struct iovec *iov, Int4 count);
int s_send(Int4 s, void *buffer, Int4 buflen, Int4 flags);
int s_sendto (Int4 s, void *buffer, Int4 buflen, Int4 flags, struct sockaddr *to, Int4 tolen);
int s_sendmsg(Int4 s,struct msghdr *msg,Int4 flags);
int s_really_send(Int4 s, void *buffer, Int4 count, Int4 flags, struct sockaddr_in *to);
int s_select(Int4 width, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
int s_getdtablesize(void);
int s_getsockname(Int4 s, struct sockaddr *name, Int4 *namelen);
int s_getpeername(Int4 s, struct sockaddr *name, Int4 *namelen);
int s_shutdown(Int4 s, Int4 how);
int s_fcntl(Int4 s, /* unsigned */ Int4 cmd, Int4 arg);
int s_dup(Int4 s);
int s_dup2(Int4 s, Int4 s1);
int s_ioctl(Int4 d, Int4 request, int *argp);  /* argp is really a caddr_t */
int s_setsockopt(Int4 s, Int4 level, Int4 optname, char *optval, Int4 optlen);
int s_setspin(SpinFn routine);
SpinFn s_getspin(void);

/* Socket.stdio.c */

/**** GONE ****/
#if 0
Ptr s_fdopen(int fd, char *type);
int s_fileno(SocketPtr sp);
int s_fgetc(SocketPtr sp);
int s_ungetc(char c, SocketPtr sp);
int s_fread(char *buffer, int size, int nitems, SocketPtr sp);
int s_fputc(char c, SocketPtr sp);
int s_fprintf(SocketPtr sp, char *fmt, ...);
int s_fwrite(char *buffer, int size, int nitems, SocketPtr sp);
int s_fflush(SocketPtr sp);
int s_fclose(SocketPtr sp);
int s_ferror(SocketPtr sp);
int s_feof(SocketPtr sp);
int s_clearerr(SocketPtr sp);
#endif

/**** unGONE ****/

/* netdb.c */

#ifndef SOCK_DEFS_ONLY

#ifdef DONT_DEFINE_INET
typedef unsigned long ip_addr;
#endif /* DONT_DEFINE_INET */

struct hostent *gethostbyname(char *name);
struct hostent *gethostbyaddr(ip_addr *addrP,Int4 len,Int4 type);
#ifndef DONT_DEFINE_INET
char *inet_ntoa(ip_addr inaddr);
ip_addr inet_addr(char *address);
#endif /* DONT_DEFINE_INET */

#endif /* SOCK_DEFS_ONLY */

#ifdef __cplusplus
}
#endif

/* FOLLOWING LINE ADDED FOR THINKC -- JAE 03/20/92 */


#endif /* SOCK_STATE_NO_STREAM */
#endif  /*__socket_ext__ */
