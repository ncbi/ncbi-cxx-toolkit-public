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
* Revision 6.0  1997/08/25 18:38:15  madden
* Revision changed to 6.0
*
* Revision 4.0  1995/07/26 13:56:09  ostell
* force revision to 4.0
*
 * Revision 1.4  1995/06/02  16:29:03  kans
 * *** empty log message ***
 *
 * Revision 1.3  1995/05/23  15:31:16  kans
 * new CodeWarrior 6 errors and warnings fixed
 *
 * Revision 1.2  1995/05/17  17:58:06  epstein
 * add RCS log revision history
 *
 */

/*
 *
 *	The following calls are implemented
 *
 *		socket
 *		bind
 *		listen
 *		accept
 *		connect
 *		read
 *		recv
 *		recvfrom
 *		write
 *		writev
 *		send
 *		sendto
 *		select
 *		close
 *		getdtablesize
 *		getsockname
 *		getpeername
 *		shutdown
 *		fcntl(F_DUPFD)
 *		fcntl(F_GETFL)
 *		fcntl(F_SETFL,FNDELAY)
 *		dup
 *		dup2
 *		ioctl(FIONBIO)
 *		ioctl(FIONREAD)
 *
 *	Non-blocking I/O is supported. All calls which would block return
 *	immediately with an 'error' indicating so. Select() may be used to
 *	determine when an operation can be performed.
 *
 *	Ioctl(FIONBIO) or fcntl(F_SETFL,FNDELAY)  can be used to toggle or set
 *  the blocking status of a socket.
 *
 *	In a blocking situation, accept() and read() return EWOULDBLOCK and 
 *	refuse to do anything. Select() for read() to learn when a incoming 
 *	connection is available. 
 *
 *	Connect() and write() (which shouldn't take too long anyway) start 
 *	the operation and return EINPROGRESS. Select for write() to learn
 *	when connect() has completed. Write() on a socket which is still
 *	'inprogress' return EALREADY.
 *
 *
 *	Socket operations are single threaded and half-duplex. Fixing this is
 *	left as an execise for the reader. Shouldn't be a terrible problem
 *	anyway. Read() never blocks and write() only blocks for long if there
 *	is a problem.
 *
 *	Calls which find the socket busy will return EALREADY. These are
 *	read() or write() with a connect() or write() in progress.
 *
 *
 *	Socket options are not supported. Hence no setsockopt() and getsockopt() 
 *	calls.
 *
 *
 *  CR: I attempted to support OOB data for send and recv, but not promises are
 *  made as I didn't have any immediate tests for them.
 *
 *
 *	Readv() is not implemented.
 *
 *
 *	All calls which encounter an error will set the global variable errno
 *	to indicate the problem. Some common values for errno are...
 *
 *		EBADF        the socket parameter is not a valid socket descriptor.
 *		
 *		EFAULT       a pointer parameters is rubbish.
 *		
 *		EINVAL       a non-pointer parameters is rubbish.
 *
 *		ENOTCONN     the socket should be in a connected state for this 
 *                   operation, but isn't.
 *
 *		EISCONN      the socket is already connected.
 *
 * ----------------- non-blocking I/O
 *
 *		EWOULDBLOCK  accept() or one of the read() calls would block. 
 *
 *		EINPROGRESS  connect() or one of the write() operations has been 
 *                   started.
 *
 * ----------------- SINGLE THREAD
 *
 *		EALREADY     an operation is already in progress on the socket.
 *
 * ----------------
 *
 *		EBUG  	     an internal error occured.
 */
 
#ifdef USEDUMP
# pragma load "socket.dump"

#else
# include <Events.h>
# include <Types.h>
# include <Stdio.h>

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

#include "sock_ext.h"

int s_recvfrom(Int4 s, void *buffer, Int4 buflen, Int4 flags, struct sockaddr *from, int *fromlen);
int s_really_send(Int4 s, void *buffer, Int4 count, Int4 flags, struct sockaddr_in *to);
void bzero(char *sp, int len);


/*
 *   GET YOUR GLOBALS HERE!
 */

int defaultSpin(spin_msg msg,long param);
SocketPtr sockets = NULL;			/* The socket table. */
AllPb *pbList = NULL;				/* The pb array */
short pbLast = 0;					/* last pb used */
StreamHashEntPtr streams = NULL;	/* The streams hash table */
SpinFn spinroutine = (SpinFn) defaultSpin;	/* The spin routine. */ 

/*
 *	s_socket(domain, type, protocol)
 *
 *		socket creates a MacTCP socket and returns a descriptor.
 *				 
 *		Domain must be AF_INET
 *		 
 *		Type may be SOCK_STREAM to create a TCP socket or 
 *		SOCK_DGRAM to create a UDP socket.
 *				 
 *		Protocol is ignored. (isn't it always?)
 *				 
 *		TCP sockets provide sequenced, reliable, two-way connection
 *		based byte streams.
 *
 *		A TCP socket must be in a connected
 *		state before any data may be sent or received on it. A 
 *		connection to another socket is created with a connect() call
 *		or the listen() and accept() calls.
 *		Once connected, data may be transferred using read() and
 *		write() calls or some variant of the send() and recv()
 *		calls. When a session has been completed a close() may  be
 *		performed.
 *
 *		
 *		A UDP socket supports the exchange of datagrams (connectionless, 
 *		unreliable messages of a fixed maximum length) with  
 *		correspondents named in send() calls. Datagrams are
 *		generally received with recv(), which returns the next
 *		datagram with its return address.
 *
 *		An fcntl() or ioctl() call can be used to enable non-blocking I/O.
 *
 *		The return value is a descriptor referencing the socket or -1
 *		if an error occurs, in which case global variable errno is
 *		set to one of:
 *
 *			ENOMEM				Failed to allocate memory for the socket
 *                              data structures.
 *		
 *			EAFNOSUPPORT     	Domain wasn't AF_INET.
 *
 *			ESOCKTNOSUPPORT     Type wasn't SOCK_STREAM or SOCK_DGRAM.
 *
 *			EMFILE              The socket descriptor table is full.
 */
