/* $Copyright:
 *
 * Copyright © 1998-1999 by the Massachusetts Institute of Technology.
 * 
 * All rights reserved.
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of M.I.T. not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  Furthermore if you modify
 * this software you must label your software as modified software and not
 * distribute it in such a fashion that it might be confused with the
 * original MIT software. M.I.T. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Individual source code files are copyright MIT, Cygnus Support,
 * OpenVision, Oracle, Sun Soft, FundsXpress, and others.
 * 
 * Project Athena, Athena, Athena MUSE, Discuss, Hesiod, Kerberos, Moira,
 * and Zephyr are trademarks of the Massachusetts Institute of Technology
 * (MIT).  No commercial use of these trademarks may be made without prior
 * written permission of MIT.
 * 
 * "Commercial use" means use of a name in a product or other for-profit
 * manner.  It does NOT prevent a commercial firm from referring to the MIT
 * trademarks in order to convey information (although in doing so,
 * recognition of their trademark status should be given).
 * $
 */

/* $Header$ */

/*
 * Internal.h -- internal header file
 */

#include "Idle.h"
//#include <Debug.h>
#include <stdlib.h>

/* Structures */

struct IdleEventHandler;

struct IdleEventHandler
{
  struct IdleEventHandler *next;
  struct IdleEventHandler *prev;
  IdleEventHandlerUPP      UPP;
  UInt32                   active;
  UInt16                   mask;
  UInt32                   refCon;
};

typedef struct IdleEventHandler IdleEventHandler;

/* Global Variables */
extern Boolean           gIsThreaded;
extern Boolean           gShouldIdle;
extern UInt32            gEventSleepTime;
extern UInt32            gIdleFrequency;
extern UInt16            gEventMask;
extern IdleEventHandler *gFirstEventHandler;
extern IdleEventHandler *gLastEventHandler;


/* How to call the event Handler UPPs */
#if TARGET_API_MAC_CARBON
#define CallIdleEventHandlerProc(userRoutine, theEvent, refCon)			\
	( (userRoutine)(theEvent, refCon) )
#else
#define CallIdleEventHandlerProc(userRoutine, theEvent, refCon)			\
	((Boolean)CALL_TWO_PARAMETER_UPP ((userRoutine), uppIdleEventHandlerProcInfo, theEvent, refCon))
#endif

/* Internal constants and default settings */
#define kDefaultIdleFrequency   6
#define kDefaultEventSleepTime  1
#define kDefaultEventMask       (~everyEvent)


/* prototypes for our two internal functions: */
IdleEventHandler *FindEventHandlerByUPP(IdleEventHandlerUPP eventHandlerUPP);
void              RecalculateEventMask(void);
