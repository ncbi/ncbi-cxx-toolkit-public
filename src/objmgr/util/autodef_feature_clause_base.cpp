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
#include <corelib/ncbimisc.hpp>
#include <objmgr/util/autodef.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Bioseq.hpp>

#include <serial/iterator.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

using namespace sequence;

CAutoDefFeatureClause_Base::CAutoDefFeatureClause_Base() :
                              m_MakePlural(false),
                              m_SuppressLocusTag(false),
                              m_GeneIsPseudo(false),
                              m_IsUnknown(false),
                              m_ClauseInfoOnly(false),
                              m_IsAltSpliced(false),
                              m_HasmRNA(false),
                              m_HasGene(false),
                              m_DeleteMe(false)
{
}


CAutoDefFeatureClause_Base::CAutoDefFeatureClause_Base(bool suppress_locus_tag) :
                              m_MakePlural(false),
                              m_SuppressLocusTag(suppress_locus_tag),
                              m_GeneIsPseudo(false),
                              m_IsUnknown(false),
                              m_ClauseInfoOnly(false),
                              m_IsAltSpliced(false),
                              m_HasmRNA(false),
                              m_HasGene(false),
                              m_DeleteMe(false)
{
}


CAutoDefFeatureClause_Base::~CAutoDefFeatureClause_Base()
{
}


void CAutoDefFeatureClause_Base::AddSubclause (CAutoDefFeatureClause_Base *subclause)
{
    if (subclause) {
        m_ClauseList.push_back(subclause);
    }
}


void CAutoDefFeatureClause_Base::TransferSubclauses(TClauseList &other_clause_list)
{
    if (m_ClauseList.size() == 0) {
        return;
    }
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        other_clause_list.push_back(m_ClauseList[k]);
        m_ClauseList[k] = NULL;
    }
    m_ClauseList.clear();
}


void CAutoDefFeatureClause_Base::SetAltSpliced(string splice_name)
{
    m_IsAltSpliced = true;
    m_ProductNameChosen = true;
    m_ProductName = splice_name;
    m_DescriptionChosen = false;
}


string CAutoDefFeatureClause_Base::PrintClause(bool print_typeword, bool typeword_is_plural)
{
    bool   print_comma_between_description_and_typeword = false;
    string clause_text;

    /* we need to place a comma between the description and the type word 
     * when the description ends with "precursor" or when the type word
     * starts with "precursor"
     */
    if (!NStr::IsBlank(m_Description)
        && ! m_ShowTypewordFirst
        && print_typeword
        && ! NStr::IsBlank(m_Typeword)
        && ((NStr::StartsWith(m_Typeword, "precursor") && !NStr::EndsWith(m_Description, ")"))
            || (NStr::EndsWith(m_Description, "precursor")))) {
        print_comma_between_description_and_typeword = true;
    }
   
    // print typeword first, if needed and shown first
    if (m_ShowTypewordFirst && print_typeword
        && !NStr::IsBlank(m_Typeword)) {
        clause_text += m_Typeword;
        if (typeword_is_plural) {
            clause_text += "s";
        }
        // put space between typeword and description
        if (!NStr::IsBlank(m_Description)) {
            clause_text += " ";
        }
    }
    
    if (!NStr::IsBlank(m_Description)) {
        clause_text += m_Description;
        if (print_comma_between_description_and_typeword) {
            clause_text += ",";
        }
    }
    
    if (!m_ShowTypewordFirst && print_typeword
        && ! NStr::IsBlank(m_Typeword)) {
        if (!NStr::IsBlank(m_Description)) {
            clause_text += " ";
        }
        clause_text += m_Typeword;
        if (typeword_is_plural) {
            clause_text += "s";
        }
        // add allele if necessary
        if (DisplayAlleleName()) {
            clause_text += ", " + m_AlleleName + " allele";
        }
    }
    return clause_text;
}


bool CAutoDefFeatureClause_Base::DisplayAlleleName ()
{
    if (NStr::IsBlank(m_AlleleName)) {
        return false;
    } else if (NStr::Equal(m_Typeword, "gene")
        || NStr::Equal(m_Typeword, "pseudogene")
        || NStr::Equal(m_Typeword, "mRNA")
        || NStr::Equal(m_Typeword, "pseudogene mRNA")
        || NStr::Equal(m_Typeword, "precursor RNA")
        || NStr::Equal(m_Typeword, "pseudogene precursor RNA"))
    {
        return true;
    } else {
        return false;
    }
}


