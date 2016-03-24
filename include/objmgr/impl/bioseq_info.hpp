#ifndef OBJECTS_OBJMGR_IMPL___BIOSEQ_INFO__HPP
#define OBJECTS_OBJMGR_IMPL___BIOSEQ_INFO__HPP

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
 * Author: Aleksey Grichenko, Eugene Vasilchenko
 *
 * File Description:
 *   Bioseq info for data source
 *
 */

#include <objmgr/impl/bioseq_base_info.hpp>
#include <corelib/ncbimtx.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_hist.hpp>

#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CSeq_entry_Info;
class CBioseq;
class CSeq_id_Handle;
class CSeqMap;
class CTSE_Info;
class CDataSource;
class CSeq_inst;
class CSeq_id;
class CPacked_seqint;
class CSeq_loc;
class CSeq_loc_mix;
class CSeq_loc_equiv;
class CSeg_ext;
class CDelta_ext;
class CDelta_seq;
class CScope_Impl;

////////////////////////////////////////////////////////////////////
//
//  CBioseq_Info::
//
//    Structure to keep bioseq's parent seq-entry along with the list
//    of seq-id synonyms for the bioseq.
//


class NCBI_XOBJMGR_EXPORT CBioseq_Info : public CBioseq_Base_Info
{
    typedef CBioseq_Base_Info TParent;
public:
    // 'ctors
    explicit CBioseq_Info(const CBioseq_Info& src, TObjectCopyMap* copy_map);
    explicit CBioseq_Info(CBioseq& seq);
    virtual ~CBioseq_Info(void);

    typedef CBioseq TObject;

    CConstRef<TObject> GetBioseqCore(void) const;
    CConstRef<TObject> GetCompleteBioseq(void) const;

    // Bioseq members
    // id
    typedef vector<CSeq_id_Handle> TId;
    bool IsSetId(void) const;
    bool CanGetId(void) const;
    const TId& GetId(void) const;
    void ResetId(void);
    bool HasId(const CSeq_id_Handle& id) const;
    bool AddId(const CSeq_id_Handle& id);
    bool RemoveId(const CSeq_id_Handle& id);
    string IdString(void) const;

    bool x_IsSetDescr(void) const;
    bool x_CanGetDescr(void) const;
    const TDescr& x_GetDescr(void) const;
    TDescr& x_SetDescr(void);
    void x_SetDescr(TDescr& v);
    void x_ResetDescr(void);

    // inst
    typedef TObject::TInst TInst;
    bool IsSetInst(void) const;
    bool CanGetInst(void) const;
    const TInst& GetInst(void) const;
    void SetInst(TInst& v);
    void ResetInst();

    // inst.repr
    typedef TInst::TRepr TInst_Repr;
    bool IsSetInst_Repr(void) const;
    bool CanGetInst_Repr(void) const;
    TInst_Repr GetInst_Repr(void) const;
    void SetInst_Repr(TInst_Repr v);
    void ResetInst_Repr();

    // inst.mol
    typedef TInst::TMol TInst_Mol;
    bool IsSetInst_Mol(void) const;
    bool CanGetInst_Mol(void) const;
    TInst_Mol GetInst_Mol(void) const;
    void SetInst_Mol(TInst_Mol v);
    void ResetInst_Mol();

    // inst.length
    typedef TInst::TLength TInst_Length;
    bool IsSetInst_Length(void) const;
    bool CanGetInst_Length(void) const;
    TInst_Length GetInst_Length(void) const;
    void SetInst_Length(TInst_Length v);
    TSeqPos GetBioseqLength(void) const; // try to calculate it if not set
    void ResetInst_Length();

    // inst.fuzz
    typedef TInst::TFuzz TInst_Fuzz;
    bool IsSetInst_Fuzz(void) const;
    bool CanGetInst_Fuzz(void) const;
    const TInst_Fuzz& GetInst_Fuzz(void) const;
    void SetInst_Fuzz(TInst_Fuzz& v);
    void ResetInst_Fuzz();

    // inst.topology
    typedef TInst::TTopology TInst_Topology;
    bool IsSetInst_Topology(void) const;
    bool CanGetInst_Topology(void) const;
    TInst_Topology GetInst_Topology(void) const;
    void SetInst_Topology(TInst_Topology v);
    void ResetInst_Topology();

    // inst.strand
    typedef TInst::TStrand TInst_Strand;
    bool IsSetInst_Strand(void) const;
    bool CanGetInst_Strand(void) const;
    TInst_Strand GetInst_Strand(void) const;
    void SetInst_Strand(TInst_Strand v);
    void ResetInst_Strand();

    // inst.seq-data
    typedef TInst::TSeq_data TInst_Seq_data;
    bool IsSetInst_Seq_data(void) const;
    bool CanGetInst_Seq_data(void) const;
    const TInst_Seq_data& GetInst_Seq_data(void) const;
    void SetInst_Seq_data(TInst_Seq_data& v);
    void ResetInst_Seq_data();

    // inst.ext
    typedef TInst::TExt TInst_Ext;
    bool IsSetInst_Ext(void) const;
    bool CanGetInst_Ext(void) const;
    const TInst_Ext& GetInst_Ext(void) const;
    void SetInst_Ext(TInst_Ext& v);
    void ResetInst_Ext();