int s_socket(
	Int4 domain,
	Int4 type,
	short protocol)
{
	SocketPtr sp;
	int s;

#if	SOCK_DEBUG >= 3
	dprintf("s_socket:\n");
#endif

	sock_init();
	
	/*
	 * Support only Internet family
	 */
	if (domain != AF_INET)
		return(sock_err(EAFNOSUPPORT));
		
	switch(type) 
	{
		case SOCK_DGRAM:
			protocol = IPPROTO_UDP;
			break;
		case SOCK_STREAM:
			protocol = IPPROTO_TCP;
			break;
		default:
			return(sock_err(ESOCKTNOSUPPORT));
	}
		
	/*
	 *	Create a socket table entry 
	 */
	s = sock_free_fd(0);		/* Get next free file descriptor */
	if (s == -1)
		return(sock_err(EMFILE));
	sp = sockets+s;
	sp->fd = s;
	sp->protocol = protocol;
	bzero((char *) &sp->sa, sizeof(struct sockaddr_in));
	bzero((char *) &sp->peer, sizeof(struct sockaddr_in));
	sp->sa.sin_family		= AF_INET;
	sp->sa.sin_len			= sizeof(struct sockaddr_in);
	sp->status				= SOCK_STATUS_USED;
	sp->nonblocking			= false;


	/* Create a new stream on the socket. */
	switch(sp->protocol) 
	{
		case IPPROTO_UDP:
			/* udp streams are not created until the last minute */
			/* because we dont know if the caller wants to assign */
			/* a special port number yet (via bind) and mactcp */
			/* assigns udp port numbers at stream creation */
			sp->sstate = SOCK_STATE_NO_STREAM;
			break;

		case IPPROTO_TCP:
			/* the tcp stream is created now because tcp port numbers */
			/* are assigned during active/passiveOpen which is done */
			/* during listen or connect */
			sp->sstate = SOCK_STATE_UNCONNECTED;
			if (sock_tcp_new_stream(sp) < 0)
				return(-1); /* sock_err already called */
	}

	return(s);
}


/*
 *	s_bind(s, name, namelen)
 *
 *		bind requests that the name (ip address and port) pointed to by 
 *		name be assigned to the socket s.
 *		
 *		The return value is 0 on success or -1 if an error occurs,
 *		in which case global variable errno is set to one of:
 *
 *		EAFNOSUPPORT        The address family in name is not AF_INET.
 *		
 *		EINVAL              The socket is already bound to an address.
 *		
 *		EADDRNOTAVAIL       The specified address is  not  available
 * 	                        from the local machine. ie. the address
 *	                        portion of name was not this machine's address.
 *
 *		MacTCP does not separate name binding and connection establishment.
 *		Therefore the port number is not verified, just stored for later use.
 *
 *		If a specific local port is not required, bind is optional in this
 *		implementation.
 */	
int s_bind( 
	Int4 s, 
	struct sockaddr *sa_name,
	Int4 namelen)
{
	SocketPtr sp;
	struct sockaddr_in *name=(struct sockaddr_in *)sa_name;
	
#if	SOCK_DEBUG >= 3
	dprintf("s_bind: bind %d to %08x/%d\n",
		s, name->sin_addr.s_addr, name->sin_port);
#endif

	if (! sock_good_fd(s))
		return(sock_err(EBADF));
	sp = sockets+s;
		
	if (namelen < sizeof(struct sockaddr_in))
		return(sock_err(EINVAL));

	if (!goodptr(name)) 
		return(sock_err(EFAULT));

	if (name->sin_family != AF_INET)
		return(sock_err(EAFNOSUPPORT));

	if (sp->sa.sin_port != 0) /* already bound */
		return(sock_err(EINVAL));

	/*
	 *	If client passed a local IP address, assure it is the right one
	 */
	if (name->sin_addr.s_addr != 0) 
	{
		if (name->sin_addr.s_addr != xIPAddr())
			return(sock_err(EADDRNOTAVAIL));
	}

	/*
	 *	NOTE: can't check a TCP port for EADDRINUSE
	 *	just save the address and port away for connect or listen or...
	 */
	sp->sa.sin_addr.s_addr = name->sin_addr.s_addr;
	sp->sa.sin_port = name->sin_port;
	return(0);
}

/*
 *	s_connect - initiate a connection on a MacTCP socket
 *
 *		If the parameter s is a UDP socket,
 *		then  this  call specifies the address to  which  datagrams
 *		are  to  be  sent, and the only address from which datagrams
 *		are to be received.  
 *			 
 *		If it is a TCP socket, then this call attempts to make a 
 *		connection to another socket. The other socket is specified 
 *		by an internet address and port.
 *			 
 *		TCP sockets may successfully connect() only once;
 *			 
 *		UDP sockets may use connect() multiple times to change
 *		their association. UDP sockets may dissolve the association
 *		by connecting to an invalid address, such as a null
 *		address.
 *		
 *		If the connection or binding succeeds, then 0 is returned.
 *		Otherwise a -1 is returned, and a more specific error code
 *		is stored in errno.
 *		
 *		EAFNOSUPPORT        The address family in addr is not AF_INET.
 *		
 *		EHOSTUNREACH        The TCP connection came up half-way and 
 *                          then failed.
 *			 
 *	-------------- some day instead of EHOSTUNREACH -----------------
 *		
 *		EADDRNOTAVAIL       The specified address is not available
 *		                    on the remote machine.
 *		
 *		ETIMEDOUT           Connection establishment timed out
 *		                    without establishing a connection.
 *		
 *		ECONNREFUSED        The attempt to  connect  was  forcefully
 *		                    rejected.
 *		
 *		ENETUNREACH         The network is not reachable from here.
 *		
 *		EHOSTUNREACH        The host is not reachable from here.
 *		
 *		EADDRINUSE          The address is already in use.
 */
int s_connect(
	Int4 s,
	struct sockaddr *sa_addr,
	Int4 addrlen)
{
	SocketPtr sp;
	struct sockaddr_in *addr=(struct sockaddr_in *)sa_addr;
	
#if SOCK_DEBUG >= 2
	dprintf("s_connect: connect %d to %08x/%d\n",
		s, addr->sin_addr.s_addr, addr->sin_port);
#endif
	if (! sock_good_fd(s))
		return(sock_err(EBADF));
		
	if (addrlen != sizeof(struct sockaddr_in))
		return(sock_err(EINVAL));

	if (! goodptr(addr))
		return(sock_err(EFAULT));

	if (addr->sin_family != AF_INET)
		return(sock_err(EAFNOSUPPORT));

	sp = sockets+s;
	switch(sp->protocol) 
	{
		case IPPROTO_UDP:
			return(sock_udp_connect(sp,addr));

		case IPPROTO_TCP:
			return(sock_tcp_connect(sp,addr));
	}
	return(0);
}

