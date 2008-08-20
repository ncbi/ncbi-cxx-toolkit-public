/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-2002  Brian Bruns
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <tds_config.h>
#ifndef _WIN32
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include "tds.h"
#include "tdsutil.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char  software_version[]   = "$Id$";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};

char *
tds_timestamp_str(char *str, int maxlen)
{
struct tm  *tm;
time_t      t;
#ifdef __FreeBSD__
struct timeval  tv;
char usecs[10];
#endif
#ifdef _REENTRANT
struct tm res;
#endif

#ifdef __FreeBSD__
	gettimeofday(&tv, NULL);
	t = tv.tv_sec;
#else
	/*
	* XXX Need to get a better time resolution for
	* non-FreeBSD systems.
	*/
	time(&t);
#endif

#ifdef _REENTRANT
	tm = localtime_r(&t, &res);
#else
	tm = localtime(&t);
#endif

	strftime(str, maxlen - 6, "%Y-%m-%d %H:%M:%S", tm);

#ifdef __FreeBSD__
    sprintf(usecs, ".%06lu", tv.tv_usec);
	strcat(str, usecs);
#endif

	return str;
}

#if (defined(HAVE_GETADDRINFO) || defined(HAVE_GETNAMEINFO))  &&  defined(_REENTRANT)
static
int s_make_hostent(struct hostent* he, char* buf, int len, const struct addrinfo* ai)
{
	static int ptr_size = sizeof(char*);
	int pos = 0, namelen, addrnum = 0, maxaddrs;
	const struct addrinfo* it;

	memset(he, 0, sizeof(*he));

	namelen = strlen(ai->ai_canonname);
	if (pos + namelen >= len) {
		return NO_RECOVERY;
	}
	if (he->h_name != ai->ai_canonname) {
		/* already present in dummy structure passed by tds_ghba_r */
		he->h_name = strcpy(buf + pos, ai->ai_canonname);
	}
	pos += namelen;

	pos += ptr_size - ((pos + (size_t)buf) % ptr_size); /* align */
	if (pos + ptr_size > len) {
		return NO_RECOVERY;
	}
	he->h_aliases = (char**)(buf + pos);
	he->h_aliases[0] = 0;
	pos += ptr_size;

	he->h_addrtype = ai->ai_family;
	he->h_length = ai->ai_addrlen;
	if (pos + ptr_size > len) {
		return NO_RECOVERY;
	}
	he->h_addr_list = (char**)(buf + pos);
	pos += ptr_size;
	maxaddrs = (len - pos) / (ptr_size + ai->ai_addrlen);
	if (maxaddrs == 0) {
		he->h_addr_list[0] = 0;
		return NO_RECOVERY;
	}
	pos += ptr_size * maxaddrs;
	
	for (it = ai;  it != 0  &&  addrnum < maxaddrs;  it = it->ai_next) {
		if (it->ai_family == ai->ai_family) {
			switch (ai->ai_family) {
			case PF_INET:
				memcpy(buf + pos,
				       &((struct sockaddr_in*)(it->ai_addr))
				           ->sin_addr,
				       it->ai_addrlen);
				break;
			case PF_INET6:
				memcpy(buf + pos,
				       &((struct sockaddr_in6*)(it->ai_addr))
				           ->sin6_addr,
				       it->ai_addrlen);
				break;
			}
			he->h_addr_list[addrnum] = buf + pos;
			pos += it->ai_addrlen;
			++addrnum;
		}
	}
	he->h_addr_list[addrnum] = 0;

	return 0;
}

static
int s_convert_ai_errno(int ai_errno)
{
	switch (ai_errno) {
	case EAI_NONAME:      return HOST_NOT_FOUND;
#ifdef EAI_FAMILY
	case EAI_FAMILY:      return NO_ADDRESS;
#else
	case EAI_ADDRFAMILY:  return NO_ADDRESS;
#endif
#if defined(EAI_NODATA)  &&  EAI_NODATA != EAI_NONAME
	case EAI_NODATA:      return NO_DATA;
#endif
	case EAI_FAIL:        return NO_RECOVERY;
	case EAI_AGAIN:       return TRY_AGAIN;
	default:              return TRY_AGAIN;
	}
}
#endif

