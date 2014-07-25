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
* Author:  Colleen Bollin, NCBI
*
* File Description:
*   Unit tests for biosample_chk.
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

#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/general/Dbtag.hpp>
#include <common/ncbi_export.h>
#include <objtools/unit_test_util/unit_test_util.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


void CheckDiffs(const TFieldDiffList& expected, const TFieldDiffList& observed)
{
    BOOST_CHECK_EQUAL(expected.size(), observed.size());
    TFieldDiffList::const_iterator ex = expected.begin();
    TFieldDiffList::const_iterator ob = observed.begin();
    while (ex != expected.end() && ob != observed.end()) {
        string ex_str = (*ex)->GetFieldName() + ":" 
                        + (*ex)->GetSrcVal() + ":"
                        + (*ex)->GetSampleVal();
        string ob_str = (*ob)->GetFieldName() + ":" 
                        + (*ob)->GetSrcVal() + ":"
                        + (*ob)->GetSampleVal();
        BOOST_CHECK_EQUAL(ex_str, ob_str);
        ex++;
        ob++;
    }
    while (ex != expected.end()) {
        string ex_str = (*ex)->GetFieldName() + ":" 
                        + (*ex)->GetSrcVal() + ":"
                        + (*ex)->GetSampleVal();
        BOOST_CHECK_EQUAL(ex_str, "");
        ex++;
    }
    while (ob != observed.end()) {
        string ob_str = (*ob)->GetFieldName() + ":" 
                        + (*ob)->GetSrcVal() + ":"
                        + (*ob)->GetSampleVal();
        BOOST_CHECK_EQUAL("", ob_str);
        ob++;
    }
}


BOOST_AUTO_TEST_CASE(Test_GetBiosampleDiffs)
{
    CRef<CBioSource> test_src(new CBioSource());
    CRef<CBioSource> test_sample(new CBioSource());

    test_src->SetOrg().SetTaxname("A");
    test_sample->SetOrg().SetTaxname("B");

    TFieldDiffList expected;
    expected.push_back(CRef<CFieldDiff>(new CFieldDiff("Organism Name", "A", "B")));
    TFieldDiffList diff_list = test_src->GetBiosampleDiffs(*test_sample);
    CheckDiffs(expected, diff_list);

    // ignore differences that can be autofixed
    unit_test_util::SetSubSource(*test_src, CSubSource::eSubtype_sex, "male");
    unit_test_util::SetSubSource(*test_sample, CSubSource::eSubtype_sex, "m");
    diff_list = test_src->GetBiosampleDiffs(*test_sample);
    CheckDiffs(expected, diff_list);

    // ignore differences that are allowed if BioSample is missing a value
    unit_test_util::SetSubSource(*test_src, CSubSource::eSubtype_chromosome, "1");
    diff_list = test_src->GetBiosampleDiffs(*test_sample);
    CheckDiffs(expected, diff_list);

    // ignore certain diffs in Org-ref
    unit_test_util::SetOrgMod(*test_sample, COrgMod::eSubtype_acronym, "acronym");
    diff_list = test_src->GetBiosampleDiffs(*test_sample);
    CheckDiffs(expected, diff_list);

    // ignore some case differences
    unit_test_util::SetSubSource(*test_src, CSubSource::eSubtype_cell_type, "abc");
    unit_test_util::SetSubSource(*test_sample, CSubSource::eSubtype_cell_type, "ABC");
    diff_list = test_src->GetBiosampleDiffs(*test_sample);
    CheckDiffs(expected, diff_list);

    try {
        test_src->UpdateWithBioSample(*test_sample, false);
        BOOST_CHECK_EQUAL("Expected exception to be thrown", "Exception not thrown!");
    } catch (CException& e) {
        BOOST_CHECK_EQUAL(e.GetMsg(), "Conflicts found");
    }

    try {
        test_src->UpdateWithBioSample(*test_sample, true);
        BOOST_CHECK_EQUAL(test_src->GetOrg().GetTaxname(), "B");
        vector<string> vals;
        vals.push_back("male");
        vals.push_back("1");
        vals.push_back("abc");
        vector<string>::iterator sit = vals.begin();
        ITERATE(CBioSource::TSubtype, it, test_src->GetSubtype()) {
            if (sit == vals.end()) {
                BOOST_CHECK_EQUAL("Unexpected SubSource Value", (*it)->GetName());
            } else {
                BOOST_CHECK_EQUAL((*it)->GetName(), *sit);
                sit++;
            }
        }
        BOOST_CHECK_EQUAL(test_src->GetOrg().IsSetOrgMod(), false);
    } catch (CException& e) {
        BOOST_CHECK_EQUAL("Unexpected exception", e.GetMsg());
    }

    // ignore name elements if in taxname
    unit_test_util::SetOrgMod(*test_src, COrgMod::eSubtype_biovar, "XYZ");
    test_src->SetOrg().SetTaxname("B XYZ");
    test_sample->SetOrg().SetTaxname("B XYZ");

    expected.clear();
    diff_list = test_src->GetBiosampleDiffs(*test_sample);
    CheckDiffs(expected, diff_list);


}


