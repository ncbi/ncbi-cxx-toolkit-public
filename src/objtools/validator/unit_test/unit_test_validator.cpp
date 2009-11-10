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
* Author:  Pavel Ivanov, NCBI
*
* File Description:
*   Sample unit tests file for main stream test developing.
*
* This file represents basic most common usage of Ncbi.Test framework based
* on Boost.Test framework. For more advanced techniques look into another
* sample - unit_test_alt_sample.cpp.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>

// This macro should be defined before inclusion of test_boost.hpp in all
// "*.cpp" files inside executable except one. It is like function main() for
// non-Boost.Test executables is defined only in one *.cpp file - other files
// should not include it. If NCBI_BOOST_NO_AUTO_TEST_MAIN will not be defined
// then test_boost.hpp will define such "main()" function for tests.
//
// Usually if your unit tests contain only one *.cpp file you should not
// care about this macro at all.
//
//#define NCBI_BOOST_NO_AUTO_TEST_MAIN


// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Ref_ext.hpp>
#include <objects/seq/Map_ext.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/Seq_gap.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/seqloc/Giimport_id.hpp>
#include <objects/seqloc/Patent_seq_id.hpp>
#include <objects/biblio/Id_pat.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objtools/validator/validator.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>


USING_NCBI_SCOPE;
USING_SCOPE(objects);

using namespace validator;

extern const char* sc_TestEntryCollidingLocusTags;

class CExpectedError
{
public:
    CExpectedError(string accession, EDiagSev severity, string err_code, string err_msg);
    ~CExpectedError();

    void Test(const CValidErrItem& err_item);

    void SetAccession (string accession) { m_Accession = accession; }
    void SetSeverity (EDiagSev severity) { m_Severity = severity; }
    void SetErrCode (string err_code) { m_ErrCode = err_code; }
    void SetErrMsg (string err_msg) { m_ErrMsg = err_msg; }

    const string& GetErrMsg(void) { return m_ErrMsg; }

private:
    string m_Accession;
    EDiagSev m_Severity;
    string m_ErrCode;
    string m_ErrMsg;
};

CExpectedError::CExpectedError(string accession, EDiagSev severity, string err_code, string err_msg) 
: m_Accession (accession), m_Severity (severity), m_ErrCode(err_code), m_ErrMsg(err_msg)
{

}


CExpectedError::~CExpectedError()
{
}


void CExpectedError::Test(const CValidErrItem& err_item)
{
    BOOST_CHECK_EQUAL(err_item.GetAccession(), m_Accession);
    BOOST_CHECK_EQUAL(err_item.GetSeverity(), m_Severity);
    BOOST_CHECK_EQUAL(err_item.GetErrCode(), m_ErrCode);
    BOOST_CHECK_EQUAL(err_item.GetMsg(), m_ErrMsg);
}


static void CheckErrors (const CValidError& eval, vector< CExpectedError* >& expected_errors)
{
    size_t err_pos = 0;

    for ( CValidError_CI vit(eval); vit; ++vit) {
        while (err_pos < expected_errors.size() && !expected_errors[err_pos]) {
            ++err_pos;
        }
        if (err_pos < expected_errors.size()) {
            expected_errors[err_pos]->Test(*vit);
            ++err_pos;
        } else {
            string description =  vit->GetAccession() + ":"
                + CValidErrItem::ConvertSeverity(vit->GetSeverity()) + ":"
                + vit->GetErrCode() + ":"
                + vit->GetMsg();
            BOOST_CHECK_EQUAL(description, "Unexpected error");
        }
    }
    while (err_pos < expected_errors.size()) {
        if (expected_errors[err_pos]) {
            BOOST_CHECK_EQUAL(expected_errors[err_pos]->GetErrMsg(), "Expected error not found");
        }
        ++err_pos;
    }
}


static CRef<CSeq_entry> BuildGoodSeq(void)
{
    CRef<CBioseq> seq(new CBioseq());
    seq->SetInst().SetMol(CSeq_inst::eMol_dna);
    seq->SetInst().SetRepr(CSeq_inst::eRepr_raw);
    seq->SetInst().SetSeq_data().SetIupacna().Set("AATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAA");
    seq->SetInst().SetLength(60);

    CRef<CSeq_id> id(new CSeq_id());
    id->SetLocal().SetStr ("good");
    seq->SetId().push_back(id);

    CRef<CSeqdesc> mdesc(new CSeqdesc());
    mdesc->SetMolinfo().SetBiomol(CMolInfo::eBiomol_genomic);    
    seq->SetDescr().Set().push_back(mdesc);
    CRef<CSeqdesc> pdesc(new CSeqdesc());
    CRef<CPub> pub(new CPub());
    pub->SetPmid((CPub::TPmid)1);
    pdesc->SetPub().SetPub().Set().push_back(pub);
    seq->SetDescr().Set().push_back(pdesc);
    CRef<CSeqdesc> odesc(new CSeqdesc());
    odesc->SetSource().SetOrg().SetTaxname("Sebaea microphylla");
    odesc->SetSource().SetOrg().SetOrgname().SetLineage("some lineage");
    CRef<CDbtag> taxon_id(new CDbtag());
    taxon_id->SetDb("taxon");
    taxon_id->SetTag().SetId(592768);
    odesc->SetSource().SetOrg().SetDb().push_back(taxon_id);
    CRef<CSubSource> subsrc(new CSubSource());
    subsrc->SetSubtype(CSubSource::eSubtype_chromosome);
    subsrc->SetName("1");
    odesc->SetSource().SetSubtype().push_back(subsrc);
    seq->SetDescr().Set().push_back(odesc);

    CRef<CSeq_entry> entry(new CSeq_entry());
    entry->SetSeq(*seq);

    return entry;
}


static CRef<CSeq_entry> BuildGoodProtSeq(void)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();

    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_aa);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("PROTEIN");
    entry->SetSeq().SetInst().SetLength(7);
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsMolinfo()) {
            (*it)->SetMolinfo().SetBiomol(CMolInfo::eBiomol_peptide);
        }
    }

    CRef<CSeq_feat> feat (new CSeq_feat());
    feat->SetData().SetProt().SetName().push_back("fake protein name");
    feat->SetLocation().SetInt().SetId().SetLocal().SetStr("good");
    feat->SetLocation().SetInt().SetFrom(0);
    feat->SetLocation().SetInt().SetTo(6);
    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetFtable().push_back(feat);
    entry->SetSeq().SetAnnot().push_back(annot);

    return entry;
}


