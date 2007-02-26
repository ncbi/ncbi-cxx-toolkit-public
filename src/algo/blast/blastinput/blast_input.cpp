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
 * Author:  Jason Papadopoulos
 *
 */

/** @file algo/blast/blastinput/blast_input.cpp
 * Convert sources of sequence data into blast sequence input
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/scope.hpp>
#include <algo/blast/blastinput/blast_input.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);

CBlastInputConfig::CBlastInputConfig(objects::ENa_strand strand,
                                     bool lowercase,
                                     bool believe_defline,
                                     TSeqRange range)
: m_Strand(strand), m_LowerCaseMask(lowercase), 
  m_BelieveDeflines(believe_defline), m_Range(range)
{}

TSeqLocVector
CBlastInput::GetNextSeqLocBatch()
{
    TSeqLocVector retval;
    TSeqPos size_read = 0;

    while (size_read < m_BatchSize) {

        if (m_Source->End())
            break;

        retval.push_back(m_Source->GetNextSSeqLoc());

        SSeqLoc& loc = retval.back();

        if (loc.seqloc->IsInt()) {
            size_read += sequence::GetLength(loc.seqloc->GetInt().GetId(), 
                                            loc.scope);
        } else if (loc.seqloc->IsWhole()) {
            size_read += sequence::GetLength(loc.seqloc->GetWhole(),
                                            loc.scope);
        } else {
            NCBI_THROW(CBlastException, eInvalidArgument, 
                       "Unsupported SeqLoc type encountered");
        }
    }

    return retval;
}


CRef<CBlastQueryVector>
CBlastInput::GetNextSeqBatch()
{
    CRef<CBlastQueryVector> retval(new CBlastQueryVector);
    TSeqPos size_read = 0;

    while (size_read < m_BatchSize) {

        if (m_Source->End())
            break;

        CRef<CBlastSearchQuery> q(m_Source->GetNextSequence());
        CConstRef<CSeq_loc> loc = q->GetQuerySeqLoc();

        if (loc->IsInt()) {
            size_read += sequence::GetLength(loc->GetInt().GetId(),
                                             q->GetScope());
        } else if (loc->IsWhole()) {
            size_read += sequence::GetLength(loc->GetWhole(), q->GetScope());
        } else {
            NCBI_THROW(CBlastException, eInvalidArgument, 
                       "Unsupported SeqLoc type encountered");
        }

        retval->AddQuery(q);
    }

    return retval;
}


TSeqLocVector
CBlastInput::GetAllSeqLocs()
{
    TSeqLocVector retval;

    while (!m_Source->End()) {
        retval.push_back(m_Source->GetNextSSeqLoc());
    }

    return retval;
}


CRef<CBlastQueryVector>
CBlastInput::GetAllSeqs()
{
    CRef<CBlastQueryVector> retval(new CBlastQueryVector);

    while (!m_Source->End()) {
        CRef<CBlastSearchQuery> q(m_Source->GetNextSequence());
        retval->AddQuery(q);
    }

    return retval;
}


CBlastInputSource::CBlastInputSource(objects::CObjectManager& objmgr)
    : m_ObjMgr(objmgr),
      m_Scope(new CScope(objmgr))
{
    m_Scope->AddDefaults();
}


END_SCOPE(blast)
END_NCBI_SCOPE

/*---------------------------------------------------------------------
 * $Log$
 * Revision 1.2  2006/09/26 21:44:12  papadopo
 * add to blast scope; add CVS log
 *
 *-------------------------------------------------------------------*/
