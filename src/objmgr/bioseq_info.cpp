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


#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/data_source.hpp>

#include <objmgr/seq_id_handle.hpp>
#include <objmgr/seq_id_mapper.hpp>

#include <objmgr/seq_map.hpp>

#include <objects/seq/Bioseq.hpp>

#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Ref_ext.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Packed_seqint.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CBioseq_Info::
//
//    Structure to keep bioseq's parent seq-entry along with the list
//    of seq-id synonyms for the bioseq.
//


CBioseq_Info::CBioseq_Info(CBioseq& seq, CSeq_entry_Info& entry_info)
    : m_Bioseq(&seq),
      m_Seq_entry_Info(&entry_info),
      m_TSE_Info(&entry_info.GetTSE_Info())
{
    _ASSERT(entry_info.m_Entries.empty());
    _ASSERT(!entry_info.m_Bioseq);
    entry_info.m_Bioseq.Reset(this);
    try {
        x_InitBioseqInfo();
        x_TSEAttach();
        
        if ( seq.IsSetAnnot() ) {
            entry_info.x_TSEAttachSeq_annots(seq.SetAnnot());
        }
    }
    catch ( exception& ) {
        entry_info.m_Bioseq.Reset();
        throw;
    }
}


CBioseq_Info::~CBioseq_Info(void)
{
}


void CBioseq_Info::x_TSEAttach(void)
{
    CTSE_Info& tse_info = GetTSE_Info();
    CBioseq& seq = GetBioseq();
    try {
        ITERATE ( CBioseq::TId, id, seq.GetId() ) {
            // Find the bioseq index
            CSeq_id_Handle key = CSeq_id_Handle::GetHandle(**id);
            tse_info.x_SetBioseqId(key, this);
            // Add new seq-id synonym
            m_Synonyms.push_back(key);
        }
    }
    catch ( exception& ) {
        x_TSEDetach();
        throw;
    }
}


void CBioseq_Info::x_TSEDetach(void)
{
    CTSE_Info& tse_info = GetTSE_Info();
    ITERATE ( TSynonyms, syn, m_Synonyms ) {
        tse_info.x_ResetBioseqId(*syn, this);
    }
    m_Synonyms.clear();
}


void CBioseq_Info::x_InitBioseqInfo(void)
{
    CBioseq& seq = GetBioseq();
    try {
        m_BioseqLength = seq.GetInst().GetLength();
    }
    catch ( exception& /*exc*/ ) {
        LOG_POST(Warning <<
                 "CBioseq_Info::x_InitBioseqInfo: Seq-inst.length is not set");
        m_BioseqLength = kInvalidSeqPos;
    }
    m_BioseqMolType = seq.GetInst().GetMol();
}


TSeqPos CBioseq_Info::x_CalcBioseqLength(void) const
{
    return x_CalcBioseqLength(GetBioseq().GetInst());
}


TSeqPos CBioseq_Info::x_CalcBioseqLength(const CSeq_inst& inst) const
{
    if ( !inst.IsSetExt() ) {
        THROW1_TRACE(runtime_error,
                     "CBioseq_Info::x_CalcBioseqLength: "
                     "failed: Seq-inst.ext is not set");
    }
    switch ( inst.GetExt().Which() ) {
    case CSeq_ext::e_Seg:
        return x_CalcBioseqLength(inst.GetExt().GetSeg());
    case CSeq_ext::e_Ref:
        return x_CalcBioseqLength(inst.GetExt().GetRef().Get());
    case CSeq_ext::e_Delta:
        return x_CalcBioseqLength(inst.GetExt().GetDelta());
    default:
        THROW1_TRACE(runtime_error,
                     "CBioseq_Info::x_CalcBioseqLength: "
                     "failed: bad Seg-ext type");
    }
}


TSeqPos CBioseq_Info::x_CalcBioseqLength(const CSeq_id& whole) const
{
    CConstRef<CBioseq_Info> ref =
        GetTSE_Info().FindBioseq(CSeq_id_Handle::GetHandle(whole));
    if ( !ref ) {
        THROW1_TRACE(runtime_error,
                     "CBioseq_Info::x_CalcBioseqLength: "
                     "failed: external whole reference");
    }
    return ref->GetBioseqLength();
}


TSeqPos CBioseq_Info::x_CalcBioseqLength(const CPacked_seqint& ints) const
{
    TSeqPos ret = 0;
    ITERATE ( CPacked_seqint::Tdata, it, ints.Get() ) {
        ret += (*it)->GetLength();
    }
    return ret;
}


TSeqPos CBioseq_Info::x_CalcBioseqLength(const CSeq_loc& seq_loc) const
{
    switch ( seq_loc.Which() ) {
    case CSeq_loc::e_not_set:
    case CSeq_loc::e_Null:
    case CSeq_loc::e_Empty:
        return 0;
    case CSeq_loc::e_Whole:
        return x_CalcBioseqLength(seq_loc.GetWhole());
    case CSeq_loc::e_Int:
        return seq_loc.GetInt().GetLength();
    case CSeq_loc::e_Pnt:
        return 1;
    case CSeq_loc::e_Packed_int:
        return x_CalcBioseqLength(seq_loc.GetPacked_int());
    case CSeq_loc::e_Packed_pnt:
        return seq_loc.GetPacked_pnt().GetPoints().size();
    case CSeq_loc::e_Mix:
        return x_CalcBioseqLength(seq_loc.GetMix());
    case CSeq_loc::e_Equiv:
        return x_CalcBioseqLength(seq_loc.GetEquiv());
    default:
        THROW1_TRACE(runtime_error,
                     "CBioseq_Info::x_CalcBioseqLength: "
                     "failed: bad Seq-loc type");
    }
}


