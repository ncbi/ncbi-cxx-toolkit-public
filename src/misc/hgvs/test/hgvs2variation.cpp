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
 * Author:
 *
 * File Description:
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

#include <objects/genomecoll/GC_Assembly.hpp>

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

    arg_desc->AddOptionalKey
            ("gc_for_chr_names",
             "assembly",
             "GC-assembly for interpreting chromosome names, e.g. chr12.",
             CArgDescriptions::eInputFile,
             CArgDescriptions::fPreOpen);

    arg_desc->AddDefaultKey(
            "vu_options",
            "bitmask",
            "CVariationUtil options",
            CArgDescriptions::eInteger,
            "0");


    arg_desc->AddOptionalKey(
            "target_id",
            "seqid",
            "genomic seq-id to remap to",
            CArgDescriptions::eString);
    
    arg_desc->AddFlag("reference_seq", "attach reference seq");
    arg_desc->AddFlag("loc_prop", "attach location properties");
    arg_desc->AddFlag("prot_effect", "attach effect on the protein");
    arg_desc->AddFlag("precursor", "calculate precursor variation");
    arg_desc->AddFlag("tr_precursor", "calculate precursor variation");
    arg_desc->AddFlag("attach_hgvs", "calculate hgvs expressions");
    arg_desc->AddFlag("roundtrip_hgvs", "roundtrip hgvs expression");
    arg_desc->AddFlag("asn_in", "in is text-asn");
    arg_desc->AddFlag("feat", "asn is seq-feat");
    arg_desc->AddFlag("o_hgvs", "Output HGVS expression instead of Variation");
    arg_desc->AddFlag("o_annot", "Output as seq-annot instead of Variation");


    // Program description
    string prog_description = "Convert hgvs expression to a variation\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);


    SetupArgDescriptions(arg_desc.release());
    CException::SetStackTraceLevel(eDiag_Warning);
    SetDiagPostLevel(eDiag_Info);
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


void ProcessVariation(CVariation& v, const CArgs& args, CScope& scope, CConstRef<CSeq_align> aln, CVariationUtil& variation_util)
{
    CHgvsParser parser(scope);

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

    if(args["reference_seq"]) {
        variation_util.AttachSeq(v);
    }

    if(args["prot_effect"]) {
        //calculate consequence based on cdna placements only, if available; otherwise on any
        bool have_cdna = false;
        for(CTypeConstIterator<CVariantPlacement> it(Begin(v)); it; ++it) {
            const CVariantPlacement& p = *it;
            if(p.GetLoc().GetId()
                && (    p.GetMol() == CVariantPlacement::eMol_mitochondrion
                     || p.GetMol() == CVariantPlacement::eMol_cdna))
            {
                have_cdna = true;
                break;
            }
        }
        variation_util.AttachProteinConsequences(v, NULL, have_cdna);
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
    if(args["attach_hgvs"] || args["o_hgvs"]) {
        parser.AttachHgvs(v);
    }

    if(args["target_id"]) {
        CSeq_id_Handle target_idh = CSeq_id_Handle::GetHandle(args["target_id"].AsString());
        CRef<CVariantPlacement> placement = variation_util.RemapToAnnotatedTarget(v, *target_idh.GetSeqId());
        placement->SetComment("Remapped via annotation to " + target_idh.AsString());
        v.SetPlacements().push_back(placement);
    }
}



int CHgvs2variationApplication::Run(void)
{
    // Setup application registry, error log, and MT-lock for CONNECT library
    CONNECT_Init(&GetConfig());

    // Process command line args:  get GI to load
    const CArgs& args = GetArgs();

    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();

    CGBDataLoader::RegisterInObjectManager(*object_manager, NULL, CObjectManager::eDefault, 2);

    CRef<CScope> scope(new CScope(*object_manager));
    scope->AddDefaults();

    CRef<CHgvsParser> parser(new CHgvsParser(*scope));
    if(args["gc_for_chr_names"]) {
        CRef<CGC_Assembly> gc_asm(new CGC_Assembly);
        args["gc_for_chr_names"].AsInputFile() >> MSerial_AsnText >> *gc_asm;
        CRef<CSeq_id_Resolver> resolver(new CSeq_id_Resolver__ChrNamesFromGC(*gc_asm, *scope));
        parser->SetSeq_id_Resolvers().push_front(resolver);
    }

    CRef<CVariationUtil> variation_util(new CVariationUtil(*scope, args["vu_options"].AsInteger()));

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
                if(args["feat"]) {
                    CRef<CSeq_feat> feat(new CSeq_feat);
                    args["in"].AsInputFile() >> MSerial_AsnText >> *feat;
                    CVariationUtil vu(*scope);
                    v = vu.AsVariation(*feat);
                } else {
                    args["in"].AsInputFile() >> MSerial_AsnText>> *v;
                }                
            } catch(CEofException&) {
                break;
            }
            ProcessVariation(*v, args, *scope, aln, *variation_util);

            if(args["o_annot"]) {
                    CRef<CSeq_annot> annot(new CSeq_annot);
                    variation_util->AsVariation_feats(*v, annot->SetData().SetFtable());
                    ostr << MSerial_AsnText << *annot;
            } else {
                ostr << MSerial_AsnText << *v;
            }
        }
    } else {

        string line_str("");
        while(NcbiGetlineEOL(istr, line_str)) {

            //expected line format: input_hgvs, optionally followed by "~" and
            //expected output expression, optionally followed by "#" and comment
            if(NStr::StartsWith(line_str, "#") || line_str.empty()) {
                if(args["roundtrip_hgvs"]) {
                    ostr << line_str << NcbiEndl;
                }
                continue;
            }

#if 0
            CBioseq_Handle bsh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle(line_str));
            NcbiCout << objects::sequence::GetAccessionForId(*bsh.GetSeqId(), *scope) << "\t" << bsh.GetInst_Length() << endl;
            continue;
#endif


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
            NStr::SplitInTwo(test_expr, "~", input_hgvs, other_synonyms);

            CRef<CVariation> v;
            try {
                v = parser->AsVariation(input_hgvs);
            } catch (CException& e) {
                NCBI_REPORT_EXCEPTION("Can't parse " + input_hgvs, e);
                //can't parse
                v.Reset(new CVariation);
                v->SetData().SetUnknown();
                v->SetName(input_hgvs);
            }
            v->SetDescription(comment);

            try {
                ProcessVariation(*v, args, *scope, aln, *variation_util);
                if(args["o_hgvs"]) {
                    for(CTypeConstIterator<CVariantPlacement> it(Begin(*v)); it; ++ it) {
                        if(it->IsSetHgvs_name()) {
                            ostr << v->GetName() << "\t" << it->GetHgvs_name() << "\t" << it->IsSetExceptions() << "\n";
                        } 
                    }
                } else if(args["o_annot"]) {
                    CRef<CSeq_annot> annot(new CSeq_annot);
                    variation_util->AsVariation_feats(*v, annot->SetData().SetFtable());
                    ostr << MSerial_AsnText << *annot;
                } else {
                    ostr << MSerial_AsnText << *v;
                }

            } catch(CException& e) {
                NCBI_RETHROW_SAME(e, "Can't process " + line_str);
            }
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