static CRef<CSeq_entry> BuildGoodNucProtSet(void)
{
    CRef<CBioseq_set> set(new CBioseq_set());
    set->SetClass(CBioseq_set::eClass_nuc_prot);
    CRef<CSeqdesc> pdesc(new CSeqdesc());
    CRef<CPub> pub(new CPub());
    pub->SetPmid((CPub::TPmid)1);
    pdesc->SetPub().SetPub().Set().push_back(pub);
    set->SetDescr().Set().push_back(pdesc);
    CRef<CSeqdesc> odesc(new CSeqdesc());
    odesc->SetSource().SetOrg().SetTaxname("Sebaea microphylla");
    odesc->SetSource().SetOrg().SetOrgname().SetLineage("some lineage");
    CRef<CDbtag> taxon_id(new CDbtag());
    taxon_id->SetDb("taxon");
    taxon_id->SetTag().SetId(592768);
    odesc->SetSource().SetOrg().SetDb().push_back(taxon_id);
    CRef<CSubSource> subsrc(new CSubSource());
    subsrc->SetSubtype(CSubSource::eSubtype_chromosome);
    subsrc->SetName("1");
    odesc->SetSource().SetSubtype().push_back(subsrc);
    set->SetDescr().Set().push_back(odesc);

    // make nucleotide
    CRef<CBioseq> nseq(new CBioseq());
    nseq->SetInst().SetMol(CSeq_inst::eMol_dna);
    nseq->SetInst().SetRepr(CSeq_inst::eRepr_raw);
    nseq->SetInst().SetSeq_data().SetIupacna().Set("ATGCCCAGAAAAACAGAGATAAACTAAGGGATGCCCAGAAAAACAGAGATAAACTAAGGG");
    nseq->SetInst().SetLength(60);

    CRef<CSeq_id> id(new CSeq_id());
    id->SetLocal().SetStr ("nuc");
    nseq->SetId().push_back(id);

    CRef<CSeqdesc> mdesc(new CSeqdesc());
    mdesc->SetMolinfo().SetBiomol(CMolInfo::eBiomol_genomic);    
    nseq->SetDescr().Set().push_back(mdesc);

    CRef<CSeq_entry> nentry(new CSeq_entry());
    nentry->SetSeq(*nseq);

    set->SetSeq_set().push_back(nentry);

    // make protein
    CRef<CBioseq> pseq(new CBioseq());
    pseq->SetInst().SetMol(CSeq_inst::eMol_aa);
    pseq->SetInst().SetRepr(CSeq_inst::eRepr_raw);
    pseq->SetInst().SetSeq_data().SetIupacaa().Set("MPROTEIN");
    pseq->SetInst().SetLength(8);

    CRef<CSeq_id> pid(new CSeq_id());
    pid->SetLocal().SetStr ("prot");
    pseq->SetId().push_back(pid);

    CRef<CSeqdesc> mpdesc(new CSeqdesc());
    mpdesc->SetMolinfo().SetBiomol(CMolInfo::eBiomol_peptide);    
    pseq->SetDescr().Set().push_back(mpdesc);

    CRef<CSeq_feat> feat (new CSeq_feat());
    feat->SetData().SetProt().SetName().push_back("fake protein name");
    feat->SetLocation().SetInt().SetId().SetLocal().SetStr("prot");
    feat->SetLocation().SetInt().SetFrom(0);
    feat->SetLocation().SetInt().SetTo(7);
    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetFtable().push_back(feat);
    pseq->SetAnnot().push_back(annot);

    CRef<CSeq_entry> pentry(new CSeq_entry());
    pentry->SetSeq(*pseq);

    set->SetSeq_set().push_back(pentry);

    CRef<CSeq_feat> cds (new CSeq_feat());
    cds->SetData().SetCdregion();
    cds->SetProduct().SetWhole().SetLocal().SetStr("prot");
    cds->SetLocation().SetInt().SetId().SetLocal().SetStr("nuc");
    cds->SetLocation().SetInt().SetFrom(0);
    cds->SetLocation().SetInt().SetTo(26);
    CRef<CSeq_annot> set_annot(new CSeq_annot());
    set_annot->SetData().SetFtable().push_back(cds);
    set->SetAnnot().push_back(set_annot);

    CRef<CSeq_entry> set_entry(new CSeq_entry());
    set_entry->SetSet(*set);
    return set_entry;
}


static CRef<CSeq_entry> BuildGoodDeltaSeq(void)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();

    entry->SetSeq().SetInst().ResetSeq_data();
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_delta);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral("ATGATGATG", CSeq_inst::eMol_dna);
    CRef<CDelta_seq> gap_seg(new CDelta_seq());
    gap_seg->SetLiteral().SetSeq_data().SetGap();
    gap_seg->SetLiteral().SetLength(10);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(gap_seg);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral("ATGATGATG", CSeq_inst::eMol_dna);
    entry->SetSeq().SetInst().SetLength(28);

    return entry;
}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_ExtNotAllowed)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "ExtNotAllowed", "Bioseq-ext not allowed on virtual Bioseq"));

    // repr = virtual
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_virtual);
    entry->SetSeq().SetInst().ResetSeq_data();
    entry->SetSeq().SetInst().SetExt().SetDelta();
    CConstRef<CValidError> eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // repr = raw
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_raw);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AATTGG");
    expected_errors[0]->SetErrMsg("Bioseq-ext not allowed on raw Bioseq");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().ResetExt();
    entry->SetSeq().SetInst().ResetSeq_data();
    expected_errors[0]->SetErrCode("SeqDataNotFound");
    expected_errors[0]->SetErrMsg("Missing Seq-data on raw Bioseq");
    expected_errors[0]->SetSeverity(eDiag_Critical);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetSeq_data().SetGap();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // repr = const
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_const);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AATTGG");
    entry->SetSeq().SetInst().SetExt().SetDelta();
    expected_errors[0]->SetErrCode("ExtNotAllowed");
    expected_errors[0]->SetErrMsg("Bioseq-ext not allowed on constructed Bioseq");
    expected_errors[0]->SetSeverity(eDiag_Error);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().ResetExt();
    entry->SetSeq().SetInst().ResetSeq_data();
    expected_errors[0]->SetErrCode("SeqDataNotFound");
    expected_errors[0]->SetErrMsg("Missing Seq-data on constructed Bioseq");
    expected_errors[0]->SetSeverity(eDiag_Critical);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetSeq_data().SetGap();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // repr = map
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_map);
    entry->SetSeq().SetInst().ResetSeq_data();
    expected_errors[0]->SetErrCode("ExtBadOrMissing");
    expected_errors[0]->SetErrMsg("Missing or incorrect Bioseq-ext on map Bioseq");
    expected_errors[0]->SetSeverity(eDiag_Error);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetExt().SetDelta();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetExt().SetRef();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetExt().SetMap();
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AATTGG");
    expected_errors[0]->SetErrCode("SeqDataNotAllowed");
    expected_errors[0]->SetErrMsg("Seq-data not allowed on map Bioseq");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);


    // repr = ref
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_ref);
    entry->SetSeq().SetInst().ResetExt();
    entry->SetSeq().SetInst().ResetSeq_data();
    expected_errors[0]->SetErrCode("ExtBadOrMissing");
    expected_errors[0]->SetErrMsg("Missing or incorrect Bioseq-ext on reference Bioseq");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // repr = seg
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_seg);
    expected_errors[0]->SetErrMsg("Missing or incorrect Bioseq-ext on seg Bioseq");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // repr = consen
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_consen);
    expected_errors[0]->SetSeverity(eDiag_Critical);
    expected_errors[0]->SetErrCode("ReprInvalid");
    expected_errors[0]->SetErrMsg("Invalid Bioseq->repr = 6");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // repr = notset
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_not_set);
    expected_errors[0]->SetErrMsg("Invalid Bioseq->repr = 0");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // repr = other
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_other);
    expected_errors[0]->SetErrMsg("Invalid Bioseq->repr = 255");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // repr = delta
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_delta);
    entry->SetSeq().SetInst().SetExt().SetDelta();
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AATTGG");
    expected_errors[0]->SetSeverity(eDiag_Error);
    expected_errors[0]->SetErrCode("SeqDataNotAllowed");
    expected_errors[0]->SetErrMsg("Seq-data not allowed on delta Bioseq");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().ResetExt();
    entry->SetSeq().SetInst().ResetSeq_data();
    expected_errors[0]->SetSeverity(eDiag_Error);
    expected_errors[0]->SetErrCode("ExtBadOrMissing");
    expected_errors[0]->SetErrMsg("Missing or incorrect Bioseq-ext on delta Bioseq");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // free errors
    for (size_t i = 0; i < expected_errors.size(); i++) {
        if (expected_errors[i]) {
            delete expected_errors[i];
        }
    }
}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_ReprInvalid)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "ReprInvalid", "Invalid Bioseq->repr = 0"));

    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_not_set);
    CConstRef<CValidError> eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    expected_errors[0]->SetErrMsg("Invalid Bioseq->repr = 255");
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_other);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    expected_errors[0]->SetErrMsg("Invalid Bioseq->repr = 6");
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_consen);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    delete expected_errors[0];
    expected_errors.clear();
}


