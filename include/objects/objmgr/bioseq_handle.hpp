#ifndef BIOSEQ_HANDLE__HPP
#define BIOSEQ_HANDLE__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman
*
* File Description:
*
*/

#include <objects/objmgr/seq_id_handle.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seq/Bioseq.hpp>
#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataSource;
class CSeqMap;
class CTSE_Info;
class CSeqVector;
class CScope;


// Bioseq handle -- must be a copy-safe const type.
class NCBI_XOBJMGR_EXPORT CBioseq_Handle
{
public:
    // Destructor
    virtual ~CBioseq_Handle(void);

    // Bioseq core -- using partially populated CBioseq
    typedef CConstRef<CBioseq> TBioseqCore;


    CBioseq_Handle(void)
        : m_Scope(0),
          m_DataSource(0),
          m_Entry(0),
          m_TSE(0) {}
    CBioseq_Handle(const CBioseq_Handle& h);
    CBioseq_Handle& operator= (const CBioseq_Handle& h);

    // Comparison
    bool operator== (const CBioseq_Handle& h) const;
    bool operator!= (const CBioseq_Handle& h) const;
    bool operator<  (const CBioseq_Handle& h) const;

    // Check
    operator bool(void)  const { return ( m_DataSource  &&  m_Value); }
    bool operator!(void) const { return (!m_DataSource  || !m_Value); }

    // Get the complete bioseq (as loaded by now)
    // Declared "virtual" to avoid circular dependencies with seqloc
    virtual const CBioseq& GetBioseq(void) const;

    const CSeq_id* GetSeqId(void)  const;

    // Get top level seq-entry for a bioseq
    virtual const CSeq_entry& GetTopLevelSeqEntry(void) const;

    // Get bioseq core structure
    // Declared "virtual" to avoid circular dependencies with seqloc
    virtual TBioseqCore GetBioseqCore(void) const;

    // Get sequence map.
    virtual const CSeqMap& GetSeqMap(void) const;

    // Get sequence map with resolved multi-level references.
    // The resulting map should contain regions of literals, gaps
    // and references to literals only (but not gaps or refs) of other
    // sequences.
    // WARNING: the returned object should be stored as a CRef<> or
    // deleted manually. Do not assign the returned value to a simple
    // reference.
    //virtual const CSeqMap& CreateResolvedSeqMap(void) const;

    // CSeqVector constructor flags
    enum EVectorCoding {
        eCoding_NotSet, // Use original coding - DANGEROUS! - may change
        eCoding_Ncbi,   // Set coding to binary coding (Ncbi4na or Ncbistdaa)
        eCoding_Iupac   // Set coding to printable coding (Iupacna or Iupacaa)
    };
    enum EVectorStrand {
        eStrand_Plus,   // Plus strand
        eStrand_Minus   // Minus strand
    };

    // Get sequence: Iupacna or Iupacaa if use_iupac_coding is true
    CSeqVector GetSeqVector(EVectorCoding coding,
                            ENa_strand strand = eNa_strand_plus) const;
    CSeqVector GetSeqVector(ENa_strand strand = eNa_strand_plus) const;
    CSeqVector GetSeqVector(EVectorCoding coding, EVectorStrand strand) const;
    CSeqVector GetSeqVector(EVectorStrand strand) const;
    // NOTE: the following function is deprecated, use enum arguments instead.
    //CSeqVector GetSeqVector(bool use_iupac_coding, bool plus_strand) const;

    // Sequence filtering: get a seq-vector for a part of the sequence.
    // The part shown depends oon the mode selected. If the location
    // contains references to other sequences they are ignored (unlike
    // CBioseq constructor, which constructs a bioseq using all references
    // from a location). Strand information from "location" is ingored
    // when creating merged or excluded views. If "minus_strand" is true,
    // the result is reverse-complement.
    enum ESequenceViewMode {
        eViewConstructed,    // Do not merge or reorder intervals
        eViewMerged,         // Merge overlapping intervals, sort by location
        eViewExcluded        // Show intervals not included in the seq-loc
    };
    CSeqVector GetSequenceView(const CSeq_loc& location,
                               ESequenceViewMode mode,
                               EVectorCoding coding = eCoding_Ncbi,
                               ENa_strand strand = eNa_strand_plus) const;


