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
 *
 *
 * RCS Modification History:
 * $Log$
 * Revision 1.7  1995/05/18 08:23:16  epstein
 * add RCS modification history (after PowerPC port)
 *
 */
 
#ifdef USEDUMP
# pragma load "Socket.dump"

#else
# include <Events.h>
# include <Memory.h>
# include <Types.h>
# include <OSUtils.h> /* for SysBeep */
# include <Events.h> /* for TickCount */
# include <Stdio.h>
#ifdef __MWERKS__
#include <MacTCP.h>
#endif


# include <s_types.h>
# include <neti_in.h>
# include <neterrno.h>
# include <s_socket.h>
# include <s_time.h>
# include <s_uio.h>

# include "sock_str.h"
# include "sock_int.h"

#endif

#if !defined(THINK_C) && !defined(__MWERKS__)
# include "sock_int.h"
#endif

extern SocketPtr sockets;
extern SpinFn spinroutine;	/* The spin routine. */ 

static void sock_tcp_send_done(TCPiopb *pb);
static void sock_tcp_listen_done(TCPiopb *pb);
static void sock_tcp_connect_done(TCPiopb *pb);
static void sock_tcp_recv_done(TCPiopb *pb);


static TCPNotifyUPP sock_tcp_notify_proc ;
static TCPIOCompletionUPP sock_tcp_connect_done_proc ;
static TCPIOCompletionUPP sock_tcp_listen_done_proc ;
static TCPIOCompletionUPP sock_tcp_send_done_proc ;
static TCPIOCompletionUPP sock_tcp_recv_done_proc ;

#ifndef __MACTCP__
#if GENERATINGCFM
#define NewTCPNotifyProc(userRoutine)		\
		(TCPNotifyUPP) NewRoutineDescriptor((ProcPtr)(userRoutine), uppTCPNotifyProcInfo, GetCurrentArchitecture())
#else
#define NewTCPNotifyProc(userRoutine)		\
		((TCPNotifyUPP) (userRoutine))
#endif

#if GENERATINGCFM
#define NewTCPIOCompletionProc(userRoutine)		\
		(TCPIOCompletionUPP) NewRoutineDescriptor((ProcPtr)(userRoutine), uppTCPIOCompletionProcInfo, GetCurrentArchitecture())
#else
#define NewTCPIOCompletionProc(userRoutine)		\
		((TCPIOCompletionUPP) (userRoutine))
#endif
#endif

void sock_tcp_init ( void )
{
	sock_tcp_notify_proc = NewTCPNotifyProc(sock_tcp_notify);
	sock_tcp_connect_done_proc = NewTCPIOCompletionProc (sock_tcp_connect_done);
	sock_tcp_listen_done_proc = NewTCPIOCompletionProc (sock_tcp_listen_done);
	sock_tcp_send_done_proc = NewTCPIOCompletionProc (sock_tcp_send_done);
	sock_tcp_recv_done_proc = NewTCPIOCompletionProc (sock_tcp_recv_done);

}

/*
 * sock_tcp_new_stream
 *
 * Create a new tcp stream.
 */
int sock_tcp_new_stream(
	SocketPtr sp)
{
	OSErr		io;
	TCPiopb		pb;
	StreamHashEntPtr shep;

#if SOCK_TCP_DEBUG >= 2
	sock_print("sock_tcp_new_stream", sp);
#endif

	sock_tcp_init() ;
	
	io = xTCPCreate(STREAM_BUFFER_SIZE, sock_tcp_notify_proc, &pb);
	switch(io)
	{
		case noErr:                 break;
		case streamAlreadyOpen:     return(sock_err(io));
		case invalidLength:         return(sock_err(ENOBUFS));
		case invalidBufPtr:         return(sock_err(ENOBUFS));
		case insufficientResources: return(sock_err(EMFILE));
		default: /* error from PBOpen */ return(sock_err(ENETDOWN));
	}
	sp->sstate = SOCK_STATE_UNCONNECTED;
	sp->peer.sin_family = AF_INET;
	sp->peer.sin_addr.s_addr = 0;
	sp->peer.sin_port = 0;
	bzero(&sp->peer.sin_zero[0], 8);
	sp->dataavail = 0;
	sp->asyncerr = 0;
	sp->stream = pb.tcpStream;
		
	if ((shep = sock_new_shep(sp->stream))!=NULL)
		{
		shep -> stream = sp->stream;
		shep -> socket = sp;
		return(0);
		}
	else
		return -1;
}



