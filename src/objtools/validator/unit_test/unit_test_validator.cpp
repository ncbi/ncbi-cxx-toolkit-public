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

#include <objects/biblio/Id_pat.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/User_object.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/GIBB_mol.hpp>
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
#include <objects/seq/Seq_hist.hpp>
#include <objects/seq/Seq_hist_rec.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqblock/EMBL_block.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/seqloc/Giimport_id.hpp>
#include <objects/seqloc/Patent_seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seqdesc_ci.hpp>
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
    string msg = err_item.GetMsg();
    size_t pos = NStr::Find(msg, " EXCEPTION: NCBI C++ Exception:");
    if (pos != string::npos) {
        msg = msg.substr(0, pos);
    }
    BOOST_CHECK_EQUAL(msg, m_ErrMsg);
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


static void AddGoodSource (CRef<CSeq_entry> entry)
{
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

    if (entry->IsSeq()) {
        entry->SetSeq().SetDescr().Set().push_back(odesc);
    } else if (entry->IsSet()) {
        entry->SetSet().SetDescr().Set().push_back(odesc);
    }
}


static void SetTaxname (CRef<CSeq_entry> entry, string taxname)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                if (NStr::IsBlank(taxname)) {
                    (*it)->SetSource().SetOrg().ResetTaxname();
                } else {
                    (*it)->SetSource().SetOrg().SetTaxname(taxname);
                }
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                if (NStr::IsBlank(taxname)) {
                    (*it)->SetSource().SetOrg().ResetTaxname();
                } else {
                    (*it)->SetSource().SetOrg().SetTaxname(taxname);
                }
            }
        }
    }
}


static void SetCommon (CRef<CSeq_entry> entry, string common)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                if (NStr::IsBlank(common)) {
                    (*it)->SetSource().SetOrg().ResetCommon();
                } else {
                    (*it)->SetSource().SetOrg().SetCommon(common);
                }
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                if (NStr::IsBlank(common)) {
                    (*it)->SetSource().SetOrg().ResetCommon();
                } else {
                    (*it)->SetSource().SetOrg().SetCommon(common);
                }
            }
        }
    }
}


static void SetLineage (CRef<CSeq_entry> entry, string lineage)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                if (NStr::IsBlank(lineage)) {
                    (*it)->SetSource().SetOrg().SetOrgname().ResetLineage();
                } else {
                    (*it)->SetSource().SetOrg().SetOrgname().SetLineage(lineage);
                }
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                if (NStr::IsBlank(lineage)) {
                    (*it)->SetSource().SetOrg().SetOrgname().ResetLineage();
                } else {
                    (*it)->SetSource().SetOrg().SetOrgname().SetLineage(lineage);
                }
            }
        }
    }
}


static void ResetOrgname (CRef<CSeq_entry> entry)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                (*it)->SetSource().SetOrg().ResetOrgname();
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                (*it)->SetSource().SetOrg().ResetOrgname();
            }
        }
    }
}


static void SetFocus (CRef<CSeq_entry> entry)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                (*it)->SetSource().SetIs_focus();
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                (*it)->SetSource().SetIs_focus();
            }
        }
    }
}


static void ClearFocus (CRef<CSeq_entry> entry)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                (*it)->SetSource().ResetIs_focus();
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                (*it)->SetSource().ResetIs_focus();
            }
        }
    }
}


static void SetGenome (CRef<CSeq_entry> entry, CBioSource::TGenome genome)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                (*it)->SetSource().SetGenome(genome);
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                (*it)->SetSource().SetGenome(genome);
            }
        }
    }
}


static void SetSubSource (CBioSource& src, CSubSource::TSubtype subtype, string val)
{
    if (NStr::IsBlank(val)) {
        EDIT_EACH_SUBSOURCE_ON_BIOSOURCE (it, src) {
            if ((*it)->IsSetSubtype() && (*it)->GetSubtype() == subtype) {
                ERASE_SUBSOURCE_ON_BIOSOURCE (it, src);
            }
        }
    } else {
        CRef<CSubSource> sub(new CSubSource(subtype, val));
        src.SetSubtype().push_back(sub);
    }
}


static void SetSubSource (CRef<CSeq_entry> entry, CSubSource::TSubtype subtype, string val)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                SetSubSource((*it)->SetSource(), subtype, val);
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                SetSubSource((*it)->SetSource(), subtype, val);
            }
        }
    }
}


static void SetCountryOnSrc (CBioSource& src, string country) 
{
    if (NStr::IsBlank(country)) {
        EDIT_EACH_SUBSOURCE_ON_BIOSOURCE (it, src) {
            if ((*it)->IsSetSubtype() && (*it)->GetSubtype() == CSubSource::eSubtype_country) {
                ERASE_SUBSOURCE_ON_BIOSOURCE (it, src);
            }
        }
    } else {
        CRef<CSubSource> sub(new CSubSource(CSubSource::eSubtype_country, country));
        src.SetSubtype().push_back(sub);
    }
}


static void SetSubCountry (CRef<CSeq_entry> entry, string country)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                SetCountryOnSrc((*it)->SetSource(), country);
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                SetCountryOnSrc((*it)->SetSource(), country);
            }
        }
    }
}


static void SetChromosome (CBioSource& src, string chromosome) 
{
    if (NStr::IsBlank(chromosome)) {
        EDIT_EACH_SUBSOURCE_ON_BIOSOURCE (it, src) {
            if ((*it)->IsSetSubtype() && (*it)->GetSubtype() == CSubSource::eSubtype_chromosome) {
                ERASE_SUBSOURCE_ON_BIOSOURCE (it, src);
            }
        }
    } else {
        CRef<CSubSource> sub(new CSubSource(CSubSource::eSubtype_chromosome, chromosome));
        src.SetSubtype().push_back(sub);
    }
}


static void SetChromosome (CRef<CSeq_entry> entry, string chromosome)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                SetChromosome((*it)->SetSource(), chromosome);
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                SetChromosome((*it)->SetSource(), chromosome);
            }
        }
    }
}


static void SetTransgenic (CBioSource& src, bool do_set) 
{
    if (do_set) {
        CRef<CSubSource> sub(new CSubSource(CSubSource::eSubtype_transgenic, ""));
        src.SetSubtype().push_back(sub);
    } else {
        EDIT_EACH_SUBSOURCE_ON_BIOSOURCE (it, src) {
            if ((*it)->IsSetSubtype() && (*it)->GetSubtype() == CSubSource::eSubtype_transgenic) {
                ERASE_SUBSOURCE_ON_BIOSOURCE (it, src);
            }
        }
    }
}


static void SetTransgenic (CRef<CSeq_entry> entry, bool do_set)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                SetTransgenic((*it)->SetSource(), do_set);
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                SetTransgenic((*it)->SetSource(), do_set);
            }
        }
    }
}


static void SetTaxon (CBioSource& src, size_t taxon)
{
    if (taxon == 0) {
        if (src.IsSetOrg()) {
            EDIT_EACH_DBXREF_ON_ORGREF(it, src.SetOrg()) {
                if ((*it)->IsSetDb() && NStr::Equal((*it)->GetDb(), "taxon")) {
                    ERASE_DBXREF_ON_ORGREF(it, src.SetOrg());
                }
            }
        }
    } else {
        CRef<CDbtag> dbtag(new CDbtag());
        dbtag->SetDb("taxon");
        dbtag->SetTag().SetId(taxon);
        src.SetOrg().SetDb().push_back(dbtag);
    }
}


static void SetTaxon (CRef<CSeq_entry> entry, size_t taxon)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                SetTaxon((*it)->SetSource(), taxon);
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                SetTaxon((*it)->SetSource(), taxon);
            }
        }
    }
}


static void SetOrgMod (CBioSource& src, COrgMod::TSubtype subtype, string val)
{
    if (NStr::IsBlank(val)) {
        EDIT_EACH_ORGMOD_ON_BIOSOURCE (it, src) {
            if ((*it)->IsSetSubtype() && (*it)->GetSubtype() == subtype) {
                ERASE_ORGMOD_ON_BIOSOURCE (it, src);
            }
        }
    } else {
        CRef<COrgMod> sub(new COrgMod(subtype, val));
        src.SetOrg().SetOrgname().SetMod().push_back(sub);
    }
}


static void SetOrgMod (CRef<CSeq_entry> entry, COrgMod::TSubtype subtype, string val)
{
    if (!entry) {
        return;
    }
    if (entry->IsSeq()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                SetOrgMod((*it)->SetSource(), subtype, val);
            }
        }
    } else if (entry->IsSet()) {
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSet().SetDescr().Set()) {
            if ((*it)->IsSource()) {
                SetOrgMod((*it)->SetSource(), subtype, val);
            }
        }
    }
}


static void AddGoodPub (CRef<CSeq_entry> entry)
{
    CRef<CSeqdesc> pdesc(new CSeqdesc());
    CRef<CPub> pub(new CPub());
    pub->SetPmid((CPub::TPmid)1);
    pdesc->SetPub().SetPub().Set().push_back(pub);

    if (entry->IsSeq()) {
        entry->SetSeq().SetDescr().Set().push_back(pdesc);
    } else if (entry->IsSet()) {
        entry->SetSet().SetDescr().Set().push_back(pdesc);
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

    CRef<CSeq_entry> entry(new CSeq_entry());
    entry->SetSeq(*seq);
    AddGoodSource (entry);
    AddGoodPub(entry);

    return entry;
}


static CRef<CSeq_entry> BuildGoodProtSeq(void)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();

    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_aa);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("PRKTEIN");
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
    pseq->SetInst().SetSeq_data().SetIupacaa().Set("MPRKTEIN");
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
    AddGoodSource (set_entry);
    AddGoodPub(set_entry);
    return set_entry;
}


static CRef<CSeq_entry> BuildGoodDeltaSeq(void)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();

    entry->SetSeq().SetInst().ResetSeq_data();
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_delta);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral("ATGATGATGCCC", CSeq_inst::eMol_dna);
    CRef<CDelta_seq> gap_seg(new CDelta_seq());
    gap_seg->SetLiteral().SetSeq_data().SetGap();
    gap_seg->SetLiteral().SetLength(10);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(gap_seg);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral("CCCATGATGATG", CSeq_inst::eMol_dna);
    entry->SetSeq().SetInst().SetLength(34);

    return entry;
}


static void RemoveDeltaSeqGaps(CRef<CSeq_entry> entry) 
{
    CDelta_ext::Tdata::iterator seg_it = entry->SetSeq().SetInst().SetExt().SetDelta().Set().begin();
    while (seg_it != entry->SetSeq().SetInst().SetExt().SetDelta().Set().end()) {
        if ((*seg_it)->IsLiteral() 
            && (!(*seg_it)->GetLiteral().IsSetSeq_data() 
                || (*seg_it)->GetLiteral().GetSeq_data().IsGap())) {
            TSeqPos len = entry->SetSeq().SetInst().GetLength();
            len -= (*seg_it)->GetLiteral().GetLength();
            seg_it = entry->SetSeq().SetInst().SetExt().SetDelta().Set().erase(seg_it);
            entry->SetSeq().SetInst().SetLength(len);
        } else {
            ++seg_it;
        }
    }
}


static void AddToDeltaSeq(CRef<CSeq_entry> entry, string seq)
{
    size_t orig_len = entry->GetSeq().GetLength();
    size_t add_len = seq.length();

    CRef<CDelta_seq> gap_seg(new CDelta_seq());
    gap_seg->SetLiteral().SetSeq_data().SetGap();
    gap_seg->SetLiteral().SetLength(10);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(gap_seg);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddLiteral(seq, CSeq_inst::eMol_dna);
    entry->SetSeq().SetInst().SetLength(orig_len + 10 + add_len);
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


static CRef<CSeq_entry> BuildSegSetPart(string id_str)
{
    CRef<CSeq_entry> part(new CSeq_entry());
    part->SetSeq().SetInst().SetMol(CSeq_inst::eMol_dna);
    part->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_raw);
    part->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAA");
    part->SetSeq().SetInst().SetLength(60);
    CRef<CSeq_id> id(new CSeq_id(id_str));
    part->SetSeq().SetId().push_back(id);
    SetBiomol(part, CMolInfo::eBiomol_genomic);
    return part;
}