/*
 *	s_listen()
 *
 *		To accept connections, a socket is first created  with
 *		socket(), a backlog for incoming connections is specified
 *		with listen() and then the  connections  are  accepted  with
 *		accept(). The listen() call applies only to TCP sockets.
 *		
 *		The qlen parameter is supposed to define the maximum length
 *		the queue of pending connections may grow to. It is ignored.
 *		
 *		A 0 return value indicates success; -1 indicates an error.
 *		
 *		EOPNOTSUPP          s is not a TCP socket.
 */
int s_listen(
	Int4 s,
	Int4 qlen)
{
#pragma unused(qlen)
	SocketPtr sp;
	
#if SOCK_DEBUG >= 2
	dprintf("listen: listen %d\n", s);
#endif
	if (! sock_good_fd(s))
		return(sock_err(EBADF));
		
	sp = sockets+s;	
	switch(sp->protocol) 
	{
		case IPPROTO_UDP:
			return(sock_err(EOPNOTSUPP));
		
		case IPPROTO_TCP:
			return(sock_tcp_listen(sp));
	}
	return(0);
}

/*
 *	s_accept(s, addr, addrlen)
 *
 *		s is a socket that has been created with socket,  bind, listen.
 *		
 *		Accept  extracts the  first  connection  on the queue of pending 
 *		connections, creates a new socket with the same properties of s 
 *		and allocates  a  new  file  descriptor,  ns, for the socket.
 *		
 *		If no pending connections are present on the queue, and the socket
 *		is not marked as non-blocking, accept blocks the caller until
 *		a connection is present. 
 *		
 *		If the socket is marked non-blocking  and  no pending connections 
 *		are present on the queue, accept returns an error EWOULDBLOCK.
 *		
 *		The accepted socket, ns, is used to read and write data to and
 *		from the socket which connected to this one; it is not  used
 *		to  accept  more connections.  The original socket s remains
 *		open for accepting further connections.
 *		
 *		The argument addr is a result parameter that  is  filled  in
 *		with  the  address of the connecting socket. The addrlen is 
 *		a value-result  parameter; it should initially contain the
 *		amount of space pointed to by addr; on return it will contain 
 *		the actual length (in bytes) of the address returned.
 *		
 *		This call is used with TCP sockets only.
 *		
 *		It is possible to select a socket  for  the  purposes  of
 *		doing an accept by selecting it for read.
 *
 *		Translation: To check and see if there is a connection pending,
 *		call s_select and check for pending reads.
 *		
 *		The call returns -1 on error.  If it succeeds, it returns  a
 *		non-negative  integer  that is a descriptor for the accepted
 *		socket.
 *		
 *		EOPNOTSUPP          s is not a TCP socket.
 *		
 *		EMFILE              The socket descriptor table is full.
 */
int s_accept(
	Int4 s,
	struct sockaddr *sa_addr,
	Int4 *addrlen)
{
	SocketPtr sp;
	struct sockaddr_in *addr=(struct sockaddr_in *)sa_addr;

#if SOCK_DEBUG >= 2
	dprintf("s_accept: %d\n", s);
#endif
	if (! sock_good_fd(s))
		return(sock_err(EBADF));
		
	if (!goodptr(addr) || !goodptr(addrlen)) 
		return(sock_err(EFAULT));

	if (*addrlen < 0) 
		return(sock_err(EINVAL));

	if (sock_free_fd(0) == -1)
		return(sock_err(EMFILE));

	sp = sockets+s;
	switch(sp->protocol) 
	{
		case IPPROTO_UDP:
			return(sock_err(EOPNOTSUPP));
		case IPPROTO_TCP:
			return(sock_tcp_accept(sp,addr,addrlen));
	}
	return(0);
}

/*
 * s_accept_once
 *
 * A mac specific routine, designed to compenstate for a bug
 * in MacTCP. If you close a passive, unconnected stream,
 * MacTCP will generate an error. s_accept always creates
 * a new listening (passive open) stream that will eventually
 * need to be closed. s_accept_once does not create a new
 * listening socket. It will return the same socket originally
 * passed to it, and NO more connections will be accepted
 * on the old listening port.
 *
 * Other than that, it is identical to s_accept.
 */
int s_accept_once(
	Int4 s,
	struct sockaddr *sa_addr,
	Int4 *addrlen)
{
	SocketPtr sp;
	struct sockaddr_in *addr=(struct sockaddr_in *)sa_addr;

#if SOCK_DEBUG >= 2
	dprintf("s_accept: %d\n", s);
#endif
	if (! sock_good_fd(s))
		return(sock_err(EBADF));
		
	if (!goodptr(addr) || !goodptr(addrlen)) 
		return(sock_err(EFAULT));

	if (*addrlen < 0) 
		return(sock_err(EINVAL));

	if (sock_free_fd(0) == -1)
		return(sock_err(EMFILE));

	sp = sockets+s;
	switch(sp->protocol) 
	{
		case IPPROTO_UDP:
			return(sock_err(EOPNOTSUPP));
		case IPPROTO_TCP:
			{
			int		returnCode;
			
			returnCode=sock_tcp_accept_once(sp,addr,addrlen);
			return (returnCode ? returnCode : s );
			}
	}
	return(0);
}

/*
 *	s_close(s)
 *
 *		The close call destroys the socket s. If this is the last reference 
 *		to the underlying MacTCP stream, then the stream will be released.
 *	
 *		A 0 return value indicates success; -1 indicates an error.
 *
 *		NOTE: if non-blocking I/O is enabled EWOULDBLOCK will be returned
 *            if there are TCP writes in progress. (UDP writes are
 *            performed synchronously.)
 *
 *		      All reads are terminated and unread data is lost.
 */