/*
 *	sock_tcp_connect - initiate a connection on a TCP socket
 *
 */
int sock_tcp_connect(
	SocketPtr sp,
	struct sockaddr_in *addr)
{
	OSErr io;
	TCPiopb	*pb;

#if SOCK_TCP_DEBUG >= 2
	sock_print("sock_tcp_connect",sp);
#endif
	
	/* Make sure this socket can connect. */
	if (sp->sstate == SOCK_STATE_CONNECTING)
		return(sock_err(EALREADY));
	if (sp->sstate != SOCK_STATE_UNCONNECTED)
		return(sock_err(EISCONN));
		
	sp->sstate = SOCK_STATE_CONNECTING;
	
	if (!(pb=sock_fetch_pb(sp)))
		return sock_err(ENOMEM);
	
#ifdef __MACTCP__
	io = xTCPActiveOpen(pb, sp->sa.sin_port,addr->sin_addr.s_addr, addr->sin_port, 
			sock_tcp_connect_done_proc);
#else
	io = xTCPActiveOpen(pb, sp->sa.sin_port,addr->sin_addr.s_addr, addr->sin_port, 
			sock_tcp_connect_done);
#endif
			
	if (io != noErr)
	{
		sp->sstate = SOCK_STATE_UNCONNECTED;
		return(sock_err(io));
	}
	
	if (sp->nonblocking)		
		return(sock_err(EINPROGRESS));
	
	/* sync connect - spin till TCPActiveOpen completes */

#if SOCK_TCP_DEBUG >= 5
	dprintf("spinning in connect\n");
#endif

	SPIN (pb->ioResult==inProgress,SP_MISC,0L)

#if SOCK_TCP_DEBUG >= 5
	dprintf("done spinning\n");
#endif
	
	if ( pb->ioResult != noErr )
		return sock_err(pb->ioResult);
	else
		return 0;
}


/*
 * sock_tcp_listen() - put s into the listen state.
 */
int sock_tcp_listen(
	SocketPtr sp)
{
	OSErr		io;
	TCPiopb		*pb;

#if SOCK_TCP_DEBUG >= 2
	sock_print("sock_tcp_listen",sp);
#endif

	if (!(pb = sock_fetch_pb(sp)))
		return (sock_err(ENOMEM));
	
	if (sp->sstate != SOCK_STATE_UNCONNECTED)
		return(sock_err(EISCONN));
	
	sp->sstate = SOCK_STATE_LISTENING;
	
#ifdef __MACTCP__
	io = xTCPPassiveOpen(pb, sp->sa.sin_port, sock_tcp_listen_done_proc);
#else
	io = xTCPPassiveOpen(pb, sp->sa.sin_port, sock_tcp_listen_done);
#endif
	if (io != noErr) 
	{
		sp->sstate = SOCK_STATE_UNCONNECTED;
		return(sock_err(io));
	}
#if SOCK_TCP_DEBUG >= 5
	dprintf("sock_tcp_listen: about to spin for port number - ticks %d\n",
			TickCount());
#endif
	while (pb->csParam.open.localPort == 0)
	{
#if SOCK_TCP_DEBUG >= 5
		tcpCheckNotify();
#endif
		SPIN(false,SP_MISC,0L)
	}
#if SOCK_TCP_DEBUG >= 5
	dprintf("sock_tcp_listen: port number is %d ticks %d\n",
	pb->csParam.open.localPort,TickCount());
#endif
	sp->sa.sin_addr.s_addr = pb->csParam.open.localHost;
	sp->sa.sin_port = pb->csParam.open.localPort;
	return(0);
}

