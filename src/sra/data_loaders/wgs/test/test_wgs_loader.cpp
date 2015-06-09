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
* Author:  Eugene Vasilchenko
*
* File Description:
*   Unit tests for WGS data loader
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <sra/data_loaders/wgs/wgsloader.hpp>
#include <sra/readers/sra/wgsread.hpp>
#include <sra/readers/ncbi_traces_path.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <corelib/ncbi_system.hpp>
#include <objtools/readers/idmapper.hpp>
#include <serial/iterator.hpp>
#include <objmgr/util/sequence.hpp>

#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

#define PILEUP_NAME_SUFFIX " pileup graphs"

// 0 - never, 1 - every 5th day, 2 - always
#define DEFAULT_REPORT_GENERAL_ID_ERROR 1
#define DEFAULT_REPORT_SEQ_STATE_ERROR 1

NCBI_PARAM_DECL(int, WGS, REPORT_GENERAL_ID_ERROR);
NCBI_PARAM_DEF(int, WGS, REPORT_GENERAL_ID_ERROR,
               DEFAULT_REPORT_GENERAL_ID_ERROR);
NCBI_PARAM_DECL(int, WGS, REPORT_SEQ_STATE_ERROR);
NCBI_PARAM_DEF(int, WGS, REPORT_SEQ_STATE_ERROR,
               DEFAULT_REPORT_SEQ_STATE_ERROR);

static bool GetReportError(int report_level)
{
    // optionally report error only when day of month is divisible by 5
    return ( (report_level >= 2) ||
             (report_level == 1 && CTime(CTime::eCurrent).Day() % 5 == 0) );
}

static bool GetReportGeneralIdError(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(WGS, REPORT_GENERAL_ID_ERROR)> s_Value;
    return GetReportError(s_Value->Get());
}

static bool GetReportSeqStateError(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(WGS, REPORT_SEQ_STATE_ERROR)> s_Value;
    return GetReportError(s_Value->Get());
}

enum EMasterDescrType
{
    eWithoutMasterDescr,
    eWithMasterDescr
};

static EMasterDescrType s_master_descr_type = eWithoutMasterDescr;

void sx_InitGBLoader(CObjectManager& om)
{
    CGBDataLoader* gbloader = dynamic_cast<CGBDataLoader*>
        (CGBDataLoader::RegisterInObjectManager(om, 0, om.eNonDefault).GetLoader());
    _ASSERT(gbloader);
    gbloader->SetAddWGSMasterDescr(s_master_descr_type == eWithMasterDescr);
}

CRef<CObjectManager> sx_GetEmptyOM(void)
{
    SetDiagPostLevel(eDiag_Info);
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CObjectManager::TRegisteredNames names;
    om->GetRegisteredNames(names);
    ITERATE ( CObjectManager::TRegisteredNames, it, names ) {
        om->RevokeDataLoader(*it);
    }
    return om;
}

CRef<CObjectManager> sx_InitOM(EMasterDescrType master_descr_type)
{
    CRef<CObjectManager> om = sx_GetEmptyOM();
    s_master_descr_type = master_descr_type;
    CWGSDataLoader* wgsloader = dynamic_cast<CWGSDataLoader*>
        (CWGSDataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault, 88).GetLoader());
    wgsloader->SetAddWGSMasterDescr(s_master_descr_type == eWithMasterDescr);
    if ( master_descr_type == eWithMasterDescr ) {
        sx_InitGBLoader(*om);
    }
    return om;
}

CRef<CObjectManager> sx_InitOM(EMasterDescrType master_descr_type,
                               const string& wgs_file)
{
    CRef<CObjectManager> om = sx_GetEmptyOM();
    s_master_descr_type = master_descr_type;
    CWGSDataLoader* wgsloader = dynamic_cast<CWGSDataLoader*>
        (CWGSDataLoader::RegisterInObjectManager(*om,
                                                 "",
                                                 vector<string>(1, wgs_file),
                                                 CObjectManager::eDefault, 88).GetLoader());
    wgsloader->SetAddWGSMasterDescr(s_master_descr_type == eWithMasterDescr);
    return om;
}

CBioseq_Handle sx_LoadFromGB(const CBioseq_Handle& bh)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    sx_InitGBLoader(*om);
    CScope scope(*om);
    scope.AddDataLoader("GBLOADER");
    return scope.GetBioseqHandle(*bh.GetSeqId());
}

