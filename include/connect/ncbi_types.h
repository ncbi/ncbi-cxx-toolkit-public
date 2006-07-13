#ifndef CONNECT___NCBI_TYPES__H
#define CONNECT___NCBI_TYPES__H

/*  $Id$
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
 * @file
 * File Description:
 *   Special types for core library.
 *
 * Timeout:
 *    struct STimeout
 *
 * Switch:
 *    ESwitch         (on/off/default)
 *
 * Fixed-size size_t and time_t equivalents:
 *    TNCBI_Size
 *    TNCBI_Time
 *       these two we need to use when mixing 32/64 bit programs
 *       which make simultaneous access to inter-process communication
 *       data areas, like shared memory segments
 *
 */

#include <connect/connect_export.h>
#include <stddef.h>


/** @addtogroup UtilityFunc
 *
 * @{
 */


#ifdef __cplusplus
extern "C" {
#endif


/** Timeout structure
 */
typedef struct {
    unsigned int sec;  /** seconds (truncated to the platf.-dep. max. limit) */
    unsigned int usec; /** microseconds (always truncated by mod. 1,000,000) */
} STimeout;

#define kDefaultTimeout  ((const STimeout*)(-1))
#define kInfiniteTimeout ((const STimeout*)( 0))


/** Aux. enum to set/unset/default various features
 */
typedef enum {
    eOff = 0,
    eOn,
    eDefault
} ESwitch;


/** Fixed size analogs of size_t and time_t (mainly for IPC)
 */
typedef unsigned int TNCBI_Size;
typedef unsigned int TNCBI_Time;

#define NCBI_TIME_INFINITE ((TNCBI_Time)(-1))


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.12  2006/07/13 16:05:42  lavr
 * Doxygenized
 *
 * Revision 6.11  2006/03/04 17:01:14  lavr
 * Clean away dead #if 0 branch
 *
 * Revision 6.10  2005/11/22 20:52:38  lavr
 * Removed a note about ncbiconf.h -- irrelevant
 *
 * Revision 6.9  2003/08/28 19:28:47  ucko
 * Use macros for kXxxTimeout on all platforms (safer, inasmuch as the C
 * include directory may be first).
 *
 * Revision 6.8  2003/08/28 18:47:25  ucko
 * Go back to previous WorkShop hack, but include connect_export.h (for
 * ncbiconf.h) so that it actually works reliably this time around.
 *
 * Revision 6.7  2003/08/27 12:32:25  ucko
 * Yet another attempt to work around the WorkShop lossage with k*Timeout.
 *
 * Revision 6.6  2003/08/27 02:00:11  ucko
 * Sigh... WorkShop still mishandles kXxxTimeout in some cases, so fall
 * back to making them macros.
 *
 * Revision 6.5  2003/08/26 18:55:13  lavr
 * Added "static" to k...Timeout to make Sun WorkShop compiler happier
 *
 * Revision 6.4  2003/08/25 14:36:26  lavr
 * +kDefaultTimeout, +kInfiniteTimeout
 *
 * Revision 6.3  2003/04/09 19:05:58  siyan
 * Added doxygen support
 *
 * Revision 6.2  2002/09/19 18:05:41  lavr
 * Header file guard macro changed; log moved to end
 *
 * Revision 6.1  2001/06/19 20:15:58  lavr
 * Initial revision
 *
 * ===========================================================================
 */

#endif /* CONNECT___NCBI_TYPES__H */