/*
 *	sock_tcp_accept()
 */
int sock_tcp_accept(
	SocketPtr sp,
	struct sockaddr_in *from,
	Int4 *fromlen)
{
	int 		s1;
	TCPiopb		*pb;
	StreamHashEntPtr shep;

#if SOCK_TCP_DEBUG >= 2
	sock_print("sock_tcp_accept",sp);
#endif

	if (sp->sstate == SOCK_STATE_UNCONNECTED)
	{
		if (sp->asyncerr != 0)
		{
			(void) sock_err(sp->asyncerr);
			sp->asyncerr = 0;
			return(-1);
		}
		else
			return(sock_err(ENOTCONN));
	}
	if (sp->sstate != SOCK_STATE_LISTENING && sp->sstate != SOCK_STATE_LIS_CON)
		return(sock_err(ENOTCONN));

	if (sp->sstate == SOCK_STATE_LISTENING) 
	{	
		if (sp->nonblocking) 
			return(sock_err(EWOULDBLOCK));

		/*	Spin till sock_tcp_listen_done runs. */
#if SOCK_TCP_DEBUG >= 5
		dprintf("--- blocking...\n");
#endif
		
		SPIN(sp->sstate == SOCK_STATE_LISTENING,SP_MISC,0L);

#if SOCK_TCP_DEBUG >= 5
		dprintf("--- done blocking...\n");
#endif
		
		/* got notification - was it success? */
		if (sp->sstate != SOCK_STATE_LIS_CON) 
		{
#if	SOCK_TCP_DEBUG >=3
			dprintf("--- failed state %04x code %d\n",sp->sstate,sp->asyncerr);
#endif
			(void) sock_err(sp->asyncerr);
			sp->asyncerr = 0;
			return(-1);
		}
	}
#if SOCK_TCP_DEBUG >= 3
	dprintf("sock_tcp_accept: Have connection, peer is %08x/%d, duplicating socket.\n",
			sp->peer.sin_addr,sp->peer.sin_port);
#endif
	/*
	 * Have connection.  Duplicate this socket.  The client gets the connection
	 * on the new socket and I create a new stream on the old socket and put it 
	 * in listen state. 
	 */
	sp->sstate = SOCK_STATE_CONNECTED;

	s1 = sock_free_fd(0);
	if (s1 < 0) 
	{
		/*	No descriptors left.  Abort the incoming connection. */
#if SOCK_TCP_DEBUG >= 2
		dprintf("sock_tcp_accept: No descriptors left.\n");
#endif
		if (!(pb = sock_fetch_pb (sp)))
			return sock_err(ENOMEM);
			
		(void) xTCPAbort(pb);
		sp->sstate = SOCK_STATE_UNCONNECTED;

		/* try and put the socket back in listen mode */
		if (sock_tcp_listen(sp) < 0) 
		{
#if SOCK_TCP_DEBUG >= 1
			dprintf("sock_tcp_accept: sock_tcp_listen fails\n");
#endif
			sp->sstate = SOCK_STATE_UNCONNECTED;
			return(-1);		/* errno already set */
		}
		return(sock_err(EMFILE));
	}
	
	/* copy the incoming connection to the new socket */
	sock_dup_fd(sp->fd,s1);
#if SOCK_TCP_DEBUG >= 3
	dprintf("sock_tcp_accept: new socket is %d\n",s1);
	sock_dump();
#endif

	/* quitely adjust the StreamHash table */
	if ((shep = sock_find_shep(sp->stream))!=NULL)
		{
		shep->socket = sockets+s1;		/* point to new socket */
		}

	/* Create a new MacTCP stream on the old socket and put it into */
	/* listen state to accept more connections. */
	if (sock_tcp_new_stream(sp) < 0 || sock_tcp_listen(sp) < 0) 
	{
#if SOCK_TCP_DEBUG >= 2
		dprintf("accept: failed to restart old socket\n");
#endif
		/* nothing to listen on */
		sp->sstate = SOCK_STATE_UNCONNECTED;
		
		/* kill the incoming connection */
		if (!(pb=sock_fetch_pb(sockets+s1)))
			return sock_err(ENOMEM);
			
		xTCPRelease(pb);
		sock_clear_fd(s1);
		
		return(-1); /* errno set */
	}
#if SOCK_TCP_DEBUG >= 3
	dprintf("sock_tcp_accept: got new stream\n");
	sock_dump();
#endif

	/* return address of partner */
	sock_copy_addr(&sockets[s1].peer, from, fromlen);

	return(s1);
}

