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
*
* File Description:
*   Splign command line argument utilities
*/


#include <ncbi_pch.hpp>

#include <algo/align/splign/splign_cmdargs.hpp>
#include <algo/align/nw/nw_spliced_aligner16.hpp>


BEGIN_NCBI_SCOPE


void CSplignArgUtil::SetupArgDescriptions(CArgDescriptions* argdescr)
{    
    
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
        ("max_extent",
         "max_extent",
         "Max genomic extent to look for exons beyond compartment ends.",
         CArgDescriptions::eInteger,
         NStr::IntToString(CSplign::s_GetDefaultMaxGenomicExtent()) );
    
    argdescr->AddDefaultKey
        ("min_exon_idty",
         "identity",
         "Minimal exon identity. "
         "Segments with lower identity will be marked as gaps.",
         CArgDescriptions::eDouble,
         NStr::DoubleToString(CSplign::s_GetDefaultMinExonIdty()));

    argdescr->AddDefaultKey
        ("Wm", "match", "match score",
         CArgDescriptions::eInteger,
         NStr::IntToString(CNWAligner::GetDefaultWm()).c_str());
    
    argdescr->AddDefaultKey
        ("Wms", "mismatch", "mismatch score",
         CArgDescriptions::eInteger,
         NStr::IntToString(CNWAligner::GetDefaultWms()).c_str());
    
    argdescr->AddDefaultKey
        ("Wg", "gap", "gap opening score",
         CArgDescriptions::eInteger,
         NStr::IntToString(CNWAligner::GetDefaultWg()).c_str());
    
    argdescr->AddDefaultKey
        ("Ws", "space", "gap extension (space) score",
         CArgDescriptions::eInteger,
         NStr::IntToString(CNWAligner::GetDefaultWs()).c_str());
    
    argdescr->AddDefaultKey
        ("Wi0", "Wi0", "Conventional intron (GT/AG) score",
         CArgDescriptions::eInteger,
         NStr::IntToString(CSplicedAligner16::GetDefaultWi(0)).c_str());
    
    argdescr->AddDefaultKey
        ("Wi1", "Wi1", "Conventional intron (GC/AG) score",
         CArgDescriptions::eInteger,
         NStr::IntToString(CSplicedAligner16::GetDefaultWi(1)).c_str());
    
    argdescr->AddDefaultKey
        ("Wi2", "Wi2", "Conventional intron (AT/AC) score",
         CArgDescriptions::eInteger,
         NStr::IntToString(CSplicedAligner16::GetDefaultWi(2)).c_str());
    
    argdescr->AddDefaultKey
        ("Wi3", "Wi3", "Arbitrary intron score",
         CArgDescriptions::eInteger,
         NStr::IntToString(CSplicedAligner16::GetDefaultWi(3)).c_str());
        
    argdescr->AddFlag ("noendgaps",
                       "Skip detection of unaligning regions at the ends.",
                       true);
    
    argdescr->AddFlag ("nopolya", "Assume no Poly(A) tail.",  true);
    

    CArgAllow* constrain01 = new CArgAllow_Doubles(0,1);
    argdescr->SetConstraint("min_compartment_idty", constrain01);
    argdescr->SetConstraint("min_exon_idty", constrain01);
    argdescr->SetConstraint("compartment_penalty", constrain01);
    
    CArgAllow* constrain_positives = new CArgAllow_Integers(1, kMax_Int);
    argdescr->SetConstraint("max_extent", constrain_positives);

}

void CSplignArgUtil::ArgsToSplign(CSplign* splign, const CArgs& args)
{
    splign->SetEndGapDetection(!(args["noendgaps"]));

    splign->SetPolyaDetection(!args["nopolya"]);

    splign->SetCompartmentPenalty(args["compartment_penalty"].AsDouble());

    splign->SetMinCompartmentIdentity(
                 args["min_compartment_idty"].AsDouble());

    if(args["min_singleton_idty"]) {
        splign->SetMinSingletonIdentity(args["min_singleton_idty"].AsDouble());
    }
    else {
        splign->SetMinSingletonIdentity(splign->GetMinCompartmentIdentity());
    }

    splign->SetMinSingletonIdentityBps(args["min_singleton_idty_bps"].AsInteger());

    splign->SetMaxGenomicExtent(args["max_extent"].AsInteger());

    splign->SetMinExonIdentity(args["min_exon_idty"].AsDouble());


    CRef<CSplicedAligner> aligner(
        static_cast<CSplicedAligner*>(new CSplicedAligner16));
        
    aligner->SetWm (args["Wm"].AsInteger());
    aligner->SetWms(args["Wms"].AsInteger());
    aligner->SetWg (args["Wg"].AsInteger());
    aligner->SetWs (args["Ws"].AsInteger());
    aligner->SetScoreMatrix(NULL);

    for(size_t i (0), n (aligner->GetSpliceTypeCount()); i < n; ++i) {
        string arg_name ("Wi");
        arg_name += NStr::IntToString(i);
        aligner->SetWi(i, args[arg_name.c_str()].AsInteger());
    }
    splign->SetAligner() = aligner;
}


END_NCBI_SCOPE