bool CAutoDefFeatureClause_Base::IsGeneMentioned(CAutoDefFeatureClause_Base *gene_clause)
{
    if (gene_clause == NULL || gene_clause->GetMainFeatureSubtype() != CSeqFeatData::eSubtype_gene) {
        return false;
    }
    
    if (NStr::Equal(gene_clause->GetGeneName(), m_GeneName) && NStr::Equal(gene_clause->GetAlleleName(), m_AlleleName)) {
        return true;
    }
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        if (m_ClauseList[k]->IsGeneMentioned(gene_clause)) {
            return true;
        }
    }    
    return false;   
}


bool CAutoDefFeatureClause_Base::AddmRNA (CAutoDefFeatureClause_Base *mRNAClause)
{
    bool retval = false;
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        retval |= m_ClauseList[k]->AddmRNA(mRNAClause);
    }
    return retval;
}
 
 
bool CAutoDefFeatureClause_Base::AddGene (CAutoDefFeatureClause_Base *gene_clause)
{
    bool retval = false;
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        retval |= m_ClauseList[k]->AddGene(gene_clause);
    }
    return retval;
}


void CAutoDefFeatureClause_Base::Label()
{
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        m_ClauseList[k]->Label();
    }
}

// Grouping functions
void CAutoDefFeatureClause_Base::GroupmRNAs()
{
    unsigned int k, j;
    // Add mRNAs to other clauses
    for (k = 0; k < m_ClauseList.size(); k++) {
        if (m_ClauseList[k]->IsMarkedForDeletion()
            || m_ClauseList[k]->GetMainFeatureSubtype() != CSeqFeatData::eSubtype_mRNA) {
            continue;
        }
        m_ClauseList[k]->Label();
        bool mRNA_used = false;
        for (j = 0; j < m_ClauseList.size(); j++) {
            if (m_ClauseList[j]->IsMarkedForDeletion()
                || j == k
                || m_ClauseList[j]->GetMainFeatureSubtype() != CSeqFeatData::eSubtype_cdregion) {
                continue;
            } else {
                mRNA_used |= m_ClauseList[j]->AddmRNA (m_ClauseList[k]);
            }
        }
        if (mRNA_used) {
            m_ClauseList[k]->MarkForDeletion();
        }
    }
}


void CAutoDefFeatureClause_Base::GroupGenes()
{
    unsigned int k, j;
    
    if (m_ClauseList.size() < 2) {
        return;
    }
    for (k = 0; k < m_ClauseList.size(); k++) {
        if (m_ClauseList[k]->GetMainFeatureSubtype() != CSeqFeatData::eSubtype_gene) {
            continue;
        }
        bool used_gene = false;
        for (j = k + 1; j < m_ClauseList.size(); j++) {
            used_gene |= m_ClauseList[j]->AddGene(m_ClauseList[k]);
        }
    }
}


void CAutoDefFeatureClause_Base::RemoveGenesMentionedElsewhere()
{
    unsigned int k, j;
    
    for (k = 0; k < m_ClauseList.size(); k++) {
        if (m_ClauseList[k]->GetMainFeatureSubtype() != CSeqFeatData::eSubtype_gene) {
            continue;
        }
        for (j = 0; j < m_ClauseList.size() && !m_ClauseList[k]->IsMarkedForDeletion(); j++) {
            if (j != k && !m_ClauseList[j]->IsMarkedForDeletion() 
                && m_ClauseList[j]->IsGeneMentioned(m_ClauseList[k])) {
                m_ClauseList[k]->MarkForDeletion();
            }
        }
    }
}


