#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */
/* ===========================================================================
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
 * Author:  Christiam Camacho
 *
 */

/** @file blast_seqsrc_adapter_priv.cpp
 * NOTE: This file contains work in progress and the APIs are likely to change,
 * please do not rely on them until this notice is removed.
 */

#include <ncbi_pch.hpp>
#include "blast_seqsrc_adapter_priv.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

IBlastSeqSrcAdapter::IBlastSeqSrcAdapter(const CSearchDatabase& dbinfo)
    : m_DbInfo(dbinfo), m_SeqSrc(0)
{
}

IBlastSeqSrcAdapter::~IBlastSeqSrcAdapter()
{
    if (m_SeqSrc) {
        m_SeqSrc = BlastSeqSrcFree(m_SeqSrc);
    }
}

BlastSeqSrc*
IBlastSeqSrcAdapter::GetBlastSeqSrc()
{
    if ( ! m_SeqSrc ) {
        m_SeqSrc = x_CalculateBlastSeqSrc(m_DbInfo);
    }
    return m_SeqSrc;
}

END_SCOPE(Blast)
END_NCBI_SCOPE

/* @} */
