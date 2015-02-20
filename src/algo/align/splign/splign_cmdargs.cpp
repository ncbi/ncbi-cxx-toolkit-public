/* $Id$
* ===========================================================================
*
*                            public DOMAIN NOTICE                          
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
* Author:  Yuri Kapustin
*          Anatoliy Kuznetsov
*          Boris Kiryutin
*
* File Description:
*   Splign command line argument utilities
*/


#include <ncbi_pch.hpp>

#include <algo/align/splign/splign_cmdargs.hpp>
#include <algo/align/util/compartment_finder.hpp>
#include <algo/align/nw/nw_spliced_aligner16.hpp>

namespace {
    const size_t kMb (1u << 20);
}

BEGIN_NCBI_SCOPE

void CSplignArgUtil::SetupArgDescriptions(CArgDescriptions* argdescr)
{        
    argdescr->SetCurrentGroup("Basic scores");

    argdescr->AddDefaultKey("type", "type",
                            "Query cDNA type: 'mrna' or 'est'."
                            " Sets basic scores to preset values",
                            CArgDescriptions::eString,
                            kQueryType_mRNA);

     argdescr->AddOptionalKey
        ("match_score",
         "match_score",
         "Score for a single match (positive)."
         " Overrides value set by '-type'",
         CArgDescriptions::eInteger);

     argdescr->AddOptionalKey
        ("mismatch_score",
         "mismatch_score",
         "Score for a single mismatch (negative)."
         " Overrides value set by '-type'",
         CArgDescriptions::eInteger);

     argdescr->AddOptionalKey
        ("gap_opening_score",
         "gap_opening_score",
         "Score for gap opening (negative)."
         " Overrides value set by '-type'",
         CArgDescriptions::eInteger);
     argdescr->AddOptionalKey
        ("gap_extension_score",
         "gap_extension_score",
         "Score for gap extension (negative)."
         " Overrides value set by '-type'",
         CArgDescriptions::eInteger);

     argdescr->AddOptionalKey
        ("gt_ag_splice_score",
         "gt_ag_splice_score",
         "Score for splice (negative)."
         " Overrides value set by '-type'",
         CArgDescriptions::eInteger);

     argdescr->AddOptionalKey
        ("gc_ag_splice_score",
         "gc_ag_splice_score",
         "Score for splice (negative)."
         " Overrides value set by '-type'",
         CArgDescriptions::eInteger);

     argdescr->AddOptionalKey
        ("at_ac_splice_score",
         "at_ac_splice_score",
         "Score for splice (negative)."
         " Overrides value set by '-type'",
         CArgDescriptions::eInteger);

     argdescr->AddOptionalKey
        ("non_consensus_splice_score",
         "non_consensus_splice_score",
         "Score for splice (negative)."
         " Overrides value set by '-type'",
         CArgDescriptions::eInteger);

    argdescr->SetCurrentGroup("");

   argdescr->AddDefaultKey
        ("compartment_penalty",
         "compartment_penalty",
         "Penalty to open a new compartment "
         "(compartment identification parameter). "
         "Multiple compartments will only be identified if "
         "they have at least this level of coverage.",
         CArgDescriptions::eDouble,
         NStr::DoubleToString(CSplign::s_GetDefaultCompartmentPenalty()));
    
    argdescr->AddDefaultKey
        ("min_compartment_idty",
         "min_compartment_identity",
         "Minimal compartment identity to align.",
         CArgDescriptions::eDouble,
         NStr::DoubleToString(CSplign::s_GetDefaultMinCompartmentIdty()));
    
    argdescr->AddOptionalKey
        ("min_singleton_idty",
         "min_singleton_identity",
         "Minimal singleton compartment identity to use per subject and strand, "
         "expressed as a fraction of the query's length.",
         CArgDescriptions::eDouble);
    
    argdescr->AddDefaultKey
        ("min_singleton_idty_bps",
         "min_singleton_identity_bps",
         "Minimal singleton compartment identity to use per subject and strand, "
         "in base pairs. "
         "The actual value passed to the compartmentization procedure is the least of "
         "(min_singleton_idty * query_length) and min_singleton_identity_bps.",
         CArgDescriptions::eInteger,
         "9999999");

    argdescr->AddDefaultKey
        ("min_exon_idty",
         "identity",
         "Minimal exon identity. "
         "Segments with lower identity will be marked as gaps.",
         CArgDescriptions::eDouble,
         NStr::DoubleToString(CSplign::s_GetDefaultMinExonIdty()));

    argdescr->AddDefaultKey
        ("min_polya_ext_idty",
         "identity",
         "Minimal identity to extend alignment into polya. "
         "Polya candidate region on mRNA is detected first. Alignment is produced without the polya candidate region "
         "After that alignment will be extended into the polya candidate region to deal with case when initial polya detection was wrong",
         CArgDescriptions::eDouble,
         NStr::DoubleToString(CSplign::s_GetDefaultPolyaExtIdty()));

    argdescr->AddDefaultKey
        ("min_polya_len",
         "min_polya_len",
         "Minimal length of polya.",
         CArgDescriptions::eInteger,
         NStr::NumericToString(CSplign::s_GetDefaultMinPolyaLen()));

    argdescr->AddDefaultKey
        ("max_intron",
         "max_intron",
         "The upper bound on intron length, in base pairs.",
         CArgDescriptions::eInteger,
         NStr::NumericToString(CCompartmentFinder<CSplign::THit>::
                           s_GetDefaultMaxIntron()));

    argdescr->AddDefaultKey
        ("max_space",
         "max_space",
         "The max space to allocate for a splice, in MB. "
         "Specify lower values to spend less time stitching "
         "over large genomic intervals.",
         CArgDescriptions::eDouble,
         NStr::DoubleToString(double(CNWAligner::GetDefaultSpaceLimit()) / kMb));

     argdescr->AddOptionalKey
         ("test",
          "test_mode",
          "Test new developments. Default behavior if not set.",
          CArgDescriptions::eString);

    CArgAllow * constrain01 (new CArgAllow_Doubles(0,1));
    argdescr->SetConstraint("min_compartment_idty", constrain01);
    argdescr->SetConstraint("min_exon_idty", constrain01);
    argdescr->SetConstraint("min_polya_ext_idty", constrain01);
    argdescr->SetConstraint("compartment_penalty", constrain01);

    CArgAllow * constrain_1_1M (new CArgAllow_Integers(1,1000000));
    argdescr->SetConstraint("min_polya_len", constrain_1_1M);

    CArgAllow * constrain_7_2M (new CArgAllow_Integers(7,2000000));
    argdescr->SetConstraint("max_intron", constrain_7_2M);

    CArgAllow * constrain_max_space (new CArgAllow_Doubles(500, 4096));
    argdescr->SetConstraint("max_space", constrain_max_space);

    CArgAllow_Strings * constrain_querytype (new CArgAllow_Strings);
    constrain_querytype ->Allow(kQueryType_mRNA) ->Allow(kQueryType_EST);
    argdescr->SetConstraint("type", constrain_querytype);

    CArgAllow_Strings * constrain_testtype (new CArgAllow_Strings);
    constrain_testtype ->Allow(kTestType_20_28)->Allow(kTestType_20_28_plus);
    argdescr->SetConstraint("test", constrain_testtype);
}


void CSplignArgUtil::ArgsToSplign(CSplign* splign, const CArgs& args)
{
    splign->SetEndGapDetection(true);
    splign->SetPolyaDetection(true);

    splign->SetMaxIntron(args["max_intron"].AsInteger());
    splign->SetCompartmentPenalty(args["compartment_penalty"].AsDouble());
    splign->SetMinCompartmentIdentity(args["min_compartment_idty"].AsDouble());
    if(args["min_singleton_idty"]) {
        splign->SetMinSingletonIdentity(args["min_singleton_idty"].AsDouble());
    }
    else {
        splign->SetMinSingletonIdentity(splign->GetMinCompartmentIdentity());
    }

    splign->SetMinSingletonIdentityBps(args["min_singleton_idty_bps"].AsInteger());
    splign->SetMinExonIdentity(args["min_exon_idty"].AsDouble());
    splign->SetPolyaExtIdentity(args["min_polya_ext_idty"].AsDouble());
    splign->SetMinPolyaLen(args["min_polya_len"].AsInteger());
    double max_space (args["max_space"].AsDouble() * kMb);
    const Uint4 kMax32 (numeric_limits<Uint4>::max());
    if(max_space > kMax32) max_space = kMax32;

    if(args["test"]) {
        splign->SetTestType(args["test"].AsString());
    } else {
        splign->SetTestType(kTestType_production_default);
    }

    //basic scores and aligner
    const bool query_low_quality (args["type"].AsString() == kQueryType_EST);
    if(query_low_quality) {
        splign->SetScoringType(CSplign::eEstScoring);
    } else {
        splign->SetScoringType(CSplign::eMrnaScoring);
    }
    if(args["match_score"]) {
        splign->SetMatchScore(args["match_score"].AsInteger());
    }
    if(args["mismatch_score"]) {
        splign->SetMismatchScore(args["mismatch_score"].AsInteger());
    }
    if(args["gap_opening_score"]) {
        splign->SetGapOpeningScore(args["gap_opening_score"].AsInteger());
    }
    if(args["gap_extension_score"]) {
        splign->SetGapExtensionScore(args["gap_extension_score"].AsInteger());
    }
    if(args["gt_ag_splice_score"]) {
        splign->SetGtAgSpliceScore(args["gt_ag_splice_score"].AsInteger());
    }
    if(args["gc_ag_splice_score"]) {
        splign->SetGcAgSpliceScore(args["gc_ag_splice_score"].AsInteger());
    }
    if(args["at_ac_splice_score"]) {
        splign->SetAtAcSpliceScore(args["at_ac_splice_score"].AsInteger());
    }
    if(args["non_consensus_splice_score"]) {
        splign->SetNonConsensusSpliceScore(args["non_consensus_splice_score"].AsInteger());
    }
    CRef<CSplicedAligner> aligner = CSplign::s_CreateDefaultAligner();
    aligner->SetSpaceLimit(size_t(max_space));
    splign->SetAligner() = aligner;
    splign->SetAlignerScores();
}


END_NCBI_SCOPE
