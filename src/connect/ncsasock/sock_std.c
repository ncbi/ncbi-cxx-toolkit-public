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
* Revision 1.2  1995/05/17 17:57:56  epstein
* add RCS log revision history
*
 */

/*
 *	A really hacked up stdio interface for the MacTCP socket library
 *	
 *	routines:
 *
 *		fdopen
 *		fileno
 *		fgetc
 *		ungetc
 *		fread
 *		fputc
 *		fwrite
 *		fprintf
 *		fflush
 *		fclose
 *		ferror
 *		feof
 *		clearerr
 *
 *	hacks:
 *
 *		the stdio data are stored in the socket descriptor. fdopen() calls
 *		simply return a pointer to the descriptor. The first call per
 *		socket initializes stdio for both read and write. (The "rw"
 *		parameter to fdopen() is ignored.) The first fclose() will destroy
 *		all stdio streams open on the socket.
 *
 *		ungetc will return EOF if the buffer is full.
 *
 *		printf uses a fixed size buffer to build the message.
 *
 *	non-blocking i/o
 *
 *		all read operations which would block return EOF with errno set 
 *		to EWOULDBLOCK courtesy of the read() call.
 *
 *		write operations hide the EINPROGRESS 'error' from the write()
 *		call, but generate EALREADY when a second operation is attempted.
 *
 *		NOTE: the write() call ties up the stdio buffer until it finishes. 
 *
 *		How to code a write operation....
 *
 *		for(;;)
 *		{
 *			errno = 0;
 *			if (s_fprintf(stream, blah, blah) != EOF)
 *				break;
 *			
 *			if (errno == EALREADY)
 *			{
 *				Handle_Mac_Events();
 *				continue;
 *			}
 *			else
 *			{
 *				a real error ...
 *			}
 *		}
 *
 *		How to code a read ...
 *
 *		for(;;)
 *		{
 *			errno = 0;
 *			c = s_fgetc(inFile);
 *			if (c != EOF)
 *				break;
 *			if (errno == EWOULDBLOCK || errno == EALREADY)
 *			{
 *				Handle_Mac_Events();
 *				continue;
 *			}
 *			else if (errno == 0)
 *			{
 *				a real end of file ...
 *			}
 *			else
 *			{
 *				a real error ...
 *			}
 *		}
 */
 
#ifdef USEDUMP
# pragma load "Socket.dump"

#else
# include <Events.h>
# include <Memory.h>
# include <Types.h>
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

#include <StdArg.h>

extern SocketPtr sockets;
extern SpinFn spinroutine;

/*
 *	tuneable constants
 */
#define SOCK_IOINBUF_SIZE		128		/* Size of stdio input buffer */
#define SOCK_IOOUTBUF_SIZE		128		/* Size of stdio output buffer */

static struct timeval select_poll = {0,0};

/*
 *	s_fdopen() - open a stdio stream on a socket
 */
Ptr s_fdopen(
	int fd,
	char *type)
{
#pragma unused(type)
	SocketPtr sp;
	
#if	SOCK_STDIO_DEBUG >= 3
	dprintf("s_fdopen: opening on fd %d\n",fd);
#endif
	if (! sock_good_fd(fd)) 
	{
		(void)sock_err(EBADF);
		return(NULL);
	}
		
	sp = sockets+fd;
	if (is_stdio(sp)) 
		return((Ptr)sp);
	
	sp->inbuf = (char *)NewPtr(SOCK_IOINBUF_SIZE);
	if (sp->inbuf == NULL) 
	{
		errno = ENOMEM;
		return(NULL);
	}
	sp->outbuf = (char *)NewPtr(SOCK_IOOUTBUF_SIZE);
	if (sp->outbuf == NULL) 
	{
		DisposPtr(sp->inbuf);
		errno = ENOMEM;
		return(NULL);
	}
	
	sp->inbufptr  = sp->inbuf;
	sp->inbufcount  = 0;
	sp->outbufptr = sp->outbuf;
	sp->outbufcount = 0;
	
	sp->ioerr = false;
	sp->ioeof = false;

	return((Ptr)sp);
}

