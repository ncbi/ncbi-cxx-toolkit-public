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

#include <objmgr/seq_id_handle.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>

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
    explicit CBioseq_Info(const CBioseq& seq);
    virtual ~CBioseq_Info(void);

    typedef CBioseq TObject;

    CConstRef<TObject> GetBioseqCore(void) const;
    CConstRef<TObject> GetCompleteBioseq(void) const;

    // Bioseq members
    // id
    typedef vector<CSeq_id_Handle> TId;
    bool IsSetId(void) const;
    const TId& GetId(void) const;
    string IdString(void) const;

    // inst
    typedef TObject::TInst TInst;
    bool IsSetInst(void) const;
    const TInst& GetInst(void) const;
    // inst.repr
    typedef TInst::TRepr TInst_Repr;
    bool IsSetInst_Repr(void) const;
    TInst_Repr GetInst_Repr(void) const;
    // inst.mol
    typedef TInst::TMol TInst_Mol;
    bool IsSetInst_Mol(void) const;
    TInst_Mol GetInst_Mol(void) const;
    // inst.length
    typedef TInst::TLength TInst_Length;
    bool IsSetInst_Length(void) const;
    TInst_Length GetInst_Length(void) const;
    TSeqPos GetBioseqLength(void) const; // try to calculate it if not set
    // inst.fuzz
    typedef TInst::TFuzz TInst_Fuzz;
    bool IsSetInst_Fuzz(void) const;
    const TInst_Fuzz& GetInst_Fuzz(void) const;
    // inst.topology
    typedef TInst::TTopology TInst_Topology;
    bool IsSetInst_Topology(void) const;
    TInst_Topology GetInst_Topology(void) const;
    // inst.strand
    typedef TInst::TStrand TInst_Strand;
    bool IsSetInst_Strand(void) const;
    TInst_Strand GetInst_Strand(void) const;
    // inst.seq-data
    typedef TInst::TSeq_data TInst_Seq_data;
    bool IsSetInst_Seq_data(void) const;
    const TInst_Seq_data& GetInst_Seq_data(void) const;
    // inst.ext
    typedef TInst::TExt TInst_Ext;
    bool IsSetInst_Ext(void) const;
    const TInst_Ext& GetInst_Ext(void) const;
    // inst.hist
    typedef TInst::THist TInst_Hist;
    bool IsSetInst_Hist(void) const;
    const TInst_Hist& GetInst_Hist(void) const;

    // Get some values from core:
    const CSeqMap& GetSeqMap(void) const;

    void x_AttachMap(CSeqMap& seq_map);

    void x_DSAttachContents(CDataSource& ds);
    void x_DSDetachContents(CDataSource& ds);

    void x_TSEAttachContents(CTSE_Info& tse);
    void x_TSEDetachContents(CTSE_Info& tse);

    virtual const char* x_GetTypeName(void) const;
    virtual const char* x_GetMemberName(TMembers member) const;

    enum EMember {
        fMember_first       = TParent::fMember_last << 1,
        fMember_id          = fMember_first << 0,
        fMember_inst        = fMember_first << 1,
        fMember_inst_repr   = fMember_first << 2,
        fMember_inst_mol    = fMember_first << 3,
        fMember_inst_length = fMember_first << 4,
        fMember_inst_fuzz   = fMember_first << 5,
        fMember_inst_topology = fMember_first << 6,
        fMember_inst_strand = fMember_first << 7,
        fMember_inst_seq_data = fMember_first << 8,
        fMember_inst_ext    = fMember_first << 9,
        fMember_inst_hist   = fMember_first << 10,

        fMember_last_plus_one,
        fMember_last        = fMember_last_plus_one - 1,

        fMember_inst_all    = (fMember_inst_hist << 1) - fMember_inst
    };
protected:
    friend class CDataSource;
    friend class CScope_Impl;
    friend class CSeq_entry_Info;

private:
    CBioseq_Info& operator=(const CBioseq_Info&);

    void x_SetObject(const TObject& obj);
    void x_UpdateModifiedObject(void) const;
    void x_UpdateObject(CConstRef<TObject> obj);

    typedef vector< CConstRef<TObject> > TDSMappedObjects;
    virtual void x_DSMapObject(CConstRef<TObject> obj, CDataSource& ds);
    virtual void x_DSUnmapObject(CConstRef<TObject> obj, CDataSource& ds);

    CRef<TObject> x_CreateObject(void) const;
    CRef<TInst> x_CreateInst(void) const;

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
    CConstRef<TObject>      m_Object;
    TDSMappedObjects        m_DSMappedObjects;

    // Bioseq members
    TId                     m_Id;
    CConstRef<TInst>        m_Inst;
    // Bioseq.inst members
    TInst_Repr              m_Inst_Repr;
    TInst_Mol               m_Inst_Mol;
    mutable TInst_Length    m_Inst_Length; // cached sequence length
    CConstRef<TInst_Fuzz>   m_Inst_Fuzz;
    TInst_Topology          m_Inst_Topology;
    TInst_Strand            m_Inst_Strand;
    CConstRef<TInst_Seq_data> m_Inst_Seq_data;
    CConstRef<TInst_Ext>    m_Inst_Ext;
    CConstRef<TInst_Hist>   m_Inst_Hist;

    // SeqMap object
    mutable CConstRef<CSeqMap>  m_SeqMap;
    mutable CFastMutex          m_SeqMap_Mtx;

};