static CRef<CSeq_entry> BuildGoodSegSet(void)
{
    CRef<CSeq_entry> segset(new CSeq_entry());
    segset->SetSet().SetClass(CBioseq_set::eClass_segset);

    CRef<CSeq_entry> seg_seq(new CSeq_entry());
    seg_seq->SetSeq().SetInst().SetMol(CSeq_inst::eMol_dna);
    seg_seq->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_seg);

    CRef<CSeq_loc> loc1(new CSeq_loc());
    loc1->SetWhole().SetLocal().SetStr("part1");
    CRef<CSeq_loc> loc2(new CSeq_loc());
    loc2->SetWhole().SetLocal().SetStr("part2");
    CRef<CSeq_loc> loc3(new CSeq_loc());
    loc3->SetWhole().SetLocal().SetStr("part3");

    seg_seq->SetSeq().SetInst().SetExt().SetSeg().Set().push_back(loc1);
    seg_seq->SetSeq().SetInst().SetExt().SetSeg().Set().push_back(loc2);
    seg_seq->SetSeq().SetInst().SetExt().SetSeg().Set().push_back(loc3);
    seg_seq->SetSeq().SetInst().SetLength(180);

    CRef<CSeq_id> id(new CSeq_id());
    id->SetLocal().SetStr ("master");
    seg_seq->SetSeq().SetId().push_back(id);
    seg_seq->SetSeq().SetInst().SetLength(180);
    SetBiomol(seg_seq, CMolInfo::eBiomol_genomic);

    segset->SetSet().SetSeq_set().push_back(seg_seq);

    // create parts set
    CRef<CSeq_entry> parts_set(new CSeq_entry());
    parts_set->SetSet().SetClass(CBioseq_set::eClass_parts);
    parts_set->SetSet().SetSeq_set().push_back(BuildSegSetPart("lcl|part1"));
    parts_set->SetSet().SetSeq_set().push_back(BuildSegSetPart("lcl|part2"));
    parts_set->SetSet().SetSeq_set().push_back(BuildSegSetPart("lcl|part3"));

    segset->SetSet().SetSeq_set().push_back(parts_set);

    CRef<CSeqdesc> pdesc(new CSeqdesc());
    CRef<CPub> pub(new CPub());
    pub->SetPmid((CPub::TPmid)1);
    pdesc->SetPub().SetPub().Set().push_back(pub);
    segset->SetDescr().Set().push_back(pdesc);
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
    segset->SetDescr().Set().push_back(odesc);

    return segset;
}


static CRef<CSeq_align> BuildGoodAlign()
{
    CRef<CSeq_align> align(new CSeq_align());
    CRef<CSeq_id> id1(new CSeq_id());
    id1->SetGenbank().SetAccession("FJ375734.2");
    id1->SetGenbank().SetVersion(2);
    CRef<CSeq_id> id2(new CSeq_id());
    id2->SetGenbank().SetAccession("FJ375735.2");
    id2->SetGenbank().SetVersion(2);
    align->SetDim(2);
    align->SetType(CSeq_align::eType_global);
    align->SetSegs().SetDenseg().SetIds().push_back(id1);
    align->SetSegs().SetDenseg().SetIds().push_back(id2);
    align->SetSegs().SetDenseg().SetDim(2);
    align->SetSegs().SetDenseg().SetStarts().push_back(0);
    align->SetSegs().SetDenseg().SetStarts().push_back(0);
    align->SetSegs().SetDenseg().SetNumseg(1);
    align->SetSegs().SetDenseg().SetLens().push_back(812);

    return align;
}


static void RemoveDescriptorType (CRef<CSeq_entry> entry, CSeqdesc::E_Choice desc_choice)
{
    EDIT_EACH_DESCRIPTOR_ON_SEQENTRY (dit, *entry) {
        if ((*dit)->Which() == desc_choice) {
            ERASE_DESCRIPTOR_ON_SEQENTRY (dit, *entry);
        }
    }
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


static void SetCompleteness(CRef<CSeq_entry> entry, CMolInfo::TCompleteness completeness)
{
    if (entry->IsSeq()) {
        bool found = false;
        NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
            if ((*it)->IsMolinfo()) {
                (*it)->SetMolinfo().SetCompleteness (completeness);
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
            mdesc->SetMolinfo().SetCompleteness (completeness);
            entry->SetSeq().SetDescr().Set().push_back(mdesc);
        }
    }
}


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

    SetCompleteness (entry, CMolInfo::eCompleteness_complete);


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


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_InvalidAlphabet)
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


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_StopInProtein)
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


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_PartialInconsistent)
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
    SetCompleteness (entry, CMolInfo::eCompleteness_unknown);

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
    SetCompleteness (entry, CMolInfo::eCompleteness_complete);

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
    SetCompleteness (entry, CMolInfo::eCompleteness_partial);

    loc1->SetPartialStart(false, eExtreme_Biological);
    loc2->SetPartialStop(false, eExtreme_Biological);
    expected_errors[0]->SetErrMsg("Complete segmented sequence with MolInfo partial");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // no-left
    SetCompleteness (entry, CMolInfo::eCompleteness_no_left);

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
    SetCompleteness (entry, CMolInfo::eCompleteness_no_right);

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
    SetCompleteness (entry, CMolInfo::eCompleteness_no_ends);

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


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_ShortSeq)
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
    SetCompleteness (entry, CMolInfo::eCompleteness_partial);

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    SetCompleteness (entry, CMolInfo::eCompleteness_no_left);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    SetCompleteness (entry, CMolInfo::eCompleteness_no_right);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    SetCompleteness (entry, CMolInfo::eCompleteness_no_ends);
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
    SetCompleteness (entry, CMolInfo::eCompleteness_unknown);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    SetCompleteness (entry, CMolInfo::eCompleteness_complete);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    SetCompleteness (entry, CMolInfo::eCompleteness_other);
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


static void AddRefGeneTrackingUserObject(CRef<CSeq_entry> entry)
{
    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetUser().SetType().SetStr("RefGeneTracking");
    CRef<CUser_field> field(new CUser_field());
    field->SetLabel().SetStr("Status");
    field->SetData().SetStr("Inferred");
    desc->SetUser().SetData().push_back(field);
    entry->SetSeq().SetDescr().Set().push_back(desc);
}


static void SetTitle(CRef<CSeq_entry> entry, string title) 
{
    bool found = false;

    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsTitle()) {
            (*it)->SetTitle(title);
            found = true;
        }
    }
    if (!found) {
        CRef<CSeqdesc> desc(new CSeqdesc());
        desc->SetTitle(title);
        entry->SetSeq().SetDescr().Set().push_back(desc);
    }
}