/*
 *	s_fileno()
 */
int s_fileno(
	SocketPtr sp)
{
#if	SOCK_STDIO_DEBUG >= 3
	dprintf("s_fileno:  on FILE * %08x\n",sp);
#endif
	if (! is_stdio(sp)) 
	{
		return(sock_err(EBADF));
		return(EOF);
	}
#if	SOCK_STDIO_DEBUG >= 5
	dprintf("   returning fd %d\n",sp->fd);
#endif
	return(sp->fd);
}

/*
 *	s_fgetc()
 *
 */
int s_fgetc(
	SocketPtr sp)
{
	char c;	
		
	if (stdio_read(sp, &c, 1) != 1)
		return(EOF);
		
	return(c);
}

/*
 *	s_ungetc()
 *
 */
int s_ungetc(
	char c,
	SocketPtr sp)
{
	
#if	SOCK_STDIO_DEBUG >=3
	dprintf("s_ungetc: %08x\n",sp);
#endif

	if (! is_stdio(sp)) 
	{
		(void)sock_err(EBADF);
		return(EOF);
	}

	if (sp->ioeof)		/* Once an EOF; Always an EOF */
		return(EOF);
		
	/*
	 *	Pop onto buffer, if there is room. 
	 */
	if (sp->inbufptr == sp->inbuf)
		return(EOF);
	else 
	{
		*(sp->inbufptr++) = c;
		sp->inbufcount++;
		return(0);
	}
}

/*
 *	s_fread()
 */
int s_fread(
	char *buffer,
	int size,
	int nitems,
	SocketPtr sp)
{
	return(stdio_read(sp, buffer, size*nitems));
}
	
/*
 *	stdio_read()
 *
 *	Buffered i/o.  Read buflen chars into buf.
 *  Returns length read, EOF on error.
 */
static int stdio_read(
	SocketPtr sp,
	char *buffer,
	int buflen)
{
	unsigned long tocopy;
	Ptr buf;
	unsigned long len;
	int cache = false;	/* a flag ===> read into sp->inbuf */

#if	SOCK_STDIO_DEBUG >=3
	dprintf("stdio_read: %08x for %d bytes\n",sp,buflen);
#endif	

	if (! is_stdio(sp)) 
	{
		(void)sock_err(EBADF);
		return(EOF);
	}

	if (sp->ioeof)		/* Once an EOF; Always an EOF */
		return(EOF);

	/*
	 *	return already buffered characters
	 */
	if (sp->inbufcount != 0) 
	{
		tocopy = min(sp->inbufcount, buflen);
		bcopy(sp->inbufptr, buffer, tocopy);
		sp->inbufptr += tocopy;
		sp->inbufcount -= tocopy;
		return(tocopy);
	}

	if (buflen > SOCK_IOINBUF_SIZE) 
	{
		/*
		 *	Read into user's buffer
		 */
		buf = buffer;
		len = buflen;
	}
	else 
	{
		/*
		 *	Read into stdio buffer
		 */
		cache = true;
		buf 	= sp->inbuf;
		len		= SOCK_IOINBUF_SIZE;
	}

	len = s_read(sp->fd, buf, len);
	switch(len) 
	{
		case -1:
			sp->ioerr = true;
			return(EOF);
			
		case 0:
			sp->ioeof = true;
			return(EOF);
	}
	if (cache) 
	{
		tocopy = min(buflen, len);
		bcopy(sp->inbuf, buffer, tocopy);	/* copy to client's buffer */
		sp->inbufcount 	= len - tocopy;
		sp->inbufptr 	= sp->inbuf + tocopy;
		return(tocopy);
	}
	return(len);			
}


/*
 *	s_fputc()
 *
 */
int s_fputc(
	char c,
	SocketPtr sp)
{
	if (stdio_write(sp, &c, 1) == EOF)
		return(EOF);
		
	return(c);
}
				
/*
 *	s_fprintf()
 * Modified to use StdArg macros by Charlie Reiman
 * Wednesday, August 8, 1990 2:52:31 PM
 *
 */