BOOST_AUTO_TEST_CASE(Test_CollidingLocusTags)
{
    CSeq_entry entry;
    {{
         CNcbiIstrstream istr(sc_TestEntryCollidingLocusTags);
         istr >> MSerial_AsnText >> entry;
     }}

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(entry);

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("LocusCollidesWithLocusTag", eDiag_Warning, "TerminalNs", "N at end of sequence"));
    expected_errors.push_back(new CExpectedError("LocusCollidesWithLocusTag", eDiag_Warning, "CollidingGeneNames", "Colliding names in gene features"));
    expected_errors.push_back(new CExpectedError("LocusCollidesWithLocusTag", eDiag_Warning, "CollidingGeneNames", "Colliding names in gene features"));
    expected_errors.push_back(new CExpectedError("LocusCollidesWithLocusTag", eDiag_Error, "CollidingLocusTags", "Colliding locus_tags in gene features"));
    expected_errors.push_back(new CExpectedError("LocusCollidesWithLocusTag", eDiag_Error, "CollidingLocusTags", "Colliding locus_tags in gene features"));
    expected_errors.push_back(new CExpectedError("LocusCollidesWithLocusTag", eDiag_Error, "CollidingLocusTags", "Colliding locus_tags in gene features"));
    expected_errors.push_back(new CExpectedError("LocusCollidesWithLocusTag", eDiag_Error, "NoMolInfoFound", "No Mol-info applies to this Bioseq"));
    expected_errors.push_back(new CExpectedError("LocusCollidesWithLocusTag", eDiag_Error, "LocusTagProblem", "Gene locus and locus_tag 'foo' match"));
    expected_errors.push_back(new CExpectedError("LocusCollidesWithLocusTag", eDiag_Warning, "LocusCollidesWithLocusTag", "locus collides with locus_tag in another gene"));
    expected_errors.push_back(new CExpectedError("", eDiag_Error, "NoPubFound", "No publications anywhere on this entire record."));
    expected_errors.push_back(new CExpectedError("", eDiag_Error, "NoOrgFound", "No organism name anywhere on this entire record."));

    CConstRef<CValidError> eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // free errors
    for (size_t i = 0; i < expected_errors.size(); i++) {
        if (expected_errors[i]) {
            delete expected_errors[i];
        }
    }
}