static void AddGenbankKeyword (CRef<CSeq_entry> entry, string keyword)
{
    bool found = false;

    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsGenbank()) {
            (*it)->SetGenbank().SetKeywords().push_back(keyword);
            found = true;
        }
    }
    if (!found) {
        CRef<CSeqdesc> desc(new CSeqdesc());
        desc->SetGenbank().SetKeywords().push_back(keyword);
        entry->SetSeq().SetDescr().Set().push_back(desc);
    }
}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_BadDeltaSeq)
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
    entry->SetSeq().SetInst().SetLength(94);
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


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_ConflictingIdsOnBioseq)
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
    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_id> id2(new CSeq_id());
    id2->SetLocal().SetStr("bad");
    entry->SetSeq().SetId().push_back(id2);
    seh = scope.AddTopLevelSeqEntry(*entry);
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
    CRef<CSeq_id> id3(new CSeq_id("gb|AY123456.1"));
    entry->SetSeq().SetId().push_back (id3);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("AY123456");
    expected_errors[0]->SetErrMsg("Conflicting ids on a Bioseq: (gi|1 - gi|2)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    entry->SetSeq().SetId().pop_back();

    // GIIM
    scope.RemoveTopLevelSeqEntry(seh);
    id1->SetGiim().SetId(1);
    id1->SetGiim().SetDb("foo");
    id2->SetGiim().SetId(2);
    id2->SetGiim().SetDb("foo");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("1");
    expected_errors[0]->SetErrMsg("Conflicting ids on a Bioseq: (gim|1 - gim|2)");
    expected_errors.push_back(new CExpectedError("1", eDiag_Error, "IdOnMultipleBioseqs", "BioseqFind (gim|1) unable to find itself - possible internal error"));
    expected_errors.push_back(new CExpectedError("1", eDiag_Error, "IdOnMultipleBioseqs", "BioseqFind (gim|2) unable to find itself - possible internal error"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    delete expected_errors[1];
    delete expected_errors[2];
    expected_errors.pop_back();
    expected_errors.pop_back();

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
    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
    expected_errors.push_back(new CExpectedError("AY123456", eDiag_Error, "ConflictingIdsOnBioseq", "Conflicting ids on a Bioseq: (gb|AY123456| - gb|AY123456.2|)"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // try similar id type
    scope.RemoveTopLevelSeqEntry(seh);
    id2->SetGpipe().SetAccession("AY123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
    expected_errors.push_back(new CExpectedError("AY123456", eDiag_Error, "ConflictingIdsOnBioseq", "Conflicting ids on a Bioseq: (gb|AY123456| - gpp|AY123456|)"));
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


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_MolNuclAcid)
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


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_ConflictingBiomolTech)
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
    genomic_list.push_back(CMolInfo::eTech_composite_wgs_htgs);

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


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_SeqIdNameHasSpace)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NC_123456");
    entry->SetSeq().SetId().front()->SetOther().SetName("good one");

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
    expected_errors.push_back(new CExpectedError("NC_123456", eDiag_Critical, "SeqIdNameHasSpace", "Seq-id.name 'good one' should be a single word without any spaces"));
    CConstRef<CValidError> eval;

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_DuplicateSegmentReferences)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    entry->SetSeq().SetInst().ResetSeq_data();
    entry->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_seg);
    CRef<CSeq_loc> seg1 (new CSeq_loc());
    seg1->SetWhole().SetGenbank().SetAccession("AY123456");
    entry->SetSeq().SetInst().SetExt().SetSeg().Set().push_back(seg1);
    CRef<CSeq_loc> seg2 (new CSeq_loc());
    seg2->SetWhole().SetGenbank().SetAccession("AY123456");
    entry->SetSeq().SetInst().SetExt().SetSeg().Set().push_back(seg2);
    entry->SetSeq().SetInst().SetLength(970);

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
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "DuplicateSegmentReferences", "Segmented sequence has multiple references to gb|AY123456"));
    CConstRef<CValidError> eval;

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    seg2->SetInt().SetId().SetGenbank().SetAccession("AY123456");
    seg2->SetInt().SetFrom(0);
    seg2->SetInt().SetTo(484);
    expected_errors[0]->SetSeverity(eDiag_Warning);
    expected_errors[0]->SetErrMsg("Segmented sequence has multiple references to gb|AY123456 that are not SEQLOC_WHOLE");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_TrailingX)
{
    CRef<CSeq_entry> entry = BuildGoodNucProtSet();
    CRef<CSeq_entry> nuc = entry->SetSet().SetSeq_set().front();
    CRef<CSeq_entry> prot = entry->SetSet().SetSeq_set().back();
    CRef<CSeq_feat> prot_feat = prot->SetSeq().SetAnnot().front()->SetData().SetFtable().front();
    CRef<CSeq_feat> cds_feat = entry->SetSet().SetAnnot().front()->SetData().SetFtable().front();
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("ATGCCCAGAAAAACAGAGATANNNNNN");
    nuc->SetSeq().SetInst().SetLength(27);
    prot->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("MPRKTEIXX");
    prot->SetSeq().SetInst().SetLength(9);
    SetCompleteness (prot, CMolInfo::eCompleteness_no_right);
    prot_feat->SetLocation().SetInt().SetTo(8);
    prot_feat->SetLocation().SetPartialStop(true, eExtreme_Biological);
    prot_feat->SetPartial(true);
    cds_feat->SetLocation().SetPartialStop(true, eExtreme_Biological);
    cds_feat->SetPartial(true);

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
    expected_errors.push_back(new CExpectedError("nuc", eDiag_Warning, "TerminalNs", "N at end of sequence"));
    expected_errors.push_back(new CExpectedError("prot", eDiag_Warning, "TrailingX", "Sequence ends in 2 trailing Xs"));
    CConstRef<CValidError> eval;

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

    // if stop codon present, do not report trailing X in protein
    scope.RemoveTopLevelSeqEntry(seh);
    nuc->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("ATGCCCAGAAAAACAGAGATANNNTAA");
    prot->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("MPRKTEIX");
    prot->SetSeq().SetInst().SetLength(8);
    SetCompleteness (prot, CMolInfo::eCompleteness_complete);
    prot_feat->SetLocation().SetInt().SetTo(7);
    prot_feat->SetLocation().SetPartialStop(false, eExtreme_Biological);
    prot_feat->SetLocation().InvalidateTotalRangeCache();
    prot_feat->SetPartial(false);
    cds_feat->SetLocation().SetPartialStop(false, eExtreme_Biological);
    cds_feat->SetPartial(false);
    seh = scope.AddTopLevelSeqEntry(*entry);

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_BadSeqIdFormat)
{
    CRef<CSeq_entry> nuc_entry = BuildGoodSeq();
    CRef<CSeq_entry> prot_entry = BuildGoodProtSeq();
    CRef<CSeq_feat> prot_feat = prot_entry->SetSeq().SetAnnot().front()->SetData().SetFtable().front();

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*prot_entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("",eDiag_Error, "BadSeqIdFormat", "Bad accession"));

    vector<string> bad_ids;
    bad_ids.push_back("AY123456ABC");  // can't have letters after digits
    bad_ids.push_back("A1234");        // for a single letter, only acceptable number of digits is 5
    bad_ids.push_back("A123456");
    bad_ids.push_back("AY12345");      // for two letters, only acceptable number of digits is 6
    bad_ids.push_back("AY1234567");
    bad_ids.push_back("ABC1234");      // three letters bad unless prot and 5 digits
    bad_ids.push_back("ABC123456");
    bad_ids.push_back("ABCD1234567");  // four letters
    bad_ids.push_back("ABCD1234567890");
    bad_ids.push_back("ABCDE123456");  // five letters
    bad_ids.push_back("ABCDE12345678");  


    vector<string> bad_nuc_ids;
    bad_nuc_ids.push_back("ABC12345");

    vector<string> bad_prot_ids;
    bad_prot_ids.push_back("AY123456");
    bad_prot_ids.push_back("A12345");

    vector<string> good_ids;

    vector<string> good_nuc_ids;
    good_nuc_ids.push_back("AY123456");
    good_nuc_ids.push_back("A12345");

    vector<string> good_prot_ids;
    good_prot_ids.push_back("ABC12345");


    // bad for both
    for (vector<string>::iterator id_it = bad_ids.begin();
         id_it != bad_ids.end();
         ++id_it) {
        string id_str = *id_it;
        expected_errors[0]->SetAccession(id_str);
        expected_errors[0]->SetErrMsg("Bad accession " + id_str);

        if (id_str.length() == 12 || id_str.length() == 13) {
            expected_errors.push_back(new CExpectedError(id_str, eDiag_Error, "Inconsistent", "WGS accession should have Mol-info.tech of wgs"));
        }

        //GenBank
        scope.RemoveTopLevelSeqEntry(seh);
        nuc_entry->SetSeq().SetId().front()->SetGenbank().SetAccession(id_str);
        seh = scope.AddTopLevelSeqEntry(*nuc_entry);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        scope.RemoveTopLevelSeqEntry(seh);
        prot_entry->SetSeq().SetId().front()->SetGenbank().SetAccession(id_str);
        prot_feat->SetLocation().SetInt().SetId().SetGenbank().SetAccession(id_str);
        seh = scope.AddTopLevelSeqEntry(*prot_entry);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);

        // EMBL
        scope.RemoveTopLevelSeqEntry(seh);
        nuc_entry->SetSeq().SetId().front()->SetEmbl().SetAccession(id_str);
        seh = scope.AddTopLevelSeqEntry(*nuc_entry);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        scope.RemoveTopLevelSeqEntry(seh);
        prot_entry->SetSeq().SetId().front()->SetEmbl().SetAccession(id_str);
        prot_feat->SetLocation().SetInt().SetId().SetEmbl().SetAccession(id_str);
        seh = scope.AddTopLevelSeqEntry(*prot_entry);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);

        // DDBJ
        scope.RemoveTopLevelSeqEntry(seh);
        nuc_entry->SetSeq().SetId().front()->SetDdbj().SetAccession(id_str);
        seh = scope.AddTopLevelSeqEntry(*nuc_entry);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        scope.RemoveTopLevelSeqEntry(seh);
        prot_entry->SetSeq().SetId().front()->SetDdbj().SetAccession(id_str);
        prot_feat->SetLocation().SetInt().SetId().SetDdbj().SetAccession(id_str);
        seh = scope.AddTopLevelSeqEntry(*prot_entry);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);

        if (id_str.length() == 12 || id_str.length() == 13) {
            delete expected_errors[1];
            expected_errors.pop_back();
        }
    }

    // bad for just nucs
    for (vector<string>::iterator id_it = bad_nuc_ids.begin();
         id_it != bad_nuc_ids.end();
         ++id_it) {
        string id_str = *id_it;
        scope.RemoveTopLevelSeqEntry(seh);
        nuc_entry->SetSeq().SetId().front()->SetGenbank().SetAccession(id_str);
        expected_errors[0]->SetAccession(id_str);
        expected_errors[0]->SetErrMsg("Bad accession " + id_str);
        seh = scope.AddTopLevelSeqEntry(*nuc_entry);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
    }

    // bad for just prots
    for (vector<string>::iterator id_it = bad_prot_ids.begin();
         id_it != bad_prot_ids.end();
         ++id_it) {
        string id_str = *id_it;
        scope.RemoveTopLevelSeqEntry(seh);
        prot_entry->SetSeq().SetId().front()->SetGenbank().SetAccession(id_str);
        expected_errors[0]->SetAccession(id_str);
        expected_errors[0]->SetErrMsg("Bad accession " + id_str);
        prot_feat->SetLocation().SetInt().SetId().SetGenbank().SetAccession(id_str);
        seh = scope.AddTopLevelSeqEntry(*prot_entry);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
    }

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

    // good for both
    for (vector<string>::iterator id_it = good_ids.begin();
         id_it != good_ids.end();
         ++id_it) {
        string id_str = *id_it;
        scope.RemoveTopLevelSeqEntry(seh);
        nuc_entry->SetSeq().SetId().front()->SetGenbank().SetAccession(id_str);
        seh = scope.AddTopLevelSeqEntry(*nuc_entry);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
        scope.RemoveTopLevelSeqEntry(seh);
        prot_entry->SetSeq().SetId().front()->SetGenbank().SetAccession(id_str);
        prot_feat->SetLocation().SetInt().SetId().SetGenbank().SetAccession(id_str);
        seh = scope.AddTopLevelSeqEntry(*prot_entry);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);

    }

    // good for just prots
    for (vector<string>::iterator id_it = good_prot_ids.begin();
         id_it != good_prot_ids.end();
         ++id_it) {
        string id_str = *id_it;
        scope.RemoveTopLevelSeqEntry(seh);
        prot_entry->SetSeq().SetId().front()->SetGenbank().SetAccession(id_str);
        prot_feat->SetLocation().SetInt().SetId().SetGenbank().SetAccession(id_str);
        seh = scope.AddTopLevelSeqEntry(*prot_entry);
        eval = validator.Validate(seh, options);
        CheckErrors (*eval, expected_errors);
    }

    // if GI, needs version
    scope.RemoveTopLevelSeqEntry(seh);
    nuc_entry->SetSeq().SetId().front()->SetGenbank().SetAccession("AY123456");
    CRef<CSeq_id> gi_id(new CSeq_id("gi|21914627"));
    nuc_entry->SetSeq().SetId().push_back(gi_id);
    seh = scope.AddTopLevelSeqEntry(*nuc_entry);
    eval = validator.Validate(seh, options);
    expected_errors.push_back (new CExpectedError ("AY123456", eDiag_Critical, "BadSeqIdFormat", 
                                                   "Accession AY123456 has 0 version"));
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
    nuc_entry->SetSeq().SetId().pop_back();

    // id that is too long
    scope.RemoveTopLevelSeqEntry(seh);
    nuc_entry->SetSeq().SetId().front()->SetLocal().SetStr("ABCDEFGHIJKLMNOPQRSTUVWXYZ012345678901234");
    seh = scope.AddTopLevelSeqEntry(*nuc_entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

    // shouldn't report if ncbifile ID
    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_id> ncbifile(new CSeq_id("gnl|NCBIFILE|ABCDEFGHIJKLMNOPQRSTUVWXYZ012345678901234"));
    nuc_entry->SetSeq().SetId().front()->SetLocal().SetStr("good");
    nuc_entry->SetSeq().SetId().push_back(ncbifile);
    seh = scope.AddTopLevelSeqEntry(*nuc_entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    nuc_entry->SetSeq().SetId().pop_back();


}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_PartsOutOfOrder)
{
    CRef<CSeq_entry> entry = BuildGoodSegSet();
    CRef<CSeq_entry> master_seg = entry->SetSet().SetSeq_set().front();

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    CRef<CSeq_loc> loc4(new CSeq_loc());
    loc4->SetWhole().SetLocal().SetStr("part1");
    master_seg->SetSeq().SetInst().SetExt().SetSeg().Set().push_back(loc4);
    master_seg->SetSeq().SetInst().SetLength(240);
    expected_errors.push_back(new CExpectedError("master", eDiag_Error, "DuplicateSegmentReferences", "Segmented sequence has multiple references to lcl|part1"));
    expected_errors.push_back(new CExpectedError("master", eDiag_Error, "PartsOutOfOrder", "Parts set does not contain enough Bioseqs"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
    
    master_seg->SetSeq().SetInst().SetExt().SetSeg().Set().pop_back();
    master_seg->SetSeq().SetInst().SetExt().SetSeg().Set().pop_back();
    master_seg->SetSeq().SetInst().SetLength(120);
    expected_errors.push_back(new CExpectedError("master", eDiag_Error, "PartsOutOfOrder", "Parts set contains too many Bioseqs"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    master_seg->SetSeq().SetInst().ResetExt();
    CRef<CSeq_loc> loc1(new CSeq_loc());
    loc1->SetWhole().SetLocal().SetStr("part1");
    CRef<CSeq_loc> loc3(new CSeq_loc());
    loc3->SetWhole().SetLocal().SetStr("part3");
    CRef<CSeq_loc> loc2(new CSeq_loc());
    loc2->SetWhole().SetLocal().SetStr("part2");
    master_seg->SetSeq().SetInst().SetExt().SetSeg().Set().push_back(loc1);
    master_seg->SetSeq().SetInst().SetExt().SetSeg().Set().push_back(loc3);
    master_seg->SetSeq().SetInst().SetExt().SetSeg().Set().push_back(loc2);
    master_seg->SetSeq().SetInst().SetLength(180);
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors[0]->SetErrMsg("Segmented bioseq seq_ext does not correspond to parts packaging order");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    master_seg->SetSeq().SetInst().SetExt().SetSeg().Set().pop_back();
    master_seg->SetSeq().SetInst().SetExt().SetSeg().Set().pop_back();
    master_seg->SetSeq().SetInst().SetExt().SetSeg().Set().push_back(loc2);
    loc3->SetWhole().SetLocal().SetStr("part4");
    master_seg->SetSeq().SetInst().SetExt().SetSeg().Set().push_back(loc3);
    master_seg->SetSeq().SetInst().SetLength(120);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSet().SetSeq_set().back()->SetSet().SetSeq_set().front()->SetSet().SetClass(CBioseq_set::eClass_parts);
    expected_errors[0]->SetErrMsg("Parts set component is not Bioseq");
    expected_errors.push_back(new CExpectedError("", eDiag_Critical, "PartsSetHasSets", "Parts set contains unwanted Bioseq-set, its class is \"parts\"."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_BadSecondaryAccn)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetGenbank().SetAccession("AY123456");
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    CRef<CSeqdesc> gbdesc (new CSeqdesc());
    gbdesc->SetGenbank().SetExtra_accessions().push_back("AY123456");
    entry->SetSeq().SetDescr().Set().push_back(gbdesc);

    expected_errors.push_back(new CExpectedError("AY123456", eDiag_Error, "BadSecondaryAccn", "AY123456 used for both primary and secondary accession"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    gbdesc->SetEmbl().SetExtra_acc().push_back("AY123456");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_SEQ_INST_ZeroGiNumber)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetGi(0);
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    expected_errors.push_back(new CExpectedError("0", eDiag_Critical, "ZeroGiNumber", "Invalid GI number"));
    expected_errors.push_back(new CExpectedError("0", eDiag_Error, "GiWithoutAccession", "No accession on sequence with gi number"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_HistoryGiCollision)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetGenbank().SetAccession("AY123456");
    entry->SetSeq().SetId().front()->SetGenbank().SetVersion(1);
    CRef<CSeq_id> gi_id(new CSeq_id());
    gi_id->SetGi(21914627);
    entry->SetSeq().SetId().push_back(gi_id);
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    CRef<CSeq_id> hist_id(new CSeq_id());
    hist_id->SetGi(21914627);
    entry->SetSeq().SetInst().SetHist().SetReplaced_by().SetIds().push_back(hist_id);
    entry->SetSeq().SetInst().SetHist().SetReplaced_by().SetDate().SetStd().SetYear(2008);

    expected_errors.push_back(new CExpectedError("AY123456", eDiag_Error, "HistoryGiCollision", "Replaced by gi (21914627) is same as current Bioseq"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetHist().ResetReplaced_by();
    entry->SetSeq().SetInst().SetHist().SetReplaces().SetIds().push_back(hist_id);
    entry->SetSeq().SetInst().SetHist().SetReplaces().SetDate().SetStd().SetYear(2008);
    expected_errors[0]->SetErrMsg("Replaces gi (21914627) is same as current Bioseq");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

    // should not generate errors if date has not been set
    entry->SetSeq().SetInst().SetHist().ResetReplaces();
    entry->SetSeq().SetInst().SetHist().SetReplaced_by().SetIds().push_back(hist_id);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetHist().ResetReplaced_by();
    entry->SetSeq().SetInst().SetHist().SetReplaces().SetIds().push_back(hist_id);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    
}


BOOST_AUTO_TEST_CASE(Test_GiWithoutAccession)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetGi(123456);
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    expected_errors.push_back(new CExpectedError("123456", eDiag_Error, "GiWithoutAccession", "No accession on sequence with gi number"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_MultipleAccessions)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetGenbank().SetAccession("AY123456");
    entry->SetSeq().SetId().front()->SetGenbank().SetVersion(1);
    CRef<CSeq_id> gi_id(new CSeq_id());
    gi_id->SetGi(21914627);
    entry->SetSeq().SetId().push_back(gi_id);
    CRef<CSeq_id> other_acc(new CSeq_id());
    other_acc->SetGenbank().SetAccession("AY123457");
    other_acc->SetGenbank().SetVersion(1);
    entry->SetSeq().SetId().push_back(other_acc);
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    // genbank, ddbj, embl, tpg, tpe, tpd, other, pir, swissprot, and prf all count as accessionts
    // genbank
    expected_errors.push_back(new CExpectedError("AY123456", eDiag_Error, "ConflictingIdsOnBioseq", "Conflicting ids on a Bioseq: (gb|AY123456.1| - gb|AY123457.1|)"));
    expected_errors.push_back(new CExpectedError("AY123456", eDiag_Error, "MultipleAccessions", "Multiple accessions on sequence with gi number"));
    expected_errors.push_back(new CExpectedError("AY123456", eDiag_Warning, "UnexpectedIdentifierChange", "New accession (gb|AY123457.1|) does not match one in NCBI sequence repository (gb|AY123456.1|) on gi (21914627)"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // ddbj
    scope.RemoveTopLevelSeqEntry(seh);
    other_acc->SetDdbj().SetAccession("AY123457");
    other_acc->SetDdbj().SetVersion(1);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    delete expected_errors[2];
    expected_errors.pop_back();
    expected_errors[0]->SetErrMsg("Conflicting ids on a Bioseq: (gb|AY123456.1| - dbj|AY123457.1|)");
    CheckErrors (*eval, expected_errors);

    // embl
    scope.RemoveTopLevelSeqEntry(seh);
    other_acc->SetEmbl().SetAccession("AY123457");
    other_acc->SetEmbl().SetVersion(1);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("Conflicting ids on a Bioseq: (gb|AY123456.1| - emb|AY123457.1|)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // pir
    scope.RemoveTopLevelSeqEntry(seh);
    other_acc->SetPir().SetAccession("AY123457");
    other_acc->SetPir().SetVersion(1);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("Conflicting ids on a Bioseq: (gb|AY123456.1| - pir|AY123457.1|)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // swissprot
    scope.RemoveTopLevelSeqEntry(seh);
    other_acc->SetSwissprot().SetAccession("AY123457");
    other_acc->SetSwissprot().SetVersion(1);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("Conflicting ids on a Bioseq: (gb|AY123456.1| - sp|AY123457.1|)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // prf
    scope.RemoveTopLevelSeqEntry(seh);
    other_acc->SetPrf().SetAccession("AY123457");
    other_acc->SetPrf().SetVersion(1);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("Conflicting ids on a Bioseq: (gb|AY123456.1| - prf|AY123457.1|)");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // tpg
    scope.RemoveTopLevelSeqEntry(seh);
    other_acc->SetTpg().SetAccession("AY123457");
    other_acc->SetTpg().SetVersion(1);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("Conflicting ids on a Bioseq: (gb|AY123456.1| - tpg|AY123457.1|)");
    delete expected_errors[1];
    expected_errors[1] = new CExpectedError("AY123456", eDiag_Info, "HistAssemblyMissing", "TPA record tpg|AY123457.1| should have Seq-hist.assembly for PRIMARY block");
    expected_errors.push_back(new CExpectedError("AY123456", eDiag_Error, "MultipleAccessions", "Multiple accessions on sequence with gi number"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // tpe
    scope.RemoveTopLevelSeqEntry(seh);
    other_acc->SetTpe().SetAccession("AY123457");
    other_acc->SetTpe().SetVersion(1);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("Conflicting ids on a Bioseq: (gb|AY123456.1| - tpe|AY123457.1|)");
    expected_errors[1]->SetErrMsg("TPA record tpe|AY123457.1| should have Seq-hist.assembly for PRIMARY block");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // tpd
    scope.RemoveTopLevelSeqEntry(seh);
    other_acc->SetTpd().SetAccession("AY123457");
    other_acc->SetTpd().SetVersion(1);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("Conflicting ids on a Bioseq: (gb|AY123456.1| - tpd|AY123457.1|)");
    expected_errors[1]->SetErrMsg("TPA record tpd|AY123457.1| should have Seq-hist.assembly for PRIMARY block");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

    // other
    scope.RemoveTopLevelSeqEntry(seh);
    other_acc->SetOther().SetAccession("NC_123457");
    other_acc->SetOther().SetVersion(1);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("AY123456", eDiag_Error, "INSDRefSeqPackaging", "INSD and RefSeq records should not be present in the same set"));
    expected_errors.push_back(new CExpectedError("AY123456", eDiag_Error, "ConflictingIdsOnBioseq", "Conflicting ids on a Bioseq: (gb|AY123456.1| - ref|NC_123457.1|)"));
    expected_errors.push_back(new CExpectedError("AY123456", eDiag_Error, "MultipleAccessions", "Multiple accessions on sequence with gi number"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_HistAssemblyMissing)
{
    CRef<CSeq_entry> tpg_entry = BuildGoodSeq();
    tpg_entry->SetSeq().SetId().front()->SetTpg().SetAccession("AY123456");
    tpg_entry->SetSeq().SetId().front()->SetTpg().SetVersion(1);

    CRef<CSeq_entry> tpe_entry = BuildGoodSeq();
    tpe_entry->SetSeq().SetId().front()->SetTpe().SetAccession("AY123456");
    tpe_entry->SetSeq().SetId().front()->SetTpe().SetVersion(1);

    CRef<CSeq_entry> tpd_entry = BuildGoodSeq();
    tpd_entry->SetSeq().SetId().front()->SetTpd().SetAccession("AY123456");
    tpd_entry->SetSeq().SetId().front()->SetTpd().SetVersion(1);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*tpg_entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    // tpg
    expected_errors.push_back(new CExpectedError("AY123456", eDiag_Info, "HistAssemblyMissing", "TPA record tpg|AY123456.1| should have Seq-hist.assembly for PRIMARY block"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // tpe
    scope.RemoveTopLevelSeqEntry(seh);
    seh = scope.AddTopLevelSeqEntry(*tpe_entry);
    expected_errors[0]->SetErrMsg("TPA record tpe|AY123456.1| should have Seq-hist.assembly for PRIMARY block");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);


    // tpd
    scope.RemoveTopLevelSeqEntry(seh);
    seh = scope.AddTopLevelSeqEntry(*tpd_entry);
    expected_errors[0]->SetErrMsg("TPA record tpd|AY123456.1| should have Seq-hist.assembly for PRIMARY block");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

    // error suppressed if keyword present
    CRef<CSeqdesc> block(new CSeqdesc());
    block->SetGenbank().SetKeywords().push_back("TPA:reassembly");
    tpg_entry->SetSeq().SetDescr().Set().push_back(block);
    scope.RemoveTopLevelSeqEntry(seh);
    seh = scope.AddTopLevelSeqEntry(*tpg_entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    block->SetEmbl().SetKeywords().push_back("TPA:reassembly");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_TerminalNs)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("NNNNNNNNNAAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCCAANNNNNNNNN");
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "TerminalNs", "N at beginning of sequence"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "TerminalNs", "N at end of sequence"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // warning level changes for different conditions
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("NNNNNNNNNNAATTGGCCAAAATTGGCCAAAATTGGCCAAAATTGGCCAANNNNNNNNNN");
    expected_errors[0]->SetSeverity(eDiag_Error);
    expected_errors[1]->SetSeverity(eDiag_Error);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetTopology(CSeq_inst::eTopology_circular);
    SetCompleteness(entry, CMolInfo::eCompleteness_complete);
    expected_errors[0]->SetSeverity(eDiag_Warning);
    expected_errors[1]->SetSeverity(eDiag_Warning);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().ResetTopology();
    SetCompleteness(entry, CMolInfo::eCompleteness_unknown);
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NC_123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("NC_123456");
    expected_errors[1]->SetAccession("NC_123456");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetPatent().SetSeqid(1);
    entry->SetSeq().SetId().front()->SetPatent().SetCit().SetCountry("USA");
    entry->SetSeq().SetId().front()->SetPatent().SetCit().SetId().SetNumber("1");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("USA|1|1");
    expected_errors[1]->SetAccession("USA|1|1");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // also try delta sequence
    scope.RemoveTopLevelSeqEntry(seh);
    entry = BuildGoodDeltaSeq ();
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLiteral().SetSeq_data().SetIupacna().Set("NNNNNNNNNCCC");
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().back()->SetLiteral().SetSeq_data().SetIupacna().Set("CCCNNNNNNNNN");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("good");
    expected_errors[1]->SetAccession("good");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // 10 Ns bumps up to error
    scope.RemoveTopLevelSeqEntry(seh);
    entry = BuildGoodDeltaSeq ();
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLiteral().SetSeq_data().SetIupacna().Set("NNNNNNNNNNCC");
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().back()->SetLiteral().SetSeq_data().SetIupacna().Set("CCNNNNNNNNNN");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetSeverity(eDiag_Error);
    expected_errors[1]->SetSeverity(eDiag_Error);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // circular topology takes it back to warning
    entry->SetSeq().SetInst().SetTopology(CSeq_inst::eTopology_circular);
    SetCompleteness(entry, CMolInfo::eCompleteness_complete);
    expected_errors[0]->SetSeverity(eDiag_Warning);
    expected_errors[1]->SetSeverity(eDiag_Warning);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // NC and patent IDs keep warning
    entry->SetSeq().SetInst().ResetTopology();
    SetCompleteness(entry, CMolInfo::eCompleteness_unknown);
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NC_123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("NC_123456");
    expected_errors[1]->SetAccession("NC_123456");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetPatent().SetSeqid(1);
    entry->SetSeq().SetId().front()->SetPatent().SetCit().SetCountry("USA");
    entry->SetSeq().SetId().front()->SetPatent().SetCit().SetId().SetNumber("1");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("USA|1|1");
    expected_errors[1]->SetAccession("USA|1|1");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_UnexpectedIdentifierChange)
{
    CRef<CSeq_entry> entry = BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetGenbank().SetAccession("AY123457");
    entry->SetSeq().SetId().front()->SetGenbank().SetVersion(1);
    CRef<CSeq_id> gi_id(new CSeq_id());
    gi_id->SetGi(21914627);
    entry->SetSeq().SetId().push_back(gi_id);
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("AY123457", eDiag_Warning, "UnexpectedIdentifierChange", "New accession (gb|AY123457.1|) does not match one in NCBI sequence repository (gb|AY123456.1|) on gi (21914627)"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetTpg().SetAccession("AY123456");
    entry->SetSeq().SetId().front()->SetTpg().SetVersion(1);
    seh = scope.AddTopLevelSeqEntry(*entry);
    delete expected_errors[0];
    expected_errors[0] = new CExpectedError("AY123456", eDiag_Info, "HistAssemblyMissing", "TPA record tpg|AY123456.1| should have Seq-hist.assembly for PRIMARY block");
    expected_errors.push_back(new CExpectedError("AY123456", eDiag_Warning, "UnexpectedIdentifierChange", "Loss of accession (gb|AY123456.1|) on gi (21914627) compared to the NCBI sequence repository"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // TODO - try to instigate other errors

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_InternalNsInSeqLit)
{
    CRef<CSeq_entry> entry = BuildGoodDeltaSeq();
    AddToDeltaSeq(entry, "ANNNNNNNNNNNNNNNNNNNNG");
    SetTech(entry, CMolInfo::eTech_wgs);
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "InternalNsInSeqLit", "Run of 20 Ns in delta component 5 that starts at base 45"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    AddToDeltaSeq(entry, "ANNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNG");
    SetTech(entry, CMolInfo::eTech_htgs_1);
    expected_errors[0]->SetErrMsg("Run of 81 Ns in delta component 7 that starts at base 77");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_htgs_2);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_composite_wgs_htgs);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    AddToDeltaSeq(entry, "ANNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNG");
    SetTech(entry, CMolInfo::eTech_unknown);
    expected_errors[0]->SetErrMsg("Run of 101 Ns in delta component 9 that starts at base 170");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_SeqLitGapLength0)
{
    CRef<CSeq_entry> entry = BuildGoodDeltaSeq();
    CRef<CDelta_seq> delta_seq(new CDelta_seq());
    delta_seq->SetLiteral().SetLength(0);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(delta_seq);
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "SeqLitGapLength0", "Gap of length 0 in delta chain"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "BadDeltaSeq", "Last delta seq component is a gap"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // some kinds of fuzz don't trigger other kind of error
    delta_seq->SetLiteral().SetFuzz().SetLim(CInt_fuzz::eLim_gt);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    delta_seq->SetLiteral().SetFuzz().Reset();
    delta_seq->SetLiteral().SetFuzz().SetP_m(10);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // others will
    delta_seq->SetLiteral().SetFuzz().Reset();
    delta_seq->SetLiteral().SetFuzz().SetLim(CInt_fuzz::eLim_unk);
    expected_errors[0]->SetErrMsg("Gap of length 0 with unknown fuzz in delta chain");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // try again with swissprot, error goes to warning
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetSwissprot().SetAccession("AY123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetSeverity(eDiag_Warning);
    expected_errors[0]->SetAccession("AY123456");
    expected_errors[1]->SetAccession("AY123456");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    delta_seq->SetLiteral().SetFuzz().SetP_m(10);
    expected_errors[0]->SetErrMsg("Gap of length 0 in delta chain");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    delta_seq->SetLiteral().SetFuzz().Reset();
    delta_seq->SetLiteral().SetFuzz().SetLim(CInt_fuzz::eLim_gt);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    delta_seq->SetLiteral().ResetFuzz();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


static void AddTpaAssemblyUserObject(CRef<CSeq_entry> entry)
{
    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetUser().SetType().SetStr("TpaAssembly");
    entry->SetSeq().SetDescr().Set().push_back(desc);
}


BOOST_AUTO_TEST_CASE(Test_TpaAssmeblyProblem)
{
    CRef<CSeq_entry> entry(new CSeq_entry());
    entry->SetSet().SetClass(CBioseq_set::eClass_genbank);
    CRef<CSeq_entry> member1 = BuildGoodSeq();
    member1->SetSeq().SetId().front()->SetLocal().SetStr("good1");
    AddTpaAssemblyUserObject(member1);
    entry->SetSet().SetSeq_set().push_back(member1);
    CRef<CSeq_entry> member2 = BuildGoodSeq();
    member2->SetSeq().SetId().front()->SetLocal().SetStr("good2");
    AddTpaAssemblyUserObject(member2);
    entry->SetSet().SetSeq_set().push_back(member2);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    // two Tpa sequences, but neither has assembly and neither has GI, so no errors expected
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // now one has hist, other does not
    member1->SetSeq().SetInst().SetHist().SetAssembly().push_back(BuildGoodAlign());
    expected_errors.push_back(new CExpectedError("", eDiag_Error, "TpaAssmeblyProblem", "There are 1 TPAs with history and 1 without history in this record."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // now one has gi
    scope.RemoveTopLevelSeqEntry(seh);
    member1->SetSeq().SetId().front()->SetTpg().SetAccession("AY123456");
    member1->SetSeq().SetId().front()->SetTpg().SetVersion(1);
    CRef<CSeq_id> gi_id(new CSeq_id());
    gi_id->SetGi(21914627);
    member1->SetSeq().SetId().push_back(gi_id);
    seh = scope.AddTopLevelSeqEntry(*entry);
    delete expected_errors[0];
    expected_errors[0] = new CExpectedError("AY123456", eDiag_Warning, "UnexpectedIdentifierChange", "Loss of accession (gb|AY123456.1|) on gi (21914627) compared to the NCBI sequence repository");
    expected_errors.push_back(new CExpectedError("", eDiag_Error, "TpaAssmeblyProblem", "There are 1 TPAs with history and 1 without history in this record."));
    expected_errors.push_back(new CExpectedError("", eDiag_Warning, "TpaAssmeblyProblem", "There are 1 TPAs without history in this record, but the record has a gi number assignment."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_SeqLocLength)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodDeltaSeq();
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetInt().SetId().SetGenbank().SetAccession("AY123456");
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetInt().SetFrom(0);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetInt().SetTo(9);
    entry->SetSeq().SetInst().SetLength(32);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "SeqLocLength", "Short length (10) on seq-loc (gb|AY123456|:1-10) of delta seq_ext"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

    // if length 11, should not be a problem
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetInt().SetTo(10);
    entry->SetSeq().SetInst().SetLength(33);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
}


BOOST_AUTO_TEST_CASE(Test_MissingGaps)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodDeltaSeq();
    // remove gaps
    RemoveDeltaSeqGaps (entry);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    // only report errors for specific molinfo tech values
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    // htgs_3 should not report
    SetTech(entry, CMolInfo::eTech_htgs_3);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    
    SetTech(entry, CMolInfo::eTech_htgs_0);
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "MissingGaps", "HTGS delta seq should have gaps between all sequence runs"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_htgs_1);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_htgs_2);
    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "BadHTGSeq", "HTGS 2 delta seq has no gaps and no graphs"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // RefGeneTracking changes severity
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NC_123456");
    AddRefGeneTrackingUserObject(entry);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetSeverity(eDiag_Info);
    expected_errors[0]->SetAccession("NC_123456");
    expected_errors[1]->SetAccession("NC_123456");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    delete expected_errors[1];
    expected_errors.pop_back();

    SetTech(entry, CMolInfo::eTech_htgs_1);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_htgs_0);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_CompleteTitleProblem)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodSeq();
    entry->SetSeq().SetId().front()->SetGenbank().SetAccession("AY123456");
    SetTitle(entry, "Foo complete genome");

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("AY123456", eDiag_Warning, "CompleteTitleProblem", "Complete genome in title without complete flag set"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
    // should be no error if complete
    SetCompleteness(entry, CMolInfo::eCompleteness_complete);

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_CompleteCircleProblem)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodSeq();
    entry->SetSeq().SetInst().SetTopology(CSeq_inst::eTopology_circular);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "CompleteCircleProblem", "Circular topology without complete flag set"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetGenbank().SetAccession("AY123456");
    SetTitle(entry, "This is just a title");
    SetCompleteness(entry, CMolInfo::eCompleteness_complete);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("AY123456", eDiag_Warning, "CompleteCircleProblem", "Circular topology has complete flag set, but title should say complete sequence or complete genome"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

}


BOOST_AUTO_TEST_CASE(Test_BadHTGSeq)
{
    // prepare entry
    CRef<CSeq_entry> delta_entry = BuildGoodDeltaSeq();
    // remove gaps
    RemoveDeltaSeqGaps (delta_entry);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*delta_entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    SetTech(delta_entry, CMolInfo::eTech_htgs_2);
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "MissingGaps", "HTGS delta seq should have gaps between all sequence runs"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "BadHTGSeq", "HTGS 2 delta seq has no gaps and no graphs"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    delete expected_errors[1];
    expected_errors.pop_back();

    // HTGS_ACTIVEFIN keyword disables BadHTGSeq error
    AddGenbankKeyword(delta_entry, "HTGS_ACTIVEFIN");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_entry> raw_entry = BuildGoodSeq();
    SetTech(raw_entry, CMolInfo::eTech_htgs_2);
    seh = scope.AddTopLevelSeqEntry(*raw_entry);
    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "BadHTGSeq", "HTGS 2 raw seq has no gaps and no graphs"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

    // HTGS_ACTIVEFIN keyword disables error
    AddGenbankKeyword(raw_entry, "HTGS_ACTIVEFIN");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);


    // htg3 errors
    SetTech(raw_entry, CMolInfo::eTech_htgs_3);
    AddGenbankKeyword(raw_entry, "HTGS_DRAFT");
    AddGenbankKeyword(raw_entry, "HTGS_PREFIN");
    AddGenbankKeyword(raw_entry, "HTGS_FULLTOP");
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "BadHTGSeq", "HTGS 3 sequence should not have HTGS_DRAFT keyword"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "BadHTGSeq", "HTGS 3 sequence should not have HTGS_PREFIN keyword"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "BadHTGSeq", "HTGS 3 sequence should not have HTGS_ACTIVEFIN keyword"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "BadHTGSeq", "HTGS 3 sequence should not have HTGS_FULLTOP keyword"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    
    scope.RemoveTopLevelSeqEntry(seh);
    seh = scope.AddTopLevelSeqEntry(*delta_entry);
    SetTech(delta_entry, CMolInfo::eTech_htgs_3);
    AddGenbankKeyword(delta_entry, "HTGS_DRAFT");
    AddGenbankKeyword(delta_entry, "HTGS_PREFIN");
    AddGenbankKeyword(delta_entry, "HTGS_FULLTOP");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_GapInProtein_and_BadProteinStart)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodProtSeq();
    entry->SetSeq().SetInst().SetSeq_data().SetNcbieaa().Set("PRK-EIN");

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "GapInProtein", "[1] internal gap symbols in protein sequence (gene? - fake protein name)"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

    entry->SetSeq().SetInst().SetSeq_data().SetNcbieaa().Set("-RKTEIN");
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "BadProteinStart", "gap symbol at start of protein sequence (gene? - fake protein name)"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetSeq_data().SetNcbieaa().Set("-RK-EIN");
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "GapInProtein", "[1] internal gap symbols in protein sequence (gene? - fake protein name)"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_TerminalGap)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodDeltaSeq();
    CRef<CDelta_seq> first_seg(new CDelta_seq());
    first_seg->SetLiteral().SetLength(9);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().push_front(first_seg);
    CRef<CDelta_seq> last_seg(new CDelta_seq());
    last_seg->SetLiteral().SetLength(9);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().push_back(last_seg);
    entry->SetSeq().SetInst().SetLength(entry->SetSeq().SetInst().GetLength() + 18);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "BadDeltaSeq", "First delta seq component is a gap"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "BadDeltaSeq", "Last delta seq component is a gap"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "TerminalGap", "Gap at beginning of sequence"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "TerminalGap", "Gap at end of sequence"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLiteral().SetLength(10);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().back()->SetLiteral().SetLength(10);
    entry->SetSeq().SetInst().SetLength(entry->SetSeq().SetInst().GetLength() + 2);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[2]->SetSeverity(eDiag_Error);
    expected_errors[3]->SetSeverity(eDiag_Error);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().SetTopology(CSeq_inst::eTopology_circular);
    SetCompleteness(entry, CMolInfo::eCompleteness_complete);
    expected_errors[2]->SetSeverity(eDiag_Warning);
    expected_errors[3]->SetSeverity(eDiag_Warning);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().ResetTopology();
    SetCompleteness(entry, CMolInfo::eCompleteness_unknown);
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NC_123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("NC_123456");
    expected_errors[1]->SetAccession("NC_123456");
    expected_errors[2]->SetAccession("NC_123456");
    expected_errors[3]->SetAccession("NC_123456");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetPatent().SetSeqid(1);
    entry->SetSeq().SetId().front()->SetPatent().SetCit().SetCountry("USA");
    entry->SetSeq().SetId().front()->SetPatent().SetCit().SetId().SetNumber("1");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetAccession("USA|1|1");
    expected_errors[1]->SetAccession("USA|1|1");
    expected_errors[2]->SetAccession("USA|1|1");
    expected_errors[3]->SetAccession("USA|1|1");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_OverlappingDeltaRange)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodDeltaSeq();
    entry->SetSeq().SetInst().ResetExt();
    CRef<CSeq_id> seqid(new CSeq_id());
    seqid->SetGenbank().SetAccession("AY123456");
    entry->SetSeq().SetInst().SetExt().SetDelta().AddSeqRange(*seqid, 0, 10);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddSeqRange(*seqid, 5, 15);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddSeqRange(*seqid, 20, 30);
    entry->SetSeq().SetInst().SetExt().SetDelta().AddSeqRange(*seqid, 25, 35);
    entry->SetSeq().SetInst().SetLength(44);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "OverlappingDeltaRange", "Overlapping delta range 6-16 and 1-11 on a Bioseq gb|AY123456"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "OverlappingDeltaRange", "Overlapping delta range 26-36 and 21-31 on a Bioseq gb|AY123456"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_LeadingX)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodProtSeq();
    entry->SetSeq().SetInst().SetSeq_data().SetIupacaa().Set("XROTEIN");

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "LeadingX", "Sequence starts with leading X"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_InternalNsInSeqRaw)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodSeq();
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AAAAANNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNTTTTT");
    entry->SetSeq().SetInst().SetLength(110);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "InternalNsInSeqRaw", "Run of 100 Ns in raw sequence starting at base 6"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

    // expect no error
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AAAAANNNNNNNNNNNNNNNNNNNNTTTTT");
    entry->SetSeq().SetInst().SetLength(30);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // WGS has lower threshold
    SetTech (entry, CMolInfo::eTech_wgs);
    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "InternalNsInSeqRaw", "Run of 20 Ns in raw sequence starting at base 6"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_InternalNsAdjacentToGap)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodDeltaSeq();
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLiteral().SetSeq_data().SetIupacna().Set("ATGATGATGNNN");
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().back()->SetLiteral().SetSeq_data().SetIupacna().Set("NNNATGATGATG");
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "InternalNsAdjacentToGap", "Ambiguous residue N is adjacent to a gap around position 13"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "InternalNsAdjacentToGap", "Ambiguous residue N is adjacent to a gap around position 23"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

}


BOOST_AUTO_TEST_CASE(Test_DeltaComponentIsGi0)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodDeltaSeq();
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetInt().SetFrom(0);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetInt().SetTo(11);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetInt().SetId().SetGi(0);
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "DeltaComponentIsGi0", "Delta component is gi|0"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

}


BOOST_AUTO_TEST_CASE(Test_InternalGapsInSeqRaw)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodSeq();
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AATTGGCCAAAATTGGCCAAAATTGG-CAAAATTGGCCAAAATTGGCCAAAATTGGCCAA");
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "InvalidResidue", "Invalid residue '-' at position [27]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "InternalGapsInSeqRaw", "Raw nucleotide should not contain gap characters"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

}


BOOST_AUTO_TEST_CASE(Test_SelfReferentialSequence)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodDeltaSeq();
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetInt().SetFrom(0);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetInt().SetTo(11);
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetInt().SetId().SetLocal().SetStr("good");
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "SelfReferentialSequence", "Self-referential delta sequence"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "Exception", "Exeption while validating multi-interval genes."));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "Exception", "Exeption while validating Seqfeat Context."));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "Exception", "Exeption while validating duplicate/overlapping features."));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "Exception", "Exeption while validating source features."));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "Exception", "Exeption while validating pub features."));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "Exception", "Exeption while validating colliding genes."));


    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

}


