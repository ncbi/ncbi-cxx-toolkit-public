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
 * Revision 1.1  2001/04/03 20:35:27  juran
 * Phil Churchill's MIT-derived OT sockets library.  No changes prior to initial check-in.
 *
 * Revision 6.1  2000/03/20 21:50:35  kans
 * initial checkin - initial work on OpenTransport (Churchill)
 *
 * Revision 6.1  1999/11/17 20:52:50  kans
 * changes to allow compilation under c++
 *
 * Revision 6.0  1997/08/25 18:37:31  madden
 * Revision changed to 6.0
 *
 * Revision 4.1  1995/08/01 16:17:20  kans
 * New gethostbyname and gethostbyaddr contributed by Doug Corarito
 *
 * Revision 4.0  1995/07/26  13:56:09  ostell
 * force revision to 4.0
 *
 * Revision 1.3  1995/05/17  17:56:47  epstein
 * add RCS log revision history
 *
 */

/*
 *	Internet name routines that every good unix program uses...
 *
 *		gethostbyname
 *		gethostbyaddr
 *      gethostid
 *		gethostname
 *		getdomainname
 *		inet_ntoa
 *		inet_addr
 */
 
#include <Stdio.h>
#include <String.h>
#include <Types.h>
#include <Resources.h>
#include <Errors.h>
#include <OSUtils.h>
#include <OpenTransport.h>
#include <OpenTptInternet.h>	// for TCP/IP

/* when debugging with the OT debugging libraries,
 *	set this variable to 1 before including the OTDebug header
 */
#ifndef qDebug
#define qDebug 1
#endif

#include <OTDebug.h>			// OTAssert macro
#include <Threads.h>			// declaration for YieldToAnyThread

#include <netdb.h>
#include <neterrno.h>
#include <stdlib.h>				// malloc
#include "SocketsInternal.h"
#include "a_inet.h"

/* global variables...
 */
static int h_errno;
static short driver = 0;
static Boolean gOTInited = false;
EndpointRef gDNRep = kOTInvalidEndpointRef;
OTNotifyUPP gDNRYieldNotifyUPP = NULL;


/* DNR notifier process
 *
 *	This function receives any notifications from the OT DNR routines.
 *
 *	Typically this is used to permit other threads to run while a time
 *	consuming process runs, but it can be used to notify of certain failures.
 *
 *	We handle the case where the user has aborted the action with cmd-.
 */

static pascal void DNRYieldNotifier( void* contextPtr, OTEventCode code, 
                             OTResult result, void* cookie)
{
#pragma unused(contextPtr)
#pragma unused(result)
#pragma unused(cookie)
	OSStatus status;
  
	switch (code) {
	  case kOTSyncIdleEvent:
		status = YieldToAnyThread();
#if !TARGET_API_MAC_CARBON
		OTAssert("YieldingNotifier: YieldToAnyThread failed", status == noErr);
#endif
		break;
	  case kOTProviderWillClose:			// if the dnr service is going away
	  case kOTProviderIsClosed:				// or already gone...
		(void) OTCloseProvider(gDNRep);		// then close our provider
		gDNRep = kOTInvalidProviderRef;		// and note that.
		break;
	  default:
		/* do nothing */
		break;
	}
}


/* ot_OpenDriver
 *
 *	Performs required initializations needed before OT can be used
 */

OSStatus ot_OpenDriver( void) 
{
	OSStatus err;
	int i;

	// if this function has already been performed, then just return
	if( gOTInited){
		return( kOTNoError);
	}

	// Set up the global array of socket pointers and clear it
	if( gSockets == NULL)
		gSockets = (SocketPtr *) malloc(kNumSockets * sizeof(SocketPtr));

	if( gSockets == NULL)
		return(memFullErr);

	for( i = 0; i < kNumSockets; i++)
		gSockets[i] = NULL;
	
#if TARGET_API_MAC_CARBON
	err = InitOpenTransportInContext(kInitOTForApplicationMask, NULL);
#else
	err = InitOpenTransport();
#endif

	gOTInited = (kOTNoError == err);
	
	return( err);
}


/* ot_DNRInit
 *
 *	Function which initializes the domain name resolver for OpenTransport
 *	for use by gethostbyname, gethostbyaddress, and other utils
 */

