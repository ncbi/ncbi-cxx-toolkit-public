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
class CBioseq_set_Handle;
class CSeq_annot_Handle;
class CSeq_entry_Handle;
class CBioseq_EditHandle;
class CBioseq_set_EditHandle;
class CSeq_annot_EditHandle;
class CSeq_entry_EditHandle;
class CBioObjectId;


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

    /// Reset handle and make it not to point to any bioseq
    void Reset(void);

    /// Get scope this handle belongs to
    CScope& GetScope(void) const;

    /// Get id used to obtain this bioseq handle
    CConstRef<CSeq_id> GetSeqId(void) const;
    
    /// Get unique object id
    const CBioObjectId& GetBioObjectId(void) const;


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
        fState_not_found     = 1 << 7,
        fState_other_error   = 1 << 8
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
    bool State_NotFound(void) const;

    /// Check if this id can be used to obtain this bioseq handle
    bool IsSynonym(const CSeq_id& id) const;
    bool IsSynonym(const CSeq_id_Handle& idh) const;

    /// Get the bioseq's synonyms
    CConstRef<CSynonymsSet> GetSynonyms(void) const;

    /// Get parent Seq-entry handle
    ///
    /// @sa 
    ///     GetSeq_entry_Handle()
    CSeq_entry_Handle GetParentEntry(void) const;

    /// Return a handle for the parent Bioseq-set, or null handle
    CBioseq_set_Handle GetParentBioseq_set(void) const;

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

    /// Unified interface for templates
    typedef CBioseq TObject;
    CConstRef<TObject> GetCompleteObject(void) const;
    CConstRef<TObject> GetObjectCore(void) const;

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

    // Check sequence type
    typedef CSeq_inst::TMol TMol;
    TMol GetSequenceType(void) const;
    bool IsProtein(void) const;
    bool IsNucleotide(void) const;

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
    TMol GetBioseqMolType(void) const;
    bool IsNa(void) const;
    bool IsAa(void) const;

    /// Get sequence map.
    const CSeqMap& GetSeqMap(void) const;

    /// Segment search flags
    enum EFindSegment {
        eFindSegment_NoLimit,   ///< No limit on resolving seq-map
        eFindSegment_LimitTSE   ///< Resolve in the parent TSE only
    };

    /// Check if the seq-id describes a segment of the bioseq
    ///
    /// @param id
    ///  Seq-id to be checked for being a segment of the handle.
    ///
    /// @param resolve_depth
    ///  Depth of resolving segments. Zero allows to check only top-level
    ///  segments.
    ///
    /// @param limit_flag
    ///  Allow/prohibit resolving far references. By default all segments are
    ///  resolved. If the flag is set to eFindSegment_LimitTSE, only near
    ///  references are checked.
    bool ContainsSegment(const CSeq_id& id,
                         size_t resolve_depth = kMax_Int,
                         EFindSegment limit_flag = eFindSegment_NoLimit) const;
    bool ContainsSegment(CSeq_id_Handle id,
                         size_t resolve_depth = kMax_Int,
                         EFindSegment limit_flag = eFindSegment_NoLimit) const;
    bool ContainsSegment(const CBioseq_Handle& part,
                         size_t resolve_depth = kMax_Int,
                         EFindSegment limit_flag = eFindSegment_NoLimit) const;

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
    /// If start == 0, stop == 0, and strand == eNa_strand_unknown,
    /// CSeq_loc will be of type 'whole'.
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

    /// Check if handle points to a bioseq and is not removed
    ///
    /// @sa
    ///    operator !()
    DECLARE_OPERATOR_BOOL(m_Info.IsValid());

    /// Check if handle points to a removed bioseq
    bool IsRemoved(void) const;

    /// Get CTSE_Handle of containing TSE
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
    friend class CSeq_entry_EditHandle;

    typedef CBioseq_ScopeInfo TScopeInfo;
    typedef CScopeInfo_Ref<TScopeInfo> TLock;

    CBioseq_Handle(const CSeq_id_Handle& id, const TScopeInfo& info);
    CBioseq_Handle(const CSeq_id_Handle& id, const TLock& lock);

    CScope_Impl& x_GetScopeImpl(void) const;
    const CBioseq_ScopeInfo& x_GetScopeInfo(void) const;

    CSeq_id_Handle  m_Handle_Seq_id;
    TLock           m_Info;

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
    /// create edit interface class to the object which already allows editing
    /// throw an exception if the argument is not in editing mode
    explicit CBioseq_EditHandle(const CBioseq_Handle& h);
    
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
    CRef<CSeqdesc> RemoveSeqdesc(const CSeqdesc& d) const;
    void AddSeq_descr(TDescr& v) const;
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
    CSeq_annot_EditHandle AttachAnnot(CSeq_annot& annot) const;

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
    enum ERemoveMode {
        eRemoveSeq_entry,
        eKeepSeq_entry
    };
    void Remove(ERemoveMode mode = eRemoveSeq_entry) const;

    /// Get CSeqMap object for sequence editing
    CSeqMap& SetSeqMap(void) const;