string CAutoDefFeatureClause_Base::ListClauses(bool allow_semicolons, bool suppress_final_and)
{
    if (m_ClauseList.size() < 1) {
        return "";
    }
    
    bool oneafter_has_detail_change, oneafter_has_interval_change, oneafter_has_typeword_change;
    bool onebefore_has_interval_change, onebefore_has_detail_change, onebefore_has_typeword_change;
    bool is_last, is_second_to_last;
    bool two_more_in_list;
    
    unsigned int last_interval_change_before_end = x_LastIntervalChangeBeforeEnd();
    
    string full_clause_list;
    
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        oneafter_has_detail_change = false;
        oneafter_has_interval_change = false;
        oneafter_has_typeword_change = false;
        onebefore_has_detail_change = false;
        onebefore_has_interval_change = false;
        onebefore_has_typeword_change = false;
        if (k == m_ClauseList.size() - 1) {
            is_last = true;
            is_second_to_last = false;
            two_more_in_list = false;
        } else {
            is_last = false;
            if (m_ClauseList.size() > 1 && k == m_ClauseList.size() - 2) {
                is_second_to_last = true;
            } else {
                is_second_to_last = false;
            }
            
            if (m_ClauseList.size() > 2 && k < m_ClauseList.size() - 2) {
                two_more_in_list = true;
            } else {
                two_more_in_list = false;
            }
        }
        
        string this_typeword = m_ClauseList[k]->GetTypeword();
        
        if (k > 0)
        {
            if (!NStr::Equal(m_ClauseList[k-1]->GetInterval(), m_ClauseList[k]->GetInterval())) {
                onebefore_has_interval_change = true;
            }
            if (!NStr::Equal(m_ClauseList[k-1]->GetTypeword(), m_ClauseList[k]->GetTypeword())){
                onebefore_has_typeword_change = true;
            }
            if (onebefore_has_typeword_change || onebefore_has_interval_change
                || m_ClauseList[k-1]->DisplayAlleleName() && m_ClauseList[k]->DisplayAlleleName()) {
                onebefore_has_detail_change = true;  
            }
        }
        if (!is_last) {
            if (!NStr::Equal(m_ClauseList[k]->GetInterval(), m_ClauseList[k + 1]->GetInterval())) {
                oneafter_has_interval_change = true;
            }
            if (!NStr::Equal(m_ClauseList[k]->GetTypeword(), m_ClauseList[k + 1]->GetTypeword())) {
                oneafter_has_typeword_change = true;
            }
            if (oneafter_has_typeword_change  || oneafter_has_interval_change
                || m_ClauseList[k+1]->DisplayAlleleName() && m_ClauseList[k]->DisplayAlleleName()) {
                oneafter_has_detail_change = true;
            }
        }
 
        bool print_typeword = false;
        bool typeword_is_plural = false;
        bool print_and = false;
        bool print_comma = false;
        bool print_semicolon = false;

        // determine whether to print typeword with this clause, and whether to pluralize it
        if (m_ClauseList[k]->IsTypewordFirst()) {
            if (k == 0 || onebefore_has_detail_change) {
                print_typeword = true;
                if (!is_last && !oneafter_has_detail_change) {
                    typeword_is_plural = true;
                } else if (NStr::Find(m_ClauseList[k]->GetDescription(), " through ") != NCBI_NS_STD::string::npos
                           && NStr::Equal(m_ClauseList[k]->GetTypeword(), "exon")) {
                    typeword_is_plural = true;
                }
            }
        } else {
            if (is_last || oneafter_has_detail_change) {
                print_typeword = true;
                if (k > 0 && ! onebefore_has_detail_change) {
                    typeword_is_plural = true;
                }
            }
        }
        
        // determine whether to print "and" before this clause
        if (k > 0) { // never print "and" before the first clause in the list!
            if (!onebefore_has_detail_change && (is_last || oneafter_has_detail_change)) {
                print_and = true;
            } else if (is_last) {
                print_and = true;
            } else if (!onebefore_has_interval_change && oneafter_has_interval_change) {
                print_and = true;
            } else if (k == last_interval_change_before_end) {
                print_and = true;
            }
            if (suppress_final_and && !is_last) {
                print_and = false;
            } 
        }
        if (suppress_final_and && is_second_to_last) {
          print_comma = true;
        }
 
        // determine when to print semicolon after this clause
        // after every interval change except when exons change "interval" 
        // exons changing interval are going from alt-spliced to not or vice versa, in either case we don't want a semicolon or comma
        if (!is_last && oneafter_has_interval_change
            && (!NStr::Equal(m_ClauseList[k]->GetTypeword(), "exon") || !NStr::Equal(m_ClauseList[k + 1]->GetTypeword(), "exon"))) {
            print_semicolon = true;
        }

        // determine when to print a comma after this section
        if (k > 0 && !is_last
            && ! onebefore_has_detail_change
            && ! oneafter_has_detail_change ) {
            print_comma = true;
        } else if (k > 0 && !is_last
                   && ! onebefore_has_interval_change && ! oneafter_has_interval_change
                   &&  onebefore_has_typeword_change &&  oneafter_has_typeword_change) {
            print_comma = true;
        } else if (two_more_in_list
                   && ! oneafter_has_detail_change
                   && NStr::Equal(m_ClauseList[k]->GetTypeword(), m_ClauseList[k + 2]->GetTypeword())
                   && NStr::Equal(m_ClauseList[k]->GetInterval(), m_ClauseList[k + 2]->GetInterval())) {
            print_comma = true;
        } else if (two_more_in_list
                   && oneafter_has_typeword_change
                   && NStr::Equal(m_ClauseList[k + 1]->GetTypeword(), m_ClauseList[k + 2]->GetTypeword())
                   && NStr::Equal(m_ClauseList[k + 1]->GetInterval(), m_ClauseList[k + 2]->GetInterval())
                   && ! print_and) {
            print_comma = true;
        } else if (((oneafter_has_interval_change || is_last)
                    && !NStr::IsBlank(m_ClauseList[k]->GetInterval()))
                   || (oneafter_has_interval_change && !is_last && ! print_semicolon)) {
            print_comma = true;
        } else if (two_more_in_list
                   && !oneafter_has_interval_change
                   && NStr::Equal(m_ClauseList[k]->GetInterval(), m_ClauseList[k + 2]->GetInterval())
                   && oneafter_has_typeword_change
                   && NStr::Equal(m_ClauseList[k]->GetTypeword(), m_ClauseList[k + 2]->GetTypeword())) {
            print_comma = true;
        } else if (k > 0 && two_more_in_list
                   && ! oneafter_has_interval_change && ! onebefore_has_interval_change
                   && NStr::Equal(m_ClauseList[k]->GetInterval(), m_ClauseList[k+2]->GetInterval())
                   && oneafter_has_typeword_change) {
            print_comma = true;
        } else if (k > 0 && two_more_in_list
                   && oneafter_has_typeword_change
                   && !NStr::Equal(m_ClauseList[k+1]->GetTypeword(), m_ClauseList[k+2]->GetTypeword())
                   && ! oneafter_has_interval_change
                   && NStr::Equal(m_ClauseList[k+1]->GetInterval(), m_ClauseList[k+2]->GetInterval())) {
            // spacer 1, foo RNA gene, and spacer2, complete sequence
            //         ^ 
            print_comma = true;
        } else if (two_more_in_list 
                   && ! oneafter_has_interval_change 
                   && NStr::Equal(m_ClauseList[k]->GetInterval(), m_ClauseList[k+2]->GetInterval())
                   && m_ClauseList[k+1]->DisplayAlleleName()
                   && m_ClauseList[k]->DisplayAlleleName()) {
            print_comma = true;  	
        } else if (k > 0 && !is_last
                   && ! oneafter_has_interval_change && ! onebefore_has_interval_change
                   && (m_ClauseList[k+1]->DisplayAlleleName ()
                       || m_ClauseList[k]->DisplayAlleleName())) {
            print_comma = true;  	
        }
 
        if (!NStr::IsBlank(m_ClauseList[k]->GetInterval())
            && NStr::Find(m_ClauseList[k]->GetInterval(), "partial") != 0
            && NStr::Find(m_ClauseList[k]->GetInterval(), "complete") != 0
            && (NStr::Equal(this_typeword, "transposon")
                || NStr::Equal(this_typeword, "insertion sequence")
                || (m_ClauseList[k]->GetMainFeatureSubtype() == CSeqFeatData::eSubtype_source
                    && NStr::Equal(this_typeword, "endogenous virus")))) {
            print_comma = false;
        }
        
        if (k > 0 && !onebefore_has_interval_change 
            && (is_last || oneafter_has_interval_change)) {
            m_ClauseList[k]->PluralizeInterval();
        }
          
        if (m_ClauseList[k]->NeedPlural()) {
            if ((k > 0 && ! onebefore_has_detail_change)
                || (!is_last && ! oneafter_has_detail_change)) {
                m_ClauseList[k]->PluralizeDescription();
            } else {
                typeword_is_plural = true;
            }
        }
    
        string clause_text = m_ClauseList[k]->PrintClause(print_typeword, typeword_is_plural);

        if (!NStr::IsBlank(clause_text)) {    
            if (!NStr::IsBlank(full_clause_list)) {
                full_clause_list += " ";
            }
            if (print_and) {
                full_clause_list += "and ";
            }
            full_clause_list += clause_text;
            if (print_comma) {
                full_clause_list += ",";
            }
        }
        
        if (is_last || oneafter_has_interval_change) {
            string this_interval = m_ClauseList[k]->GetInterval();
            if (!NStr::IsBlank(this_interval)) {
                full_clause_list += " " + this_interval;
            }
            if (print_semicolon && (NStr::IsBlank(this_interval) || !NStr::EndsWith(this_interval, ";"))) {
                if (allow_semicolons) {
                    full_clause_list += ";";
                } else {
                    full_clause_list += ",";
                }
            }
        }
    }
    return full_clause_list;       
}


