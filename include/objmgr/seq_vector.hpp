#ifndef SEQ_VECTOR__HPP
#define SEQ_VECTOR__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman, Eugene Vasilchenko
*
* File Description:
*   Sequence data container for object manager
*
*/

#include <objmgr/bioseq_handle.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_map.hpp>
#include <objmgr/seq_vector_ci.hpp>
#include <objects/seq/Seq_literal.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup ObjectManagerSequenceRep
 *
 * @{
 */

class CRandom;

BEGIN_SCOPE(objects)

class CScope;
class CSeq_loc;
class CSeqMap;
class CSeqVector_CI;

/////////////////////////////////////////////////////////////////////////////
///
///  CSeqVector --
///
///  Provide sequence data in the selected coding

class NCBI_XOBJMGR_EXPORT CSeqVector : public CObject, public CSeqVectorTypes
{
public:
    typedef CBioseq_Handle::EVectorCoding EVectorCoding;
    typedef CSeqVector_CI const_iterator;
    typedef CMutex TMutex;
    typedef TMutex::TWriteLockGuard TMutexGuard;

    CSeqVector(void);
    explicit
    CSeqVector(const CBioseq_Handle& bioseq,
               EVectorCoding coding = CBioseq_Handle::eCoding_Ncbi,
               ENa_strand strand = eNa_strand_unknown);
    CSeqVector(const CSeqMap& seqMap, CScope& scope,
               EVectorCoding coding = CBioseq_Handle::eCoding_Ncbi,
               ENa_strand strand = eNa_strand_unknown);
    CSeqVector(const CSeqMap& seqMap, const CTSE_Handle& top_tse,
               EVectorCoding coding = CBioseq_Handle::eCoding_Ncbi,
               ENa_strand strand = eNa_strand_unknown);
    CSeqVector(const CSeq_loc& loc, CScope& scope,
               EVectorCoding coding = CBioseq_Handle::eCoding_Ncbi,
               ENa_strand strand = eNa_strand_unknown);
    CSeqVector(const CSeq_loc& loc, const CTSE_Handle& top_tse,
               EVectorCoding coding = CBioseq_Handle::eCoding_Ncbi,
               ENa_strand strand = eNa_strand_unknown);
    CSeqVector(const CBioseq& bioseq,
               CScope* scope = 0,
               EVectorCoding coding = CBioseq_Handle::eCoding_Ncbi,
               ENa_strand strand = eNa_strand_unknown);
    CSeqVector(const CSeqVector& vec);

    virtual ~CSeqVector(void);

    CSeqVector& operator= (const CSeqVector& vec);

    bool empty(void) const;
    TSeqPos size(void) const;

    /// Get mutex for a few non-MT-safe methods to make them MT-safe at a cost
    /// of performance.
    TMutex& GetMutex(void) const;

    /// 0-based array of residues
    /// Note: this method is not MT-safe,
    /// do not call it in parallel with other, even MT-safe method.
    /// It will be MT-safe to call this method after locking GetMutex().
    TResidue operator[] (TSeqPos pos) const;

    /// true if sequence at 0-based position 'pos' has gap
    /// Note: this method is not MT-safe,
    /// do not call it in parallel with other, even MT-safe method.
    /// It will be MT-safe to call this method after locking GetMutex().
    bool IsInGap(TSeqPos pos) const;

    /// returns number of gap symbols ahead including base at position 'pos'
    /// returns 0 if the position is not in gap
    TSeqPos GetGapSizeForward(TSeqPos pos) const;

    /// returns gap Seq-literal object ref
    /// returns null if it's not a gap or an unspecified gap
    CConstRef<CSeq_literal> GetGapSeq_literal(TSeqPos pos) const;

    /// Check if the sequence data is available for the interval [start, stop).
    bool CanGetRange(TSeqPos start, TSeqPos stop) const;
    bool CanGetRange(const const_iterator& start,
                     const const_iterator& stop) const;

    /// Fill the buffer string with the sequence data for the interval
    /// [start, stop).
    void GetSeqData(TSeqPos start, TSeqPos stop, string& buffer) const;
    void GetSeqData(const const_iterator& start,
                    const const_iterator& stop,
                    string& buffer) const;
    void GetPackedSeqData(string& buffer,
                          TSeqPos start = 0,
                          TSeqPos stop = kInvalidSeqPos);

    typedef CSeq_inst::TMol TMol;

    TMol GetSequenceType(void) const;
    bool IsProtein(void) const;
    bool IsNucleotide(void) const;

    CScope& GetScope(void) const;
    const CSeqMap& GetSeqMap(void) const;
    ENa_strand GetStrand(void) const;
    void SetStrand(ENa_strand strand);

    /// Target sequence coding. CSeq_data::e_not_set -- do not
    /// convert sequence (use GetCoding() to check the real coding).
    TCoding GetCoding(void) const;
    void SetCoding(TCoding coding);
    /// Set coding to either Iupacaa or Iupacna depending on molecule type
    void SetIupacCoding(void);
    /// Set coding to either Ncbi8aa or Ncbi8na depending on molecule type
    void SetNcbiCoding(void);
    /// Set coding to either Iupac or Ncbi8xx
    void SetCoding(EVectorCoding coding);

    /// Return gap symbol corresponding to the selected coding
    TResidue GetGapChar(ECaseConversion case_cvt = eCaseConversion_none) const;

    const_iterator begin(void) const;
    const_iterator end(void) const;

    /// Randomization of ambiguities and gaps in ncbi2na coding
    void SetRandomizeAmbiguities(void);
    void SetRandomizeAmbiguities(Uint4 seed);
    void SetRandomizeAmbiguities(CRandom& random_gen);
    void SetRandomizeAmbiguities(CRef<INcbi2naRandomizer> randomizer);
    void SetNoAmbiguities(void);

private:

    friend class CBioseq_Handle;
    friend class CSeqVector_CI;

    void x_InitSequenceType(void);

    // this internal method is not MT-safe and must be guarded if necessary
    CSeqVector_CI& x_GetIterator(TSeqPos pos) const;
    // this internal method is not MT-safe and must be guarded if necessary
    CSeqVector_CI* x_CreateIterator(TSeqPos pos) const;

    void x_ResetIterator(void) const;

    void x_InitRandomizer(CRandom& random_gen);

    void x_GetPacked8SeqData(string& dst_str,
                             TSeqPos src_pos, TSeqPos src_end);
    void x_GetPacked4naSeqData(string& dst_str,
                               TSeqPos src_pos, TSeqPos src_end);
    void x_GetPacked2naSeqData(string& dst_str,
                               TSeqPos src_pos, TSeqPos src_end);

    CHeapScope               m_Scope;
    CConstRef<CSeqMap>       m_SeqMap;
    CTSE_Handle              m_TSE;
    TSeqPos                  m_Size;
    TMol                     m_Mol;
    ENa_strand               m_Strand;
    TCoding                  m_Coding;
    CRef<INcbi2naRandomizer> m_Randomizer;

    mutable TMutex                  m_IteratorMutex;
    mutable AutoPtr<CSeqVector_CI>  m_Iterator;
};


/////////////////////////////////////////////////////////////////////////////
///
///  CNcbi2naRandomizer --
///

class NCBI_XOBJMGR_EXPORT CNcbi2naRandomizer : public INcbi2naRandomizer
{
public:
    // If seed == 0 then use random number for seed
    CNcbi2naRandomizer(CRandom& gen);
    ~CNcbi2naRandomizer(void);

    void RandomizeData(char* buffer,  // buffer to be randomized
                       size_t count,  // number of bases in the buffer
                       TSeqPos pos);  // sequence pos of the buffer

private:
    enum {
        kRandomizerPosMask = 0x3f,
        kRandomDataSize    = kRandomizerPosMask + 1,
        kRandomValue       = 16
    };

    char m_FixedTable[16];
    char m_RandomTable[16][kRandomDataSize];
};


/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
CSeqVector_CI& CSeqVector::x_GetIterator(TSeqPos pos) const
{
    CSeqVector_CI* iter = m_Iterator.get();
    if ( !iter ) {
        iter = x_CreateIterator(pos);
    }
    else {
        iter->SetPos(pos);
    }
    return *iter;
}


inline
CSeqVector::TMutex& CSeqVector::GetMutex(void) const
{
    return m_IteratorMutex;
}


inline
CSeqVector::TResidue CSeqVector::operator[] (TSeqPos pos) const
{
    return *x_GetIterator(pos);
}


inline
bool CSeqVector::IsInGap(TSeqPos pos) const
{
    return x_GetIterator(pos).IsInGap();
}


inline
bool CSeqVector::empty(void) const
{
    return m_Size == 0;
}


inline
TSeqPos CSeqVector::size(void) const
{
    return m_Size;
}


inline
CSeqVector_CI CSeqVector::begin(void) const
{
    return CSeqVector_CI(*this, 0);
}


inline
CSeqVector_CI CSeqVector::end(void) const
{
    return CSeqVector_CI(*this, size());
}


inline
CSeqVector::TCoding CSeqVector::GetCoding(void) const
{
    return m_Coding;
}

inline
CSeqVector::TResidue CSeqVector::GetGapChar(ECaseConversion case_cvt) const
{
    return sx_GetGapChar(GetCoding(), case_cvt);
}

inline
const CSeqMap& CSeqVector::GetSeqMap(void) const
{
    return *m_SeqMap;
}

inline
CScope& CSeqVector::GetScope(void) const
{
    return m_Scope;
}

inline
ENa_strand CSeqVector::GetStrand(void) const
{
    return m_Strand;
}


inline
CSeqVector::TMol CSeqVector::GetSequenceType(void) const
{
    return m_Mol;
}


inline
bool CSeqVector::IsProtein(void) const
{
    return CSeq_inst::IsAa(GetSequenceType());
}


inline
bool CSeqVector::IsNucleotide(void) const
{
    return CSeq_inst::IsNa(GetSequenceType());
}


inline
bool CSeqVector::CanGetRange(const const_iterator& start,
                             const const_iterator& stop) const
{
    return CanGetRange(start.GetPos(), stop.GetPos());
}


inline
void CSeqVector::GetSeqData(const const_iterator& start,
                            const const_iterator& stop,
                            string& buffer) const
{
    GetSeqData(start.GetPos(), stop.GetPos(), buffer);
}


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SEQ_VECTOR__HPP
