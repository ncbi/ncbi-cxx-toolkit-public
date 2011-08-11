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


#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
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

#include <misc/hgvs/hgvs_parser2.hpp>
#include <misc/hgvs/variation_util2.hpp>
#include <objects/variation/Variation.hpp>
#include <objects/seqfeat/VariantProperties.hpp>
#include <objmgr/util/sequence.hpp>

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

    arg_desc->AddOptionalKey
        ("aln",
         "alignment",
         "Text seq-align to remap placement to",
         CArgDescriptions::eInputFile,
         CArgDescriptions::fPreOpen);

    arg_desc->AddFlag("loc_prop", "attach location properties");
    arg_desc->AddFlag("prot_effect", "attach effect on the protein");
    arg_desc->AddFlag("precursor", "calculate precursor variation");
    arg_desc->AddFlag("tr_precursor", "calculate precursor variation");
    arg_desc->AddFlag("compute_hgvs", "calculate hgvs expressions");
    arg_desc->AddFlag("roundtrip_hgvs", "roundtrip hgvs expression");
    arg_desc->AddFlag("asn_in", "in is text-asn");

    // Program description
    string prog_description = "Convert hgvs expression to a variation\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);


    SetupArgDescriptions(arg_desc.release());
    CException::SetStackTraceLevel(eDiag_Warning);
}

using namespace variation;

string RoundTripHgvs(const string& hgvs, CHgvsParser& parser)
{
    CRef<CVariation> v = parser.AsVariation(hgvs);
    string v_out = parser.AsHgvsExpression(*v);
    return v_out;
}

//input: pipe-delimited tuple of hgvs synonymous HGVS expressions.
//output: same-size tuple, where i'th element contains "ok" if
//the i'th round-trip matches to one of the inputs, or the generated
//expression if it does not match.
//
//That is, the assertion is that every synonym, when converted to
//variation and back, will produce one of the input synonyms
//(but not necessarily itself)
string TestRoundTrip(const string& hgvs_synonyms, CHgvsParser& parser)
{
    typedef vector<string> TStrings;
    TStrings inputs;
    NStr::Tokenize(hgvs_synonyms, "|", inputs);
    string result;
    string delim = "";
    bool all_ok = true;
    ITERATE(TStrings, it, inputs) {
        const string& hgvs_in = *it;
        string hgvs_out = RoundTripHgvs(hgvs_in, parser);

        bool is_found = find(inputs.begin(), inputs.end(), hgvs_out) != inputs.end();
        all_ok = all_ok && is_found;
        result += delim + (is_found ? "OK" : hgvs_out);
        delim = "|";
    }

    return all_ok ? "OK" : result;
}

void AttachHgvs(CVariation& v, CHgvsParser& parser)
{
    v.Index();
    CVariationUtil util(parser.SetScope());

    //compute and attach placement-specific HGVS expressions
    for(CTypeIterator<CVariation> it(Begin(v)); it; ++it) {
        CVariation& v2 = *it;
        if(!v2.IsSetPlacements()) {
            continue;
        }
        NON_CONST_ITERATE(CVariation::TPlacements, it2, v2.SetPlacements()) {
            CVariantPlacement& p2 = **it2;
            util.AttachSeq(p2);

            if(!p2.GetLoc().GetId()) {
                continue;
            }

            if(p2.GetMol() != CVariantPlacement::eMol_protein && v2.GetConsequenceParent()) {
                //if this variation is in consequnece, only compute HGVS for protein variations
                //(as otherwise it will throw - can't have HGVS expression for protein with nuc placement)
                continue;
            }

            //compute hgvs-expression specific to the placement and the variation to which it is attached
            string hgvs_expression = parser.AsHgvsExpression(v2, CConstRef<CSeq_id>(p2.GetLoc().GetId()));
            p2.SetHgvs_name(hgvs_expression);
        }
    }

    //If the root variation does not have placements (e.g. a container for placement-specific subvariations)
    //then compute the hgvs expression for the root placement and attach it to variation itself as a synonym.
    if(!v.IsSetPlacements()) {
        string root_output_hgvs = parser.AsHgvsExpression(v);
        v.SetSynonyms().push_back(root_output_hgvs);
    }
}