/*
 *	sock_tcp_accept_once()
 */
int sock_tcp_accept_once(
	SocketPtr sp,
	struct sockaddr_in *from,
	Int4 *fromlen)
{

#if SOCK_TCP_DEBUG >= 2
	sock_print("sock_tcp_accept_once",sp);
#endif

	if (sp->sstate == SOCK_STATE_UNCONNECTED)
	{
		if (sp->asyncerr != 0)
		{
			(void) sock_err(sp->asyncerr);
			sp->asyncerr = 0;
			return(-1);
		}
		else
			return(sock_err(ENOTCONN));
	}
	if (sp->sstate != SOCK_STATE_LISTENING && sp->sstate != SOCK_STATE_LIS_CON)
		return(sock_err(ENOTCONN));

	if (sp->sstate == SOCK_STATE_LISTENING) 
	{	
		if (sp->nonblocking) 
			return(sock_err(EWOULDBLOCK));

		/*	Spin till sock_tcp_listen_done runs. */
#if SOCK_TCP_DEBUG >= 5
		dprintf("--- blocking...\n");
#endif
		
		SPIN(sp->sstate == SOCK_STATE_LISTENING,SP_MISC,0L);
		
#if SOCK_TCP_DEBUG >= 5
		dprintf("--- done blocking...\n");
#endif
		
		/* got notification - was it success? */
		if (sp->sstate != SOCK_STATE_LIS_CON) 
		{
#if	SOCK_TCP_DEBUG >=3
			dprintf("--- failed state %04x code %d\n",sp->sstate,sp->asyncerr);
#endif
			(void) sock_err(sp->asyncerr);
			sp->asyncerr = 0;
			return(-1);
		}
	}
#if SOCK_TCP_DEBUG >= 3
	dprintf("sock_tcp_accept_once: Have connection, peer is %08x/%d.\n",
			sp->peer.sin_addr,sp->peer.sin_port);
#endif
	/*
	 * Have connection.
	 */
	sp->sstate = SOCK_STATE_CONNECTED;
	
	/* return address of partner */
	sock_copy_addr(&(sp->peer), from, fromlen);

	return(0);
}

/*
 * sock_tcp_recv()
 *
 * returns bytes received or -1 and errno
 */