BOOST_AUTO_TEST_CASE(Test_UpdateWithBioSample)
{
    CRef<CBioSource> src(new CBioSource());
    src->SetGenome(CBioSource::eGenome_genomic);
    src->SetOrg().SetTaxname("Campylobacter jejuni Cj3");
    unit_test_util::SetTaxon(*src, 1365660);
    unit_test_util::SetOrgMod(*src, COrgMod::eSubtype_strain, "Cj3");
    src->SetOrg().SetOrgname().SetLineage("Bacteria; Proteobacteria; Epsilonproteobacteria;  Campylobacterales; Campylobacteraceae; Campylobacter");
    src->SetOrg().SetOrgname().SetGcode(11);
    src->SetOrg().SetOrgname().SetDiv("BCT");

    CRef<CBioSource> biosample(new CBioSource());
    biosample->SetOrg().SetTaxname("Campylobacter jejuni Cj3");
    unit_test_util::SetTaxon(*biosample, 1365660);
    unit_test_util::SetOrgMod(*biosample, COrgMod::eSubtype_nat_host, "Homo sapiens");
    unit_test_util::SetOrgMod(*biosample, COrgMod::eSubtype_strain, "Cj3");
    biosample->SetOrg().SetOrgname().SetLineage("Bacteria; Proteobacteria; Epsilonproteobacteria;  Campylobacterales; Campylobacteraceae; Campylobacter");
    biosample->SetOrg().SetOrgname().SetGcode(11);
    biosample->SetOrg().SetOrgname().SetDiv("BCT");
    unit_test_util::SetSubSource(*biosample, CSubSource::eSubtype_country, "Thailand");
    unit_test_util::SetSubSource(*biosample, CSubSource::eSubtype_isolation_source, "stool");
 
    src->UpdateWithBioSample(*biosample, true);

    TFieldDiffList diff_list = src->GetBiosampleDiffs(*biosample);
    TFieldDiffList expected;
    CheckDiffs(expected, diff_list);

}


const char *sc_TestSQD1833_Src = "\
BioSource ::= { \
  genome genomic, \
  org { \
    taxname \"Escherichia coli\", \
    db { \
      { \
        db \"taxon\", \
        tag id 562 \
      } \
    }, \
    orgname { \
      name binomial { \
        genus \"Escherichia\", \
        species \"coli\" \
      }, \
      lineage \"Bacteria; Proteobacteria; Gammaproteobacteria; \
 Enterobacteriales; Enterobacteriaceae; Escherichia\", \
      gcode 11, \
      div \"BCT\" \
    } \
  } \
} \
";


const char *sc_TestSQD1833_Smpl = "\
BioSource ::= { \
  org { \
    taxname \"Escherichia coli\", \
    db { \
      { \
        db \"taxon\", \
        tag id 562 \
      } \
    }, \
    syn { \
      \"bacterium E3\", \
      \"Enterococcus coli\", \
      \"Bacterium coli commune\", \
      \"Bacterium coli\", \
      \"Bacillus coli\" \
    }, \
    orgname { \
      name binomial { \
        genus \"Escherichia\", \
        species \"coli\" \
      }, \
      mod { \
        { \
          subtype nat-host, \
          subname \"Homo sapiens\" \
        }, \
        { \
          subtype strain, \
          subname \"CS01\" \
        } \
      }, \
      lineage \"Bacteria; Proteobacteria; Gammaproteobacteria; \
 Enterobacteriales; Enterobacteriaceae; Escherichia\", \
      gcode 11, \
      div \"BCT\" \
    } \
  }, \
  subtype { \
    { \
      subtype collection-date, \
      name \"24-Jun-2013\" \
    }, \
    { \
      subtype country, \
      name \"USA: Santa Clara\" \
    }, \
    { \
      subtype isolation-source, \
      name \"Human fecal sample\" \
    } \
  } \
} \
";


