#ifndef OBJECTS_ALNMGR___ALNVEC__HPP
#define OBJECTS_ALNMGR___ALNVEC__HPP

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
*   Access to the actual aligned residues
*
*/


#include <objects/alnmgr/alnmap.hpp>
#include <objects/objmgr/seq_vector.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::


// forward declarations
class CObjectManager;
class CScope;

class CAlnVec : public CAlnMap
{
    typedef CAlnMap Tparent;
    typedef map<TNumrow, CBioseq_Handle> TBioseqHandleCache;
    typedef map<TNumrow, CRef<CSeqVector> > TSeqVectorCache;

public:

    // constructor
    CAlnVec(const CDense_seg& ds);
    CAlnVec(const CDense_seg& ds, TNumrow anchor);
    CAlnVec(const CDense_seg& ds, CScope& scope);
    CAlnVec(const CDense_seg& ds, TNumrow anchor, CScope& scope);

    // destructor
    ~CAlnVec(void);

    CScope& GetScope(void) const;

    string GetSeqString(TNumrow row, TSeqPos from, TSeqPos to)        const;
    string GetSeqString(TNumrow row, TNumseg seg, TNumseg offset = 0) const;
    string GetSeqString(TNumrow row, const CRange<TSeqPos>& range)    const;

    const CBioseq_Handle&   GetBioseqHandle(TNumrow row)                  const;
    CSeqVector::TResidue    GetResidue     (TNumrow row, TSeqPos aln_pos) const;

    // functions for manipulating the consensus sequence
    const CBioseq_Handle&   GetConsensusHandle  (void)        const;
    TSignedSeqPos           GetConsensusStart   (TNumseg seg) const;
    TSignedSeqPos           GetConsensusStop    (TNumseg seg) const;
    TSeqPos                 GetConsensusSeqStart(void)        const;
    TSeqPos                 GetConsensusSeqStop (void)        const;

    string      GetConsensusString(TSeqPos from, TSeqPos to) const;
    string      GetConsensusString(TNumseg seg)              const;
    void        SetAnchorConsensus(void);

    // temporaries for conversion (see note below)
    static unsigned char FromIupac(unsigned char c);
    static unsigned char ToIupac  (unsigned char c);

private:
    // Prohibit copy constructor and assignment operator
    CAlnVec(const CAlnVec& value);
    CAlnVec& operator=(const CAlnVec& value);

    CSeqVector& x_GetSeqVector         (TNumrow row) const;
    CSeqVector& x_GetConsensusSeqVector(void)        const;
    void        x_CreateConsensus      (void)        const;

    mutable CRef<CObjectManager>    m_ObjMgr;
    mutable CRef<CScope>            m_Scope;
    mutable TBioseqHandleCache      m_BioseqHandlesCache;
    mutable TSeqVectorCache         m_SeqVectorCache;

    // our consensus sequence: a bioseq object and a vector of starts
    mutable CBioseq_Handle          m_ConsensusBioseq;
    mutable CRef<CSeqVector>        m_ConsensusSeqvector;
    mutable vector<TSignedSeqPos>   m_ConsensusStarts;
    
};

///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////

inline
CSeqVector& CAlnVec::x_GetSeqVector(TNumrow row) const
{
    TSeqVectorCache::iterator iter = m_SeqVectorCache.find(row);
    if (iter != m_SeqVectorCache.end()) {
        return *(iter->second);
    } else {
        CRef<CSeqVector> vec =
            new CSeqVector(GetBioseqHandle(row).GetSeqVector
                           (CBioseq_Handle::eCoding_Iupac,
                            CBioseq_Handle::eStrand_Plus));
        return *(m_SeqVectorCache[row] = vec);
    }
}

inline
CSeqVector& CAlnVec::x_GetConsensusSeqVector(void) const
{
    if ( !m_ConsensusBioseq ) {
        x_CreateConsensus();
    }

    if ( !m_ConsensusSeqvector ) {
        m_ConsensusSeqvector =
            new CSeqVector(m_ConsensusBioseq.GetSeqVector
                           (CBioseq_Handle::eCoding_Iupac,
                            CBioseq_Handle::eStrand_Plus));
    }

    return *m_ConsensusSeqvector;
}


