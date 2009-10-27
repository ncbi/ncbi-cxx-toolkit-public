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
#include <objects/seq/Ref_ext.hpp>
#include <objects/seq/Map_ext.hpp>
#include <objects/seq/Seq_gap.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objtools/validator/validator.hpp>

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

    for ( CValidError_CI vit(eval); vit; ++vit, ++err_pos ) {
        while (err_pos < expected_errors.size() && !expected_errors[err_pos]) {
            ++err_pos;
        }
        if (err_pos < expected_errors.size()) {
            expected_errors[err_pos]->Test(*vit);
        } else {
            string description =  vit->GetAccession() + ":"
                + CValidErrItem::ConvertSeverity(vit->GetSeverity()) + ":"
                + vit->GetErrCode() + ":"
                + vit->GetMsg();
            BOOST_CHECK_EQUAL(description, "Unexpected error");
        }
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

    delete expected_errors[0];
    expected_errors.clear();

    // shouldn't get fuzzy length if gap
    entry->SetSeq().SetInst().SetSeq_data().SetGap();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

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
