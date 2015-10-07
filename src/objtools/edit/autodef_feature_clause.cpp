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
#include <objtools/edit/autodef.hpp>
#include <objtools/edit/gene_utils.hpp>
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

CAutoDefFeatureClause::CAutoDefFeatureClause(CBioseq_Handle bh, const CSeq_feat& main_feat, const CSeq_loc& mapped_loc) 
                              : m_MainFeat(main_feat),
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

    CSeqFeatData::ESubtype subtype = m_MainFeat.GetData().GetSubtype();

    m_ClauseLocation = new CSeq_loc();
    m_ClauseLocation->Add(mapped_loc);

    if (subtype == CSeqFeatData::eSubtype_operon || IsGeneCluster()) {
        m_SuppressSubfeatures = true;
    }

    if (m_MainFeat.CanGetComment() && NStr::Find(m_MainFeat.GetComment(), "alternatively spliced") != NCBI_NS_STD::string::npos
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
    return m_MainFeat.GetData().GetSubtype();
}


bool CAutoDefFeatureClause::IsMobileElement()
{
    if (m_MainFeat.GetData().GetSubtype() != CSeqFeatData::eSubtype_mobile_element) {
        return false;
    } else {
        return true;
    }
}


bool CAutoDefFeatureClause::IsInsertionSequence()
{
    if (m_MainFeat.GetData().GetSubtype() != CSeqFeatData::eSubtype_repeat_region
        || NStr::IsBlank(m_MainFeat.GetNamedQual("insertion_seq"))) {
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


bool CAutoDefFeatureClause::IsControlRegion()
{
    return IsControlRegion(m_MainFeat);
}


bool CAutoDefFeatureClause::IsEndogenousVirusSourceFeature ()
{
    if (m_MainFeat.GetData().GetSubtype() != CSeqFeatData::eSubtype_biosrc
        || !m_MainFeat.GetData().GetBiosrc().CanGetSubtype()) {
        return false;
    }
    ITERATE (CBioSource::TSubtype, subSrcI, m_MainFeat.GetData().GetBiosrc().GetSubtype()) {
        if ((*subSrcI)->GetSubtype() == CSubSource::eSubtype_endogenous_virus_name) {
             return true;
        }
    }
    return false;
}


bool CAutoDefFeatureClause::IsGeneCluster ()
{
    return IsGeneCluster (m_MainFeat);
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


bool CAutoDefFeatureClause::IsRecognizedFeature()
{
    CSeqFeatData::ESubtype subtype = m_MainFeat.GetData().GetSubtype();
    if (subtype == CSeqFeatData::eSubtype_3UTR
        || subtype == CSeqFeatData::eSubtype_5UTR
        || subtype == CSeqFeatData::eSubtype_LTR
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
        || subtype == CSeqFeatData::eSubtype_D_loop
        || IsNoncodingProductFeat()
        || IsMobileElement()
        || IsInsertionSequence()
        || IsControlRegion()
        || IsIntergenicSpacer()
        || IsEndogenousVirusSourceFeature()
        || IsSatelliteClause()
        || IsPromoter()
        || IsGeneCluster()) {
        return true;
    } else {
        return false;
    }
}


void CAutoDefFeatureClause::x_SetBiomol()
{
    CSeqdesc_CI desc_iter(m_BH, CSeqdesc::e_Molinfo);
    for ( ;  desc_iter;  ++desc_iter) {
        if (desc_iter->GetMolinfo().IsSetBiomol()) {
            m_Biomol = desc_iter->GetMolinfo().GetBiomol();
        }
    }
}


bool CAutoDefFeatureClause::x_IsPseudo()
{
    if (m_MainFeat.CanGetPseudo() && m_MainFeat.IsSetPseudo()) {
        return true;
    } else {
        return false;
    }
}


bool CAutoDefFeatureClause::x_GetFeatureTypeWord(string &typeword)
{
    string qual, comment;
  
    CSeqFeatData::ESubtype subtype = m_MainFeat.GetData().GetSubtype();
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
        case CSeqFeatData::eSubtype_LTR:
            typeword = "LTR";
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
            qual = m_MainFeat.GetNamedQual("endogenous_virus");
            if (!NStr::IsBlank(qual)) {
                typeword = "endogenous virus";
                return true;
            }
            if (IsMobileElement()) {
                typeword = "transposon";
                return true;
            }
            break;
        case CSeqFeatData::eSubtype_misc_feature:
            if (m_MainFeat.CanGetComment()) {
                comment = m_MainFeat.GetComment();
                if (NStr::StartsWith(comment, "control region", NStr::eNocase)) {
                    typeword = "control region";
                    return true;
                }
            }
            break;
        case CSeqFeatData::eSubtype_biosrc:
            if (IsEndogenousVirusSourceFeature()) {
                typeword = "endogenous virus";
                return true;
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
            typeword = "pseudogene precursor mRNA";
        } else {
            typeword = "precursor mRNA";
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


bool CAutoDefFeatureClause::x_FindNoncodingFeatureKeywordProduct (string comment, string keyword, string &product_name)
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


bool CAutoDefFeatureClause::x_GetNoncodingProductFeatProduct (string &product_name)
{
    if (GetMainFeatureSubtype() != CSeqFeatData::eSubtype_misc_feature
        || !m_MainFeat.CanGetComment()) {
        return false;
    }
    string comment = m_MainFeat.GetComment();
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

bool CAutoDefFeatureClause::IsNoncodingProductFeat() 
{
    string product_name;
    return x_GetNoncodingProductFeatProduct(product_name);
}

CAutoDefGeneClause::CAutoDefGeneClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, bool suppress_locus_tag)
    : CAutoDefFeatureClause(bh, main_feat, mapped_loc)
{
    m_SuppressLocusTag = suppress_locus_tag;
    m_GeneName = x_GetGeneName(m_MainFeat.GetData().GetGene(), suppress_locus_tag);
    if (m_MainFeat.GetData().GetGene().CanGetAllele()) {
        m_AlleleName = m_MainFeat.GetData().GetGene().GetAllele();
        if (!NStr::StartsWith(m_AlleleName, m_GeneName, NStr::eNocase)) {
            if (!NStr::StartsWith(m_AlleleName, "-")) {
                m_AlleleName = "-" + m_AlleleName;
            }
            m_AlleleName = m_GeneName + m_AlleleName;
        }
    }
    if (m_MainFeat.CanGetPseudo() && m_MainFeat.GetPseudo()) {
        m_GeneIsPseudo = true;
    }
    m_HasGene = true;
}


bool CAutoDefGeneClause::x_IsPseudo()
{
    if (CAutoDefFeatureClause::x_IsPseudo()) {
        return true;
    }
    const CGene_ref& gene = m_MainFeat.GetData().GetGene();
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
    if (m_MainFeat.GetData().GetGene().CanGetDesc()
        && !NStr::Equal(m_MainFeat.GetData().GetGene().GetDesc(),
        m_GeneName)) {
        product_name = m_MainFeat.GetData().GetGene().GetDesc();
        return true;
    } else {
        return false;
    }
}


string s_tRNAGeneFromProduct (string product)
{
    string gene = "";

    if (!NStr::StartsWith (product, "tRNA-")) {
        return "";
    }
    product = product.substr (5);

    if (NStr::EqualNocase (product, "Ala")) {
        gene = "trnA";
    } else if (NStr::EqualNocase (product, "Asx")) {
        gene = "trnB";
    } else if (NStr::EqualNocase (product, "Cys")) {
        gene = "trnC";
    } else if (NStr::EqualNocase (product, "Asp")) {
        gene = "trnD";
    } else if (NStr::EqualNocase (product, "Glu")) {
        gene = "trnE";
    } else if (NStr::EqualNocase (product, "Phe")) {
        gene = "trnF";
    } else if (NStr::EqualNocase (product, "Gly")) {
        gene = "trnG";
    } else if (NStr::EqualNocase (product, "His")) {
        gene = "trnH";
    } else if (NStr::EqualNocase (product, "Ile")) {
        gene = "trnI";
    } else if (NStr::EqualNocase (product, "Xle")) {
        gene = "trnJ";
    } else if (NStr::EqualNocase (product, "Lys")) {
        gene = "trnK";
    } else if (NStr::EqualNocase (product, "Leu")) {
        gene = "trnL";
    } else if (NStr::EqualNocase (product, "Met")) {
        gene = "trnM";
    } else if (NStr::EqualNocase (product, "Asn")) {
        gene = "trnN";
    } else if (NStr::EqualNocase (product, "Pyl")) {
        gene = "trnO";
    } else if (NStr::EqualNocase (product, "Pro")) {
        gene = "trnP";
    } else if (NStr::EqualNocase (product, "Gln")) {
        gene = "trnQ";
    } else if (NStr::EqualNocase (product, "Arg")) {
        gene = "trnR";
    } else if (NStr::EqualNocase (product, "Ser")) {
        gene = "trnS";
    } else if (NStr::EqualNocase (product, "Thr")) {
        gene = "trnT";
    } else if (NStr::EqualNocase (product, "Sec")) {
        gene = "trnU";
    } else if (NStr::EqualNocase (product, "Val")) {
        gene = "trnV";
    } else if (NStr::EqualNocase (product, "Trp")) {
        gene = "trnW";
    } else if (NStr::EqualNocase (product, "OTHER")) {
        gene = "trnX";
    } else if (NStr::EqualNocase (product, "Tyr")) {
        gene = "trnY";
    } else if (NStr::EqualNocase (product, "Glx")) {
        gene = "trnZ";
    }
    return gene;
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


CAutoDefParsedtRNAClause *s_tRNAClauseFromNote(CBioseq_Handle bh, const CSeq_feat& cf, const CSeq_loc& mapped_loc, string comment, bool is_first, bool is_last) 
{
    string product_name = "";
    string gene_name = "";
    if (!CAutoDefParsedtRNAClause::ParseString(comment, gene_name, product_name))  {
        return NULL;
    }
         
    return new CAutoDefParsedtRNAClause(bh, cf, mapped_loc, gene_name, product_name, is_first, is_last);
}        


const int kParsedTrnaGene = 1;
const int kParsedTrnaSpacer = 2;

vector<string> GetIntergenicSpacerClausePhrases(string comment)
{
    vector<string> clause_list;

    clause_list.clear();

    if (NStr::StartsWith (comment, "contains ")) {
        comment = comment.substr (9);
    }
    if (NStr::StartsWith (comment, "may contain ")) {
        comment = comment.substr (12);
    }
    string::size_type pos = NStr::Find(comment, ";");
    if (pos != NCBI_NS_STD::string::npos) {
        comment = comment.substr(0, pos);
    }

    pos = NStr::Find (comment, " and ");
    // insert comma for parsing
    if (pos != NCBI_NS_STD::string::npos && pos > 0 && !NStr::StartsWith (comment.substr(pos - 1), ",")) {
        comment = comment.substr(0, pos) + "," + comment.substr(pos);
    }

    vector<string> parts;
    NStr::Tokenize(comment, ",", parts, NStr::eMergeDelims );
    if ( parts.empty() ) {
        return clause_list;
    }

    if (parts.size() == 1 && NStr::Find (parts[0], "intergenic spacer") != NCBI_NS_STD::string::npos) {
        clause_list.push_back (parts[0]);
        return clause_list;
    }
    
    bool bad_phrase = false;
    bool names_correct = true;
    int last_type = 0;

    string last_gene_name = "";
    string last_product = "";

    for (size_t j = 0; j < parts.size() && names_correct && !bad_phrase; j++) {
        NStr::TruncateSpacesInPlace(parts[j]);
        if (NStr::StartsWith (parts[j], "and ")) {
            parts[j] = parts[j].substr(4);
        }
        if (NStr::EndsWith (parts[j], " intergenic spacer")) {
            // must be a spacer
            string spacer_description = parts[j].substr(0, parts[j].length() - 18);
            clause_list.push_back(parts[j]);
            if (last_type == kParsedTrnaGene) {
                // spacer names and gene names must agree
                if (NStr::IsBlank (last_gene_name)) {
                    last_gene_name = s_tRNAGeneFromProduct (last_product);
                }
                if (!NStr::StartsWith (spacer_description, last_gene_name + "-")) {
                    names_correct = false;
                }
            }
            last_type = kParsedTrnaSpacer;
        } else {
            string gene_name = "";
            string product_name = "";
            if (!CAutoDefParsedtRNAClause::ParseString(parts[j], gene_name, product_name)) {
                bad_phrase = true;
            } else {
                // must alternate between genes and spacers
                if (last_type == kParsedTrnaSpacer) {
                    // spacer names and gene names must agree
                    if (NStr::IsBlank (gene_name)) {
                        gene_name = s_tRNAGeneFromProduct (product_name);
                    }
                    if (!NStr::EndsWith (parts[j - 1], "-" + gene_name + " intergenic spacer")) {
                        names_correct = false;
                    }
                }                
                clause_list.push_back (parts[j]);
                last_type = kParsedTrnaGene;
                last_gene_name = gene_name;
                last_product = product_name;
            }
        }
    }
    if (bad_phrase || !names_correct) {
        clause_list.clear();
    }
    return clause_list;
}


vector<CAutoDefFeatureClause *> GetIntergenicSpacerClauseList (string comment, CBioseq_Handle bh, const CSeq_feat& cf, const CSeq_loc& mapped_loc)
{
    vector<CAutoDefFeatureClause *> clause_list;

    clause_list.clear();

    vector<string> parts = GetIntergenicSpacerClausePhrases(comment);
    if (parts.empty()) {
        return clause_list;
    }

    for (size_t j = 0; j < parts.size(); j++) {
        size_t pos = NStr::Find(parts[j], "intergenic spacer");
        if (pos != string::npos) {
            string spacer_description = parts[j].substr(0, pos);
            NStr::TruncateSpacesInPlace(spacer_description);
            CAutoDefParsedIntergenicSpacerClause *spacer =
                  new CAutoDefParsedIntergenicSpacerClause(bh, 
                                                           cf,
                                                           mapped_loc,
                                                           spacer_description, 
                                                           j == 0, 
                                                           j == parts.size() - 1);
            if (NStr::EndsWith(parts[j], "region")) {
                // change interval to region
                spacer->MakeRegion();
            }
            clause_list.push_back((CAutoDefFeatureClause*)spacer);
        } else {
            CAutoDefFeatureClause *gene = s_tRNAClauseFromNote(bh, cf, mapped_loc, parts[j], j == 0, j == parts.size() - 1);
            clause_list.push_back(gene);
        }
    }

    return clause_list;
}


bool CAutoDefFeatureClause::IsIntergenicSpacer (const CSeq_feat& feat)
{
    CSeqFeatData::ESubtype subtype = feat.GetData().GetSubtype();
    if ((subtype != CSeqFeatData::eSubtype_misc_feature && subtype != CSeqFeatData::eSubtype_otherRNA)
        || !feat.IsSetComment()) {
        return false;
    }
    string comment = feat.GetComment();

    vector<string> parts = GetIntergenicSpacerClausePhrases(comment);
    if (parts.size() > 0) {
        return true;
    } else {
        return false;
    }
}


bool CAutoDefFeatureClause::IsIntergenicSpacer()
{
    CSeqFeatData::ESubtype subtype = m_MainFeat.GetData().GetSubtype();
    if ((subtype != CSeqFeatData::eSubtype_misc_feature && subtype != CSeqFeatData::eSubtype_otherRNA)
        || !m_MainFeat.CanGetComment()) {
        return false;
    }
    string comment = m_MainFeat.GetComment();

    if (NStr::StartsWith (comment, "contains ")) {
        comment = comment.substr (9);
    }
    if (NStr::StartsWith (comment, "may contain ")) {
        comment = comment.substr (12);
    }
    string::size_type pos = NStr::Find(comment, ";");
    if (pos != NCBI_NS_STD::string::npos) {
        comment = comment.substr(0, pos);
    }


    vector<CAutoDefFeatureClause *> clause_list = GetIntergenicSpacerClauseList (comment, m_BH, m_MainFeat, m_MainFeat.GetLocation());

    if (clause_list.size() == 0) {
        return false;
    } else {
        for (size_t i = 0; i < clause_list.size(); i++) {
            delete (clause_list[i]);
        }
        return true;
    }
}


string CAutoDefFeatureClause::x_GetGeneName(const CGene_ref& gref, bool suppress_locus_tag) const
{
    if (gref.IsSuppressed()) {
        return "";
    } else if (gref.CanGetLocus() && !NStr::IsBlank(gref.GetLocus())) {
        return gref.GetLocus();
    } else if (!suppress_locus_tag && gref.IsSetLocus_tag() && !NStr::IsBlank(gref.GetLocus_tag())) {
        return gref.GetLocus_tag();
    } else {
        return "";
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
    CSeqFeatData::ESubtype subtype = m_MainFeat.GetData().GetSubtype();
    
    if (subtype == CSeqFeatData::eSubtype_misc_feature && x_GetNoncodingProductFeatProduct(product_name)) {
        return true;
    } else if (subtype == CSeqFeatData::eSubtype_cdregion 
               && m_MainFeat.CanGetPseudo() 
               && m_MainFeat.IsSetPseudo()
               && m_MainFeat.CanGetComment()) {
        string comment = m_MainFeat.GetComment();
        if (!NStr::IsBlank(comment)) {
            string::size_type pos = NStr::Find(comment, ";");
            if (pos != NCBI_NS_STD::string::npos) {
                comment = comment.substr(0, pos);
            }
            product_name = comment;
            return true;
        }
    } else if (m_MainFeat.GetData().Which() == CSeqFeatData::e_Rna) {
        if (subtype == CSeqFeatData::eSubtype_tRNA) {
            string label;
            
            feature::GetLabel(m_MainFeat, &label, feature::fFGL_Content);
            if (NStr::Equal(label, "tRNA-Xxx")) {
                label = "tRNA-OTHER";
            } else if (!NStr::StartsWith(label, "tRNA-")) {
                label = "tRNA-" + label;
            }
            product_name = label;
            return true;
        } else {
            product_name = m_MainFeat.GetNamedQual("product");
            if (NStr::IsBlank(product_name)) {
                if (subtype == CSeqFeatData::eSubtype_otherRNA 
                    || subtype == CSeqFeatData::eSubtype_misc_RNA) {
                    if (m_MainFeat.CanGetComment()) {
                        product_name = m_MainFeat.GetComment();
                        if (!NStr::IsBlank(product_name)) {
                            string::size_type pos = NStr::Find(product_name, ";");
                            if (pos != NCBI_NS_STD::string::npos) {
                                product_name = product_name.substr(0, pos);
                            }
                        }
                    }
                } else if (m_MainFeat.GetData().GetRna().IsSetExt()
                           && m_MainFeat.GetData().GetRna().GetExt().Which() == CRNA_ref::C_Ext::e_Name) {
                    product_name = m_MainFeat.GetData().GetRna().GetExt().GetName();
                }
            }
            return true;
        }
    } else {
        string label;
        
        if (m_MainFeat.CanGetProduct()) {
            CConstRef<CSeq_feat> prot = GetBestOverlappingFeat(m_MainFeat.GetProduct(),
                                                               CSeqFeatData::e_Prot,
                                                               eOverlap_Simple,
                                                               m_BH.GetScope());
            if (prot) {
                feature::GetLabel(*prot, &label, feature::fFGL_Content);
            }
        }
        
        if (NStr::IsBlank(label)) {                    
            feature::GetLabel(m_MainFeat, &label, feature::fFGL_Content);
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
            string::size_type pos = NStr::Find(label, ";");
            if (pos != NCBI_NS_STD::string::npos) {
                label = label.substr(0, pos);
            }
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
    CSeqFeatData::ESubtype subtype = m_MainFeat.GetData().GetSubtype();
    
    if (m_MainFeat.IsSetQual()) {
        ITERATE(CSeq_feat::TQual, it, m_MainFeat.GetQual()) {
            if ((*it)->IsSetQual() && (*it)->IsSetVal()
                && NStr::EqualNocase((*it)->GetQual(), "number")) {
                description = (*it)->GetVal();
                return true;
            }
        }
    }

    string label;
    feature::GetLabel(m_MainFeat, &label, feature::fFGL_Content);

    if ((subtype == CSeqFeatData::eSubtype_exon && NStr::Equal(label, "exon"))
        || (subtype == CSeqFeatData::eSubtype_intron && NStr::Equal(label, "[intron]"))
        || (subtype != CSeqFeatData::eSubtype_exon && subtype != CSeqFeatData::eSubtype_intron)) {
        description = "";
        return false;
    } else {
        description = label;
        return true;
    }
}


bool CAutoDefFeatureClause::x_GetDescription(string &description)
{
    CSeqFeatData::ESubtype subtype = m_MainFeat.GetData().GetSubtype();

    description = "";
    if (subtype == CSeqFeatData::eSubtype_exon || subtype == CSeqFeatData::eSubtype_intron) {
        return x_GetExonDescription(description);
    } else if (NStr::Equal(m_Typeword, "insertion sequence")) {
        description = m_MainFeat.GetNamedQual("insertion_seq");
        if (NStr::Equal(description, "unnamed")
            || NStr::IsBlank(description)) {
            description = "";
            return false;
        } else {
            return true;
        }
    } else if (subtype == CSeqFeatData::eSubtype_repeat_region
               && NStr::Equal(m_Typeword, "endogenous virus")) {
        description = m_MainFeat.GetNamedQual("endogenous_virus");
        if (NStr::Equal(description, "unnamed")
            || NStr::IsBlank(description)) {
            description = "";
            return false;
        } else {
            return true;
        }
    } else if (subtype == CSeqFeatData::eSubtype_biosrc
               && NStr::Equal(m_Typeword, "endogenous virus")) {
        if (m_MainFeat.GetData().GetBiosrc().CanGetSubtype()) {
            ITERATE (CBioSource::TSubtype, subSrcI, m_MainFeat.GetData().GetBiosrc().GetSubtype()) {
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
    } else if (subtype == CSeqFeatData::eSubtype_LTR) {
        if (m_MainFeat.CanGetComment()) {
            string comment = m_MainFeat.GetComment();
            if (NStr::StartsWith(comment,"LTR ")) {
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


bool CAutoDefFeatureClause::IsSatelliteClause() 
{
    return IsSatellite(m_MainFeat);
}


bool CAutoDefFeatureClause::IsSatellite(const CSeq_feat& feat)
{
    if (feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_repeat_region
		&& !NStr::IsBlank (feat.GetNamedQual("satellite"))) {
        return true;
    }
    return false;
}


bool CAutoDefFeatureClause::IsPromoter()
{
    return IsPromoter(m_MainFeat);
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


/* This function calculates the "interval" for a clause in the definition
 * line.  The interval could be an empty string, it could indicate whether
 * the location of the feature is partial or complete and whether or not
 * the feature is a CDS, the interval could be a description of the
 * subfeatures of the clause, or the interval could be a combination of the
 * last two items if the feature is a CDS.
 */
bool CAutoDefFeatureClause::x_GetGenericInterval (string &interval, bool suppress_allele)
{
    unsigned int k;
    
    interval = "";
    if (m_IsUnknown) {
        return false;
    }
    
    CSeqFeatData::ESubtype subtype = m_MainFeat.GetData().GetSubtype();
    if (subtype == CSeqFeatData::eSubtype_exon && m_IsAltSpliced) {
        interval = "alternatively spliced";
        return true;
    }
    
    if (IsSatelliteClause() 
        || IsPromoter() 
        || subtype == CSeqFeatData::eSubtype_exon
        || subtype == CSeqFeatData::eSubtype_intron
        || subtype == CSeqFeatData::eSubtype_5UTR
        || subtype == CSeqFeatData::eSubtype_3UTR) {
        return false;
    } 
    
    CAutoDefFeatureClause_Base *utr3 = NULL;
    
    if (!m_SuppressSubfeatures) {
        // label subclauses
        // check to see if 3'UTR is present, and whether there are any other features
        for (k = 0; k < m_ClauseList.size(); k++) {
            m_ClauseList[k]->Label(suppress_allele);
            if (m_ClauseList[k]->GetMainFeatureSubtype() == CSeqFeatData::eSubtype_3UTR && subtype == CSeqFeatData::eSubtype_cdregion) {
                utr3 = m_ClauseList[k];
                for (unsigned int j = k + 1; j < m_ClauseList.size(); j++) {
                    m_ClauseList[j-1] = m_ClauseList[j];
                }
                m_ClauseList[m_ClauseList.size() - 1] = NULL;
                m_ClauseList.pop_back();
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
        && (!m_MainFeat.CanGetPseudo() || !m_MainFeat.GetPseudo())) {
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


sequence::ECompare CAutoDefFeatureClause::CompareLocation(const CSeq_loc& loc)
{
    return sequence::Compare(loc, *m_ClauseLocation, &(m_BH.GetScope()),
        sequence::fCompareOverlapping);
}


bool CAutoDefFeatureClause::SameStrand(const CSeq_loc& loc)
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

bool CAutoDefFeatureClause::IsPartial()
{
    if (m_ClauseLocation->IsPartialStart(eExtreme_Biological)
        || m_ClauseLocation->IsPartialStop(eExtreme_Biological)) {
        return true;
    } else {
        return false;
    }
}


CRef<CSeq_loc> CAutoDefFeatureClause::GetLocation()
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
    
    CSeqFeatData::ESubtype subtype = m_MainFeat.GetData().GetSubtype();
    sequence::ECompare loc_compare = mRNAClause->CompareLocation(*m_ClauseLocation);
    if (subtype == CSeqFeatData::eSubtype_cdregion) {
        adjust_partials = false;
    }
    
    if (subtype == CSeqFeatData::eSubtype_cdregion
        && m_ProductNameChosen
        && NStr::Equal(m_ProductName, mRNAClause->GetProductName())
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
    
    sequence::ECompare loc_compare = gene_clause->CompareLocation(*m_ClauseLocation);
    

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
        CConstRef <CSeq_feat> gene_for_feat = edit::GetGeneForFeature(m_MainFeat, m_BH.GetScope());
        if (gene_for_feat && NStr::Equal(x_GetGeneName(gene_for_feat->GetData().GetGene(), suppress_locus_tag), gene_clause->GetGeneName())) {
            used_gene = true;
            m_HasGene = true;
            m_GeneName = gene_clause->GetGeneName();
            m_AlleleName = gene_clause->GetAlleleName();
            m_GeneIsPseudo = gene_clause->GetGeneIsPseudo();
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


bool CAutoDefFeatureClause::OkToGroupUnderByType(CAutoDefFeatureClause_Base *parent_clause)
{
    bool ok_to_group = false;
    
    if (parent_clause == NULL) {
        return false;
    }
    CSeqFeatData::ESubtype subtype = m_MainFeat.GetData().GetSubtype();
    CSeqFeatData::ESubtype parent_subtype = parent_clause->GetMainFeatureSubtype();
    
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
    } else if (IsPromoter()) {
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
               || IsGeneCluster()
               || IsIntergenicSpacer()) {
        if (parent_clause->IsMobileElement()
            || parent_clause->IsInsertionSequence()
            || parent_clause->IsEndogenousVirusSourceFeature()
            || parent_subtype == CSeqFeatData::eSubtype_operon
            || parent_clause->IsGeneCluster()) {
            ok_to_group = true;
        }
    } else if (subtype == CSeqFeatData::eSubtype_3UTR 
               || subtype == CSeqFeatData::eSubtype_5UTR
               || subtype == CSeqFeatData::eSubtype_LTR) {
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
bool CAutoDefFeatureClause::OkToGroupUnderByLocation(CAutoDefFeatureClause_Base *parent_clause, bool gene_cluster_opp_strand)
{
    if (parent_clause == NULL) {
        return false;
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
    } else if (m_MainFeat.GetData().GetSubtype() == CSeqFeatData::eSubtype_intron
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
        TClauseList tmp;
        tmp.clear();
        for (unsigned int k = m_ClauseList.size(); k > 0; k--) {
            tmp.push_back(m_ClauseList[k - 1]);
            m_ClauseList[k - 1] = NULL;
        }
        m_ClauseList.clear();
        for (unsigned int k = 0; k < tmp.size(); k++) {
            m_ClauseList.push_back(tmp[k]);
            tmp[k] = NULL;
        }
        tmp.clear();
    }
    
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        m_ClauseList[k]->ReverseCDSClauseLists();
    }
}


bool CAutoDefFeatureClause::ShouldRemoveExons()
{
    unsigned int subtype = GetMainFeatureSubtype();
    
    if (subtype == CSeqFeatData::eSubtype_mRNA) {
        return false;
    } else if (subtype == CSeqFeatData::eSubtype_cdregion) {
        return ! IsPartial();
    } else {
        return true;
    }
}

bool CAutoDefFeatureClause::IsBioseqPrecursorRNA()
{
    if (m_Biomol == CMolInfo::eBiomol_pre_RNA && GetMainFeatureSubtype() == CSeqFeatData::eSubtype_preRNA) {
        return true;
    } else {
        return false;
    }
}


CAutoDefNcRNAClause::CAutoDefNcRNAClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, bool use_comment)
                      : CAutoDefFeatureClause(bh, main_feat, mapped_loc),
					   m_UseComment (use_comment)
{
}


CAutoDefNcRNAClause::~CAutoDefNcRNAClause()
{
}


bool CAutoDefNcRNAClause::x_GetProductName(string &product_name)
{
    string ncrna_product = "";
    string ncrna_class = "";
    if (m_MainFeat.IsSetData() && m_MainFeat.GetData().IsRna()
        && m_MainFeat.GetData().GetRna().IsSetExt()) {
        const CRNA_ref::TExt& ext = m_MainFeat.GetData().GetRna().GetExt();
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
        ncrna_product = m_MainFeat.GetNamedQual("product");
    }
    if (NStr::IsBlank(ncrna_class)) {
        ncrna_class = m_MainFeat.GetNamedQual("ncRNA_class");
    }
    if (NStr::EqualNocase(ncrna_class, "other")) {
        ncrna_class = "";
    }
    NStr::ReplaceInPlace(ncrna_class, "_", " ");
    
	string ncrna_comment;
    if (m_MainFeat.IsSetComment()) {
        ncrna_comment = m_MainFeat.GetComment();
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
  

CAutoDefMobileElementClause::CAutoDefMobileElementClause(CBioseq_Handle bh, const CSeq_feat& main_feat, const CSeq_loc& mapped_loc)
                  : CAutoDefFeatureClause(bh, main_feat, mapped_loc)
{
    string mobile_element_name = m_MainFeat.GetNamedQual("mobile_element_type");
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

CAutoDefSatelliteClause::CAutoDefSatelliteClause(CBioseq_Handle bh, const CSeq_feat& main_feat, const CSeq_loc &mapped_loc)
                  : CAutoDefFeatureClause(bh, main_feat, mapped_loc)
{
	string comment = m_MainFeat.GetNamedQual("satellite");
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


CAutoDefPromoterClause::CAutoDefPromoterClause(CBioseq_Handle bh, const CSeq_feat& main_feat, const CSeq_loc &mapped_loc)
                  : CAutoDefFeatureClause(bh, main_feat, mapped_loc)
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


CAutoDefIntergenicSpacerClause::CAutoDefIntergenicSpacerClause(CBioseq_Handle bh, const CSeq_feat& main_feat, const CSeq_loc &mapped_loc, string comment)
                  : CAutoDefFeatureClause(bh, main_feat, mapped_loc)
{
    InitWithString (comment, true);
}


CAutoDefIntergenicSpacerClause::CAutoDefIntergenicSpacerClause(CBioseq_Handle bh, const CSeq_feat& main_feat, const CSeq_loc &mapped_loc)
                  : CAutoDefFeatureClause(bh, main_feat, mapped_loc)
{

    string comment = m_MainFeat.GetComment();

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
                                                                           string description, bool is_first, bool is_last)
                                                                           : CAutoDefIntergenicSpacerClause(bh, main_feat, mapped_loc)
{
    if (!NStr::IsBlank(description)) {
        m_Description = description;
    }

    // adjust partialness of location
    bool partial5 = m_ClauseLocation->IsPartialStart(eExtreme_Biological) && is_first;
    bool partial3 = m_ClauseLocation->IsPartialStop(eExtreme_Biological) && is_last;
    m_ClauseLocation->SetPartialStart(partial5, eExtreme_Biological);
    m_ClauseLocation->SetPartialStop(partial3, eExtreme_Biological);
    x_GetGenericInterval(m_Interval, true);        
}


CAutoDefParsedIntergenicSpacerClause::~CAutoDefParsedIntergenicSpacerClause()
{
}


CAutoDefParsedtRNAClause::~CAutoDefParsedtRNAClause()
{
}


CAutoDefParsedClause::CAutoDefParsedClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, bool is_first, bool is_last)
                                       : CAutoDefFeatureClause (bh, main_feat, mapped_loc)
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


CAutoDefParsedtRNAClause::CAutoDefParsedtRNAClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, 
                                                   string gene_name, string product_name, 
                                                   bool is_first, bool is_last)
                                                   : CAutoDefParsedClause (bh, main_feat, mapped_loc, is_first, is_last)
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


CAutoDefGeneClusterClause::CAutoDefGeneClusterClause(CBioseq_Handle bh, const CSeq_feat& main_feat, const CSeq_loc &mapped_loc)
                  : CAutoDefFeatureClause(bh, main_feat, mapped_loc)
{
    m_Pluralizable = false;
    m_ShowTypewordFirst = false;
    string comment = m_MainFeat.GetComment();
    
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


CAutoDefMiscCommentClause::CAutoDefMiscCommentClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc)
                  : CAutoDefFeatureClause(bh, main_feat, mapped_loc)
{
    if (m_MainFeat.CanGetComment()) {
        m_Description = m_MainFeat.GetComment();
        string::size_type pos = NStr::Find(m_Description, ";");
        if (pos != NCBI_NS_STD::string::npos) {
            m_Description = m_Description.substr(0, pos);
        }
        m_DescriptionChosen = true;
    }
    if (m_Biomol == CMolInfo::eBiomol_genomic) {
        m_Typeword = "genomic sequence";
    } else if (m_Biomol == CMolInfo::eBiomol_mRNA) {
        m_Typeword = "mRNA sequence";
    } else {
        m_Typeword = "sequence";
    }
    m_TypewordChosen = true;
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
(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc, string product)
                       : CAutoDefFeatureClause(bh, main_feat, mapped_loc)
{
    m_Description = product;
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

CAutoDefFakePromoterClause::CAutoDefFakePromoterClause(CBioseq_Handle bh, const CSeq_feat &main_feat, const CSeq_loc &mapped_loc)
                   : CAutoDefFeatureClause (bh, main_feat, mapped_loc)
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


bool CAutoDefFakePromoterClause::OkToGroupUnderByLocation(CAutoDefFeatureClause_Base *parent_clause, bool gene_cluster_opp_strand)
{
    if (parent_clause == NULL) {
        return false;
    } else {
        return true;
    } 
}


bool CAutoDefFakePromoterClause::OkToGroupUnderByType(CAutoDefFeatureClause_Base *parent_clause)
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


END_SCOPE(objects)
END_NCBI_SCOPE