unsigned int CAutoDefFeatureClause_Base::x_LastIntervalChangeBeforeEnd ()
{
    if (m_ClauseList.size() < 2) {
        return 0;
    }
    string last_interval = m_ClauseList[m_ClauseList.size() - 1]->GetInterval();
    
    for (unsigned int k = m_ClauseList.size() - 2; k > 0; k--) {
        if (!NStr::Equal(m_ClauseList[k]->GetInterval(), last_interval)) {
            return k + 1;
        }
    }
    if (NStr::Equal(m_ClauseList[0]->GetInterval(), last_interval)) {
        return m_ClauseList.size();
    } else {
        return 0;
    }    
}


sequence::ECompare CAutoDefFeatureClause_Base::CompareLocation(const CSeq_loc& loc)
{
    return sequence::eNoOverlap;
}


bool CAutoDefFeatureClause_Base::SameStrand(const CSeq_loc& loc)
{
    return false;
}


CRef<CSeq_loc> CAutoDefFeatureClause_Base::GetLocation()
{
    CRef<CSeq_loc> tmp;
    tmp.Reset(NULL);
    return tmp;
}


void CAutoDefFeatureClause_Base::AddToOtherLocation(CRef<CSeq_loc> loc)
{
}


void CAutoDefFeatureClause_Base::AddToLocation(CRef<CSeq_loc> loc)
{
}