string sx_GetASN(const CBioseq_Handle& bh)
{
    CNcbiOstrstream str;
    if ( bh ) {
        str << MSerial_AsnText << *bh.GetCompleteBioseq();
    }
    return CNcbiOstrstreamToString(str);
}

CRef<CSeq_id> sx_ExtractGeneralId(CBioseq& seq)
{
    NON_CONST_ITERATE ( CBioseq::TId, it, seq.SetId() ) {
        CRef<CSeq_id> id = *it;
        if ( id->Which() == CSeq_id::e_General ) {
            seq.SetId().erase(it);
            return id;
        }
    }
    return null;
}

CRef<CDelta_ext> sx_ExtractDelta(CBioseq& seq)
{
    CRef<CDelta_ext> delta;
    CSeq_inst& inst = seq.SetInst();
    if ( inst.IsSetExt() ) {
        CSeq_ext& ext = inst.SetExt();
        if ( ext.IsDelta() ) {
            delta = &ext.SetDelta();
            ext.Reset();
        }
    }
    else if ( inst.IsSetSeq_data() ) {
        CRef<CDelta_seq> ext(new CDelta_seq);
        ext->SetLiteral().SetLength(inst.GetLength());
        ext->SetLiteral().SetSeq_data(inst.SetSeq_data());
        delta = new CDelta_ext();
        delta->Set().push_back(ext);
        inst.ResetSeq_data();
        if ( inst.GetRepr() == CSeq_inst::eRepr_raw ) {
            inst.SetRepr(CSeq_inst::eRepr_delta);
            inst.SetExt();
        }
    }
    if ( inst.IsSetStrand() && inst.GetStrand() == CSeq_inst::eStrand_ds ) {
        inst.ResetStrand();
    }
    return delta;
}

bool sx_EqualGeneralId(const CSeq_id& gen1, const CSeq_id& gen2)
{
    if ( gen1.Equals(gen2) ) {
        return true;
    }
    // allow partial match in Dbtag.db like "WGS:ABBA" vs "WGS:ABBA01"
    if ( !gen1.IsGeneral() || !gen2.IsGeneral() ) {
        return false;
    }
    const CDbtag& id1 = gen1.GetGeneral();
    const CDbtag& id2 = gen2.GetGeneral();
    if ( !id1.GetTag().Equals(id2.GetTag()) ) {
        return false;
    }
    const string& db1 = id1.GetDb();
    const string& db2 = id2.GetDb();
    size_t len = min(db1.size(), db2.size());
    if ( db1.substr(0, len) != db2.substr(0, len) ) {
        return false;
    }
    if ( db1.size() <= len+2 && db2.size() <= len+2 ) {
        return true;
    }
    return false;
}

bool sx_EqualDelta(const CDelta_ext& delta1, const CDelta_ext& delta2,
                   bool report_error = false)
{
    if ( delta1.Equals(delta2) ) {
        return true;
    }
    // slow check for possible different representation of delta sequence
    CScope scope(*CObjectManager::GetInstance());
    CRef<CBioseq> seq1(new CBioseq);
    CRef<CBioseq> seq2(new CBioseq);
    seq1->SetId().push_back(Ref(new CSeq_id("lcl|1")));
    seq2->SetId().push_back(Ref(new CSeq_id("lcl|2")));
    seq1->SetInst().SetRepr(CSeq_inst::eRepr_delta);
    seq2->SetInst().SetRepr(CSeq_inst::eRepr_delta);
    seq1->SetInst().SetMol(CSeq_inst::eMol_na);
    seq2->SetInst().SetMol(CSeq_inst::eMol_na);
    seq1->SetInst().SetExt().SetDelta(const_cast<CDelta_ext&>(delta1));
    seq2->SetInst().SetExt().SetDelta(const_cast<CDelta_ext&>(delta2));
    CBioseq_Handle bh1 = scope.AddBioseq(*seq1);
    CBioseq_Handle bh2 = scope.AddBioseq(*seq2);
    CSeqVector sv1 = bh1.GetSeqVector(CBioseq_Handle::eCoding_Ncbi);
    CSeqVector sv2 = bh2.GetSeqVector(CBioseq_Handle::eCoding_Ncbi);
    if ( sv1.size() != sv2.size() ) {
        if ( report_error ) {
            NcbiCout << "ERROR: Lengths are different: "
                     << "WGS: " << sv1.size() << " != "
                     << "GB: " << sv2.size()
                     << NcbiEndl;
        }
        return false;
    }
    for ( CSeqVector_CI it1 = sv1.begin(), it2 = sv2.begin();
          it1 && it2; ++it1, ++it2 ) {
        if ( it1.IsInGap() != it2.IsInGap() ) {
            if ( report_error ) {
                NcbiCout << "ERROR: Gaps are different @ "<<it1.GetPos()<<": "
                         << "WGS: " << it1.IsInGap() << " != "
                         << "GB: " << it2.IsInGap()
                         << NcbiEndl;
            }
            return false;
        }
        if ( *it1 != *it2 ) {
            if ( report_error ) {
                NcbiCout << "ERROR: Bases are different @ "<<it1.GetPos()<<": "
                         << "WGS: " << int(*it1) << " != "
                         << "GB: " << int(*it2)
                         << NcbiEndl;
            }
            return false;
        }
    }
    return true;
}