int s_close(
	Int4 s)
{
	int t;
	SocketPtr sp;
	int status;
	
#if SOCK_DEBUG >= 2
	dprintf("s_close: %d\n", s);
#endif
	if (! sock_good_fd(s))
		return(sock_err(EBADF));
		
	sp = sockets+s;

	/*
	 *	Look for duplicates of the socket.  Only close down the connection
	 *	if there are no duplicates(i.e. socket not dup'd).
	 */
	 for (t = 0; t < NUM_SOCKETS; t++) 
	 {
	 	if (t == s)
			continue;
	 	if (!(sockets[t].status & SOCK_STATUS_USED))
			continue;
		else if (sockets[t].protocol == sp->protocol && sockets[t].stream == sp->stream )
		{	/* found a duplicate */
#if SOCK_DEBUG >= 3
			dprintf("s_close: found a dup at %d(%d, don't close stream\n",
				t, sockets[t].protocol);
#endif
			sock_clear_fd(s);
			return(0);
		}
	}

	/*
	 *	No duplicates, close the stream. 
	 */
	switch(sp->protocol) 
	{
		case IPPROTO_UDP:
			status = sock_udp_close(sp);
			break;
			
		case IPPROTO_TCP:
			status = sock_tcp_close(sp);
			break;
	}
	if (status != EWOULDBLOCK)
		sock_clear_fd(s);
	return(status);
}


/*
 *	s_read(s, buffer, buflen)
 *
 *	s_recv(s, buffer, buflen, flags)
 *	
 *	s_recvfrom(s, buffer, buflen, flags, from, fromlen)
 *	
 *		read() attempts to read nbytes of data from the socket s.  
 *		
 *		recv() and recvfrom() attempt to receive a message (ie a datagram) 
 *		on the socket s. 
 *
 *		from returns the address of the socket which sent the message.
 *		fromlen is the usual value-result length parameter. 
 *		
 *		Typically, read() is used with a TCP stream and recv() with
 *		UDP where the idea of a message makes more sense. But in fact,
 *		read() and recv() are equivalent.
 *		
 *		For UDP...
 *			If a message (ie. datagram) is too long to fit in the supplied 
 *			buffer, excess bytes will be discarded..
 *
 *			If no messages are available at the socket, the receive call
 *			waits for a message to arrive, unless the socket is non-
 *			blocking in which case -1 is returned with errno set to 
 *			EWOULDBLOCK.
 *
 *		For TCP...
 *			Regardless of non-blocking status, if less data is available
 *			than has been requested, only that much data is returned.
 *
 *			If the socket is marked for non-blocking I/O, and the socket 
 *			is empty, the operation will fail with the error EWOULDBLOCK.
 *			Otherwise, the operation will block until data is available 
 *			or an error occurs.
 *
 *			A return value of zero indicates that the stream has been
 *			closed and all data has already been read. ie. end-of-file.
 *		
 *		Flags is ignored.
 *		
 *		If successful, the number of bytes actually received is
 *		returned. Otherwise, a -1 is returned and the global variable
 *		errno is set to indicate the error. 
 *		
 *		ESHUTDOWN    The socket has been shutdown for receive operations.
 */
int s_read(
	Int4 s,
	char *buffer,
	Int4 buflen)
{
	int fromlen = 0;
	return(s_recvfrom(s, buffer, buflen, 0, NULL, &fromlen));
}

int s_recv(
	Int4 s,
	char *buffer,
	Int4 buflen,
	Int4 flags)
{
	int fromlen = 0;
	return(s_recvfrom(s, buffer, buflen, flags, NULL, &fromlen));
}

int s_recvfrom(
	Int4 s,
	void *buffer,
	Int4 buflen,
	Int4 flags,
	struct sockaddr *sa_from,
	int *fromlen)
{
	SocketPtr sp;
	struct sockaddr_in *from=(struct sockaddr_in *)sa_from;

#if SOCK_DEBUG >= 3
	dprintf ("s_recvfrom: %d\n", s);
#endif
	if (s < 0 || s >= NUM_SOCKETS)
		return (sock_err (EBADF));
		
	sp = sockets+s;
	if (! is_used (sp))
		return (sock_err (EBADF));
		
	if (!goodptr((char *)buffer))
		return (sock_err (EFAULT));
		
	if (buflen <= 0)
		return(sock_err(EINVAL));
		
	if (! (from == NULL || goodptr(from)) )
		return(sock_err(EFAULT));
		
	if (! (fromlen == NULL || goodptr(fromlen)) )
		return(sock_err(EFAULT));
	else
		if ( *fromlen < 0 )
			return(sock_err(EINVAL));

	if (sock_noread(sp))
		return(sock_err(ESHUTDOWN));

	switch(sp->protocol) 
	{
		case IPPROTO_UDP:
			return(sock_udp_recv(sp, (char *)buffer, buflen, flags, from, (int *)fromlen));

		case IPPROTO_TCP:
			return(sock_tcp_recv(sp, (char *)buffer, buflen, flags));
	}
	return(0);
}


/*
 *	s_write(s, buffer, buflen)
 *
 *	s_writev(s, iov, iovcnt)
 *
 *	s_send(s, buffer, buflen, flags)
 *
 *	s_sendto(s, buffer, buflen, flags, to, tolen)
 *
 *		write() attempts to write nbytes of data to the socket s from
 *		the buffer pointed to by buffer.
 *		
 *		writev() gathers the output data from the buffers specified 
 *		by the members of the iov array. Each iovec entry specifies 
 *		the base address and length of an area in memory from which 
 *		data should be written.
 *		
 *		send() and sendto() are used to transmit a message to another 
 *		socket on the socket s.
 *
 *		Typically, write() is used with a TCP stream and send() with
 *		UDP where the idea of a message makes more sense. But in fact,
 *		write() and send() are equivalent.
 *		
 *		For UDP...
 *
 *          Write() and send() operations are completed as soon as the
 *          data is placed on the transmission queue.???????
 *
 *			The address of the target is given by to.
 *
 *			The message must be short enough to fit into one datagram.
 *		
 *			Buffer space must be available to hold the message to be 
 *			transmitted, regardless of its non-blocking I/O state.
 *		
 *		For TCP...
 *			Write() and send() operations are not considered complete
 *			until all data has been sent and acknowledged.
 *
 *			If a socket is marked for non-blocking I/O, the operation
 *			will return an 'error' of EINPROGRESS.
 *		
 *			If the socket is not marked for non-blocking I/O, the write will
 *			block until space becomes available.
 *		
 *		write() and send() may be used only when the socket is in a connected
 *		state, sendto() may be used at any time.
 *
 *		Flags is ignored.
 *		
 *		These calls return the number of bytes sent, or -1 if an error 
 *		occurred.
 *		
 *		EINVAL           The sum of the iov_len values in the iov array was
 *						 greater than 65535 (TCP) or 65507 (UDP) or there
 *                       were too many entries in the array (16 for TCP or
 *                       6 for UDP).
 *
 *		ESHUTDOWN        The socket has been shutdown for send operations.
 *		
 *		EMSGSIZE         The message is too big to send in one datagram. (UDP)
 *
 *		ENOBUFS          The transmit queue is full. (UDP)
 */
 