/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
bool CBioseq_Info::IsSetId(void) const
{
    return x_IsSetMember(fMember_id);
}


inline
const CBioseq_Info::TId& CBioseq_Info::GetId(void) const
{
    return m_Id;
}


inline
bool CBioseq_Info::IsSetInst(void) const
{
    return x_IsSetMember(fMember_inst);
}


inline
const CBioseq_Info::TInst& CBioseq_Info::GetInst(void) const
{
    x_CheckSetMember(fMember_inst);
    return *m_Inst;
}


inline
bool CBioseq_Info::IsSetInst_Repr(void) const
{
    return x_IsSetMember(fMember_inst_repr);
}


inline
CBioseq_Info::TInst_Repr CBioseq_Info::GetInst_Repr(void) const
{
    x_CheckSetMember(fMember_inst_repr);
    return m_Inst_Repr;
}


inline
bool CBioseq_Info::IsSetInst_Mol(void) const
{
    return x_IsSetMember(fMember_inst_mol);
}


inline
CBioseq_Info::TInst_Mol CBioseq_Info::GetInst_Mol(void) const
{
    x_CheckSetMember(fMember_inst_mol);
    return m_Inst_Mol;
}


inline
bool CBioseq_Info::IsSetInst_Length(void) const
{
    return x_IsSetMember(fMember_inst_length);
}


inline
CBioseq_Info::TInst_Length CBioseq_Info::GetInst_Length(void) const
{
    x_CheckSetMember(fMember_inst_length);
    return m_Inst_Length;
}


inline
CBioseq_Info::TInst_Length CBioseq_Info::GetBioseqLength(void) const
{
    TSeqPos length = m_Inst_Length;
    if ( length == kInvalidSeqPos ) {
        length = m_Inst_Length = x_CalcBioseqLength();
    }
    return length;
}


inline
bool CBioseq_Info::IsSetInst_Fuzz(void) const
{
    return x_IsSetMember(fMember_inst_fuzz);
}


inline
const CBioseq_Info::TInst_Fuzz& CBioseq_Info::GetInst_Fuzz(void) const
{
    x_CheckSetMember(fMember_inst_fuzz);
    return *m_Inst_Fuzz;
}


inline
bool CBioseq_Info::IsSetInst_Topology(void) const
{
    return x_IsSetMember(fMember_inst_topology);
}


inline
CBioseq_Info::TInst_Topology CBioseq_Info::GetInst_Topology(void) const
{
    x_CheckSetMember(fMember_inst_topology);
    return m_Inst_Topology;
}


inline
bool CBioseq_Info::IsSetInst_Strand(void) const
{
    return x_IsSetMember(fMember_inst_strand);
}


inline
CBioseq_Info::TInst_Strand CBioseq_Info::GetInst_Strand(void) const
{
    x_CheckSetMember(fMember_inst_strand);
    return m_Inst_Strand;
}


inline
bool CBioseq_Info::IsSetInst_Seq_data(void) const
{
    return x_IsSetMember(fMember_inst_seq_data);
}


inline
const CBioseq_Info::TInst_Seq_data& CBioseq_Info::GetInst_Seq_data(void) const
{
    x_CheckSetMember(fMember_inst_seq_data);
    return *m_Inst_Seq_data;
}


inline
bool CBioseq_Info::IsSetInst_Ext(void) const
{
    return x_IsSetMember(fMember_inst_ext);
}


inline
const CBioseq_Info::TInst_Ext& CBioseq_Info::GetInst_Ext(void) const
{
    x_CheckSetMember(fMember_inst_ext);
    return *m_Inst_Ext;
}


inline
bool CBioseq_Info::IsSetInst_Hist(void) const
{
    return x_IsSetMember(fMember_inst_hist);
}


inline
const CBioseq_Info::TInst_Hist& CBioseq_Info::GetInst_Hist(void) const
{
    x_CheckSetMember(fMember_inst_hist);
    return *m_Inst_Hist;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ---------------------------------------------------------------------------
 * $Log$
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
