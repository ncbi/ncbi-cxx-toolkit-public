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
 * rexec.c
 * Mac implimentation of rexec. Uses NCSA MacTCP sockets.
 * Written by Charlie Reiman - Tom actually had nothing to do with this file
 * Started Friday, July 13, 1990 10:04:33 AM
 */
 
# include <Events.h>
# include <Types.h>
# include <Stdio.h>

# include <s_types.h>
# include <netdb.h>
# include <neti_in.h>
# include <s_socket.h>
# include <s_time.h>
# include <neterrno.h>
# include <string.h>

# include <MacTCPCommonTypes.h>

#include "rexec.h"
#include "sock_ext.h"		/* uses only high level calls */

#define BUFZ 256

#ifndef min
#define min(a,b) ( ( a < b ) ? a : b )
#endif

#define NIL NULL
#define DOERR	if ( anErr < 0 ) return anErr;

extern	int	errno;

int rexec(
	char **ahost,
	Int4 inport,
	char *user,
	char *passwd,
	char *cmd,
	int *fd2p)
	
	{
	struct	hostent *hp;
	struct  sockaddr_in	sa,me,her;
	int		sockOut;
	int		anErr;		/* an error? */
	Int4 	herlen = sizeof(her),melen;
	char	wbuf[BUFZ];
	
	
	hp=gethostbyname(*ahost);


	if ( hp == NULL )
		{
		int		a,b,c,d;
		
		if ( sscanf(*ahost,"%d.%d.%d.%d",&a,&b,&c,&d) != 4 )
			{
			errno = errno_long = EHOSTUNREACH;
			return -1;
			}
		else
			{
			herlen = (a<<24) | ( b<<16) | (c<<8) | (d);
			bcopy(&herlen,&sa.sin_addr,4);
			}
		}
	else
		bcopy(hp->h_addr,&sa.sin_addr,hp->h_length);

	
	sa.sin_port=htons(inport);
	sa.sin_family=AF_INET;

	sockOut=s_socket(AF_INET,SOCK_STREAM,0);
	
	if (sockOut<0)
		{
		return -1;
		}
	
	if (s_connect(sockOut,(struct sockaddr *)&sa,sizeof(sa))<0)
		{
		return -1;
		}
		
	if (fd2p)
		{
		/* create listening socket */
		*fd2p=s_socket(AF_INET,SOCK_STREAM,0);

		bzero((char *)&me, sizeof(me));
		me.sin_family = AF_INET;
		me.sin_port = htons(0);		/* any port */
		
		if (s_bind(*fd2p, (struct sockaddr *)&me, sizeof(me) ) < 0)
			{
			return -1;
			}

		if (s_listen(*fd2p,1) < 0)
			{
			/* 
			 * s_listen is the routine that actuall puts the correct
			 * IP address to the socket. Up to this point, the socket
			 * has had an address of 0, port 0.
			 */
			return -1;
			}
		/*
		 * Now we must fetch the number of the port so we can tell
		 * the rexecd where to find us.
		 */
		melen=sizeof(me);
		s_getsockname(*fd2p, (struct sockaddr *)&me , &melen);
		sprintf(wbuf,"%d\0",(long)me.sin_port);	/* tell rexec where to hook up to */
		}
	else
		*wbuf='\0';		/* pass an empty string for no stderr port */
		
	anErr = s_write (sockOut,wbuf,strlen(wbuf)+1);		/* write out string */
	
	DOERR
	
	if (fd2p)	/* do we need to accept? */
		{
		struct		timeval selectPoll;
		fd_set		readfds;
		
		selectPoll.tv_sec=60;	/* block for one minute */
		selectPoll.tv_usec=0;
		FD_ZERO(&readfds);
		FD_SET(*fd2p,&readfds);	/* only looking for connect on one socket */
		
		anErr = s_select(32, &readfds, (fd_set *)0,
				(fd_set *)0, &selectPoll);
		
		if (anErr < 1 )
			return -1;			
			
		anErr = s_accept_once(*fd2p,(struct sockaddr *)&her,&herlen);
		/*
		 * might want to check and make sure that the connected machine
		 * is the right one, but that seems a bit execessive.
		 */
		DOERR
		
		}
		
	anErr = s_write (sockOut,user,strlen(user)+1);
	DOERR
	anErr = s_write (sockOut,passwd,strlen(passwd)+1);
	DOERR
	anErr = s_write (sockOut,cmd,strlen(cmd)+1);
	DOERR
	
	/*
	 * Hey, wouldn't it be great if rexec sent encrypted passwords?
	 * And wouldn't it be great if it sent along a case of a really good beer?
	 * Like Keystone!
	 */
	 
	anErr = s_read (sockOut,wbuf,1);	/* fetch answer */
	DOERR
	
	if (*wbuf!=0)
		{
		errno = errno_long = EACCES;
		return -1;  /* permission denied! */
		}
	
	return sockOut;
	}

/*
 * Polls an exisiting stderr hookup from a previous rexec for any error
 * text. If there is, it will be returned in str, up to strln bytes
 *
 * Assumes sock is connected properly and str is not null.
 */
int rexecerr(Int4 sock,char *str,Int4 strln)
	{
	fd_set	rfds;
	struct	timeval tv;
	int		selectcode;
	
	FD_ZERO(&rfds);
	
	FD_SET(sock,&rfds);
	
	tv.tv_sec=1;
	tv.tv_usec=0;
	
	selectcode = s_select(sock+1,&rfds,(fd_set *)NULL,(fd_set *)NULL,&tv);
	
	if ( selectcode < 0 )
		return -1;
	else if ( !selectcode )
		return 0;
	else
		if (s_read(sock,str,strln)<0)
			return -1;
		return 1;
	
	}