const char* sc_TestEntryCollidingLocusTags ="Seq-entry ::= seq {\
    id {\
      local str \"LocusCollidesWithLocusTag\" } ,\
    inst {\
      repr raw ,\
      mol dna ,\
      length 24 ,\
      seq-data\
        iupacna \"AATTGGCCAANNAATTGGCCAANN\" } ,\
    annot {\
      {\
        data\
          ftable {\
            {\
              data\
                gene {\
                  locus \"foo\" ,\
                  locus-tag \"foo\" } ,\
              location\
                int {\
                  from 0 ,\
                  to 4 ,\
                  strand plus ,\
                  id\
                    local str \"LocusCollidesWithLocusTag\" } } ,\
            {\
              data\
                gene {\
                  locus \"bar\" ,\
                  locus-tag \"foo\" } ,\
              location\
                int {\
                  from 5 ,\
                  to 9 ,\
                  strand plus ,\
                  id\
                    local str \"LocusCollidesWithLocusTag\" } } ,\
            {\
              data\
                gene {\
                  locus \"bar\" ,\
                  locus-tag \"baz\" } ,\
              location\
                int {\
                  from 10 ,\
                  to 14 ,\
                  strand plus ,\
                  id\
                    local str \"LocusCollidesWithLocusTag\" } } ,\
            {\
              data\
                gene {\
                  locus \"quux\" ,\
                  locus-tag \"baz\" } ,\
              location\
                int {\
                  from 15 ,\
                  to 19 ,\
                  strand plus ,\
                  id\
                    local str \"LocusCollidesWithLocusTag\" } } } } } }\
";


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_CircularProtein)
{
    CRef<CSeq_entry> entry = BuildGoodProtSeq();

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "CircularProtein", "Non-linear topology set on protein"));

    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsMolinfo()) {
            (*it)->SetMolinfo().SetCompleteness(CMolInfo::eCompleteness_complete);
        }
    }


    entry->SetSeq().SetInst().SetTopology(CSeq_inst::eTopology_circular);
    CConstRef<CValidError> eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetTopology(CSeq_inst::eTopology_tandem);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetTopology(CSeq_inst::eTopology_other);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // should be no error for not set or linear
    delete expected_errors[0];
    expected_errors.clear();
    entry->SetSeq().SetInst().SetTopology(CSeq_inst::eTopology_not_set);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetTopology(CSeq_inst::eTopology_linear);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_DSProtein)
{
    CRef<CSeq_entry> entry = BuildGoodProtSeq();

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "DSProtein", "Protein not single stranded"));

    entry->SetSeq().SetInst().SetStrand(CSeq_inst::eStrand_ds);
    CConstRef<CValidError> eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetStrand(CSeq_inst::eStrand_mixed);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetStrand(CSeq_inst::eStrand_other);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // no errors expected for not set or single strand
    delete expected_errors[0];
    expected_errors.clear();

    entry->SetSeq().SetInst().SetStrand(CSeq_inst::eStrand_not_set);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetStrand(CSeq_inst::eStrand_ss);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_MolNotSet)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "MolNotSet", "Bioseq.mol is 0"));

    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_not_set);
    CConstRef<CValidError> eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    expected_errors[0]->SetErrCode("MolOther");
    expected_errors[0]->SetErrMsg("Bioseq.mol is type other");
    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_other);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    expected_errors[0]->SetErrCode("MolNuclAcid");
    expected_errors[0]->SetErrMsg("Bioseq.mol is type na");
    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_na);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    delete expected_errors[0];
    expected_errors.clear();

}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_FuzzyLen)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "FuzzyLen", "Fuzzy length on raw Bioseq"));

    entry->SetSeq().SetInst().SetFuzz();
    CConstRef<CValidError> eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    expected_errors[0]->SetErrMsg("Fuzzy length on const Bioseq");
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_const);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // shouldn't get fuzzy length if gap
    expected_errors[0]->SetErrCode("SeqDataNotFound");
    expected_errors[0]->SetErrMsg("Missing Seq-data on constructed Bioseq");
    expected_errors[0]->SetSeverity(eDiag_Critical);
    entry->SetSeq().SetInst().SetSeq_data().SetGap();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    delete expected_errors[0];
    expected_errors.clear();
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_InvalidAlphabet)
{
    CRef<CSeq_entry> prot_entry = BuildGoodProtSeq();

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle prot_seh = scope.AddTopLevelSeqEntry(*prot_entry);

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidAlphabet", "Using a nucleic acid alphabet on a protein sequence"));
    prot_entry->SetSeq().SetInst().SetSeq_data().SetIupacna();
    CConstRef<CValidError> eval = validator.Validate(prot_seh, options);
    CheckErrors (*eval, expected_errors);

    prot_entry->SetSeq().SetInst().SetSeq_data().SetNcbi2na();
    eval = validator.Validate(prot_seh, options);
    CheckErrors (*eval, expected_errors);

    prot_entry->SetSeq().SetInst().SetSeq_data().SetNcbi4na();
    eval = validator.Validate(prot_seh, options);
    CheckErrors (*eval, expected_errors);

    prot_entry->SetSeq().SetInst().SetSeq_data().SetNcbi8na();
    eval = validator.Validate(prot_seh, options);
    CheckErrors (*eval, expected_errors);

    prot_entry->SetSeq().SetInst().SetSeq_data().SetNcbipna();
    eval = validator.Validate(prot_seh, options);
    CheckErrors (*eval, expected_errors);

    CRef<CSeq_entry> entry = BuildGoodSeq();
    CScope scope2(*objmgr);
    scope2.AddDefaults();
    CSeq_entry_Handle seh = scope2.AddTopLevelSeqEntry(*entry);

    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa();
    expected_errors[0]->SetErrMsg("Using a protein alphabet on a nucleic acid");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetSeq_data().SetNcbi8aa();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetSeq_data().SetNcbieaa();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetSeq_data().SetNcbipaa();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetSeq_data().SetNcbistdaa();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    delete expected_errors[0];
    expected_errors.clear();
}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_InvalidResidue)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ");
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back(251);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back(251);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back(251);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back(252);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back(252);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back(252);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back(253);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back(253);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back(253);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back(254);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back(254);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back(255);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back(255);
    entry->SetSeq().SetInst().SetLength(65);
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'E' at position [5]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'F' at position [6]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'I' at position [9]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'J' at position [10]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'L' at position [12]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'O' at position [15]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'P' at position [16]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'Q' at position [17]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'U' at position [21]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'X' at position [24]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'Z' at position [26]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'E' at position [31]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'F' at position [32]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'I' at position [35]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'J' at position [36]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'L' at position [38]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'O' at position [41]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'P' at position [42]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'Q' at position [43]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'U' at position [47]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'X' at position [50]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid nucleotide residue 'Z' at position [52]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [251] at position [53]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [251] at position [54]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [251] at position [55]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [252] at position [56]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [252] at position [57]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [252] at position [58]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [253] at position [59]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [253] at position [60]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [253] at position [61]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [254] at position [62]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "More than 10 invalid residues. Checking stopped"));

    CConstRef<CValidError> eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // now repeat test, but with mRNA - this time Us should not be reported
    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);
    delete expected_errors[8];
    expected_errors[8] = NULL;
    delete expected_errors[19];
    expected_errors[19] = NULL;
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // now repeat test, but with protein
    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_aa);
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsMolinfo()) {
            (*it)->SetMolinfo().SetBiomol(CMolInfo::eBiomol_peptide);
        }
    }
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ");
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set().push_back(251);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set().push_back(251);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set().push_back(251);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set().push_back(252);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set().push_back(252);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set().push_back(252);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set().push_back(253);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set().push_back(253);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set().push_back(253);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set().push_back(254);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set().push_back(254);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set().push_back(255);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set().push_back(255);
    entry->SetSeq().SetInst().SetLength(65);
    CRef<CSeq_feat> feat (new CSeq_feat());
    feat->SetData().SetProt().SetName().push_back("fake protein name");
    feat->SetLocation().SetInt().SetId().SetLocal().SetStr("good");
    feat->SetLocation().SetInt().SetFrom(0);
    feat->SetLocation().SetInt().SetTo(64);
    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetFtable().push_back(feat);
    entry->SetSeq().SetAnnot().push_back(annot);
    scope.RemoveEntry (*entry);
    seh = scope.AddTopLevelSeqEntry(*entry);

    for (int j = 0; j < 22; j++) {
        if (expected_errors[j] != NULL) {
            delete expected_errors[j];
            expected_errors[j] = NULL;
        }
    }
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

    // now look for lowercase characters
    scope.RemoveEntry (*entry);
    entry = BuildGoodSeq();
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("abcdefghijklmnopqrstuvwxyz");
    entry->SetSeq().SetInst().SetLength(26);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Sequence contains lower-case characters"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveEntry (*entry);
    entry = BuildGoodProtSeq();
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("protein");
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);


    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

    // now try delta sequence
    scope.RemoveEntry (*entry);
    entry = BuildGoodSeq();
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_delta);
    entry->SetSeq().SetInst().ResetSeq_data();
    CRef<CDelta_seq> seg(new CDelta_seq());
    seg->SetLiteral().SetSeq_data().SetIupacna().Set("ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ");
    seg->SetLiteral().SetLength(52);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(seg);
    entry->SetSeq().SetInst().SetLength(52);
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [E] at position [5]"));    
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [F] at position [6]"));    
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [I] at position [9]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [J] at position [10]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [L] at position [12]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [O] at position [15]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [P] at position [16]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [Q] at position [17]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [U] at position [21]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [X] at position [24]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [Z] at position [26]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [E] at position [31]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [F] at position [32]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [I] at position [35]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [J] at position [36]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [L] at position [38]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [O] at position [41]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [P] at position [42]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [Q] at position [43]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [U] at position [47]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [X] at position [50]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [Z] at position [52]"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

    // try protein delta sequence
    scope.RemoveEntry (*entry);
    entry = BuildGoodProtSeq();
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_delta);
    entry->SetSeq().SetInst().ResetSeq_data();
    CRef<CDelta_seq> seg2(new CDelta_seq());
    seg2->SetLiteral().SetSeq_data().SetIupacaa().Set("1234567");
    seg2->SetLiteral().SetLength(7);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(seg2);
    entry->SetSeq().SetInst().SetLength(7);
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [1] at position [1]"));    
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [2] at position [2]"));    
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [3] at position [3]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [4] at position [4]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [5] at position [5]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [6] at position [6]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue [7] at position [7]"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_StopInProtein)
{
    CRef<CSeq_entry> entry = BuildGoodNucProtSet();

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    entry->SetSet().SetSeq_set().back()->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("MP*K*E*N");
    entry->SetSet().SetSeq_set().front()->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("GTGCCCTAAAAATAAGAGTAAAACTAAGGGATGCCCAGAAAAACAGAGATAAACTAAGGG");
    entry->SetSet().SetAnnot().front()->SetData().SetFtable().front()->SetExcept(true);
    entry->SetSet().SetAnnot().front()->SetData().SetFtable().front()->SetExcept_text("unclassified translation discrepancy");
    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("prot", eDiag_Error, "StopInProtein", "[3] termination symbols in protein sequence (gene? - fake protein name)"));
    expected_errors.push_back(new CExpectedError("nuc", eDiag_Error, "ExceptionProblem", "unclassified translation discrepancy is not a legal exception explanation"));
    expected_errors.push_back(new CExpectedError("nuc", eDiag_Warning, "StartCodon", "Illegal start codon (and 3 internal stops). Probably wrong genetic code [0]"));
    expected_errors.push_back(new CExpectedError("nuc", eDiag_Warning, "InternalStop", "3 internal stops (and illegal start codon). Genetic code [0]"));

    CConstRef<CValidError> eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSet().SetAnnot().front()->SetData().SetFtable().front()->ResetExcept();
    entry->SetSet().SetAnnot().front()->SetData().SetFtable().front()->ResetExcept_text();
    delete expected_errors[1];
    expected_errors[1] = NULL;
    expected_errors[2]->SetSeverity(eDiag_Error);
    expected_errors[3]->SetSeverity(eDiag_Error);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSet().SetSeq_set().front()->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("ATGCCCTAAAAATAAGAGTAAAACTAAGGGATGCCCAGAAAAACAGAGATAAACTAAGGG");
    delete expected_errors[2];
    expected_errors[2] = NULL;
    expected_errors[3]->SetErrMsg("3 internal stops. Genetic code [0]");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_PartialInconsistent)
{
    CRef<CSeq_entry> entry = BuildGoodProtSeq();

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    entry->SetSeq().SetInst().ResetSeq_data();
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_seg);
    CRef<CSeq_id> id(new CSeq_id("gb|AY123456"));
    CRef<CSeq_loc> loc1(new CSeq_loc(*id, 0, 3));
    entry->SetSeq().SetInst().SetExt().SetSeg().Set().push_back(loc1);
    CRef<CSeq_id> id2(new CSeq_id("gb|AY123457"));
    CRef<CSeq_loc> loc2(new CSeq_loc(*id2, 0, 2));
    entry->SetSeq().SetInst().SetExt().SetSeg().Set().push_back(loc2);

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "PartialInconsistent", "Partial segmented sequence without MolInfo partial"));

    // not-set
    loc1->SetPartialStart(true, eExtreme_Biological);
    loc2->SetPartialStop(true, eExtreme_Biological);
    CConstRef<CValidError> eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    loc1->SetPartialStart(true, eExtreme_Biological);
    loc2->SetPartialStop(false, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    loc1->SetPartialStart(false, eExtreme_Biological);
    loc2->SetPartialStop(true, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // unknown
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsMolinfo()) {
            (*it)->SetMolinfo().SetCompleteness(CMolInfo::eCompleteness_unknown);
        }
    }
    loc1->SetPartialStart(true, eExtreme_Biological);
    loc2->SetPartialStop(true, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    loc1->SetPartialStart(true, eExtreme_Biological);
    loc2->SetPartialStop(false, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    loc1->SetPartialStart(false, eExtreme_Biological);
    loc2->SetPartialStop(true, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // complete
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsMolinfo()) {
            (*it)->SetMolinfo().SetCompleteness(CMolInfo::eCompleteness_complete);
        }
    }
    loc1->SetPartialStart(true, eExtreme_Biological);
    loc2->SetPartialStop(true, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    loc1->SetPartialStart(true, eExtreme_Biological);
    loc2->SetPartialStop(false, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    loc1->SetPartialStart(false, eExtreme_Biological);
    loc2->SetPartialStop(true, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // partial
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsMolinfo()) {
            (*it)->SetMolinfo().SetCompleteness(CMolInfo::eCompleteness_partial);
        }
    }
    loc1->SetPartialStart(false, eExtreme_Biological);
    loc2->SetPartialStop(false, eExtreme_Biological);
    expected_errors[0]->SetErrMsg("Complete segmented sequence with MolInfo partial");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // no-left
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsMolinfo()) {
            (*it)->SetMolinfo().SetCompleteness(CMolInfo::eCompleteness_no_left);
        }
    }
    loc1->SetPartialStart(true, eExtreme_Biological);
    loc2->SetPartialStop(true, eExtreme_Biological);
    expected_errors[0]->SetErrMsg("No-left inconsistent with segmented SeqLoc");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    loc1->SetPartialStart(false, eExtreme_Biological);
    loc2->SetPartialStop(true, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    loc1->SetPartialStart(false, eExtreme_Biological);
    loc2->SetPartialStop(false, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // no-right
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsMolinfo()) {
            (*it)->SetMolinfo().SetCompleteness(CMolInfo::eCompleteness_no_right);
        }
    }
    loc1->SetPartialStart(true, eExtreme_Biological);
    loc2->SetPartialStop(true, eExtreme_Biological);
    expected_errors[0]->SetErrMsg("No-right inconsistent with segmented SeqLoc");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    loc1->SetPartialStart(true, eExtreme_Biological);
    loc2->SetPartialStop(false, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    loc1->SetPartialStart(false, eExtreme_Biological);
    loc2->SetPartialStop(false, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // no-ends
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsMolinfo()) {
            (*it)->SetMolinfo().SetCompleteness(CMolInfo::eCompleteness_no_ends);
        }
    }
    expected_errors[0]->SetErrMsg("No-ends inconsistent with segmented SeqLoc");
    loc1->SetPartialStart(true, eExtreme_Biological);
    loc2->SetPartialStop(false, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    loc1->SetPartialStart(false, eExtreme_Biological);
    loc2->SetPartialStop(true, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    loc1->SetPartialStart(false, eExtreme_Biological);
    loc2->SetPartialStop(false, eExtreme_Biological);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_ShortSeq)
{
    CRef<CSeq_entry> entry = BuildGoodProtSeq();

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.clear();

    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("MPR");
    entry->SetSeq().SetInst().SetLength(3);
    entry->SetSeq().SetAnnot().front()->SetData().SetFtable().front()->SetLocation().SetInt().SetTo(2);

    // don't report if pdb
    CRef<CPDB_seq_id> pdb_id(new CPDB_seq_id());
    pdb_id->SetMol().Set("foo");
    entry->SetSeq().SetId().front()->SetPdb(*pdb_id);
    entry->SetSeq().SetAnnot().front()->SetData().SetFtable().front()->SetLocation().SetInt().SetId().SetPdb(*pdb_id);
    scope.RemoveTopLevelSeqEntry(seh);
    seh = scope.AddTopLevelSeqEntry(*entry);
    CConstRef<CValidError> eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // don't report if partial
    entry->SetSeq().SetId().front()->SetLocal().SetStr("good");
    entry->SetSeq().SetAnnot().front()->SetData().SetFtable().front()->SetLocation().SetInt().SetId().SetLocal().SetStr("good");
    scope.RemoveTopLevelSeqEntry(seh);
    seh = scope.AddTopLevelSeqEntry(*entry);
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsMolinfo()) {
            (*it)->SetMolinfo().SetCompleteness(CMolInfo::eCompleteness_partial);
        }
    }
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsMolinfo()) {
            (*it)->SetMolinfo().SetCompleteness(CMolInfo::eCompleteness_no_left);
        }
    }
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsMolinfo()) {
            (*it)->SetMolinfo().SetCompleteness(CMolInfo::eCompleteness_no_right);
        }
    }
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsMolinfo()) {
            (*it)->SetMolinfo().SetCompleteness(CMolInfo::eCompleteness_no_ends);
        }
    }
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // for all other completeness, report
    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "ShortSeq", "Sequence only 3 residues"));
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsMolinfo()) {
            (*it)->SetMolinfo().ResetCompleteness();
        }
    }
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsMolinfo()) {
            (*it)->SetMolinfo().SetCompleteness(CMolInfo::eCompleteness_unknown);
        }
    }
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsMolinfo()) {
            (*it)->SetMolinfo().SetCompleteness(CMolInfo::eCompleteness_complete);
        }
    }
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsMolinfo()) {
            (*it)->SetMolinfo().SetCompleteness(CMolInfo::eCompleteness_other);
        }
    }
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // nucleotide
    scope.RemoveTopLevelSeqEntry(seh);
    entry = BuildGoodSeq();
    seh = scope.AddTopLevelSeqEntry(*entry);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("ATGCCCTTT");
    entry->SetSeq().SetInst().SetLength(9);
    expected_errors[0]->SetErrMsg("Sequence only 9 residues");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
    
    // don't report if pdb
    entry->SetSeq().SetId().front()->SetPdb(*pdb_id);
    scope.RemoveTopLevelSeqEntry(seh);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