int s_write(
	Int4 s,
	char *buffer,
	Int4 buflen)
{
	return(s_really_send(s, buffer, buflen, 0, NULL));
} 
 
int s_writev(
	Int4 s,
	struct iovec *iov,
	Int4 count)
{	
	int		result,tally=0;
	
	while (count--)
		{
		if ( !goodptr( iov ) )
			return sock_err(EFAULT);
		result= s_really_send(s, (char *)(iov->iov_base),
				(int)(iov->iov_len), 0, NULL);
		if (result < 0)
			return sock_err(result);
		iov++;
		tally+=result;
		}
	return tally;
} 

int s_send(
	Int4 s,
	char *buffer,
	Int4 buflen,
	Int4 flags)
{
	return(s_really_send(s, buffer, buflen, flags, NULL));
} 
 
int s_sendto (
	Int4 s,
	char *buffer,
	Int4 buflen,
	Int4 flags,
	struct sockaddr *sa_to,
	Int4 tolen)
{
	SocketPtr sp;
	struct sockaddr_in *to=(struct sockaddr_in *)sa_to;

	if (s < 0 || s >= NUM_SOCKETS)
		return (sock_err (EBADF));
		
	sp = sockets+s;
	
	if (! is_used (sp))
		return (sock_err (EBADF));
		
	if (!goodptr(buffer))
		return (sock_err (EFAULT));
		
	if (buflen <= 0)
		return(sock_err(EINVAL));
		
	if (to != NULL && !goodptr(to))
		return(sock_err(EFAULT));
		
	if (to != NULL && tolen < sizeof(struct sockaddr_in))
		return(sock_err(EINVAL));

	return(s_really_send(s, buffer, buflen, flags, to));
}

int s_sendmsg(Int4 s,struct msghdr *msg,Int4 flags) {
	SocketPtr sp;
	struct iovec *iov=NULL;
	int tally=0,result,count;

	if (s < 0 || s >= NUM_SOCKETS)
		return (sock_err (EBADF));
		
	sp = sockets+s;
	
	if (! is_used (sp))
		return (sock_err (EBADF));
	if (!goodptr(msg))
		return (sock_err (EFAULT));
	if ( msg->msg_name != NULL && !goodptr(msg->msg_name) ) 
		return (sock_err (EFAULT));
	if ( msg->msg_name != NULL && msg->msg_namelen < sizeof (struct sockaddr_in))
		return (sock_err (EFAULT));
	
	count = msg->msg_iovlen;
	iov = msg->msg_iov;
	while ( count -- ) {
		if ( !goodptr( iov ) )
			return sock_err(EFAULT);
		result= s_really_send(s, (char *)(iov->iov_base),
				(int)(iov->iov_len), flags,(struct sockaddr_in *)msg->msg_name);
		if (result < 0)
			return (sock_err(result));
		iov++;
		tally+=result;
		}
	return tally;
	}

int s_really_send(
	Int4 s,
	void *buffer,
	Int4 count,
	Int4 flags,
	struct sockaddr_in *to)
	
	{
	SocketPtr	sp;
	int			tally=0;

#if SOCK_DEBUG >= 2
	dprintf("s_really_send: %d  %d bytes", s, count);
	if (to != NULL)
		dprintf(" to %08x/%d",to->sin_addr.s_addr,to->sin_port);
	dprintf("\n");
#endif
	if (! sock_good_fd(s))
		return(sock_err(EBADF));
	sp = sockets+s;

#if SOCK_DEBUG >= 2
	dprintf("state %d\n",sp->sstate);
	dprintf("peer %08x/%d\n",sp->peer.sin_addr.s_addr,sp->peer.sin_port);
#endif
	
	if (sock_nowrite(sp))
		return(sock_err(ESHUTDOWN));

	switch(sp->protocol) 
		{
		case IPPROTO_UDP:
			if (to == NULL && sp->sstate != SOCK_STATE_CONNECTED) 
				return(sock_err(ENOTCONN));
			return(sock_udp_send(sp, to, (char *)buffer, count, flags));
			break;
			
		case IPPROTO_TCP:
			if (to != NULL) /* sendto */
				return(sock_err(EOPNOTSUPP));
			if (sp->sstate != SOCK_STATE_CONNECTED) 
				return(sock_err(ENOTCONN));
			return ( sock_tcp_send(sp, (char *)buffer, count,0 ));
			break;
		}
	return(0);
	}