void CAutoDefFeatureClause_Base::PluralizeInterval()
{
    if (NStr::IsBlank(m_Interval)) {
        return;
    }
    
    unsigned int pos = NStr::Find(m_Interval, "gene");
    if (pos != NCBI_NS_STD::string::npos 
        && (m_Interval.length() == pos + 4 || !NStr::Equal(m_Interval.substr(pos + 4, 1), "s"))) {
        m_Interval = m_Interval.substr(0, pos + 4) = "s" + m_Interval.substr(pos + 5);
    }
}


void CAutoDefFeatureClause_Base::PluralizeDescription()
{
    if (NStr::IsBlank(m_Description)) {
        return;
    }
    m_Description += "s";
}


void CAutoDefFeatureClause_Base::RemoveDeletedSubclauses()
{
    unsigned int k, j;
    k = 0;
    while (k < m_ClauseList.size()) {
        j = k;
        while (j < m_ClauseList.size() && (m_ClauseList[j] == NULL || m_ClauseList[j]->IsMarkedForDeletion())) {   
            if (m_ClauseList[j] != NULL) {
                delete m_ClauseList[j];         
            }
            j++;
        }
        if (j > k) {
           unsigned int num_removed = j - k;
           while (j < m_ClauseList.size()) {
                m_ClauseList[j - num_removed] = m_ClauseList[j];
                j++;
            }
            while (num_removed > 0) {
                m_ClauseList[m_ClauseList.size() - 1] = NULL;
                m_ClauseList.pop_back();
                num_removed --;
            }
        }
        while (k < m_ClauseList.size() && m_ClauseList[k] != NULL && !m_ClauseList[k]->IsMarkedForDeletion()) {
            m_ClauseList[k]->RemoveDeletedSubclauses();
            k++;
        }
    }   
}


