/*
 * BSD-style socket emulation library for the Mac
 * Original author: Tom Milligan
 * Current author: Charlie Reiman - creiman@ncsa.uiuc.edu
 *
 * This source file is placed in the public domain.
 * Any resemblance to NCSA Telnet, living or dead, is purely coincidental.
 *
 *      National Center for Supercomputing Applications
 *      152 Computing Applications Building
 *      605 E. Springfield Ave.
 *      Champaign, IL  61820
 *
 *
 * RCS Modification History:
 * $Log$
 * Revision 6.1  2001/05/17 15:04:38  lavr
 * Typos corrected
 *
 * Revision 6.0  1997/08/25 18:38:02  madden
 * Revision changed to 6.0
 *
 * Revision 4.0  1995/07/26 13:56:09  ostell
 * force revision to 4.0
 *
 * Revision 1.5  1995/06/02  16:29:03  kans
 * *** empty log message ***
 *
 * Revision 1.4  1995/05/18  08:23:19  epstein
 * add RCS modification history (after PowerPC port)
 *
 */

/*
 * Internal prototypes for the socket library.
 * There is duplication between socket.ext.h and socket.int.h, but
 * there are too many complications in combining them.
 */

#ifndef __socket_int__
#define __socket_int__

#ifdef __MWERKS__
#include <MacTCP.h>
#endif

typedef enum spin_msg
	{
	SP_MISC,			/* some weird thing */
	SP_SELECT,			/* in a select call */
	SP_NAME,			/* getting a host by name */
	SP_ADDR,			/* getting a host by address */
	SP_TCP_READ,		/* TCP read call */
	SP_TCP_WRITE,		/* TCP write call */
	SP_UDP_READ,		/* UDP read call */
	SP_UDP_WRITE,		/* UDP write call */
	SP_SLEEP			/* sleeping */
	} spin_msg;
	

/* spin routine prototype, doesn't work. Sigh. */

extern int (*spinroutine)(int mesg,int param);

/* tcpglue.c */

#ifndef __MWERKS__
#ifdef __MACTCP__
#define TCPIOCompletionProc TCPIOCompletionUPP
#define TCPNotifyProc TCPNotifyUPP
#define UDPIOCompletionProc UDPIOCompletionUPP
#else
#define TCPIOCompletionUPP TCPIOCompletionProc
#define TCPNotifyUPP TCPNotifyProc
#define UDPIOCompletionUPP UDPIOCompletionProc
#endif

#endif


OSErr xPBControlSync(TCPiopb *pb);
OSErr xPBControl(TCPiopb *pb, TCPIOCompletionUPP completion);


OSErr xOpenDriver(void);
OSErr xTCPCreate(int buflen, TCPNotifyUPP notify, TCPiopb *pb);
OSErr xTCPPassiveOpen(TCPiopb *pb, short port, TCPIOCompletionUPP completion);
OSErr xTCPActiveOpen(TCPiopb *pb, short port, long rhost, short rport, TCPIOCompletionUPP completion);
OSErr xTCPRcv(TCPiopb *pb, char *buf, int buflen, int timeout, TCPIOCompletionUPP completion);
OSErr xTCPNoCopyRcv(TCPiopb *,rdsEntry *,int,int,TCPIOCompletionUPP);
OSErr xTCPBufReturn(TCPiopb *pb,rdsEntry *rds,TCPIOCompletionUPP completion);
OSErr xTCPSend(TCPiopb *pb, wdsEntry *wds, Boolean push, Boolean urgent, TCPIOCompletionUPP completion);
OSErr xTCPClose(TCPiopb *pb,TCPIOCompletionUPP completion);
OSErr xTCPAbort(TCPiopb *pb);
OSErr xTCPRelease(TCPiopb *pb);
int xTCPBytesUnread(SocketPtr sp);
int xTCPBytesWriteable(SocketPtr sp);
int xTCPWriteBytesLeft(SocketPtr sp);
int xTCPState(TCPiopb *pb);
OSErr xUDPCreate(SocketPtr sp,int buflen,ip_port port);
OSErr xUDPRead(SocketPtr sp,UDPIOCompletionUPP completion);
OSErr xUDPBfrReturn(SocketPtr sp);
OSErr xUDPWrite(SocketPtr sp,ip_addr host,ip_port port,miniwds *wds,
		UDPIOCompletionUPP completion);
OSErr xUDPRelease(SocketPtr sp);
ip_addr xIPAddr(void);
long xNetMask(void);
unsigned short xMaxMTU(void);


/* socket.tcp.c */
pascal void sock_tcp_notify(
	StreamPtr tcpStream,
	unsigned short eventCode,
	Ptr userDataPtr,
	unsigned short terminReason,
	ICMPReport *icmpMsg);
int sock_tcp_new_stream(SocketPtr sp);
int sock_tcp_connect(SocketPtr sp, struct sockaddr_in *addr);
int sock_tcp_listen(SocketPtr sp);
int sock_tcp_accept(SocketPtr sp, struct sockaddr_in *from, Int4 *fromlen);
int sock_tcp_accept_once(SocketPtr sp, struct sockaddr_in *from, Int4 *fromlen);
int sock_tcp_recv(SocketPtr sp, char *buffer, int buflen, int flags);
int sock_tcp_can_read(SocketPtr sp);
int sock_tcp_send(SocketPtr sp, char *buffer, int count, int flags);
int sock_tcp_can_write(SocketPtr sp);
int sock_tcp_close(SocketPtr sp);

/* socket.udp.c */

int sock_udp_new_stream(SocketPtr sp);
int sock_udp_connect(SocketPtr sp,struct sockaddr_in *addr);
int sock_udp_recv(SocketPtr sp, char *buffer, int buflen, int flags, struct sockaddr_in *from, int *fromlen);
int sock_udp_can_recv(SocketPtr sp);
int sock_udp_send(SocketPtr sp, struct sockaddr_in *to, char *buffer, int count, int flags);
int sock_udp_can_send(SocketPtr sp);
int sock_udp_close(SocketPtr sp);

/* socket.util.c */

int sock_init(void);
void sock_close_all(void);
int sock_free_fd(int f);
void sock_dup_fd(int s,int s1);
void sock_clear_fd(int s);
void sock_init_fd(int s);
int sock_err(int err_code);
void sock_copy_addr(void *from, void *to, Int4 *tolen);
void sock_dump(void);
void sock_print(char *title, SocketPtr sp);
StreamHashEntPtr sock_find_shep(StreamPtr);
StreamHashEntPtr sock_new_shep(StreamPtr);
void *sock_fetch_pb(SocketPtr);

#endif  /*__socket_int__ */