static void SetTech (CRef<CSeq_entry> entry, CMolInfo::TTech tech)
{
    bool found = false;

    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsMolinfo()) {
            (*it)->SetMolinfo().SetTech(tech);
            found = true;
        }
    }
    if (!found) {
        CRef<CSeqdesc> mdesc(new CSeqdesc());
        if (entry->GetSeq().IsAa()) {
            mdesc->SetMolinfo().SetBiomol(CMolInfo::eBiomol_peptide);
        } else {
            mdesc->SetMolinfo().SetBiomol(CMolInfo::eBiomol_genomic);
        }
        mdesc->SetMolinfo().SetTech(tech);
        entry->SetSeq().SetDescr().Set().push_back(mdesc);
    }
}


static bool IsProteinTech (CMolInfo::TTech tech)
{
    bool rval = false;

    switch (tech) {
         case CMolInfo::eTech_concept_trans:
         case CMolInfo::eTech_seq_pept:
         case CMolInfo::eTech_both:
         case CMolInfo::eTech_seq_pept_overlap:
         case CMolInfo::eTech_seq_pept_homol:
         case CMolInfo::eTech_concept_trans_a:
             rval = true;
             break;
         default:
             break;
    }
    return rval;
}


static void SetBiomol (CRef<CSeq_entry> entry, CMolInfo::TBiomol biomol)
{
    bool found = false;

    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsMolinfo()) {
            (*it)->SetMolinfo().SetBiomol(biomol);
            found = true;
        }
    }
    if (!found) {
        CRef<CSeqdesc> mdesc(new CSeqdesc());
        mdesc->SetMolinfo().SetBiomol(biomol);
        entry->SetSeq().SetDescr().Set().push_back(mdesc);
    }
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_BadDeltaSeq)
{
    CRef<CSeq_entry> entry = BuildGoodDeltaSeq();

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsMolinfo()) {
            (*it)->SetMolinfo().SetTech(CMolInfo::eTech_derived);
        }
    }

    // don't report if NT or NC
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NC_123456");
    scope.RemoveTopLevelSeqEntry(seh);
    seh = scope.AddTopLevelSeqEntry(*entry);
    CConstRef<CValidError> eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NT_123456");
    scope.RemoveTopLevelSeqEntry(seh);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // don't report if gen-prod-set

    entry->SetSeq().SetId().front()->SetLocal().SetStr("good");
    scope.RemoveTopLevelSeqEntry(seh);
    seh = scope.AddTopLevelSeqEntry(*entry);

    // allowed tech values
    vector<CMolInfo::TTech> allowed_list;
    allowed_list.push_back(CMolInfo::eTech_htgs_0);
    allowed_list.push_back(CMolInfo::eTech_htgs_1);
    allowed_list.push_back(CMolInfo::eTech_htgs_2);
    allowed_list.push_back(CMolInfo::eTech_htgs_3);
    allowed_list.push_back(CMolInfo::eTech_wgs);
    allowed_list.push_back(CMolInfo::eTech_composite_wgs_htgs); 
    allowed_list.push_back(CMolInfo::eTech_unknown);
    allowed_list.push_back(CMolInfo::eTech_standard);
    allowed_list.push_back(CMolInfo::eTech_htc);
    allowed_list.push_back(CMolInfo::eTech_barcode);
    allowed_list.push_back(CMolInfo::eTech_tsa);

    for (CMolInfo::TTech i = CMolInfo::eTech_unknown;
         i <= CMolInfo::eTech_tsa;
         i++) {
         bool allowed = false;
         for (vector<CMolInfo::TTech>::iterator it = allowed_list.begin();
              it != allowed_list.end() && !allowed;
              ++it) {
              if (*it == i) {
                  allowed = true;
              }
         }
         if (allowed) {
             // don't report for htgs_0
             SetTech(entry, i);
             eval = validator.Validate(seh, options);
             if (i == CMolInfo::eTech_barcode) {
                 expected_errors.push_back(new CExpectedError("good", eDiag_Info, "BadKeyword", "Molinfo.tech barcode without BARCODE keyword"));
             }
             CheckErrors (*eval, expected_errors);
             if (i == CMolInfo::eTech_barcode) {
                 delete expected_errors[0];
                 expected_errors.clear();
             }
         }
    }

    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "BadDeltaSeq", "Delta seq technique should not be [1]"));
    for (CMolInfo::TTech i = CMolInfo::eTech_unknown;
         i <= CMolInfo::eTech_tsa;
         i++) {
         bool allowed = false;
         for (vector<CMolInfo::TTech>::iterator it = allowed_list.begin();
              it != allowed_list.end() && !allowed;
              ++it) {
              if (*it == i) {
                  allowed = true;
              }
         }
         if (!allowed) {
             // report
             SetTech(entry, i);
             if (IsProteinTech(i)) {
                 if (expected_errors.size() < 2) {
                     expected_errors.push_back(new CExpectedError("good", eDiag_Error, "InvalidForType", "Nucleic acid with protein sequence method"));
                 }
             } else {
                 if (expected_errors.size() > 1) {
                     delete expected_errors[1];
                     expected_errors.pop_back();
                 }
             }
             expected_errors[0]->SetErrMsg("Delta seq technique should not be [" + NStr::UIntToString(i) + "]");
             eval = validator.Validate(seh, options);
             CheckErrors (*eval, expected_errors);
         }
    }   

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

    CRef<CDelta_seq> start_gap_seg(new CDelta_seq());
    start_gap_seg->SetLiteral().SetLength(10);
    start_gap_seg->SetLiteral().SetSeq_data().SetGap();
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().insert(entry->SetSeq().SetInst().SetExt().SetDelta().Set().begin(), start_gap_seg);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral(10);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral(10);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral("AAATTTGGGC", CSeq_inst::eMol_dna);
    CRef<CDelta_seq> end_gap_seg(new CDelta_seq());
    end_gap_seg->SetLiteral().SetLength(10);
    end_gap_seg->SetLiteral().SetSeq_data().SetGap();
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(end_gap_seg);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral(10);
    entry->SetSeq().SetInst().SetLength(88);
    SetTech(entry, CMolInfo::eTech_wgs);
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "BadDeltaSeq", "First delta seq component is a gap"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "BadDeltaSeq", "There are 2 adjacent gaps in delta seq"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "BadDeltaSeq", "Last delta seq component is a gap"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    SetTech(entry, CMolInfo::eTech_htgs_0);
    expected_errors[0]->SetSeverity(eDiag_Warning);
    expected_errors[2]->SetSeverity(eDiag_Warning);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_ConflictingIdsOnBioseq)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "ConflictingIdsOnBioseq", "Conflicting ids on a Bioseq: (lcl|good - lcl|bad)"));

    // local IDs
    CRef<CSeq_id> id2(new CSeq_id());
    id2->SetLocal().SetStr("bad");
    entry->SetSeq().SetId().push_back(id2);
    CConstRef<CValidError> eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // GIBBSQ
    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_id> id1 = entry->SetSeq().SetId().front();
    id1->SetGibbsq(1);
    id2->SetGibbsq(2);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("1");
    expected_errors[0]->SetErrMsg("Conflicting ids on a Bioseq: (bbs|1 - bbs|2)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // GIBBSQ
    scope.RemoveTopLevelSeqEntry(seh);
    id1->SetGibbmt(1);
    id2->SetGibbmt(2);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("1");
    expected_errors[0]->SetErrMsg("Conflicting ids on a Bioseq: (bbm|1 - bbm|2)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // GI
    scope.RemoveTopLevelSeqEntry(seh);
    id1->SetGi(1);
    id2->SetGi(2);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("1");
    expected_errors[0]->SetErrMsg("Conflicting ids on a Bioseq: (gi|1 - gi|2)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // GIIM
    scope.RemoveTopLevelSeqEntry(seh);
    id1->SetGiim().SetId(1);
    id2->SetGiim().SetId(2);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("1");
    expected_errors[0]->SetErrMsg("Conflicting ids on a Bioseq: (gim|1 - gim|2)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // patent
    scope.RemoveTopLevelSeqEntry(seh);
    id1->SetPatent().SetSeqid(1);
    id1->SetPatent().SetCit().SetCountry("USA");
    id1->SetPatent().SetCit().SetId().SetNumber("1");
    id2->SetPatent().SetSeqid(2);
    id2->SetPatent().SetCit().SetCountry("USA");
    id2->SetPatent().SetCit().SetId().SetNumber("2");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("USA|1|1");
    expected_errors[0]->SetErrMsg("Conflicting ids on a Bioseq: (pat|USA|1|1 - pat|USA|2|2)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // pdb
    scope.RemoveTopLevelSeqEntry(seh);
    id1->SetPdb().SetMol().Set("good");
    id2->SetPdb().SetMol().Set("badd");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("good- ");
    expected_errors[0]->SetErrMsg("Conflicting ids on a Bioseq: (pdb|good|  - pdb|badd| )");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // general
    scope.RemoveTopLevelSeqEntry(seh);
    id1->SetGeneral().SetDb("a");
    id1->SetGeneral().SetTag().SetStr("good");
    id2->SetGeneral().SetDb("a");
    id2->SetGeneral().SetTag().SetStr("bad");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("a|good");
    expected_errors[0]->SetErrMsg("Conflicting ids on a Bioseq: (gnl|a|good - gnl|a|bad)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // should get no error if db values are different
    scope.RemoveTopLevelSeqEntry(seh);
    id2->SetGeneral().SetDb("b");
    seh = scope.AddTopLevelSeqEntry(*entry);
    delete expected_errors[0];
    expected_errors.clear();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // genbank
    scope.RemoveTopLevelSeqEntry(seh);
    expected_errors.push_back(new CExpectedError("AY123456", eDiag_Error, "ConflictingIdsOnBioseq", "Conflicting ids on a Bioseq: (gb|AY123456| - gb|AY222222|)"));
    id1->SetGenbank().SetAccession("AY123456");
    id2->SetGenbank().SetAccession("AY222222");
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // try genbank with accession same, versions different
    scope.RemoveTopLevelSeqEntry(seh);
    id2->SetGenbank().SetAccession("AY123456");
    id2->SetGenbank().SetVersion(2);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("Conflicting ids on a Bioseq: (gb|AY123456| - gb|AY123456.2|)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // try similar id type
    scope.RemoveTopLevelSeqEntry(seh);
    id2->SetGpipe().SetAccession("AY123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("Conflicting ids on a Bioseq: (gb|AY123456| - gpp|AY123456|)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // LRG
    scope.RemoveTopLevelSeqEntry(seh);
    id1->SetGeneral().SetDb("LRG");
    id1->SetGeneral().SetTag().SetStr("good");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("AY123456");
    expected_errors[0]->SetErrMsg("LRG sequence needs NG_ accession");
    expected_errors[0]->SetSeverity(eDiag_Critical);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    // no error if has NG
    scope.RemoveTopLevelSeqEntry(seh);
    id2->SetOther().SetAccession("NG_123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
   


    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_MolNuclAcid)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "MolNuclAcid", "Bioseq.mol is type na"));

    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_na);
    CConstRef<CValidError> eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_ConflictingBiomolTech)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    CConstRef<CValidError> eval;

    // allowed tech values
    vector<CMolInfo::TTech> genomic_list;
    genomic_list.push_back(CMolInfo::eTech_sts);
    genomic_list.push_back(CMolInfo::eTech_survey);
    genomic_list.push_back(CMolInfo::eTech_wgs);
    genomic_list.push_back(CMolInfo::eTech_htgs_0);
    genomic_list.push_back(CMolInfo::eTech_htgs_1);
    genomic_list.push_back(CMolInfo::eTech_htgs_2);
    genomic_list.push_back(CMolInfo::eTech_htgs_3);

    for (CMolInfo::TTech i = CMolInfo::eTech_unknown;
         i <= CMolInfo::eTech_tsa;
        i++) {
        bool genomic = false;
        for (vector<CMolInfo::TTech>::iterator it = genomic_list.begin();
              it != genomic_list.end() && !genomic;
              ++it) {
            if (*it == i) {
                genomic = true;
            }
        }
        entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_dna);
        SetTech (entry, i);
        SetBiomol (entry, CMolInfo::eBiomol_cRNA);
        if (i == CMolInfo::eTech_htgs_2) {
            expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "BadHTGSeq", "HTGS 2 raw seq has no gaps and no graphs"));
        }
        if (genomic) {
            expected_errors.push_back(new CExpectedError("good", eDiag_Error, "ConflictingBiomolTech", "HTGS/STS/GSS/WGS sequence should be genomic"));            
            eval = validator.Validate(seh, options);
            CheckErrors (*eval, expected_errors);
            SetBiomol(entry, CMolInfo::eBiomol_genomic);
            entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);
            expected_errors.back()->SetErrMsg("HTGS/STS/GSS/WGS sequence should not be RNA");
            eval = validator.Validate(seh, options);
            CheckErrors (*eval, expected_errors);
        } else {
            if (IsProteinTech(i)) {
                expected_errors.push_back(new CExpectedError("good", eDiag_Error, "InvalidForType", "Nucleic acid with protein sequence method"));
            }
            if (i == CMolInfo::eTech_barcode) {
               expected_errors.push_back(new CExpectedError("good", eDiag_Info, "BadKeyword", "Molinfo.tech barcode without BARCODE keyword"));
            }
            eval = validator.Validate(seh, options);
            CheckErrors (*eval, expected_errors);
        }
        while (expected_errors.size() > 0) {
            if (expected_errors[expected_errors.size() - 1] != NULL) {
                delete expected_errors[expected_errors.size() - 1];
            }
            expected_errors.pop_back();
        }
    }

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_ExceptionProblem)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "ExceptionProblem", "Exception explanation text is also found in feature comment"));

    CRef<CSeq_feat> feat(new CSeq_feat());
    feat->SetData().SetImp().SetKey("misc_feature");
    feat->SetLocation().SetInt().SetFrom(0);
    feat->SetLocation().SetInt().SetTo(10);
    feat->SetLocation().SetInt().SetId().SetLocal().SetStr("good");
    feat->SetExcept();
    CRef<CSeq_annot> annot (new CSeq_annot());
    annot->SetData().SetFtable().push_back(feat);
    entry->SetSeq().SetAnnot().push_back(annot);

    // look for exception in comment
    feat->SetExcept_text("RNA editing");
    feat->SetComment("RNA editing");
    CConstRef<CValidError> eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // look for one exception in comment
    feat->SetExcept_text("RNA editing, rearrangement required for product");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // no citation
    feat->SetExcept_text("reasons given in citation");
    expected_errors[0]->SetErrMsg("Reasons given in citation exception does not have the required citation");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // no inference
    feat->SetExcept_text("annotated by transcript or proteomic data");
    expected_errors[0]->SetErrMsg("Annotated by transcript or proteomic data exception does not have the required inference qualifier");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // not legal
    feat->SetExcept_text("not a legal exception");
    expected_errors[0]->SetErrMsg("not a legal exception is not a legal exception explanation");
    expected_errors[0]->SetSeverity(eDiag_Error);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // change to ref-seq
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NC_123456");
    scope.RemoveTopLevelSeqEntry(seh);
    seh = scope.AddTopLevelSeqEntry(*entry);
    feat->SetLocation().SetInt().SetId().SetOther().SetAccession("NC_123456");


    // multiple ref-seq exceptions
    feat->SetExcept_text("unclassified transcription discrepancy, RNA editing");
    feat->ResetComment();
    expected_errors[0]->SetErrMsg("Genome processing exception should not be combined with other explanations");
    expected_errors[0]->SetAccession("NC_123456");
    expected_errors[0]->SetSeverity(eDiag_Warning);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // not legal (is warning for NC or NT)
    feat->SetExcept_text("not a legal exception");
    expected_errors[0]->SetErrMsg("not a legal exception is not a legal exception explanation");
    expected_errors[0]->SetSeverity(eDiag_Warning);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    delete expected_errors[0];
    expected_errors.clear();


}


