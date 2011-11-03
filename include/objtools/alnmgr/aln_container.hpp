#ifndef OBJTOOLS_ALNMGR___ALN_CONTAINER__HPP
#define OBJTOOLS_ALNMGR___ALN_CONTAINER__HPP
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
* Author:  Kamen Todorov, NCBI
*
* File Description:
*   Seq-align container
*
* ===========================================================================
*/


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/seqalign_exception.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


class CAlnContainer
{
public:
    CAlnContainer() :
        m_SplitDisc(true)
    {};

    virtual ~CAlnContainer() {};

private:
    typedef CSeq_align::TSegs TSegs;
    typedef list<CConstRef<CSeq_align> > TAlnSet;
	TAlnSet m_AlnSet;

public:
    typedef TAlnSet::size_type size_type;
    typedef TAlnSet::const_iterator const_iterator;
    typedef TAlnSet::const_reverse_iterator const_reverse_iterator;


    /// Split Disc alignments?
    void SetSplitDisc() {
        m_SplitDisc = true;
    }
    void UnsetSplitDisc() {
        m_SplitDisc = false;
    }
    bool IsSetSplitDisc() {
        return m_SplitDisc;
    }

    const_iterator insert(const CSeq_align& seq_align)
    {
#if _DEBUG
        seq_align.Validate(true);
#endif
        const_iterator ret_it = end();
        switch (seq_align.GetSegs().Which()) {
        case TSegs::e_Disc:
            if (m_SplitDisc) {
                ITERATE(CSeq_align_set::Tdata,
                        seq_align_it,
                        seq_align.GetSegs().GetDisc().Get()) {
                    ret_it = insert(**seq_align_it);
                }
                break;
            }
        case TSegs::e_Dendiag:
        case TSegs::e_Denseg:
        case TSegs::e_Std:
        case TSegs::e_Packed:
        case TSegs::e_Spliced:
        case TSegs::e_Sparse:
			NON_CONST_ITERATE(TAlnSet, iter, m_AlnSet) {
				if( *iter == &seq_align )
					return iter;
			}
            ret_it =
                m_AlnSet.insert(m_AlnSet.end(), CConstRef<CSeq_align>(&seq_align));
            break;
        case TSegs::e_not_set:
            NCBI_THROW(CSeqalignException, eInvalidAlignment,
                       "Seq-align.segs not set.");
        default:
            NCBI_THROW(CSeqalignException, eUnsupported,
                       "Unsupported alignment type.");
        }
        return ret_it;
    }

    const_iterator begin() const {
        return m_AlnSet.begin();
    }

    const_iterator end() const {
        return m_AlnSet.end();
    }

    const_reverse_iterator rbegin() const {
        return m_AlnSet.rbegin();
    }

    const_reverse_iterator rend() const {
        return m_AlnSet.rend();
    }

    size_type size() const {
        return m_AlnSet.size();
    }

    bool empty() const {
        return m_AlnSet.empty();
    }

private:
    bool m_SplitDisc;
};


END_NCBI_SCOPE

#endif  // OBJTOOLS_ALNMGR___ALN_CONTAINER__HPP
