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
 * Authors:  Frank Ludwig
 *
 * File Description:  Sequence Ontology Type Mapping
 *
 */

#include <ncbi_pch.hpp>
//#include <objects/seq/genbank_type.hpp>
#include <objects/seq/sofa_type.hpp>
#include <objects/seq/sofa_map.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

#define GT( a, b ) CFeatListItem( CSeqFeatData::a, CSeqFeatData::b, "", "" )

void CSofaMap::x_Init() 
{
    m_default = SofaType( 1, "region" );

    m_Map[ GT( e_Cdregion, eSubtype_cdregion ) ] = SofaType( 316, "CDS" );

    m_Map[ GT( e_Gene, eSubtype_gene ) ]        = SofaType( 704, "gene" );

    m_Map[ GT( e_Rna, eSubtype_mRNA ) ]         = SofaType( 234, "mRNA" );
    m_Map[ GT( e_Rna, eSubtype_ncRNA ) ]        = SofaType( 655, "ncRNA" );
    m_Map[ GT( e_Rna, eSubtype_otherRNA ) ]     = SofaType( 673, "transcript" );
    m_Map[ GT( e_Rna, eSubtype_rRNA ) ]         = SofaType( 252, "rRNA" );
    m_Map[ GT( e_Rna, eSubtype_scRNA ) ]        = SofaType( 13, "scRNA" );
    m_Map[ GT( e_Rna, eSubtype_snRNA ) ]        = SofaType( 274, "snRNA" );
    m_Map[ GT( e_Rna, eSubtype_snoRNA ) ]       = SofaType( 275, "snoRNA" );
    m_Map[ GT( e_Rna, eSubtype_tRNA ) ]         = SofaType( 253, "tRNA" );
    m_Map[ GT( e_Rna, eSubtype_tmRNA ) ]        = SofaType( 584, "tmRNA" );

    m_Map[ GT( e_Imp, eSubtype_10_signal ) ]    = SofaType( 175, "minus_10_signal" );
    m_Map[ GT( e_Imp, eSubtype_35_signal ) ]    = SofaType( 176, "minus_35_signal" );
    m_Map[ GT( e_Imp, eSubtype_3clip ) ]        = SofaType( 557, "three_prime_clip" );
    m_Map[ GT( e_Imp, eSubtype_3UTR ) ]         = SofaType( 205, "three_prime_UTR" );
    m_Map[ GT( e_Imp, eSubtype_5clip ) ]        = SofaType( 555, "five_prime_clip" );
    m_Map[ GT( e_Imp, eSubtype_5UTR ) ]         = SofaType( 204, "five_prime_UTR" );
    m_Map[ GT( e_Imp, eSubtype_C_region ) ]     = SofaType( 478, "C_gene_segment" );
    m_Map[ GT( e_Imp, eSubtype_CAAT_signal ) ]  = SofaType( 172, "CAAT_signal" );
    m_Map[ GT( e_Imp, eSubtype_D_loop ) ]       = SofaType( 297, "D_loop" );
    m_Map[ GT( e_Imp, eSubtype_D_segment ) ]    = SofaType( 458, "D_gene_segment" );
    m_Map[ GT( e_Imp, eSubtype_exon ) ]         = SofaType( 147, "exon" );
    m_Map[ GT( e_Imp, eSubtype_GC_signal ) ]    = SofaType( 173, "GC_rich_region" );
    m_Map[ GT( e_Imp, eSubtype_J_segment ) ]    = SofaType( 470, "J_gene_segment" );
    m_Map[ GT( e_Imp, eSubtype_LTR ) ]          = SofaType( 286, "long_terminal_repeat" );
    m_Map[ GT( e_Imp, eSubtype_RBS ) ]          = SofaType( 139, "ribosome_entry_site" );
    m_Map[ GT( e_Imp, eSubtype_STS ) ]          = SofaType( 331, "STS" );
    m_Map[ GT( e_Imp, eSubtype_TATA_signal ) ]  = SofaType( 174, "TATA_box" );
    m_Map[ GT( e_Imp, eSubtype_attenuator ) ]   = SofaType( 140, "attenuator" );
    m_Map[ GT( e_Imp, eSubtype_enhancer ) ]     = SofaType( 165, "enhancer" );
    m_Map[ GT( e_Imp, eSubtype_gap ) ]          = SofaType( 730, "gap" );
    m_Map[ GT( e_Imp, eSubtype_iDNA ) ]         = SofaType( 723, "iDNA" );
    m_Map[ GT( e_Imp, eSubtype_intron ) ]       = SofaType( 188, "intron" );
    m_Map[ GT( e_Imp, eSubtype_mat_peptide ) ]  = SofaType( 419, "mature_peptide" );
    m_Map[ GT( e_Imp, eSubtype_misc_RNA ) ]     = SofaType( 673, "transcript" );
    m_Map[ GT( e_Imp, eSubtype_misc_binding ) ] = SofaType( 409, "binding_site" );
    m_Map[ GT( e_Imp, eSubtype_misc_difference) ] = SofaType( 413, "sequence_difference" );
    m_Map[ GT( e_Imp, eSubtype_misc_feature ) ] = SofaType( 1, "region" );
    m_Map[ GT( e_Imp, eSubtype_misc_recomb ) ]  = SofaType( 298, "recombination_feature" );
    m_Map[ GT( e_Imp, eSubtype_misc_signal ) ]  = SofaType( 5836, "regulatory_region" );
    m_Map[ GT( e_Imp, eSubtype_misc_structure ) ] = SofaType( 2, "sequence_secondary_structure" );
    m_Map[ GT( e_Imp, eSubtype_modified_base ) ]= SofaType( 305, "modified_base_site" );
    m_Map[ GT( e_Imp, eSubtype_operon ) ]       = SofaType( 178, "operon" );
    m_Map[ GT( e_Imp, eSubtype_oriT ) ]         = SofaType( 724, "oriT" );
    m_Map[ GT( e_Imp, eSubtype_polyA_signal ) ] = SofaType( 551, "polyA_signal_sequence" );
    m_Map[ GT( e_Imp, eSubtype_polyA_site ) ]   = SofaType( 553, "polyA_site" );
    m_Map[ GT( e_Imp, eSubtype_precursor_RNA ) ]= SofaType( 185, "primary_transcript" );
    m_Map[ GT( e_Imp, eSubtype_prim_transcript ) ] = SofaType( 185, "primary_transcript" );
    m_Map[ GT( e_Imp, eSubtype_primer_bind ) ]  = SofaType( 5850, "primer_binding_site" );
    m_Map[ GT( e_Imp, eSubtype_promoter ) ]     = SofaType( 167, "promoter" );
    m_Map[ GT( e_Imp, eSubtype_protein_bind ) ] = SofaType( 410, "protein_binding_site" );
    m_Map[ GT( e_Imp, eSubtype_repeat_region ) ] = SofaType( 657, "repeat_region" );
    m_Map[ GT( e_Imp, eSubtype_repeat_unit ) ]  = SofaType( 726, "repeat_unit" );
    m_Map[ GT( e_Imp, eSubtype_satellite ) ]    = SofaType( 5, "satellite_DNA" );
    m_Map[ GT( e_Imp, eSubtype_sig_peptide ) ]  = SofaType( 418, "signal_peptide" );
    m_Map[ GT( e_Imp, eSubtype_source ) ]       = SofaType( 2000061, "databank_entry" );
    m_Map[ GT( e_Imp, eSubtype_stem_loop ) ]    = SofaType( 313, "stem_loop" );
    m_Map[ GT( e_Imp, eSubtype_terminator ) ]   = SofaType( 141, "terminator" );
    m_Map[ GT( e_Imp, eSubtype_transit_peptide ) ] = SofaType( 725, "transit_peptide" );
    m_Map[ GT( e_Imp, eSubtype_V_segment ) ]    = SofaType( 466, "V_gene_segment" );
    m_Map[ GT( e_Imp, eSubtype_variation ) ]    = SofaType( 109, "sequence_variant" );
};

#undef GT
        
END_NCBI_SCOPE