BOOST_AUTO_TEST_CASE(Test_WholeComponent)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodDeltaSeq();
    entry->SetSeq().SetInst().SetExt().SetDelta().Set().front()->SetLoc().SetWhole().SetGenbank().SetAccession("AY123456");
    entry->SetSeq().SetInst().SetLength(507);
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "WholeComponent", "Delta seq component should not be of type whole"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

}


BOOST_AUTO_TEST_CASE(Test_ProteinsHaveGeneralID)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodProtSeq();
    entry->SetSeq().SetId().front()->SetGeneral().SetDb("a");
    entry->SetSeq().SetId().front()->SetGeneral().SetTag().SetStr("b");
    entry->SetSeq().SetAnnot().front()->SetData().SetFtable().front()->SetLocation().SetInt().SetId().SetGeneral().SetDb("a");
    entry->SetSeq().SetAnnot().front()->SetData().SetFtable().front()->SetLocation().SetInt().SetId().SetGeneral().SetTag().SetStr("b");

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    // no error unless part of nuc-prot set
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    
    scope.RemoveTopLevelSeqEntry(seh);
    entry = BuildGoodNucProtSet();
    entry->SetSet().SetSeq_set().back()->SetSeq().SetId().front()->SetGeneral().SetDb("a");
    entry->SetSet().SetSeq_set().back()->SetSeq().SetId().front()->SetGeneral().SetTag().SetStr("b");
    entry->SetSet().SetSeq_set().back()->SetSeq().SetAnnot().front()->SetData().SetFtable().front()->SetLocation().SetInt().SetId().SetGeneral().SetDb("a");
    entry->SetSet().SetSeq_set().back()->SetSeq().SetAnnot().front()->SetData().SetFtable().front()->SetLocation().SetInt().SetId().SetGeneral().SetTag().SetStr("b");
    entry->SetSet().SetAnnot().front()->SetData().SetFtable().front()->SetProduct().SetWhole().SetGeneral().SetDb("a");
    entry->SetSet().SetAnnot().front()->SetData().SetFtable().front()->SetProduct().SetWhole().SetGeneral().SetTag().SetStr("b");
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors.push_back(new CExpectedError("", eDiag_Info, "ProteinsHaveGeneralID", "INDEXER_ONLY - Protein bioseqs have general seq-id."));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

}


BOOST_AUTO_TEST_CASE(Test_HighNContentPercent_and_HighNContentStretch)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodSeq();
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AAAAATTTTTGGGGGCCCCCAAAAATTTTTGGGGGCCCCCNNNNNNNNNNNAAAATTTTTGGGGGCCCCCAAAAATTTTTGGGGGCCCCCAAAAATTTTT");
    entry->SetSeq().SetInst().SetLength(100);
    SetTech (entry, CMolInfo::eTech_tsa);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "HighNContentPercent", "Sequence contains 11 percent Ns"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AAAAATTTTTGGGGGCCCCCAAAAATTTTTGGGGGCCCCCNNNNNNNNNNNNNNNTTTTTGGGGGCCCCCAAAAATTTTTGGGGGCCCCCAAAAATTTTT");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("Sequence contains 15 percent Ns");
    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "HighNContentStretch", "Sequence has a stretch of 15 Ns"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    
    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetInst().SetSeq_data().SetIupacna().Set("AAAAANNNNNGGGGGCCCCCAAAAATTTTTGGGGGCCCCCAAAAATTTTTGGGGGTTTTTGGGGGCCCCCAAAAATTTTTGGGGGCCCCCNNNNNAAAAA");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "HighNContentStretch", "Sequence has a stretch of at least 5 Ns within the first 20 bases"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "HighNContentStretch", "Sequence has a stretch of at least 5 Ns within the last 20 bases"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

}


