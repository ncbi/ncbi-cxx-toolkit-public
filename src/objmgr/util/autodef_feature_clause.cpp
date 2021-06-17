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
* Author:  Colleen Bollin
*
* File Description:
*   Generate unique definition lines for a set of sequences using organism
*   descriptions and feature clauses.
*/

#include <ncbi_pch.hpp>
#include <algorithm>
#include <objmgr/util/autodef.hpp>
#include <corelib/ncbimisc.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/RNA_gen.hpp>

#include <serial/iterator.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

using namespace sequence;

CAutoDefFeatureClause::CAutoDefFeatureClause(CBioseq_Handle bh, const CSeq_feat& main_feat, const CSeq_loc& mapped_loc, const CAutoDefOptions& opts) 
                              : CAutoDefFeatureClause_Base(opts),
                                m_pMainFeat(&main_feat),
                                m_BH(bh)
{
    x_SetBiomol();
    m_ClauseList.clear();
    m_GeneName = "";
    m_AlleleName = "";
    m_Interval = "";
    m_IsAltSpliced = false;
    m_Pluralizable = false;
    m_TypewordChosen = x_GetFeatureTypeWord(m_Typeword);
    m_ShowTypewordFirst = x_ShowTypewordFirst(m_Typeword);
    m_Description = "";
    m_DescriptionChosen = false;
    m_ProductName = "";
    m_ProductNameChosen = false;

    CSeqFeatData::ESubtype subtype = m_pMainFeat->GetData().GetSubtype();

    m_ClauseLocation = new CSeq_loc();
    m_ClauseLocation->Add(mapped_loc);

    if (subtype == CSeqFeatData::eSubtype_operon || IsGeneCluster()) {
        m_SuppressSubfeatures = true;
    }

    if (m_pMainFeat->CanGetComment() && NStr::Find(m_pMainFeat->GetComment(), "alternatively spliced") != NCBI_NS_STD::string::npos
        && (subtype == CSeqFeatData::eSubtype_cdregion
        || subtype == CSeqFeatData::eSubtype_exon
        || IsNoncodingProductFeat())) {
        m_IsAltSpliced = true;
    }
}


CAutoDefFeatureClause::~CAutoDefFeatureClause()
{
}


CSeqFeatData::ESubtype CAutoDefFeatureClause::GetMainFeatureSubtype() const
{
    if (IsLTR(*m_pMainFeat)) {
        return CSeqFeatData::eSubtype_LTR;
    }
    return m_pMainFeat->GetData().GetSubtype();
}


bool CAutoDefFeatureClause::IsMobileElement() const
{
    if (m_pMainFeat->GetData().GetSubtype() != CSeqFeatData::eSubtype_mobile_element) {
        return false;
    } else {
        return true;
    }
}


bool CAutoDefFeatureClause::IsInsertionSequence() const
{
    if (m_pMainFeat->GetData().GetSubtype() != CSeqFeatData::eSubtype_repeat_region
        || NStr::IsBlank(m_pMainFeat->GetNamedQual("insertion_seq"))) {
        return false;
    } else {
        return true;
    }
}


bool CAutoDefFeatureClause::IsControlRegion (const CSeq_feat& feat)
{
    if (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_misc_feature
        && feat.CanGetComment()
        && NStr::StartsWith(feat.GetComment(), "control region")) {
        return true;
    } else {
        return false;
    }
}


bool CAutoDefFeatureClause::IsControlRegion() const
{
    return IsControlRegion(*m_pMainFeat);
}


bool CAutoDefFeatureClause::IsEndogenousVirusSourceFeature () const
{
    if (m_pMainFeat->GetData().GetSubtype() != CSeqFeatData::eSubtype_biosrc
        || !m_pMainFeat->GetData().GetBiosrc().CanGetSubtype()) {
        return false;
    }
    ITERATE (CBioSource::TSubtype, subSrcI, m_pMainFeat->GetData().GetBiosrc().GetSubtype()) {
        if ((*subSrcI)->GetSubtype() == CSubSource::eSubtype_endogenous_virus_name) {
             return true;
        }
    }
    return false;
}


bool CAutoDefFeatureClause::IsGeneCluster () const
{
    return IsGeneCluster (*m_pMainFeat);
}


bool CAutoDefFeatureClause::IsGeneCluster (const CSeq_feat& feat)
{
    if (feat.GetData().GetSubtype() != CSeqFeatData::eSubtype_misc_feature
        || !feat.CanGetComment()) {
        return false;
    }
    
    string comment = feat.GetComment();
    if (NStr::Find(comment, "gene cluster") != string::npos 
        || NStr::Find(comment, "gene locus") != string::npos) {
        return true;
    } else {
        return false;
    }
}


bool CAutoDefFeatureClause::IsRecognizedFeature() const
{
    CSeqFeatData::ESubtype subtype = m_pMainFeat->GetData().GetSubtype();
    if (subtype == CSeqFeatData::eSubtype_3UTR
        || subtype == CSeqFeatData::eSubtype_5UTR
        || IsLTR(*m_pMainFeat)
        || subtype == CSeqFeatData::eSubtype_cdregion
        || subtype == CSeqFeatData::eSubtype_gene
        || subtype == CSeqFeatData::eSubtype_mRNA
        || subtype == CSeqFeatData::eSubtype_operon
        || subtype == CSeqFeatData::eSubtype_exon
        || subtype == CSeqFeatData::eSubtype_intron
        || subtype == CSeqFeatData::eSubtype_rRNA
        || subtype == CSeqFeatData::eSubtype_tRNA
        || subtype == CSeqFeatData::eSubtype_otherRNA
        || subtype == CSeqFeatData::eSubtype_misc_RNA
        || subtype == CSeqFeatData::eSubtype_ncRNA
        || subtype == CSeqFeatData::eSubtype_preRNA
        || subtype == CSeqFeatData::eSubtype_tmRNA
        || subtype == CSeqFeatData::eSubtype_D_loop
        || subtype == CSeqFeatData::eSubtype_regulatory
        || subtype == CSeqFeatData::eSubtype_misc_recomb
        || IsNoncodingProductFeat()
        || IsMobileElement()
        || IsInsertionSequence()
        || IsControlRegion()
        || IsEndogenousVirusSourceFeature()
        || IsSatelliteClause()
        || IsPromoter()
        || IsGeneCluster()
        || GetClauseType() != eDefault) {
        return true;
    } else {
        return false;
    }
}


void CAutoDefFeatureClause::x_SetBiomol()
{
    m_Biomol = CMolInfo::eBiomol_genomic;
    CSeqdesc_CI desc_iter(m_BH, CSeqdesc::e_Molinfo);
    for ( ;  desc_iter;  ++desc_iter) {
        if (desc_iter->GetMolinfo().IsSetBiomol()) {
            m_Biomol = desc_iter->GetMolinfo().GetBiomol();
        }
    }
}


bool CAutoDefFeatureClause::IsPseudo(const CSeq_feat& f)
{
    bool is_pseudo = false;
    if (f.CanGetPseudo() && f.IsSetPseudo()) {
        is_pseudo = true;
    } else if (f.IsSetQual()) {
        for (auto& it : f.GetQual()) {
            if (it->IsSetQual() && NStr::EqualNocase(it->GetQual(), "pseudogene")) {
                is_pseudo = true;
                break;
            }
        }
    }
    return is_pseudo;
}


bool CAutoDefFeatureClause::x_IsPseudo() 
{
    return (m_GeneIsPseudo || IsPseudo(*m_pMainFeat));
}


void CAutoDefFeatureClause::x_TypewordFromSequence()
{
    if (m_Biomol == CMolInfo::eBiomol_genomic) {
        m_Typeword = "genomic sequence";
    } else if (m_Biomol == CMolInfo::eBiomol_mRNA) {
        m_Typeword = "mRNA sequence";
    } else {
        m_Typeword = "sequence";
    }
    m_TypewordChosen = true;
}


bool CAutoDefFeatureClause::x_GetFeatureTypeWord(string &typeword)
{
    string qual, comment;

    if (IsLTR(*m_pMainFeat)) {
        typeword = "LTR repeat region";
        return true;
    }
  
    CSeqFeatData::ESubtype subtype = m_pMainFeat->GetData().GetSubtype();
    switch (subtype) {
        case CSeqFeatData::eSubtype_exon:
            typeword = "exon";
            return true;
            break;
        case CSeqFeatData::eSubtype_intron:
            typeword = "intron";
            return true;
            break;
        case CSeqFeatData::eSubtype_D_loop:
            typeword = "D-loop";
            return true;
            break;
        case CSeqFeatData::eSubtype_3UTR:
            typeword = "3' UTR";
            return true;
            break;
        case CSeqFeatData::eSubtype_5UTR:
            typeword = "5' UTR";
            return true;
            break;
        case CSeqFeatData::eSubtype_operon:
            typeword = "operon";
            return true;
            break;
        case CSeqFeatData::eSubtype_repeat_region:
            //if has insertion_seq gbqual
            if (IsInsertionSequence()) {
                typeword = "insertion sequence";
                return true;
            }
            qual = m_pMainFeat->GetNamedQual("endogenous_virus");
            if (!NStr::IsBlank(qual)) {
                typeword = "endogenous virus";
                return true;
            }
            if (IsMobileElement()) {
                typeword = "transposon";
                return true;
            }
            typeword = "repeat region";
            return true;
            break;
        case CSeqFeatData::eSubtype_misc_feature:
            if (m_pMainFeat->CanGetComment()) {
                comment = m_pMainFeat->GetComment();
                if (NStr::StartsWith(comment, "control region", NStr::eNocase)) {
                    typeword = "control region";
                    return true;
                }
            }
            break;
        case CSeqFeatData::eSubtype_misc_recomb:
            x_TypewordFromSequence();
            return true;
            break;
        case CSeqFeatData::eSubtype_biosrc:
            if (IsEndogenousVirusSourceFeature()) {
                typeword = "endogenous virus";
                return true;
            }
            break;
        case CSeqFeatData::eSubtype_regulatory:
            if (m_pMainFeat->IsSetQual()) {
                ITERATE(CSeq_feat::TQual, q, m_pMainFeat->GetQual()) {
                    if ((*q)->IsSetQual() &&
                        NStr::Equal((*q)->GetQual(), "regulatory_class") &&
                        (*q)->IsSetVal() && !NStr::IsBlank((*q)->GetVal())) {
                        typeword = (*q)->GetVal();
                        return true;
                    }
                }
            }
            break;
        default:
            break;
    }
    
    if (m_Biomol == CMolInfo::eBiomol_genomic || m_Biomol == CMolInfo::eBiomol_cRNA) {
        if (x_IsPseudo()) {
            typeword = "pseudogene";
            return true;
        } else {
            typeword = "gene";
            return true;
        }
    } else if (subtype == CSeqFeatData::eSubtype_rRNA 
               || subtype == CSeqFeatData::eSubtype_snoRNA
               || subtype == CSeqFeatData::eSubtype_snRNA
			   || subtype == CSeqFeatData::eSubtype_ncRNA) {
        return false;
    } else if (subtype == CSeqFeatData::eSubtype_precursor_RNA) {
        typeword = "precursor RNA";
        return true;
    } else if (m_Biomol == CMolInfo::eBiomol_mRNA) {
        if (x_IsPseudo()) {
            typeword = "pseudogene mRNA";
        } else {
            typeword = "mRNA";
        }
        return true;
    } else if (m_Biomol == CMolInfo::eBiomol_pre_RNA) {
        if (x_IsPseudo()) {
            typeword = "pseudogene precursor RNA";
        } else {
            typeword = "precursor RNA";
        }
        return true;
    } else if (m_Biomol == CMolInfo::eBiomol_other_genetic) {
        typeword = "gene";
        return true;
    }
    typeword = "";
    return true;
}