string sx_BinaryASN(const CSerialObject& obj)
{
    CNcbiOstrstream str;
    str << MSerial_AsnBinary << obj;
    return CNcbiOstrstreamToString(str);
}

struct PDescLess {
    bool operator()(const CRef<CSeqdesc>& r1,
                    const CRef<CSeqdesc>& r2) const
        {
            const CSeqdesc& d1 = *r1;
            const CSeqdesc& d2 = *r2;
            if ( d1.Which() != d2.Which() ) {
                return d1.Which() < d2.Which();
            }
            return sx_BinaryASN(d1) < sx_BinaryASN(d2);
        }
};

CRef<CSeq_descr> sx_ExtractDescr(const CBioseq_Handle& bh)
{
    CRef<CSeq_descr> descr(new CSeq_descr);
    CSeq_descr::Tdata& dst = descr->Set();
    for ( CSeqdesc_CI it(bh); it; ++it ) {
        dst.push_back(Ref(SerialClone(*it)));
    }
    dst.sort(PDescLess());
    return descr;
}

class CEntryAnnots
{
public:
    explicit CEntryAnnots(CRef<CSeq_entry> entry)
        : m_Entry(entry)
        {
        }
    CSeq_annot& Select(const CSeq_annot_Handle& src_annot)
        {
            if ( m_CurSrcAnnot != src_annot ) {
                CRef<CSeq_annot> tmp_annot(new CSeq_annot);
                tmp_annot->Assign(*src_annot.GetObjectCore(), eShallow);
                tmp_annot->SetData(*new CSeq_annot::TData);
                tmp_annot->SetData().SetLocs();
                string key = sx_BinaryASN(*tmp_annot);
                if ( !m_Annots.count(key) ) {
                    m_Annots[key] = tmp_annot;
                    m_Entry->SetSet().SetAnnot().push_back(tmp_annot);
                }
                m_CurSrcAnnot = src_annot;
                m_CurDstAnnot = m_Annots[key];
            }
            return *m_CurDstAnnot;
        }

private:
    CRef<CSeq_entry> m_Entry;
    CSeq_annot_Handle m_CurSrcAnnot;
    CRef<CSeq_annot> m_CurDstAnnot;
    map<string, CRef<CSeq_annot> > m_Annots;
};

CRef<CSeq_entry> sx_ExtractAnnot(const CBioseq_Handle& bh)
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSet().SetSeq_set();

    CFeat_CI feat_it(bh);
    if ( feat_it ) {
        CEntryAnnots annots(entry);
        for ( ; feat_it; ++feat_it ) {
            CSeq_annot& dst_annot = annots.Select(feat_it.GetAnnot());
            dst_annot.SetData().SetFtable()
                .push_back(Ref(SerialClone(feat_it->GetOriginalFeature())));
        }
    }
    return entry;
}

