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
    explicit CBioseq_Info(const CBioseq_Info&);
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

    // inst.repr
    typedef TInst::TRepr TInst_Repr;
    bool IsSetInst_Repr(void) const;
    bool CanGetInst_Repr(void) const;
    TInst_Repr GetInst_Repr(void) const;
    void SetInst_Repr(TInst_Repr v);

    // inst.mol
    typedef TInst::TMol TInst_Mol;
    bool IsSetInst_Mol(void) const;
    bool CanGetInst_Mol(void) const;
    TInst_Mol GetInst_Mol(void) const;
    void SetInst_Mol(TInst_Mol v);

    // inst.length
    typedef TInst::TLength TInst_Length;
    bool IsSetInst_Length(void) const;
    bool CanGetInst_Length(void) const;
    TInst_Length GetInst_Length(void) const;
    void SetInst_Length(TInst_Length v);
    TSeqPos GetBioseqLength(void) const; // try to calculate it if not set

    // inst.fuzz
    typedef TInst::TFuzz TInst_Fuzz;
    bool IsSetInst_Fuzz(void) const;
    bool CanGetInst_Fuzz(void) const;
    const TInst_Fuzz& GetInst_Fuzz(void) const;
    void SetInst_Fuzz(TInst_Fuzz& v);

    // inst.topology
    typedef TInst::TTopology TInst_Topology;
    bool IsSetInst_Topology(void) const;
    bool CanGetInst_Topology(void) const;
    TInst_Topology GetInst_Topology(void) const;
    void SetInst_Topology(TInst_Topology v);

    // inst.strand
    typedef TInst::TStrand TInst_Strand;
    bool IsSetInst_Strand(void) const;
    bool CanGetInst_Strand(void) const;
    TInst_Strand GetInst_Strand(void) const;
    void SetInst_Strand(TInst_Strand v);

    // inst.seq-data
    typedef TInst::TSeq_data TInst_Seq_data;
    bool IsSetInst_Seq_data(void) const;
    bool CanGetInst_Seq_data(void) const;
    const TInst_Seq_data& GetInst_Seq_data(void) const;
    void SetInst_Seq_data(TInst_Seq_data& v);

    // inst.ext
    typedef TInst::TExt TInst_Ext;
    bool IsSetInst_Ext(void) const;
    bool CanGetInst_Ext(void) const;
    const TInst_Ext& GetInst_Ext(void) const;
    void SetInst_Ext(TInst_Ext& v);

    // inst.hist
    typedef TInst::THist TInst_Hist;
    bool IsSetInst_Hist(void) const;
    bool CanGetInst_Hist(void) const;
    const TInst_Hist& GetInst_Hist(void) const;
    void SetInst_Hist(TInst_Hist& v);

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

    // Get some values from core:
    const CSeqMap& GetSeqMap(void) const;

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

    TObjAnnot& x_SetObjAnnot(void);
    void x_ResetObjAnnot(void);

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
    void x_SetObject(const CBioseq_Info& info);

    typedef vector< CConstRef<TObject> > TDSMappedObjects;
    virtual void x_DSMapObject(CConstRef<TObject> obj, CDataSource& ds);
    virtual void x_DSUnmapObject(CConstRef<TObject> obj, CDataSource& ds);

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
    mutable CConstRef<CSeqMap>  m_SeqMap;
    mutable CFastMutex          m_SeqMap_Mtx;

    TChunkIds               m_Seq_dataChunks;
    TChunkId                m_AssemblyChunk;
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

/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.24  2005/02/02 21:59:39  vasilche
 * Implemented CBioseq_Handle AddId() & RemoveId().
 *
 * Revision 1.23  2005/01/04 16:31:07  grichenk
 * Added IsNa(), IsAa()
 *
 * Revision 1.22  2004/10/18 13:56:36  vasilche
 * Added support for split assembly.
 *
 * Revision 1.21  2004/07/12 16:57:32  vasilche
 * Fixed loading of split Seq-descr and Seq-data objects.
 * They are loaded correctly now when GetCompleteXxx() method is called.
 *
 * Revision 1.20  2004/07/12 15:05:31  grichenk
 * Moved seq-id mapper from xobjmgr to seq library
 *
 * Revision 1.19  2004/05/06 17:32:37  grichenk
 * Added CanGetXXXX() methods
 *
 * Revision 1.18  2004/03/31 17:08:06  vasilche
 * Implemented ConvertSeqToSet and ConvertSetToSeq.
 *
 * Revision 1.17  2004/03/24 18:30:28  vasilche
 * Fixed edit API.
 * Every *_Info object has its own shallow copy of original object.
 *
 * Revision 1.16  2004/03/16 15:47:26  vasilche
 * Added CBioseq_set_Handle and set of EditHandles
 *
 * Revision 1.15  2003/11/28 15:13:25  grichenk
 * Added CSeq_entry_Handle
 *
 * Revision 1.14  2003/09/30 16:22:00  vasilche
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
 * Revision 1.13  2003/06/02 16:01:37  dicuccio
 * Rearranged include/objects/ subtree.  This includes the following shifts:
 *     - include/objects/alnmgr --> include/objtools/alnmgr
 *     - include/objects/cddalignview --> include/objtools/cddalignview
 *     - include/objects/flat --> include/objtools/flat
 *     - include/objects/objmgr/ --> include/objmgr/
 *     - include/objects/util/ --> include/objmgr/util/
 *     - include/objects/validator --> include/objtools/validator
 *
 * Revision 1.12  2003/04/29 19:51:12  vasilche
 * Fixed interaction of Data Loader garbage collector and TSE locking mechanism.
 * Made some typedefs more consistent.
 *
 * Revision 1.11  2003/04/25 14:23:46  vasilche
 * Added explicit constructors, destructor and assignment operator to make it compilable on MSVC DLL.
 *
 * Revision 1.10  2003/04/24 16:12:37  vasilche
 * Object manager internal structures are splitted more straightforward.
 * Removed excessive header dependencies.
 *
 * Revision 1.9  2003/04/14 21:31:05  grichenk
 * Removed operators ==(), !=() and <()
 *
 * Revision 1.8  2003/03/12 20:09:31  grichenk
 * Redistributed members between CBioseq_Handle, CBioseq_Info and CTSE_Info
 *
 * Revision 1.7  2003/02/05 17:57:41  dicuccio
 * Moved into include/objects/objmgr/impl to permit data loaders to be defined
 * outside of xobjmgr.
 *
 * Revision 1.6  2002/12/26 21:03:33  dicuccio
 * Added Win32 export specifier.  Moved file from src/objects/objmgr to
 * include/objects/objmgr.
 *
 * Revision 1.5  2002/07/08 20:51:01  grichenk
 * Moved log to the end of file
 * Replaced static mutex (in CScope, CDataSource) with the mutex
 * pool. Redesigned CDataSource data locking.
 *
 * Revision 1.4  2002/05/29 21:21:13  gouriano
 * added debug dump
 *
 * Revision 1.3  2002/05/06 03:28:46  vakatov
 * OM/OM1 renaming
 *
 * Revision 1.2  2002/02/21 19:27:05  grichenk
 * Rearranged includes. Added scope history. Added searching for the
 * best seq-id match in data sources and scopes. Updated tests.
 *
 * Revision 1.1  2002/02/07 21:25:05  grichenk
 * Initial revision
 *
 *
 * ===========================================================================
 */

#endif  /* OBJECTS_OBJMGR_IMPL___BIOSEQ_INFO__HPP */