bool CAutoDefFeatureClause::x_ShowTypewordFirst(string typeword)
{
    if (NStr::Equal(typeword, "")) {
        return false;
    } else if (NStr::EqualNocase(typeword, "exon")
               || NStr::EqualNocase(typeword, "intron")
               || NStr::EqualNocase(typeword, "transposon")
               || NStr::EqualNocase(typeword, "insertion sequence")
               || NStr::EqualNocase(typeword, "endogenous virus")
               || NStr::EqualNocase(typeword, "retrotransposon")             
               || NStr::EqualNocase(typeword, "P-element")
               || NStr::EqualNocase(typeword, "transposable element")
               || NStr::EqualNocase(typeword, "integron")
               || NStr::EqualNocase(typeword, "superintegron")
               || NStr::EqualNocase(typeword, "MITE")) {
        return true;
    } else {
        return false;
    }
}


bool CAutoDefFeatureClause::x_FindNoncodingFeatureKeywordProduct (string comment, string keyword, string &product_name) const
{
    if (NStr::IsBlank(comment) || NStr::IsBlank(keyword)) {
        return false;
    }
    string::size_type start_pos = 0;
    
    while (start_pos != NCBI_NS_STD::string::npos) {
        start_pos = NStr::Find(comment, keyword, start_pos);
        if (start_pos != NCBI_NS_STD::string::npos) {
            string possible = comment.substr(start_pos + keyword.length());
            NStr::TruncateSpacesInPlace(possible);
            if (!NStr::StartsWith(possible, "GenBank Accession Number")) {
                product_name = possible;
                // truncate at first semicolon
                string::size_type end = NStr::Find(product_name, ";");
                if (end != NCBI_NS_STD::string::npos) {
                    product_name = product_name.substr(0, end);
                }
                // remove sequence from end of product name if found
                if (NStr::EndsWith(product_name, " sequence")) {
                    product_name = product_name.substr(0, product_name.length() - 9);
                }
                // add "-like" if not present
                if (!NStr::EndsWith(product_name, "-like")) {
                    product_name += "-like";
                }
                return true;
            } else {
                start_pos += keyword.length();
            }
        }
    }
    return false;
}


bool CAutoDefFeatureClause::x_GetNoncodingProductFeatProduct (string &product_name) const
{
    if (GetMainFeatureSubtype() != CSeqFeatData::eSubtype_misc_feature
        || !m_pMainFeat->CanGetComment()) {
        return false;
    }
    string comment = m_pMainFeat->GetComment();
    string::size_type start_pos = NStr::Find(comment, "nonfunctional ");
    if (start_pos != NCBI_NS_STD::string::npos) {
        string::size_type sep_pos = NStr::Find (comment, " due to ", start_pos);
        if (sep_pos != NCBI_NS_STD::string::npos) {
            product_name = comment.substr(start_pos, sep_pos - start_pos);
            return true;
        }
    }
    if (x_FindNoncodingFeatureKeywordProduct (comment, "similar to ", product_name)) {
        return true;
    } else if (x_FindNoncodingFeatureKeywordProduct (comment, "contains ", product_name)) {
        return true;
    } else {
        return false;
    }
}

bool CAutoDefFeatureClause::IsNoncodingProductFeat() const
{
    string product_name;
    return x_GetNoncodingProductFeatProduct(product_name);
}

CAutoDefGeneClause::CAutoDefGeneClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, const CAutoDefOptions& opts)
    : CAutoDefFeatureClause(bh, main_feat, mapped_loc, opts)
{
    m_GeneName = x_GetGeneName(m_pMainFeat->GetData().GetGene(), GetSuppressLocusTag());
    if (m_pMainFeat->GetData().GetGene().CanGetAllele()) {
        m_AlleleName = m_pMainFeat->GetData().GetGene().GetAllele();
        if (!NStr::StartsWith(m_AlleleName, m_GeneName, NStr::eNocase)) {
            if (!NStr::StartsWith(m_AlleleName, "-")) {
                m_AlleleName = "-" + m_AlleleName;
            }
            m_AlleleName = m_GeneName + m_AlleleName;
        }
    }
    m_GeneIsPseudo = IsPseudo(*m_pMainFeat);
    m_HasGene = true;
}


bool CAutoDefGeneClause::x_IsPseudo()
{
    if (CAutoDefFeatureClause::x_IsPseudo()) {
        return true;
    }
    const CGene_ref& gene = m_pMainFeat->GetData().GetGene();
    if (gene.CanGetPseudo() && gene.IsSetPseudo()) {
        return true;
    }
    return false;
}

/*
*If the feature is a gene and has different strings in the description than
* in the locus or locus tag, the description will be used as the product for
* the gene.
*/
bool CAutoDefGeneClause::x_GetProductName(string &product_name)
{
    if (m_pMainFeat->GetData().GetGene().CanGetDesc()
        && !NStr::Equal(m_pMainFeat->GetData().GetGene().GetDesc(),
        m_GeneName)) {
        product_name = m_pMainFeat->GetData().GetGene().GetDesc();
        return true;
    } else {
        return false;
    }
}


bool CAutoDefParsedtRNAClause::ParseString(string comment, string& gene_name, string& product_name)
{
    product_name = "";
    gene_name = "";    

    NStr::TruncateSpacesInPlace(comment);
    if (NStr::EndsWith (comment, " gene")) {
        comment = comment.substr (0, comment.length() - 5);
    } else if (NStr::EndsWith (comment, " genes")) {
        comment = comment.substr (0, comment.length() - 6);
    }

    string::size_type pos = NStr::Find(comment, "(");
    if (pos == NCBI_NS_STD::string::npos) {
        if (NStr::StartsWith (comment, "tRNA-")) {
            product_name = comment;
        } else {
            /* if not tRNA, gene name is required */
            return false;
        }
    } else {
        product_name = comment.substr(0, pos);
        comment = comment.substr (pos + 1);
        pos = NStr::Find(comment, ")");
        if (pos == NCBI_NS_STD::string::npos) {
            return false;
        }
        gene_name = comment.substr (0, pos);
        NStr::TruncateSpacesInPlace(gene_name);
    }
    NStr::TruncateSpacesInPlace(product_name);
    
    if (NStr::StartsWith (product_name, "tRNA-")) {
        /* tRNA name must start with "tRNA-" and be followed by one uppercase letter and
         * two lowercase letters.
         */
        if (product_name.length() < 8
            || !isalpha(product_name.c_str()[5]) || !isupper(product_name.c_str()[5])
            || !isalpha(product_name.c_str()[6]) || !islower(product_name.c_str()[6])
            || !isalpha(product_name.c_str()[7]) || !islower(product_name.c_str()[7])) {
            return false;
        }

        /* if present, gene name must start with letters "trn",
         * and end with one uppercase letter.
         */    
        if (!NStr::IsBlank (gene_name) 
            && (gene_name.length() < 4
                || !NStr::StartsWith(gene_name, "trn" )
                || !isalpha(gene_name.c_str()[3])
                || !isupper(gene_name.c_str()[3]))) {
            return false;
        }
    }
    if (NStr::IsBlank (product_name)) {
        return false;
    }
    return true;
}


CAutoDefParsedtRNAClause *s_tRNAClauseFromNote(CBioseq_Handle bh, const CSeq_feat& cf, const CSeq_loc& mapped_loc, string comment, bool is_first, bool is_last, const CAutoDefOptions& opts)
{
    string product_name;
    string gene_name;
    if (!CAutoDefParsedtRNAClause::ParseString(comment, gene_name, product_name)) {
        return NULL;
    }

    return new CAutoDefParsedtRNAClause(bh, cf, mapped_loc, gene_name, product_name, is_first, is_last, opts);
}


string CAutoDefFeatureClause::x_GetGeneName(const CGene_ref& gref, bool suppress_locus_tag) const
{
    if (gref.IsSuppressed()) {
        return "";
    } else if (gref.CanGetLocus() && !NStr::IsBlank(gref.GetLocus())) {
        return gref.GetLocus();
    } else if (!suppress_locus_tag && gref.IsSetLocus_tag() && !NStr::IsBlank(gref.GetLocus_tag())) {
        return gref.GetLocus_tag();
    } else if (gref.IsSetDesc() && !NStr::IsBlank(gref.GetDesc())) {
        return gref.GetDesc();
    } else {
        return "";
    }
}


void s_UseCommentBeforeSemicolon(const CSeq_feat& feat, string& label)
{
    if (feat.IsSetComment()) {
        label = feat.GetComment();
        string::size_type pos = NStr::Find(label, ";");
        if (pos != NCBI_NS_STD::string::npos) {
            label = label.substr(0, pos);
        }
    }
}


/* Frequently the product associated with a feature is listed as part of the
 * description of the feature in the definition line.  This function determines
 * the name of the product associated with this specific feature.  Some
 * features will be listed with the product of a feature that is associated
 * with the feature being described - this function does not look at other
 * features to determine a product name.
 * If the feature is a misc_feat with particular keywords in the comment,
 * the product will be determined based on the contents of the comment.
 * If the feature is a CDS and is marked as pseudo, the product will be
 * determined based on the contents of the comment.
 * If the feature is a gene and has different strings in the description than
 * in the locus or locus tag, the description will be used as the product for
 * the gene.
 * If none of the above conditions apply, the sequence indexing context label
 * will be used to obtain the product name for the feature.
 */
