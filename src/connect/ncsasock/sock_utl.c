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
* Revision 4.0  1995/07/26 13:56:09  ostell
* force revision to 4.0
*
 * Revision 1.3  1995/05/23  15:31:16  kans
 * new CodeWarrior 6 errors and warnings fixed
 *
 * Revision 1.2  1995/05/17  17:58:02  epstein
 * add RCS log revision history
 *
 */

#ifdef USEDUMP
# pragma load "Socket.dump"
#else
# include <Events.h>
# include <Types.h>
# include <Memory.h>
# include <Stdio.h>
# include <OSUtils.h>

# include <s_types.h>
# include <neti_in.h>
# include <neterrno.h>
# include <s_file.h>
# include <s_ioctl.h>
# include <s_socket.h>
# include <s_time.h>
# include <s_uio.h>

# include "sock_str.h"
# include "sock_int.h"
#endif

#include <StdLib.h>

extern SocketPtr sockets;
extern AllPb *pbList;
extern short pbLast;
extern StreamHashEntPtr streams;
extern SpinFn spinroutine;

long errno_long; /* same as errno, but of known length */

/*
 * sock_init() - initialize everything.
 * BUG NOTE: returns error codes, but no one that calls it actually checks the return
 * codes. probably bad.
 */
int sock_init()
{
	OSErr io;
	int i;
	
	if (sockets != NULL)
		return 0;

	sock_find_shep((StreamPtr) NULL);		/* load resident segment */
			
	/*
	 * call up driver
	 */
	
	xOpenDriver();
	
#if SOCK_UTIL_DEBUG >= 2
	dprintf("sock_init: first time through\n");
	dprintf("sock_init: allocating %d bytes for %d socket records\n",
			NUM_SOCKETS * sizeof(SocketRecord),NUM_SOCKETS);
#endif

	/* allocate storage for socket records */
	sockets = (SocketPtr)NewPtrClear(NUM_SOCKETS * sizeof(SocketRecord));
	if (sockets == NULL)
		return(sock_err(ENOMEM));
		
	/* allocate storage for pbs */
	pbList = (AllPb *)NewPtrClear(NUM_PBS * sizeof (AllPb));
	if ( pbList == NULL )
		return sock_err(ENOMEM);

	/* allocate storage for stream->socket hash table */
	streams = (StreamHashEntPtr)NewPtrClear(NUM_SOCKETS * sizeof(StreamHashEnt));
	if ( streams == NULL )
		return(sock_err(ENOMEM));
	
	/* initialize them */
	for (i=0; i<NUM_SOCKETS; i++)
	{
		sock_clear_fd(i);
	}
	
	/* load the MacTCP name server resolver */
#if SOCK_UTIL_DEBUG >= 2
	dprintf("sock_init: loading name server resolver\n");
#endif

	io = OpenResolver(NULL);
	
#if SOCK_UTIL_DEBUG >= 1
	if (io != noErr)
		dprintf("sock_init: failed to load name server resolver code %d\n",io);
	else
		dprintf("sock_init: loaded name server ok.\n");
#endif

	/* establish our clean up man */
	atexit(sock_close_all);
	
#if SOCK_UTIL_DEBUG >= 1
	dprintf("sock_init: exiting.\n");
#endif
	return(0);
}

/*
 * sock_close_all() - Close all sockets (aborting their connections) and
 *                    release dynamic storage.
 */
void sock_close_all()
{
	int s;
	SocketPtr sp;
	TCPiopb		*tpb;
	UDPiopb		*upb;

	for (s = 0 ; s < NUM_SOCKETS ; ++s) 
	{
		sp = sockets+s;
		if (sp->status == SOCK_STATUS_USED) 
		{
			switch(sp->protocol) 
			{
				case IPPROTO_UDP:
					upb = sock_fetch_pb(sp);		/* must succeed */
					(void) xUDPRelease(sp);
					break;
					
				case IPPROTO_TCP:
					tpb = sock_fetch_pb(sp);
					(void) xTCPRelease(tpb);
					break;
			}
			sock_clear_fd(s);
		}
	}
	DisposPtr((Ptr)sockets);
	DisposPtr((Ptr)streams);
	DisposPtr((Ptr)pbList);
	
	/* release name server resources */
	(void) CloseResolver();
}

