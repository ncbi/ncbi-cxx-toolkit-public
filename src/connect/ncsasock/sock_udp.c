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
 * Revision 1.4  1995/05/18 08:23:12  epstein
 * add RCS modification history (after PowerPC port)
 *
 */

#ifdef USEDUMP
# pragma load "Socket.dump"

#else
# include <Events.h>
# include <Memory.h>
# include <Errors.h>
# include <Types.h>
# include <OSUtils.h>
# include <Stdio.h>

# include <s_types.h>
# include <neti_in.h>
# include <neterrno.h>
# include <s_socket.h>
# include <s_time.h>
# include <s_uio.h>

# include "sock_str.h"
# include "sock_int.h"

#endif

extern SocketPtr sockets;
extern SpinFn spinroutine;

static void sock_udp_read_ahead_done(UDPiopb *pb);
static void sock_udp_send_done(UDPiopb *pb);

#if 0
/*
 * asynchronous notification routine 
 */
static int notified = 0;
static int lastNotifyCount = 0;

static StreamPtr notifyUdpStream;
static unsigned short notifyEventCode;
static Ptr notifyUserDataPtr;
static unsigned short notifyTerminReason;
static struct ICMPReport *notifyIcmpMsg;

pascal void sock_udp_notify(
	StreamPtr udpStream,
	unsigned short eventCode,
	Ptr userDataPtr,
	struct ICMPReport *icmpMsg)
{
	notified++;

	notifyUdpStream = udpStream;
	notifyEventCode = eventCode;
	notifyUserDataPtr = userDataPtr;
	notifyIcmpMsg = icmpMsg;
}

static char *eventNames[] = 
{
	"event 0",
	"data arrival",
	"ICMP message"
};
static char *icmpMessages[] =
{
	"net unreachable",
	"host unreachable",
	"protocol unreachable",
	"port unreachable",
	"fragmentation required",
	"source route failed",
	"time exceeded",
	"parameter problem",
	"missing required option"
};


int udpCheckNotify()
{
	if (notified == lastNotifyCount)
		return(0);
	
	lastNotifyCount = notified;
	dprintf("notify count is now %d\n",lastNotifyCount);
	dprintf("stream %08x\n",notifyUdpStream);
	dprintf("event %d '%s'\n",notifyEventCode,eventNames[notifyEventCode]);
	if (notifyEventCode == UDPDataArrival)
		dprintf("%d bytes\n",notifyTerminReason/*!?*/);
	dprintf("icmp msg %08x\n",notifyIcmpMsg);
	if (notifyEventCode == UDPICMPReceived)
	{
		dprintf("stream %08x\n",notifyIcmpMsg->streamPtr);
		dprintf("local %08x/%d\n",notifyIcmpMsg->localHost,notifyIcmpMsg->localPort);
		dprintf("remote %08x/%d\n",notifyIcmpMsg->remoteHost,notifyIcmpMsg->remotePort);
		dprintf("%s\n",icmpMessages[notifyIcmpMsg->reportType]);
		dprintf("optionalAddlInfo %04x\n",notifyIcmpMsg->optionalAddlInfo);
		dprintf("optionalAddlInfoPtr %08x\n",notifyIcmpMsg->optionalAddlInfoPtr);
	}
	dprintf("userdata %s\n",notifyUserDataPtr);
	return(1);
}

#endif

/*
 * sock_udp_new_stream()  - create the MacTCP stream for this socket. not
 *                          called till the last minute while we wait for
 *                          a bind to come in.
 *
 *                          called from whichever of connect, recv, send or 
 *                          select (can_recv or can_send) is called first.
 */