int sock_tcp_recv(
	SocketPtr sp,
	char *buffer,
	int buflen,
	int flags)
{
#pragma unused(flags)
	TCPiopb	*pb;
	int iter; /* iteration */
	
#if SOCK_TCP_DEBUG >= 2
	sock_print("sock_tcp_recv",sp);
#endif

	/* socket hasn't finished connecting yet */
	if (sp->sstate == SOCK_STATE_CONNECTING)
	{
#if SOCK_TCP_DEBUG >= 5
		dprintf("sock_tcp_recv: connection still in progress\n");
#endif
		if (sp->nonblocking)
			return(sock_err(EWOULDBLOCK));
			
		/* async connect and sync recv? */
#if SOCK_TCP_DEBUG >= 5
		dprintf("sock_tcp_recv: spinning on connect\n");
#endif

		SPIN(sp->sstate == SOCK_STATE_CONNECTING,SP_MISC,0L)

#if SOCK_TCP_DEBUG >= 5
		dprintf("sock_tcp_recv: done spinning\n");
#endif
	}
		
	/* socket is not connected */
	if (! (sp->sstate == SOCK_STATE_CONNECTED)) 
	{
		/* see if the connect died (pretty poor test) */
		if (sp->sstate == SOCK_STATE_UNCONNECTED && sp->asyncerr != 0)
		{
			(void) sock_err(sp->asyncerr);
			sp->asyncerr = 0;
			return(-1);
		}

		/* I guess he just forgot */
		return(sock_err(ENOTCONN));		
	}
			
	if (sp->dataavail == 0)
		sp->dataavail = xTCPBytesUnread(sp);		/* sync. call */
#if SOCK_TCP_DEBUG >= 3
	dprintf("sock_tcp_recv: %d bytes available\n", sp->dataavail);
#endif
	if (sp->nonblocking && sp->dataavail == 0) 
			return(sock_err(EWOULDBLOCK));

	sp->torecv              = 1; /* # of bytes to try to receive */
	sp->recvBuf             = buffer;
	sp->recvd               = 0; /* count of bytes received */	
	pb = sock_fetch_pb(sp);
	
	/* make 2 iterations; on 1st try, read 1 byte; on 2nd, read
       all outstanding available bytes, up to buflen ... this mechanism seems to
       be necessary for decent performance, because TCPRcv only completes when one
       of the following takes place:
       * enough data has arrived to fill the receive buffer
       * pushed data arrives
       * urgent data is outstanding
       * some reasonable period passes after the arrival of nonpushed, nonurgent data
       * the amount of data received is greater than or equal to 25 percent of the total
         receive buffering for this stream
       * the command time-out expires
       
       In the case when a caller has requested N bytes, and the data is "normal" TCP
       data, the "reasonable" period must expire before this function will return. This
       "reasonable" period appears to be about one second (MacTCP version 1.1), and is
       not configurable. The hope in the algorithm implemented here is that a reasonable
       amount of data will arrive along with the first byte, and that the caller is
       capable of issuing another read() to obtain more data. The one-second "reasonable"
       delay is thus eliminated.
       
       J. Epstein, NCBI, 06/24/92
    */
    
	for (iter = 0; iter < 2; iter++) {

		sp->asyncerr			= inProgress;
	
		xTCPRcv(pb, sp->recvBuf, min (sp->torecv,TCP_MAX_MSG),0,sock_tcp_recv_done_proc);
	
		SPIN(sp->torecv&&(pb->ioResult==noErr||pb->ioResult==inProgress),
			SP_TCP_READ,sp->torecv)
		
		if ( pb->ioResult == commandTimeout )
			pb->ioResult = noErr;

		switch(pb->ioResult)
		{
			case noErr:
#if SOCK_TCP_DEBUG >= 3
				dprintf("sock_tcp_recv: got %d bytes\n", sp->recvd);
#endif
				sp->dataavail = xTCPBytesUnread(sp);
				sp->asyncerr = noErr;
				if (sp->dataavail <= 0 || buflen <= 1 || iter == 1)
					return(sp->recvd);
				/* loop back and obtain the remaining outstanding data */
				sp->torecv = min(sp->dataavail, buflen - 1);
				sp->recvBuf = &buffer[1];
				break;
				
			case connectionClosing:
#if SOCK_TCP_DEBUG >= 2
				dprintf("sock_tcp_recv: connection closed\n");
#endif
				return (sp->recvd);
				break;


			case connectionTerminated:
				/* The connection is aborted. */
				sp->sstate = SOCK_STATE_UNCONNECTED;
#if SOCK_TCP_DEBUG >= 1
				dprintf("sock_tcp_recv: connection gone!\n");
#endif
				return(sock_err(ENOTCONN));

			case commandTimeout: /* this one should be caught by sock_tcp_recv_done */
			case connectionDoesntExist:
			case invalidStreamPtr:
			case invalidLength:
			case invalidBufPtr:
			default:
				return(sock_err(pb->ioResult));
		}
	}
}

/*
 *	sock_tcp_can_read() - returns non-zero if data or a connection is available
 *  must also return one if the connection is down to force an exit from the
 *  select routine (select is the only thing that uses this).
 */