struct hostent   *
tds_gethostbyname_r(const char *servername, struct hostent *result, char *buffer, int buflen, int *h_errnop)
{
#if defined(NETDB_REENTRANT)  ||  !defined(_REENTRANT)
        return gethostbyname(servername);

#else

#if defined(HAVE_FUNC_GETHOSTBYNAME_R_6)
	if (gethostbyname_r(servername, result, buffer, buflen, &result, h_errnop))
		return NULL;

#elif defined(HAVE_FUNC_GETHOSTBYNAME_R_5)
        gethostbyname_r(servername, result, buffer, buflen, h_errnop);
#elif defined(HAVE_FUNC_GETHOSTBYNAME_R_3)
	struct hostent_data data;
	gethostbyname_r(servername, result, &data);
#elif defined(HAVE_GETADDRINFO)
	struct addrinfo hints, *out = 0;
	int ai_errno;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_INET;
#ifdef AI_CANONNAME
        hints.ai_flags |= AI_CANONNAME;
#endif

	ai_errno = getaddrinfo(servername, 0, &hints, &out);
	if (ai_errno == 0  &&  out) {
		*h_errnop = s_make_hostent(result, buffer, buflen, out);
	} else {
		*h_errnop = s_convert_ai_errno(ai_errno);
	}
	if (out) {
		freeaddrinfo(out);
	}
#elif defined(_REENTRANT)
#error gethostbyname_r style unknown and getaddrinfo unavailable
#endif

        return result;
#endif
}

#if defined(HAVE_GETNAMEINFO)  &&  defined(_REENTRANT)
#  ifndef HAVE_SOCKLEN_T
typedef int socklen_t;
#  endif

static
socklen_t s_make_sa(const char *addr, int len, int type, void *buf, int blen)
{
	switch (type) {
	case PF_INET:
	{
		struct sockaddr_in* sain = (struct sockaddr_in*)buf;
		if (sizeof(*sain) > blen) {
			return 0;
		}
		memset(sain, 0, sizeof(*sain));
		sain->sin_family = type;
		memcpy(&sain->sin_addr, addr, len);
#ifdef HAVE_SIN_LEN
		sain->sin_len = len;
#endif
		return sizeof(*sain);
	}

#ifdef PF_INET6
	case PF_INET6:
	{
		struct sockaddr_in6* sa6 = (struct sockaddr_in6*)buf;
		if (sizeof(*sa6) > blen) {
			return 0;
		}
		memset(sa6, 0, sizeof(*sa6));
		sa6->sin6_family = type;
		memcpy(&sa6->sin6_addr, addr, len);
#ifdef HAVE_SIN_LEN
		sa6->sin6_len = len;
#endif
		return sizeof(*sa6);
	}
#endif

	default:
		return 0;
	}
	
}
#endif

struct hostent   *
tds_gethostbyaddr_r(const char *addr, int len, int type, struct hostent *result, char *buffer, int buflen, int *h_errnop)
{
#if defined(NETDB_REENTRANT)  ||  !defined(_REENTRANT)
        return gethostbyaddr(addr, len, type);

#else

#if defined(HAVE_FUNC_GETHOSTBYADDR_R_8)
	if (gethostbyaddr_r(addr, len, type, result, buffer, buflen, &result, h_errnop))
		return NULL;
#elif defined(HAVE_FUNC_GETHOSTBYADDR_R_7)
        gethostbyaddr_r(addr, len, type, result, buffer, buflen, h_errnop);
#elif defined(HAVE_FUNC_GETHOSTBYADDR_R_5)
	struct hostent_data data;
	gethostbyaddr_r(addr, len, type, result, &data);
#elif defined(HAVE_GETNAMEINFO)
	struct sockaddr* sa = (struct sockaddr*)buffer;
	socklen_t salen;
	int ai_errno;

	salen = s_make_sa(addr, len, type, buffer, buflen);
	if (salen <= 0) {
		*h_errnop = NO_RECOVERY;
		return NULL;
	}
	ai_errno = getnameinfo(sa, salen, buffer + salen, buflen - salen, 0, 0, 0);
	if (ai_errno == 0) {
		struct addrinfo ai; /* much easier to construct than hostent */
		ai.ai_family 	= type;
		ai.ai_addrlen 	= salen;
		ai.ai_addr 		= sa;
		ai.ai_canonname = buffer + salen;
		ai.ai_next 		= 0;
		*h_errnop = s_make_hostent(result, buffer + salen, buflen - salen, &ai);
	} else {
		*h_errnop = s_convert_ai_errno(ai_errno);
	}
#else
#error gethostbyaddr_r style unknown and getnameinfo unavailable
#endif

        return result;
#endif
}

