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
* Author:  Alex Astashyn
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/test_boost.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <misc/hgvs/hgvs_parser2.hpp>
#include <misc/hgvs/variation_util2.hpp>
#include <objects/variation/Variation.hpp>

#include <boost/test/parameterized_test.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

#include <serial/iterator.hpp>

USING_NCBI_SCOPE;

using namespace variation;

/**
 * Check check that the hgvs-name of an existing variation-feature, when parsed,
 * results in exact same variation-feature.
 */
class CTestCase
{
public:

    //todo: reuse variation-util
    CTestCase(const CVariation& v, CRef<CHgvsParser> parser, CNcbiOstream& out)
      : m_expr("")
      , m_variation(&v)
      , m_parser(parser)
      , m_out(out)
    {
        m_expr = m_variation->GetName();
    }

    void operator()()
    {
        CRef<CVariation> variation;
        string exception_str;
        string err_code;

        CVariationUtil variation_util(m_parser->SetScope());
        variation = m_parser->AsVariation(m_expr);

        for(CTypeIterator<CVariantPlacement> it(Begin(*variation)); it; ++it) {
            variation_util.AttachSeq(*it);
        }

        variation_util.SetVariantProperties(*variation);
        variation_util.AttachProteinConsequences(*variation);

        if(!variation.IsNull() && m_variation->IsSetDescription()) {
            variation->SetDescription(m_variation->GetDescription());
        }

        //if we are checking against reference variation-feats, compare the one
        //that we just created with the reference one
        if(!m_variation->Equals(*variation)) {
            NcbiCerr << "---\nExpected\n" << MSerial_AsnText << *m_variation;
            NcbiCerr << "Produced\n---\n" << MSerial_AsnText << *variation;
            NCBITEST_CHECK(false);
        }
    }

    string m_expr;
    CConstRef<CVariation> m_variation;
    CRef<CHgvsParser> m_parser;
    CNcbiOstream& m_out;
};


#include <limits>
NCBITEST_INIT_TREE()
{
    CException::SetStackTraceLevel(eDiag_Warning);
    SetDiagPostLevel(eDiag_Warning);

    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    CRef<CObjectManager> obj_mgr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*obj_mgr);
    CRef<CScope> scope(new CScope(*obj_mgr));
    scope->AddDefaults();
    CRef<CHgvsParser> parser(new CHgvsParser(*scope));
    CException::SetStackTraceLevel(eDiag_Trace);

    CNcbiIstream& istr = args["in"].AsInputFile();
    CNcbiOstream& ostr = args["out"].AsOutputFile();

    while(true) {
        CRef<CVariation> variation(new CVariation);
        try {
            istr >> MSerial_AsnText >> *variation;
        } catch (CEofException e) {
            break;
        }

        LOG_POST("Adding test-case");
        boost::unit_test::framework::master_test_suite().add(
                BOOST_TEST_CASE(CTestCase(*variation, parser, ostr)));

    }
}


NCBITEST_AUTO_INIT()
{

}

NCBITEST_AUTO_FINI()
{

}


NCBITEST_INIT_CMDLINE(arg_desc)
{
    arg_desc->AddDefaultKey
        ("in",
         "input",
         "Text file of hgvs expressions",
         CArgDescriptions::eInputFile,
         "-",
         CArgDescriptions::fPreOpen);

    arg_desc->AddDefaultKey
        ("out",
         "output",
         "Text file of hgvs expressions; used in conjunction with -in",
         CArgDescriptions::eOutputFile,
         "-",
         CArgDescriptions::fPreOpen);
}