int sock_tcp_can_read(SocketPtr sp)
	{
	TCPiopb		*pb;
	
	if (sp->sstate == SOCK_STATE_LIS_CON) 
		return(1);

	else if (sp->sstate == SOCK_STATE_CONNECTED) 
		{
		if (!(pb=sock_fetch_pb(sp)))
			return sock_err (ENOMEM);
		
		sp->dataavail = xTCPBytesUnread(sp);
		if (sp->dataavail > 0) 
			return(1);
		}
	else if ( ( sp->sstate == SOCK_STATE_UNCONNECTED ) || 
			( sp->sstate == SOCK_STATE_CLOSING ) )
		return 1;

	return(0);
	}

/* avoid using standard library here because size of an int may vary externally */
void *sock_memset(void *s, int c, size_t n)
{
	char *t = s;
	
	for (; n > 0; n--, t++)
		*t = c;
}
	

/*
 *	sock_tcp_send() - send data
 *
 *	returns bytes sent or -1 and errno
 */
int sock_tcp_send(
	SocketPtr sp,
	char *buffer,
	int count,
	int flags)
{
	int		bytes,towrite;
	miniwds	*thiswds;
	short	wdsnum;
	TCPiopb	*pb;
	miniwds	wdsarray[TCP_MAX_WDS];

#if SOCK_TCP_DEBUG >= 2
	sock_print("sock_tcp_send",sp);
#endif

	/* socket hasn't finished connecting yet */
	if (sp->sstate == SOCK_STATE_CONNECTING)
	{
#if SOCK_TCP_DEBUG >= 5
		dprintf("sock_tcp_send: connection still in progress\n");
#endif
		if (sp->nonblocking)
			return(sock_err(EALREADY));
			
		/* async connect and sync send? */
#if SOCK_TCP_DEBUG >= 5
		dprintf("sock_tcp_send: spinning on connect\n");
#endif
		while(sp->sstate == SOCK_STATE_CONNECTING)
		{
#if SOCK_TCP_DEBUG >= 5
			tcpCheckNotify();
#endif
			SPIN(false,SP_MISC,0L)
		}
#if SOCK_TCP_DEBUG >= 5
		dprintf("sock_tcp_send: done spinning\n");
#endif
	}
		
	/* socket is not connected */
	if (! (sp->sstate == SOCK_STATE_CONNECTED)) 
	{
		/* see if a previous operation failed */
		if (sp->sstate == SOCK_STATE_UNCONNECTED && sp->asyncerr != 0)
		{
			(void) sock_err(sp->asyncerr);
			sp->asyncerr = 0;
			return(-1);
		}

		/* I guess he just forgot */
		return(sock_err(ENOTCONN));		
	}
	
	if ( (xTCPBytesWriteable(sp) < count) && (sp->nonblocking) )
		return sock_err(EWOULDBLOCK);
		
	bytes=count;	/* save count before we nuke it */
	sock_memset(wdsarray,0,TCP_MAX_WDS*sizeof(miniwds));	/* clear up terminus and mark empty */
	thiswds = wdsarray;
	wdsnum = 0;
	
	while (count > 0)
		{	
		/* make sure the thing that just finished worked ok */
		if (sp->asyncerr != 0)
			{
			(void) sock_err(sp->asyncerr);
			sp->asyncerr = 0;
			return(-1);
			}

/*
 * for deBUGging: try replacing TCP_MAX_MSG with a small value (like 7) so
 * you can test that the loop won't choke while waiting for writes to finish
 */
		towrite=min(count,TCP_MAX_MSG);
		
		/* find a clean wds */
		
		while ( thiswds->length != 0 )
			{
			wdsnum = (short)((wdsnum+1)%TCP_MAX_WDS); /* generates compiler warning w/o short - why? */
			if (wdsnum)
				thiswds++;
			else
				thiswds = wdsarray;
			SPIN(false,SP_TCP_WRITE,count);	/* spin once */
			}
		
		/* find a clean pb */
		
		if (!(pb=sock_fetch_pb(sp)))
			return sock_err(ENOMEM);
			
		
		thiswds->length = (short)towrite;
		thiswds->ptr=buffer;
				
#ifdef __MACTCP__
		xTCPSend(pb,(wdsEntry *)thiswds,(count <= TCP_MAX_MSG), /* push */
				flags & MSG_OOB,	/*urgent*/
				sock_tcp_send_done_proc);
#else
		xTCPSend(pb,(wdsEntry *)thiswds,(count <= TCP_MAX_MSG), /* push */
				flags & MSG_OOB,	/*urgent*/
				sock_tcp_send_done);
#endif
				
		SPIN(false,SP_TCP_WRITE,count);
		count -= towrite;
		buffer += towrite;
		}
		
	SPIN(pb->ioResult == inProgress,SP_TCP_WRITE,0);
	
	if ( pb->ioResult == noErr )
		{
		return(bytes);
		}
	else
		return(sock_err(pb->ioResult));
}


