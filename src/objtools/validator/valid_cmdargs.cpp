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
    argdescr->AddFlag("y", "Special Indexer Tests");
    argdescr->AddFlag("U", "Genome Center Submission");
    argdescr->AddFlag("T", "Validate Taxonomy");
    argdescr->AddFlag("ovl_pep", "Overlapping peptide features produce error instead of warning");
    argdescr->AddFlag("rubisco", "Look for rubisco abbreviations");
    argdescr->AddFlag("far_fetch_mRNA", "Fetch far mRNA products");
    argdescr->AddFlag("w", "SeqSubmitParent Flag");
    argdescr->AddFlag("require_taxid", "Require Taxonomy ID on BioSources");
    argdescr->AddFlag("q", "Taxonomy Lookup");
    argdescr->AddFlag("suppress_context", "Suppress context when reporting");
    argdescr->AddFlag("splice_as_error", "Report splice problems as errors");
    argdescr->AddDefaultKey("N", "LatLonStrictness", "Flags for lat-lon tests (1 Test State/Province, 2 Ignore Water Exception)", CArgDescriptions::eInteger, "0");
    argdescr->AddFlag("B", "Do Barcode Validation");
    argdescr->AddFlag("refseq", "Use RefSeq Conventions");
    argdescr->AddFlag("collect_locus_tags", "Collect locus tags for formatted reports");
    argdescr->AddFlag("golden_file", "Suppress context part of message");
    argdescr->AddFlag("vdjc", "Compare CDS against VDJC segments");
    argdescr->AddFlag("g", "Ignore Inferences");
    argdescr->AddFlag("full_inference", "Force Inference Validation");
    argdescr->AddFlag("new_strain_test", "New Strain validation");
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
        options |= CValidator::eVal_far_fetch_mrna_products;
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
    if (args["w"]) {
        options |= CValidator::eVal_seqsubmit_parent;
    }
    if (args["q"]) {
        options |= CValidator::eVal_do_tax_lookup;
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

    if (args["B"]) {
        options |= CValidator::eVal_do_barcode_tests;
    }

    if (args["refseq"]) {
        options |= CValidator::eVal_refseq_conventions;
    }

    if (args["r"]) {
        options |= CValidator::eVal_far_fetch_cds_products;
        options |= CValidator::eVal_far_fetch_mrna_products;
    }

    if (args["collect_locus_tags"]) {
        options |= CValidator::eVal_collect_locus_tags;
    }

    if (args["golden_file"]) {
        options |= CValidator::eVal_generate_golden_file;
    }

    if (args["vdjc"]) {
        options |= CValidator::eVal_compare_vdjc_to_cds;
    }

    if (args["g"]) {
        options |= CValidator::eVal_ignore_inferences;
    }
    if (args["full_inference"]) {
        options |= CValidator::eVal_force_inferences;
    }
    if (args["new_strain_test"]) {
        options |= CValidator::eVal_new_strain_validation;
    }

    return options;
}

END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE
