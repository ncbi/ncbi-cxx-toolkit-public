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
* Revision 1.2  1995/05/17 17:56:44  epstein
* add RCS log revision history
*
 */

/*
 * Various #defines to make the DTM source compile on a Mac and
 * use the NCSA socket library. This file written by 
 * Charlie Reiman, NCSA
 * Wednesday, July 11, 1990 9:18:40 AM
 *
 * BE CAREFUL if you use this file, as you can no longer access all of the Mac 
 * Standard C library without extreme difficulty.  Well, you might be able to, 
 * but I can't think of a nice 'n easy way to do it. 
 */
 
#define socket			s_socket
#define bind			s_bind
#define listen			s_listen
#define accept			s_accept
#define connect			s_connect
#define read			s_read
#define recvfrom		s_recvfrom
#define recv			s_recv
#define writev			s_writev
#define write			s_write
#define sendto			s_sendto
#define send			s_send
#define sendmsg			s_sendmsg
#define select			s_select
#define close			s_close
#define getdtablesize	s_getdtablesize
#define getsockname		s_getsockname
#define getpeername		s_getpeername
#define shutdown		s_shutdown
#define	fcntl			s_fcntl
#define dup2			s_dup2
#define dup				s_dup
#define ioctl			s_ioctl
#define setsockopt		s_setsockopt
#define perror			s_perror

/*
 * There is also a s_spinroutine, but there is no unix parallel for this,
 * so it has no place in this list.
 */