bool sx_Equal(const CBioseq_Handle& bh1, const CBioseq_Handle& bh2)
{
    CRef<CSeq_descr> descr1 = sx_ExtractDescr(bh1);
    CRef<CSeq_descr> descr2 = sx_ExtractDescr(bh2);
    CRef<CSeq_entry> annot1 = sx_ExtractAnnot(bh1);
    CRef<CSeq_entry> annot2 = sx_ExtractAnnot(bh2);
    CConstRef<CBioseq> seq1 = bh1.GetCompleteBioseq();
    CConstRef<CBioseq> seq2 = bh2.GetCompleteBioseq();
    CRef<CBioseq> nseq1(SerialClone(*seq1));
    CRef<CBioseq> nseq2(SerialClone(*seq2));
    CRef<CSeq_id> gen1 = sx_ExtractGeneralId(*nseq1);
    CRef<CSeq_id> gen2 = sx_ExtractGeneralId(*nseq2);
    CRef<CDelta_ext> delta1 = sx_ExtractDelta(*nseq1);
    CRef<CDelta_ext> delta2 = sx_ExtractDelta(*nseq2);
    nseq1->ResetDescr();
    nseq2->ResetDescr();
    nseq1->ResetAnnot();
    nseq2->ResetAnnot();
    if ( !nseq1->Equals(*nseq2) ) {
        NcbiCout << "Seq-id: " << bh1.GetAccessSeq_id_Handle() << NcbiEndl;
        NcbiCout << "ERROR: Sequences do not match:\n"
                 << "WGS: " << MSerial_AsnText << *seq1
                 << "GB: " << MSerial_AsnText << *seq2;
        return false;
    }
    bool has_delta_error = false;
    bool has_state_error = false;
    bool report_state_error = false;
    bool has_id_error = false;
    bool report_id_error = false;
    bool has_descr_error = false;
    bool has_annot_error = false;
    if ( !delta1 != !delta2 ) {
        has_delta_error = true;
    }
    else if ( delta1 && !sx_EqualDelta(*delta1, *delta2) ) {
        has_delta_error = true;
    }
    if ( !gen1 && gen2 ) {
        // GB has general id but VDB hasn't
        has_id_error = report_id_error = true;
    }
    else if ( gen1 && !gen2 ) {
        // VDB has general id but GB hasn't
        // it's possible and shouldn't be considered as an error
        //has_id_error = report_id_error = true;
    }
    else if ( gen1 && !sx_EqualGeneralId(*gen1, *gen2) ) {
        has_id_error = true;
        report_id_error = GetReportGeneralIdError();
    }
    if ( !descr1->Equals(*descr2) ) {
        has_descr_error = true;
    }
    if ( !annot1->Equals(*annot2) ) {
        has_annot_error = true;
    }
    if ( bh1.GetState() != bh2.GetState() ) {
        has_state_error = true;
        report_state_error = GetReportSeqStateError();
    }
    if ( has_state_error || has_id_error ||
         has_descr_error || has_descr_error || has_annot_error ) {
        NcbiCout << "Seq-id: " << bh1.GetAccessSeq_id_Handle() << NcbiEndl;
    }
    if ( has_state_error ) {
        NcbiCout << (report_state_error? "ERROR": "WARNING")
                 << ": States do not match:"
                 << " WGS: " << bh1.GetState()
                 << " GB: " << bh2.GetState()
                 << NcbiEndl;
    }
    if ( has_id_error ) {
        NcbiCout << (report_id_error? "ERROR": "WARNING")
                 << ": General ids do no match:\n";
        NcbiCout << "Id1: ";
        if ( !gen1 ) {
            NcbiCout << "null\n";
        }
        else {
            NcbiCout << MSerial_AsnText << *gen1;
        }
        NcbiCout << "Id2: ";
        if ( !gen2 ) {
            NcbiCout << "null\n";
        }
        else {
            NcbiCout << MSerial_AsnText << *gen2;
        }
    }
    if ( has_descr_error ) {
        NcbiCout << "ERROR: Descriptors do no match:\n";
        NcbiCout << "WGS: " << MSerial_AsnText << *descr1;
        NcbiCout << "GB: " << MSerial_AsnText << *descr2;
    }
    if ( has_delta_error ) {
        NcbiCout << "ERROR: Delta sequences do not match:\n";
        NcbiCout << "WGS: ";
        if ( delta1 && delta2 ) {
            // report errors
            sx_EqualDelta(*delta1, *delta2, true);
        }
        if ( !delta1 ) {
            NcbiCout << "null\n";
        }
        else {
            NcbiCout << MSerial_AsnText << *delta1;
        }
        NcbiCout << "GB: ";
        if ( !delta2 ) {
            NcbiCout << "null\n";
        }
        else {
            NcbiCout << MSerial_AsnText << *delta2;
        }
    }
    if ( has_annot_error ) {
        NcbiCout << "ERROR: Annotations do no match:\n";
        NcbiCout << "WGS: " << MSerial_AsnText << *annot1;
        NcbiCout << "GB: " << MSerial_AsnText << *annot2;
    }
    return !report_id_error && !report_state_error &&
        !has_delta_error && !has_descr_error && !has_annot_error;
}

