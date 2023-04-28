#ifndef CONNECT___NCBI_LBSM__H
#define CONNECT___NCBI_LBSM__H

/* $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   LBSM server-to-client data exchange API
 *
 */

#include "ncbi_comm.h"
#include "ncbi_config.h"
#include <connect/ncbi_heapmgr.h>
#include <connect/ncbi_service.h>
#include <math.h>  /* NB: pull only HUGE_VAL */


#if defined(_DEBUG)  &&  !defined(NDEBUG)
/*#  define LBSM_DEBUG  1*/
#endif /*_DEBUG && !NDEBUG*/


#define LBSM_DEFAULT_CFGFILE         DISPATCHER_CFGPATH DISPATCHER_CFGFILE
#define LBSM_DEFAULT_LOGFILE         "/var/log/lbsmd"
#define LBSM_DEFAULT_HOMEDIR         "/opt/machine/lbsm/"
#define LBSM_DEFAULT_RUNFILE         LBSM_DEFAULT_HOMEDIR "run/lbsmd.pid"
#define LBSM_DEFAULT_FEEDFILE        LBSM_DEFAULT_HOMEDIR "run/.lbsmd"
#define LBSM_DEFAULT_INFOFILE        LBSM_DEFAULT_HOMEDIR "run/lbsmd"

/* Current LBSM heap version */
#define LBSM_HEAP_VERSION_MAJ        1
#define LBSM_HEAP_VERSION_MIN        3

/* Last LBSM heap version checksummed with CRC32 */
#define LBSM_HEAP_VERSION_MAJ_CRC32  1
#define LBSM_HEAP_VERSION_MIN_CRC32  2

/* MISCINFO flags (above daemon version) */
#define LBSM_MISC_MASK               0xF000
#define LBSM_MISC_UNUSED             0x8000/*reserved*/
#define LBSM_MISC_INERT              0x4000
#define LBSM_MISC_JUMBO              0x2000
#define LBSM_MISC_x64                0x1000

/* KERNELID svcpack flag */
#define LBSM_KERNELID_SVPKBIT        0x8000
#define LBSM_KERNELID_SVPKDIV        10


