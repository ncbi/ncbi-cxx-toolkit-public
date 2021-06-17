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
* Authors:  Kamen Todorov, Andrey Yazhuk, NCBI
*
* File Description:
*   Seq-align converters
*
* ===========================================================================
*/


#include <ncbi_pch.hpp>

#include <objtools/alnmgr/aln_container.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


CAlnContainer::CAlnContainer()
    : m_SplitDisc(true)
{
}


CAlnContainer::~CAlnContainer(void)
{
}


CAlnContainer::const_iterator CAlnContainer::insert(const CSeq_align& seq_align)
{
#if _DEBUG
    seq_align.Validate(true);
#endif
    switch ( seq_align.GetSegs().Which() ) {
    case TSegs::e_Disc:
        if ( m_SplitDisc ) {
            const_iterator ret_it = end();
            for ( auto& align_ref : seq_align.GetSegs().GetDisc().Get() ) {
                ret_it = insert(*align_ref);
            }
            return ret_it;
        }
        break; // continue with insertion
    case TSegs::e_Dendiag:
    case TSegs::e_Denseg:
    case TSegs::e_Std:
    case TSegs::e_Packed:
    case TSegs::e_Spliced:
    case TSegs::e_Sparse:
        break; // continue with insertion
    case TSegs::e_not_set:
        NCBI_THROW(CSeqalignException, eInvalidAlignment,
                   "Seq-align.segs not set.");
    default:
        NCBI_THROW(CSeqalignException, eUnsupported,
                   "Unsupported alignment type.");
    }

    auto existing = m_AlnMap.find(&seq_align);
    if ( existing != m_AlnMap.end() ) {
        // Return the existing object iterator.
        return existing->second;
    }
    const_iterator ret_it = m_AlnSet.insert(m_AlnSet.end(), ConstRef(&seq_align));
    m_AlnMap.insert(make_pair(&seq_align, ret_it));
    return ret_it;
}


END_NCBI_SCOPE