BOOST_AUTO_TEST_CASE(Test_SeqLitDataLength0)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodDeltaSeq();

    CDelta_ext::Tdata::iterator seg_it = entry->SetSeq().SetInst().SetExt().SetDelta().Set().begin();
    ++seg_it;
    (*seg_it)->SetLiteral().SetSeq_data().SetIupacna().Set();
    (*seg_it)->SetLiteral().SetLength(0);

    entry->SetSeq().SetInst().SetLength(24);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "SeqLitDataLength0", "Seq-lit of length 0 in delta chain"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

}


BOOST_AUTO_TEST_CASE(Test_DSmRNA)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodSeq();
    entry->SetSeq().SetInst().SetMol(CSeq_inst::eMol_rna);
    SetBiomol(entry, CMolInfo::eBiomol_mRNA);
    entry->SetSeq().SetInst().SetStrand(CSeq_inst::eStrand_ds);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    // double strand
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "DSmRNA", "mRNA not single stranded"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // mixed strand
    entry->SetSeq().SetInst().SetStrand(CSeq_inst::eStrand_mixed);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // mixed strand
    entry->SetSeq().SetInst().SetStrand(CSeq_inst::eStrand_other);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

    // these should not produce errors

    // strand not set
    entry->SetSeq().SetInst().SetStrand(CSeq_inst::eStrand_not_set);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    entry->SetSeq().SetInst().ResetStrand();
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // single strand
    entry->SetSeq().SetInst().SetStrand(CSeq_inst::eStrand_ss);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
}


BOOST_AUTO_TEST_CASE(Test_BioSourceMissing)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodNucProtSet();
    RemoveDescriptorType (entry, CSeqdesc::e_Source);
    AddGoodSource (entry->SetSet().SetSeq_set().front());

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    expected_errors.push_back(new CExpectedError("nuc", eDiag_Warning, "BioSourceMissing", "Nuc-prot set does not contain expected BioSource descriptor"));
    expected_errors.push_back(new CExpectedError("prot", eDiag_Error, "NoOrgFound", "No organism name has been applied to this Bioseq.  Other qualifiers may exist."));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


static void SetErrorsAccessions (vector< CExpectedError *> & expected_errors, string accession)
{
    size_t i, len = expected_errors.size();
    for (i = 0; i < len; i++) {
        expected_errors[i]->SetAccession(accession);
    }
}