    //CConstRef<CSeqMap> CreateSeqMapForStrand(CConstRef<CSeqMap> seqMap,
    //                                         ENa_strand strand) const;
    //CConstRef<CSeqMap> GetSeqMapByStrand(ENa_strand strand) const;
    CConstRef<CSeqMap> GetSeqMapByLocation(const CSeq_loc& location,
                                           ESequenceViewMode mode) const;

    CScope& GetScope(void) const;

    // Modification functions
    // Add/remove/replace annotations:
    void AddAnnot(CSeq_annot& annot);
    void RemoveAnnot(const CSeq_annot& annot);
    void ReplaceAnnot(const CSeq_annot& old_annot, CSeq_annot& new_annot);

private:
    CBioseq_Handle(const CSeq_id_Handle& value);

    // Get the internal seq-id handle
    const CSeq_id_Handle&  GetKey(void) const;

    // Get data source
    CDataSource& x_GetDataSource(void) const;
    // Set the handle seq-entry and datasource
    void x_ResolveTo(CScope& scope, CDataSource& datasource,
                     CSeq_entry& entry, CTSE_Info& tse);

    bool x_IsSynonym(const CSeq_id& id) const;

    CSeq_id_Handle       m_Value;       // Seq-id equivalent
    CScope*              m_Scope;
    mutable CDataSource* m_DataSource;  // Data source for resolved handles
    mutable CSeq_entry*  m_Entry;       // Seq-entry, containing the bioseq

    // Top-level seq-entry is declared as an atomic counter to use inline locks
    mutable CMutableAtomicCounter* m_TSE;         // Top level seq-entry

    friend class CSeqVector;
    friend class CHandleRangeMap;
    friend class CDataSource;
    friend class CAnnot_CI;
    friend class CAnnotTypes_CI;
};


inline
CBioseq_Handle::CBioseq_Handle(const CSeq_id_Handle& value)
    : m_Value(value),
      m_Scope(0),
      m_DataSource(0),
      m_Entry(0),
      m_TSE(0)
{
}

inline
CBioseq_Handle::CBioseq_Handle(const CBioseq_Handle& h)
    : m_Value(h.m_Value),
      m_Scope(h.m_Scope),
      m_DataSource(h.m_DataSource),
      m_Entry(h.m_Entry),
      m_TSE(h.m_TSE)
{
    if ( m_TSE )
        m_TSE->Add(1);
}

inline
CBioseq_Handle& CBioseq_Handle::operator= (const CBioseq_Handle& h)
{
    m_Value = h.m_Value;
    m_Scope = h.m_Scope;
    m_DataSource = h.m_DataSource;
    m_Entry = h.m_Entry;
    if ( m_TSE )
        m_TSE->Add(-1);
    m_TSE = h.m_TSE;
    if ( m_TSE )
        m_TSE->Add(1);
    return *this;
}

inline
const CSeq_id_Handle& CBioseq_Handle::GetKey(void) const
{
    return m_Value;
}

inline
bool CBioseq_Handle::operator== (const CBioseq_Handle& h) const
{
    if (m_Scope != h.m_Scope) {
        THROW1_TRACE(runtime_error,
            "CBioseq_Handle::operator==() -- "
            "Unable to compare handles from different scopes");
    }
    if ( m_Entry  &&  h.m_Entry )
        return m_Entry == h.m_Entry;
    // Compare by id key
    return m_Value == h.m_Value;
}

inline
bool CBioseq_Handle::operator!= (const CBioseq_Handle& h) const
{
    return !(*this == h);
}

inline
bool CBioseq_Handle::operator< (const CBioseq_Handle& h) const
{
    if (m_Scope != h.m_Scope) {
        THROW1_TRACE(runtime_error,
            "CBioseq_Handle::operator<() -- "
            "Unable to compare CBioseq_Handles from different scopes");
    }
    if ( m_Entry  &&  h.m_Entry )
	return m_Entry < h.m_Entry;
    return m_Value < h.m_Value;
}

inline
CDataSource& CBioseq_Handle::x_GetDataSource(void) const
{
    if ( !m_DataSource ) {
        THROW1_TRACE(runtime_error,
            "CBioseq_Handle::x_GetDataSource() -- "
            "Can not resolve data source for bioseq handle.");
    }
    return *m_DataSource;
}

inline
CScope& CBioseq_Handle::GetScope(void) const 
{
    return *m_Scope;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.29  2003/01/23 19:33:57  vasilche
* Commented out obsolete methods.
* Use strand argument of CSeqVector instead of creation reversed seqmap.
* Fixed ordering operators of CBioseqHandle to be consistent.
*
* Revision 1.28  2003/01/22 20:11:53  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.27  2002/12/26 20:51:35  dicuccio
* Added Win32 export specifier
*
* Revision 1.26  2002/12/26 16:39:21  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.25  2002/11/08 22:15:50  grichenk
* Added methods for removing/replacing annotations
*
* Revision 1.24  2002/09/03 21:26:58  grichenk
* Replaced bool arguments in CSeqVector constructor and getters
* with enums.
*
* Revision 1.23  2002/08/23 16:49:06  grichenk
* Added warning about using CreateResolvedSeqMap()
*
* Revision 1.22  2002/07/08 20:50:56  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.21  2002/06/12 14:39:00  grichenk
* Renamed enumerators
*
* Revision 1.20  2002/06/06 19:36:02  clausen
* Added GetTitle()
*
* Revision 1.19  2002/05/31 17:52:58  grichenk
* Optimized for better performance (CTSE_Info uses atomic counter,
* delayed annotations indexing, no location convertions in
* CAnnot_Types_CI if no references resolution is required etc.)
*
* Revision 1.18  2002/05/21 18:39:27  grichenk
* CBioseq_Handle::GetResolvedSeqMap() -> CreateResolvedSeqMap()
*
* Revision 1.17  2002/05/06 03:30:35  vakatov
* OM/OM1 renaming
*
* Revision 1.16  2002/05/03 18:35:36  grichenk
* throw -> THROW1_TRACE
*
* Revision 1.15  2002/04/23 19:01:06  grichenk
* Added optional flag to GetSeqVector() and GetSequenceView()
* for switching to IUPAC encoding.
*
* Revision 1.14  2002/04/22 20:06:58  grichenk
* +GetSequenceView(), +x_IsSynonym()
*
* Revision 1.13  2002/04/18 20:35:10  gouriano
* correction in comment
*
* Revision 1.12  2002/04/11 12:07:28  grichenk
* Redesigned CAnnotTypes_CI to resolve segmented sequences correctly.
*
* Revision 1.11  2002/03/19 19:17:33  gouriano
* added const qualifier to GetTitle and GetSeqVector
*
* Revision 1.10  2002/03/15 18:10:04  grichenk
* Removed CRef<CSeq_id> from CSeq_id_Handle, added
* key to seq-id map th CSeq_id_Mapper
*
* Revision 1.9  2002/03/04 15:08:43  grichenk
* Improved CTSE_Info locks
*
* Revision 1.8  2002/02/21 19:27:00  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.7  2002/02/07 21:27:33  grichenk
* Redesigned CDataSource indexing: seq-id handle -> TSE -> seq/annot
*
* Revision 1.6  2002/02/01 21:49:10  gouriano
* minor changes to make it compilable and run on Solaris Workshop
*
* Revision 1.5  2002/01/29 17:05:53  grichenk
* GetHandle() -> GetKey()
*
* Revision 1.4  2002/01/28 19:45:33  gouriano
* changed the interface of BioseqHandle: two functions moved from Scope
*
* Revision 1.3  2002/01/23 21:59:29  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.2  2002/01/16 16:26:36  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:04:00  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#endif  // BIOSEQ_HANDLE__HPP