bool sx_EqualToGB(const CBioseq_Handle& bh)
{
    BOOST_REQUIRE(bh);
    CBioseq_Handle gb_bh = sx_LoadFromGB(bh);
    BOOST_REQUIRE(gb_bh);
    return sx_Equal(bh, gb_bh);
}

void sx_CheckNames(CScope& scope, const CSeq_loc& loc,
                   const string& name)
{
    SAnnotSelector sel;
    sel.SetSearchUnresolved();
    sel.SetCollectNames();
    CAnnotTypes_CI it(CSeq_annot::C_Data::e_not_set, scope, loc, &sel);
    CAnnotTypes_CI::TAnnotNames names = it.GetAnnotNames();
    ITERATE ( CAnnotTypes_CI::TAnnotNames, i, names ) {
        if ( i->IsNamed() ) {
            NcbiCout << "Named annot: " << i->GetName()
                     << NcbiEndl;
        }
        else {
            NcbiCout << "Unnamed annot"
                     << NcbiEndl;
        }
    }
    //NcbiCout << "Checking for name: " << name << NcbiEndl;
    BOOST_CHECK_EQUAL(names.count(name), 1u);
    if ( names.size() == 2 ) {
        BOOST_CHECK_EQUAL(names.count(name+PILEUP_NAME_SUFFIX), 1u);
    }
    else {
        BOOST_CHECK_EQUAL(names.size(), 1u);
    }
}

void sx_CheckSeq(CScope& scope,
                 const CSeq_id_Handle& main_idh,
                 const CSeq_id& id)
{
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(id);
    if ( idh == main_idh ) {
        return;
    }
    const CBioseq_Handle::TId& ids = scope.GetIds(idh);
    BOOST_REQUIRE_EQUAL(ids.size(), 1u);
    BOOST_CHECK_EQUAL(ids[0], idh);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
}

void sx_ReportState(const CBioseq_Handle& bsh, const CSeq_id_Handle& idh)
{
    if ( !bsh ) {
        cerr << "failed to find sequence: " << idh << endl;
        CBioseq_Handle::TBioseqStateFlags flags = bsh.GetState();
        if (flags & CBioseq_Handle::fState_suppress_temp) {
            cerr << "  suppressed temporarily" << endl;
        }
        if (flags & CBioseq_Handle::fState_suppress_perm) {
            cerr << "  suppressed permanently" << endl;
        }
        if (flags & CBioseq_Handle::fState_suppress) {
            cerr << "  suppressed" << endl;
        }
        if (flags & CBioseq_Handle::fState_dead) {
            cerr << "  dead" << endl;
        }
        if (flags & CBioseq_Handle::fState_confidential) {
            cerr << "  confidential" << endl;
        }
        if (flags & CBioseq_Handle::fState_withdrawn) {
            cerr << "  withdrawn" << endl;
        }
        if (flags & CBioseq_Handle::fState_no_data) {
            cerr << "  no data" << endl;
        }
        if (flags & CBioseq_Handle::fState_conflict) {
            cerr << "  conflict" << endl;
        }
        if (flags & CBioseq_Handle::fState_not_found) {
            cerr << "  not found" << endl;
        }
        if (flags & CBioseq_Handle::fState_other_error) {
            cerr << "  other error" << endl;
        }
    }
    else {
        cerr << "found sequence: " << idh << endl;
        //cerr << MSerial_AsnText << *bsh.GetCompleteBioseq();
    }
}

int sx_GetDescCount(const CBioseq_Handle& bh, CSeqdesc::E_Choice type)
{
    int ret = 0;
    for ( CSeqdesc_CI it(bh, type, 1); it; ++it ) {
        ++ret;
    }
    return ret;
}

BOOST_AUTO_TEST_CASE(FetchSeq1)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "AAAA01000102";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), CBioseq_Handle::fState_suppress_perm);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Molinfo), 1);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Pub), 0);
    BOOST_CHECK(sx_EqualToGB(bh));
}

BOOST_AUTO_TEST_CASE(FetchSeq2)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "AAAA02000102.1";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Molinfo), 1);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Pub), 0);
    BOOST_CHECK(sx_EqualToGB(bh));
}

BOOST_AUTO_TEST_CASE(FetchSeq3)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "ref|AAAA01000102";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), CBioseq_Handle::fState_suppress_perm);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Molinfo), 1);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Pub), 0);
}

BOOST_AUTO_TEST_CASE(FetchSeq4)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "ref|AAAA010000001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_CHECK(!bh);
}