#ifdef __cplusplus
extern "C" {
#endif


#ifndef __GNUC__
#  define LBSM_PACKED  /* */
#  if    defined(NCBI_OS_SOLARIS)
#    ifndef __i386
#      pragma pack(4)
#    endif
#  elif  defined(NCBI_OS_IRIX)
#    pragma pack 4
#  elif !defined(__i386)  &&  !defined(_CRAYC)
#    error "Don't know how to pack on this platform"
#  endif
#else
#  define LBSM_PACKED  __attribute__((aligned(4),packed))
#endif /*__GNUC__*/


/* In calculation of offsets for data structures below, not all implicit 4-byte
 * paddings may exist on a particular platform.  The "worst" estimate is given.
 */

/* Host load                                                            offsets
 */
typedef struct LBSM_PACKED {
    double         avg;           /* 1min    lavg                          0 */
    double         avgBLAST;      /* instant load                          8 */
    double         status;        /* base rate for 1min    load           16 */
    double         statusBLAST;   /* base rate for instant load           24 */
    unsigned int   ram_total;     /* total RAM, pages                     32 */
    unsigned int   ram_cache;     /* used discardable RAM, pages          36 */
    unsigned int   ram_free;      /* free RAM, pages                      40 */
    unsigned int   swap_total;    /* total swap, pages                    44 */
    unsigned int   swap_free;     /* free swap, pages                     48 */
    unsigned int   nrtask;        /* # of running tasks (processes)       52 */
    unsigned short port[4];       /* first 4 monitored ports, h.b.o.      56 */
    unsigned char  used[4];       /* used capacities (%%*2) of the ports  64 */
} SLBSM_HostLoad;                 /*                        Total bytes:  68 */


/* Host data                                                            offsets
 */
typedef struct LBSM_PACKED {
    unsigned short version;       /* daemon version number (misc, 4, 4, 4) 0 */
    unsigned short machine;       /* bits / arch / soft          (2, 6, 8) 2 */
    TNCBI_Time     sys_uptime;    /* time the system booted up             4 */
    TNCBI_Time     start_time;    /* time the daemon started up            8 */
    unsigned int   kernel;        /* kernel id (8, 8, 1, 15)[/SP]         12 */
    unsigned short pgsize;        /* page size, KiB units                 16 */
    unsigned short hzfreq;        /* CPU freq, fixed point base-128, GHz  18 */
    unsigned short nrproc;        /* number of processors (8 phy, 8 log)  20 */
    unsigned short alerts;        /* alert code for this host             22 */
    TNCBI_Time     joined;        /* host join time                       24 */
    unsigned short table_hosts;   /* how many hosts this host now sees    28 */
    unsigned short known_hosts;   /* how many hosts this host has seen    30 */
} SLBSM_HostData;                 /*                        Total bytes:  32 */


/* Types of blocks in the heap
 */
typedef enum {
    eLBSM_Invalid = 0,            /* not a valid entry                       */
    eLBSM_Host,                   /* host entry                              */
    eLBSM_Service,                /* service entry                           */
    eLBSM_Version,                /* heap version number                     */
    eLBSM_Pending,                /* service entry pending removal           */
    eLBSM_Config                  /* configuration file name                 */
} ELBSM_Type;


/* Prefix header of the heap entry                                      offsets
 */
typedef struct LBSM_PACKED {
    SHEAP_Block    head;          /* heap manager control                  0 */
    ELBSM_Type     type;          /* type of the block                     8 */
    TNCBI_Time     good;          /* entry valid thru                     12 */
} SLBSM_Entry;                    /*                        Total bytes:  16 */


/* Heap version/integrity control                                       offsets
 */
typedef struct LBSM_PACKED {
    SLBSM_Entry    entry;         /* entry header (type == eLBSM_Version)  0 */
    unsigned short major;         /* heap version number, major           16 */
    unsigned short minor;         /* heap version number, minor           18 */
    unsigned int   count;         /* update serial number                 20 */
    unsigned int   cksum;         /* checksum (CRC32/Adler32) of the heap 24 */
    unsigned int   local;         /* host IP address, network byte order  28 */
    TNCBI_Time     start;         /* daemon startup time (LoW: instance#) 32 */
    TNCBI_Size     index;         /* index position on the heap (0=unused)36 */
    unsigned short demon;         /* daemon version code                  40 */
    TSERV_TypeOnly types;         /* server types registering (0=all)     42 */
} SLBSM_Version;                  /*                        Total bytes:  44 */


/* System information                                                   offsets
 */
typedef struct LBSM_PACKED {
    SLBSM_HostLoad load;          /* load information                      0 */
    SLBSM_HostData data;          /* data information                     68 */
} SLBSM_Sysinfo;                  /*                        Total bytes: 100 */


/* Host information as kept in the heap                                 offsets
 */
typedef struct LBSM_PACKED {
    SLBSM_Entry    entry;         /* entry header (type == eLBSM_Host)     0 */
    unsigned int   addr;          /* host IP addr (net byte order)        16 */
#if (defined(NCBI_OS_SOLARIS)  &&  !defined(__i386))  ||  defined(NCBI_OS_IRIX)
    unsigned int  _pad;           /* 4-byte padding to 8 bytes            20 */
#endif
    SLBSM_Sysinfo  sys;           /* system information                   24 */
    TNCBI_Size     env;           /* offset to host environment          124 */
    unsigned short zid;           /* site/zone ID?                       128 */
} SLBSM_Host;                     /*                        Total bytes: 130 */


/* Full service info structure as kept in the heap                      offsets
 */
typedef struct LBSM_PACKED {
    SLBSM_Entry    entry;         /* header (eLBSM_Service|eLBSM_Pending) 0 */
    TNCBI_Size     name;          /* name of this service (offset)        16 */
#if (defined(NCBI_OS_SOLARIS)  &&  !defined(__i386))  ||  defined(NCBI_OS_IRIX)
    unsigned int  _pad1;          /* 4-byte padding to 8 bytes            20 */
#endif
    double         fine;          /* feedback on unreachability, %        24 */
    unsigned int   addr;          /* host, which posted this service      32 */
#if (defined(NCBI_OS_SOLARIS)  &&  !defined(__i386))  ||  defined(NCBI_OS_IRIX)
    unsigned int  _pad2;          /* 4-byte padding to 8 bytes            36 */
#endif
    SSERV_Info     info;          /* server descriptor, must go last      40 */
} SLBSM_Service;                  /*                        Total bytes: 112 */


/* Configuration file name                                              offsets
 */
typedef struct LBSM_PACKED {
    SLBSM_Entry    entry;         /* entry header (type == eLBSM_Config)   0 */
    char           name[1];       /* '\0'-terminated C string             16 */
} SLBSM_Config;                   /*                        Total bytes:  17+*/


#ifndef __GNUC__
#  if   defined(NCBI_OS_SOLARIS)
#    ifndef __i386
#      pragma pack()
#    endif
#  elif defined(NCBI_OS_IRIX)
#    pragma pack 0
#  endif
#endif


/* Get current LBSM heap version, or 0 if none detected.
 */
const SLBSM_Version* LBSM_GetVersion(HEAP heap);


/* Get configuration back from the heap (NULL if not available).
 */
const char*          LBSM_GetConfig(HEAP heap);


/* Get next named service info from LBSM heap with just a few sanity checks.
 * In client code, it can only return eLBSM_Service entries.
 * In daemon code, it can return either eLBSM_Service or eLBSM_Pending.
 * NB: "name" passed as NULL causes to return every service entry
 *      from the heap (the value of the "mask" parameter is ignored).
 */
const SLBSM_Service* LBSM_LookupServiceEx
(HEAP                  heap,         /* LBSM heap handle                     */
 const char*           name,         /* name(mask) of service to look for    */
 int/*bool*/           mask,         /* true if name is a mask               */
 const SLBSM_Entry**   prev          /* previously found info ptr (non-NULL) */
 );


/* Get next named service info from LBSM heap, with argument sanity checks.
 * Only entries of type eLBSM_Service can be returned.
 * NB: "name" passed as NULL causes to return every service entry
 *      from the heap (the value of the "mask" parameter is ignored).
 */
const SLBSM_Service* LBSM_LookupService
(HEAP                  heap,         /* LBSM heap handle                     */
 const char*           name,         /* name(mask) of service to look for    */
 int/*bool*/           mask,         /* true if name is a mask               */
 const SLBSM_Service*  prev          /* previously found info (or NULL)      */
 );


/* Get host info from LBSM heap.  Passing "addr" as 0 returns the next host
 * from the heap, which can be used as "hint" to find the following host, etc
 * -- but in this case, it's user's responsibility to track for wrapped search
 * by making sure it stops when seeing the very first host returned again.
 */
const SLBSM_Host*    LBSM_LookupHost
(HEAP                  heap,         /* LBSM heap handle                     */
 unsigned              addr,         /* host IP addr (n.b.o) to look for     */
 const SLBSM_Entry*    hint          /* hint to where start looking from     */
 );


/* Calculate status of the service based on machine load and rating.
 * NOTE: Last 2 arguments are not used if rate <= 0.0 (static or down server).
 */
double               LBSM_CalculateStatus
(double                rate,  /* [in] service rate                           */
 double                fine,  /* [in] feedback penalty from application(s)   */
 ESERV_Algo            algo,  /* [in] status calculation flags {Reg | Blast} */
 const SLBSM_HostLoad* load   /* [in] machine load parameters                */
 );


#ifdef   HUGE_VAL
#  define LBSM_RERATE_SPECIAL  HUGE_VAL
#else
#  define LBSM_RERATE_SPECIAL  1e9
#endif /*HUGE_VAL*/

#define LBSM_RERATE_RESERVE  (-LBSM_RERATE_SPECIAL)
#define LBSM_RERATE_DEFAULT  ( LBSM_RERATE_SPECIAL)


/* Set a penalty (expressed in percentages of unavailability) via LBSM daemon,
 * or rerate the given server (LBSM_RERATE_DEFAULT to set the default rate per
 * server configuration, LBSM_RERATE_RESERVE to reserve the server, which makes
 * it unavailable for most uses, or 0 to turn the server off) so that every
 * LBSM host will have the information soon.  Return 0 if submission failed;
 * non-zero otherwise.
 */
int/*bool*/ LBSM_SubmitPenaltyOrRerate
(const char*           name,  /* [in] service name to be penalized/re-rated  */
 ESERV_Type            type,  /* [in] service type (must be valid)           */
 double                rate,  /* [in] fine[0..100] or rate(or SPECIAL) to set*/
 TNCBI_Time            fine,  /* [in] non-zero for penalty (hold time) else 0*/
 unsigned int          host,  /* [in] optional host address; default(0)-local*/
 unsigned short        port,  /* [in] optional port number;  default(0)-any  */
 const char*           path   /* [in] path to LBSMD feedback file (NULL=def) */
 );


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CONNECT___NCBI_LBSM__H */