OSStatus ot_DNRInit(void)
{
	OSStatus theError = kOTNoError;

	// First, check to see if we've already initialized.  If so, then just return...
	if( gDNRep != kOTInvalidProviderRef){
		return (kOTNoError);
	}

	// Next, check to see if any OT service has been used yet
	if (!gOTInited){
		theError = ot_OpenDriver();
	}

	// Get the internet services reference for DNS
	if( theError == kOTNoError){
#if TARGET_API_MAC_CARBON
		gDNRep = OTOpenInternetServicesInContext( kDefaultInternetServicesPath, 0, &theError, NULL);
#else
		gDNRep = OTOpenInternetServices( kDefaultInternetServicesPath, 0, &theError);
#endif
	}

	/* We have to choose to be synchronous or async, sync is simpler but
	 *	I think that async could be done in the future
	 */
	if( theError == kOTNoError){
		theError = OTSetSynchronous( gDNRep);
	}

	// Give OT a pointer to a notifier function that handles async events
	if( theError == kOTNoError){
		gDNRYieldNotifyUPP = NewOTNotifyUPP(DNRYieldNotifier);
		theError = OTInstallNotifier(gDNRep, gDNRYieldNotifyUPP, nil);
	}

	/* If we get idle events, then implementing a multi-threaded app shouldn't
	 *	be any big deal
	 */
	if( theError == kOTNoError){
		theError = OTUseSyncIdleEvents(gDNRep, true);
	}
	return( theError);
}


/* ot_DNRClose
 *
 *	If OT ever needs any formal closing/cleanup actions, this is the place
 *	to put it...
 */

OSStatus ot_DNRClose( InetSvcRef theRef)
{
	OSStatus theErr = kOTNoError;

	return theErr;
}


/*
 *   Gethostbyname and gethostbyaddr each return a pointer to an
 *   object with the following structure describing an Internet
 *   host referenced by name or by address, respectively. This
 *   structure contains the information obtained from the OpenTransport
 *   name server.
 *
 *   struct    hostent 
 *   {
 *        char *h_name;
 *        char **h_aliases;
 *        int  h_addrtype;
 *        int  h_length;
 *        char **h_addr_list;
 *   };
 *   #define   h_addr  h_addr_list[0]
 *
 *   The members of this structure are:
 *
 *   h_name       Official name of the host.
 *
 *   h_aliases    A zero terminated array of alternate names for the host.
 *
 *   h_addrtype   The type of address being  returned; always AF_INET.
 *
 *   h_length     The length, in bytes, of the address.
 *
 *   h_addr_list  A zero terminated array of network addresses for the host.
 *
 *   Error return status from gethostbyname and gethostbyaddr  is
 *   indicated by return of a null pointer.  The external integer
 *   h_errno may then  be checked  to  see  whether  this  is  a
 *   temporary  failure  or  an  invalid  or  unknown  host.  The
 *   routine herror  can  be  used  to  print  an error  message
 *   describing the failure.  If its argument string is non-NULL,
 *   it is printed, followed by a colon and a space.   The  error
 *   message is printed with a trailing newline.
 *
 *   h_errno can have the following values:
 *
 *     HOST_NOT_FOUND  No such host is known.
 *
 *     TRY_AGAIN	This is usually a temporary error and
 *					means   that  the  local  server  did  not
 *					receive a response from  an  authoritative
 *					server.   A  retry at some later time may
 *					succeed.
 *
 *     NO_RECOVERY	Some unexpected server failure was encountered.
 *	 				This is a non-recoverable error.
 *
 *     NO_DATA		The requested name is valid but  does  not
 *					have   an IP  address;  this  is not  a
 *					temporary error. This means that the  name
 *					is known  to the name server but there is
 *					no address  associated  with  this  name.
 *					Another type of request to the name server
 *					using this domain name will result in  an
 *					answer;  for example, a mail-forwarder may
 *					be registered for this domain.
 *					(NOT GENERATED BY THIS IMPLEMENTATION)
 */

static char	cname[255];
static struct InetHostInfo hinfo;

#define MAXALIASES 2
static char	aliases[MAXALIASES+1][kMaxHostNameLen + 1];
static char* aliasPtrs[MAXALIASES+1] = {NULL};
static InetHost* addrPtrs[kMaxHostAddrs+1] = {0};