int sock_udp_new_stream(
	SocketPtr sp)
{
OSErr 				io;
StreamHashEntPtr 	shep;
	
#if SOCK_UDP_DEBUG >= 2
	dprintf("sock_udp_new_stream: sp %08x port %d\n", sp, sp->sa.sin_port);
#endif
	
	if ( (io=xUDPCreate(sp, STREAM_BUFFER_SIZE, sp->sa.sin_port)) != noErr )
		return(sock_err(io));
		
	sp->sstate = SOCK_STATE_UNCONNECTED;
	
	if (shep = sock_new_shep(sp->stream))
		{
		shep -> stream = sp->stream;
		shep -> socket = sp;
		}
	else
		return -1;
		
	sp-> recvd = 0;
	sp-> recvBuf = 0;
	sp-> asyncerr = inProgress;
	
	/* start up the read ahead */
	return(sock_udp_read_ahead(sp));
}


/*
 *	sock_udp_connect() - sets the peer process we will be talking to
 */
int sock_udp_connect(
	SocketPtr sp,
	struct sockaddr_in *addr)
{
	int status;
	
	/* make the stream if its not made already */
	if (sp->sstate == SOCK_STATE_NO_STREAM)
	{
		status = sock_udp_new_stream(sp);
		if (status != 0)
			return(status);
	}
	
	/* record our peer */
	sp->peer.sin_len = sizeof(struct sockaddr_in);
	sp->peer.sin_addr.s_addr = addr->sin_addr.s_addr;
	sp->peer.sin_port = addr->sin_port;
	sp->peer.sin_family = AF_INET;
	sp->sstate = SOCK_STATE_CONNECTED;
	
	if (sp-> recvBuf) {
		xUDPBfrReturn(sp);
		}
	/* flush the read-ahead buffer if its not from our new friend */
	(void) sock_udp_can_recv(sp);
	
	return(0);
}

/*
 * sock_udp_read_ahead() - start up the one packet read-ahead
 *
 *                         called from new_stream, recv and can_recv
 */
static int sock_udp_read_ahead(SocketPtr sp)
	{
	OSErr io;
	
	io = xUDPRead(sp, (UDPIOCompletionUPP)sock_udp_read_ahead_done);
	if (io != noErr) 
		return(sock_err(io));

	return(0);
	}

/*
 * sock_udp_return_buffer() - return the receive buffer to MacTCP
 */
static 
int sock_udp_return_buffer(
	SocketPtr sp)
{
	OSErr io;
	
	if (sp->recvBuf)
	{
		io = xUDPBfrReturn(sp);
		if (io != noErr)
			return(sock_err(io));
	}
	return(noErr);
}

/*
 * sock_udp_recv()
 *
 * returns bytes received or -1 and errno
 */
int sock_udp_recv(
	SocketPtr sp,
	char *buffer,
	int buflen,
	int flags,
	struct sockaddr_in *from,
	int *fromlen)
{
#pragma unused(flags)
	
#if SOCK_UDP_DEBUG >= 2
	dprintf("sock_udp_recv: sp %08x buflen %d state %04x\n", sp,buflen,sp->sstate);
#endif

	/* make the stream if its not made already */
	if (sp->sstate == SOCK_STATE_NO_STREAM)
	{
		int status = sock_udp_new_stream(sp);
		if (status != 0)
			return(status);
	}
	
	/* dont block a non-blocking socket */
	if (sp->nonblocking && !sock_udp_can_recv(sp))
		return(sock_err(EWOULDBLOCK));
	
	SPIN(!sock_udp_can_recv(sp),SP_UDP_READ,0)

	if (sp->asyncerr!=noErr)
		return(sock_err(sp->asyncerr));

	/* return the data to the user - truncate the packet if necessary */
	buflen = min(buflen,sp->recvd);	
	BlockMove(sp->recvBuf,buffer,buflen);
	
#if (SOCK_UDP_DEBUG >= 7) || defined(UDP_PACKET_TRACE)
/*
	hex_dump(buffer, buflen, "udp from %08x/%d\n",
			sp->apb.pb.udp.csParam.receive.remoteHost,
			sp->apb.pb.udp.csParam.receive.remotePort);
*/
#endif

	if (from != NULL && *fromlen >= sizeof(struct sockaddr_in))
		{
		(*from) = sp->peer;
		(*fromlen) = sizeof (struct sockaddr_in);
		}	
	
	/* continue the read-ahead - errors which occur */
	/* here will show up next time around */
	(void) sock_udp_return_buffer(sp);
	(void) sock_udp_read_ahead(sp);

	return(buflen);
}


