#ifndef OBJTOOLS_ALNMGR___ALN_TESTS__HPP
#define OBJTOOLS_ALNMGR___ALN_TESTS__HPP
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
*   Tests on Seq-align containers
*
* ===========================================================================
*/


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <objtools/alnmgr/alnexception.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


/// Vector of Seq-ids per Seq-align
template <class _TAlnVec,
          class TAlnSeqIdExtract>
class CAlnIdMap : public CObject
{
public:
    /// Types:
    typedef _TAlnVec TAlnVec;
    typedef typename TAlnSeqIdExtract::TIdVec TIdVec;
    typedef TIdVec value_type;
    typedef size_t size_type;


    /// Construction
    CAlnIdMap(const TAlnSeqIdExtract& extract,    //< AlnSeqId extract functor
              size_t expected_number_of_alns = 0) //< Optimization, since most likely size is known in advance
        : m_Extract(extract)
    {
        m_AlnIdVec.reserve(expected_number_of_alns);
        m_AlnVec.reserve(expected_number_of_alns);
    }


    /// Adding an alignment.  
    /// NB #1: An exception might be thrown here if the alignment's
    ///        seq-ids are invalid.
    /// NB #2: Only seq-ids are validated in release mode.  The
    ///        alignment is assumed to be otherwise valid.  For
    ///        efficiency (to avoid multiple validation), it is up to
    ///        the user, to assure the validity of the alignments.
    void push_back(const CSeq_align& aln) {
#ifdef _DEBUG
        aln->Validate(true);
#endif
        TAlnMap::const_iterator it = m_AlnMap.find(&aln);
        if (it != m_AlnMap.end()) {
            NCBI_THROW(CAlnException, 
                       eInvalidRequest, 
                       "Seq-align was previously pushed_back.");
        } else {
            try {
                size_t aln_idx = m_AlnIdVec.size();
                m_AlnMap.insert(make_pair(&aln, aln_idx));
                m_AlnIdVec.resize(aln_idx + 1);
                m_Extract(aln, m_AlnIdVec[aln_idx]);
                _ASSERT( !m_AlnIdVec[aln_idx].empty() );
            } catch (const CAlnException& e) {
                m_AlnMap.erase(&aln);
                m_AlnIdVec.pop_back();
                NCBI_EXCEPTION_THROW(e);
            }
            m_AlnVec.push_back(&aln);
        }
    }


    /// Accessing the vector of alignments
    const TAlnVec& GetAlnVec() const {
        return m_AlnVec;
    }


    /// Accessing the seq-ids of a particular seq-align
    const TIdVec& operator[](size_t aln_idx) const {
        _ASSERT(aln_idx < m_AlnIdVec.size());
        return m_AlnIdVec[aln_idx];
    }


    /// Accessing the seq-ids of a particular seq-align
    const TIdVec& operator[](const CSeq_align& aln) const {
        return m_AlnIdVec[m_AlnMap[&aln]];
    }


    /// Size
    size_type size() const {
        return m_AlnIdVec.size();
    }


private:
    const TAlnSeqIdExtract& m_Extract;

    typedef map<const CSeq_align*, size_t> TAlnMap;
    TAlnMap m_AlnMap;

    typedef vector<TIdVec> TAlnIdVec;
    TAlnIdVec m_AlnIdVec;

    TAlnVec m_AlnVec;
};


END_NCBI_SCOPE


/*
* ===========================================================================
* $Log$
* Revision 1.13  2007/01/10 19:15:13  todorov
* Improved comments.
* CException -> CAlnException
*
* Revision 1.12  2007/01/10 18:11:34  todorov
* Renamed CAlnSeqIdVector -> CAlnIdMap
* CAlnIdMap is now a dual-access (vector-like indexed + seq-align
* lookup) aln<->id mapping container.
*
* Revision 1.11  2006/12/12 20:50:53  todorov
* Update CAlnSeqIdVector per the new seq-id abstraction.
* Rm CSeqIdAlnBitmap since this analysis is now done directly in
* CAlnStats's constructor.
*
* Revision 1.10  2006/12/11 20:43:09  yazhuk
* Removed conflicting TSeqIdPtr definition.
*
* Revision 1.9  2006/11/27 19:39:19  todorov
* Require comp in CSeqIdAlnBitmap
*
* Revision 1.8  2006/11/20 18:47:16  todorov
* + CSeqIdAlnBitmap::GetBaseWidths()
* + CSeqIdAlnBitmap::GetAnchorRows()
*
* Revision 1.7  2006/11/09 00:16:54  todorov
* Fixed Dump.
*
* Revision 1.6  2006/11/08 22:27:59  todorov
* + template <class TOutStream> void Dump(TOutStream& os)
*
* Revision 1.5  2006/11/06 19:59:54  todorov
* Minor changes.  Added GetAlnVector()
*
* Revision 1.4  2006/10/23 17:17:12  todorov
* name change AlnContainer -> AlnVector, since random access is required.
*
* Revision 1.3  2006/10/19 17:08:12  todorov
* - #include <objtools/alnmgr/seqid_comp.hpp>
*
* Revision 1.2  2006/10/17 21:53:31  todorov
* + GetAnchorHandle
* Renamed all accessors with Get*
*
* Revision 1.1  2006/10/17 19:23:57  todorov
* Initial revision.
*
* ===========================================================================
*/

#endif  // OBJTOOLS_ALNMGR___ALN_TESTS__HPP