bool CAutoDefFeatureClause::x_GetProductName(string &product_name)
{
    CSeqFeatData::ESubtype subtype = m_pMainFeat->GetData().GetSubtype();
    
    if (subtype == CSeqFeatData::eSubtype_misc_feature && x_GetNoncodingProductFeatProduct(product_name)) {
        return true;
    } else if (subtype == CSeqFeatData::eSubtype_cdregion 
               && m_pMainFeat->CanGetPseudo() 
               && m_pMainFeat->IsSetPseudo()
               && m_pMainFeat->CanGetComment()) {
        string comment = m_pMainFeat->GetComment();
        if (!NStr::IsBlank(comment)) {
            string::size_type pos = NStr::Find(comment, ";");
            if (pos != NCBI_NS_STD::string::npos) {
                comment = comment.substr(0, pos);
            }
            product_name = comment;
            return true;
        }
    } else if (subtype == CSeqFeatData::eSubtype_tmRNA) {
        product_name = "tmRNA";
        return true;
    } else if (m_pMainFeat->GetData().Which() == CSeqFeatData::e_Rna) {
        product_name = m_pMainFeat->GetData().GetRna().GetRnaProductName();
        if (NStr::IsBlank(product_name) && m_pMainFeat->IsSetComment()) {
            product_name = m_pMainFeat->GetComment();
        }
        return true;
    } else if (subtype == CSeqFeatData::eSubtype_regulatory) {
        return true;
    } else if (subtype == CSeqFeatData::eSubtype_misc_recomb) {
        if (m_pMainFeat->IsSetQual()) {
            ITERATE(CSeq_feat::TQual, q, m_pMainFeat->GetQual()) {
                if ((*q)->IsSetQual() && NStr::Equal((*q)->GetQual(), "recombination_class") &&
                    (*q)->IsSetVal() && !NStr::IsBlank((*q)->GetVal())) {
                    product_name = (*q)->GetVal();
                    return true;
                }
            }
        }
        s_UseCommentBeforeSemicolon(*m_pMainFeat, product_name);
        return true;
    } else if (subtype == CSeqFeatData::eSubtype_exon || subtype == CSeqFeatData::eSubtype_intron) {
        return x_GetExonDescription(product_name);
    } else {
        string label;
        
        if (subtype == CSeqFeatData::eSubtype_cdregion && m_pMainFeat->IsSetProduct() && !m_Opts.IsFeatureSuppressed(CSeqFeatData::eSubtype_mat_peptide_aa)) {
            const CSeq_loc& product_loc = m_pMainFeat->GetProduct();
            CBioseq_Handle prot_h = m_BH.GetScope().GetBioseqHandle(product_loc);
            if (prot_h) {
                CFeat_CI prot_f(prot_h, CSeqFeatData::eSubtype_prot);
                if (prot_f) {
                    feature::GetLabel(*(prot_f->GetSeq_feat()), &label, feature::fFGL_Content);
                    if (m_pMainFeat->IsSetPartial() && m_pMainFeat->GetPartial()) {
                        // RW-1216 suppress mat-peptide region phrase if sig-peptide also present
                        CFeat_CI sig_pi(prot_h, CSeqFeatData::eSubtype_sig_peptide_aa);
                        if (!sig_pi) {
                            CFeat_CI mat_pi(prot_h, CSeqFeatData::eSubtype_mat_peptide_aa);
                            if (mat_pi && mat_pi->GetData().GetProt().IsSetName()) {
                                const string&  m_name = mat_pi->GetData().GetProt().GetName().front();
                                ++mat_pi;
                                if (!mat_pi && !m_name.empty()) {
                                    if (label.empty()) {
                                        label = m_name;
                                    }
                                    else {
                                        label += ", " + m_name + " region,";
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        if (NStr::IsBlank(label)) {                    
            feature::GetLabel(*m_pMainFeat, &label, feature::fFGL_Content);
        }
        if ((subtype == CSeqFeatData::eSubtype_cdregion && !NStr::Equal(label, "CDS"))
            || (subtype == CSeqFeatData::eSubtype_mRNA && !NStr::Equal(label, "mRNA"))
            || (subtype != CSeqFeatData::eSubtype_cdregion && subtype != CSeqFeatData::eSubtype_mRNA)) {
        } else {
            label = "";
        }
        
        // remove unwanted "mRNA-" tacked onto label for mRNA features
        if (subtype == CSeqFeatData::eSubtype_mRNA && NStr::StartsWith(label, "mRNA-")) {
            label = label.substr(5);
        } else if (subtype == CSeqFeatData::eSubtype_rRNA && NStr::StartsWith(label, "rRNA-")) {
            label = label.substr(5);
        }
       
        if (!NStr::IsBlank(label)) {
            product_name = label;
            return true;
        } else {
            product_name = "";
            return false;
        }        
    }
    return false;
}


bool CAutoDefFeatureClause::x_GetExonDescription(string &description)
{
    if (m_pMainFeat->IsSetQual()) {
        ITERATE(CSeq_feat::TQual, it, m_pMainFeat->GetQual()) {
            if ((*it)->IsSetQual() && (*it)->IsSetVal()
                && NStr::EqualNocase((*it)->GetQual(), "number")) {
                description = (*it)->GetVal();
                return true;
            }
        }
    }
    description = kEmptyStr;
    return false;
}


bool CAutoDefFeatureClause::x_GetDescription(string &description)
{
    CSeqFeatData::ESubtype subtype = m_pMainFeat->GetData().GetSubtype();

    description = "";
    if (subtype == CSeqFeatData::eSubtype_exon || subtype == CSeqFeatData::eSubtype_intron) {
        return x_GetExonDescription(description);
    } else if (NStr::Equal(m_Typeword, "insertion sequence")) {
        description = m_pMainFeat->GetNamedQual("insertion_seq");
        if (NStr::Equal(description, "unnamed")
            || NStr::IsBlank(description)) {
            description = "";
            return false;
        } else {
            return true;
        }
    } else if (subtype == CSeqFeatData::eSubtype_repeat_region) {
        if (NStr::Equal(m_Typeword, "endogenous virus")) {
            description = m_pMainFeat->GetNamedQual("endogenous_virus");
            if (NStr::Equal(description, "unnamed")
                || NStr::IsBlank(description)) {
                description = "";
                return false;
            } else {
                return true;
            }
        } else {
            description = m_pMainFeat->GetNamedQual("rpt_family");
            if (NStr::IsBlank(description) && m_pMainFeat->IsSetComment()) {
                description = m_pMainFeat->GetComment();
                if (IsLTR() && NStr::EndsWith(description, " LTR")) {
                    description = description.substr(0, description.length() - 4);
                }
            }
            return true;
        }
    } else if (subtype == CSeqFeatData::eSubtype_biosrc
               && NStr::Equal(m_Typeword, "endogenous virus")) {
        if (m_pMainFeat->GetData().GetBiosrc().CanGetSubtype()) {
            ITERATE (CBioSource::TSubtype, subSrcI, m_pMainFeat->GetData().GetBiosrc().GetSubtype()) {
                if ((*subSrcI)->GetSubtype() == CSubSource::eSubtype_endogenous_virus_name) {
                    description = (*subSrcI)->GetName();
                    if (NStr::Equal(description, "unnamed")
                        || NStr::IsBlank(description)) {
                        description = "";
                    } else {
                        return true;
                    }                    
                }
            }
        }
        return false;
    } else if (NStr::Equal(m_Typeword, "control region")
               || NStr::Equal(m_Typeword, "D-loop")
               || subtype == CSeqFeatData::eSubtype_3UTR
               || subtype == CSeqFeatData::eSubtype_5UTR) {
        return false;
    } else if (IsLTR(*m_pMainFeat)) {
        if (m_pMainFeat->CanGetComment()) {
            string comment = m_pMainFeat->GetComment();
            if (NStr::StartsWith(comment, "LTR ")) {
                comment = comment.substr(4);
            } else if (NStr::EndsWith(comment, " LTR")) {
                comment = comment.substr(0, comment.length() - 4);
            }
            description = comment;
        }
        if (NStr::IsBlank(description)) {
            return false;
        } else {
            return true;
        }
    } else if (subtype == CSeqFeatData::eSubtype_operon) {
        description = m_pMainFeat->GetNamedQual("operon");
        return true;
    } else {
        if (!m_ProductNameChosen) {
            m_ProductNameChosen = x_GetProductName(m_ProductName);
        }
        
        if (!NStr::IsBlank(m_GeneName) && !NStr::IsBlank(m_ProductName)) {
            description = m_ProductName + " (" + m_GeneName + ")";
        } else if (!NStr::IsBlank(m_GeneName)) {
            description = m_GeneName;
        } else if (!NStr::IsBlank(m_ProductName)) {
            description = m_ProductName;
        }
        if (NStr::IsBlank(description)) {
            return false;
        } else {
            return true;
        }
    }
}


bool CAutoDefFeatureClause::IsSatelliteClause() const
{
    return IsSatellite(*m_pMainFeat);
}


bool CAutoDefFeatureClause::IsSatellite(const CSeq_feat& feat)
{
    if (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_repeat_region
		&& !NStr::IsBlank (feat.GetNamedQual("satellite"))) {
        return true;
    }
    return false;
}


bool CAutoDefFeatureClause::IsPromoter() const
{
    return IsPromoter(*m_pMainFeat);
}


bool CAutoDefFeatureClause::IsLTR() const
{
    return IsLTR(*m_pMainFeat);
}


bool CAutoDefFeatureClause::IsPromoter(const CSeq_feat& feat)
{
    if (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_promoter) {
        return true;
    } else if (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_regulatory &&
        NStr::Equal(feat.GetNamedQual("regulatory_class"), "promoter")) {
        return true;
    } else {
        return false;
    }
}


bool CAutoDefFeatureClause::IsLTR(const CSeq_feat& feat)
{
    if (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_LTR) {
        return true;
    } else if (feat.GetData().GetSubtype() != CSeqFeatData::eSubtype_repeat_region ||
        !feat.IsSetQual()) {
        return false;
    }
    ITERATE(CSeq_feat::TQual, it, feat.GetQual()) {
        if ((*it)->IsSetQual() && (*it)->IsSetVal() &&
            NStr::EqualNocase((*it)->GetQual(), "rpt_type") &&
            NStr::FindNoCase((*it)->GetVal(), "long_terminal_repeat") != string::npos) {
            return true;
        }
    }
    return false;
}

/* operons suppress all subfeatures except promoters (see GB-5635) */
void CAutoDefFeatureClause::x_GetOperonSubfeatures(string &interval)
{
    bool has_promoter = false;

    for (auto it : m_ClauseList) {
        if (it->IsPromoter()) {
            has_promoter = true;
            break;
        }
    }
    if (has_promoter) {
        interval += ", promoter region, ";
    }
}


/* This function calculates the "interval" for a clause in the definition
 * line.  The interval could be an empty string, it could indicate whether
 * the location of the feature is partial or complete and whether or not
 * the feature is a CDS, the interval could be a description of the
 * subfeatures of the clause, or the interval could be a combination of the
 * last two items if the feature is a CDS.
 */
bool CAutoDefFeatureClause::x_GetGenericInterval (string &interval, bool suppress_allele)
{    
    interval = "";
    if (m_IsUnknown) {
        return false;
    }
    
    CSeqFeatData::ESubtype subtype = GetMainFeatureSubtype();
    if (subtype == CSeqFeatData::eSubtype_exon && m_IsAltSpliced) {
        interval = "alternatively spliced";
        return true;
    }
    
    if (IsSatelliteClause() 
        || IsPromoter() 
        || subtype == CSeqFeatData::eSubtype_regulatory
        || subtype == CSeqFeatData::eSubtype_exon
        || subtype == CSeqFeatData::eSubtype_intron
        || subtype == CSeqFeatData::eSubtype_5UTR
        || subtype == CSeqFeatData::eSubtype_3UTR
        || (subtype == CSeqFeatData::eSubtype_repeat_region && !NStr::Equal(m_Typeword, "endogenous virus"))
        || subtype == CSeqFeatData::eSubtype_misc_recomb
        || IsLTR()) {
        return false;
    } 
    
    CRef<CAutoDefFeatureClause_Base> utr3;
    
    if (subtype == CSeqFeatData::eSubtype_operon) {
        // suppress subclauses except promoters
        x_GetOperonSubfeatures(interval);
    } else if (!m_SuppressSubfeatures) {
        // label subclauses
        // check to see if 3'UTR is present, and whether there are any other features
        auto it = m_ClauseList.begin();
        while (it != m_ClauseList.end()) {
            if (*it) {
                (*it)->Label(suppress_allele);
                if ((*it)->GetMainFeatureSubtype() == CSeqFeatData::eSubtype_3UTR && subtype == CSeqFeatData::eSubtype_cdregion) {
                    utr3 = *it;
                    it = m_ClauseList.erase(it);
                }
                else {
                    ++it;
                }
            } else {
                it = m_ClauseList.erase(it);
            }
        }
    
        // label any subclauses
        if (m_ClauseList.size() > 0) {
            bool suppress_final_and = false;
            if (subtype == CSeqFeatData::eSubtype_cdregion && !m_ClauseInfoOnly) {
                suppress_final_and = true;
            }
        
            // create subclause list for interval
            interval += ListClauses(false, suppress_final_and, suppress_allele);
            
            if (subtype == CSeqFeatData::eSubtype_cdregion && !m_ClauseInfoOnly) {
                if (utr3 != NULL) {
                    interval += ", ";
                } else if (m_ClauseList.size() == 1) {
                    interval += " and ";
                } else {
                    interval += ", and ";
                }
            } else {
                return true;
            }            
        }
    }
    
    if (IsPartial()) {
        interval += "partial ";
    } else {
        interval += "complete ";
    }
    
    if (subtype == CSeqFeatData::eSubtype_cdregion
        && (!x_IsPseudo())) {
        interval += "cds";
        if (m_IsAltSpliced) {
            interval += ", alternatively spliced";
        }
    } else {
        interval += "sequence";
        string product_name;
        if (m_IsAltSpliced && x_GetNoncodingProductFeatProduct (product_name)) {
            interval += ", alternatively spliced";
        }
    }

    if (utr3 != NULL) {
        /* tack UTR3 on at end of clause */
        if (m_ClauseList.size() == 0) {
            interval += " and 3' UTR";
        } else {
            interval += ", and 3' UTR";
        }
        m_ClauseList.push_back(utr3);
    } 
  
    return true;
} 


void CAutoDefFeatureClause::Label(bool suppress_allele)
{
    if (!m_TypewordChosen) {
        m_TypewordChosen = x_GetFeatureTypeWord(m_Typeword);
        m_ShowTypewordFirst = x_ShowTypewordFirst(m_Typeword);
        m_Pluralizable = true;
    }
    if (!m_ProductNameChosen) {
        m_ProductNameChosen = x_GetProductName(m_ProductName);
    }
    if (!m_DescriptionChosen) {
        m_DescriptionChosen = x_GetDescription(m_Description);
    }
    
    x_GetGenericInterval (m_Interval, suppress_allele);

}


sequence::ECompare CAutoDefFeatureClause::CompareLocation(const CSeq_loc& loc) const
{
    return sequence::Compare(loc, *m_ClauseLocation, &(m_BH.GetScope()),
        sequence::fCompareOverlapping);
}


bool CAutoDefFeatureClause::SameStrand(const CSeq_loc& loc) const
{
    ENa_strand loc_strand = loc.GetStrand();
    ENa_strand this_strand = m_ClauseLocation->GetStrand();
    
    if ((loc_strand == eNa_strand_minus && this_strand != eNa_strand_minus)
        || (loc_strand != eNa_strand_minus && this_strand == eNa_strand_minus)) {
        return false;
    } else {
        return true;
    }
    
}

bool CAutoDefFeatureClause::IsPartial() const
{
    if (m_ClauseLocation->IsPartialStart(eExtreme_Biological)
        || m_ClauseLocation->IsPartialStop(eExtreme_Biological)) {
        return true;
    } else {
        return false;
    }
}


CRef<CSeq_loc> CAutoDefFeatureClause::GetLocation() const
{
    return m_ClauseLocation;
}


void CAutoDefFeatureClause::AddToLocation(CRef<CSeq_loc> loc, bool also_set_partials)
{
    bool partial5 = m_ClauseLocation->IsPartialStart(eExtreme_Biological);
    bool partial3 = m_ClauseLocation->IsPartialStop(eExtreme_Biological);
    
    if (also_set_partials) {
        partial5 |= loc->IsPartialStart(eExtreme_Biological);
    }
    if (also_set_partials) {
        partial3 |= loc->IsPartialStop(eExtreme_Biological);
    }
    m_ClauseLocation = Seq_loc_Add(*m_ClauseLocation, *loc, 
                                   CSeq_loc::fSort | CSeq_loc::fMerge_Overlapping, 
                                   &(m_BH.GetScope()));

                                      
    m_ClauseLocation->SetPartialStart(partial5, eExtreme_Biological);
    m_ClauseLocation->SetPartialStop(partial3, eExtreme_Biological);
}


// Match for identical strings or for match at the beginning followed by mat-peptide region
bool CAutoDefFeatureClause::DoesmRNAProductNameMatch(const string& mrna_product) const
{
    if (!m_ProductNameChosen) {
        return false;
    }
    if (NStr::Equal(m_ProductName, mrna_product)) {
        return true;
    }
    if (NStr::StartsWith(m_ProductName, mrna_product) && m_ProductName[mrna_product.length()] == ',' && NStr::EndsWith(m_ProductName, " region,")) {
        return true;
    }
    return false;
}


/* This function searches this list for clauses to which this mRNA should
 * apply.  This is not taken care of by the GroupAllClauses function
 * because when an mRNA is added to a CDS, the product for the clause is
 * replaced and the location for the clause is expanded, rather than simply
 * adding the mRNA as an additional feature in the list, and because an 
 * mRNA can apply to more than one clause, while other features should 
 * really only belong to one clause.
 */
bool CAutoDefFeatureClause::AddmRNA (CAutoDefFeatureClause_Base *mRNAClause)
{
    bool used_mRNA = false;
    string clause_product, mRNA_product;
    bool adjust_partials = true;
    
    if (mRNAClause == NULL || ! mRNAClause->SameStrand(*m_ClauseLocation)) {
        return false;
    }
    
    CSeqFeatData::ESubtype subtype = m_pMainFeat->GetData().GetSubtype();
    sequence::ECompare loc_compare = mRNAClause->CompareLocation(*m_ClauseLocation);
    if (subtype == CSeqFeatData::eSubtype_cdregion) {
        adjust_partials = false;
    }
    
    if (subtype == CSeqFeatData::eSubtype_cdregion
        && DoesmRNAProductNameMatch(mRNAClause->GetProductName())
        && (loc_compare == sequence::eContained || loc_compare == sequence::eSame)) {
        m_HasmRNA = true;
        // when expanding "location" to include mRNA, leave partials for CDS as they were
        AddToLocation(mRNAClause->GetLocation(), adjust_partials);
        used_mRNA = true;
    } else if ((subtype == CSeqFeatData::eSubtype_cdregion || subtype == CSeqFeatData::eSubtype_gene)
               && !m_ProductNameChosen
               && (loc_compare == sequence::eContained
                   || loc_compare == sequence::eContains
                   || loc_compare == sequence::eSame)) {
        m_HasmRNA = true;
        AddToLocation(mRNAClause->GetLocation(), adjust_partials);        
        used_mRNA = true;
        m_ProductName = mRNAClause->GetProductName();
        m_ProductNameChosen = true;
    }       
    
    if (used_mRNA && mRNAClause->IsAltSpliced()) {
        m_IsAltSpliced = true;
    }
    
    return used_mRNA;
}


/* This function searches this list for clauses to which this gene should
 * apply.  This is not taken care of by the GroupAllClauses function
 * because genes are added to clauses as a GeneRefPtr instead of as an
 * additional feature in the list, and because a gene can apply to more
 * than one clause, while other features should really only belong to
 * one clause.
 */
bool CAutoDefFeatureClause::AddGene (CAutoDefFeatureClause_Base *gene_clause, bool suppress_allele) 
{
    bool used_gene = false;
    
    if (gene_clause == NULL || gene_clause->GetMainFeatureSubtype() != CSeqFeatData::eSubtype_gene) {
        return false;
    }
    
    CSeqFeatData::ESubtype subtype = GetMainFeatureSubtype ();
    
    string noncoding_product_name;
    
    // only add gene to certain other types of clauses
    if (subtype != CSeqFeatData::eSubtype_cdregion
        && subtype != CSeqFeatData::eSubtype_mRNA
        && subtype != CSeqFeatData::eSubtype_rRNA
        && subtype != CSeqFeatData::eSubtype_tRNA
        && subtype != CSeqFeatData::eSubtype_misc_RNA
        && subtype != CSeqFeatData::eSubtype_otherRNA
        && subtype != CSeqFeatData::eSubtype_ncRNA
        && subtype != CSeqFeatData::eSubtype_precursor_RNA
        && subtype != CSeqFeatData::eSubtype_preRNA
        && subtype != CSeqFeatData::eSubtype_tmRNA
        && subtype != CSeqFeatData::eSubtype_intron
        && subtype != CSeqFeatData::eSubtype_exon
        && !x_GetNoncodingProductFeatProduct(noncoding_product_name)) {
        return false;
    }

    if (m_HasGene) {
        // already assigned
    } else {
        // find overlapping gene for this feature    
        CAutoDefGeneClause *gene = dynamic_cast<CAutoDefGeneClause *>(gene_clause);
        bool suppress_locus_tag = gene ? gene->GetSuppressLocusTag() : false;
        CConstRef <CSeq_feat> gene_for_feat = sequence::GetGeneForFeature(*m_pMainFeat, m_BH.GetScope());
        if (gene_for_feat && NStr::Equal(x_GetGeneName(gene_for_feat->GetData().GetGene(), suppress_locus_tag), gene_clause->GetGeneName())) {
            used_gene = true;
            m_HasGene = true;
            m_GeneName = gene_clause->GetGeneName();
            m_AlleleName = gene_clause->GetAlleleName();
            m_GeneIsPseudo = gene_clause->GetGeneIsPseudo();
            m_TypewordChosen = x_GetFeatureTypeWord(m_Typeword);
        }
    }
    
    if (used_gene && ! m_ProductNameChosen) {
        Label(suppress_allele);
        if (!m_ProductNameChosen) {
            m_ProductNameChosen = true;
            m_ProductName = gene_clause->GetProductName();
        }
    }
    if (used_gene) {
        m_DescriptionChosen = false;
        Label(suppress_allele);
    }
    
    return used_gene;
}


bool CAutoDefFeatureClause::OkToGroupUnderByType(const CAutoDefFeatureClause_Base *parent_clause) const
{
    bool ok_to_group = false;
    
    if (parent_clause == NULL) {
        return false;
    }
    CSeqFeatData::ESubtype subtype = m_pMainFeat->GetData().GetSubtype();
    CSeqFeatData::ESubtype parent_subtype = parent_clause->GetMainFeatureSubtype();

    if (parent_subtype == CSeqFeatData::eSubtype_mobile_element) {
        return true;
    }
    
    if (subtype == CSeqFeatData::eSubtype_exon || subtype == CSeqFeatData::eSubtype_intron) {
        if (parent_subtype == CSeqFeatData::eSubtype_cdregion
            || parent_subtype == CSeqFeatData::eSubtype_D_loop
            || parent_subtype == CSeqFeatData::eSubtype_mRNA
            || parent_subtype == CSeqFeatData::eSubtype_gene
            || parent_subtype == CSeqFeatData::eSubtype_operon
            || parent_clause->IsNoncodingProductFeat()
            || parent_clause->IsEndogenousVirusSourceFeature()
            || parent_clause->IsGeneCluster()) {
            ok_to_group = true;
        }
    } else if (IsPromoter() || subtype == CSeqFeatData::eSubtype_regulatory) {
        if (parent_subtype == CSeqFeatData::eSubtype_cdregion
            || parent_subtype == CSeqFeatData::eSubtype_mRNA
            || parent_subtype == CSeqFeatData::eSubtype_gene
            || parent_subtype == CSeqFeatData::eSubtype_operon
            || parent_clause->IsEndogenousVirusSourceFeature()
            || parent_clause->IsGeneCluster()) {
            ok_to_group = true;
        }
    } else if (subtype == CSeqFeatData::eSubtype_cdregion) {
        if (parent_subtype == CSeqFeatData::eSubtype_mRNA
            || parent_clause->IsInsertionSequence()
            || parent_clause->IsMobileElement()
            || parent_clause->IsEndogenousVirusSourceFeature()
            || parent_subtype == CSeqFeatData::eSubtype_operon
            || parent_clause->IsGeneCluster()) {
            ok_to_group = true;
        }
    } else if (IsInsertionSequence()
               || subtype == CSeqFeatData::eSubtype_gene
               || IsMobileElement()
               || IsNoncodingProductFeat()
               || subtype == CSeqFeatData::eSubtype_operon
               || IsGeneCluster()) {
        if (parent_clause->IsMobileElement()
            || parent_clause->IsInsertionSequence()
            || parent_clause->IsEndogenousVirusSourceFeature()
            || parent_subtype == CSeqFeatData::eSubtype_operon
            || parent_clause->IsGeneCluster()) {
            ok_to_group = true;
        }
    } else if (subtype == CSeqFeatData::eSubtype_3UTR 
               || subtype == CSeqFeatData::eSubtype_5UTR
               || IsLTR(*m_pMainFeat)) {
        if (parent_subtype == CSeqFeatData::eSubtype_cdregion
            || parent_subtype == CSeqFeatData::eSubtype_mRNA
            || parent_subtype == CSeqFeatData::eSubtype_gene
            || parent_clause->IsEndogenousVirusSourceFeature()
            || parent_subtype == CSeqFeatData::eSubtype_operon
            || parent_clause->IsGeneCluster()) {
            ok_to_group = true;
        }
    }
         
    return ok_to_group;
}


// Transposons, insertion sequences, and endogenous virii
// take subfeatures regardless of whether the subfeature is
// on the same strand.
// Gene Clusters can optionally take subfeatures on either
// strand (gene_cluster_opp_strand is flag).
// Promoters will match up to features that are adjacent.
// Introns will match up to coding regions if the intron
// location is the space between two coding region intervals.
// Any feature on an mRNA sequence groups locationally.
// All other feature matches must be that the feature to
// go into the clause must fit inside the location of the
// other clause.
bool CAutoDefFeatureClause::OkToGroupUnderByLocation(const CAutoDefFeatureClause_Base *parent_clause, bool gene_cluster_opp_strand) const
{
    if (parent_clause == NULL) {
        return false;
    }

    if (m_HasGene && parent_clause->GetMainFeatureSubtype() == CSeqFeatData::eSubtype_gene) {
        // genes must match to be parents
        if (!NStr::Equal(m_GeneName, parent_clause->GetGeneName())) {
            return false;
        }
    }
    
    if (m_Biomol == CMolInfo::eBiomol_mRNA) {
        return true;
    }
    
    sequence::ECompare loc_compare = parent_clause->CompareLocation(*m_ClauseLocation);
    
    if (loc_compare == sequence::eContained || loc_compare == sequence::eSame) {
        if (parent_clause->SameStrand(*m_ClauseLocation)) {
            return true;
        } else if (parent_clause->IsMobileElement()
                   || parent_clause->IsInsertionSequence()
                   || parent_clause->IsEndogenousVirusSourceFeature()
                   || (parent_clause->IsGeneCluster() && gene_cluster_opp_strand)) {
            return true;
        }
    } else if (IsPromoter()
               && parent_clause->SameStrand(*m_ClauseLocation)) {
        unsigned int promoter_stop = sequence::GetStop(*m_ClauseLocation, &(m_BH.GetScope()), eExtreme_Biological);
        unsigned int parent_start = sequence::GetStart(*(parent_clause->GetLocation()), &(m_BH.GetScope()), eExtreme_Biological);
        if (m_ClauseLocation->GetStrand() == eNa_strand_minus) {
            if (promoter_stop == parent_start + 1) {
                return true;
            }
        } else if (promoter_stop + 1 == parent_start) {
            return true;
        }
    } else if (m_pMainFeat->GetData().GetSubtype() == CSeqFeatData::eSubtype_intron
               && parent_clause->GetMainFeatureSubtype() == CSeqFeatData::eSubtype_cdregion
               && parent_clause->SameStrand(*m_ClauseLocation)) {               
        CSeq_loc_CI seq_loc_it(*(parent_clause->GetLocation()));
        if (seq_loc_it) {
            int intron_start = sequence::GetStart(*m_ClauseLocation, &(m_BH.GetScope()), eExtreme_Biological);
            int intron_stop = sequence::GetStop (*m_ClauseLocation, &(m_BH.GetScope()), eExtreme_Biological);
            int prev_start = seq_loc_it.GetRange().GetFrom();
            int prev_stop = seq_loc_it.GetRange().GetTo();
            ++seq_loc_it;
            while (seq_loc_it) {
                int cds_start = seq_loc_it.GetRange().GetFrom();
                int cds_stop = seq_loc_it.GetRange().GetTo();
                if ((intron_start == prev_stop + 1 && intron_stop == cds_start - 1)
                    || (intron_start == cds_stop + 1 && intron_stop == prev_start - 1)) {
                    return true;
                }
                prev_start = cds_start;
                prev_stop = cds_stop;
                ++seq_loc_it;
            }
            // intron could also group with coding region if coding region is adjacent
            if (intron_start > prev_stop && intron_start - 1 == prev_stop) {
                return true;
            } else if (prev_start > intron_stop && prev_start - 1 == intron_stop) {
                return true;
            }
        }                              
    }
               
    return false;
}


CAutoDefFeatureClause_Base *CAutoDefFeatureClause::FindBestParentClause(CAutoDefFeatureClause_Base * subclause, bool gene_cluster_opp_strand)
{
    CAutoDefFeatureClause_Base *best_parent;
    
    if (subclause == NULL || subclause == this) {
        return NULL;
    }

	if (!NStr::IsBlank(subclause->GetGeneName()) &&
		!NStr::IsBlank(this->GetGeneName()) &&
		!NStr::Equal(subclause->GetGeneName(), this->GetGeneName())) {
		return NULL;
	}
    
    best_parent = CAutoDefFeatureClause_Base::FindBestParentClause(subclause, gene_cluster_opp_strand);
    
    if (subclause->OkToGroupUnderByLocation(this, gene_cluster_opp_strand)
        && subclause->OkToGroupUnderByType(this)) {
        if (best_parent == NULL || best_parent->CompareLocation(*m_ClauseLocation) == sequence::eContained) {
            best_parent = this;
        }
    }
    return best_parent;
}

void CAutoDefFeatureClause::ReverseCDSClauseLists()
{
    ENa_strand this_strand = m_ClauseLocation->GetStrand();
    if (this_strand == eNa_strand_minus 
        && GetMainFeatureSubtype() == CSeqFeatData::eSubtype_cdregion) {
        std::reverse(m_ClauseList.begin(), m_ClauseList.end());
    }
    
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        m_ClauseList[k]->ReverseCDSClauseLists();
    }
}



bool CAutoDefFeatureClause::ShouldRemoveExons() const
{
    unsigned int subtype = GetMainFeatureSubtype();
    
    if (subtype == CSeqFeatData::eSubtype_mRNA) {
        return false;
    } else if (subtype == CSeqFeatData::eSubtype_cdregion) {
        if (IsPartial()) {
            // keep only if exons have numbers
            for (size_t k = 0; k < m_ClauseList.size(); k++) {
                if (m_ClauseList[k]->IsExonWithNumber()) {
                    return false;
                }
            }
            return true;
        } else {
            return true;
        }
    } else {
        return true;
    }
}


bool CAutoDefFeatureClause::IsExonWithNumber() const
{
    if (m_pMainFeat->IsSetData() &&
        m_pMainFeat->GetData().GetSubtype() == CSeqFeatData::eSubtype_exon &&
        m_pMainFeat->IsSetQual()) {
        ITERATE(CSeq_feat::TQual, it, m_pMainFeat->GetQual()) {
            if ((*it)->IsSetQual() &&
                NStr::Equal((*it)->GetQual(), "number") &&
                (*it)->IsSetVal() &&
                !NStr::IsBlank((*it)->GetVal())) {
                return true;
            }
        }
    }
    return false;
}


bool CAutoDefFeatureClause::IsBioseqPrecursorRNA() const
{
    if (m_Biomol == CMolInfo::eBiomol_pre_RNA && GetMainFeatureSubtype() == CSeqFeatData::eSubtype_preRNA) {
        return true;
    } else {
        return false;
    }
}


CAutoDefNcRNAClause::CAutoDefNcRNAClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, const CAutoDefOptions& opts)
                      : CAutoDefFeatureClause(bh, main_feat, mapped_loc, opts),
					   m_UseComment (m_Opts.GetUseNcRNAComment())
{
}


CAutoDefNcRNAClause::~CAutoDefNcRNAClause()
{
}


bool CAutoDefNcRNAClause::x_GetProductName(string &product_name)
{
    string ncrna_product;
    string ncrna_class;
    if (m_pMainFeat->IsSetData() && m_pMainFeat->GetData().IsRna()
        && m_pMainFeat->GetData().GetRna().IsSetExt()) {
        const CRNA_ref::TExt& ext = m_pMainFeat->GetData().GetRna().GetExt();
        if (ext.IsName()) {
            ncrna_product = ext.GetName();
            if (NStr::EqualNocase(ncrna_product, "ncRNA")) {
                ncrna_product = "";
            }
        } else if (ext.IsGen()) {
            if (ext.GetGen().IsSetProduct()) {
                ncrna_product = ext.GetGen().GetProduct();
            }
            if (ext.GetGen().IsSetClass()) {
                ncrna_class = ext.GetGen().GetClass();
            }
        }
    }
    if (NStr::IsBlank(ncrna_product)) {
        ncrna_product = m_pMainFeat->GetNamedQual("product");
    }
    if (NStr::IsBlank(ncrna_class)) {
        ncrna_class = m_pMainFeat->GetNamedQual("ncRNA_class");
    }
    if (NStr::EqualNocase(ncrna_class, "other")) {
        ncrna_class = "";
    }
    NStr::ReplaceInPlace(ncrna_class, "_", " ");
    
	string ncrna_comment;
    if (m_pMainFeat->IsSetComment()) {
        ncrna_comment = m_pMainFeat->GetComment();
        if (!NStr::IsBlank(ncrna_comment)) {
            string::size_type pos = NStr::Find(ncrna_comment, ";");
            if (pos != NCBI_NS_STD::string::npos) {
                ncrna_comment = ncrna_comment.substr(0, pos);
            }
        }
    }

    if (!NStr::IsBlank (ncrna_product)) {
        product_name = ncrna_product;
        if (!NStr::IsBlank (ncrna_class)) {
            product_name += " " + ncrna_class;
        }
	} else if (!NStr::IsBlank(ncrna_class)) {
        product_name = ncrna_class;
	} else if (m_UseComment && !NStr::IsBlank (ncrna_comment)) {
		product_name = ncrna_comment;
    } else {
        product_name = "non-coding RNA";
    }
    return true;

}


static string mobile_element_keywords [] = {
  "insertion sequence",
  "retrotransposon",
  "non-LTR retrotransposon",
  "transposon",
  "P-element",
  "transposable element",
  "integron",
  "superintegron",
  "SINE",
  "MITE",
  "LINE"
};
  

CAutoDefMobileElementClause::CAutoDefMobileElementClause(CBioseq_Handle bh, const CSeq_feat& main_feat, const CSeq_loc& mapped_loc, const CAutoDefOptions& opts)
                  : CAutoDefFeatureClause(bh, main_feat, mapped_loc, opts)
{
    string mobile_element_name = m_pMainFeat->GetNamedQual("mobile_element_type");
    if (NStr::StartsWith(mobile_element_name, "other:")) {
        mobile_element_name = mobile_element_name.substr(6);
    }
    bool   found_keyword = false;

    m_Pluralizable = true;

    if (NStr::IsBlank(mobile_element_name)) {
        m_Description = "";
        m_ShowTypewordFirst = false;
        m_Typeword = "mobile element";
    } else {
        for (unsigned int k = 0; k < sizeof (mobile_element_keywords) / sizeof (string) && !found_keyword; k++) {
            size_t pos;
            if (NStr::StartsWith(mobile_element_name, mobile_element_keywords[k])) {
                // keyword at the beginning
                m_Typeword = mobile_element_keywords[k];
                if (NStr::Equal(mobile_element_name, mobile_element_keywords[k])) {
                    m_ShowTypewordFirst = false;
                    m_Description = "";
                } else {
                    m_ShowTypewordFirst = true;
                    m_Description = mobile_element_name.substr(mobile_element_keywords[k].length());
                    NStr::TruncateSpacesInPlace(m_Description);
                }
                if (mobile_element_name.c_str()[mobile_element_keywords[k].length()] == '-') {
                    // if keyword is hyphenated portion of name, no pluralization
                    m_Pluralizable = false;
                }
                found_keyword = true;
            } else if (NStr::EndsWith(mobile_element_name, mobile_element_keywords[k])) {
                // keyword at the end
                m_Typeword = mobile_element_keywords[k];
                m_ShowTypewordFirst = false;
                m_Description = mobile_element_name.substr(0, mobile_element_name.length() - mobile_element_keywords[k].length());
                NStr::TruncateSpacesInPlace(m_Description);
                found_keyword = true;
            } else if ((pos = NStr::Find(mobile_element_name, mobile_element_keywords[k])) != string::npos
                       && isspace(mobile_element_name.c_str()[pos])) {
                // keyword in the middle
                m_Typeword = "";
                m_ShowTypewordFirst = false;
                m_Description = mobile_element_name.substr(pos);
                m_Pluralizable = false;
            }
        }
        if (!found_keyword) {
            // keyword not in description
            m_Typeword = "mobile element";
            m_Description = mobile_element_name;
        }
    }
    if (NStr::EqualNocase(m_Typeword, "integron")) {
        m_ShowTypewordFirst = false;
    }

    m_DescriptionChosen = true;
    m_TypewordChosen = true;
    m_ProductName = "";
    m_ProductNameChosen = true;   
    NStr::TruncateSpacesInPlace(m_Description);
    if (NStr::StartsWith(m_Description, ":")) {
        m_Description = m_Description.substr(1);
        NStr::TruncateSpacesInPlace(m_Description);
    }
    if (NStr::Equal(m_Description, "unnamed")) {
        m_Description = "";
    }
}


CAutoDefMobileElementClause::~CAutoDefMobileElementClause()
{
}


void CAutoDefMobileElementClause::Label(bool suppress_allele)
{
    m_DescriptionChosen = true;
    x_GetGenericInterval (m_Interval, suppress_allele);
}


bool CAutoDefMobileElementClause::IsOptional()
{
    if (NStr::Equal(m_Typeword, "SINE") ||
        NStr::Equal(m_Typeword, "LINE") ||
        NStr::Equal(m_Typeword, "MITE")) {
        return true;
    } else {
        return false;
    }

}


const char *kMinisatellite = "minisatellite";
const char *kMicrosatellite = "microsatellite";
const char *kSatellite = "satellite";

CAutoDefSatelliteClause::CAutoDefSatelliteClause(CBioseq_Handle bh, const CSeq_feat& main_feat, const CSeq_loc &mapped_loc, const CAutoDefOptions& opts)
                  : CAutoDefFeatureClause(bh, main_feat, mapped_loc, opts)
{
	string comment = m_pMainFeat->GetNamedQual("satellite");
    string::size_type pos = NStr::Find(comment, ";");
    if (pos != NCBI_NS_STD::string::npos) {
        comment = comment.substr(0, pos);
    }

	size_t len = 0;
	
	if (NStr::StartsWith(comment, kMinisatellite)) {
		len = strlen (kMinisatellite);
	} else if (NStr::StartsWith (comment, kMicrosatellite)) {
		len = strlen (kMicrosatellite);
	} else if (NStr::StartsWith (comment, kSatellite)) {
		len = strlen (kSatellite);
    } else {
        // use default label satellite
        string prefix = kSatellite;
        comment = prefix + " " + comment;
    }
	if (len > 0 && NStr::Equal(comment.substr(len, 1), ":")) {
	    comment = comment.substr (0, len) + " " + comment.substr (len + 1);
	}
	
    m_Description = comment;
    m_DescriptionChosen = true;
    m_Typeword = "sequence";
    m_TypewordChosen = true;
}


CAutoDefSatelliteClause::~CAutoDefSatelliteClause()
{
}


void CAutoDefSatelliteClause::Label(bool suppress_allele)
{
    m_DescriptionChosen = true;
    x_GetGenericInterval(m_Interval, suppress_allele);
}


CAutoDefPromoterClause::CAutoDefPromoterClause(CBioseq_Handle bh, const CSeq_feat& main_feat, const CSeq_loc &mapped_loc, const CAutoDefOptions& opts)
                  : CAutoDefFeatureClause(bh, main_feat, mapped_loc, opts)
{
    m_Description = "";
    m_DescriptionChosen = true;
    m_Typeword = "promoter region";
    m_TypewordChosen = true;
    m_Interval = "";
}


CAutoDefPromoterClause::~CAutoDefPromoterClause()
{
}


void CAutoDefPromoterClause::Label(bool suppress_allele)
{
    m_DescriptionChosen = true;
}


/* This class produces the default definition line label for a misc_feature 
 * that has the word "intergenic spacer" in the comment.  If the comment starts
 * with the word "contains", "contains" is ignored.  If "intergenic spacer"
 * appears first in the comment (or first after the word "contains", the text
 * after the words "intergenic spacer" but before the first semicolon (if any)
 * appear after the words "intergenic spacer" in the definition line.  If there
 * are words after "contains" or at the beginning of the comment before the words
 * "intergenic spacer", this text will appear in the definition line before the words
 * "intergenic spacer".
 */

void CAutoDefIntergenicSpacerClause::InitWithString (string comment, bool suppress_allele)
{
    m_Typeword = "intergenic spacer";
    m_TypewordChosen = true;
    m_ShowTypewordFirst = false;
    m_Pluralizable = false;
    

    if (NStr::StartsWith(comment, "may contain ")) {
        m_Description = comment.substr(12);
        m_DescriptionChosen = true;
        m_Typeword = "";
        m_TypewordChosen = true;
        m_Interval = "region";
    } else {
        if (NStr::StartsWith(comment, "contains ")) {
            comment = comment.substr(9);
        }
    
        if (NStr::StartsWith(comment, "intergenic spacer")) {
            comment = comment.substr(17);
            if (NStr::IsBlank(comment)) {
                m_ShowTypewordFirst = false;
                m_Description = "";
                m_DescriptionChosen = true;
            } else {
                NStr::TruncateSpacesInPlace(comment);
                if (NStr::StartsWith(comment, "and ")) {
                    m_Description = "";
                    m_DescriptionChosen = true;
                    m_ShowTypewordFirst = false;
                } else {
                    m_Description = comment;
                    m_DescriptionChosen = true;
                    m_ShowTypewordFirst = true;
                }
            }
        } else {
            string::size_type pos = NStr::Find(comment, "intergenic spacer");
            if (pos != NCBI_NS_STD::string::npos) {
                m_Description = comment.substr(0, pos);
                NStr::TruncateSpacesInPlace(m_Description);
                m_DescriptionChosen = true;
                m_ShowTypewordFirst = false;
            }
        }
        x_GetGenericInterval(m_Interval, suppress_allele);        
    }
}


CAutoDefIntergenicSpacerClause::CAutoDefIntergenicSpacerClause(CBioseq_Handle bh, const CSeq_feat& main_feat, const CSeq_loc &mapped_loc, string comment, const CAutoDefOptions& opts)
                  : CAutoDefFeatureClause(bh, main_feat, mapped_loc, opts)
{
    InitWithString (comment, true);
}


CAutoDefIntergenicSpacerClause::CAutoDefIntergenicSpacerClause(CBioseq_Handle bh, const CSeq_feat& main_feat, const CSeq_loc &mapped_loc, const CAutoDefOptions& opts)
                  : CAutoDefFeatureClause(bh, main_feat, mapped_loc, opts)
{

    string comment;
    if (m_pMainFeat->IsSetComment()) {
        comment = m_pMainFeat->GetComment();
    }

    /* truncate at first semicolon */
    string::size_type pos = NStr::Find(comment, ";");
    if (pos != NCBI_NS_STD::string::npos) {
        comment = comment.substr(0, pos);
    }

    InitWithString (comment, true);
}


CAutoDefIntergenicSpacerClause::~CAutoDefIntergenicSpacerClause()
{
}


void CAutoDefIntergenicSpacerClause::Label(bool suppress_allele)
{
    m_DescriptionChosen = true;
}


CAutoDefParsedIntergenicSpacerClause::CAutoDefParsedIntergenicSpacerClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, 
                                                                           const string& description, bool is_first, bool is_last, const CAutoDefOptions& opts)
                                                                           : CAutoDefIntergenicSpacerClause(bh, main_feat, mapped_loc, opts)
{
    if (!NStr::IsBlank(description)) {
        m_Description = description;
        size_t pos = NStr::Find(m_Description, "intergenic spacer");
        if (pos != string::npos) {
            m_Description = m_Description.substr(0, pos);
            NStr::TruncateSpacesInPlace(m_Description);
        }
        m_DescriptionChosen = true;
    }
    m_Typeword = "intergenic spacer";
    m_TypewordChosen = true;

    // adjust partialness of location
    bool partial5 = m_ClauseLocation->IsPartialStart(eExtreme_Biological) && is_first;
    bool partial3 = m_ClauseLocation->IsPartialStop(eExtreme_Biological) && is_last;
    m_ClauseLocation->SetPartialStart(partial5, eExtreme_Biological);
    m_ClauseLocation->SetPartialStop(partial3, eExtreme_Biological);
    x_GetGenericInterval(m_Interval, true);    
    if (NStr::EndsWith(description, " region")) {
        MakeRegion();
    }
}


CAutoDefParsedIntergenicSpacerClause::~CAutoDefParsedIntergenicSpacerClause()
{
}


CAutoDefParsedtRNAClause::~CAutoDefParsedtRNAClause()
{
}


CAutoDefParsedClause::CAutoDefParsedClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, bool is_first, bool is_last, const CAutoDefOptions& opts)
                                       : CAutoDefFeatureClause (bh, main_feat, mapped_loc, opts)
{
    // adjust partialness of location
    bool partial5 = m_ClauseLocation->IsPartialStart(eExtreme_Biological) && is_first;
    bool partial3 = m_ClauseLocation->IsPartialStop(eExtreme_Biological) && is_last;
    m_ClauseLocation->SetPartialStart(partial5, eExtreme_Biological);
    m_ClauseLocation->SetPartialStop(partial3, eExtreme_Biological);
}

CAutoDefParsedClause::~CAutoDefParsedClause()
{
}

void CAutoDefParsedClause::SetMiscRNAWord(const string& phrase)
{
    ERnaMiscWord word_type = x_GetRnaMiscWordType(phrase);
    if (word_type == eMiscRnaWordType_InternalSpacer ||
        word_type == eMiscRnaWordType_ExternalSpacer ||
        word_type == eMiscRnaWordType_RNAIntergenicSpacer ||
        word_type == eMiscRnaWordType_IntergenicSpacer) {
        const string& item_name = x_GetRnaMiscWord(word_type);
        if (NStr::StartsWith(phrase, item_name)) {
            SetTypewordFirst(true);
            m_Description = phrase.substr(item_name.length());
        } else {
            SetTypewordFirst(false);
            m_Description = phrase.substr(0, NStr::Find(phrase, item_name));
        }
        if (NStr::EndsWith(phrase, " region") &&
            (!m_ShowTypewordFirst || m_Description != " region")) {
            SetTypeword(item_name + " region");
        } else {
            SetTypeword(item_name);
        }
    } else if (word_type == eMiscRnaWordType_RNA) {
        m_Description = phrase;
        if (NStr::EndsWith(m_Description, " gene")) {
            m_Description = m_Description.substr(0, m_Description.length() - 5);
        }
        SetTypeword("gene");
        SetTypewordFirst(false);
    } else if (word_type == eMiscRnaWordType_tRNA) {
        string gene_name;
        string product_name;
        if (CAutoDefParsedtRNAClause::ParseString(phrase, gene_name, product_name)) {
            m_TypewordChosen = true;
            m_GeneName = gene_name;
            if (!NStr::IsBlank(m_GeneName)) {
                m_HasGene = true;
            }
            m_ProductName = product_name;
            m_ProductNameChosen = true;
            x_GetDescription(m_Description);
        } else {
            m_Description = phrase;
        }
        SetTypeword("gene");
        SetTypewordFirst(false);
    }
    NStr::TruncateSpacesInPlace(m_Description);
    m_DescriptionChosen = true;
}


CAutoDefParsedtRNAClause::CAutoDefParsedtRNAClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, 
                                                   string gene_name, string product_name, 
                                                   bool is_first, bool is_last, const CAutoDefOptions& opts)
                                                   : CAutoDefParsedClause (bh, main_feat, mapped_loc, is_first, is_last, opts)
{
    m_Typeword = "gene";
    m_TypewordChosen = true;
    m_GeneName = gene_name;
    if (!NStr::IsBlank (m_GeneName)) {
        m_HasGene = true;
    }
    m_ProductName = product_name;
    m_ProductNameChosen = true;
}


CAutoDefGeneClusterClause::CAutoDefGeneClusterClause(CBioseq_Handle bh, const CSeq_feat& main_feat, const CSeq_loc &mapped_loc, const CAutoDefOptions& opts)
                  : CAutoDefFeatureClause(bh, main_feat, mapped_loc, opts)
{
    m_Pluralizable = false;
    m_ShowTypewordFirst = false;
    string comment = m_pMainFeat->GetComment();
    
    string::size_type pos = NStr::Find(comment, "gene cluster");
    if (pos == NCBI_NS_STD::string::npos) {
        pos = NStr::Find(comment, "gene locus");
        m_Typeword = "gene locus";
        m_TypewordChosen = true;
    } else {
        m_Typeword = "gene cluster";
        m_TypewordChosen = true;
    }
    
    if (pos != NCBI_NS_STD::string::npos) {
        comment = comment.substr(0, pos);
    }
    NStr::TruncateSpacesInPlace(comment);
    m_Description = comment;
    m_DescriptionChosen = true;
    m_SuppressSubfeatures = true;
}


CAutoDefGeneClusterClause::~CAutoDefGeneClusterClause()
{
}


void CAutoDefGeneClusterClause::Label(bool suppress_allele)
{
    x_GetGenericInterval(m_Interval, suppress_allele);
    m_DescriptionChosen = true;        
}


CAutoDefMiscCommentClause::CAutoDefMiscCommentClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, const CAutoDefOptions& opts)
                  : CAutoDefFeatureClause(bh, main_feat, mapped_loc, opts)
{
    if (m_pMainFeat->CanGetComment()) {
        m_Description = m_pMainFeat->GetComment();
        string::size_type pos = NStr::Find(m_Description, ";");
        if (pos != NCBI_NS_STD::string::npos) {
            m_Description = m_Description.substr(0, pos);
        }
        m_DescriptionChosen = true;
    }
    if (NStr::EndsWith(m_Description, " sequence")) {
        m_Description = m_Description.substr(0, m_Description.length() - 9);
        m_Typeword = "sequence";
        m_TypewordChosen = true;
    } else {
        x_TypewordFromSequence();
    }
    m_Interval = "";
}


CAutoDefMiscCommentClause::~CAutoDefMiscCommentClause()
{
}


void CAutoDefMiscCommentClause::Label(bool suppress_allele)
{
    m_DescriptionChosen = true;
}


CAutoDefParsedRegionClause::CAutoDefParsedRegionClause
(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, string product, const CAutoDefOptions& opts)
: CAutoDefFeatureClause(bh, main_feat, mapped_loc, opts)
{
    vector<string> elements = GetMiscRNAElements(product);
    if (elements.empty()) {
        m_Description = product;
    } else {        
        ITERATE(vector<string>, it, elements) {
            if (!NStr::IsBlank(m_Description)) {
                m_Description += ", ";
                if (*it == elements.back()) {
                    m_Description += "and ";
                }
            }
            m_Description += *it;
            if (NStr::Find(*it, "RNA") != string::npos && !NStr::EndsWith(*it, "gene") && !NStr::EndsWith(*it, "genes")) {
                m_Description += " gene";
            }
        }
    }
    m_DescriptionChosen = true;

    m_Typeword = "";
    m_TypewordChosen = true;
    m_Interval = "region";
}


CAutoDefParsedRegionClause::~CAutoDefParsedRegionClause()
{
}


void CAutoDefParsedRegionClause::Label(bool suppress_allele)
{
}

CAutoDefFakePromoterClause::CAutoDefFakePromoterClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, const CAutoDefOptions& opts)
                   : CAutoDefFeatureClause (bh, main_feat, mapped_loc, opts)
{
    m_Description = "";
    m_DescriptionChosen = true;
    m_Typeword = "promoter region";
    m_TypewordChosen = true;
    m_ShowTypewordFirst = false;
    m_Interval = "";
        
    
    m_ClauseLocation = new CSeq_loc();
    const CSeq_id* id = FindBestChoice(bh.GetBioseqCore()->GetId(), CSeq_id::BestRank);
    CRef <CSeq_id> new_id(new CSeq_id);
    new_id->Assign(*id);
    m_ClauseLocation->SetInt().SetId(*new_id);
    m_ClauseLocation->SetInt().SetFrom(0);
    m_ClauseLocation->SetInt().SetTo(bh.GetInst_Length() - 1);

}


CAutoDefFakePromoterClause::~CAutoDefFakePromoterClause()
{
}


void CAutoDefFakePromoterClause::Label(bool suppress_allele)
{
}


bool CAutoDefFakePromoterClause::OkToGroupUnderByLocation(const CAutoDefFeatureClause_Base *parent_clause, bool gene_cluster_opp_strand) const
{
    if (parent_clause == NULL) {
        return false;
    } else {
        return true;
    } 
}


bool CAutoDefFakePromoterClause::OkToGroupUnderByType(const CAutoDefFeatureClause_Base *parent_clause) const
{
    bool ok_to_group = false;
    
    if (parent_clause == NULL) {
        return false;
    }
    CSeqFeatData::ESubtype parent_subtype = parent_clause->GetMainFeatureSubtype();

    if (parent_subtype == CSeqFeatData::eSubtype_cdregion
        || parent_subtype == CSeqFeatData::eSubtype_mRNA
        || parent_subtype == CSeqFeatData::eSubtype_gene
        || parent_subtype == CSeqFeatData::eSubtype_operon
        || parent_clause->IsEndogenousVirusSourceFeature()
        || parent_clause->IsGeneCluster()) {
        ok_to_group = true;
    }

    return ok_to_group;
}


CAutoDefPromoterAnd5UTRClause::CAutoDefPromoterAnd5UTRClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, const CAutoDefOptions& opts)
    : CAutoDefFeatureClause(bh, main_feat, mapped_loc, opts)
{
    m_Description = "promoter region and 5' UTR";
    m_DescriptionChosen = true;
    m_Typeword = "";
    m_TypewordChosen = true;
    m_ShowTypewordFirst = false;
    m_Interval = "genomic sequence";


    m_ClauseLocation = new CSeq_loc();
    const CSeq_id* id = FindBestChoice(bh.GetBioseqCore()->GetId(), CSeq_id::BestRank);
    CRef <CSeq_id> new_id(new CSeq_id);
    new_id->Assign(*id);
    m_ClauseLocation->SetInt().SetId(*new_id);
    m_ClauseLocation->SetInt().SetFrom(0);
    m_ClauseLocation->SetInt().SetTo(bh.GetInst_Length() - 1);

}


