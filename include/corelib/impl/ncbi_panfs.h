#ifndef CORELIB___IMPL___NCBI_PANFS__H
#define CORELIB___IMPL___NCBI_PANFS__H

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
 * Author:  Vladimir Ivanov
 *
 *
 */

/**
 * @file ncbi_panfs.h
 *
 * Defines interface to ncbi_panfs.so to get information about PANFS mounts.
 *
 */

#include <corelib/ncbitype.h>

#if defined(NCBI_OS_LINUX)  &&  defined(HAVE_DLFCN_H)

#  include <dlfcn.h>
#  define ALLOW_USE_NCBI_PANFS_DLL  1


/* Error codes */
#  define  NCBI_PANFS_OK            0   /* No error */
#  define  NCBI_PANFS_ERR           1   /* Common error code */
#  define  NCBI_PANFS_ERR_OPEN     11   /* Cannot open mount */
#  define  NCBI_PANFS_ERR_QUERY    12   /* Path exists, but cannot query information */
#  define  NCBI_PANFS_ERR_VERSION  13   /* Unsupported version of extended information */
#  define  NCBI_PANFS_THROW        -1   /* Critical error, advise to throw an exception */


#if defined __cplusplus
extern "C" {
#endif


/**
 * Get disk space info for PANFS partitions
 * @param path
 *   [in] Path for which to get the space info.
 * @param total_space
 *   [out] Total space at the path  (not NULL!)
 * @param free_space
 *   [out] Free space available at the path  (not NULL!)
 * @param err_msg
 *   [out] Pointer to an immutable "C" string with an error description
 *   (not NULL!)
 * @return
 *   NCBI_PANFS_OK (zero) on success. 
 *   Non-zero error code on error and set 'err_msg' to an error description.
 */
extern int ncbi_GetDiskSpace_PANFS (const char*  path,
                                    Uint8*       total_space,
                                    Uint8*       free_space,
                                    const char** err_msg);

/* Type definition */
typedef int (*FGetDiskSpace_PANFS) (const char*  path,
                                    Uint8*       total_space,
                                    Uint8*       free_space,
                                    const char** err_msg);


#if defined __cplusplus
}
#endif

#endif  /* ALLOW_USE_NCBI_PANFS_DLL */

#endif  /* CORELIB___IMPL___NCBI_PANFS__H */
