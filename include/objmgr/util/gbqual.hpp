#ifndef GBQUAL__HPP
#define GBQUAL__HPP

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
* Author:  Aaron Ucko <ucko@ncbi.nlm.nih.gov>
*
* File Description:
*   Qualifiers for GenBank/EMBL/DDBJ features
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2001/11/01 16:32:23  ucko
* Rework qualifier handling to support appropriate reordering
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/serialbase.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// Qualifiers for normal (non-source) features
class CGBQual
{
public:
    enum EType {
        eType_unknown,
        // Qualifiers should appear in this order:
        eType_partial,
        eType_gene,

        eType_product,

        eType_prot_EC_number,
        eType_prot_activity,

        eType_standard_name,
        eType_coded_by,

        eType_prot_name,
        eType_region_name,
        eType_site_type,
        eType_sec_str_type,
        eType_heterogen,

        eType_note, eType_citation,

        eType_number,
        
        eType_pseudo,
        
        eType_codon_start,

        eType_anticodon,
        eType_bound_moiety,
        eType_clone,
        eType_cons_splice,
        eType_direction,
        eType_function,
        eType_evidence,
        eType_exception,
        eType_frequency,
        eType_EC_number,
        eType_gene_map,
        eType_gene_allele,
        eType_allele,
        eType_map,
        eType_mod_base,
        eType_PCR_conditions,
        eType_phenotype,
        eType_rpt_family,
        eType_rpt_type,
        eType_rpt_unit,
        eType_usedin,

        eType_illegal_qual,

        eType_replace,

        eType_transl_except,
        eType_transl_table,
        eType_codon,
        eType_organism,
        eType_label,
        eType_cds_product,
        eType_protein_id,
        eType_transcript_id,
        eType_db_xref, eType_gene_xref,
        eType_translation
    };
    DECLARE_INTERNAL_ENUM_INFO(EType);

    CGBQual(EType type = eType_unknown, string value = kEmptyStr)
        : m_Type(type), m_Value(value) {}
    string ToString(void) const;

    DECLARE_INTERNAL_TYPE_INFO();

private:
    EType  m_Type;
    string m_Value;

    friend bool operator<(const CGBQual& q1, const CGBQual& q2);
};


// Qualifiers for source features
class CGBSQual
{
public:
    enum EType {
        eType_unknown,
        // Qualifiers should appear in this order:
        eType_organism,

        eType_organelle,
        eType_macronuclear,
        eType_proviral,
        eType_virion,

        eType_strain,
        eType_sub_strain,
        eType_variety,
        eType_serotype,
        eType_cultivar,
        eType_isolate,
        eType_specific_host,
        eType_sub_species,
        eType_specimen_voucher,

        eType_db_xref,

        eType_chromosome,
        eType_map,
        eType_clone,
        eType_sub_clone,
        eType_haplotype,
        eType_sex,
        eType_cell_line,
        eType_cell_type,
        eType_tissue_type,
        eType_clone_lib,
        eType_dev_stage,
        eType_frequency,

        eType_germline,
        eType_rearranged,

        eType_lab_host,
        eType_pop_variant,
        eType_tissue_lib,

        eType_plasmid,
        eType_transposon,
        eType_insertion_seq,

        eType_country,

        eType_focus,

        eType_note,

        eType_sequenced_mol,
        eType_label,
        eType_usedin,
        eType_citation
    };
    DECLARE_INTERNAL_ENUM_INFO(EType);

    CGBSQual(EType type = eType_unknown, string value = kEmptyStr)
        : m_Type(type), m_Value(value) {}
    string ToString(void) const;

    DECLARE_INTERNAL_TYPE_INFO();

private:
    EType  m_Type;
    string m_Value;

    friend bool operator<(const CGBSQual& q1, const CGBSQual& q2);
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* GBQUAL__HPP */
