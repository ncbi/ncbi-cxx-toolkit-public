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
#ifdef ALGOALIGN_NW_SPLIGN_MAKE_PUBLIC_BINARY
    argdescr->AddDefaultKey("type", "type",
                            "Query cDNA type: 'mrna' or 'est'",
                            CArgDescriptions::eString,
                            kQueryType_mRNA);
#endif
    
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

    CArgAllow * constrain01 (new CArgAllow_Doubles(0,1));
    argdescr->SetConstraint("min_compartment_idty", constrain01);
    argdescr->SetConstraint("min_exon_idty", constrain01);
    argdescr->SetConstraint("compartment_penalty", constrain01);

#ifdef ALGOALIGN_NW_SPLIGN_MAKE_PUBLIC_BINARY
    CArgAllow_Strings * constrain_querytype (new CArgAllow_Strings);
    constrain_querytype ->Allow(kQueryType_mRNA) ->Allow(kQueryType_EST);
    argdescr->SetConstraint("type", constrain_querytype);
#endif

}

void CSplignArgUtil::ArgsToSplign(CSplign* splign, const CArgs& args)
{
    splign->SetEndGapDetection(true);
    splign->SetPolyaDetection(true);

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

    CRef<CSplicedAligner> aligner(
        static_cast<CSplicedAligner*>(new CSplicedAligner16));
        
#ifdef ALGOALIGN_NW_SPLIGN_MAKE_PUBLIC_BINARY

    if(args["type"].AsString() == kQueryType_mRNA) {
        aligner->SetWm  (1000);
        aligner->SetWms (-1044);
        aligner->SetWg  (-3070);
        aligner->SetWs  (-173);
        aligner->SetScoreMatrix(NULL);

        aligner->SetWi(0, -4270);
        aligner->SetWi(1, -5314);
        aligner->SetWi(2, -6358);
        aligner->SetWi(3, -7395);
    }
    else {
        aligner->SetWm  (1000);
        aligner->SetWms (-1011);
        aligner->SetWg  (-1460);
        aligner->SetWs  (-464);
        aligner->SetScoreMatrix(NULL);

        aligner->SetWi(0, -4988);
        aligner->SetWi(1, -5999);
        aligner->SetWi(2, -7010);
        aligner->SetWi(3, -13060);
    }

#endif

    splign->SetAligner() = aligner;
}


END_NCBI_SCOPE
