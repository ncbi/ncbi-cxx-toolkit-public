#ifndef CONNECT___NCBI_ONCE__H
#define CONNECT___NCBI_ONCE__H

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
 * Authors:  Anton Lavrentiev
 *
 * File Description:
 *   Once time setter CORE_Once
 *
 */

#include "ncbi_config.h"
#ifdef NCBI_CXX_TOOLKIT
#  include <corelib/ncbiatomic.h>
#endif /*NCBI_CXX_TOOLKIT*/
#include "ncbi_priv.h"


/** Return non-zero (true) if "*once" had a value of NULL, and set the value to
 *  non-NULL regardless (best effort: atomically).  Return 0 (false) otherwise.
 */
#ifdef NCBI_CXX_TOOLKIT
#  define CORE_Once(once)  (!NCBI_SwapPointers((once), (void*) 1))
#else
#  ifdef __GNUC__
inline
#  endif /*__GNUC__*/
static int/*bool*/ CORE_Once(void* volatile* once)
{
    /* poor man's solution */
    if (*once)
        return 0/*false*/;
    CORE_LOCK_WRITE;
    if (*once) {
        CORE_UNLOCK;
        return 0/*false*/;
    }
    *once = (void*) 1;
    CORE_UNLOCK;
    return 1/*true*/;
}
#endif /*NCBI_CXX_TOOLKIT*/


#endif /* CONNECT___NCBI_ONCE__H */