BOOST_AUTO_TEST_CASE(FetchSeq5)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "ref|AAAA0100000001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_CHECK(!bh);
}

BOOST_AUTO_TEST_CASE(FetchSeq6)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "ref|ZZZZ01000001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_CHECK(!bh);
}


BOOST_AUTO_TEST_CASE(FetchSeq7)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    string id = "AAAA01010001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), CBioseq_Handle::fState_suppress_perm);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Molinfo), 1);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Pub), 2);
    BOOST_CHECK(sx_EqualToGB(bh));
}


BOOST_AUTO_TEST_CASE(FetchSeq8)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    string id = "ALWZ01S31652451";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Title), 0);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Pub), 1);
}

BOOST_AUTO_TEST_CASE(FetchSeq8_2)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    string id = "ALWZ02S4870253";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_not_set), 10);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Title), 0);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Pub), 3);
}

BOOST_AUTO_TEST_CASE(FetchSeq9)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    string id = "ALWZ0100000001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Title), 0);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Pub), 1);
    //BOOST_CHECK(sx_EqualToGB(bh)); no in GB
}

BOOST_AUTO_TEST_CASE(FetchSeq9_2)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    string id = "ALWZ020000001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_not_set), 10);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Title), 0);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Pub), 3);
    //BOOST_CHECK(sx_EqualToGB(bh)); no in GB
}

BOOST_AUTO_TEST_CASE(FetchSeq10)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    string id = "ALWZ010000001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_CHECK(!bh);
}


BOOST_AUTO_TEST_CASE(FetchSeq11)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    string id = "ACUJ01000001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Title), 1);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Pub), 0);
    BOOST_CHECK(sx_EqualToGB(bh));
}


BOOST_AUTO_TEST_CASE(FetchSeq12)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    string id = "ACUJ01000001";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Title), 1);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Pub), 2);
    BOOST_CHECK(sx_EqualToGB(bh));
}


BOOST_AUTO_TEST_CASE(FetchSeq13)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    string id = "ACUJ01000001.1";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Title), 1);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bh, CSeqdesc::e_Pub), 2);
}


BOOST_AUTO_TEST_CASE(FetchSeq14)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    string id = "ACUJ01000001.3";
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_CHECK(!bh);
}


BOOST_AUTO_TEST_CASE(FetchSeq15)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();
    scope->AddDataLoader("GBLOADER");

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle("gb|ABBA01000001.1|");
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);
    BOOST_CHECK(sx_EqualToGB(bsh));
}


BOOST_AUTO_TEST_CASE(FetchSeq16)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle("JRAS01000001");
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);
    BOOST_CHECK(sx_EqualToGB(bsh));
}


BOOST_AUTO_TEST_CASE(FetchSeq17)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle("AVCP010000001");
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_Title), 1);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_Molinfo), 1);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_Source), 1);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_Create_date), 1);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_Update_date), 1);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_Pub), 2);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_User), 2);
}


BOOST_AUTO_TEST_CASE(FetchSeq18)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle("CCMW01000001");
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(!bsh);
}


BOOST_AUTO_TEST_CASE(FetchSeq19)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle("CCMW01000002");
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);
    BOOST_CHECK(sx_EqualToGB(bsh));
}


BOOST_AUTO_TEST_CASE(FetchSeq20)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle("GAAA01000001");
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);
    BOOST_CHECK(sx_EqualToGB(bsh));
}


const char* s_ProteinFile = 
                    "/net/snowman/vol/export2/dondosha/SVN64/trunk/internal/c++/src/internal/ID/WGS/XXXX01";

BOOST_AUTO_TEST_CASE(FetchSeq21)
{
    if ( !CDirEntry(s_ProteinFile).Exists() ) {
        return;
    }
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr, s_ProteinFile);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle("AXXX01000001");
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_not_set), 14);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_Pub), 4);
}


BOOST_AUTO_TEST_CASE(FetchSeq22)
{
    if ( !CDirEntry(s_ProteinFile).Exists() ) {
        return;
    }
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr, s_ProteinFile);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle("AXXX01S000001");
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(!bsh);
}


BOOST_AUTO_TEST_CASE(FetchSeq23)
{
    if ( !CDirEntry(s_ProteinFile).Exists() ) {
        return;
    }
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr, s_ProteinFile);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle("AXXX01P000001");
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    sx_ReportState(bsh, idh);
    BOOST_REQUIRE(bsh);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_not_set), 9);
    BOOST_CHECK_EQUAL(sx_GetDescCount(bsh, CSeqdesc::e_Pub), 2);
    BOOST_CHECK_EQUAL(CFeat_CI(bsh.GetParentEntry()).GetSize(), 1);
}


