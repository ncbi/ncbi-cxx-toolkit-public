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


#include <Threads.h>			// declaration for YieldToAnyThread
#include <Resources.h>			// GetResFile, SetResFile
#include "IdleInternal.h"


/* Global Variables */
Boolean           gIsThreaded		= false;
Boolean           gShouldIdle		= true;
UInt32            gEventSleepTime	= kDefaultEventSleepTime;
UInt32            gIdleFrequency	= kDefaultIdleFrequency;
UInt16            gEventMask 		= kDefaultEventMask;
IdleEventHandler *gFirstEventHandler= NULL;
IdleEventHandler *gLastEventHandler	= NULL;

/*
 * FindEventHandlerbyUPP() - finds the first event handler with a given UPP
 *                           returns nil if there is no such UPP
 */
IdleEventHandler *FindEventHandlerByUPP(IdleEventHandlerUPP eventHandlerUPP)
{
  IdleEventHandler *handler = gFirstEventHandler;
  
  while(handler != nil)
    {
      if(handler->UPP == eventHandlerUPP)
        break;
      else
        handler = handler->next;
    }
    
  return(handler);
}


/*
 * RecalculateEventMask() - recalculates the global event mask
 */
void RecalculateEventMask(void)
{
  IdleEventHandler *handler = gFirstEventHandler;
  
  gEventMask = 0;
  handler = gFirstEventHandler;
  while(handler != nil)
    {
      if(handler->active)
        gEventMask |= handler->mask;
      
      handler = handler->next;
    }
}


/*
 * IdleAddEventHandler() - adds an idle event handler. You should only call this if the 
 *                         library is running in single thread mode.  In this way, 
 *                         the application can handle events that come in while 
 *                         waiting on the network.  mask is the eventMask you want.
 *
 *                         Handlers are active by default. Call IdleSetActive to change this.
 */
OSStatus IdleAddEventHandler(IdleEventHandlerUPP eventHandlerUPP, UInt16 mask, UInt32 refCon)
{
  IdleEventHandler *handler;
  
  handler = malloc(sizeof(IdleEventHandler));
  if(handler == nil)
    return(paramErr);
    
  handler->UPP  =   eventHandlerUPP;
  handler->mask =   mask;
  handler->refCon = refCon;
  handler->active = 1L;
  
  /* Add to linked list */
  handler->prev = gLastEventHandler;
  handler->next = nil;
  if (gLastEventHandler != NULL) gLastEventHandler->next = handler;
  
  /* repoint the last event handler */
  if(gFirstEventHandler == nil)
    gFirstEventHandler = handler;
  gLastEventHandler = handler;
  
  /* Add our mask to the global mask */
  gEventMask |= handler->mask;
  
  return(noErr);
}


/*
 * IdleRemoveEventHandler() - Removes an idle event handler. You should only call this 
 *                            if the library is running in single thread mode. In this way, 
 *                            the application can handle events that come in while 
 *                            waiting on the network.  Returns paramErr if it fails.
 */
OSStatus IdleRemoveEventHandler(IdleEventHandlerUPP eventHandlerUPP)
{
  IdleEventHandler *handler;
  
  handler = FindEventHandlerByUPP(eventHandlerUPP);
  
  if(handler == nil)
    return(paramErr);
  
  /* fix forward links */
  if(handler == gFirstEventHandler)
    gFirstEventHandler = handler->next;      /* make the next entry the first one */
  else
    { 
      IdleEventHandler *previous;
      
      previous = handler->prev;
      previous->next = handler->next;
    }

  /* fix backward links  */
  if(handler == gLastEventHandler)
    gLastEventHandler = handler->prev;      /* make the previous entry the last one */
  else
    {
      IdleEventHandler *next;
      
      next = handler->next;
      next->prev = handler->prev;
    }
    
  free(handler);                        /* release memory */
  
  RecalculateEventMask();
  
  return(noErr);
}


/*
 * IdleSetActive() - Makes the Event Handler active
 */
void IdleSetActive(IdleEventHandlerUPP eventHandlerUPP)
{
  IdleEventHandler *handler;
  
  handler = FindEventHandlerByUPP(eventHandlerUPP);
  
  if(handler != nil)
    {
      handler->active++;
      gEventMask |= handler->mask;
    }
  return;
}


/*
 * IdleSetInactive() - Makes the Event Handler inactive
 */