TSeqPos CBioseq_Info::x_CalcBioseqLength(const CSeq_loc_mix& seq_mix) const
{
    TSeqPos ret = 0;
    ITERATE ( CSeq_loc_mix::Tdata, it, seq_mix.Get() ) {
        ret += x_CalcBioseqLength(**it);
    }
    return ret;
}


TSeqPos CBioseq_Info::x_CalcBioseqLength(const CSeq_loc_equiv& seq_equiv) const
{
    TSeqPos ret = 0;
    ITERATE ( CSeq_loc_equiv::Tdata, it, seq_equiv.Get() ) {
        ret += x_CalcBioseqLength(**it);
    }
    return ret;
}


TSeqPos CBioseq_Info::x_CalcBioseqLength(const CSeg_ext& seg_ext) const
{
    TSeqPos ret = 0;
    ITERATE ( CSeg_ext::Tdata, it, seg_ext.Get() ) {
        ret += x_CalcBioseqLength(**it);
    }
    return ret;
}


TSeqPos CBioseq_Info::x_CalcBioseqLength(const CDelta_ext& delta) const
{
    TSeqPos ret = 0;
    ITERATE ( CDelta_ext::Tdata, it, delta.Get() ) {
        ret += x_CalcBioseqLength(**it);
    }
    return ret;
}


TSeqPos CBioseq_Info::x_CalcBioseqLength(const CDelta_seq& delta_seq) const
{
    switch ( delta_seq.Which() ) {
    case CDelta_seq::e_Loc:
        return x_CalcBioseqLength(delta_seq.GetLoc());
    case CDelta_seq::e_Literal:
        return delta_seq.GetLiteral().GetLength();
    default:
        THROW1_TRACE(runtime_error,
                     "CBioseq_Info::x_CalcBioseqLength: "
                     "failed: bad Delta-seq type");
    }
}


string CBioseq_Info::IdsString(void) const
{
    CNcbiOstrstream os;
    ITERATE ( CBioseq::TId, it, GetBioseq().GetId() ) {
        os << (*it)->DumpAsFasta() << " | ";
    }
    return CNcbiOstrstreamToString(os);
}


CDataSource& CBioseq_Info::GetDataSource(void) const
{
    return m_Seq_entry_Info->GetDataSource();
}


const CSeq_entry& CBioseq_Info::GetTSE(void) const
{
    return m_Seq_entry_Info->GetTSE();
}


const CSeq_entry& CBioseq_Info::GetSeq_entry(void) const
{
    return m_Seq_entry_Info->GetSeq_entry();
}


CSeq_entry& CBioseq_Info::GetSeq_entry(void)
{
    return m_Seq_entry_Info->GetSeq_entry();
}


void CBioseq_Info::x_DSAttachThis(void)
{
    GetDataSource().x_MapBioseq(*this);
}


void CBioseq_Info::x_DSDetachThis(void)
{
    GetDataSource().x_UnmapBioseq(*this);
}


void CBioseq_Info::x_AttachMap(CSeqMap& seq_map)
{
    CFastMutexGuard guard(m_SeqMap_Mtx);
    if ( m_SeqMap ) {
        THROW1_TRACE(runtime_error,
                     "CBioseq_Info::AttachMap: bioseq already has SeqMap");
    }
    m_SeqMap.Reset(&seq_map);
}


const CSeqMap& CBioseq_Info::GetSeqMap(void) const
{
    const CSeqMap* ret = m_SeqMap.GetPointer();
    if ( !ret ) {
        CFastMutexGuard guard(m_SeqMap_Mtx);
        ret = m_SeqMap.GetPointer();
        if ( !ret ) {
            m_SeqMap = CSeqMap::CreateSeqMapForBioseq(GetBioseq());
            ret = m_SeqMap.GetPointer();
            _ASSERT(ret);
        }
    }
    return *ret;
}


void CBioseq_Info::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    ddc.SetFrame("CBioseq_Info");
    CObject::DebugDump( ddc, depth);

    ddc.Log("m_Bioseq", m_Bioseq.GetPointer(),0);
    if (depth == 0) {
        DebugDumpValue(ddc, "m_Synonyms.size()", m_Synonyms.size());
    } else {
        DebugDumpValue(ddc, "m_Synonyms.type",
            "vector<CSeq_id_Handle>");
        CDebugDumpContext ddc2(ddc,"m_Synonyms");
        TSynonyms::const_iterator it;
        int n;
        for (n=0, it=m_Synonyms.begin(); it!=m_Synonyms.end(); ++n, ++it) {
            string member_name = "m_Synonyms[ " +
                NStr::IntToString(n) +" ]";
            ddc2.Log(member_name, (*it).AsString());
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.13  2003/11/12 16:53:17  grichenk
* Modified CSeqMap to work with const objects (CBioseq, CSeq_loc etc.)
*
* Revision 1.12  2003/09/30 16:22:02  vasilche
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
* Revision 1.11  2003/06/02 16:06:37  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.10  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.9  2003/03/14 19:10:41  grichenk
* + SAnnotSelector::EIdResolving; fixed operator=() for several classes
*
* Revision 1.8  2003/03/11 15:51:06  kuznets
* iterate -> ITERATE
*
* Revision 1.7  2003/02/05 17:59:17  dicuccio
* Moved formerly private headers into include/objects/objmgr/impl
*
* Revision 1.6  2002/12/26 20:55:17  dicuccio
* Moved seq_id_mapper.hpp, tse_info.hpp, and bioseq_info.hpp -> include/ tree
*
* Revision 1.5  2002/11/04 21:29:12  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.4  2002/07/08 20:51:01  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.3  2002/05/29 21:21:13  gouriano
* added debug dump
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