/*
 *	sock_tcp_can_write() - returns non-zero if a write will not block
 * Very lousy check. Need to check (send window)-(unack data).
 */
int sock_tcp_can_write(
	SocketPtr sp)
{
	return (sp->sstate == SOCK_STATE_CONNECTED);
}

/*
 *	sock_tcp_close() - close down a socket being careful about i/o in progress
 */
int sock_tcp_close(
	SocketPtr sp)
{
	OSErr io;
	TCPiopb	*pb;

 void sock_flush_out(SocketPtr);
 void sock_flush_in(SocketPtr);
	
#if SOCK_TCP_DEBUG >= 2
	sock_print("sock_tcp_close ",sp);
#endif
	
	if (!(pb=sock_fetch_pb(sp)))
		return sock_err(ENOMEM);
		
	sock_flush_out(sp);
	
	/* close the stream */ 
	io = xTCPClose(pb,(TCPIOCompletionUPP)(-1));
	
#if SOCK_TCP_DEBUG >= 5
	dprintf("sock_tcp_close: xTCPClose returns %d\n",io);
#endif

	switch (io)
	{
		case noErr:
		case connectionClosing:
			break;
		case connectionDoesntExist:
		case connectionTerminated:
			break;
		case invalidStreamPtr:
		default:
			return(sock_err(io));
	}
	
	sock_flush_in(sp);
	
	/* destroy the stream */ 
	if ((io = xTCPRelease(pb)) != noErr)
	{
#if SOCK_TCP_DEBUG >= 5
		dprintf("sock_tcp_close: xTCPRelease error %d\n",io);
#endif
		return(sock_err(io));
	}
	/* check for errors from async writes etc */
	if (( sp->asyncerr != noErr ) && ( sp->asyncerr != connectionTerminated ))
	{
#if SOCK_TCP_DEBUG >= 5
		dprintf("sock_tcp_close: asyncerr %d\n",sp->asyncerr);
#endif
		return(sock_err(sp->asyncerr));
	}
	return(0);
}

static void sock_flush_out(SocketPtr sp) {
	while (xTCPWriteBytesLeft(sp)>0) {
		(*spinroutine)(SP_MISC,0L);
		}
	}

static void sock_flush_in(SocketPtr sp) {
	TCPiopb	*pb;
	rdsEntry	rdsarray[TCP_MAX_WDS+1];
	int		passcount;
	const int maxpass =4;
	
	if (!(pb = sock_fetch_pb(sp)))
		return;	
		
	for (passcount=0;passcount<maxpass;passcount++) {
		if (xTCPNoCopyRcv(pb,rdsarray,TCP_MAX_WDS,1,0)==noErr) {
			xTCPBufReturn(pb,rdsarray,0);
			(*spinroutine)(SP_MISC,0L);
			}
		else
			break;
		}
		
	if (passcount == maxpass) {		/* remote side isn't being nice */
		(void)xTCPAbort(pb);		/* then try again */
		
		for (passcount=0;passcount<maxpass;passcount++) {
			if (xTCPNoCopyRcv(pb,rdsarray,TCP_MAX_WDS,1,0)==noErr) {
				xTCPBufReturn(pb,rdsarray,0);
				(*spinroutine)(SP_MISC,0L);
				}
			else
				break;
			}
		}
	}
	