/*
 *	s_select(width, readfds, writefds, exceptfds, timeout)
 *
 *		select() examines the I/O descriptor  sets  whose  addresses
 *		are  passed  in  readfds,  writefds, and exceptfds to see if
 *		some of their descriptors are ready for reading,  ready  for
 *		writing, or have an exceptional condition pending.  width is
 *		the number of bits to be  checked  in  each  bit  mask  that
 *		represent  a file descriptor; the descriptors from 0 through
 *		width-1 in the  descriptor  sets  are  examined.   Typically
 *		width  has  the  value  returned by getdtablesize for the
 *		maximum number of file  descriptors.   On  return,  select
 *		replaces  the  given descriptor sets with subsets consisting
 *		of those descriptors that are ready for the requested opera-
 *		tion.  The total number of ready descriptors in all the sets
 *		is returned.
 *
 *		If timeout is not a NULL pointer,  it  specifies  a  maximum
 *		interval  to wait for the selection to complete.  If timeout
 *		is a NULL  pointer,  the  select  blocks  indefinitely.   To
 *		effect  a  poll,  the  timeout argument should be a non-NULL
 *		pointer, pointing to a zero-valued timeval structure.
 *
 *		Any of readfds, writefds, and exceptfds may be given as NULL
 *		pointers if no descriptors are of interest.
 *
 *		Using select to open a socket for reading is analogous  to
 *		performing an accept call.
 *
 *		select() returns the number of ready  descriptors  that  are
 *		contained  in  the  descriptor  sets,  or  -1  if  an  error
 *		occurred.  If the time limit expires then  select()  returns
 *		0.   If select() returns with an error the descriptor sets 
 *      will be unmodified.
 */
int s_select(
	Int4 width,
	fd_set *readfds,
	fd_set *writefds,
	fd_set *exceptfds,
	struct timeval *timeout)
{
	SocketPtr sp;
	long count;
	int s;
	long starttime, waittime;
	fd_set rd, wd, ed;
	int errorHappened;

#if SOCK_DEBUG >= 2
	dprintf("select:  socket: width %d\n",width);
#endif
	if (!goodptr(timeout) && timeout != NULL)
		return(sock_err(EFAULT));
	if (!goodptr(readfds) && readfds != NULL)
		return(sock_err(EFAULT));
	if (!goodptr(writefds) && writefds != NULL)
		return(sock_err(EFAULT));
	if (!goodptr(exceptfds) && exceptfds != NULL)
		return(sock_err(EFAULT));
		
#if SOCK_DEBUG >= 3
	dprintf("select: timeout %d sec, read %08x write %08x except %08x\n",
		(timeout ? timeout->tv_sec : 99999), 
		(readfds!=NULL ? *readfds : 0L),
		(writefds!=NULL ? *writefds : 0L),
		(exceptfds!=NULL ? *exceptfds : 0L));
#endif

	if (width > NUM_SOCKETS)	/* for now..xxx. */
		width = NUM_SOCKETS;
	count = 0;
	FD_ZERO(&rd);
	FD_ZERO(&wd);
	FD_ZERO(&ed);

	if (timeout) 
	{
		waittime = TVTOTICK(timeout->tv_sec,timeout->tv_usec);
		starttime = TickCount();
	}
#if SOCK_DEBUG >= 5
	dprintf("     starttime = %d(tics);  waittime = %d\n",starttime, waittime);
#endif

	do 
	{
		for (s = 0 , sp = sockets ; s < width ; ++s, ++sp) 
		{
			if (is_used(sp)) 
			{
				errorHappened = 0;
				
				/* Check if there is data or connection available. */
				if (readfds && FD_ISSET(s,readfds)) 
				{
						
					switch(sp->protocol) 
					{
						case IPPROTO_UDP:
							switch (sock_udp_can_recv(sp))
							{
								case 1:
									FD_SET(s,&rd);
									++count;
									break;
								case -1:
									errorHappened = 1;
									break;
							}
							break;
				
						case IPPROTO_TCP:
					/* Must exit if stream is dead to avoid eternal lock up */
							if (sock_tcp_can_read(sp) ) {
								FD_SET(s,&rd);
								++count;
								} 
							break;
					}
				}
				if (writefds && FD_ISSET(s,writefds)) 
				{
					switch(sp->protocol) 
					{
						case IPPROTO_UDP:
							switch (sock_udp_can_send(sp))
							{
								case 1:
									FD_SET(s,&wd);
									++count;
									break;
								case -1:
									errorHappened = 1;
									break;
							}
							break;
				
						case IPPROTO_TCP:
							if (sock_tcp_can_write(sp))
							{
								FD_SET(s,&wd);
								++count;
							}
							break;
					}
				}
				if (exceptfds && FD_ISSET(s,exceptfds)) 
				{
					if (errorHappened)  
					{
						FD_SET(s,&ed);
						++count;
					}
				}
			}
		}
		SPIN(false,SP_SELECT,0)
	} 
	while(count == 0 &&(timeout == 0 || TickCount() - starttime < waittime));

	if (readfds) 			
		*readfds = rd;
	if (writefds) 			
		*writefds = wd;
	if (exceptfds) 			
		*exceptfds = ed;
#if SOCK_DEBUG >= 5
	dprintf("     elapsed = %d(tics)  count %d, read %08x write %08x except %08x\n",
		TickCount()-starttime,count,
		(readfds!=NULL ? *readfds : 0L),
		(writefds!=NULL ? *writefds : 0L),
		(exceptfds!=NULL ? *exceptfds : 0L));
#endif
	return(count);
}
	
/*
 *	s_getdtablesize()
 *
 *		The entries in the socket descriptor table are numbered with small
 *		integers starting at 0. getdtablesize returns the size of the 
 *		descriptor table.
 */
int s_getdtablesize()
{
	return(NUM_SOCKETS);
}

/*
 *	s_getsockname(s, name, namelen)
 *
 *		getsockname returns the current name for the  socket s.
 *		Namelen should  be initialized to
 *		indicate the amount of space pointed to by name.  On  return
 *		it contains the actual size of the name returned (in bytes).
 *		
 *		A 0 is returned if the call succeeds, -1 if it fails.
 */

int s_getsockname(
	Int4 s,
	struct sockaddr *name,
	Int4 *namelen)
{
#if SOCK_DEBUG >= 3
	dprintf("GETSOCKNAME: %d\n", s);
#endif
	if (! sock_good_fd(s))
		return(sock_err(EBADF));

	if (! goodptr(name))
		return(sock_err(EFAULT));
		
	if (*namelen < 0)
		return(sock_err(EINVAL));

	sock_copy_addr(&sockets[s].sa, name, namelen);
	return(0);
}

/*
 *	s_getpeername(s, name, namelen)
 *
 *		getpeername returns the name of the peer connected to socket s.
 *
 *		The  int  pointed  to  by the namelen parameter
 *		should be  initialized  to  indicate  the  amount  of  space
 *		pointed  to  by name.  On return it contains the actual size
 *		of the name returned (in bytes).  The name is  truncated  if
 *		the buffer provided is too small.
 *		
 *		A 0 is returned if the call succeeds, -1 if it fails.
 */
