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
 * Author:  Frank Ludwig
 *
 * File Description:
 *   GFF file reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objtools/readers/gff3_sofa.hpp>

#include <objects/seq/sofa_type.hpp>
#include <objects/seq/sofa_map.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

#define GT( a, b ) CFeatListItem( CSeqFeatData::a, CSeqFeatData::b, "", "" )

//  --------------------------------------------------------------------------
CSafeStatic<TLookupSofaToGenbank> CGff3SofaTypes::m_Lookup;
CSafeStatic<TAliasToTerm> CGff3SofaTypes::m_Aliases;
//  --------------------------------------------------------------------------

//  --------------------------------------------------------------------------
CGff3SofaTypes& SofaTypes()
//  --------------------------------------------------------------------------
{
    static CSafeStatic<CGff3SofaTypes> m_Lookup;    
    return *m_Lookup;
}

//  --------------------------------------------------------------------------
CGff3SofaTypes::CGff3SofaTypes()
//  --------------------------------------------------------------------------
{
    typedef map<CFeatListItem, SofaType> SOFAMAP;
    typedef SOFAMAP::const_iterator SOFAITER;

    CSofaMap SofaMap;
    const SOFAMAP& entries = SofaMap.Map();
    TLookupSofaToGenbank& lookup = *m_Lookup;

    for (SOFAITER cit = entries.begin(); cit != entries.end(); ++cit) {
        lookup[cit->second.m_name] = cit->first;
    }
    //overrides:
    lookup["primary_transcript"] = GT(e_Imp, eSubtype_preRNA);
    lookup["sequence_alteration"] = GT(e_Variation, eSubtype_variation_ref);
    lookup["signal_peptide"] = GT(e_Imp, eSubtype_sig_peptide);

    TAliasToTerm& aliases = *m_Aliases;
    aliases["-10_signal"] = "minus_10_signal";
    aliases["-35_signal"] = "minus_35_signal";
    aliases["3'UTR"] = "three_prime_UTR";
    aliases["3'clip"] = "three_prime_clip";
    aliases["5'UTR"] = "five_prime_UTR";
    aliases["5'clip"] = "five_prime_clip";
    aliases["C_region"] = "C_gene_segment";
    aliases["D-loop"] = "D_loop";
    aliases["D_segment"] = "D_gene_segment";
    aliases["GC_signal"] = "GC_rich_promoter_region";
    aliases["J_segment"] = "J_gene_segment";
    aliases["LTR"] = "long_terminal_repeat";
    aliases["RBS"] = "ribosome_entry_site";
    aliases["TATA_signal"] = "TATA_box";
    aliases["V_segment"] = "V_gene_segment";
    aliases["assembly_gap"] = "gap";
    aliases["Comment"] = "remark";
    aliases["conflict"] = "sequence_conflict";
    aliases["mat_peptide_nt"] = "mature_protein_region";
    aliases["mat_peptide"] = "mature_peptide";
    aliases["misc_binding"] = "binding_site";
    aliases["misc_difference"] = "sequence_difference";
    aliases["misc_feature"] = "sequence_feature";
    aliases["misc_recomb"] = "recombination_feature";
    aliases["misc_signal"] = "regulatory_region";
    aliases["misc_structure"] = "sequence_secondary_structure";
    aliases["mobile_element"] = "mobile_genetic_element";
    aliases["modified_base"] = "modified_DNA_base";
    aliases["misc_RNA"] = "transcript";
    aliases["polyA_signal"] = "polyA_signal_sequence";
    aliases["pre_RNA"] = "primary_transcript";
    aliases["precursor_RNA"] = "primary_transcript";
    aliases["proprotein"] = "immature_peptide_region";
    aliases["prim_transcript"] = "primary_transcript";
    aliases["primer_bind"] = "primer_binding_site";
    aliases["Protein"] = "polypeptide";
    aliases["protein_bind"] = "protein_binding_site";
    aliases["SecStr"] = "sequence_secondary_structure";
    aliases["rep_origin"] = "origin_of_replication";
    aliases["Rsite"] = "restriction_enzyme_cut_site";
    aliases["satellite"] = "satellite_DNA";
    aliases["sig_peptide_nt"] = "signal_peptide";
    aliases["sig_peptide"] = "signal_peptide";
    aliases["Site"] = "site";
    aliases["Site-ref"] = "site";
    aliases["transit_peptide_nt"] = "transit_peptide";
    aliases["unsure"] = "sequence_uncertainty";
    aliases["variation"] = "sequence_alteration";
    aliases["VariationRef"] = "sequence_alteration";
    aliases["virion"] = "viral_sequence";
};

//  --------------------------------------------------------------------------
CGff3SofaTypes::~CGff3SofaTypes()
//  --------------------------------------------------------------------------
{
};

//  --------------------------------------------------------------------------
CSeqFeatData::ESubtype CGff3SofaTypes::MapSofaTermToGenbankType(
    const string& strSofa )
//  --------------------------------------------------------------------------
{
    TLookupSofaToGenbankCit cit = m_Lookup->find( strSofa );
    if ( cit == m_Lookup->end() ) {
        return CSeqFeatData::eSubtype_bad;
    }
    return CSeqFeatData::ESubtype(cit->second.GetSubtype());
}

//  --------------------------------------------------------------------------
CFeatListItem CGff3SofaTypes::MapSofaTermToFeatListItem(
    const string& strSofa )
//  --------------------------------------------------------------------------
{
    TLookupSofaToGenbankCit cit = m_Lookup->find( strSofa );
    if ( cit == m_Lookup->end() ) {
        return CFeatListItem(CSeqFeatData::e_Imp, 
            CSeqFeatData::eSubtype_bad, "", "");
    }
    return cit->second;
}

//  ---------------------------------------------------------------------------
bool CGff3SofaTypes::IsStringSofaAlias(
    const string& str)
//  ---------------------------------------------------------------------------
{
    return (m_Aliases->find(str) != m_Aliases->end());
}

//  ---------------------------------------------------------------------------
string CGff3SofaTypes::MapSofaAliasToSofaTerm(
    const string& alias)
//  ---------------------------------------------------------------------------
{
    TAliasToTermCit cit = m_Aliases->find(alias);
    if (cit == m_Aliases->end()) {
        return "";
    }
    return cit->second;
}

#undef GT

END_objects_SCOPE
END_NCBI_SCOPE