bool CAutoDefFeatureClause_Base::x_OkToConsolidate (unsigned int clause1, unsigned int clause2)
{
    if (clause1 == clause2
        || clause1 >= m_ClauseList.size() || clause2 >= m_ClauseList.size()
        || m_ClauseList[clause1]->IsMarkedForDeletion()
        || m_ClauseList[clause2]->IsMarkedForDeletion()
        || (m_ClauseList[clause1]->IsPartial() && !m_ClauseList[clause2]->IsPartial())
        || (!m_ClauseList[clause1]->IsPartial() && m_ClauseList[clause2]->IsPartial())
        || !NStr::Equal(m_ClauseList[clause1]->GetDescription(), m_ClauseList[clause2]->GetDescription())
        || (m_ClauseList[clause1]->IsAltSpliced() && !m_ClauseList[clause2]->IsAltSpliced())
        || (!m_ClauseList[clause1]->IsAltSpliced() && m_ClauseList[clause2]->IsAltSpliced())) {
        return false;
    }
    
    CSeqFeatData::ESubtype subtype1 = m_ClauseList[clause1]->GetMainFeatureSubtype();
    CSeqFeatData::ESubtype subtype2 = m_ClauseList[clause2]->GetMainFeatureSubtype();
    
    if ((subtype1 == CSeqFeatData::eSubtype_cdregion 
         && subtype2 != CSeqFeatData::eSubtype_cdregion
         && subtype2 != CSeqFeatData::eSubtype_gene)
        || (subtype1 != CSeqFeatData::eSubtype_cdregion
            && subtype1 != CSeqFeatData::eSubtype_gene
            && subtype2 == CSeqFeatData::eSubtype_cdregion)) {
        return false;
    }
    return true;
}


void CAutoDefFeatureClause_Base::ConsolidateRepeatedClauses ()
{
    unsigned int last_clause = m_ClauseList.size();
    if (m_ClauseList.size() < 2) {
        return;
    }
    
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        m_ClauseList[k]->ConsolidateRepeatedClauses();
        if (m_ClauseList[k]->IsMarkedForDeletion()) {
            continue;
        }
        if (x_OkToConsolidate(k, last_clause)) {
            // Add subfeatures from new clause to last clause
            TClauseList new_subfeatures;
            new_subfeatures.clear();
            m_ClauseList[k]->TransferSubclauses(new_subfeatures);
            for (unsigned int j = 0; j < new_subfeatures.size(); j++) {
                m_ClauseList[last_clause]->AddSubclause(new_subfeatures[j]);
                new_subfeatures[j] = NULL;
            }
            new_subfeatures.clear();
            
            // Add new clause location to last clause
            m_ClauseList[last_clause]->AddToLocation(m_ClauseList[k]->GetLocation());
            // if we have two clauses that are really identical instead of
            // just sharing a "prefix", make the description plural
            if (NStr::Equal(m_ClauseList[last_clause]->GetInterval(), m_ClauseList[k]->GetInterval())) {
                m_ClauseList[last_clause]->SetMakePlural();
            }
            
            // this will regenerate the interval
            m_ClauseList[last_clause]->Label();
            
            // mark new clause for deletion
            m_ClauseList[k]->MarkForDeletion();
        } else {
            last_clause = k;
        }
    }
} 

// These are words that are used to introduced the part of the protein
// name that differs in alt-spliced products - they should not be part of
// the alt-spliced product name.
// Note that "splice variant" is checked before "variant" so that it will be
// found first and "variant" will not be removed from "splice variant", leaving
// splice as an orphan.
static const string unwanted_words[] = {"splice variant", "splice product", "variant", "isoform"};


/* This function determines whether two CDS clauses meet the conditions for
 * alternative splicing, and if so, it returns the name of the alternatively
 * spliced product.  In order to be alternatively spliced, the two CDSs 
 * must have the same gene, must share a complete interval, and must have
 * similarly named products.
 */
