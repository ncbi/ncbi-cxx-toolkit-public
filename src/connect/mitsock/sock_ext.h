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


 /*
  * High level prototypes.
  */
  
#ifdef __cplusplus
extern "C" {
#endif

/* Socket.c */

#define fcntl MIT_fcntl
#define read MIT_read
#define write MIT_write
#define close MIT_close

/* forward declarations */
struct sockaddr;
struct timeval;
struct iovec;
struct in_addr;

/* External Sockets API calls */
int socket(int family, int type, int protocol);
int bind(int sockFD, const struct sockaddr *myAddr, int addrLength);
int fcntl(int sockFD, int command, int flags);
int close(int sockFD);
int shutdown(int sockFD, int howTo);
int connect(int sockFD, struct sockaddr *servAddr, int addrLength);
int listen(int sockFD, int backLog);
int accept(int sockFD, struct sockaddr *peer, int *addrlen);
int select(int maxFDsExamined, fd_set *readFDs, fd_set *writeFDs, fd_set *exceptFDs,
                  struct timeval *timeOut);
int getpeername(int sockFD, struct sockaddr *peerAddr, int *addrLength);
int getsockname(int sockFD, struct sockaddr *localAddr, int *addrLength);
int read(int sockFD, void *buffer, UInt32 numBytes);
int write(int sockFD, void *buffer, UInt32 numBytes);
int readv(int sockFD, struct iovec *iov, UInt32 iovCount);
int writev(int sockFD, struct iovec *iov, UInt32 iovCount);
int recv(int sockFD, void *buffer, UInt32 numBytes, int flags);
int send(int sockFD, void *buffer, UInt32 numBytes, int flags);
int recvfrom(int sockFD, void *buffer, UInt32 numBytes, int flags, struct sockaddr *fromAddr, 
                    socklen_t *addrLength);
int sendto(int sockFD, void *buffer, UInt32 numBytes, int flags, struct sockaddr *toAddr, 
                  socklen_t addrLength);
int setsockopt(long sockFD, long level, long optname, char *optval, long optlen);
int	inet_aton(const char *str, struct in_addr *addr);

/* not implemented (yet)...
 *
int s_accept_once(Int4 s, struct sockaddr *addr, Int4 *addrlen);
int s_writev(Int4 s, struct iovec *iov, Int4 count);
int s_sendmsg(Int4 s,struct msghdr *msg,Int4 flags);
int s_really_send(Int4 s, void *buffer, Int4 count, Int4 flags, struct sockaddr_in *to);
int s_getdtablesize(void);
int s_ioctl(Int4 d, Int4 request, int *argp);  // argp is really a caddr_t

 */

#ifdef __cplusplus
}
#endif

#endif  /*__socket_ext__ */