static const bool s_MakeFasta = 0;


BOOST_AUTO_TEST_CASE(Scaffold2Fasta)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    CStopWatch sw(CStopWatch::eStart);
    CScope::TIds ids;
    {{
        CVDBMgr mgr;
        CWGSDb wgs_db(mgr, "ALWZ01");
        size_t limit_count = 30000, start_row = 1, count = 0;
        for ( CWGSScaffoldIterator it(wgs_db, start_row); it; ++it ) {
            ++count;
            if (limit_count > 0 && count > limit_count)
                break;
            ids.push_back(CSeq_id_Handle::GetHandle(*it.GetAccSeq_id()));
        }
    }}

    CScope scope(*om);
    scope.AddDefaults();

    auto_ptr<CNcbiOstream> out;
    auto_ptr<CFastaOstream> fasta;

    if ( s_MakeFasta ) {
        string outfile_name = "out.fsa";
        out.reset(new CNcbiOfstream(outfile_name.c_str()));
        fasta.reset(new CFastaOstream(*out));
    }

    ITERATE ( CScope::TIds, it, ids ) {
        //scope.ResetHistory();
        CBioseq_Handle scaffold = scope.GetBioseqHandle(*it);
        if ( fasta.get() ) {
            fasta->Write(scaffold);
        }
    }
    NcbiCout << "Scanned in "<<sw.Elapsed() << NcbiEndl;
}


BOOST_AUTO_TEST_CASE(StateTest)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);
    CScope scope(*om);
    scope.AddDefaults();

    CBioseq_Handle bh;

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("CDBB01000001"));
    BOOST_REQUIRE(bh);
    /*
    NcbiCout << bh.GetAccessSeq_id_Handle() << ": "
             << bh.GetState() << " vs " << sx_LoadFromGB(bh).GetState()
             << NcbiEndl;
    */
    BOOST_CHECK(sx_Equal(bh, sx_LoadFromGB(bh)));

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AFFP01000011"));
    BOOST_REQUIRE(bh);
    /*
    NcbiCout << bh.GetAccessSeq_id_Handle() << ": "
             << bh.GetState() << " vs " << sx_LoadFromGB(bh).GetState()
             << NcbiEndl;
    */
    BOOST_CHECK(sx_Equal(bh, sx_LoadFromGB(bh)));

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("JPNT01000001"));
    BOOST_REQUIRE(bh);
    /*
    NcbiCout << bh.GetAccessSeq_id_Handle() << ": "
             << bh.GetState() << " vs " << sx_LoadFromGB(bh).GetState()
             << NcbiEndl;
    */
    BOOST_CHECK(sx_Equal(bh, sx_LoadFromGB(bh)));
}


BOOST_AUTO_TEST_CASE(Scaffold2Fasta2)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    CStopWatch sw(CStopWatch::eStart);
    CScope::TIds ids;
    {{
        CVDBMgr mgr;
        CWGSDb wgs_db(mgr, "ALWZ02");
        size_t limit_count = 30000, start_row = 1, count = 0;
        for ( CWGSScaffoldIterator it(wgs_db, start_row); it; ++it ) {
            ++count;
            if (limit_count > 0 && count > limit_count)
                break;
            ids.push_back(CSeq_id_Handle::GetHandle(*it.GetAccSeq_id()));
        }
    }}

    CScope scope(*om);
    scope.AddDefaults();

    auto_ptr<CNcbiOstream> out;
    auto_ptr<CFastaOstream> fasta;

    if ( s_MakeFasta ) {
        string outfile_name = "out.fsa";
        out.reset(new CNcbiOfstream(outfile_name.c_str()));
        fasta.reset(new CFastaOstream(*out));
    }

    CScope::TBioseqHandles handles = scope.GetBioseqHandles(ids);
    ITERATE ( CScope::TBioseqHandles, it, handles ) {
        CBioseq_Handle scaffold = *it;
        if ( fasta.get() ) {
            fasta->Write(scaffold);
        }
    }

    NcbiCout << "Scanned in "<<sw.Elapsed() << NcbiEndl;
}


