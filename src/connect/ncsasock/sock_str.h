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
* Revision 6.2  2001/05/17 15:04:38  lavr
* Typos corrected
*
* Revision 6.1  1999/07/03 23:58:49  kans
* now including MacTCP.h
*
* Revision 6.0  1997/08/25 18:38:06  madden
* Revision changed to 6.0
*
* Revision 4.1  1997/01/29 00:12:00  kans
* include <MacTCP.h> instead of obsolete <MacTCPCommonTypes.h>
*
 * Revision 4.0  1995/07/26  13:56:09  ostell
 * force revision to 4.0
 *
 * Revision 1.3  1995/05/23  15:31:16  kans
 * new CodeWarrior 6 errors and warnings fixed
 *
 * Revision 1.2  1995/05/17  17:57:59  epstein
 * add RCS log revision history
 *
 */

/*
 * Internal Structures for the Toronto socket library
 * Most of the complicated header stuff goes here.
 */

#ifndef ipBadLapErr
#include <MacTCP.h>
#endif
#ifdef ParamBlockHeader
#undef ParamBlockHeader
#endif
/*
#include <GetMyIPAddr.h>
#include <TCPPB.h>
#include <UDPPB.h>
*/
#include <MacTCP.h>
#include <AddressXlation.h>

#ifdef __MWERKS__
typedef unsigned char byte;
#endif

#define TCPStateClosed		 0
#define TCPStateListen		 2
#define TCPStateSynReceived	 4
#define TCPStateSynSent		 6
#define TCPStateEstablished	 8
#define TCPStateFinWait1	10
#define TCPStateFinWait2	12
#define TCPStateCloseWait	14
#define TCPStateClosing		16
#define TCPStateLastAck		18
#define TCPStateTimeWait	20

/* NUM_SOCKETS must be a power of 2 for the stream->socket hasing to work */
#define NUM_SOCKETS			32		/* Number of sockets.  Should never exceed 32 */
#define SOCKETS_MASK    (NUM_SOCKETS-1)  /* used for hash table wrap around via bitwise and */
#define NUM_PBS			(NUM_SOCKETS+1)	 /* number of pbs in global pool */

#define STREAM_BUFFER_SIZE 	32768	/* memory for MacTCP streams */

#define UDP_MAX_MSG		65507	/* MacTCP max legal udp message */
#define TCP_MAX_MSG		65535	/* MacTCP max legal tcp message */

#define TCP_MAX_WDS		4		/* arbitrary number of wds to alloc in sock_tcp_send */

/*
 *	In use and shutdown status.
 */
#define	SOCK_STATUS_USED		0x1		/* Used socket table entry */
#define	SOCK_STATUS_NOREAD		0x2		/* No more reading allowed from socket */
#define	SOCK_STATUS_NOWRITE		0x4		/* No more writing allowed to socket */

/*
 *	Socket connection states.
 */
#define SOCK_STATE_NO_STREAM	0	/* Socket doesn't have a MacTCP stream yet */
#define	SOCK_STATE_UNCONNECTED	1	/* Socket is unconnected. */
#define	SOCK_STATE_LISTENING	2	/* Socket is listening for connection. */
#define	SOCK_STATE_LIS_CON		3	/* Socket is in transition from listen to connected. */
#define	SOCK_STATE_CONNECTING	4	/* Socket is initiating a connection. */
#define	SOCK_STATE_CONNECTED	5	/* Socket is connected. */
#define SOCK_STATE_CLOSING      6   /* Socket is closing */

typedef union AllPb 
	{
	TCPiopb		tcp;
	UDPiopb		udp;
	} AllPb;
	
typedef struct SocketRecord 
{
	StreamPtr			stream;		/* stream pointer */
	byte				status;		/* Is file descriptor in use */
	int					fd;			/* fd number */
	short				protocol;	/* Protocol (e.g. TCP, UDP) */
	Boolean				nonblocking;/* socket set for non-blocking I/O. */
	char				*recvBuf;	/* receive buffer */
	int					recvd;		/* amount received */
	int					torecv;		/* amount to receive */
	struct sockaddr_in	sa;			/* My address. */
	struct sockaddr_in	peer;		/* Her address. */
	byte				sstate;		/* socket's connection state. */
	unsigned long		dataavail;	/* Amount of data available on connection. */
	int					asyncerr;	/* Last async error to arrive.  zero if none. */
	/* stdio stuff */
#ifndef  DONT_INCLUDE_SOCKET_STDIO
	char				*outbuf;	/* Ptr to array to buffer output */
	int					outbufcount;/* # of characters in outbuf */
	Ptr					outbufptr;	/* Pointer into outbuf */
	char				*inbuf;		/* Ptr to array to buffer input */
	int					inbufcount;	/* # of characters in inbuf */
	Ptr					inbufptr;	/* Pointer into inbuf */
	Boolean				ioerr;		/* Holds error status for stdio calls */
	Boolean				ioeof;		/* EOF was detected on stream */
#endif
} SocketRecord, *SocketPtr;

/*
 * Quick note for hash table: 
 *  Stream =  0 => unused
 *  Stream = -1 => deleted
 *  Stream = anything else => stream ptr
 */
typedef struct StreamHashEnt
	{
	StreamPtr	stream;
	SocketPtr	socket;
	} StreamHashEnt, *StreamHashEntPtr;

typedef	struct	miniwds
	{
	unsigned short length;
	char * ptr;
	unsigned short terminus;	/* must be zero'd for use */
	} miniwds;

#ifndef __socket_ext__
typedef int (*SpinFn)();
#endif /* __socket_ext__ */

/*-------------------------------------------------------------------------*/
#define		SOCK_MIN_PTR			(Ptr)0x1400		/* Minimum reasonable pointer */
#define		goodptr(p)				(((Ptr) p) > SOCK_MIN_PTR)
#define		is_used(p)				(goodptr(p) && (p)->status & SOCK_STATUS_USED)
#define		is_stdio(p)				(is_used(p) && (p)->inbuf != NULL)
#define		sock_good_fd(s)			((0 <= s && s < NUM_SOCKETS) && is_used (sockets+s))
#define		sock_nowrite(p)			((p)->status & SOCK_STATUS_NOWRITE)
#define		sock_noread(p)			((p)->status & SOCK_STATUS_NOREAD)

#define 	TVTOTICK(tvsec,tvusec)	( ((tvsec)*60) + ((tvusec)/16666) )
#define		min(a,b)				( (a) < (b) ? (a) : (b))
#define		max(a,b)				( (a) > (b) ? (a) : (b))

/* SPIN returns a -1 on user cancel for fn returning integers */
#define		SPIN(cond,mesg,param)	do {if (spinroutine)\
									if ((*spinroutine)(mesg,param))\
										return -1;\
									}while(cond);

/* SPINP returns a NULL on user cancel, for fn returning pointers */				
#define		SPINP(cond,mesg,param)	do {if (spinroutine)\
									if ((*spinroutine)(mesg,param))\
										return NULL;\
									}while(cond);
									
								
