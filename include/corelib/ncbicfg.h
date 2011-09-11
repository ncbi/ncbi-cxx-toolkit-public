#ifndef CORELIB___NCBICFG__H
#define CORELIB___NCBICFG__H

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
 * Author:  Aaron Ucko
 *
 *
 */

/**
 * @file ncbicfg.h
 *
 * Defines access to miscellaneous global configuration settings.
 *
 */

#include <common/ncbi_export.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Get default run path. */
NCBI_XNCBI_EXPORT
const char* NCBI_GetDefaultRunpath(void);

/** Get run path. */
NCBI_XNCBI_EXPORT
const char* NCBI_GetRunpath(void);

/** Set run path. */
NCBI_XNCBI_EXPORT
void        NCBI_SetRunpath(const char* runpath);


/** Get default Sybase client installation path. */
NCBI_XNCBI_EXPORT
const char* NCBI_GetDefaultSybasePath(void);

/** Get Sybase client installation path. */
NCBI_XNCBI_EXPORT
const char* NCBI_GetSybasePath(void);

/** Set Sybase client installation path. */
NCBI_XNCBI_EXPORT
void        NCBI_SetSybasePath(const char* sybpath);

#ifdef __cplusplus
}
#endif


#endif  /* CORELIB___NCBICFG__HPP */