bool CAutoDefFeatureClause_Base::x_MeetAltSpliceRules (unsigned int clause1, unsigned int clause2, string &splice_name)
{
    if (clause1 >= m_ClauseList.size() || clause2 >= m_ClauseList.size()
        || m_ClauseList[clause1]->GetMainFeatureSubtype() != CSeqFeatData::eSubtype_cdregion
        || m_ClauseList[clause2]->GetMainFeatureSubtype() != CSeqFeatData::eSubtype_cdregion
        || m_ClauseList[clause1]->CompareLocation(*(m_ClauseList[clause2]->GetLocation())) != eOverlap) {
        return false;
    }
    // are genes the same?
    if (!NStr::Equal(m_ClauseList[clause1]->GetGeneName(), m_ClauseList[clause2]->GetGeneName())
        || !NStr::Equal(m_ClauseList[clause1]->GetAlleleName(), m_ClauseList[clause2]->GetAlleleName())) {
        return false;
    }

    // This section is used to calculate the parts of a product name that
    // are "the same" for use as the name of an alternatively spliced product.
    // The common portion of the string must end at a recognized separator,
    // such as a space, comma, or dash instead of in the middle of a word.
    // The matching portions of the string could occur at the beginning or end
    // of the string, or even occasionally at the beginning and end of a
    // string, but not as the center of the string with a different beginning
    // and ending.
    string product1 = m_ClauseList[clause1]->GetProductName();
    string product2 = m_ClauseList[clause2]->GetProductName();
    if (NStr::IsBlank(product1) || NStr::IsBlank(product2)) {
        return false;
    }
    
    if (NStr::Equal(product1, product2)) {
        splice_name = product1;
        return true;
    }
    
    unsigned int match_left_len = 0, match_left_token = 0;
    unsigned int len1 = product1.length();
    unsigned int len2 = product2.length();
    unsigned int match_len = 0;

    // find the length of match on the left
    while (match_left_len < len1 && match_left_len < len2
           && NStr::EqualNocase(product1, 0, match_left_len, product2)) {
        unsigned char ch = product1.c_str()[match_left_len];
        
        if (ch == ',' || ch == '-' || (isspace(ch) && match_left_token != match_left_len - 1)) {
            match_left_token = match_left_len;
        }
        match_left_len++;
    }
    if (match_left_len == len1 && m_ClauseList[clause1]->IsAltSpliced()) {
        match_left_token = match_left_len;
    } else {
        match_left_len = match_left_token;
    }

    // find the length of the match on the right
    unsigned int match_right_len = 0, match_right_token = 0;
    while (match_right_len < len1 && match_right_len < len2
           && NStr::EndsWith(product1, product2.substr(len1 - match_right_len - 1, match_right_len + 1))) {
        unsigned char ch = product1.c_str()[len1 - match_right_len - 1];
        
        if (ch == ',' || ch == '-' || isspace(ch)) {
            match_right_token = match_right_len;
        }
        match_right_len++;
    }
    if (match_right_len == len1 && m_ClauseList[clause1]->IsAltSpliced()) {
        match_right_token = match_right_len;
    } else {
        match_right_len = match_right_token;
    }
    
    if (match_left_len == 0 && match_right_len == 0) {
        return false;
    } else {
        splice_name = "";
        if (match_left_len > 0) {
            splice_name += product1.substr(0, match_left_len);
        }
        if (match_right_len > 0) {
            if (match_left_len > 0) {
                splice_name += " ";
            }
            
            splice_name += product1.substr(len1 - match_right_len);
        }
        
        // remove unwanted words from splice name
        for (unsigned int k = 0; k < sizeof(unwanted_words) / sizeof(string); k++) {
            unsigned int pos;
            while ((pos = NStr::Find(splice_name, unwanted_words[k])) != NCBI_NS_STD::string::npos ) {
                string temp_name = "";
                if (pos > 0) {
                    temp_name += splice_name.substr(0, pos);
                }
                if (pos < splice_name.length()) {
                    temp_name += splice_name.substr(pos + unwanted_words[k].length());
                }
            }
        }
        
        // remove spaces from either end
        NStr::TruncateSpacesInPlace(splice_name);
        
        return true;
    }
}