static struct hostent unixHost = 
{
	cname,
	aliasPtrs,
	AF_INET,
	sizeof(UInt32),
	(char **) addrPtrs
};


/* Gethostbyname
 *
 *	UNIX function implemented on top of OpenTransport
 */

struct hostent *
gethostbyname(const char *name)
{
	extern EndpointRef gDNRep;
	OSStatus	theErr;
	int i, l;

	// open or get the current resolver reference
	if( gDNRep == kOTInvalidEndpointRef){
		theErr = ot_DNRInit();
		if( theErr != kOTNoError){
			ncbi_SetOTErrno( theErr);
			return (NULL);
		}
	}

	// Call OT to resolve the address...
	theErr = OTInetStringToAddress( gDNRep, (char*) name, &hinfo);

	if( theErr != kOTNoError){
		ncbi_SetOTErrno( theErr);
		return( NULL);
	}

	// sometimes the hostname returned has a trailing dot?? (nuke it)
	l = strlen( hinfo.name);
	if( hinfo.name[l-1] == '.'){
		hinfo.name[l-1] = '\0';
	}

#if NETDB_DEBUG >= 5
	printf("gethostbyname: name '%s' returned the following addresses:\n", name);
	for( i = 0; i < kMaxHostAddrs; i++){
		if( hinfo.addrs[i] == 0) break;
		printf("%08x\n", hinfo.addrs[i]);
	}
#endif

	// copy the official hostname to the return struct and first alias
	strcpy( unixHost.h_name, hinfo.name);

	// copy the name passed in as the first alias
	strncpy( aliases[0], name, kMaxHostNameLen);
	aliases[0][kMaxHostNameLen] = '\0';

	// if the answer is different from the query, copy the query as another alias
	theErr = strcmp( name, hinfo.name);
	if( theErr != 0){
		strncpy( aliases[1], hinfo.name, kMaxHostNameLen);
		aliases[1][kMaxHostNameLen] = '\0';
	}

	// This block will not need to be changed if we find a way to determine
	// more aliases than are currently implemented
	for( i = 0; i <= MAXALIASES; i++){
		if( aliases[i][0] != '\0'){
			aliasPtrs[i] = &aliases[i][0];
		} else{
			aliasPtrs[i] = NULL;
		}
	}
	
	// copy all of the returned addresses
	for( i = 0; i < kMaxHostAddrs && hinfo.addrs[i] != 0; i++){
		addrPtrs[i] = &hinfo.addrs[i];
	}

	// make the rest NULL
	for( ; i < kMaxHostAddrs; i++){
		addrPtrs[i] = NULL;
	}
	return( &unixHost);
}


/* gethostbyaddr
 *
 *  Currently only supports IPv4
 */
 
struct hostent *
gethostbyaddr( InetHost *addrP, int len, int type)
{
	extern EndpointRef gDNRep;
	OSStatus	theErr = noErr;
	char	addrString[255];
	int		l;
  
	// Check that the arguments are appropriate
	if( len != INADDRSZ || (type != AF_INET /* && type != AF_UNSPEC */)){
		ncbi_SetErrno( EAFNOSUPPORT);
		return( NULL);
	}
  
	// open or get the current resolver reference
	if( gDNRep == kOTInvalidEndpointRef){
		theErr = ot_DNRInit();
		if( theErr != kOTNoError){
			ncbi_SetErrno( theErr);
			return (NULL);
		}
	}

#if NETDB_DEBUG >= 5
	printf("gethostbyaddr: Looking up hostname for address 0x%8x\n", *addrP);
#endif

	/*  OpenTransport call to get the address as a string */
	theErr = OTInetAddressToName( gDNRep, *(InetHost*)addrP, addrString);
	if( theErr == noErr){
		// for some reason the names have a trailing "."???  
		// Next couple of lines remove it
		l = strlen( addrString);
		if( addrString[l-1] == '.'){
			addrString[l-1] = '\0';
		}
		// with the full name, use gethostbyname to fill in all the blanks...
		return (gethostbyname(addrString) );
	}
	else{
		ncbi_SetErrno( theErr);
	}
	return( NULL);
}

/*
 * gethostid()
 *
 * Get Internet address from the primary enet interface.
 */
 