BOOST_AUTO_TEST_CASE(WithdrawnCheck)
{
    CRef<CObjectManager> om = sx_InitOM(eWithMasterDescr);
    CScope scope(*om);
    scope.AddDefaults();

    CBioseq_Handle bh;

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AFFP02000011.1"));
    BOOST_CHECK(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    BOOST_CHECK(sx_Equal(bh, sx_LoadFromGB(bh)));

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AFFP01000011.1"));
    BOOST_CHECK(bh);
    BOOST_CHECK_EQUAL(bh.GetState(),
                      CBioseq_Handle::fState_suppress_perm);
    BOOST_CHECK(sx_Equal(bh, sx_LoadFromGB(bh)));

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AFFP01000012.1"));
    BOOST_CHECK(!bh);
    BOOST_CHECK_EQUAL(bh.GetState(),
                      CBioseq_Handle::fState_no_data |
                      CBioseq_Handle::fState_withdrawn);
}


#if defined(NCBI_OS_DARWIN) || (defined(NCBI_OS_LINUX) && SIZEOF_VOIDP == 4)
# define PAN1_PATH "/net/pan1"
#else
# define PAN1_PATH "//panfs/pan1"
#endif

BOOST_AUTO_TEST_CASE(TPGTest)
{
    CRef<CObjectManager> om = sx_GetEmptyOM();
    CScope scope(*om);

    string wgs_root = PAN1_PATH "/id_dumps/WGS/tmp";
    CWGSDataLoader::RegisterInObjectManager(*om,  wgs_root, vector<string>(), CObjectManager::eDefault);
    scope.AddDefaults();

    CBioseq_Handle bh;

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("DAAH01000001.1"));
    BOOST_CHECK(bh);
    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("DAAG01000001.1"));
    BOOST_CHECK(bh);
    //bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AGKB01000001.1"));
    //BOOST_CHECK(!bh);
    //bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AAAA01000001.1"));
    //BOOST_CHECK(!bh);
}


BOOST_AUTO_TEST_CASE(FixedFileTest)
{
    CRef<CObjectManager> om = sx_GetEmptyOM();
    CScope scope(*om);

    vector<string> files;
    string wgs_root = PAN1_PATH "/id_dumps/WGS/tmp";
    files.push_back(wgs_root+"/DAAH01");
    CWGSDataLoader::RegisterInObjectManager(*om,  "", files, CObjectManager::eDefault);
    scope.AddDefaults();

    CBioseq_Handle bh;

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("DAAH01000001.1"));
    BOOST_CHECK(bh);
    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("DAAG01000001.1"));
    BOOST_CHECK(!bh);
    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AGKB01000001.1"));
    BOOST_CHECK(!bh);
    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AAAA01000001.1"));
    BOOST_CHECK(!bh);
}


BOOST_AUTO_TEST_CASE(QualityTest)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);
    CScope scope(*om);

    scope.AddDefaults();

    CBioseq_Handle bh;

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("ABYI02000001.1"));
    BOOST_REQUIRE(bh);
    CGraph_CI graph_it(bh, SAnnotSelector().AddNamedAnnots("Phrap Graph"));
    BOOST_CHECK_EQUAL(graph_it.GetSize(), 1u);
    CFeat_CI feat_it(bh);
    BOOST_CHECK_EQUAL(feat_it.GetSize(), 116u);
}


BOOST_AUTO_TEST_CASE(GITest)
{
    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);
    CScope scope(*om);

    scope.AddDefaults();

    CBioseq_Handle bh;

    //bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AABR01000100.1"));
    //BOOST_CHECK(bh);

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetGiHandle(TIntId(25725721)));
    BOOST_CHECK(bh);

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetGiHandle(TIntId(2)));
    BOOST_CHECK(!bh);
}


BOOST_AUTO_TEST_CASE(HashTest)
{
    CSeq_id_Handle id = CSeq_id_Handle::GetHandle("BBXB01000080.1");

    CRef<CObjectManager> om = sx_InitOM(eWithoutMasterDescr);

    CScope wgs_scope(*om);
    wgs_scope.AddDefaults();

    int wgs_hash = wgs_scope.GetSequenceHash(id);
    BOOST_CHECK(wgs_hash != 0);

    sx_InitGBLoader(*om);
    CScope gb_scope(*om);
    gb_scope.AddDataLoader("GBLOADER");
    
    int gb_hash = gb_scope.GetSequenceHash(id);
    BOOST_CHECK(gb_hash != 0);

    BOOST_CHECK_EQUAL(wgs_hash, gb_hash);
}