void IdleSetInactive(IdleEventHandlerUPP eventHandlerUPP)
{
  IdleEventHandler *handler;
  
  handler = FindEventHandlerByUPP(eventHandlerUPP);
  
  if(handler == nil)
    return;
    
  if(handler->active > 0)
    handler->active--;
    
  RecalculateEventMask();
  return;
}


/*
 * IdleSetIdleFrequency() - Sets the time the idle loop waits before calling 
 *                          WaitNextEvent() or YieldToAnyThread()
 *                          (the result is in ticks -- 1/60 of a second)
 */
void IdleSetIdleFrequency(UInt32 idleFrequency)
{
  gIdleFrequency = idleFrequency;
  return;
}


/*
 * IdleGetIdleFrequency() - Gets the time the idle loop waits before calling 
 *                          WaitNextEvent() or YieldToAnyThread()
 *                          (the result is in ticks -- 1/60 of a second)
 */
UInt32 IdleGetIdleFrequency(void)
{
  return(gIdleFrequency);
}


/*
 * IdleSetEventSleepTime() - Sets the time WaitNextEvent() will sleep when waiting for
 *                           an event -- only used in non-threaded mode or when the
 *                           main thread makes blocking calls
 *                           (sleepTime is in ticks -- 1/60 of a second)
 */
void IdleSetEventSleepTime(UInt32 eventSleepTime)
{
  gEventSleepTime = eventSleepTime;
  return;
}


/*
 * IdleGetEventSleepTime() - Gets the time WaitNextEvent() will sleep when waiting for
 *                           an event -- only used in non-threaded mode or when the
 *                           main thread makes blocking calls
 *                           (the result is in ticks -- 1/60 of a second)
 */
UInt32 IdleGetEventSleepTime(void)
{
  return(gEventSleepTime);
}


/*
 * IdleSetThreaded() -- Sets whether we are running in multi-threaded mode
 */
void IdleSetThreaded(Boolean isThreaded)
{
  gIsThreaded = isThreaded;
  return;
}


/*
 * IdleGetThreaded() -- Gets whether we are running in multi-threaded mode
 */
Boolean IdleGetThreaded(void)
{
  return(gIsThreaded);
}


/*
 * IdleSetShouldIdle() -- Sets whether we should idle at all (used when you can't idle)
 */
void IdleSetShouldIdle(Boolean shouldIdle)
{
  gShouldIdle = shouldIdle;
  return;
}


/*
 * IdleGetShouldIdle() -- Sets whether we should idle at all 
 */
Boolean IdleGetShouldIdle(void)
{
  return(gShouldIdle);
}


/*
 * Idle() - Give time to other threads or the system while we wait.
 */
OSStatus Idle(void)
{
  OSStatus             theError;
  EventRecord          theEvent;
  static UInt32        lastCheck = 0;
  UInt32               now = TickCount();
  
  /* Don't idle if we shouldn't!  We're probably at some scary system time */
  if(gShouldIdle == false)
    return(noErr);
  
  /* If we are multi-threaded call YieldToAnyThread, otherwise call WaitNextEvent */
  if(gIsThreaded)
    {
      /* We are not the main thread -- we should just give time
         to the other threads.  The main thread will take care of events */
      if(now > (lastCheck + gIdleFrequency))
        {
          ThreadID      currentThread;
          
          theError = GetCurrentThread(&currentThread);
          if(theError != noErr)
            return(theError);

          theError = YieldToAnyThread();
          if(theError != noErr)
            return(theError);
            
          lastCheck = now;
        }
    }
  else
    {
      /* We are the main thread.  We need to give time to the system periodically */
      if(now > (lastCheck + gIdleFrequency))
        {
          /* WaitNextEvent can change the resource file (through patches like Retrospect does) */
          SInt16 saveResFile = CurResFile(); 
          
          Boolean retval;
          /* If we are the main thread, we always need to give time to the system */
          retval = WaitNextEvent(gEventMask, &theEvent, gEventSleepTime, nil);
          
          if((retval == true) || (theEvent.what == nullEvent))
            {
              IdleEventHandler *handler = gFirstEventHandler;
              
			  while(handler != nil)
			    {
			      if(handler->active > 0)
			        {
			          Boolean handled = false;
					  handled = CallIdleEventHandlerProc(handler->UPP, &theEvent, handler->refCon);
			          if(handled && theEvent.what != nullEvent)
			            break; /* non-null event handled -- stop */
			        }
			      handler = handler->next;
			    }
			}
          
          lastCheck = now;
          
          /* Restore the res file to whatever it was when before WaitNextEvent and the callback */
          UseResFile(saveResFile);
        }
    }
  
  return(noErr);
}