int s_getpeername(
	Int4 s,
	struct sockaddr *name,
	Int4 *namelen)
{
	SocketPtr sp;

#if SOCK_DEBUG >= 2
	dprintf("getpeername: socket %d\n", s);
#endif
	if (! sock_good_fd(s))
		return(sock_err(EBADF));		

	sp = sockets+s;
	if (! is_used (sp))
		return (sock_err (EBADF));

	if (! goodptr(name))
		return(sock_err(EFAULT));
		
	if (*namelen < 0)
		return(sock_err(EINVAL));

	if (sp->sstate != SOCK_STATE_CONNECTED) 
		return(sock_err(ENOTCONN));

	sock_copy_addr(&sockets[s].peer, name, namelen);

	return(0);
}

/*
 *	s_shutdown(s, how)
 *
 *		shutdown call causes all or part of a full-duplex
 *		connection on the socket s to be shut down.  If
 *		how is 0, then further receives will be disallowed.  If  how
 *		is  1,  then further sends will be disallowed.  If how is 2,
 *		then further sends and receives will be disallowed.
 *		
 *		A 0 is returned if the call succeeds, -1 if it fails.
 */
int s_shutdown(
	Int4 s,
	Int4 how)
{
	SocketPtr sp;
	
#if SOCK_DEBUG >= 2
	dprintf("shutdown: shutdown %d\n", s);
#endif
	if (! sock_good_fd(s))
		return(sock_err(EBADF));
	sp = sockets+s;

	switch(how) 
	{
		case 0 : 
			sp->status |= SOCK_STATUS_NOREAD;
			break;
			
		case 1 : 
			sp->status |= SOCK_STATUS_NOWRITE;
			break;

		case 2 :
			sp->status |= SOCK_STATUS_NOREAD | SOCK_STATUS_NOWRITE;
			break;
			
		default :
			return(sock_err(EINVAL));
	}
	return(0);
}

/*
 *	fcntl() operates on the socket s according to the order in cmd:
 *
 *		F_DUPFD	Like Dup. Returns a new descriptor which refers to the 
 *				same MacTCP stream as s and has the same descriptor 
 *				status.
 *
 *		F_GETFL	returns the descriptor status flags for s. The only
 *				flag supported is FNDELAY for non-blocking i/o.
 *
 *		F_SETFL	sets descriptor status flags for s. The only
 *		 		flag supported is FNDELAY for non-blocking i/o.
 *
 *		A dup or F_DUPFD operation copies the descriptor status flags
 *		maintained by F_SETFL, but once the copy is done, the two are
 *		disjoint. THIS IS DIFFERENT FROM UNIX.
 *
 *		Upon successful completion, the value  returned  depends  on
 *		cmd as follows:
 *			F_DUPFD   A new descriptor.
 * 			F_GETFL   Value of flags.
 *			F_SETFL   0.
 *
 *		On error, a value of -1  is returned and errno is set to indicate 
 *		the error.
 *
 *		EBADF           s is not a valid open descriptor.
 *
 *		EMFILE          cmd is F_DUPFD and socket descriptor table is full.
 *
 *		EINVAL          cmd is F_DUPFD and arg  is  negative  or
 *                      greater   than   the  maximum  allowable
 *                      number (see getdtablesize).
 */
int s_fcntl(
	Int4 s,
	unsigned Int4 cmd,
	Int4 arg)
{
#if SOCK_DEBUG >= 2
	dprintf("s_fcntl: %d\n", s);
#endif

	if (! sock_good_fd(s))
		return(sock_err(EBADF));

	switch(cmd) 
	{
		/*
		 * Duplicate a socket.
		 */
		case F_DUPFD : 
		{
			int s1;
			
			if (arg < 0 || arg >= NUM_SOCKETS)
				return(sock_err(EINVAL));
			
			s1 = sock_free_fd(arg);
			if (s1 == -1)
				return(sock_err(EMFILE));
			
			sock_dup_fd(s,s1);
			return(s1);
		}
		
		/*
		 *  Get socket status.  This is like getsockopt().
		 *  Only supported descriptor status is FNDELAY.
		 */
		case F_GETFL : 
		{
			SocketPtr sp;
	
			sp = sockets+s;
			if (sp->nonblocking)
				return(FNDELAY);
			else
				return(0);
		}
		
		/*
		 *  Set socket status.  This is like setsockopt().
		 *  Only supported descriptor status is FNDELAY.
		 */
		case F_SETFL : 
		{
			SocketPtr sp;
	
			sp = sockets+s;
			if (arg & FNDELAY)
				sp->nonblocking = true;
			else
				sp->nonblocking = false;
			
			return(0);
		}
	}
	return(0);
}

/*
 *	dup(s)
 *
 *	dup2(s, news)
 *
 *		dup() duplicates an existing socket descriptor.   The  argu-
 *		ment s is a small non-negative integer index in the per-
 *		process descriptor table.  The value must be less  than  the
 *		size  of  the  table, which is returned by getdtablesize(2).
 *		The new descriptor returned by the call is the  lowest  num-
 *		bered  descriptor  that  is not currently in use by the pro-
 *		cess.
 *
 *		In the second form  of  the  call,  the  value  of  the  new
 *		descriptor  desired  is  specified.   If  that descriptor is
 *		already in use, the descriptor is first deallocated as if  a
 *		close(2) call had been done first.
 *
 *		The value -1 is returned if an error occurs in either  call.
 *		The  external  variable  errno  indicates  the  cause of the
 *		error.
 *
 *		EBADF               s or  news is  not  a  valid  socket
 *		                    descriptor.
 *
 *		EMFILE              Too many descriptors are active.
 */
int s_dup(
	Int4 s)
{
	return(s_fcntl(s, F_DUPFD, 0));
}

int s_dup2(
	Int4 s,
	Int4 s1)
{
	if (! sock_good_fd(s))
		return(sock_err(EBADF));

	if (s1 < 0 || s1 >= NUM_SOCKETS)
		return(sock_err(EBADF));

	if (is_used(sockets+s1))
	{
		if (s_close(s1) == -1)
			return(-1);
	}
	sock_dup_fd(s,s1);
	return(s1);
}