#if defined(HAVE_GETADDRINFO)  &&  defined(_REENTRANT)
static
int s_make_servent(struct servent* se, char* buf, int len, const struct addrinfo* ai, const char* name, const char* proto)
{
	static int ptr_size = sizeof(char*);
	int pos = 0, namelen;
	const struct addrinfo* it;

	memset(se, 0, sizeof(*se));

	namelen = strlen(name);
	if (pos + namelen >= len) {
		return NO_RECOVERY;
	}
	se->s_name = strcpy(buf + pos, ai->ai_canonname);
	pos += namelen;

	pos += ptr_size - ((pos + (size_t)buf) % ptr_size); /* align */
	if (pos + ptr_size > len) {
		return NO_RECOVERY;
	}
	se->s_aliases = (char**)(buf + pos);
	se->s_aliases[0] = 0;
	pos += ptr_size;

	switch (ai->ai_family) {
	case PF_INET:
		se->s_port = ((struct sockaddr_in*)(it->ai_addr))->sin_port;
		break;
	case PF_INET6:
		se->s_port = ((struct sockaddr_in6*)(it->ai_addr))->sin6_port;
		break;
	}
	if (proto  &&  pos + strlen(proto) < len) {
		se->s_proto = strcpy(buf + pos, proto);
		pos += strlen(proto) + 1;
	}

	return 0;
}
#endif

struct servent *
tds_getservbyname_r(const char *name, char *proto, struct servent *result, char *buffer, int buflen)
{

#if defined(NETDB_REENTRANT)  ||  !defined(_REENTRANT)
        return getservbyname(name, proto);

#else

#if defined(HAVE_FUNC_GETSERVBYNAME_R_6)
	if (getservbyname_r(name, proto, result, buffer, buflen, &result))
		return NULL;
#elif defined(HAVE_FUNC_GETSERVBYNAME_R_5)
        getservbyname_r(name, proto, result, buffer, buflen);
#elif defined(HAVE_FUNC_GETSERVBYNAME_R_4)
	struct servent_data data;
	getservbyname_r(name, proto, result, &data);
#elif defined(HAVE_GETADDRINFO)
	struct addrinfo hints, *out = 0;
	struct protoent* pe;
	int status;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_INET;
	if (proto) {
		pe = getprotobyname(proto);
		if (pe) {
			/* might as well canonicalize */
			proto             = pe->p_name;
			hints.ai_protocol = pe->p_proto;
		}
	}
	status = getaddrinfo(0, name, &hints, &out);
	if (status == 0  &&  out) {
		status = s_make_servent(result, buffer, buflen, out, name, proto);
		if (status != 0) {
			result = 0;
		}
	} else {
		result = 0;
	}
	if (out) {
		freeaddrinfo(out);
	}
#else
#error getservbyname_r style unknown and getaddrinfo unavailable
#endif

        return result;
#endif
}


char *
tds_strtok_r(char *s, const char *delim, char **ptrptr)
{
#ifdef _REENTRANT
#  if defined(NCBI_FTDS) && defined(_WIN32)
    char *p;

    if (s == NULL) {
        s = *ptrptr;
    }
    if (s == NULL) {
        return NULL;
    }
    s += strspn(s, delim);
    if ((p = strpbrk(s, delim)) != NULL) {
        *ptrptr = p + 1;
        *p = '\0';
    } else {
        *ptrptr = NULL;
    }
    return s;
#  else
	return strtok_r(s, delim, ptrptr);
#  endif
#else
	return strtok(s, delim);
#endif
}
