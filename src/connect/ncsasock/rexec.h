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
 * Revision 1.2  1995/05/17  17:57:07  epstein
 * add RCS log revision history
 *
 */

/*
 * REXECPORT should be passed as the second parameter to rexec (512)
 *
 * Remember to provide an stderr (fd2p) socket if you want to use
 * rexecerr
 */

#ifdef __cplusplus
extern "C" {
#endif

int rexec(
	char **ahost,
	Int4 inport,
	char *user,
	char *passwd,
	char *cmd,
	int *fd2p);
	
int rexecerr(Int4 sock,char *str,Int4 strln);

#define REXECPORT	512	

#ifdef __cplusplus
}
#endif
