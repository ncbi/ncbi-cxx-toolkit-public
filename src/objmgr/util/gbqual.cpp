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
* Revision 1.2  2001/11/02 20:54:51  ucko
* Make gbqual.hpp private; clean up cruft from genbank.hpp.
*
* Revision 1.1  2001/11/01 16:32:24  ucko
* Rework qualifier handling to support appropriate reordering
*
* ===========================================================================
*/

#include "gbqual.hpp"

#include <serial/serialimpl.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


static string s_Quote(const string& s)
{
    return '"' + NStr::Replace(s, "\"", "\"\"") + '"';
}


BEGIN_CLASS_INFO(CGBQual)
{
    ADD_ENUM_MEMBER(m_Type, EType);
    ADD_STD_MEMBER(m_Value);
}
END_CLASS_INFO


#define NORMAL_QUAL(x) ADD_ENUM_VALUE(#x, eType_ ## x)
BEGIN_ENUM_IN_INFO(CGBQual::, EType, false)
{
    NORMAL_QUAL(unknown);
    NORMAL_QUAL(partial);
    NORMAL_QUAL(gene);
    NORMAL_QUAL(product);
    NORMAL_QUAL(prot_EC_number);
    NORMAL_QUAL(prot_activity);
    NORMAL_QUAL(standard_name);
    NORMAL_QUAL(coded_by);
    NORMAL_QUAL(prot_name);
    NORMAL_QUAL(region_name);
    NORMAL_QUAL(site_type);
    NORMAL_QUAL(sec_str_type);
    NORMAL_QUAL(heterogen);
    NORMAL_QUAL(note);
    NORMAL_QUAL(citation);
    NORMAL_QUAL(number);
    NORMAL_QUAL(pseudo);
    NORMAL_QUAL(codon_start);
    NORMAL_QUAL(anticodon);
    NORMAL_QUAL(bound_moiety);
    NORMAL_QUAL(clone);
    NORMAL_QUAL(cons_splice);
    NORMAL_QUAL(direction);
    NORMAL_QUAL(function);
    NORMAL_QUAL(evidence);
    NORMAL_QUAL(exception);
    NORMAL_QUAL(frequency);
    NORMAL_QUAL(EC_number);
    NORMAL_QUAL(gene_map);
    NORMAL_QUAL(gene_allele);
    NORMAL_QUAL(allele);
    NORMAL_QUAL(map);
    NORMAL_QUAL(mod_base);
    NORMAL_QUAL(PCR_conditions);
    NORMAL_QUAL(phenotype);
    NORMAL_QUAL(rpt_family);
    NORMAL_QUAL(rpt_type);
    NORMAL_QUAL(rpt_unit);
    NORMAL_QUAL(usedin);
    NORMAL_QUAL(illegal_qual);
    NORMAL_QUAL(replace);
    NORMAL_QUAL(transl_except);
    NORMAL_QUAL(transl_table);
    NORMAL_QUAL(codon);
    NORMAL_QUAL(organism);
    NORMAL_QUAL(label);
    NORMAL_QUAL(cds_product);
    NORMAL_QUAL(protein_id);
    NORMAL_QUAL(transcript_id);
    NORMAL_QUAL(db_xref);
    NORMAL_QUAL(gene_xref);
    NORMAL_QUAL(translation);
}
END_ENUM_IN_INFO


bool operator<(const CGBQual& q1, const CGBQual& q2)
{
    if (q1.m_Type < q2.m_Type) {
        return true;
    } else if (q1.m_Type > q2.m_Type) {
        return false;
    } else {
        return q1.m_Value < q2.m_Value;
    }
}