protected:
    friend class CScope_Impl;

    CBioseq_EditHandle(const CSeq_id_Handle& id, TScopeInfo& info);
    CBioseq_EditHandle(const CSeq_id_Handle& id, const TLock& lock);

    void x_Detach(void) const;

public: // non-public section
    CBioseq_ScopeInfo& x_GetScopeInfo(void) const;
    CBioseq_Info& x_GetInfo(void) const;

public:
    void x_RealResetDescr(void) const;
    void x_RealSetDescr(TDescr& v) const;
    bool x_RealAddSeqdesc(CSeqdesc& d) const;
    CRef<CSeqdesc> x_RealRemoveSeqdesc(const CSeqdesc& d) const;
    void x_RealAddSeq_descr(TDescr& v) const;

    void x_RealResetId(void) const;
    bool x_RealAddId(const CSeq_id_Handle& id) const;
    bool x_RealRemoveId(const CSeq_id_Handle& id) const;

    void x_RealSetInst(TInst& v) const;
    void x_RealSetInst_Repr(TInst_Repr v) const;
    void x_RealSetInst_Mol(TInst_Mol v) const;
    void x_RealSetInst_Length(TInst_Length v) const;
    void x_RealSetInst_Fuzz(TInst_Fuzz& v) const;
    void x_RealSetInst_Topology(TInst_Topology v) const;
    void x_RealSetInst_Strand(TInst_Strand v) const;
    void x_RealSetInst_Seq_data(TInst_Seq_data& v) const;
    void x_RealSetInst_Ext(TInst_Ext& v) const;
    void x_RealSetInst_Hist(TInst_Hist& v) const;
    void x_RealResetInst() const;
    void x_RealResetInst_Repr() const;
    void x_RealResetInst_Mol() const;
    void x_RealResetInst_Length() const;
    void x_RealResetInst_Fuzz() const;
    void x_RealResetInst_Topology() const;
    void x_RealResetInst_Strand() const;
    void x_RealResetInst_Seq_data() const;
    void x_RealResetInst_Ext() const;
    void x_RealResetInst_Hist() const;

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
bool CBioseq_Handle::State_NotFound(void) const
{
    return (GetState() & fState_not_found) != 0;
}


inline
const CTSE_Handle& CBioseq_Handle::GetTSE_Handle(void) const
{
    return m_Info.GetObject().GetTSE_Handle();
}


inline
bool CBioseq_Handle::IsRemoved(void) const
{
    return m_Info && m_Info.GetPointerOrNull()->IsDetached();
}


inline
const CSeq_id_Handle& CBioseq_Handle::GetSeq_id_Handle(void) const
{
    return m_Handle_Seq_id;
}


inline
CScope& CBioseq_Handle::GetScope(void) const 
{
    return GetTSE_Handle().GetScope();
}


inline
CScope_Impl& CBioseq_Handle::x_GetScopeImpl(void) const 
{
    return GetTSE_Handle().x_GetScopeImpl();
}


inline
bool CBioseq_Handle::operator==(const CBioseq_Handle& h) const
{
    // No need to check m_TSE, because m_ScopeInfo is scope specific too.
    return m_Info == h.m_Info;
}


inline
bool CBioseq_Handle::operator<(const CBioseq_Handle& h) const
{
    return m_Info < h.m_Info;
}


inline
bool CBioseq_Handle::operator!=(const CBioseq_Handle& h) const
{
    return m_Info != h.m_Info;
}


inline
CBioseq_ScopeInfo& CBioseq_EditHandle::x_GetScopeInfo(void) const
{
    return const_cast<CBioseq_ScopeInfo&>(CBioseq_Handle::x_GetScopeInfo());
}


inline
CBioseq_Info& CBioseq_EditHandle::x_GetInfo(void) const
{
    return const_cast<CBioseq_Info&>(CBioseq_Handle::x_GetInfo());
}


inline
CConstRef<CBioseq> CBioseq_Handle::GetCompleteObject(void) const
{
    return GetCompleteBioseq();
}


inline
CConstRef<CBioseq> CBioseq_Handle::GetObjectCore(void) const
{
    return GetBioseqCore();
}


inline
CBioseq_Handle::TMol CBioseq_Handle::GetSequenceType(void) const
{
    return GetInst_Mol();
}


inline
bool CBioseq_Handle::IsProtein(void) const
{
    return CSeq_inst::IsAa(GetSequenceType());
}


inline
bool CBioseq_Handle::IsNucleotide(void) const
{
    return CSeq_inst::IsNa(GetSequenceType());
}


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // BIOSEQ_HANDLE__HPP
