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

/** @file psiblast_subject.cpp
 * Defines IPsiBlastSubject interface
 */

#include <ncbi_pch.hpp>
#include "psiblast_subject.hpp"
#include <algo/blast/api/blast_seqinfosrc.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

IPsiBlastSubject::IPsiBlastSubject() THROWS_NONE
: m_SeqSrc(0), m_OwnSeqSrc(false), m_SeqInfoSrc(0), m_OwnSeqInfoSrc(false)
{}

IPsiBlastSubject::~IPsiBlastSubject()
{
    if (m_OwnSeqSrc && m_SeqSrc) {
        BlastSeqSrcFree(m_SeqSrc);
    }
    if (m_OwnSeqInfoSrc && m_SeqInfoSrc) {
        delete m_SeqInfoSrc;
    }
}

/// Checks if the BlastSeqSrc initialization succeeded
/// @throws CBlastException if BlastSeqSrc initialization failed
static void
s_CheckForBlastSeqSrcErrors(const BlastSeqSrc* seqsrc)
{
    if ( !seqsrc ) {
        return;
    }

    char* error_str = BlastSeqSrcGetInitError(seqsrc);
    if (error_str) {
        string msg(error_str);
        sfree(error_str);
        NCBI_THROW(CBlastException, eSeqSrcInit, msg);
    }
}

BlastSeqSrc*
IPsiBlastSubject::MakeSeqSrc()
{
    if ( !m_SeqSrc ) {
        x_MakeSeqSrc();
        s_CheckForBlastSeqSrcErrors(m_SeqSrc);
        ASSERT(m_SeqSrc);
    }
    return m_SeqSrc;
}

IBlastSeqInfoSrc*
IPsiBlastSubject::MakeSeqInfoSrc()
{
    if ( !m_SeqInfoSrc ) {
        x_MakeSeqInfoSrc();
        ASSERT(m_SeqInfoSrc);
    }
    return m_SeqInfoSrc;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