bool CAutoDefPromoterAnd5UTRClause::IsPromoterAnd5UTR(const CSeq_feat& feat)
{
    return (feat.IsSetData() &&
        feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_misc_feature &&
        feat.IsSetComment() &&
        NStr::Equal(feat.GetComment(), "contains promoter and 5' UTR"));
}


void CAutoDefPromoterAnd5UTRClause::Label(bool suppress_allele)
{

}


CAutoDefFeatureClause::EClauseType CAutoDefFeatureClause::GetClauseType() const
{
    CSeqFeatData::ESubtype subtype = GetMainFeatureSubtype();
    if (subtype == CSeqFeatData::eSubtype_repeat_region) {
        if (!NStr::IsBlank(m_pMainFeat->GetNamedQual("endogenous_virus"))) {
            return eEndogenousVirusRepeatRegion;
        }
    }
    return eDefault;
}


// Some misc_RNA clauses have a comment that actually lists multiple
// features.  These functions create a clause for each element in the
// comment.

vector<CRef<CAutoDefFeatureClause > > AddMiscRNAFeatures(const CBioseq_Handle& bh, const CSeq_feat& cf, const CSeq_loc& mapped_loc, const CAutoDefOptions& opts)
{
    vector<CRef<CAutoDefFeatureClause > > rval;
    string comment;
    string::size_type pos;

    if (cf.GetData().Which() == CSeqFeatData::e_Rna) {
        comment = cf.GetNamedQual("product");
        if (NStr::IsBlank(comment)
            && cf.IsSetData()
            && cf.GetData().IsRna()
            && cf.GetData().GetRna().IsSetExt()) {
            if (cf.GetData().GetRna().GetExt().IsName()) {
                comment = cf.GetData().GetRna().GetExt().GetName();
            }
            else if (cf.GetData().GetRna().GetExt().IsGen()
                && cf.GetData().GetRna().GetExt().GetGen().IsSetProduct()) {
                comment = cf.GetData().GetRna().GetExt().GetGen().GetProduct();
            }
        }
    }

    if ((NStr::Equal(comment, "misc_RNA") || NStr::IsBlank(comment)) && cf.CanGetComment()) {
        comment = cf.GetComment();
    }
    if (NStr::IsBlank(comment)) {
        return rval;
    }

    pos = NStr::Find(comment, "spacer");
    if (pos == NPOS) {
        return rval;
    }

    bool is_region = false;

    NStr::TrimPrefixInPlace(comment, "contains ");
    if (NStr::StartsWith(comment, "may contain ")) {
        NStr::TrimPrefixInPlace(comment, "may contain ");
        is_region = true;
    }

    pos = NStr::Find(comment, ";");
    if (pos != string::npos) {
        comment = comment.substr(0, pos);
    }

    if (is_region) {
        rval.push_back(CRef<CAutoDefFeatureClause>(new CAutoDefParsedRegionClause(bh, cf, mapped_loc, comment, opts)));
    } else {
        vector<string> elements = CAutoDefFeatureClause::GetMiscRNAElements(comment);
        if (!elements.empty()) {
            for (auto s : elements) {
                CRef<CAutoDefParsedClause> new_clause(new CAutoDefParsedClause(bh, cf, mapped_loc,
                    (s == elements.front()), (s == elements.back()), opts));
                new_clause->SetMiscRNAWord(s);
                rval.push_back(new_clause);
            }
        } else {
            elements = CAutoDefFeatureClause::GetTrnaIntergenicSpacerClausePhrases(comment);
            if (!elements.empty()) {
                for (auto s : elements) {
                    size_t pos = NStr::Find(s, "intergenic spacer");
                    if (pos != string::npos) {
                        rval.push_back(CRef<CAutoDefFeatureClause>(new CAutoDefParsedIntergenicSpacerClause(bh,
                            cf,
                            mapped_loc,
                            (s),
                            (s == elements.front()),
                            (s == elements.back()), opts)));
                    } else {
                        rval.push_back(CRef<CAutoDefFeatureClause>(s_tRNAClauseFromNote(bh, cf, mapped_loc, s, (s == elements.front()), (s == elements.back()), opts)));
                    }
                }
            } else {
                rval.push_back(CRef<CAutoDefFeatureClause>(new CAutoDefParsedIntergenicSpacerClause(bh,
                    cf,
                    mapped_loc,
                    comment,
                    true,
                    true, 
                    opts)));
            }
        }
    }
    return rval;
}