/*
 *	sock_udp_can_recv() - returns non-zero if a packet has arrived.
 *
 *                        Used by select, connect and recv.
 */
int sock_udp_can_recv(SocketPtr sp)
	{
	if (sp->sstate == SOCK_STATE_NO_STREAM)
		{
		int status = sock_udp_new_stream(sp);
		if (status != 0)
			return(-1);
		}
	
	if (sp->asyncerr == inProgress)
		return(0);
	
	return 1;  // must recieve if not reading, even if an error occured - must handle error.
	}

/*
 *	sock_udp_send() - send the data in the (already prepared) wds
 *
 *	returns bytes sent or -1 and errno
 */
int sock_udp_send(SocketPtr sp,struct sockaddr_in *to,char *buffer,int count,int flags)
	{
#pragma unused(flags)
	miniwds  awds;
	OSErr    io;
	
#if SOCK_UDP_DEBUG >= 2
	dprintf("sock_udp_send: %08x state %04x\n",sp,sp->sstate);
#endif

	/* make the stream if its not made already */
	if (sp->sstate == SOCK_STATE_NO_STREAM)
		{
		io = sock_udp_new_stream(sp);
		if (io != 0)
			return(io);
		}
	
	if ( count > UDP_MAX_MSG )
		return sock_err(EMSGSIZE);
		
	awds.terminus = 0;
	awds.length = count;
	awds.ptr = buffer;
	
	// if no address passed, hope we have one already in peer field
	if (to == NULL)
		if (sp->peer.sin_len)
			to = &sp->peer;
		else
			return (sock_err(EHOSTUNREACH));
	
	io = xUDPWrite(sp, to->sin_addr.s_addr,to->sin_port, &awds, 
			(UDPIOCompletionUPP)sock_udp_send_done);
	
	if (io != noErr )
		return(io);
	
	// get sneaky. compl. proc sets ptr to nil on completion, and puts result code in
	// terminus field.
	
	SPIN(awds.ptr != NULL,SP_UDP_WRITE,count)
	return (awds.terminus);
	}

/*
 *	sock_udp_can_send() - returns non-zero if a write will not block
 */
int sock_udp_can_send(SocketPtr sp)
	{
	if (sp->sstate == SOCK_STATE_NO_STREAM)
		{
		if ( sock_udp_new_stream(sp) != 0 )
			return(-1);
		}
	
	return (1);
	}

/*
 *	sock_udp_close()
 */
int sock_udp_close(SocketPtr sp)
	{
	OSErr io;
	
	if (sp->sstate == SOCK_STATE_NO_STREAM)
		return(0);
	io = xUDPRelease(sp);
	if (io != noErr)
		return(sock_err(io));
	return(0);
	}
	
#pragma segment SOCK_RESIDENT
/*
 * Interrupt routines - MUST BE IN A RESIDENT SEGMENT! Most important to 
 * MacApp programmers
 */
	
	
/*
 * sock_udp_send_done
 */

static void sock_udp_send_done(UDPiopb *pb) {
	((miniwds *)pb->csParam.send.wdsPtr)->terminus = pb->ioResult;
	((miniwds *)pb->csParam.send.wdsPtr)->ptr = NULL;
	}

/*
 * sock_udp_read_ahead_done
 */
static void sock_udp_read_ahead_done(UDPiopb *pb) {
	register	SocketPtr	sp;
	
	sp = sock_find_shep( pb->udpStream )->socket;
	if (pb->ioResult == noErr) {
		sp->recvBuf = pb->csParam.receive.rcvBuff;
		sp->recvd   = pb->csParam.receive.rcvBuffLen;
		}
	else {
		sp-> recvd = 0;
		sp-> recvBuf = 0;
		}
	sp->asyncerr = pb->ioResult;
	}