BOOST_AUTO_TEST_CASE(Test_Descr_InvalidForType)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodSeq();
    CRef<CSeqdesc> desc;
    desc.Reset(new CSeqdesc());
    desc->SetMol_type(eGIBB_mol_genomic);
    entry->SetDescr().Set().push_back(desc);
    desc.Reset(new CSeqdesc());
    desc->SetModif().push_back(eGIBB_mod_dna);
    entry->SetDescr().Set().push_back(desc);
    desc.Reset(new CSeqdesc());
    desc->SetMethod(eGIBB_method_other);
    entry->SetDescr().Set().push_back(desc);
    desc.Reset(new CSeqdesc());
    desc->SetOrg().SetTaxname("Sebaea microphylla");
    entry->SetDescr().Set().push_back(desc);
    AddTpaAssemblyUserObject (entry);
   

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "InvalidForType",
                              "Nucleic acid with protein sequence method"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "InvalidForType",
                              "MolType descriptor is obsolete"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "InvalidForType",
                              "Modif descriptor is obsolete"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "InvalidForType",
                              "Method descriptor is obsolete"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "InvalidForType",
                              "OrgRef descriptor is obsolete"));

    // won't complain about TPA assembly if only local ID
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetGenbank().SetAccession("AY123456");
    RemoveDescriptorType (entry, CSeqdesc::e_Mol_type);
    RemoveDescriptorType (entry, CSeqdesc::e_Modif);
    RemoveDescriptorType (entry, CSeqdesc::e_Method);
    RemoveDescriptorType (entry, CSeqdesc::e_Org);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "InvalidForType",
                              "Non-TPA record AY123456 should not have TpaAssembly object"));
    SetErrorsAccessions(expected_errors, "AY123456");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NC_123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    SetErrorsAccessions(expected_errors, "NC_123456");
    expected_errors[0]->SetErrMsg("Non-TPA record NC_123456 should not have TpaAssembly object");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    desc.Reset(new CSeqdesc());
    desc->SetMol_type(eGIBB_mol_peptide);
    entry->SetDescr().Set().push_back(desc);
    expected_errors.push_back(new CExpectedError("NC_123456", eDiag_Error, "InvalidForType",
                              "Nucleic acid with GIBB-mol = peptide"));
    expected_errors.push_back(new CExpectedError("NC_123456", eDiag_Error, "InvalidForType",
                              "MolType descriptor is obsolete"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    desc->SetMol_type(eGIBB_mol_other);
    expected_errors[1]->SetErrMsg("GIBB-mol unknown or other used");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    desc->SetMol_type(eGIBB_mol_unknown);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
    
    scope.RemoveTopLevelSeqEntry(seh);
    entry = BuildGoodProtSeq();
    desc.Reset(new CSeqdesc());
    desc->SetMol_type(eGIBB_mol_genomic);
    entry->SetDescr().Set().push_back(desc);
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "InvalidForType",
                              "GIBB-mol [1] used on protein"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "InvalidForType",
                              "MolType descriptor is obsolete"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    desc->SetMol_type(eGIBB_mol_pre_mRNA);
    expected_errors[0]->SetErrMsg("GIBB-mol [2] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    desc->SetMol_type(eGIBB_mol_mRNA);
    expected_errors[0]->SetErrMsg("GIBB-mol [3] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    desc->SetMol_type(eGIBB_mol_rRNA);
    expected_errors[0]->SetErrMsg("GIBB-mol [4] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    desc->SetMol_type(eGIBB_mol_tRNA);
    expected_errors[0]->SetErrMsg("GIBB-mol [5] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    desc->SetMol_type(eGIBB_mol_snRNA);
    expected_errors[0]->SetErrMsg("GIBB-mol [6] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    desc->SetMol_type(eGIBB_mol_scRNA);
    expected_errors[0]->SetErrMsg("GIBB-mol [7] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    desc->SetMol_type(eGIBB_mol_other_genetic);
    expected_errors[0]->SetErrMsg("GIBB-mol [9] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    desc->SetMol_type(eGIBB_mol_genomic_mRNA);
    expected_errors[0]->SetErrMsg("GIBB-mol [10] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
 
    // invalid modif
    desc->SetModif().push_back(eGIBB_mod_dna);
    desc->SetModif().push_back(eGIBB_mod_rna);
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "InvalidForType",
                              "Nucleic acid GIBB-mod [0] on protein"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "InvalidForType",
                              "Nucleic acid GIBB-mod [1] on protein"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "InvalidForType",
                              "Modif descriptor is obsolete"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

    scope.RemoveTopLevelSeqEntry(seh);
    entry = BuildGoodSeq();
    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsSource()) {
            (*it)->SetSource().SetOrigin(CBioSource::eOrigin_synthetic);
        }
    }
    seh = scope.AddTopLevelSeqEntry(*entry);
    // if biomol not other, should generate error
    expected_errors.push_back(new CExpectedError ("good", eDiag_Warning, "InvalidForType",
                                                  "artificial origin should have other-genetic"));
    expected_errors.push_back(new CExpectedError ("good", eDiag_Warning, "InvalidForType",
                                                  "Molinfo-biomol other should be used if Biosource-location is synthetic"));
    expected_errors.push_back(new CExpectedError ("good", eDiag_Warning, "InvalidForType",
                                                  "artificial origin should have other-genetic and synthetic construct"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

    NON_CONST_ITERATE (CSeq_descr::Tdata, it, entry->SetSeq().SetDescr().Set()) {
        if ((*it)->IsSource()) {
            (*it)->SetSource().ResetOrigin();
        }
    }

    SetBiomol (entry, CMolInfo::eBiomol_peptide);
    expected_errors.push_back(new CExpectedError ("good", eDiag_Error, "InvalidForType",
                                                  "Nucleic acid with Molinfo-biomol = peptide"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetBiomol (entry, CMolInfo::eBiomol_other_genetic);
    expected_errors[0]->SetErrMsg("Molinfo-biomol = other genetic");
    expected_errors[0]->SetSeverity(eDiag_Warning);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetBiomol (entry, CMolInfo::eBiomol_unknown);
    expected_errors[0]->SetErrMsg("Molinfo-biomol unknown used");
    expected_errors[0]->SetSeverity(eDiag_Error);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetBiomol (entry, CMolInfo::eBiomol_other);
    expected_errors[0]->SetErrMsg("Molinfo-biomol other used");
    expected_errors[0]->SetSeverity(eDiag_Warning);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry = BuildGoodProtSeq();
    seh = scope.AddTopLevelSeqEntry(*entry);

    expected_errors[0]->SetSeverity(eDiag_Error);
    SetBiomol(entry, CMolInfo::eBiomol_genomic);
    expected_errors[0]->SetErrMsg("Molinfo-biomol [1] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetBiomol(entry, CMolInfo::eBiomol_pre_RNA);
    expected_errors[0]->SetErrMsg("Molinfo-biomol [2] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetBiomol(entry, CMolInfo::eBiomol_mRNA);
    expected_errors[0]->SetErrMsg("Molinfo-biomol [3] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetBiomol(entry, CMolInfo::eBiomol_rRNA);
    expected_errors[0]->SetErrMsg("Molinfo-biomol [4] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetBiomol(entry, CMolInfo::eBiomol_tRNA);
    expected_errors[0]->SetErrMsg("Molinfo-biomol [5] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetBiomol(entry, CMolInfo::eBiomol_snRNA);
    expected_errors[0]->SetErrMsg("Molinfo-biomol [6] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetBiomol(entry, CMolInfo::eBiomol_scRNA);
    expected_errors[0]->SetErrMsg("Molinfo-biomol [7] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetBiomol(entry, CMolInfo::eBiomol_genomic_mRNA);
    expected_errors[0]->SetErrMsg("Molinfo-biomol [10] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetBiomol(entry, CMolInfo::eBiomol_cRNA);
    expected_errors[0]->SetErrMsg("Molinfo-biomol [11] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetBiomol(entry, CMolInfo::eBiomol_snoRNA);
    expected_errors[0]->SetErrMsg("Molinfo-biomol [12] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetBiomol(entry, CMolInfo::eBiomol_transcribed_RNA);
    expected_errors[0]->SetErrMsg("Molinfo-biomol [13] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetBiomol(entry, CMolInfo::eBiomol_ncRNA);
    expected_errors[0]->SetErrMsg("Molinfo-biomol [14] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetBiomol(entry, CMolInfo::eBiomol_tmRNA);
    expected_errors[0]->SetErrMsg("Molinfo-biomol [15] used on protein");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

    scope.RemoveTopLevelSeqEntry(seh);
    entry = BuildGoodSeq();
    seh = scope.AddTopLevelSeqEntry(*entry);
    SetTaxname(entry, "synthetic construct");
    expected_errors.push_back(new CExpectedError ("good", eDiag_Warning, "InvalidForType",
                                                  "synthetic construct should have other-genetic"));
    expected_errors.push_back(new CExpectedError ("good", eDiag_Warning, "InvalidForType",
                                                  "synthetic construct should have artificial origin"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
    SetTaxname(entry, "Sebaea microphylla");

    SetTech(entry, CMolInfo::eTech_concept_trans);
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "InvalidForType",
                                                 "Nucleic acid with protein sequence method"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_seq_pept);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    
    SetTech(entry, CMolInfo::eTech_both);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_seq_pept_overlap);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    
    SetTech(entry, CMolInfo::eTech_seq_pept_homol);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_concept_trans_a);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    entry = BuildGoodProtSeq();
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetErrMsg("Protein with nucleic acid sequence method");

    SetTech(entry, CMolInfo::eTech_est);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_genemap);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_physmap);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_fli_cdna);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_htc);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "ConflictingBiomolTech",
                                                 "HTGS/STS/GSS/WGS sequence should be genomic"));
    SetTech(entry, CMolInfo::eTech_sts);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_htgs_1);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_htgs_3);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_htgs_0);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_wgs);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetTech(entry, CMolInfo::eTech_composite_wgs_htgs);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "BadHTGSeq",
                                                 "HTGS 2 raw seq has no gaps and no graphs"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "InvalidForType",
                                                 "Protein with nucleic acid sequence method"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "ConflictingBiomolTech",
                                                 "HTGS/STS/GSS/WGS sequence should be genomic"));

    SetTech(entry, CMolInfo::eTech_htgs_2);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

    expected_errors.push_back(new CExpectedError("good", eDiag_Info, "BadKeyword",
                                                 "Molinfo.tech barcode without BARCODE keyword"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "InvalidForType",
                                                 "Protein with nucleic acid sequence method"));

    SetTech(entry, CMolInfo::eTech_barcode);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

}


BOOST_AUTO_TEST_CASE(Test_Descr_Unknown)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodSeq();
    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetModif().push_back(eGIBB_mod_other);
    entry->SetDescr().Set().push_back(desc);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "InvalidForType",
                              "Modif descriptor is obsolete"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "Unknown",
                              "GIBB-mod = other used"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


static CRef<CSeq_entry> MakeGps(CRef<CSeq_entry> member)
{
    CRef<CSeq_entry> set(new CSeq_entry());
    set->SetSet().SetClass(CBioseq_set::eClass_gen_prod_set);
    set->SetSet().SetSeq_set().push_back(member);
    return set;
}


BOOST_AUTO_TEST_CASE(Test_Descr_NoPubFound)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodNucProtSet();
    RemoveDescriptorType (entry, CSeqdesc::e_Pub);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    expected_errors.push_back(new CExpectedError("", eDiag_Error, "NoPubFound",
                              "No publications anywhere on this entire record."));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
    // make gpipe - should suppress error
    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_id> id_suppress(new CSeq_id());
    id_suppress->SetGpipe().SetAccession("AY123456");
    entry->SetSet().SetSeq_set().front()->SetSeq().SetId().push_back(id_suppress);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    // make noncurated refseq
    scope.RemoveTopLevelSeqEntry(seh);
    id_suppress->SetOther().SetAccession("NX_123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    // make GPS - will suppress pub errors, although introduce gps erros
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSet().SetSeq_set().front()->SetSeq().SetId().pop_back();
    CRef<CSeq_entry> gps = MakeGps(entry);
    seh = scope.AddTopLevelSeqEntry(*gps);
    expected_errors.push_back(new CExpectedError("nuc", eDiag_Warning, 
                              "GenomicProductPackagingProblem", 
                              "Nucleotide bioseq should be product of mRNA feature on contig, but is not"));
    expected_errors.push_back(new CExpectedError("prot", eDiag_Warning, 
                              "GenomicProductPackagingProblem", 
                              "Protein bioseq should be product of CDS feature on contig, but is not"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

    // only one has pub
    scope.RemoveTopLevelSeqEntry(seh);
    entry = BuildGoodNucProtSet();
    RemoveDescriptorType (entry, CSeqdesc::e_Pub);
    AddGoodPub(entry->SetSet().SetSeq_set().front());
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors.push_back(new CExpectedError("prot", eDiag_Error, "NoPubFound",
                              "No publications refer to this Bioseq."));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

    // noncurated refseq should suppress
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSet().SetSeq_set().back()->SetSeq().SetId().push_back(id_suppress);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // intermediate wgs should suppress
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSet().SetSeq_set().back()->SetSeq().SetId().pop_back();
    id_suppress->SetOther().SetAccession("NC_123456");
    entry->SetSet().SetSeq_set().front()->SetSeq().SetId().push_back(id_suppress);
    SetTech (entry->SetSet().SetSeq_set().front(), CMolInfo::eTech_wgs);
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


BOOST_AUTO_TEST_CASE(Test_Descr_NoOrgFound)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodNucProtSet();
    RemoveDescriptorType (entry, CSeqdesc::e_Source);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    expected_errors.push_back(new CExpectedError("nuc", eDiag_Warning, "BioSourceMissing",
                              "Nuc-prot set does not contain expected BioSource descriptor"));
    expected_errors.push_back(new CExpectedError("", eDiag_Error, "NoOrgFound",
                              "No organism name anywhere on this entire record."));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    delete expected_errors[1];
    expected_errors.pop_back();

    // suppress if patent or pdb
    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CSeq_id> id2(new CSeq_id());
    id2->SetPatent().SetSeqid(1);
    id2->SetPatent().SetCit().SetCountry("USA");
    id2->SetPatent().SetCit().SetId().SetNumber("1");
    entry->SetSet().SetSeq_set().front()->SetSeq().SetId().push_back(id2);
    seh = scope.AddTopLevelSeqEntry(*entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    scope.RemoveTopLevelSeqEntry(seh);
    CRef<CPDB_seq_id> pdb_id(new CPDB_seq_id());
    pdb_id->SetMol().Set("foo");
    id2->SetPdb(*pdb_id);
    seh = scope.AddTopLevelSeqEntry(*entry);
    SetErrorsAccessions(expected_errors, "foo- ");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // add one source
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSet().SetSeq_set().front()->SetSeq().SetId().pop_back();
    AddGoodSource (entry->SetSet().SetSeq_set().front());
    seh = scope.AddTopLevelSeqEntry(*entry);
    SetErrorsAccessions(expected_errors, "nuc");
    expected_errors.push_back(new CExpectedError("prot", eDiag_Error, "NoOrgFound",
                              "No organism name has been applied to this Bioseq.  Other qualifiers may exist."));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

    // if there is a source descriptor but no tax name, still produce error
    AddGoodSource(entry->SetSet().SetSeq_set().back());
    SetTaxname(entry->SetSet().SetSeq_set().back(), "");
    expected_errors.push_back(new CExpectedError("prot", eDiag_Error, "NoOrgFound",
                              "No organism name has been applied to this Bioseq.  Other qualifiers may exist."));
    expected_errors.push_back(new CExpectedError("nuc", eDiag_Warning, "BioSourceOnProtein",
                                                 "Nuc-prot set has 1 protein with a BioSource descriptor"));
    expected_errors.push_back(new CExpectedError("nuc", eDiag_Warning, "BioSourceMissing",
                              "Nuc-prot set does not contain expected BioSource descriptor"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_Descr_MultipleBioSources)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodSeq();
    AddGoodSource (entry);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "MultipleBioSources",
                              "Undesired multiple source descriptors"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_Descr_NoMolInfoFound)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodSeq();
    RemoveDescriptorType (entry, CSeqdesc::e_Molinfo);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "NoMolInfoFound",
                              "No Mol-info applies to this Bioseq"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);
    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_Descr_NoTaxonID)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodSeq();
    SetTaxon(entry, 0);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;

    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "NoTaxonID",
                              "BioSource is missing taxon ID"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_Descr_InconsistentBiosources)
{
    // prepare entry
    CRef<CSeq_entry> entry(new CSeq_entry());
    entry->SetSet().SetClass(CBioseq_set::eClass_pop_set);
    CRef<CSeq_entry> first = BuildGoodSeq();
    entry->SetSet().SetSeq_set().push_back(first);
    CRef<CSeq_entry> second = BuildGoodSeq();
    second->SetSeq().SetId().front()->SetLocal().SetStr("good2");
    SetTaxname(second, "");
    SetTaxon(second, 0);
    SetTaxname(second, "Trichechus manatus latirostris");
    SetTaxon(second, 127582);
    entry->SetSet().SetSeq_set().push_back(second);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("good2", eDiag_Error, "InconsistentBioSources",
                              "Population set contains inconsistent organisms."));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // warning instead of error if same up to ' sp. '
    SetTaxname(first, "");
    SetTaxon(first, 0);
    SetTaxname(first, "Corynebacterium sp. 979");
    SetTaxon(first, 215582);
    SetTaxname(second, "");
    SetTaxon(second, 0);
    SetTaxname(second, "Corynebacterium sp. DJ1");
    SetTaxon(second, 632939);
    expected_errors[0]->SetSeverity(eDiag_Warning);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // warning instead of error if one name is subset of the other
    SetTaxname(first, "");
    SetTaxon(first, 0);
    SetTaxname(first, "Trichechus manatus");
    SetTaxon(first, 9778);
    SetTaxname(second, "");
    SetTaxon(second, 0);
    SetTaxname(second, "Trichechus manatus latirostris");
    SetTaxon(second, 127582);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

    // no error if not pop-set
    SetTaxname(first, "");
    SetTaxon(first, 0);
    SetTaxname(first, "Corynebacterium sp. 979");
    SetTaxon(first, 215582);
    SetTaxname(second, "");
    SetTaxon(second, 0);
    SetTaxname(second, "Trichechus manatus latirostris");
    SetTaxon(second, 127582);
    entry->SetSet().SetClass(CBioseq_set::eClass_genbank);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_Descr_MissingLineage)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodSeq();
    ResetOrgname(entry);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "MissingLineage",
                              "No lineage for this BioSource."));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetLineage (entry, "");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // warning if EMBL
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetEmbl().SetAccession("A12345");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetSeverity(eDiag_Warning);
    expected_errors[0]->SetAccession("A12345");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // warning if DDBJ
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetDdbj().SetAccession("A12345");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetSeverity(eDiag_Warning);
    expected_errors[0]->SetAccession("A12345");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);


    // critical instead of error if refseq AND has taxon
    scope.RemoveTopLevelSeqEntry(seh);
    entry->SetSeq().SetId().front()->SetOther().SetAccession("NC_123456");
    seh = scope.AddTopLevelSeqEntry(*entry);
    expected_errors[0]->SetSeverity(eDiag_Critical);
    expected_errors[0]->SetAccession("NC_123456");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // back to error if no taxon but refseq
    SetTaxon (entry, 0);
    expected_errors[0]->SetSeverity(eDiag_Error);
    expected_errors.push_back(new CExpectedError("NC_123456", eDiag_Warning, "NoTaxonID",
                              "BioSource is missing taxon ID"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

}


BOOST_AUTO_TEST_CASE(Test_Descr_SerialInComment)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodSeq();
    CRef<CSeqdesc> comment(new CSeqdesc());
    comment->SetComment("blah blah [123456]");
    entry->SetSeq().SetDescr().Set().push_back(comment);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("good", eDiag_Info, "SerialInComment",
                              "Comment may refer to reference by serial number - attach reference specific comments to the reference REMARK instead."));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

}


BOOST_AUTO_TEST_CASE(Test_Descr_BioSourceNeedsFocus)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodSeq();
    CRef<CSeq_feat> feat(new CSeq_feat());
    feat->SetData().SetBiosrc().SetOrg().SetTaxname("Trichechus manatus");
    SetTaxon (feat->SetData().SetBiosrc(), 127582);
    feat->SetData().SetBiosrc().SetOrg().SetOrgname().SetLineage("some lineage");
    feat->SetLocation().SetInt().SetId().SetLocal().SetStr("good");
    feat->SetLocation().SetInt().SetFrom(0);
    feat->SetLocation().SetInt().SetTo(5);
    CRef<CSeq_annot> annot(new CSeq_annot());
    annot->SetData().SetFtable().push_back(feat);
    entry->SetSeq().SetAnnot().push_back(annot);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "BioSourceNeedsFocus",
                              "BioSource descriptor must have focus or transgenic when BioSource feature with different taxname is present."));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }

    // error goes away if focus is set on descriptor
    SetFocus(entry);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    // error goes away if descriptor is transgenic
    ClearFocus(entry);
    SetTransgenic (entry, true);
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

}


BOOST_AUTO_TEST_CASE(Test_Descr_BadOrganelle)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodSeq();
    SetGenome (entry, CBioSource::eGenome_kinetoplast);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "BadOrganelle",
                              "Only Kinetoplastida have kinetoplasts"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetGenome (entry, CBioSource::eGenome_nucleomorph);
    expected_errors[0]->SetErrMsg("Only Chlorarachniophyceae and Cryptophyta have nucleomorphs");
    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "TaxonomyLookupProblem",
                                                 "Taxonomy lookup does not have expected nucleomorph flag"));
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_Descr_MultipleChromosomes)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodSeq();
    SetChromosome (entry, "1");

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "MultipleChromosomes",
                              "Multiple identical chromosome qualifiers"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    SetChromosome (entry, "2");
    expected_errors[0]->SetErrMsg("Multiple conflicting chromosome qualifiers");
    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_Descr_BadSubSource)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodSeq();
    SetSubSource (entry, 0, "foo");

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "BadSubSource",
                              "Unknown subsource subtype 0"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_Descr_BadOrgMod)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodSeq();
    SetOrgMod (entry, 0, "foo");
    SetOrgMod (entry, 1, "bar");
    SetOrgMod (entry, COrgMod::eSubtype_strain, "a");
    SetOrgMod (entry, COrgMod::eSubtype_strain, "b");
    SetOrgMod (entry, COrgMod::eSubtype_variety, "c");
    SetOrgMod (entry, COrgMod::eSubtype_nat_host, "Sebaea microphylla");
    SetCommon (entry, "some common name");
    SetOrgMod (entry, COrgMod::eSubtype_common, "some common name");

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "BadOrgMod",
                              "Unknown orgmod subtype 0"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Critical, "BadOrgMod",
                              "Unknown orgmod subtype 1"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "BadOrgMod",
                              "Multiple strain qualifiers on the same BioSource"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "BadOrgMod",
                              "Orgmod variety should only be in plants, fungi, or cyanobacteria"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "BioSourceInconsistency",
                              "Variety value specified is not found in taxname"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "BadOrgMod",
                              "Specific host is identical to taxname"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Warning, "BadOrgMod",
                              "OrgMod common is identical to Org-ref common"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}