void CAutoDefFeatureClause_Base::FindAltSplices()
{
    unsigned int last_cds = m_ClauseList.size();
    string splice_name;
    
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        if (m_ClauseList[k]->IsMarkedForDeletion()) {
            continue;
        }
        m_ClauseList[k]->FindAltSplices();
        if (m_ClauseList[k]->GetMainFeatureSubtype() == CSeqFeatData::eSubtype_cdregion) {
            if (x_MeetAltSpliceRules(last_cds, k, splice_name)) {
                // set splice flag and product name for last cds
                m_ClauseList[last_cds]->SetAltSpliced(splice_name);
                
                // transfer sub features from new cds to last cds
                TClauseList new_subfeatures;
                new_subfeatures.clear();
                m_ClauseList[k]->TransferSubclauses(new_subfeatures);
                for (unsigned int j = 0; j < new_subfeatures.size(); j++) {
                    m_ClauseList[last_cds]->AddSubclause(new_subfeatures[j]);
                    new_subfeatures[j] = NULL;
                }
                new_subfeatures.clear();
                
                // remove new cds
                m_ClauseList[k]->MarkForDeletion();
                
                //Label original
                m_ClauseList[last_cds]->Label();
            } else {
                last_cds = k;
            }
        }
    }
}


CAutoDefFeatureClause_Base *CAutoDefFeatureClause_Base::FindBestParentClause(CAutoDefFeatureClause_Base * subclause, bool gene_cluster_opp_strand)
{
    CAutoDefFeatureClause_Base *best_parent = NULL;
    CAutoDefFeatureClause_Base *new_candidate;
    
    if (subclause == NULL || subclause == this) {
        return NULL;
    }
    
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        if (m_ClauseList[k] == NULL) {
            continue;
        }
        new_candidate = m_ClauseList[k]->FindBestParentClause(subclause, gene_cluster_opp_strand);
        if (new_candidate != NULL && new_candidate->GetLocation() != NULL) {
             if (best_parent == NULL) {
                 best_parent = new_candidate;
             } else if (best_parent->CompareLocation(*(new_candidate->GetLocation())) == sequence::eContained) {
                 best_parent = new_candidate;
             }
        }
    }
    return best_parent;
}


void CAutoDefFeatureClause_Base::GroupClauses(bool gene_cluster_opp_strand)
{
    CAutoDefFeatureClause_Base *best_parent;
    
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        best_parent = FindBestParentClause(m_ClauseList[k], gene_cluster_opp_strand);
        if (best_parent != NULL && best_parent != this) {
            best_parent->AddSubclause(m_ClauseList[k]);
            m_ClauseList[k] = NULL;
        } 
    }
    
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        if (m_ClauseList[k] == NULL) {
            continue;
        }
        m_ClauseList[k]->GroupClauses(gene_cluster_opp_strand);
    }
    
}


void CAutoDefFeatureClause_Base::CountUnknownGenes()
{
    CAutoDefUnknownGeneList *unknown_list = new CAutoDefUnknownGeneList(m_SuppressLocusTag);
    bool any_found = false;
    
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        if (NStr::Equal(m_ClauseList[k]->GetTypeword(), "gene")
            && NStr::Equal(m_ClauseList[k]->GetDescription(), "unknown")) {
            any_found = true;
            unknown_list->AddSubclause(m_ClauseList[k]);
            m_ClauseList[k] = NULL;
        } else {
            m_ClauseList[k]->CountUnknownGenes();
        }
    }
    
    if (any_found) {
        AddSubclause(unknown_list);
    } else {
        delete unknown_list;
    }        
}


CAutoDefUnknownGeneList::CAutoDefUnknownGeneList(bool suppress_locus_tags)
                  : CAutoDefFeatureClause_Base(suppress_locus_tags)
{
    m_Description = "unknown";
    m_DescriptionChosen = true;
    m_Typeword = "gene";
    m_TypewordChosen = true;
    m_ShowTypewordFirst = false;
}


CAutoDefUnknownGeneList::~CAutoDefUnknownGeneList()
{
}


void CAutoDefUnknownGeneList::Label()
{
    if (m_ClauseList.size() > 1) {
        m_MakePlural = true;
    }
}



END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.3  2006/04/18 01:05:07  ucko
* Don't bother clear()ing freshly allocated strings, particularly given
* that it would have been necessary to call erase() instead for
* compatibility with GCC 2.95.
*
* Revision 1.2  2006/04/17 17:38:45  ucko
* Fix capitalization of header filenames, and of "true".
*
* Revision 1.1  2006/04/17 16:25:05  bollin
* files for automatically generating definition lines, using a combination
* of modifiers to make definition lines unique within a set and listing the
* relevant features on the sequence.
*
*
* ===========================================================================
*/