vector<CRef<CAutoDefFeatureClause > > AddtRNAAndOther(const CBioseq_Handle& bh, const CSeq_feat& cf, const CSeq_loc& mapped_loc, const CAutoDefOptions& opts)
{
    vector<CRef<CAutoDefFeatureClause> > rval;
    if (cf.GetData().GetSubtype() != CSeqFeatData::eSubtype_misc_feature ||
        !cf.IsSetComment()) {
        return rval;
    }

    vector<string> phrases = CAutoDefFeatureClause_Base::GetFeatureClausePhrases(cf.GetComment());
    if (phrases.size() < 2) {
        return rval;
    }

    bool first = true;
    string last = phrases.back();
    phrases.pop_back();
    ITERATE(vector<string>, it, phrases) {
        rval.push_back(CRef<CAutoDefFeatureClause>(CAutoDefFeatureClause_Base::ClauseFromPhrase(*it, bh, cf, mapped_loc, first, false, opts)));
        first = false;
    }
    rval.push_back(CRef<CAutoDefFeatureClause>(CAutoDefFeatureClause_Base::ClauseFromPhrase(last, bh, cf, mapped_loc, first, true, opts)));

    return rval;
}


vector<CRef<CAutoDefFeatureClause > > FeatureClauseFactory(CBioseq_Handle bh, const CSeq_feat& cf, const CSeq_loc& mapped_loc, const CAutoDefOptions& opts, bool is_single_misc_feat)
{
    vector<CRef<CAutoDefFeatureClause> > rval;

    auto subtype = cf.GetData().GetSubtype();

    if (opts.IsFeatureSuppressed(subtype)) {
        return rval;
    }

    if (subtype == CSeqFeatData::eSubtype_gene) {
        rval.push_back(CRef<CAutoDefFeatureClause>(new CAutoDefGeneClause(bh, cf, mapped_loc, opts)));
    } else if (subtype == CSeqFeatData::eSubtype_ncRNA) {
        rval.push_back(CRef<CAutoDefFeatureClause>(new CAutoDefNcRNAClause(bh, cf, mapped_loc, opts)));
    } else if (subtype == CSeqFeatData::eSubtype_mobile_element) {
        rval.push_back(CRef<CAutoDefFeatureClause>(new CAutoDefMobileElementClause(bh, cf, mapped_loc, opts)));
    } else if (CAutoDefFeatureClause::IsSatellite(cf)) {
        rval.push_back(CRef<CAutoDefFeatureClause>(new CAutoDefSatelliteClause(bh, cf, mapped_loc, opts)));
    } else if (subtype == CSeqFeatData::eSubtype_otherRNA
        || subtype == CSeqFeatData::eSubtype_misc_RNA
        || subtype == CSeqFeatData::eSubtype_rRNA) {
        auto misc_rna = AddMiscRNAFeatures(bh, cf, mapped_loc, opts);
        if (misc_rna.empty()) {
            rval.push_back(CRef<CAutoDefFeatureClause>(new CAutoDefFeatureClause(bh, cf, mapped_loc, opts)));
        } else {
            for (auto it : misc_rna) {
                rval.push_back(it);
            }
        }
    } else if (CAutoDefFeatureClause::IsPromoter(cf)) {
        rval.push_back(CRef<CAutoDefFeatureClause>(new CAutoDefPromoterClause(bh, cf, mapped_loc, opts)));
    } else if (CAutoDefFeatureClause::IsGeneCluster(cf)) {
        rval.push_back(CRef<CAutoDefFeatureClause>(new CAutoDefGeneClusterClause(bh, cf, mapped_loc, opts)));
    } else if (CAutoDefFeatureClause::IsControlRegion(cf)) {
        rval.push_back(CRef<CAutoDefFeatureClause>(new CAutoDefFeatureClause(bh, cf, mapped_loc, opts)));
    }
    else if (subtype == CSeqFeatData::eSubtype_otherRNA) {
        auto misc_rna = AddMiscRNAFeatures(bh, cf, mapped_loc, opts);
        if (misc_rna.empty()) {
            // try to make trna clauses
            misc_rna = AddtRNAAndOther(bh, cf, mapped_loc, opts);
        }
        if (misc_rna.empty()) {
            rval.push_back(CRef<CAutoDefFeatureClause>(new CAutoDefFeatureClause(bh, cf, mapped_loc, opts)));
        }
        else {
            for (auto it : misc_rna) {
                rval.push_back(it);
            }
        }
    } else if (subtype == CSeqFeatData::eSubtype_misc_feature &&
        is_single_misc_feat && CAutoDefPromoterAnd5UTRClause::IsPromoterAnd5UTR(cf)) {
        rval.push_back(CRef<CAutoDefFeatureClause>(new CAutoDefPromoterAnd5UTRClause(bh, cf, mapped_loc, opts)));
    } else if (subtype == CSeqFeatData::eSubtype_misc_feature) {
        auto misc_rna = AddMiscRNAFeatures(bh, cf, mapped_loc, opts);
        if (misc_rna.empty()) {
            // try to make trna clauses
            misc_rna = AddtRNAAndOther(bh, cf, mapped_loc, opts);
        }
        if (misc_rna.empty()) {
            // some misc-features may require more parsing
            CRef<CAutoDefFeatureClause> new_clause(new CAutoDefFeatureClause(bh, cf, mapped_loc, opts));
            if (!is_single_misc_feat &&
                (opts.GetMiscFeatRule() == CAutoDefOptions::eDelete
                    || (opts.GetMiscFeatRule() == CAutoDefOptions::eNoncodingProductFeat && !new_clause->IsNoncodingProductFeat()))) {
                // do not create a clause at all
                new_clause.Reset(NULL);
            } else if (opts.GetMiscFeatRule() == CAutoDefOptions::eCommentFeat) {
                new_clause.Reset(NULL);
                if (cf.CanGetComment() && !NStr::IsBlank(cf.GetComment())) {
                    misc_rna.push_back(CRef<CAutoDefFeatureClause>(new CAutoDefMiscCommentClause(bh, cf, mapped_loc, opts)));
                }
            } else {
                misc_rna.push_back(new_clause);
            }
        }
        if (!misc_rna.empty()) {
            for (auto it : misc_rna) {
                rval.push_back(it);
            }
        }

    }  else {
        rval.push_back(CRef<CAutoDefFeatureClause>(new CAutoDefFeatureClause(bh, cf, mapped_loc, opts)));
    }
    return rval;
}


END_SCOPE(objects)
END_NCBI_SCOPE