unsigned long gethostid( void)
{
	OSStatus          theErr = kOTNoError;
	InetInterfaceInfo info;
	extern EndpointRef gDNRep;
  
	// open or get the current resolver reference
	if( gDNRep == kOTInvalidEndpointRef){
		theErr = ot_DNRInit();
		if( theErr != kOTNoError){
			ncbi_SetErrno( theErr);
			return 0;
		}
	}
	theErr = OTInetGetInterfaceInfo( &info, kDefaultInetInterface);
	if(theErr != kOTNoError){
		ncbi_SetErrno( theErr);
		return 0;
	}
	return info.fAddress;
}
 

/*
 * gethostname()
 *
 *	Try to get my host name from DNR. If it fails, just return my
 *	IP address as ASCII. This is non-standard, but since Mac's don't require
 *	hostnames to run, we don't have many other options.
 */

int gethostname( char* machname, size_t buflen)
{
	OSStatus          theErr = kOTNoError;
	InetHost		  addr;
	struct hostent   *host;
	char* firstDot;

	if( (machname == NULL) || (buflen < 31)){
		ncbi_SetErrno( EINVAL);
		return( -1);
	}

	addr = gethostid();

	host = gethostbyaddr( &addr, INADDRSZ, AF_INET);
	if(host == NULL){
		ncbi_SetErrno( EINVAL);
		return( -1);
	}
  
	firstDot = strchr(host->h_name, '.');
	if(firstDot != NULL && *firstDot == '.'){
		*firstDot = '\0';
	}

	strncpy( machname, host->h_name, buflen);
	machname[buflen-1] = '\0';  // strncpy doesn't always null terminate
    
	if(theErr != kOTNoError)
		return(-1);
	else
		return(0);
}

/* inet_ntoa()
 *
 *	Converts an IP address in 32 bit IPv4 address to a dotted string notation
 *  Function returns a pointer to the resulting string (success), or NULL (failure).
 */

char *inet_ntoa( struct in_addr addr)
{
	static char addrString[INET_ADDRSTRLEN + 1 /* '\0' */];

	OTInetHostToString( addr.s_addr, addrString);
  
	return (addrString);
}

/*
 * inet_aton() - converts an IP address in dotted string notation to a 32 bit IPv4 address
 *               Function returns 1 on success and 0 on failure.
 */
int inet_aton(const char *str, struct in_addr *addr)
{
	InetHost host;
	OSStatus theError;

	if((str == NULL) || (addr == NULL)){
		return( INET_FAILURE);
	}
  
	theError = OTInetStringToHost((char *) str, &host);
	if(theError != kOTNoError){
		ncbi_SetErrno( theError);
		return( INET_FAILURE);
	}
	addr->s_addr = host;
	return(INET_SUCCESS);  
}


/* inet_addr
 *
 *	Does much the same thing as inet_aton, but returns the value rather than
 *	modifying a parameter that's passed in.
 *		Note: failure is reported by returning INADDR_NONE (255.255.255.255) 
 */

InetHost inet_addr(const char *str)
{
	OSStatus       theError = kOTNoError;
	struct in_addr addr;

	theError = inet_aton(str, &addr);

	if(theError != INET_SUCCESS){
		addr.s_addr = INADDR_NONE;
	}

	return(addr.s_addr);
}


/*		One last unix compatibility function that was included
 *		in the ncsa socket lib...
 */
void bzero( char *b, long s)
{
	for( ; s ; ++b, --s)
		*b = 0;
}


/*		Error reporting strings and functions
 */

char* h_errlist[] = {
	"Error 0",
	"Unknown host",						/* 1 HOST_NOT_FOUND */
	"Host name lookup failure",			/* 2 TRY_AGAIN */
	"Unknown server error",				/* 3 NO_RECOVERY */
	"No address associated with name",	/* 4 NO_ADDRESS */
};

// compute the number of elements in the list (one less place for human error)
const int	h_nerr = { sizeof(h_errlist)/sizeof(h_errlist[0]) };

void herror(char *s)
{
	fprintf(stderr,"%s: ",s);
	if (h_errno < h_nerr)
		fprintf(stderr,h_errlist[h_errno]);
	else
		fprintf(stderr,"error %d",h_errno);
	fprintf(stderr,"\n");
}

char *herror_str(int theErr) 
{
	if (theErr > h_nerr )
		return NULL;
	else
		return h_errlist[theErr];
}

// end of file
