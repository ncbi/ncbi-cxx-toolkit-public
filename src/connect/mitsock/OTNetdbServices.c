/* NetdbServices.c
 *
 *	Performs network services lookups, via the Internet Config utilities that
 *	are now built-into the MacOS.
 *
 *	Modified from sources obtained under the following notification from MIT
 *
 *   ----------------------------------------------------------
 * Copyright © 1998-1999 by the Massachusetts Institute of Technology.
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
 *	  ------------------------------------------------------------
 *
 *	INSERT STANDARD NIH COPYRIGHT/DISCLAIMER HERE...
 *
 */

#include <InternetConfig.h>			//MacOS internet config interfaces
#include <TextUtils.h>				// p2cstr

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "SocketsInternal.h"
#include "netdb.h"


//  prototypes
static UInt32 ifAddr(void);
static void endservent (void);


/* Globals for {get,end}hostent() */
static ICServices *gServices=NULL;
static ICInstance gConfig;
static int gServiceIndex;


/* Constants for servents */
static const char *prot_none="reserved";
static const char *prot_tcp="tcp";
static const char *prot_udp="udp";
static const char *not_an_alias=NULL; /* IC doesn't do aliases, so we use an empty list. */


/* ifAddr
 *
 *	Returns the internet IP address of the default endpoint
 *	which is configured for this system.  In most all cases
 *	there will be only one physical interface, and most hosts will
 *	only be assigned a single IP address, but if that is not the case
 *	then it is possible that this call could return the information 
 *	for the wrong interface.
 */
UInt32 ifAddr(void) 
{
	OSStatus	err;
	InetInterfaceInfo info;
	
	err = OTInetGetInterfaceInfo ( &info, kDefaultInetInterface);
	if (err != noErr){
		return(0);
	}
	return( info.fAddress);
}


/*
 * getservbyname() - looks up a service by its name
 */
struct servent *getservbyname(const char *servname, const char *protname)
{
	struct servent *s = NULL;
	int found = false;

	ncbi_ClearErrno();

	if(servname == NULL){
		ncbi_SetErrno(EINVAL);
		goto abort;
	}
  
	if( (strcmp( protname, "tcp") != 0) && ( strcmp( protname, "udp") != 0)){
		ncbi_SetErrno(EINVAL);
		goto abort;
	}
  
	/* Make sure the database is closed */
	endservent();

	while(!found){
		s = getservent();  // get the next entry

		if(s == NULL){
			break;		// did we reach the end?  Guess we return NULL
		}
  	  
		// Do we have a match on the name?
		if(strcmp(s->s_name, servname) == 0){
			// If protocol is "any" or an exact match, then we're done
			if( (protname == NULL) || (strcmp(s->s_proto, protname) == 0)){
				found = true;
			}
  	    }
    }

abort:  
	endservent();
	return(s);
}


/*
 * getservbyport() - looks up a service by its port number
 */
struct servent *getservbyport(int port, const char *protname)
{
  struct servent *s = NULL;
  int found = false;
  
  ncbi_ClearErrno();
  
  if((strcmp(protname, "tcp") != 0) && (strcmp(protname, "udp") != 0))
    {
      ncbi_SetErrno(EINVAL);
      goto abort;
    }
  
  /* Make sure the database is closed */
  endservent();
  
  while(!found)
    {
  	  s = getservent();  /* get the next entry */
  	  
  	  /* did we reach the end?  Guess we return NULL */
  	  if(s == NULL)
  	    break;
  	  
  	  /* Do we have a match on the port? */
  	  if(s->s_port == port)
  	    {
  	      if(protname == NULL)
  	        found = true;  /* any protocol is okay */
  	      else
  	        if(strcmp(s->s_proto, protname) == 0)
  	          found = true;  /* protocol matches */
  	    }
    }

abort:  
  endservent();
  return(s);
}

/* getservent
 *
 *	Returns pointer to a servent structure for the requested protocol/service
 *	comination.
 *
 *	Some service table entries do multiple protocols.  The first time we see one,
 *	return one protocol.  Don't advance to the next entry, and set preferedProto
 *	to 1 (udp) or 2 (tcp), the one we didn't return.  If we are called again and
 *	preferedProto is one of these, return the other one this time and advance to
 *	the next entry. 
 */

struct servent *getservent(void)
{
	static int	preferedProto = 0;
	OSStatus	err;
	long		len;
	ICAttr		attr;
	ICServices	*buf;
	ICServiceEntry tmpICSvc; /* We use this to store an aligned copy of *curSvc. */
	static ICServiceEntry *curICSvc = NULL;
	static struct servent svc = {NULL, NULL, 0, NULL};
  
