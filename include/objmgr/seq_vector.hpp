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
#include <objects/seq/Seq_data.hpp>
#include <vector>

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
class CSeq_data;
class CSeqVector_CI;
class CNcbi2naRandomizer;

/////////////////////////////////////////////////////////////////////////////
///
///  SSeqData --
///
///  Sequence data

struct NCBI_XOBJMGR_EXPORT SSeqData {
    TSeqPos              length;      ///< Length of the sequence data piece
    TSeqPos              dest_start;  ///< Starting pos in the dest. Bioseq
    TSeqPos              src_start;   ///< Starting pos in the source Bioseq
    CConstRef<CSeq_data> src_data;    ///< Source sequence data
};

/////////////////////////////////////////////////////////////////////////////
///
///  CSeqVector --
///
///  Provide sequence data in the selected coding

class NCBI_XOBJMGR_EXPORT CSeqVector : public CObject
{
public:
    typedef CSeqVector_CI::TResidue TResidue;
    typedef CSeqVector_CI::TCoding  TCoding;
    typedef CBioseq_Handle::EVectorCoding EVectorCoding;

    typedef CSeqVector_CI const_iterator;
    typedef TResidue value_type;
    typedef TSeqPos size_type;
    typedef TSignedSeqPos difference_type;

    CSeqVector(void);
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
    CSeqVector(const CSeqVector& vec);

    virtual ~CSeqVector(void);

    CSeqVector& operator= (const CSeqVector& vec);

    bool empty(void) const;
    TSeqPos size(void) const;
    /// 0-based array of residues
    TResidue operator[] (TSeqPos pos) const;

    /// true if sequence at 0-based position 'pos' has gap
    bool IsInGap(TSeqPos pos) const;

    /// Fill the buffer string with the sequence data for the interval
    /// [start, stop).
    void GetSeqData(TSeqPos start, TSeqPos stop, string& buffer) const;
    void GetSeqData(const const_iterator& start,
                    const const_iterator& stop,
                    string& buffer) const;

    typedef CSeq_inst::TMol TMol;

    TMol GetSequenceType(void) const;
    bool IsProtein(void) const;
    bool IsNucleotide(void) const;

    CScope& GetScope(void) const;
    const CSeqMap& GetSeqMap(void) const;
    ENa_strand GetStrand(void) const;

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
    TResidue GetGapChar(void) const;

    bool CanGetRange(TSeqPos from, TSeqPos to) const;
    bool CanGetRange(const const_iterator& from,
                     const const_iterator& to) const;

    const_iterator begin(void) const;
    const_iterator end(void) const;

    /// Randomization of ambiguities and gaps in ncbi2na coding
    void SetRandomizeAmbiguities(void);
    void SetRandomizeAmbiguities(Uint4 seed);
    void SetRandomizeAmbiguities(CRandom& random_gen);
    void SetNoAmbiguities(void);

private:

    friend class CBioseq_Handle;
    friend class CSeqVector_CI;

    void x_InitSequenceType(void);
    static TResidue x_GetGapChar(TCoding coding);
    CSeqVector_CI& x_GetIterator(TSeqPos pos) const;
    CSeqVector_CI* x_CreateIterator(TSeqPos pos) const;
    void x_InitRandomizer(CRandom& random_gen);

    static const char* sx_GetConvertTable(TCoding src, TCoding dst,
                                          bool reverse);

    CHeapScope               m_Scope;
    CConstRef<CSeqMap>       m_SeqMap;
    CTSE_Handle              m_TSE;
    TSeqPos                  m_Size;
    TMol                     m_Mol;
    ENa_strand               m_Strand;
    TCoding                  m_Coding;
    CRef<CNcbi2naRandomizer> m_Randomizer;

    mutable CSeqVector_CI    m_Iterator;
};


const size_t kRandomizerPosMask = 0x3f;
const size_t kRandomDataSize    = kRandomizerPosMask + 1;


/////////////////////////////////////////////////////////////////////////////
///
///  CNcbi2naRandomizer --
///

class NCBI_XOBJMGR_EXPORT CNcbi2naRandomizer : public CObject
{
public:
    // If seed == 0 then use random number for seed
    CNcbi2naRandomizer(CRandom& gen);
    ~CNcbi2naRandomizer(void);

    typedef char* TData;

    void RandomizeData(TData data,    // cache to be randomized
                       size_t count,  // number of bases in the cache
                       TSeqPos pos);  // sequence pos of the cache

private:
    CNcbi2naRandomizer(const CNcbi2naRandomizer&);
    CNcbi2naRandomizer& operator=(const CNcbi2naRandomizer&);