/*
 *	sock_free_fd()
 *
 *	Get the next free file descriptor >= f.  Return -1 if none available.
 */
int sock_free_fd(
	int f)
{
	int s;

	for (s = f; s < NUM_SOCKETS; s++)
	  	if (! is_used(sockets+s))
			return(s);
	return(-1);
}

/*
 *	sock_dup_fd() - duplicate a socket table entry. Very dangerous (ie. dumb) routine.
 *  IMPORTANT: It is up to the caller to straighten out the StreamHash table
 *  to reflect the new situation.  Nasty things may happen if s/he doesn't.
 */
void sock_dup_fd(
	int	s,
	int	s1)
{
	BlockMove((Ptr)(sockets+s), (Ptr)(sockets+s1), sizeof(SocketRecord));	
	sock_init_fd(s1);
}

/*
 *	sock_clear_fd() - Clear out a socket table entry freeing any 
 *                    storage attached to it.
 *  
 *                    Then re-initialize it for reuse.
 */
void sock_clear_fd(
	int s)
{
	SocketPtr sp = sockets+s;
	StreamHashEntPtr shep;
	
	if ((shep=sock_find_shep(sp->stream))!=NULL)	/* in hash table */
		shep->stream = -1;					/* mark as deleted */
		
	bzero(sp, sizeof(SocketRecord));
	sp->sa.sin_family = AF_UNSPEC;
	sp->status &= ~SOCK_STATUS_USED;

	sock_init_fd(s);
}

	
/*
 * Close relative of sock_find_shep, sock_new_shep returns a StreamHashEntPtr
 * that is unused and most appropriate for the passed stream.
 */
StreamHashEntPtr sock_new_shep(StreamPtr newStream)
	{
	StreamHashEntPtr	shep;
	int		counter,start;
	
	/* start at hash point */
	start = counter = (newStream & (SOCKETS_MASK << 3) ) >> 3;    /* extract some arbitrary bits */
	shep = streams + counter;
	do
	 	{
		/*
		 * scan till we find entry or unused or deleted slot.
		 */
		if ( (shep->stream == newStream) || (shep->stream == (StreamPtr) NULL) || 
		    (shep->stream == -1) )
			break;
		else
			{
			counter = (counter+1) & SOCKETS_MASK;
			if (counter)
				shep++;
			else
				shep = streams;
			}
		}
	while(counter != start);
	
	if ( (shep->stream == (StreamPtr) NULL ) || (shep->stream == -1 ) )
		return shep;
	else
		return NULL;	/* error: already in table or table full. Should never happen. (right...) */
	}
	
/*
 * sock_fetch_pb grabs a pb from the global pool, making sure it isn't
 * used. It may block for a long time if all pb are in progress. Declared
 * void * because I'm getting sick of typecasting every goddamn little pointer
 */ 
void *sock_fetch_pb(SocketPtr sp)
	{
	AllPb	*pb;
	do
		{
		pbLast ++;
		if (pbLast == NUM_PBS)
			pbLast = 0;
		
		pb = pbList + pbLast;
		} 
	while ( pb->tcp.ioResult == inProgress );
	
	pb->tcp.tcpStream = sp->stream;		/* all the calls have the stream at the same offset */
										/* thank god. */
	return ((void *)pb);
	}


void sock_init_fd(
	int s)
{
	SocketPtr sp = sockets+s;

	sp->fd = s;
}

 
/*
 * Convert a MacTCP err code into a unix error code, if needed. Otherwise it
 * will pass thru unmolested.
 */