#pragma segment SOCK_RESIDENT
/*
 * Interrupt routines - MUST BE IN A RESIDENT SEGMENT! Most important to 
 * MacApp programmers
 */
pascal void sock_tcp_notify(
	StreamPtr tcpStream,
	unsigned short eventCode,
	Ptr userDataPtr,
	unsigned short terminReason,
	ICMPReport *icmpMsg)
	{
#pragma unused (userDataPtr,terminReason,icmpMsg)
	register 	StreamHashEntPtr	shep;
	

	shep = sock_find_shep(tcpStream);
	if ( shep )
		{
		SocketPtr	sp = shep->socket;
		
		if ( eventCode == TCPClosing )
			{
			sp->sstate = SOCK_STATE_CLOSING;
			}
		else if ( eventCode == TCPTerminate )
			{
			sp->sstate = SOCK_STATE_UNCONNECTED;
			}
		}
	}

static void sock_tcp_connect_done(TCPiopb *pb)
	{
	SocketPtr sp;
	
	sp = sock_find_shep(pb->tcpStream)->socket;
	
	if (pb->ioResult == noErr )
		{
		sp->sa.sin_addr.s_addr = pb->csParam.open.localHost;
		sp->sa.sin_port = pb->csParam.open.localPort;
		sp->peer.sin_addr.s_addr = pb->csParam.open.remoteHost;
		sp->peer.sin_port = pb->csParam.open.remotePort;
		sp->sstate = SOCK_STATE_CONNECTED;
		sp->asyncerr = noErr;
		}
	}


static void
sock_tcp_listen_done(TCPiopb *pb)
{	
	SocketPtr sp;

	sp = sock_find_shep(pb->tcpStream)->socket;
	
	switch(pb->ioResult)
	{
		case noErr:
			sp->peer.sin_addr.s_addr = pb->csParam.open.remoteHost;
			sp->peer.sin_port = pb->csParam.open.remotePort;
			sp->sstate = SOCK_STATE_LIS_CON;
			sp->asyncerr = 0;
			break;
			
		case openFailed:
		case invalidStreamPtr:
		case connectionExists:
		case duplicateSocket:
		case commandTimeout:
		default:				
			sp->sstate = SOCK_STATE_UNCONNECTED;
			sp->asyncerr = pb->ioResult;
			break;
	}
}


static void
sock_tcp_recv_done(
	TCPiopb *pb)
	{
	register	readin;
	register	SocketPtr	sp;

	sp = sock_find_shep( pb->tcpStream )->socket;;
	
	if (pb->ioResult == noErr)
		{
		readin = pb->csParam.receive.rcvBuffLen;
		sp -> recvBuf += readin;
		sp -> recvd   += readin;
		sp -> torecv  -= readin;
		if ( sp -> torecv )
			{
			xTCPRcv(pb,sp->recvBuf,min(sp -> torecv,TCP_MAX_MSG),1, sock_tcp_recv_done_proc);
			}
		}
	}


static void
sock_tcp_send_done(
	TCPiopb *pb)
{	
	SocketPtr sp;

	sp = sock_find_shep(pb->tcpStream)->socket;
	
	switch(pb->ioResult)
	{
		case noErr:
			((wdsEntry *)(pb->csParam.send.wdsPtr))->length = 0;	/* mark it free */
			break;
			
		case ipNoFragMemErr:
		case connectionClosing:
		case connectionTerminated:
		case connectionDoesntExist:
			sp->sstate = SOCK_STATE_UNCONNECTED;
			sp->asyncerr = ENOTCONN;
			break;
			
		case ipDontFragErr:
		case invalidStreamPtr:
		case invalidLength:
		case invalidWDS:
		default:
			sp->sstate = SOCK_STATE_UNCONNECTED;
			sp->asyncerr = pb->ioResult;
			break;
	}
	
}

