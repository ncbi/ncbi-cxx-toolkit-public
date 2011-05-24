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
 * Author:  Aleksey Grichenko
 *
 * File Description:
 *   Demo of using the C++ Object Manager (OM)
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <connect/ncbi_core_cxx.hpp>

// Objects includes
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seq/Seq_inst.hpp>

// Object Manager includes
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <objtools/hgvs/hgvs_parser2.hpp>
#include <objtools/hgvs/variation_util2.hpp>
#include <objects/variation/Variation.hpp>
#include <objects/seqfeat/VariantProperties.hpp>

#include <serial/iterator.hpp>
#include <common/test_assert.h>

using namespace ncbi;
using namespace objects;


/////////////////////////////////////////////////////////////////////////////
//
//  Demo application
//


class CHgvs2variationApplication : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);
};


void CHgvs2variationApplication::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

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

    arg_desc->AddFlag("loc_prop", "attach location properties");
    arg_desc->AddFlag("prot_effect", "attach effect on the protein");
    arg_desc->AddFlag("precursor", "calculate precursor variation");
    arg_desc->AddFlag("compute_hgvs", "calculate hgvs expressions");

    // Program description
    string prog_description = "Convert hgvs expression to a variation\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);


    SetupArgDescriptions(arg_desc.release());
    CException::SetStackTraceLevel(eDiag_Warning);
}

using namespace variation;

int CHgvs2variationApplication::Run(void)
{
    // Setup application registry, error log, and MT-lock for CONNECT library
    CONNECT_Init(&GetConfig());

    // Process command line args:  get GI to load
    const CArgs& args = GetArgs();

    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*object_manager);
    CRef<CScope> scope(new CScope(*object_manager));
    scope->AddDefaults();

    CRef<CHgvsParser> parser(new CHgvsParser(*scope));
    CRef<CVariationUtil> variation_util(new CVariationUtil(*scope));

    CNcbiIstream& istr = args["in"].AsInputFile();
    CNcbiOstream& ostr = args["out"].AsOutputFile();


    string line_str("");
    while(NcbiGetlineEOL(istr, line_str)) {
         if(NStr::StartsWith(line_str, "#") || line_str.empty()) {
             continue;
         }
         string hgvs_expr, comment;
         NStr::SplitInTwo(line_str, "#", hgvs_expr, comment);

         NStr::TruncateSpacesInPlace(hgvs_expr);
         NStr::TruncateSpacesInPlace(comment);

         CRef<CVariation> v = parser->AsVariation(hgvs_expr);
         v->SetDescription(comment);

         for(CTypeIterator<CVariantPlacement> it(Begin(*v)); it; ++it) {
             variation_util->AttachSeq(*it);
             if(args["compute_hgvs"]) {
                 try {
                     it->SetComment(parser->AsHgvsExpression(*it));
                 } catch (CException& e) {
                     NCBI_REPORT_EXCEPTION("Can't compute hgvs expression", e);
                 }
             }
         }

         if(args["compute_hgvs"]) {
             string computed_hgvs = parser->AsHgvsExpression(*v);

             v->SetDescription() += "| Computed HGVS: " + computed_hgvs;
         }

         if(args["loc_prop"]) {
             variation_util->SetVariantProperties(*v);
         }

         if(args["prot_effect"]) {
             variation_util->AttachProteinConsequences(*v);
         }

         if(args["precursor"]) {
             CRef<CVariation> v2 = variation_util->InferNAfromAA(*v);
             CVariation::TConsequence::value_type consequence(new CVariation::TConsequence::value_type::TObjectType);
             consequence->SetVariation(*v);
             v2->SetConsequence().push_back(consequence);
             v = v2;
         }

         ostr << MSerial_AsnText << *v;
    }

    return 0;
}



/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    return CHgvs2variationApplication().AppMain(argc, argv);
}