string CGBQual::ToString(void) const
{
    string base = '/' + GetTypeInfo_enum_EType()->FindName(m_Type, false);
    // 3 possible styles: quoted arg (most common), unquoted arg, no arg.
    switch (m_Type) {
        // We need to use different names in some cases. (Sigh.)
    case eType_cds_product:     return "/product="       + s_Quote(m_Value);
    case eType_gene_allele:     return "/allele="        + s_Quote(m_Value);
    case eType_gene_map:        return "/map="           + s_Quote(m_Value);
    case eType_gene_xref:       return "/db_xref="       + s_Quote(m_Value);
    case eType_illegal_qual:    return "/illegal="       + s_Quote(m_Value);
        // case eType_product_quals:   return "/product="       + s_Quote(m_Value);
    case eType_prot_activity:   return "/function="      + s_Quote(m_Value);
    case eType_prot_EC_number:  return "/EC_number="     + s_Quote(m_Value);
    case eType_prot_name:       return "/name="          + s_Quote(m_Value);
        // case eType_xtra_prod_quals: return "/xtra_products=" + s_Quote(m_Value);

    case eType_anticodon:
    case eType_citation:
    case eType_codon:
    case eType_codon_start:
    case eType_cons_splice:
    case eType_direction:
    case eType_evidence:
    case eType_label:
    case eType_mod_base:
    case eType_number:
    case eType_rpt_type:
    case eType_rpt_unit:
    case eType_transl_except:
    case eType_transl_table:
    case eType_usedin:
        return base + '=' + m_Value;

    case eType_partial:
    case eType_pseudo:
        return base;

    default:
        return base + '=' + s_Quote(m_Value);
    }
}



BEGIN_CLASS_INFO(CGBSQual)
{
    ADD_ENUM_MEMBER(m_Type, EType);
    ADD_STD_MEMBER(m_Value);
}
END_CLASS_INFO


#define SOURCE_QUAL(x) ADD_ENUM_VALUE(#x, eType_ ## x)
BEGIN_ENUM_IN_INFO(CGBSQual::, EType, false)
{
    SOURCE_QUAL(unknown);
    SOURCE_QUAL(organism);
    SOURCE_QUAL(organelle);
    SOURCE_QUAL(macronuclear);
    SOURCE_QUAL(proviral);
    SOURCE_QUAL(virion);
    SOURCE_QUAL(strain);
    SOURCE_QUAL(sub_strain);
    SOURCE_QUAL(variety);
    SOURCE_QUAL(serotype);
    SOURCE_QUAL(cultivar);
    SOURCE_QUAL(isolate);
    SOURCE_QUAL(specific_host);
    SOURCE_QUAL(sub_species);
    SOURCE_QUAL(specimen_voucher);
    SOURCE_QUAL(db_xref);
    SOURCE_QUAL(chromosome);
    SOURCE_QUAL(map);
    SOURCE_QUAL(clone);
    SOURCE_QUAL(sub_clone);
    SOURCE_QUAL(haplotype);
    SOURCE_QUAL(sex);
    SOURCE_QUAL(cell_line);
    SOURCE_QUAL(cell_type);
    SOURCE_QUAL(tissue_type);
    SOURCE_QUAL(clone_lib);
    SOURCE_QUAL(dev_stage);
    SOURCE_QUAL(frequency);
    SOURCE_QUAL(germline);
    SOURCE_QUAL(rearranged);
    SOURCE_QUAL(lab_host);
    SOURCE_QUAL(pop_variant);
    SOURCE_QUAL(tissue_lib);
    SOURCE_QUAL(plasmid);
    SOURCE_QUAL(transposon);
    SOURCE_QUAL(insertion_seq);
    SOURCE_QUAL(country);
    SOURCE_QUAL(focus);
    SOURCE_QUAL(note);
    SOURCE_QUAL(sequenced_mol);
    SOURCE_QUAL(label);
    SOURCE_QUAL(usedin);
    SOURCE_QUAL(citation);
}
END_ENUM_IN_INFO


bool operator<(const CGBSQual& q1, const CGBSQual& q2)
{
    if (q1.m_Type < q2.m_Type) {
        return true;
    } else if (q1.m_Type > q2.m_Type) {
        return false;
    } else {
        return q1.m_Value < q2.m_Value;
    }
}


string CGBSQual::ToString(void) const
{
    string base = '/' + GetTypeInfo_enum_EType()->FindName(m_Type, false);
    // 3 possible styles: quoted arg (most common), unquoted arg, no arg
    switch (m_Type) {
    case eType_citation:
    case eType_label:
    case eType_usedin:
        return base + '=' + m_Value;

    case eType_focus:
    case eType_germline:
    case eType_macronuclear:
    case eType_proviral:
    case eType_rearranged:
    case eType_virion:
        return base;

    default:
        return base + '=' + s_Quote(m_Value);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
