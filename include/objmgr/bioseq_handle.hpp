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
* Author: Aleksey Grichenko, Michael Kimelman, Eugene Vasilchenko
*
* File Description:
*
*/

#include <corelib/ncbistd.hpp>

#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqset/Bioseq_set.hpp> // for EClass
#include <objects/seq/Seq_inst.hpp> // for EMol

#include <objects/seq/seq_id_handle.hpp>
#include <objmgr/tse_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/** @addtogroup ObjectManagerHandles
 *
 * @{
 */


class CDataSource;
class CSeqMap;
class CSeqVector;
class CScope;
class CSeq_id;
class CSeq_loc;
class CBioseq_Info;
class CSeq_descr;
class CSeqdesc;
class CTSE_Info;
class CSeq_entry;
class CSeq_annot;
class CSynonymsSet;
class CBioseq_ScopeInfo;
class CSeq_id_ScopeInfo;
class CTSE_Lock;

class CBioseq_Handle;
class CSeq_annot_Handle;
class CSeq_entry_Handle;
class CBioseq_EditHandle;
class CBioseq_set_EditHandle;
class CSeq_annot_EditHandle;
class CSeq_entry_EditHandle;


/////////////////////////////////////////////////////////////////////////////
///
///  CBioseq_Handle --
///
///  Proxy to access the bioseq data
///

// Bioseq handle -- must be a copy-safe const type.
class NCBI_XOBJMGR_EXPORT CBioseq_Handle
{
public:
    // Default constructor
    CBioseq_Handle(void);

    // Assignment
    CBioseq_Handle& operator=(const CBioseq_Handle& bh);

    /// Reset handle and make it not to point to any bioseq
    void Reset(void);

    /// Get scope this handle belongs to
    CScope& GetScope(void) const;

    /// Get id used to obtain this bioseq handle
    CConstRef<CSeq_id> GetSeqId(void) const;

    /// Get handle of id used to obtain this bioseq handle
    const CSeq_id_Handle& GetSeq_id_Handle(void) const;

    /// State of bioseq handle.
    enum EBioseqStateFlags {
        fState_none          = 0,
        fState_suppress_temp = 1 << 0,
        fState_suppress_perm = 1 << 1,
        fState_suppress      = fState_suppress_temp |
                                fState_suppress_perm,
        fState_dead          = 1 << 2,
        fState_confidential  = 1 << 3,
        fState_withdrawn     = 1 << 4,
        fState_no_data       = 1 << 5, 
        fState_conflict      = 1 << 6,
        fState_conn_failed   = 1 << 7,
        fState_not_found     = 1 << 8,
        fState_other_error   = 1 << 9
    };
    typedef int TBioseqStateFlags;

    /// Get state of the bioseq. May be used with an empty bioseq handle
    /// to check why the bioseq retrieval failed.
    TBioseqStateFlags GetState(void) const;
    bool State_SuppressedTemp(void) const;
    bool State_SuppressedPerm(void) const;
    bool State_Suppressed(void) const;
    bool State_Confidential(void) const;
    bool State_Dead(void) const;
    bool State_Withdrawn(void) const;
    bool State_NoData(void) const;
    bool State_Conflict(void) const;
    bool State_ConnectionFailed(void) const;
    bool State_NotFound(void) const;

    /// Check if this id can be used to obtain this bioseq handle
    bool IsSynonym(const CSeq_id& id) const;

    /// Get the bioseq's synonyms
    CConstRef<CSynonymsSet> GetSynonyms(void) const;

    /// Get parent Seq-entry handle
    ///
    /// @sa 
    ///     GetSeq_entry_Handle()
    CSeq_entry_Handle GetParentEntry(void) const;

    /// Get parent Seq-entry handle
    ///
    /// @sa 
    ///     GetParentEntry()
    CSeq_entry_Handle GetSeq_entry_Handle(void) const;

    /// Get top level Seq-entry handle
    CSeq_entry_Handle GetTopLevelEntry(void) const;

    /// Get 'edit' version of handle
    CBioseq_EditHandle GetEditHandle(void) const;

    /// Bioseq core -- using partially populated CBioseq
    typedef CConstRef<CBioseq> TBioseqCore;
    
    /// Get bioseq core structure
    TBioseqCore GetBioseqCore(void) const;
    
    /// Get the complete bioseq
    CConstRef<CBioseq> GetCompleteBioseq(void) const;

    //////////////////////////////////////////////////////////////////
    // Bioseq members
    // id
    typedef vector<CSeq_id_Handle> TId;
    bool IsSetId(void) const;
    bool CanGetId(void) const;
    const TId& GetId(void) const;
    // descr
    typedef CSeq_descr TDescr;
    bool IsSetDescr(void) const;
    bool CanGetDescr(void) const;
    const TDescr& GetDescr(void) const;
    // inst
    typedef CSeq_inst TInst;
    bool IsSetInst(void) const;
    bool CanGetInst(void) const;
    const TInst& GetInst(void) const;
    // inst.repr
    typedef TInst::TRepr TInst_Repr;
    bool IsSetInst_Repr(void) const;
    bool CanGetInst_Repr(void) const;
    TInst_Repr GetInst_Repr(void) const;
    // inst.mol
    typedef TInst::TMol TInst_Mol;
    bool IsSetInst_Mol(void) const;
    bool CanGetInst_Mol(void) const;
    TInst_Mol GetInst_Mol(void) const;
    // inst.length
    typedef TInst::TLength TInst_Length;
    bool IsSetInst_Length(void) const;
    bool CanGetInst_Length(void) const;
    TInst_Length GetInst_Length(void) const;
    TSeqPos GetBioseqLength(void) const; // try to calculate it if not set
    // inst.fuzz
    typedef TInst::TFuzz TInst_Fuzz;
    bool IsSetInst_Fuzz(void) const;
    bool CanGetInst_Fuzz(void) const;
    const TInst_Fuzz& GetInst_Fuzz(void) const;
    // inst.topology
    typedef TInst::TTopology TInst_Topology;
    bool IsSetInst_Topology(void) const;
    bool CanGetInst_Topology(void) const;
    TInst_Topology GetInst_Topology(void) const;
    // inst.strand
    typedef TInst::TStrand TInst_Strand;
    bool IsSetInst_Strand(void) const;
    bool CanGetInst_Strand(void) const;
    TInst_Strand GetInst_Strand(void) const;
    // inst.seq-data
    typedef TInst::TSeq_data TInst_Seq_data;
    bool IsSetInst_Seq_data(void) const;
    bool CanGetInst_Seq_data(void) const;
    const TInst_Seq_data& GetInst_Seq_data(void) const;
    // inst.ext
    typedef TInst::TExt TInst_Ext;
    bool IsSetInst_Ext(void) const;
    bool CanGetInst_Ext(void) const;
    const TInst_Ext& GetInst_Ext(void) const;
    // inst.hist
    typedef TInst::THist TInst_Hist;
    bool IsSetInst_Hist(void) const;
    bool CanGetInst_Hist(void) const;
    const TInst_Hist& GetInst_Hist(void) const;
    // annot
    bool HasAnnots(void) const;

    bool IsNa(void) const;
    bool IsAa(void) const;
    //////////////////////////////////////////////////////////////////
    // Old interface:

    /// Go up to a certain complexity level (or the nearest level of the same
    /// priority if the required class is not found):
    /// level   class
    /// 0       not-set (0) ,
    /// 3       nuc-prot (1) ,       -- nuc acid and coded proteins
    /// 2       segset (2) ,         -- segmented sequence + parts
    /// 2       conset (3) ,         -- constructed sequence + parts
    /// 1       parts (4) ,          -- parts for 2 or 3
    /// 1       gibb (5) ,           -- geninfo backbone
    /// 1       gi (6) ,             -- geninfo
    /// 5       genbank (7) ,        -- converted genbank
    /// 3       pir (8) ,            -- converted pir
    /// 4       pub-set (9) ,        -- all the seqs from a single publication
    /// 4       equiv (10) ,         -- a set of equivalent maps or seqs
    /// 3       swissprot (11) ,     -- converted SWISSPROT
    /// 3       pdb-entry (12) ,     -- a complete PDB entry
    /// 4       mut-set (13) ,       -- set of mutations
    /// 4       pop-set (14) ,       -- population study
    /// 4       phy-set (15) ,       -- phylogenetic study
    /// 4       eco-set (16) ,       -- ecological sample study
    /// 4       gen-prod-set (17) ,  -- genomic products, chrom+mRNa+protein
    /// 4       wgs-set (18) ,       -- whole genome shotgun project
    /// 0       other (255)
    CSeq_entry_Handle GetComplexityLevel(CBioseq_set::EClass cls) const;
    
    /// Return level with exact complexity, or empty handle if not found.
    CSeq_entry_Handle GetExactComplexityLevel(CBioseq_set::EClass cls) const;

    /// Get some values from core:
    CSeq_inst::TMol GetBioseqMolType(void) const;

    /// Get sequence map.
    const CSeqMap& GetSeqMap(void) const;

    /// CSeqVector constructor flags
    enum EVectorCoding {
        eCoding_NotSet, ///< Use original coding - DANGEROUS! - may change
        eCoding_Ncbi,   ///< Set coding to binary coding (Ncbi4na or Ncbistdaa)
        eCoding_Iupac   ///< Set coding to printable coding (Iupacna or Iupacaa)
    };
    enum EVectorStrand {
        eStrand_Plus,   ///< Plus strand
        eStrand_Minus   ///< Minus strand
    };

    /// Get sequence: Iupacna or Iupacaa if use_iupac_coding is true
    CSeqVector GetSeqVector(EVectorCoding coding,
                            ENa_strand strand = eNa_strand_plus) const;
    /// Get sequence
    CSeqVector GetSeqVector(ENa_strand strand = eNa_strand_plus) const;
    /// Get sequence: Iupacna or Iupacaa if use_iupac_coding is true
    CSeqVector GetSeqVector(EVectorCoding coding, EVectorStrand strand) const;
    /// Get sequence
    CSeqVector GetSeqVector(EVectorStrand strand) const;

    /// Return CSeq_loc referencing the given range and strand on the bioseq
    CRef<CSeq_loc> GetRangeSeq_loc(TSeqPos start,
                                   TSeqPos stop,
                                   ENa_strand strand
                                   = eNa_strand_unknown) const;

    /// Map a seq-loc from the bioseq's segment to the bioseq
    CRef<CSeq_loc> MapLocation(const CSeq_loc& loc) const;

    // Utility methods/operators

    /// Check if handles point to the same bioseq
    ///
    /// @sa
    ///     operator!=()
    bool operator== (const CBioseq_Handle& h) const;

    // Check if handles point to different bioseqs
    ///
    /// @sa
    ///     operator==()
    bool operator!= (const CBioseq_Handle& h) const;

    /// For usage in containers
    bool operator<  (const CBioseq_Handle& h) const;

    /// Check if handle points to a bioseq
    ///
    /// @sa
    ///    operator !()
    DECLARE_OPERATOR_BOOL_REF(m_Info);

    // Get CTSE_Handle of containing TSE
    const CTSE_Handle& GetTSE_Handle(void) const;


    // these methods are for cross scope move only.
    /// Copy current bioseq into seq-entry
    /// 
    /// @param entry
    ///  Current bioseq will be inserted into seq-entry pointed 
    ///  by this handle. 
    //   If seq-entry is not seqset exception will be thrown
    /// @param index
    ///  Start index is 0 and -1 means end
    ///
    /// @return
    ///  Edit handle to inserted bioseq
    CBioseq_EditHandle CopyTo(const CSeq_entry_EditHandle& entry,
                              int index = -1) const;

    /// Copy current bioseq into seqset
    /// 
    /// @param entry
    ///  Current bioseq will be inserted into seqset pointed 
    ///  by this handle. 
    /// @param index
    ///  Start index is 0 and -1 means end
    ///
    /// @return
    ///  Edit handle to inserted bioseq
    CBioseq_EditHandle CopyTo(const CBioseq_set_EditHandle& seqset,
                              int index = -1) const;

    /// Copy current bioseq into seq-entry and set seq-entry as bioseq
    /// 
    /// @param entry
    ///  Seq-entry pointed by entry handle will be set to bioseq
    ///
    /// @return
    ///  Edit handle to inserted bioseq
    CBioseq_EditHandle CopyToSeq(const CSeq_entry_EditHandle& entry) const;

    /// Register argument bioseq as used by this bioseq, so it will be
    /// released by scope only after this bioseq is released.
    ///
    /// @param bh
    ///  Used bioseq handle
    ///
    /// @return
    ///  True if argument bioseq was successfully registered as 'used'.
    ///  False if argument bioseq was not registered as 'used'.
    ///  Possible reasons:
    ///   Circular reference in 'used' tree.
    bool AddUsedBioseq(const CBioseq_Handle& bh) const;

protected:
    friend class CScope_Impl;
    friend class CSynonymsSet;

    //CBioseq_Handle(const CSeq_id_Handle& id, CBioseq_ScopeInfo* bioseq_info);
    CBioseq_Handle(const CSeq_id_Handle& id,
                   const CBioseq_ScopeInfo& binfo,
                   const CTSE_Handle& tse);

    // Create empty bioseq handle from empty info with error status
    CBioseq_Handle(const CSeq_id_Handle&    id,
                   const CBioseq_ScopeInfo& binfo);

    CScope_Impl& x_GetScopeImpl(void) const;
    const CBioseq_ScopeInfo& x_GetScopeInfo(void) const;

    CTSE_Handle         m_TSE;
    CSeq_id_Handle      m_Seq_id;
    CConstRef<CObject>  m_ScopeInfo;
    CConstRef<CObject>  m_Info;

public: // non-public section
    const CBioseq_Info& x_GetInfo(void) const;
};


/////////////////////////////////////////////////////////////////////////////
///
///  CBioseq_EditHandle --
///
///  Proxy to access and edit the bioseq data
///

class NCBI_XOBJMGR_EXPORT CBioseq_EditHandle : public CBioseq_Handle
{
public:
    CBioseq_EditHandle(void);
    
    /// Navigate object tree
    CSeq_entry_EditHandle GetParentEntry(void) const;

    // Modification functions

    //////////////////////////////////////////////////////////////////
    // Bioseq members
    // id
    void ResetId(void) const;
    bool AddId(const CSeq_id_Handle& id) const;
    bool RemoveId(const CSeq_id_Handle& id) const;
    // descr
    void ResetDescr(void) const;
    void SetDescr(TDescr& v) const;
    TDescr& SetDescr(void) const;
    bool AddSeqdesc(CSeqdesc& d) const;
    bool RemoveSeqdesc(const CSeqdesc& d) const;
    void AddSeq_descr(const TDescr& v) const;
    // inst
    void SetInst(TInst& v) const;
    // inst.repr
    void SetInst_Repr(TInst_Repr v) const;
    // inst.mol
    void SetInst_Mol(TInst_Mol v) const;
    // inst.length
    void SetInst_Length(TInst_Length v) const;
    // inst.fuzz
    void SetInst_Fuzz(TInst_Fuzz& v) const;
    // inst.topology
    void SetInst_Topology(TInst_Topology v) const;
    // inst.strand
    void SetInst_Strand(TInst_Strand v) const;
    // inst.seq-data
    void SetInst_Seq_data(TInst_Seq_data& v) const;
    // inst.ext
    void SetInst_Ext(TInst_Ext& v) const;
    // inst.hist
    void SetInst_Hist(TInst_Hist& v) const;
    // annot
    //////////////////////////////////////////////////////////////////


    /// Attach an annotation
    ///
    /// @param annot
    ///  Reference to this annotation will be attached
    ///
    /// @return
    ///  Edit handle to the attached annotation
    ///
    /// @sa
    ///  CopyAnnot()
    ///  TakeAnnot()
    CSeq_annot_EditHandle AttachAnnot(const CSeq_annot& annot) const;

    /// Attach a copy of the annotation
    ///
    /// @param annot
    ///  Copy of the annotation pointed by this handle will be attached
    ///
    /// @return
    ///  Edit handle to the attached annotation
    ///
    /// @sa
    ///  AttachAnnot()
    ///  TakeAnnot()
    CSeq_annot_EditHandle CopyAnnot(const CSeq_annot_Handle& annot) const;

    /// Remove the annotation from its location and attach to current one
    ///
    /// @param annot
    ///  An annotation  pointed by this handle will be removed and attached
    ///
    /// @return
    ///  Edit handle to the attached annotation
    ///
    /// @sa
    ///  AttachAnnot()
    ///  CopyAnnot()
    CSeq_annot_EditHandle TakeAnnot(const CSeq_annot_EditHandle& annot) const;

    // Tree modification, target handle must be in the same TSE
    // entry.Which() must be e_not_set or e_Set.

    /// Move current bioseq into seq-entry
    /// 
    /// @param entry
    ///  Current bioseq will be inserted into seq-entry pointed 
    ///  by this handle. 
    //   If seq-entry is not seqset exception will be thrown
    /// @param index
    ///  Start index is 0 and -1 means end
    ///
    /// @return
    ///  Edit handle to inserted bioseq
    CBioseq_EditHandle MoveTo(const CSeq_entry_EditHandle& entry,
                              int index = -1) const;

    /// Move current bioseq into seqset
    /// 
    /// @param entry
    ///  Current bioseq will be inserted into seqset pointed 
    ///  by this handle. 
    /// @param index
    ///  Start index is 0 and -1 means end
    ///
    /// @return
    ///  Edit handle to inserted bioseq
    CBioseq_EditHandle MoveTo(const CBioseq_set_EditHandle& seqset,
                              int index = -1) const;

    /// Move current bioseq into seq-entry and set seq-entry as bioseq
    /// 
    /// @param entry
    ///  seq-entry pointed by entry handle will be set to bioseq
    ///
    /// @return
    ///  Edit handle to inserted bioseq
    CBioseq_EditHandle MoveToSeq(const CSeq_entry_EditHandle& entry) const;

    /// Remove current bioseq from its location
    void Remove(void) const;

protected:
    friend class CScope_Impl;

    CBioseq_EditHandle(const CBioseq_Handle& h);
    CBioseq_EditHandle(const CSeq_id_Handle& id,
                       CBioseq_ScopeInfo& binfo,
                       const CTSE_Handle& tse);

public: // non-public section
    CBioseq_Info& x_GetInfo(void) const;
};


/////////////////////////////////////////////////////////////////////////////
// CBioseq_Handle inline methods
/////////////////////////////////////////////////////////////////////////////


inline
CBioseq_Handle::CBioseq_Handle(void)
{
}


inline
bool CBioseq_Handle::State_SuppressedTemp(void) const
{
    return (GetState() & fState_suppress_temp) != 0;
}


inline
bool CBioseq_Handle::State_SuppressedPerm(void) const
{
    return (GetState() & fState_suppress_perm) != 0;
}


inline
bool CBioseq_Handle::State_Suppressed(void) const
{
    return (GetState() & fState_suppress) != 0;
}


inline
bool CBioseq_Handle::State_Confidential(void) const
{
    return (GetState() & fState_confidential) != 0;
}


inline
bool CBioseq_Handle::State_Dead(void) const
{
    return (GetState() & fState_dead) != 0;
}


inline
bool CBioseq_Handle::State_Withdrawn(void) const
{
    return (GetState() & fState_withdrawn) != 0;
}


inline
bool CBioseq_Handle::State_NoData(void) const
{
    return (GetState() & fState_no_data) != 0;
}


inline
bool CBioseq_Handle::State_Conflict(void) const
{
    return (GetState() & fState_conflict) != 0;
}


inline
bool CBioseq_Handle::State_ConnectionFailed(void) const
{
    return (GetState() & fState_conn_failed) != 0;
}


inline
bool CBioseq_Handle::State_NotFound(void) const
{
    return (GetState() & fState_not_found) != 0;
}


inline
const CTSE_Handle& CBioseq_Handle::GetTSE_Handle(void) const
{
    return m_TSE;
}


inline
const CSeq_id_Handle& CBioseq_Handle::GetSeq_id_Handle(void) const
{
    return m_Seq_id;
}


inline
CScope& CBioseq_Handle::GetScope(void) const 
{
    return m_TSE.GetScope();
}


inline
CScope_Impl& CBioseq_Handle::x_GetScopeImpl(void) const 
{
    return m_TSE.x_GetScopeImpl();
}


inline
bool CBioseq_Handle::operator!= (const CBioseq_Handle& h) const
{
    return !(*this == h);
}


inline
CBioseq_Info& CBioseq_EditHandle::x_GetInfo(void) const
{
    return const_cast<CBioseq_Info&>(CBioseq_Handle::x_GetInfo());
}


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.74  2005/02/09 19:11:07  vasilche
* Implemented setters for Bioseq.descr.
*
* Revision 1.73  2005/02/01 21:43:35  grichenk
* Added comments
*
* Revision 1.72  2005/01/26 16:25:21  grichenk
* Added state flags to CBioseq_Handle.
*
* Revision 1.71  2005/01/24 17:09:36  vasilche
* Safe boolean operators.
*
* Revision 1.70  2005/01/12 17:16:13  vasilche
* Avoid performance warning on MSVC.
*
* Revision 1.69  2005/01/06 16:41:30  grichenk
* Removed deprecated methods
*
* Revision 1.68  2005/01/04 16:31:07  grichenk
* Added IsNa(), IsAa()
*
* Revision 1.67  2004/12/22 15:56:05  vasilche
* Introduced CTSE_Handle.
* Added auto-release of unused TSEs in scope.
*
* Revision 1.66  2004/12/13 15:19:20  grichenk
* Doxygenized comments
*
* Revision 1.65  2004/12/06 17:11:25  grichenk
* Marked GetSequenceView and GetSeqMapFromSeqLoc as deprecated
*
* Revision 1.64  2004/11/22 16:04:06  grichenk
* Fixed/added doxygen comments
*
* Revision 1.63  2004/11/01 19:31:56  grichenk
* Added GetRangeSeq_loc()
*
* Revision 1.62  2004/10/29 16:29:47  grichenk
* Prepared to remove deprecated methods, added new constructors.
*
* Revision 1.61  2004/09/28 19:36:29  kononenk
* Updated doxygen documentation
*
* Revision 1.60  2004/09/27 20:14:21  kononenk
* Added doxygen formatting
*
* Revision 1.59  2004/08/05 18:28:17  vasilche
* Fixed order of CRef<> release in destruction and assignment of handles.
*
* Revision 1.58  2004/08/04 14:53:25  vasilche
* Revamped object manager:
* 1. Changed TSE locking scheme
* 2. TSE cache is maintained by CDataSource.
* 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
* 4. Fixed processing of split data.
*
* Revision 1.57  2004/07/12 15:05:31  grichenk
* Moved seq-id mapper from xobjmgr to seq library
*
* Revision 1.56  2004/05/06 17:32:37  grichenk
* Added CanGetXXXX() methods
*
* Revision 1.55  2004/03/31 19:23:13  vasilche
* Fixed scope in CBioseq_Handle::GetEditHandle().
*
* Revision 1.54  2004/03/31 17:08:06  vasilche
* Implemented ConvertSeqToSet and ConvertSetToSeq.
*
* Revision 1.53  2004/03/29 20:13:05  vasilche
* Implemented whole set of methods to modify Seq-entry object tree.
* Added CBioseq_Handle::GetExactComplexityLevel().
*
* Revision 1.52  2004/03/25 19:27:43  vasilche
* Implemented MoveTo and CopyTo methods of handles.
*
* Revision 1.51  2004/03/24 18:30:28  vasilche
* Fixed edit API.
* Every *_Info object has its own shallow copy of original object.
*
* Revision 1.50  2004/03/16 21:01:32  vasilche
* Added methods to move Bioseq withing Seq-entry
*
* Revision 1.49  2004/03/16 15:47:26  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.48  2004/02/06 16:07:26  grichenk
* Added CBioseq_Handle::GetSeq_entry_Handle()
* Fixed MapLocation()
*
* Revision 1.47  2003/11/28 15:13:24  grichenk
* Added CSeq_entry_Handle
*
* Revision 1.46  2003/11/17 16:03:12  grichenk
* Throw exception in CBioseq_Handle if the parent scope has been reset
*
* Revision 1.45  2003/11/10 18:12:43  grichenk
* Added MapLocation()
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
* Revision 1.43  2003/09/05 17:29:39  grichenk
* Structurized Object Manager exceptions
*
* Revision 1.42  2003/08/27 14:28:50  vasilche
* Reduce amount of object allocations in feature iteration.
*
* Revision 1.41  2003/07/15 16:14:06  grichenk
* CBioseqHandle::IsSynonym() made public
*
* Revision 1.40  2003/06/19 18:23:44  vasilche
* Added several CXxx_ScopeInfo classes for CScope related information.
* CBioseq_Handle now uses reference to CBioseq_ScopeInfo.
* Some fine tuning of locking in CScope.
*
* Revision 1.38  2003/05/20 15:44:36  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.37  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.36  2003/03/27 19:39:34  grichenk
* +CBioseq_Handle::GetComplexityLevel()
*
* Revision 1.35  2003/03/21 19:22:48  grichenk
* Redesigned TSE locking, replaced CTSE_Lock with CRef<CTSE_Info>.
*
* Revision 1.34  2003/03/18 21:48:27  grichenk
* Removed obsolete class CAnnot_CI
*
* Revision 1.33  2003/03/18 14:46:35  grichenk
* Set limit object type to "none" if the object is null.
*
* Revision 1.32  2003/03/14 19:10:33  grichenk
* + SAnnotSelector::EIdResolving; fixed operator=() for several classes
*
* Revision 1.31  2003/03/12 20:09:30  grichenk
* Redistributed members between CBioseq_Handle, CBioseq_Info and CTSE_Info
*
* Revision 1.30  2003/02/27 14:35:32  vasilche
* Splitted PopulateTSESet() by logically independent parts.
*
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