inline 
CSeqVector::TResidue CAlnVec::GetResidue(TNumrow row, TSeqPos aln_pos) const
{
    return x_GetSeqVector(row)[GetSeqPosFromAlnPos(aln_pos, row)];
}

inline
const CBioseq_Handle& CAlnVec::GetConsensusHandle(void) const
{
    if ( !m_ConsensusBioseq ) {
        x_CreateConsensus();
    }

    return m_ConsensusBioseq;
}

inline
TSignedSeqPos CAlnVec::GetConsensusStart(TNumseg seg) const
{
    if ( !m_ConsensusBioseq ) {
        x_CreateConsensus();
    }

    return m_ConsensusStarts[x_GetRawSegFromSeg(seg)];
}

inline
TSignedSeqPos CAlnVec::GetConsensusStop(TNumseg seg) const
{
    if ( !m_ConsensusBioseq ) {
        x_CreateConsensus();
    }

    TSignedSeqPos pos = m_ConsensusStarts[x_GetRawSegFromSeg(seg)];

    if (pos != -1) {
        return pos + m_DS->GetLens()[x_GetRawSegFromSeg(seg)] - 1;
    } else {
        return -1;
    }
}

inline
TSeqPos CAlnVec::GetConsensusSeqStart(void) const
{
    if ( !m_ConsensusBioseq ) {
        x_CreateConsensus();
    }

    for (TNumseg seg = 0;  seg < m_ConsensusStarts.size();  ++seg) {
        if (m_ConsensusStarts[seg] != -1) {
            return m_ConsensusStarts[seg];
        }
    }

    return -1;
}

inline
TSeqPos CAlnVec::GetConsensusSeqStop(void) const
{
    if ( !m_ConsensusBioseq ) {
        x_CreateConsensus();
    }

    for (TNumseg seg = m_ConsensusStarts.size() - 1;  seg >= 0;  --seg) {
        if (m_ConsensusStarts[seg] != -1) {
            return m_ConsensusStarts[seg] + m_DS->GetLens()[seg] - 1;
        }
    }

    return -1;
}
//
// these are a temporary work-around
// CSeqportUtil contains routines for converting sequence data from one
// format to another, but it places a requirement on the data: it must in
// a CSeq_data class.  I can get this for my data, but it is a bit of work
// (much more work than calling CSeqVector::GetSeqdata(), which, if I use the
// internal sequence vector, is guaranteed to be in IUPAC notation)
//
inline unsigned char CAlnVec::FromIupac(unsigned char c)
{
    switch (c)
    {
    case 'A': return 0x01;
    case 'C': return 0x02;
    case 'M': return 0x03;
    case 'G': return 0x04;
    case 'R': return 0x05;
    case 'S': return 0x06;
    case 'V': return 0x07;
    case 'T': return 0x08;
    case 'W': return 0x09;
    case 'Y': return 0x0a;
    case 'H': return 0x0b;
    case 'K': return 0x0c;
    case 'D': return 0x0d;
    case 'B': return 0x0e;
    case 'N': return 0x0f;
    }

    return 0x00;
}

inline unsigned char CAlnVec::ToIupac(unsigned char c)
{
    const char *data = "-ACMGRSVTWYHKDBN";
    return ( (c < 16) ? data[c] : 0);
}



///////////////////////////////////////////////////////////
////////////////// end of inline methods //////////////////
///////////////////////////////////////////////////////////

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.3  2002/09/05 19:31:18  dicuccio
* - added ability to reference a consensus sequence for a given alignment
* - added caching of CSeqVector (big performance win)
* - many small bugs fixed
*
* Revision 1.2  2002/08/29 18:40:53  dicuccio
* added caching mechanism for CSeqVector - this greatly improves speed in
* accessing sequence data.
*
* Revision 1.1  2002/08/23 14:43:50  ucko
* Add the new C++ alignment manager to the public tree (thanks, Kamen!)
*
*
* ===========================================================================
*/

#endif // OBJECTS_ALNMGR___ALNVEC__HPP