BOOST_AUTO_TEST_CASE(Test_SEQ_FEAT_SeqDataLenWrong)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    // need to call this statement before calling AddDefaults 
    // to make sure that we can fetch the sequence referenced by the
    // delta sequence so that we can detect that the loc in the
    // delta sequence is longer than the referenced sequence
    CGBDataLoader::RegisterInObjectManager(*objmgr);
    CScope scope(*objmgr);
    scope.AddDefaults();

    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    // validate - should be fine
    CConstRef<CValidError> eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // longer and shorter for iupacna
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "SeqDataLenWrong", "Bioseq.seq_data too short [60] for given length [65]"));
    entry->SetSeq().SetInst().SetLength(65);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetLength(55);
    expected_errors[0]->SetErrMsg("Bioseq.seq_data is larger [60] than given length [55]");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // try other divisors
    entry->SetSeq().SetInst().SetLength(60);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back('A');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back('T');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back('G');
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set().push_back('C');
    CRef<CSeq_data> packed_data(new CSeq_data);
    // convert seq data to another format
    // (NCBI2na = 2 bit nucleic acid code)
    CSeqportUtil::Convert(entry->SetSeq().SetInst().GetSeq_data(),
                          packed_data,
                          CSeq_data::e_Ncbi2na);
    entry->SetSeq().SetInst().SetSeq_data(*packed_data);
    expected_errors[0]->SetErrMsg("Bioseq.seq_data is larger [64] than given length [60]");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetSeq_data().SetNcbi2na().Set().pop_back();
    entry->SetSeq().SetInst().SetSeq_data().SetNcbi2na().Set().pop_back();
    expected_errors[0]->SetErrMsg("Bioseq.seq_data too short [56] for given length [60]");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    CSeqportUtil::Convert(entry->SetSeq().SetInst().GetSeq_data(),
                          packed_data,
                          CSeq_data::e_Ncbi4na);
    entry->SetSeq().SetInst().SetSeq_data(*packed_data);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetSeq_data().SetNcbi4na().Set().push_back('1');
    entry->SetSeq().SetInst().SetSeq_data().SetNcbi4na().Set().push_back('8');
    entry->SetSeq().SetInst().SetSeq_data().SetNcbi4na().Set().push_back('1');
    entry->SetSeq().SetInst().SetSeq_data().SetNcbi4na().Set().push_back('8');
    expected_errors[0]->SetErrMsg("Bioseq.seq_data is larger [64] than given length [60]");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);


    // now try seg and ref
    entry->SetSeq().SetInst().ResetSeq_data();
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_seg);
    CRef<CSeq_id> id(new CSeq_id("gb|AY123456"));
    CRef<CSeq_loc> loc(new CSeq_loc(*id, 0, 55));
    entry->SetSeq().SetInst().SetExt().SetSeg().Set().push_back(loc);
    expected_errors[0]->SetErrMsg("Bioseq.seq_data too short [56] for given length [60]");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
  
    loc->SetInt().SetTo(63);
    expected_errors[0]->SetErrMsg("Bioseq.seq_data is larger [64] than given length [60]");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_ref);
    entry->SetSeq().SetInst().SetExt().SetRef().SetInt().SetId(*id);
    entry->SetSeq().SetInst().SetExt().SetRef().SetInt().SetFrom(0);
    entry->SetSeq().SetInst().SetExt().SetRef().SetInt().SetTo(55);
    expected_errors[0]->SetErrMsg("Bioseq.seq_data too short [56] for given length [60]");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetExt().SetRef().SetInt().SetTo(63);
    expected_errors[0]->SetErrMsg("Bioseq.seq_data is larger [64] than given length [60]");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // delta sequence
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_delta);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddSeqRange(*id, 0, 55);
    expected_errors[0]->SetErrMsg("Bioseq.seq_data too short [56] for given length [60]");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    entry->SetSeq().SetInst().SetExt().Reset();
    entry->SetSeq().SetInst().SetExt().SetDelta().AddSeqRange(*id, 0, 30);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddSeqRange(*id, 40, 72);
    expected_errors[0]->SetErrMsg("Bioseq.seq_data is larger [64] than given length [60]");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetExt().Reset();
    entry->SetSeq().SetInst().SetExt().SetDelta().AddSeqRange(*id, 0, 59);
    CRef<CDelta_seq> delta_seq;
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(delta_seq);
    expected_errors[0]->SetErrMsg("NULL pointer in delta seq_ext valnode (segment 2)");
    expected_errors[0]->SetSeverity(eDiag_Error);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetExt().Reset();
    CRef<CDelta_seq> delta_seq2(new CDelta_seq());
    delta_seq2->SetLoc().SetInt().SetId(*id);
    delta_seq2->SetLoc().SetInt().SetFrom(0);
    delta_seq2->SetLoc().SetInt().SetTo(485);
    entry->SetSeq().SetInst().SetLength(486);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(delta_seq2);
    expected_errors[0]->SetErrMsg("Seq-loc extent (486) greater than length of gb|AY123456 (485)");
    expected_errors[0]->SetSeverity(eDiag_Critical);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    delete expected_errors[0];
    expected_errors.clear();
}