int sock_err( int MacTCPerr )
	{
	switch ( MacTCPerr )
		{
		case ipBadLapErr:
		case ipBadCnfgErr:
		case ipNoCnfgErr:
		case ipLoadErr:
		case ipBadAddr:
			errno = ENXIO;			/* device not configured */	/* a cheap cop out */
			break;
			
		case connectionClosing:
			errno = ESHUTDOWN;		/* Can't send after socket shutdown */
			break;

		case connectionExists:
			errno = EISCONN;		/* Socket is already connected */
			break;

		case connectionTerminated:
			errno = ENOTCONN;		/* Connection reset by peer */  /* one of many possible */
			break;

		case openFailed:
			errno = ECONNREFUSED;	/* Connection refused */
			break;

		case duplicateSocket:		/* technically, duplicate port */
			errno = EADDRINUSE;		/* Address already in use */
			break;
			
		case ipDestDeadErr:
			errno = EHOSTDOWN;		/* Host is down */
			break;
			
		case ipRouteErr:
			errno = EHOSTUNREACH;	/* No route to host */
			break;
			
		default:
			errno = MacTCPerr;		/* cop out; an internal err, unix err, or no err */
			break;
		}
#if	SOCK_UTIL_DEBUG >= 1
	dprintf("SOCK  error %d\n", errno);
#endif

    errno_long = errno;
	return (-1);
	}

/*
 *  sock_copy_addr
 */
void sock_copy_addr(
	void *from,
	void *to,
	Int4 *tolen)
{
	*tolen = min(*tolen, sizeof(struct sockaddr_in));
	BlockMove((Ptr)from, (Ptr)to, *tolen);
}


#if SOCK_UTIL_DEBUG > 1
/*
 * print the socket records.
 */
void sock_dump()
{
	int s;
	char title[20];

	for (s=0; s<NUM_SOCKETS; s++) 
	{
		if (! is_used(sockets+s))
			continue;
		sprintf(title,"%2d",s);
		sock_print(title,sockets+s);
	}
}

void sock_print(
	char *title,
	SocketPtr sp)
{
	dprintf("%s: %08x %s %08x addr %08x/%d %08x/%d state %d/%d err %d\n",
			title, sp, 
			(sp->protocol == IPPROTO_UDP ? "udp" : "tcp"),
			(sp->protocol == IPPROTO_UDP ? sp->pb.udp.udpStream : sp->pb.tcp.tcpStream),
			sp->sa.sin_addr.s_addr,sp->sa.sin_port,
			sp->peer.sin_addr.s_addr,sp->peer.sin_port,
			sp->sstate,xTCPState(&sp->pb),
			sp->asyncerr);
}
#endif



#pragma segment SOCK_RESIDENT
/*
 * sock_find_shep returns a StreamHashEntPtr for the passed
 * stream. Will return 0 if the stream doesn't exits.
 */
StreamHashEntPtr sock_find_shep(StreamPtr theStream)
	{
	StreamHashEntPtr	shep;
	int 	counter,start;
	
	if (!goodptr(theStream))				/* DO NOT CHANGE THESE TWO LINES! */
		return NULL;						/* used to load the resident segment */
		
	/* start at hash point */
	start = counter = (theStream & (SOCKETS_MASK << 3) ) >> 3;    /* extract a bunch of arbitrary bits */
	shep = streams + counter;
	do
	 	{
		/*
		 * scan till we find entry or unused slot. Uses linear
		 * collision resolution because it's too complicated to
		 * do anything else for this small of a hash table.
		 */
		if ( (shep->stream == theStream) || (shep->stream == (StreamPtr) NULL))
			break;
		else
			{
			counter = (counter+1) & SOCKETS_MASK;
			if (counter)
				shep++;
			else
				shep = streams;
			}
		}
	while(counter != start);
	
	if ( shep->stream == theStream ) /* found it */
		return shep;
	else
		return NULL;
	}