	ncbi_ClearErrno(); 
  
	// Deallocate the last service name we returned.
	if (svc.s_name!=NULL)
	{
		free(svc.s_name);
		svc.s_name=NULL;
	}
  
	// If we haven't loaded the service table, load it now.
	if (gServices==NULL)
	{
		err=ICStart(&gConfig, 'Sock');
		if (err) {
			ncbi_SetOTErrno(err); 
			return NULL; 
		}
#if !TARGET_API_MAC_CARBON
	// Need to #ifdef this out for Carbon builds
		err=ICFindConfigFile(gConfig, 0, nil);
		if (err) { 
			ncbi_SetOTErrno(err);
			return NULL;
		}
	// End of #ifdef for Carbon
#endif

		len = 0;
		err = ICGetPref(gConfig, kICServices, &attr, NULL, &len);
		if (err) { 
			ncbi_SetOTErrno(err);
			return NULL;
		}
		buf=(ICServices*)NewPtr(len);
		if (buf==NULL || MemError()!=noErr) {
			ncbi_SetOTErrno(MemError());
			return NULL;
		}
		err=ICGetPref(gConfig, kICServices, &attr, (char*)buf, &len);
		if (err){
			ncbi_SetOTErrno(err);
			return NULL;
		}

		gServices=buf;
		curICSvc=&gServices->services[0];
		gServiceIndex=0;
	}
  
	/* If we are out of entries, return NULL. */
	if (curICSvc==NULL || gServiceIndex>=gServices->count)  return NULL;
  
	/* gServices is "packed", which means we cannot directly index gServices->services.
	 * So, we have to manually increment the current record pointer.  We also have to
	 * memmove any numbers into an aligned variable, because entries in gServices are
	 * not guaranteed to be on whatever boundary the current processor prefers. 
	 */

	/* Make an aligned copy of *curICSvc */
	memmove(tmpICSvc.name, curICSvc, ((char*)curICSvc)[0]+1);
	memmove(&tmpICSvc.port, (char*)curICSvc+((char*)curICSvc)[0]+1, 2);
	memmove(&tmpICSvc.flags, (char*)curICSvc+((char*)curICSvc)[0]+3, 2);

	/* Put together a servent based on the current service table entry. */
	len = tmpICSvc.name[0]+1;
	svc.s_name = malloc(len);
	if (svc.s_name == NULL) { 
		ncbi_SetOTErrno(memFullErr); 
		return NULL;
	}
	//memmove(svc.s_name, tmpICSvc.name, len);
	//p2cstr((StringPtr)svc.s_name);
	p2cstrcpy(svc.s_name, tmpICSvc.name);

	svc.s_aliases=(char**)&not_an_alias;

	svc.s_port=tmpICSvc.port;

	switch(preferedProto){
		case 0:
			switch(tmpICSvc.flags){
				case 0:
					svc.s_proto=(char*)prot_none;
					break;
				case kICServicesUDPMask:
					svc.s_proto=(char*)prot_udp;
					break;
				case kICServicesTCPMask:
					svc.s_proto=(char*)prot_tcp;
					break;
				case 3:
					svc.s_proto=(char*)prot_udp;
					preferedProto=kICServicesTCPMask;
					break;
			}
			break;
		case kICServicesUDPMask:
			svc.s_proto=(char*)prot_udp;      
			preferedProto=0;
			break;
		case kICServicesTCPMask:
			svc.s_proto=(char*)prot_tcp;
			preferedProto=0;
			break;
		default:
			// Assert_(0); /* We have set a preferedProto that we don't support. */
			break;
	}

	if (preferedProto==0){
		if ((++gServiceIndex)<gServices->count){
			/* Cast gCurrentService to char* so we can play pointer arithmetic games in
			 * byte-sized increments. The 5 is the length byte plus two 2-byte ints.
			 */
			curICSvc=(ICServiceEntry*)((char*)curICSvc+((char*)curICSvc)[0]+5);
		}
		else {
			curICSvc=NULL;
		}
	}
  
  return &svc;  
}


/* endservent
 *
 *	Function which resets the getnextservice database 
 */

static void endservent (void)
{
	OSStatus err;

	if( gServices!=NULL){
		DisposePtr( (Ptr)gServices);
		gServices=NULL;
		err = MemError();
		if( kOTNoError != err) {
			ncbi_SetOTErrno(MemError()); 
			return;
		}
		err = ICStop(gConfig);
		if( kOTNoError != err) { 
			ncbi_SetOTErrno(err);
			return;
		}
	}
}
