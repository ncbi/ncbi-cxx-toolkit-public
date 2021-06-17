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
#include <objtools/alnmgr/seqids_extractor.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


/// Container mapping seq-aligns to vectors of participating seq-ids.
/// TAlnSeqIdExtract is a functor used to extract seq-ids from seq-aligns.
/// @sa CAlnSeqIdsExtract
/// @sa TAlnIdMap
/// @sa TScopeAlnIdMap
template <class _TAlnVec,
          class TAlnSeqIdExtract>
class CAlnIdMap : public CObject
{
public:
    /// Container (vector) of seq-aligns.
    typedef _TAlnVec TAlnVec;
    /// Container (vector) of seq-ids.
    /// @sa TAlnSeqIdIRef
    typedef typename TAlnSeqIdExtract::TIdVec TIdVec;
    typedef TIdVec value_type;
    typedef size_t size_type;


    /// Constructor.
    /// @param extract
    ///   Functor for extracting AlnSeqId from seq-aligns.
    /// @param expected_number_of_alns
    ///   Hint for optimization - the expected number of alignments.
    CAlnIdMap(const TAlnSeqIdExtract& extract,
              size_t expected_number_of_alns = 0)
        : m_Extract(extract)
    {
        m_AlnIdVec.reserve(expected_number_of_alns);
        m_AlnVec.reserve(expected_number_of_alns);
    }

    /// Adding an alignment.
    /// NB #1: An exception might be thrown here if the alignment's
    ///        seq-ids are inconsistent.
    /// NB #2: Only seq-ids are validated in release mode. The
    ///        alignment is assumed to be otherwise valid. For
    ///        efficiency (to avoid multiple validation), it is up to
    ///        the user to assure the validity of the alignments.
    void push_back(const CSeq_align& aln)
    {
#ifdef _DEBUG
        aln.Validate(true);
#endif
        TAlnMap::const_iterator it = m_AlnMap.find(&aln);
        if (it != m_AlnMap.end()) {
            NCBI_THROW(CAlnException,
                eInvalidRequest, 
                "Seq-align was previously pushed_back.");
        }
        else {
            try {
                size_t aln_idx = m_AlnIdVec.size();
                m_AlnMap.insert(make_pair(&aln, aln_idx));
                m_AlnIdVec.resize(aln_idx + 1);
                m_Extract(aln, m_AlnIdVec[aln_idx]);
                _ASSERT( !m_AlnIdVec[aln_idx].empty() );
            }
            catch (const CAlnException& e) {
                m_AlnMap.erase(&aln);
                m_AlnIdVec.pop_back();
                NCBI_EXCEPTION_THROW(e);
            }
            m_AlnVec.push_back(CConstRef<CSeq_align>(&aln));
        }
    }

    /// Accessing the vector of alignments
    const TAlnVec& GetAlnVec(void) const
    {
        return m_AlnVec;
    }

    /// Accessing the seq-ids of a particular seq-align
    const TIdVec& operator[](size_t aln_idx) const
    {
        _ASSERT(aln_idx < m_AlnIdVec.size());
        return m_AlnIdVec[aln_idx];
    }

    /// Accessing the seq-ids of a particular seq-align
    const TIdVec& operator[](const CSeq_align& aln) const
    {
        TAlnMap::const_iterator it = m_AlnMap.find(&aln);
        if (it == m_AlnMap.end()) {
            NCBI_THROW(CAlnException, eInvalidRequest,
                "alignment not present in map");
        }
        else {
            return m_AlnIdVec[it->second];
        }
    }

    /// Size (number of alignments)
    size_type size(void) const
    {
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


/// Default implementations of CAlnIdMap.
typedef CAlnIdMap<vector<const CSeq_align*>, TIdExtract> TAlnIdMap;
typedef CAlnIdMap<vector<const CSeq_align*>, TScopeIdExtract> TScopeAlnIdMap;


END_NCBI_SCOPE

#endif  // OBJTOOLS_ALNMGR___ALN_TESTS__HPP
