/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)ttychars.h	7.3 (Berkeley) 10/18/88
*
*
* RCS Modification History:
* $Log$
* Revision 6.0  1997/08/25 18:37:54  madden
* Revision changed to 6.0
*
* Revision 4.0  1995/07/26 13:56:09  ostell
* force revision to 4.0
*
 * Revision 1.2  1995/05/17  17:57:37  epstein
 * add RCS log revision history
 *
 */

/*
 * 4.3 COMPATIBILITY FILE
 *
 * User visible structures and constants
 * related to terminal handling.
 */
#ifndef _TTYCHARS_
#define	_TTYCHARS_

struct ttychars {
	char	tc_erase;	/* erase last character */
	char	tc_kill;	/* erase entire line */
	char	tc_intrc;	/* interrupt */
	char	tc_quitc;	/* quit */
	char	tc_startc;	/* start output */
	char	tc_stopc;	/* stop output */
	char	tc_eofc;	/* end-of-file */
	char	tc_brkc;	/* input delimiter (like nl) */
	char	tc_suspc;	/* stop process signal */
	char	tc_dsuspc;	/* delayed stop process signal */
	char	tc_rprntc;	/* reprint line */
	char	tc_flushc;	/* flush output (toggles) */
	char	tc_werasc;	/* word erase */
	char	tc_lnextc;	/* literal next character */
};
#ifdef USE_OLD_TTY
#include <sys/ttydefaults.h>	/* to pick up character defaults */
#endif
#endif /* _TTYCHARS */