int s_fprintf(
	SocketPtr sp,
	char *fmt,
	...)
{
	va_list	nextArg;
	int len;
	char buf[1000];
	
	va_start(nextArg,fmt);
	
	(void) vsprintf(buf,fmt,nextArg);
	len = strlen(buf);
	return(stdio_write(sp, buf, len));
}

/*
 *	s_fwrite()
 *
 */

int s_fwrite(
	char *buffer,
	int size,
	int nitems,
	SocketPtr sp)
{
	return(stdio_write(sp, buffer, size*nitems));
}

/*
 *	stdio_write()
 *
 *	Buffered i/o.  Move buflen chars into stdio buf., writing out as necessary.
 *	Returns # of characters written, or EOF.
 */
static int stdio_write(
	SocketPtr sp,
	char *buffer,
	unsigned long buflen)
{
	long buffree;
	struct iovec iov[2];
	
#if	SOCK_STDIO_DEBUG >=3
	dprintf("stdio_write: %08x for %d bytes\n",sp,buflen);
#endif	

	if (! is_stdio(sp))
	{
		(void) sock_err(EBADF);
		return(EOF);
	}
	
	/* will the new stuff fit in the buffer? */
	buffree = SOCK_IOOUTBUF_SIZE - sp->outbufcount;
	if (buflen < buffree)
	{
		/* yes...add it in */
		bcopy(buffer, sp->outbufptr, buflen);
		sp->outbufptr += buflen;
		sp->outbufcount += buflen;
		return(buflen);
	}
	else
	{
		/* no...send both buffers now */
		iov[0].iov_len = sp->outbufcount;
		iov[0].iov_base = sp->outbuf;
		iov[1].iov_len = buflen;
		iov[1].iov_base = buffer;
		/* hide the 'error' generated by a non-blocking write */
		if (s_writev(sp->fd,&iov[0],2) < 0 && errno != EINPROGRESS)
		{
			sp->ioerr = true;
			return(EOF);
		}

		sp->outbufptr = sp->outbuf;
		sp->outbufcount = 0;

		return(buflen);
	}
}

/*
 *	s_fflush()
 *
 */
int s_fflush(
	SocketPtr sp)
{
#if	SOCK_STDIO_DEBUG >=3
	dprintf("s_fflush: %08x\n",sp);
#endif
	if (! is_stdio(sp))
	{
		(void)sock_err(EBADF);
		return(EOF);
	}
	
	if (sp->outbufcount == 0)
		return(0);

	if (s_write(sp->fd,sp->outbuf,sp->outbufcount) < 0)
		/* hide the 'error' generated by non-blocking I/O */
		if (errno != EINPROGRESS)
		{
			sp->ioerr = true;
			return(EOF);
		}

	sp->outbufptr = sp->outbuf;
	sp->outbufcount = 0;
	return(0);
}

/*
 *	s_fclose() - close the stdio stream AND the underlying socket
 */
int s_fclose(
	SocketPtr sp)
{	
#if	SOCK_STDIO_DEBUG >=3
	dprintf("s_fclose: %08x\n",sp);
#endif
	
	if (s_fflush(sp) == EOF) /* flush validates sp */
		return(EOF);

	if (sp->inbuf != NULL) DisposPtr(sp->inbuf);
	if (sp->outbuf != NULL) DisposPtr(sp->outbuf);
	
	return(s_close(sp->fd));
}

/*
 *	s_ferror()
 */
int s_ferror(
	SocketPtr sp)
{
	if (! is_stdio(sp))
	{
		(void)sock_err(EBADF);
		return(EOF);
	}
	return(sp->ioerr);
}

/*
 *	s_feof()
 */
int s_feof(
	SocketPtr sp)
{
	if (! is_stdio(sp))
	{
		(void)sock_err(EBADF);
		return(EOF);
	}	
	return(sp->ioeof);
}

/*
 *	s_clearerr()
 */
int s_clearerr(
	SocketPtr sp)
{	
	if (! is_stdio(sp))
	{
		(void)sock_err(EBADF);
		return(EOF);
	}
	sp->ioerr = false;
	sp->ioeof = false;
	return (0);
}