BOOST_AUTO_TEST_CASE(Test_Descr_InconsistentProteinTitle)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodNucProtSet();
    CRef<CSeqdesc> desc(new CSeqdesc());
    desc->SetTitle("Not the correct title");
    entry->SetSet().SetSeq_set().back()->SetSeq().SetDescr().Set().push_back(desc);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("prot", eDiag_Warning, "InconsistentProteinTitle",
                              "Instantiated protein title does not match automatically generated title"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}

#if 0
BOOST_AUTO_TEST_CASE(Test_Descr_Inconsistent)
{
    // prepare entry
    CRef<CSeq_entry> entry = BuildGoodSeq();
    CRef<CSeqdesc> desc1(new CSeqdesc());
    desc1->SetMol_type(eGIBB_mol_genomic);
    entry->SetSeq().SetDescr().Set().push_back(desc1);
    CRef<CSeqdesc> desc2(new CSeqdesc());
    desc2->SetMol_type(eGIBB_mol_pre_mRNA);
    entry->SetSeq().SetDescr().Set().push_back(desc2);
    CRef<CSeqdesc> desc3(new CSeqdesc());
    desc3->SetModif().push_back(eGIBB_mod_dna);
    desc3->SetModif().push_back(eGIBB_mod_rna);
    desc3->SetModif().push_back(eGIBB_mod_mitochondrial);
    desc3->SetModif().push_back(eGIBB_mod_cyanelle);
    desc3->SetModif().push_back(eGIBB_mod_complete);
    desc3->SetModif().push_back(eGIBB_mod_partial);
    desc3->SetModif().push_back(eGIBB_mod_no_left);
    desc3->SetModif().push_back(eGIBB_mod_no_right);
    entry->SetSeq().SetDescr().Set().push_back(desc3);

    CRef<CSeqdesc> desc_gb1(new CSeqdesc());
    desc_gb1->SetGenbank();
    entry->SetSeq().SetDescr().Set().push_back(desc_gb1);
    CRef<CSeqdesc> desc_gb2(new CSeqdesc());
    desc_gb2->SetGenbank();
    entry->SetSeq().SetDescr().Set().push_back(desc_gb2);

    CRef<CSeqdesc> desc_embl1(new CSeqdesc());
    desc_embl1->SetEmbl();
    entry->SetSeq().SetDescr().Set().push_back(desc_embl1);
    CRef<CSeqdesc> desc_embl2(new CSeqdesc());
    desc_embl2->SetEmbl();
    entry->SetSeq().SetDescr().Set().push_back(desc_embl2);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddDefaults();
    CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

    CConstRef<CValidError> eval;

    CValidator validator(*objmgr);

    // Set validator options
    unsigned int options = CValidator::eVal_need_isojta
                          | CValidator::eVal_far_fetch_mrna_products
	                      | CValidator::eVal_validate_id_set | CValidator::eVal_indexer_version
	                      | CValidator::eVal_use_entrez;

    // list of expected errors
    vector< CExpectedError *> expected_errors;
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "Inconsistent",
                              "Inconsistent GIBB-mol [1] and [2]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "Inconsistent",
                              "Inconsistent GIBB-mod [0] and [1]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "Inconsistent",
                              "Inconsistent GIBB-mod [4] and [7]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "Inconsistent",
                              "Inconsistent GIBB-mod [11] and [16]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "Inconsistent",
                              "Inconsistent GIBB-mod [11] and [17]"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "Inconsistent",
                              "Multiple GenBank blocks"));
    expected_errors.push_back(new CExpectedError("good", eDiag_Error, "Inconsistent",
                              "Multiple EMBL blocks"));

    eval = validator.Validate(seh, options);
    CheckErrors (*eval, expected_errors);

    while (expected_errors.size() > 0) {
        if (expected_errors[expected_errors.size() - 1] != NULL) {
            delete expected_errors[expected_errors.size() - 1];
        }
        expected_errors.pop_back();
    }
}
#endif

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


