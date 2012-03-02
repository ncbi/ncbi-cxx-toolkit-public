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
* Author:  Colleen Bollin
*
* File Description:
*   Validator command line argument utilities
*/


#include <ncbi_pch.hpp>

#include <objtools/validator/valid_cmdargs.hpp>

namespace {
    const size_t kMb (1u << 20);
}

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)

void CValidatorArgUtil::SetupArgDescriptions(CArgDescriptions* argdescr)
{    
    argdescr->AddFlag("A", "Validate Alignments");
    argdescr->AddFlag("J", "Require ISO-JTA");
    argdescr->AddFlag("Z", "Remote CDS Product Fetch");
    argdescr->AddFlag("X", "Exon Splice Check");
    argdescr->AddFlag("G", "Verify Inference Accessions");
    argdescr->AddFlag("M", "Match locus_tag against General ID");
    argdescr->AddFlag("Y", "Check Against Old IDs");
    argdescr->AddFlag("e", "Ignore Transcription/Translation Exceptions");
    argdescr->AddFlag("r", "Remote Fetching from ID");
    argdescr->AddFlag("y", "Special Indexer Tests");
    argdescr->AddFlag("U", "Genome Center Submission");
    argdescr->AddFlag("T", "Validate Taxonomy");
    argdescr->AddFlag("ovl_pep", "Overlapping peptide features produce error instead of warning");
    argdescr->AddFlag("rubisco", "Look for rubisco abbreviations");
    argdescr->AddFlag("far_fetch_mRNA", "Fetch far mRNA products");
    argdescr->AddFlag("require_taxid", "Require Taxonomy ID on BioSources");
    argdescr->AddFlag("non_ascii", "Report non-ASCII from reading");
    argdescr->AddFlag("suppress_context", "Suppress context when reporting");
    argdescr->AddFlag("splice_as_error", "Report splice problems as errors");
    argdescr->AddDefaultKey("N", "LatLonStrictness", "Flags for lat-lon tests (1 Test State/Province, 2 Ignore Water Exception)", CArgDescriptions::eInteger, "0"); 
}


int CValidatorArgUtil::ArgsToValidatorOptions(const CArgs& args)
{
    int options = 0;

    if (args["A"]) {
        options |= CValidator::eVal_val_align;
    }
    if (args["J"]) {
        options |= CValidator::eVal_need_isojta;
    }
    if (args["Z"]) {
        options |= CValidator::eVal_far_fetch_cds_products;
    }
    if (args["X"]) {
        options |= CValidator::eVal_val_exons;
    }
    if (args["G"]) {
        options |= CValidator::eVal_inference_accns;
    }
    if (args["M"]) {
        options |= CValidator::eVal_locus_tag_general_match;
    }
    if (args["Y"]) {
        options |= CValidator::eVal_validate_id_set;
    }
    if (args["e"]) {
        options |= CValidator::eVal_ignore_exceptions;
    }
    if (args["r"]) {
        options |= CValidator::eVal_remote_fetch;
    }
    if (args["y"]) {
        options |= CValidator::eVal_indexer_version;
    }
    if (args["U"]) {
        options |= CValidator::eVal_genome_submission;
    }
    if (args["T"]) {
        options |= CValidator::eVal_use_entrez;
    }
    if (args["ovl_pep"]) {
        options |= CValidator::eVal_ovl_pep_err;
    }
    if (args["rubisco"]) {
        options |= CValidator::eVal_do_rubisco_test;
    }
    if (args["far_fetch_mRNA"]) {
        options |= CValidator::eVal_far_fetch_mrna_products;
    }
    if (args["require_taxid"]) {
        options |= CValidator::eVal_need_taxid;
    }
    if (args["non_ascii"]) {
        options |= CValidator::eVal_non_ascii;
    }
    if (args["suppress_context"]) {
        options |= CValidator::eVal_no_context;
    }
    if (args["splice_as_error"]) {
        options |= CValidator::eVal_report_splice_as_error;
    }

    if (args["N"].AsInteger() & 1) {
        options |= CValidator::eVal_latlon_check_state;
    }
    if (args["N"].AsInteger() & 2) {
        options |= CValidator::eVal_latlon_ignore_water;
    }

    return options;
}

END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