    // inst.hist
    typedef TInst::THist TInst_Hist;
    bool IsSetInst_Hist(void) const;
    bool CanGetInst_Hist(void) const;
    const TInst_Hist& GetInst_Hist(void) const;
    void SetInst_Hist(TInst_Hist& v);
    void ResetInst_Hist();

    // inst.hist.assembly
    typedef TInst::THist::TAssembly TInst_Hist_Assembly;
    bool IsSetInst_Hist_Assembly(void) const;
    bool CanGetInst_Hist_Assembly(void) const;
    const TInst_Hist_Assembly& GetInst_Hist_Assembly(void) const;
    void SetInst_Hist_Assembly(const TInst_Hist_Assembly& v);

    // inst.hist.replaces
    typedef TInst::THist::TReplaces TInst_Hist_Replaces;
    bool IsSetInst_Hist_Replaces(void) const;
    bool CanGetInst_Hist_Replaces(void) const;
    const TInst_Hist_Replaces& GetInst_Hist_Replaces(void) const;
    void SetInst_Hist_Replaces(TInst_Hist_Replaces& v);

    // inst.hist.replaced-by
    typedef TInst::THist::TReplaced_by TInst_Hist_Replaced_by;
    bool IsSetInst_Hist_Replaced_by(void) const;
    bool CanGetInst_Hist_Replaced_by(void) const;
    const TInst_Hist_Replaced_by& GetInst_Hist_Replaced_by(void) const;
    void SetInst_Hist_Replaced_by(TInst_Hist_Replaced_by& v);

    // inst.hist.deleted
    typedef TInst::THist::TDeleted TInst_Hist_Deleted;
    bool IsSetInst_Hist_Deleted(void) const;
    bool CanGetInst_Hist_Deleted(void) const;
    const TInst_Hist_Deleted& GetInst_Hist_Deleted(void) const;
    void SetInst_Hist_Deleted(TInst_Hist_Deleted& v);

    bool IsNa(void) const;
    bool IsAa(void) const;

    int GetFeatureFetchPolicy(void) const;

    // Get some values from core:
    const CSeqMap& GetSeqMap(void) const;

    int GetTaxId(void) const;

    int GetSequenceClass(void) const;

    void x_AttachMap(CSeqMap& seq_map);

    void x_AddSeq_dataChunkId(TChunkId chunk_id);
    void x_AddAssemblyChunkId(TChunkId chunk_id);
    void x_DoUpdate(TNeedUpdateFlags flags);

protected:
    friend class CDataSource;
    friend class CScope_Impl;

    friend class CTSE_Info;
    friend class CSeq_entry_Info;
    friend class CBioseq_set_Info;
    friend class CSeqMap;

    TObjAnnot& x_SetObjAnnot(void);
    void x_ResetObjAnnot(void);
    
    void x_ResetSeqMap(void);
    void x_SetChangedSeqMap(void);

private:
    CBioseq_Info& operator=(const CBioseq_Info&);

    void x_DSAttachContents(CDataSource& ds);
    void x_DSDetachContents(CDataSource& ds);

    void x_TSEAttachContents(CTSE_Info& tse);
    void x_TSEDetachContents(CTSE_Info& tse);

    void x_ParentAttach(CSeq_entry_Info& parent);
    void x_ParentDetach(CSeq_entry_Info& parent);

    TObject& x_GetObject(void);
    const TObject& x_GetObject(void) const;

    void x_SetObject(TObject& obj);
    void x_SetObject(const CBioseq_Info& info, TObjectCopyMap* copy_map);

    void x_DSMapObject(CConstRef<TObject> obj, CDataSource& ds);
    void x_DSUnmapObject(CConstRef<TObject> obj, CDataSource& ds);

    static CRef<TObject> sx_ShallowCopy(const TObject& obj);
    static CRef<TInst> sx_ShallowCopy(const TInst& inst);

    TSeqPos x_CalcBioseqLength(void) const;
    TSeqPos x_CalcBioseqLength(const CSeq_inst& inst) const;
    TSeqPos x_CalcBioseqLength(const CSeq_id& whole) const;
    TSeqPos x_CalcBioseqLength(const CPacked_seqint& ints) const;
    TSeqPos x_CalcBioseqLength(const CSeq_loc& seq_loc) const;
    TSeqPos x_CalcBioseqLength(const CSeq_loc_mix& seq_mix) const;
    TSeqPos x_CalcBioseqLength(const CSeq_loc_equiv& seq_equiv) const;
    TSeqPos x_CalcBioseqLength(const CSeg_ext& seg_ext) const;
    TSeqPos x_CalcBioseqLength(const CDelta_ext& delta) const;
    TSeqPos x_CalcBioseqLength(const CDelta_seq& delta_seq) const;

    // Bioseq object
    CRef<TObject>           m_Object;

    // Bioseq members
    TId                     m_Id;

    // SeqMap object
    mutable CRef<CSeqMap>   m_SeqMap;
    mutable CFastMutex      m_SeqMap_Mtx;

    TChunkIds               m_Seq_dataChunks;
    TChunkId                m_AssemblyChunk;
    mutable int             m_FeatureFetchPolicy;
};



/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
CBioseq& CBioseq_Info::x_GetObject(void)
{
    return *m_Object;
}


inline
const CBioseq& CBioseq_Info::x_GetObject(void) const
{
    return *m_Object;
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJECTS_OBJMGR_IMPL___BIOSEQ_INFO__HPP */