void ProcessVariation(CVariation& v, const CArgs& args, CScope& scope, CConstRef<CSeq_align> aln)
{
    CHgvsParser parser(scope);
    CVariationUtil variation_util(scope);

    if(aln) {
        for(CTypeIterator<CVariation> it(Begin(v)); it; ++it) {
            CVariation& v2 = *it;
            if(!v2.IsSetPlacements()) {
                continue;
            }
            CVariation::TPlacements mapped;
            ITERATE(CVariation::TPlacements, it2, v2.GetPlacements()) {
                const CVariantPlacement& p = **it2;
                if(   ncbi::sequence::IsSameBioseq(aln->GetSeq_id(0), ncbi::sequence::GetId(p.GetLoc(), NULL), &scope)
                   || ncbi::sequence::IsSameBioseq(aln->GetSeq_id(1), ncbi::sequence::GetId(p.GetLoc(), NULL), &scope))
                {
                    mapped.push_back(variation_util.Remap(p, *aln));
                }
            }
            NON_CONST_ITERATE(CVariation::TPlacements, it2, mapped) {
                v2.SetPlacements().push_back(*it2);
            }
        }
    }



    if(args["loc_prop"]) {
        variation_util.SetVariantProperties(v);
    }

    if(args["prot_effect"]) {
#if 0
        CRef<CSeq_id> id(NULL);
#else
        set<CSeq_id_Handle> ids;
        for(CTypeConstIterator<CVariantPlacement> it(Begin(v)); it; ++it) {
            const CVariantPlacement& p = *it;
            if(p.GetLoc().GetId() && p.GetMol() == CVariantPlacement::eMol_cdna) {
                ids.insert(CSeq_id_Handle::GetHandle(*p.GetLoc().GetId()));
            }
        }
        ITERATE(set<CSeq_id_Handle>, it, ids) {
            variation_util.AttachProteinConsequences(v, it->GetSeqId());
        }
#endif
    }

    if(args["precursor"] || args["tr_precursor"]) {
        CRef<CVariation> v2 = variation_util.InferNAfromAA(
                v,
                args["tr_precursor"] ? CVariationUtil::fAA2NA_truncate_common_prefix_and_suffix : 0);
        CVariation::TConsequence::value_type consequence(new CVariation::TConsequence::value_type::TObjectType);
        consequence->SetVariation().Assign(v);
        v2->SetConsequence().push_back(consequence);
        v.Assign(*v2);

    }
    if(args["compute_hgvs"]) {
        AttachHgvs(v, parser);
    }

}



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


    CRef<CSeq_align> aln;
    if(args["aln"]) {
        aln.Reset(new CSeq_align);
        args["aln"].AsInputFile() >> MSerial_AsnText >> *aln;
    }


    if(args["asn_in"]) {
        while(true) {
            CRef<CVariation> v (new CVariation);
            try {
                args["in"].AsInputFile() >> MSerial_AsnText>> *v;
            } catch(CEofException& e) {
                break;
            }
            ProcessVariation(*v, args, *scope, aln);
            ostr << MSerial_AsnText << *v;
        }
    } else {

        string line_str("");
        while(NcbiGetlineEOL(istr, line_str)) {
            //expected line format: input_hgvs, optionally followed by "|" and
            //expected output expression, optionally followed by "#" and comment
            if(NStr::StartsWith(line_str, "#") || line_str.empty()) {
                if(args["roundtrip_hgvs"]) {
                    ostr << line_str << NcbiEndl;
                }
                continue;
            }

            string test_expr, comment;
            NStr::SplitInTwo(line_str, "#", test_expr, comment);
            NStr::TruncateSpacesInPlace(comment);
            NStr::TruncateSpacesInPlace(test_expr);

            if(args["roundtrip_hgvs"]) {
                string result = TestRoundTrip(test_expr, *parser);
                ostr << test_expr << "\t" << result << NcbiEndl;
                continue;
            }

            string input_hgvs, other_synonyms;
            NStr::SplitInTwo(test_expr, "|", input_hgvs, other_synonyms);

            CRef<CVariation> v = parser->AsVariation(input_hgvs);
            v->SetDescription(comment);

            ProcessVariation(*v, args, *scope, aln);

            ostr << MSerial_AsnText << *v;
        }

    }
    return 0;
}





/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    return CHgvs2variationApplication().AppMain(argc, argv);
}
