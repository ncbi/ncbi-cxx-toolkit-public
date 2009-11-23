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
 * Author: Pavel Ivanov
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiatomic.hpp>

#include "nc_utils.hpp"


BEGIN_NCBI_SCOPE;

/// TLS key for storing index of the current thread
static TNCTlsKey      s_NCThreadIndexKey;
/// Next available thread index
static CAtomicCounter s_NextThreadIndex;

void
g_InitNCThreadIndexes(void)
{
    NCTlsCreate(s_NCThreadIndexKey, NULL);
    s_NextThreadIndex.Set(0);
}

TNCThreadIndex
g_GetNCThreadIndex(void)
{
    TNCThreadIndex index = static_cast<TNCThreadIndex>(
                      reinterpret_cast<size_t>(NCTlsGet(s_NCThreadIndexKey)));
    if (!index) {
        index = static_cast<TNCThreadIndex>(s_NextThreadIndex.Add(1));
        NCTlsSet(s_NCThreadIndexKey, (void*)index);
    }
    return index;
}

END_NCBI_SCOPE;