    // First value in each row indicates ambiguity (1) or
    // normal base (0)
    typedef char        TRandomData[kRandomDataSize + 1];
    typedef TRandomData TRandomTable[16];

    TRandomTable m_RandomTable;
};


/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
CSeqVector::TResidue CSeqVector::operator[] (TSeqPos pos) const
{
    return *m_Iterator.SetPos(pos);
}


inline
bool CSeqVector::IsInGap(TSeqPos pos) const
{
    return m_Iterator.SetPos(pos).IsInGap();
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
CSeqVector::TResidue CSeqVector::GetGapChar(void) const
{
    return x_GetGapChar(GetCoding());
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
bool CSeqVector::CanGetRange(const const_iterator& from,
                             const const_iterator& to) const
{
    return CanGetRange(from.GetPos(), to.GetPos());
}


inline
CSeqVector::TMol CSeqVector::GetSequenceType(void) const
{
    return m_Mol;
}


inline
bool CSeqVector::IsProtein(void) const
{
    return m_Mol == CSeq_inst::eMol_aa;
}


inline
bool CSeqVector::IsNucleotide(void) const
{
    return m_Mol != CSeq_inst::eMol_aa;
}


inline
void CSeqVector::GetSeqData(TSeqPos start, TSeqPos stop, string& buffer) const
{
    m_Iterator.GetSeqData(start, stop, buffer);
}


inline
void CSeqVector::GetSeqData(const const_iterator& start,
                            const const_iterator& stop,
                            string& buffer) const
{
    m_Iterator.GetSeqData(start.GetPos(), stop.GetPos(), buffer);
}


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.57  2005/03/28 20:43:50  jcherry
* Added export specifier
*
* Revision 1.56  2004/12/22 15:56:17  vasilche
* Added CTSE_Handle.
* Added CSeqVector constructor from CBioseq_Handle to allow used TSE linking.
*
* Revision 1.55  2004/12/06 17:10:50  grichenk
* Removed constructor accepting CConstRef<CSeqMap>
*
* Revision 1.54  2004/11/22 16:04:06  grichenk
* Fixed/added doxygen comments
*
* Revision 1.53  2004/10/27 14:17:33  vasilche
* Implemented CSeqVector::IsInGap() and CSeqVector_CI::IsInGap().
*
* Revision 1.52  2004/10/01 19:52:50  kononenk
* Added doxygen formatting
*
* Revision 1.51  2004/06/14 18:30:08  grichenk
* Added ncbi2na randomizer to CSeqVector
*
* Revision 1.50  2004/04/12 16:49:16  vasilche
* Allow null scope in CSeqMap_CI and CSeqVector.
*
* Revision 1.49  2004/02/25 18:58:17  shomrat
* Added a new construtor based on Seq-loc in scope
*
* Revision 1.48  2003/12/02 18:28:07  grichenk
* Pass const_iterator to GetSeqData by reference.
*
* Revision 1.47  2003/12/02 16:42:49  grichenk
* Fixed GetSeqData to return empty string if start > stop.
* Added GetSeqData(const_iterator, const_iterator, string).
*
* Revision 1.46  2003/10/24 19:28:12  vasilche
* Added implementation of CSeqVector::empty().
*
* Revision 1.45  2003/10/08 14:16:54  vasilche
* Removed circular reference CSeqVector <-> CSeqVector_CI.
*
* Revision 1.44  2003/09/30 16:21:59  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* Revision 1.43  2003/08/29 13:34:47  vasilche
* Rewrote CSeqVector/CSeqVector_CI code to allow better inlining.
* CSeqVector::operator[] made significantly faster.
* Added possibility to have platform dependent byte unpacking functions.
*
* Revision 1.42  2003/08/21 18:43:29  vasilche
* Added CSeqVector::IsProtein() and CSeqVector::IsNucleotide() methods.
*
* Revision 1.41  2003/08/21 13:32:04  vasilche
* Optimized CSeqVector iteration.
* Set some CSeqVector values (mol type, coding) in constructor instead of detecting them while iteration.
* Remove unsafe bit manipulations with coding.
*
* Revision 1.40  2003/07/17 19:10:27  grichenk
* Added methods for seq-map and seq-vector validation,
* updated demo.
*
* Revision 1.39  2003/06/24 19:46:41  grichenk
* Changed cache from vector<char> to char*. Made
* CSeqVector::operator[] inline.
*
* Revision 1.38  2003/06/13 19:40:14  grichenk
* Removed _ASSERT() from x_GetSeqMap()
*
* Revision 1.37  2003/06/13 17:22:26  grichenk
* Check if seq-map is not null before using it
*
* Revision 1.36  2003/06/12 18:38:47  vasilche
* Added default constructor of CSeqVector.
*
* Revision 1.35  2003/06/11 19:32:53  grichenk
* Added molecule type caching to CSeqMap, simplified
* coding and sequence type calculations in CSeqVector.
*
* Revision 1.34  2003/06/02 16:01:36  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.33  2003/05/27 19:44:04  grichenk
* Added CSeqVector_CI class
*
* Revision 1.32  2003/05/20 15:44:37  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.31  2003/05/05 21:00:27  vasilche
* Fix assignment of empty CSeqVector.
*
* Revision 1.30  2003/04/29 19:51:12  vasilche
* Fixed interaction of Data Loader garbage collector and TSE locking mechanism.
* Made some typedefs more consistent.
*
* Revision 1.29  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.28  2003/02/27 20:56:03  vasilche
* kTypeUnknown bit made fitting in 8 bit integer.
*
* Revision 1.27  2003/02/25 14:48:06  vasilche
* Added Win32 export modifier to object manager classes.
*
* Revision 1.26  2003/02/20 17:01:20  vasilche
* Changed value of kTypeUnknown to fit in 16 bit enums on Mac.
*
* Revision 1.25  2003/02/06 19:05:39  vasilche
* Fixed old cache data copying.
* Delayed sequence type (protein/dna) resolution.
*
* Revision 1.24  2003/01/23 19:33:57  vasilche
* Commented out obsolete methods.
* Use strand argument of CSeqVector instead of creation reversed seqmap.
* Fixed ordering operators of CBioseqHandle to be consistent.
*
* Revision 1.23  2003/01/22 20:11:53  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.22  2003/01/03 19:45:44  dicuccio
* Replaced kPosUnknwon with kInvalidSeqPos (non-static variable; work-around for
* MSVC)
*
* Revision 1.21  2002/12/26 20:51:36  dicuccio
* Added Win32 export specifier
*
* Revision 1.20  2002/12/26 16:39:22  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.19  2002/10/03 13:45:37  grichenk
* CSeqVector::size() made const
*
* Revision 1.18  2002/09/03 21:26:58  grichenk
* Replaced bool arguments in CSeqVector constructor and getters
* with enums.
*
* Revision 1.17  2002/07/08 20:50:56  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.16  2002/05/31 17:52:58  grichenk
* Optimized for better performance (CTSE_Info uses atomic counter,
* delayed annotations indexing, no location convertions in
* CAnnot_Types_CI if no references resolution is required etc.)
*
* Revision 1.15  2002/05/17 17:14:50  grichenk
* +GetSeqData() for getting a range of characters from a seq-vector
*
* Revision 1.14  2002/05/09 14:16:29  grichenk
* sm_SizeUnknown -> kPosUnknown, minor fixes for unsigned positions
*
* Revision 1.13  2002/05/06 03:30:36  vakatov
* OM/OM1 renaming
*
* Revision 1.12  2002/05/03 21:28:02  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.11  2002/05/03 18:36:13  grichenk
* Fixed members initialization
*
* Revision 1.10  2002/04/30 14:32:51  ucko
* Have size() return int in keeping with its actual behavior; should cut
* down on warnings about truncation of 64-bit integers.
*
* Revision 1.9  2002/04/29 16:23:25  grichenk
* GetSequenceView() reimplemented in CSeqVector.
* CSeqVector optimized for better performance.
*
* Revision 1.8  2002/04/25 16:37:19  grichenk
* Fixed gap coding, added GetGapChar() function
*
* Revision 1.7  2002/04/23 19:01:06  grichenk
* Added optional flag to GetSeqVector() and GetSequenceView()
* for switching to IUPAC encoding.
*
* Revision 1.6  2002/02/21 19:27:00  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.5  2002/02/15 20:36:29  gouriano
* changed implementation of HandleRangeMap
*
* Revision 1.4  2002/01/28 19:45:34  gouriano
* changed the interface of BioseqHandle: two functions moved from Scope
*
* Revision 1.3  2002/01/23 21:59:29  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.2  2002/01/16 16:26:36  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:04:04  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#endif  // SEQ_VECTOR__HPP