/*
 * s_Ioctl()
 */
int s_ioctl(
	Int4 d,
	Int4 request,
	Int4 *argp)
{
	struct	ifreq	*ifr;
	TCPiopb		*tpb;
	SocketPtr	sp;
	int			size;

#if SOCK_DEBUG >= 2
	dprintf("s_ioctl: %d, request %d\n", d,request);
#endif

	if (! sock_good_fd(d))
		return(sock_err(EBADF));

	sp = sockets+d;
	
	/*
	 * Interpret high order word to find amount of data to be copied 
	 * to/from the user's address space.
	 */
	size =(request &~(IOC_INOUT | IOC_VOID)) >> 16;
	
	/*
	 * Zero the buffer on the stack so the user gets back something deterministic.
	 */
	if ((request & IOC_OUT) && size)
		bzero((char *) argp, size);

	ifr =(struct ifreq *)argp;
	switch(request) 
	{
		/* Non-blocking I/O */
		case FIONBIO:
			sp->nonblocking = *(Boolean *)argp;
			return(0);

 		/* Number of bytes on input Q */
		case FIONREAD:
			tpb = sock_fetch_pb(sp);
			sp->dataavail = xTCPBytesUnread(sp);
			*(int *)argp=sp->dataavail;
			return 0;

#ifdef IOCTL_LATER	
		/*
		 *	Get interface list.  Pass in buffer and buffer length.
		 *	Returns list of length one of ifreq's 
		 */
		case SIOCGIFCONF: 
		{	
			struct ifconf *ifc =(struct ifconf *)argp;
			struct ifreq *req = ifc->ifc_req;
			int reqlen;
			struct sockaddr_in *addr;
			
			/*
			 *	Fill in req fields for the IF's name and local IP addr.
			 */
			strncpy(req->ifr_name, myIFName, IFNAMSIZ);
			addr =(struct sockaddr_in *)&req->ifr_addr;
			addr->sin_family 		= AF_INET;
			addr->sin_addr.s_addr	= myIPAddress;
			addr->sin_port			= 0;
			bzero(addr->sin_zero, sizeof(addr->sin_zero));
			ifc->ifc_len = sizeof(*req);
					
			return(0);			
		}

		/*
		 *	Returns MTU of specified IF.
		 */
		case SIOCGIFMTU: 
		{	
			/* don't check IF specification - we only have one anyway */
			*(int *)ifr->ifr_data= xMaxMTU();
			return(0);
		}
		
		/*
		 *	Returns local IP Address of IF
		 */
		case SIOCGIFADDR: 
		{
			struct sockaddr_in *addr;
			
			/* don't check IF specification - we only have one anyway */

			addr = &ifr->ifr_addr;
			addr->sin_addr.s_addr = xIPAddr();
			addr->sin_family = AF_INET;
			return(0);
		}

		case SIOCGIFDSTADDR: 		/* For point to point, which we don't support */
			return(sock_err(EINVAL));

		case SIOCGIFFLAGS: 			/* Returns IF flags(none yet) */
			ifr->ifr_flags = 0;
			return(0);
			
		/*
		 *	Return broadcast address - net address plus all ones in host part
		 */
		case SIOCGIFBRDADDR:	 
		{
			struct sockaddr_in *addr;
			
			/* don't check IF specification */
			/* we only have one and its broadcast  */
				
			addr = &ifr->ifr_addr;
			addr->sin_addr.s_addr = xIPAddr() | ~xNetMask();
			return(0);
		}
#endif /* IOCTL_LATER	 */
		default :
			return(sock_err(EOPNOTSUPP));
	}
}

/*
 *	s_setsockopt()
 *
 *		Set socket options. None implemented. In Unix there are...
 *
 *          SO_REUSEADDR        toggle local address reuse
 *          SO_KEEPALIVE        toggle keep connections alive
 *          SO_DONTROUTE        toggle routing bypass for  outgoing
 *                              messages
 *          SO_LINGER           linger on close if data present
 *          SO_BROADCAST        toggle   permission   to   transmit
 *                              broadcast messages
 *          SO_OOBINLINE        toggle  reception  of   out-of-band
 *                              data in band
 *          SO_SNDBUF           set buffer size for output
 *          SO_RCVBUF           set buffer size for input
 *          SO_TYPE             get the type  of  the  socket  (get
 *                              only)
 *          SO_ERROR            get and clear error on  the  socket
 *                             (get only)
 */
int s_setsockopt(
	Int4 s,
	Int4 level,
	Int4 optname,
	char *optval,
	Int4 optlen)
{
#pragma unused(optval)
#pragma unused(optlen)
	SocketPtr sp;

#if SOCK_DEBUG >= 3
	dprintf("SETSOCKOPT:  socket: %d  option: %d  \n", s,optname);
#endif
	if (! sock_good_fd(s))
		return(sock_err(EBADF));

	sp = sockets+s;

	/*
	 * demultiplex to socket option handlers at other protocol levels.(None
	 * supported yet).
	 */
	switch(level) 
	{
		case SOL_SOCKET : 		/* socket level option */
			switch(optname) 
			{
				default :
					return(0);
			}
			break;
		case IPPROTO_TCP:
			switch(optname)
			{
				default:
					return(0);
			}
			break;
		default :
			return(sock_err(ENOPROTOOPT));
	}
	return(0);
}


/*
 * s_setspin() - define a routine to be called repeatedly when
 *				 socket routines are blocked (ie. spinning)
 *
 *				 pass a NULL pointer to turn off a previously
 *				 defined spin routine.
 */
int s_setspin(
	SpinFn routine)
{
	if (routine == NULL || goodptr(routine))
		{
		spinroutine = routine;
		return(0);
		}
	else
		return(sock_err(EFAULT));
}

/*
 * s_getspin() - returns current spinroutine
 */
SpinFn s_getspin()
	{
	return (spinroutine);
	}
	
int defaultSpin(spin_msg msg,long param)
	{
#pragma unused (msg,param)
    EventRecord evrec;
	
	WaitNextEvent(0, &evrec, 1 /* ticks */, NULL);
	return 0;		/* return non-zero to exit current routine */
	}