BOOST_AUTO_TEST_CASE(Test_SQD_1833)
{
    CBioSource src;
    {{
         CNcbiIstrstream istr(sc_TestSQD1833_Src);
         istr >> MSerial_AsnText >> src;
     }}

    CBioSource smpl;
    {{
         CNcbiIstrstream istr(sc_TestSQD1833_Smpl);
         istr >> MSerial_AsnText >> smpl;
     }}


    try {
        src.UpdateWithBioSample(smpl, false);
        vector<string> vals;
        vals.push_back("24-Jun-2013");
        vals.push_back("USA: Santa Clara");
        vals.push_back("Human fecal sample");
        vector<string>::iterator sit = vals.begin();
        ITERATE(CBioSource::TSubtype, it, src.GetSubtype()) {
            if (sit == vals.end()) {
                BOOST_CHECK_EQUAL("Unexpected SubSource Value", (*it)->GetName());
            } else {
                BOOST_CHECK_EQUAL((*it)->GetName(), *sit);
                sit++;
            }
        }
    } catch (CException& e) {
        BOOST_CHECK_EQUAL("Unexpected exception", e.GetMsg());
    }

    
}


BOOST_AUTO_TEST_CASE(Test_GP_9113)
{
    CBioSource src;
    src.SetOrg().SetTaxname("a");
    CBioSource smpl;
    smpl.SetOrg().SetTaxname("a");
    smpl.SetOrg().SetTaxId(123);

    try {
        src.UpdateWithBioSample(smpl, true);
        BOOST_CHECK_EQUAL(src.GetOrg().GetDb().front()->GetTag().GetId(), 123);
    } catch (CException& e) {
        BOOST_CHECK_EQUAL("Unexpected exception", e.GetMsg());
    }

    
}


BOOST_AUTO_TEST_CASE(Test_FuzzyStrainMatch)
{
    BOOST_CHECK_EQUAL(COrgMod::FuzzyStrainMatch("abc", "ABC"), true);
    BOOST_CHECK_EQUAL(COrgMod::FuzzyStrainMatch("ab c", "ABC"), true);
    BOOST_CHECK_EQUAL(COrgMod::FuzzyStrainMatch("a/b c", "ABC"), true);
    BOOST_CHECK_EQUAL(COrgMod::FuzzyStrainMatch("a/b c", "AB:C"), true);
    BOOST_CHECK_EQUAL(COrgMod::FuzzyStrainMatch("a/b d", "AB:C"), false);
}


BOOST_AUTO_TEST_CASE(Test_SQD_1836)
{
    CRef<CBioSource> src(new CBioSource());
    src->SetOrg().SetTaxname("Porphyromonas sp. KLE 1280");
    CRef<CDbtag> taxid(new CDbtag());
    taxid->SetDb("taxon");
    taxid->SetTag().SetId(997829);
    src->SetOrg().SetDb().push_back(taxid);
    CRef<COrgMod> m1(new COrgMod());
    m1->SetSubtype(COrgMod::eSubtype_strain);
    m1->SetSubname("KLE 1280");
    src->SetOrg().SetOrgname().SetMod().push_back(m1);

    CRef<CBioSource> smpl(new CBioSource());
    smpl->Assign(*src);

    CRef<CDbtag> hmp(new CDbtag());
    hmp->SetDb("HMP");
    hmp->SetTag().SetId(1121);
    src->SetOrg().SetDb().push_back(hmp);

    try {
        src->UpdateWithBioSample(*smpl, false);
        BOOST_CHECK_EQUAL(src->GetOrg().GetDb().size(), 2);
    } catch (CException& e) {
        BOOST_CHECK_EQUAL("Unexpected exception", e.GetMsg());
    }

    
}

END_SCOPE(objects)
END_NCBI_SCOPE
