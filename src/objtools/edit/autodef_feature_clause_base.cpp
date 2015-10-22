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
#include <corelib/ncbimisc.hpp>
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
                              m_GeneIsPseudo(false),
                              m_IsAltSpliced(false),
                              m_HasmRNA(false),
                              m_HasGene(false),
                              m_MakePlural(false),
                              m_IsUnknown(false),
                              m_ClauseInfoOnly(false),
                              m_SuppressSubfeatures(false),
                              m_DeleteMe(false)
{
}


CAutoDefFeatureClause_Base::~CAutoDefFeatureClause_Base()
{
}


CSeqFeatData::ESubtype CAutoDefFeatureClause_Base::GetMainFeatureSubtype() const
{
    if (m_ClauseList.size() == 1) {
        return m_ClauseList[0]->GetMainFeatureSubtype();
    } else {
        return CSeqFeatData::eSubtype_bad;
    }    
}


bool CAutoDefFeatureClause_Base::IsuORF(const string& product)
{
    // find uORF as whole word in product name
    size_t pos = NStr::Find(product, "uORF");
    if (pos != string::npos &&
        (pos == 0 || isspace(product.c_str()[pos - 1])) &&
        (pos == product.length() - 4 || isspace(product.c_str()[pos + 4]))) {
        return true;
    } else if (NStr::EndsWith(product, "leader peptide")) {
        return true;
    } else {
        return false;
    }
}


void CAutoDefFeatureClause_Base::AddSubclause (CAutoDefFeatureClause_Base *subclause)
{
    if (subclause) {
        m_ClauseList.push_back(subclause);
        if (subclause->IsAltSpliced()) {
            m_IsAltSpliced = true;
        }
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


string CAutoDefFeatureClause_Base::PrintClause(bool print_typeword, bool typeword_is_plural, bool suppress_allele)
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


bool CAutoDefFeatureClause_Base::IsUnattachedGene() const
{
    if (GetMainFeatureSubtype() != CSeqFeatData::eSubtype_gene) {
        return false;
    }

    for (unsigned int j = 0; j < m_ClauseList.size(); j++) {
        if (!m_ClauseList[j]->IsMarkedForDeletion()) {
            return false;
        }
    }
    return true;
}


bool CAutoDefFeatureClause_Base::AddmRNA (CAutoDefFeatureClause_Base *mRNAClause)
{
    bool retval = false;
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        retval |= m_ClauseList[k]->AddmRNA(mRNAClause);
    }
    return retval;
}
 
 
bool CAutoDefFeatureClause_Base::AddGene (CAutoDefFeatureClause_Base *gene_clause, bool suppress_allele)
{
    bool retval = false;
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        retval |= m_ClauseList[k]->AddGene(gene_clause, suppress_allele);
    }
    return retval;
}


void CAutoDefFeatureClause_Base::Label(bool suppress_allele)
{
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        m_ClauseList[k]->Label(suppress_allele);
    }
}

// Grouping functions
void CAutoDefFeatureClause_Base::GroupmRNAs(bool suppress_allele)
{
    unsigned int k, j;
    
    // Add mRNAs to other clauses
    for (k = 0; k < m_ClauseList.size(); k++) {
        if (m_ClauseList[k]->IsMarkedForDeletion()
            || m_ClauseList[k]->GetMainFeatureSubtype() != CSeqFeatData::eSubtype_mRNA) {
            continue;
        }
        m_ClauseList[k]->Label(suppress_allele);
        bool mRNA_used = false;
        for (j = 0; j < m_ClauseList.size() && !mRNA_used; j++) {
            if (m_ClauseList[j]->IsMarkedForDeletion()
                || j == k
                || m_ClauseList[j]->GetMainFeatureSubtype() != CSeqFeatData::eSubtype_cdregion) {
                continue;
            } else {
                m_ClauseList[j]->Label(suppress_allele);
                mRNA_used |= m_ClauseList[j]->AddmRNA (m_ClauseList[k]);
            }
        }
        if (mRNA_used) {
            m_ClauseList[k]->MarkForDeletion();
        }
    }
}


void CAutoDefFeatureClause_Base::GroupGenes(bool suppress_allele)
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
        for (j = 0; j < m_ClauseList.size(); j++) {
            if (j == k || m_ClauseList[j]->GetMainFeatureSubtype() == CSeqFeatData::eSubtype_gene) {
                continue;
            }
            used_gene |= m_ClauseList[j]->AddGene(m_ClauseList[k], suppress_allele);
        }
    }
}


void CAutoDefFeatureClause_Base::RemoveGenesMentionedElsewhere()
{
    unsigned int k, j;
    
    for (k = 0; k < m_ClauseList.size(); k++) {
        if (m_ClauseList[k]->GetMainFeatureSubtype() == CSeqFeatData::eSubtype_gene) {
            // only remove the gene if it has no subclauses not already marked for deletion
            if (m_ClauseList[k]->IsUnattachedGene()) {
                for (j = 0; j < m_ClauseList.size() && !m_ClauseList[k]->IsMarkedForDeletion(); j++) {
                    if (j != k && !m_ClauseList[j]->IsMarkedForDeletion() 
                        && m_ClauseList[j]->IsGeneMentioned(m_ClauseList[k])) {
                        m_ClauseList[k]->MarkForDeletion();
                    }
                }
            }
        } else {
            m_ClauseList[k]->RemoveGenesMentionedElsewhere();
        }

    }
}


string CAutoDefFeatureClause_Base::ListClauses(bool allow_semicolons, bool suppress_final_and, bool suppress_allele)
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
            } else if ((m_ClauseList[k-1]->IsAltSpliced() && ! m_ClauseList[k]->IsAltSpliced())
                       || (!m_ClauseList[k-1]->IsAltSpliced() && m_ClauseList[k]->IsAltSpliced())) {
                onebefore_has_interval_change = true;
            }
            if (!NStr::Equal(m_ClauseList[k-1]->GetTypeword(), m_ClauseList[k]->GetTypeword())){
                onebefore_has_typeword_change = true;
            }
            if (onebefore_has_typeword_change  ||
                onebefore_has_interval_change  ||
                (m_ClauseList[k-1]->DisplayAlleleName()  &&
                 m_ClauseList[k]->DisplayAlleleName())) {
                onebefore_has_detail_change = true;  
            }
        }
        if (!is_last) {
            if (!NStr::Equal(m_ClauseList[k]->GetInterval(), m_ClauseList[k + 1]->GetInterval())) {
                oneafter_has_interval_change = true;
            } else if ((m_ClauseList[k+1]->IsAltSpliced() && ! m_ClauseList[k]->IsAltSpliced())
                       || (!m_ClauseList[k+1]->IsAltSpliced() && m_ClauseList[k]->IsAltSpliced())) {
                oneafter_has_interval_change = true;
            }
            if (!NStr::Equal(m_ClauseList[k]->GetTypeword(), m_ClauseList[k + 1]->GetTypeword())) {
                oneafter_has_typeword_change = true;
            }
            if (oneafter_has_typeword_change  ||
                oneafter_has_interval_change  ||
                (m_ClauseList[k+1]->DisplayAlleleName()  &&
                 m_ClauseList[k]->DisplayAlleleName())) {
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
            if (suppress_final_and && is_last) {
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
            && (m_ClauseList[k]->IsMobileElement()
                || (m_ClauseList[k]->GetMainFeatureSubtype() == CSeqFeatData::eSubtype_biosrc
                    && NStr::Equal(this_typeword, "endogenous virus")))) {
            print_comma = false;
        }
        
        if (k > 0 && !onebefore_has_interval_change 
            && (is_last || oneafter_has_interval_change)) {
            m_ClauseList[k]->PluralizeInterval();
        }
        
        // pluralize description or typeword as necessary  
        if (m_ClauseList[k]->IsExonList()) {
            typeword_is_plural = true;
        } else if (m_ClauseList[k]->NeedPlural()) {
            if ((k > 0 && ! onebefore_has_detail_change)
                || (!is_last && ! oneafter_has_detail_change)) {
                m_ClauseList[k]->PluralizeDescription();
            } else {
                typeword_is_plural = true;
            }
        }
    
        string clause_text = m_ClauseList[k]->PrintClause(print_typeword, typeword_is_plural, suppress_allele);

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
        if (!NStr::Equal(m_ClauseList[k]->GetInterval(), last_interval)
            || (m_ClauseList[k]->IsAltSpliced() && ! m_ClauseList[k + 1]->IsAltSpliced())
            || (!m_ClauseList[k]->IsAltSpliced() && m_ClauseList[k + 1]->IsAltSpliced())) {
            return k + 1;
        }
    }
    if (NStr::Equal(m_ClauseList[0]->GetInterval(), last_interval)
        && ((m_ClauseList[0]->IsAltSpliced() && m_ClauseList[1]->IsAltSpliced())
            || (!m_ClauseList[0]->IsAltSpliced() && !m_ClauseList[1]->IsAltSpliced()))) {
        return m_ClauseList.size();
    } else {
        return 1;
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


void CAutoDefFeatureClause_Base::AddToLocation(CRef<CSeq_loc> loc, bool also_set_partials)
{
}


void CAutoDefFeatureClause_Base::PluralizeInterval()
{
    if (NStr::IsBlank(m_Interval)) {
        return;
    }
    
    string::size_type pos = NStr::Find(m_Interval, "gene");
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


void CAutoDefFeatureClause_Base::ShowSubclauses()
{
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        //bool marked = m_ClauseList[k]->IsMarkedForDeletion();                      
        //bool partial = m_ClauseList[k]->IsPartial();
        //string desc1 = m_ClauseList[k]->GetDescription();
        
        //unsigned int stop = m_ClauseList[k]->GetLocation()->GetStop(eExtreme_Positional);
        //unsigned int start = m_ClauseList[k]->GetLocation()->GetStart(eExtreme_Positional);
            
        //CSeqFeatData::ESubtype st1 = m_ClauseList[k]->GetMainFeatureSubtype();
        m_ClauseList[k]->ShowSubclauses();
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


void CAutoDefFeatureClause_Base::Consolidate(CAutoDefFeatureClause_Base& other, bool suppress_allele)
{
    // Add subfeatures from new clause to last clause
    TClauseList new_subfeatures;
    new_subfeatures.clear();
    other.TransferSubclauses(new_subfeatures);
    for (unsigned int j = 0; j < new_subfeatures.size(); j++) {
        AddSubclause(new_subfeatures[j]);
        new_subfeatures[j] = NULL;
    }
    new_subfeatures.clear();
            
    // Add new clause location to last clause
    AddToLocation(other.GetLocation());
    // if we have two clauses that are really identical instead of
    // just sharing a "prefix", make the description plural
    if (NStr::Equal(GetInterval(), other.GetInterval())) {
        SetMakePlural();
    }
            
    // this will regenerate the interval
    Label(suppress_allele);
            
    // mark other clause for deletion
    other.MarkForDeletion();
}


void CAutoDefFeatureClause_Base::x_RemoveNullClauses()
{
    TClauseList::iterator it = m_ClauseList.begin();
    while (it != m_ClauseList.end()) {
        if (*it == NULL) {
            it = m_ClauseList.erase(it);
        } else {
            ++it;
        }
    }
}


void CAutoDefFeatureClause_Base::ConsolidateRepeatedClauses (bool suppress_allele)
{
    if (m_ClauseList.size() < 2) {
        return;
    }
    
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        if (!m_ClauseList[k] || m_ClauseList[k]->IsMarkedForDeletion()) {
            continue;
        }
        m_ClauseList[k]->ConsolidateRepeatedClauses(suppress_allele);
        for (unsigned int n = k + 1; n < m_ClauseList.size(); n++) {
            if (!m_ClauseList[n] || m_ClauseList[n]->IsMarkedForDeletion()) {
                continue;
            }
            if (x_OkToConsolidate(k, n)) {
                CSeqFeatData::ESubtype subtypek = m_ClauseList[k]->GetMainFeatureSubtype();
                CSeqFeatData::ESubtype subtypen = m_ClauseList[n]->GetMainFeatureSubtype();

                if (subtypek == CSeqFeatData::eSubtype_gene) {
                    m_ClauseList[n]->Consolidate(*m_ClauseList[k], suppress_allele);
                } else if (subtypen == CSeqFeatData::eSubtype_gene) {
                    m_ClauseList[k]->Consolidate(*m_ClauseList[n], suppress_allele);
                } else {
                    m_ClauseList[k]->AddSubclause(m_ClauseList[n]);
                    m_ClauseList[k]->SuppressSubfeatures();
                    m_ClauseList[k]->SetMakePlural();
                    m_ClauseList[n] = NULL;
                } 
            } else {
                // do not consolidate with non-consecutive clauses
                break;
            }
        }
   
    }
    x_RemoveNullClauses();
    Label(suppress_allele);
} 

// These are words that are used to introduced the part of the protein
// name that differs in alt-spliced products - they should not be part of
// the alt-spliced product name.
// Note that "splice variant" is checked before "variant" so that it will be
// found first and "variant" will not be removed from "splice variant", leaving
// splice as an orphan.
static const string unwanted_words[] = {"splice variant", "splice product", "variant", "isoform"};


// This function determins whether two locations share a common interval
bool ShareInterval(const CSeq_loc& loc1, const CSeq_loc& loc2)
{
    for (CSeq_loc_CI loc_iter1(loc1); loc_iter1;  ++loc_iter1) {
        for (CSeq_loc_CI loc_iter2(loc2); loc_iter2; ++loc_iter2) {
            if (loc_iter1.GetEmbeddingSeq_loc().Equals(loc_iter2.GetEmbeddingSeq_loc())) {
                return true;
            }
        }
    }
    return false;
}


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
        || m_ClauseList[clause2]->GetMainFeatureSubtype() != CSeqFeatData::eSubtype_cdregion) {
        return false;
    }
    
    if (!ShareInterval(*(m_ClauseList[clause1]->GetLocation()), *(m_ClauseList[clause2]->GetLocation()))) {
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
    
    unsigned int match_left_len = 1, match_left_token = 0;
    unsigned int len1 = product1.length();
    unsigned int len2 = product2.length();

    // find the length of match on the left
    while (match_left_len < len1 && match_left_len < len2
           && NStr::Equal (product1.substr(0, match_left_len), product2.substr(0, match_left_len))) {
        unsigned char ch = product1.c_str()[match_left_len];
        
        if (ch == ',' || ch == '-' || (isspace(ch) && match_left_token != match_left_len - 1)) {
            match_left_token = match_left_len;
        }
        match_left_len++;
    }
    if (!NStr::Equal (product1.substr(0, match_left_len), product2.substr(0, match_left_len))
        && match_left_len > 0) {
        match_left_len--;
    }
    if (match_left_len == len1 && m_ClauseList[clause1]->IsAltSpliced()) {
        match_left_token = match_left_len;
    } else {
        match_left_len = match_left_token;
    }
    
    // find the length of the match on the right
    unsigned int match_right_len = 0, match_right_token = 0;
    while (match_right_len < len1 && match_right_len < len2
           && NStr::Equal (product1.substr(len1 - match_right_len - 1), product2.substr(len2 - match_right_len - 1))) {
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
            string::size_type pos;
            while ((pos = NStr::Find(splice_name, unwanted_words[k])) != NCBI_NS_STD::string::npos ) {
                string temp_name = "";
                if (pos > 0) {
                    temp_name += splice_name.substr(0, pos);
                }
                if (pos < splice_name.length()) {
                    temp_name += splice_name.substr(pos + unwanted_words[k].length());
                }
                splice_name = temp_name;
            }
        }
        
        // remove spaces from either end
        NStr::TruncateSpacesInPlace(splice_name);
        
        return true;
    }
}


void CAutoDefFeatureClause_Base::FindAltSplices(bool suppress_allele)
{
    unsigned int last_cds = m_ClauseList.size();
    string splice_name;
    
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        if (m_ClauseList[k]->IsMarkedForDeletion()) {
            continue;
        }
        m_ClauseList[k]->FindAltSplices(suppress_allele);
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
                m_ClauseList[last_cds]->Label(suppress_allele);
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
    sequence::ECompare loc_compare;
    
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
             } else {
                 loc_compare = best_parent->CompareLocation(*(new_candidate->GetLocation()));
                 if (loc_compare == sequence::eContained) {
                     best_parent = new_candidate;
                 } else if (loc_compare == sequence::eSame) {
                     // locations are identical, choose the better feature
                     // for now, everything beats a gene, coding region beats mRNA
                     CSeqFeatData::ESubtype best_subtype = best_parent->GetMainFeatureSubtype();
                     CSeqFeatData::ESubtype new_subtype = new_candidate->GetMainFeatureSubtype();
                     if (best_subtype == CSeqFeatData::eSubtype_gene) {
                         best_parent = new_candidate;
                     } else if (best_subtype == CSeqFeatData::eSubtype_mRNA 
                                && new_subtype == CSeqFeatData::eSubtype_cdregion) {
                         best_parent = new_candidate;
                     }
                 }
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


void CAutoDefFeatureClause_Base::RemoveNonSegmentClauses(CRange<TSeqPos> range)
{
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        if (m_ClauseList[k] != NULL && !m_ClauseList[k]->IsMarkedForDeletion()) {
            m_ClauseList[k]->RemoveNonSegmentClauses(range);
            unsigned int stop = m_ClauseList[k]->GetLocation()->GetStop(eExtreme_Positional);
            if (stop < range.GetFrom() || stop > range.GetTo()) {
                if (m_ClauseList[k]->GetNumSubclauses() == 0) {
                    m_ClauseList[k]->MarkForDeletion();
                } else {
                    m_ClauseList[k]->SetInfoOnly(true);
                }
            }
        }
    }
    RemoveDeletedSubclauses();
}


string CAutoDefFeatureClause_Base::FindGeneProductName(CAutoDefFeatureClause_Base *gene_clause)
{
    if (gene_clause == NULL) {
        return "";
    }
    string look_for_name = gene_clause->GetGeneName();
    string look_for_allele = gene_clause->GetAlleleName();
    if (NStr::IsBlank(look_for_name)) {
        return "";
    }
    
    string product_name = "";
    
    for (unsigned int k = 0; k < m_ClauseList.size() && NStr::IsBlank(product_name); k++) {
        if (gene_clause == m_ClauseList[k]) {
            continue;
        }
        if (NStr::Equal(look_for_name, m_ClauseList[k]->GetGeneName())
            && NStr::Equal(look_for_allele, m_ClauseList[k]->GetAlleleName())) {
            product_name = m_ClauseList[k]->GetProductName();
        }
        if (NStr::IsBlank(product_name)) {
            product_name = m_ClauseList[k]->FindGeneProductName(gene_clause);
        }
    }
    return product_name;
}


void CAutoDefFeatureClause_Base::AssignGeneProductNames(CAutoDefFeatureClause_Base *main_clause, bool suppress_allele)
{
    if (main_clause == NULL) {
        return;
    }
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        if (NStr::IsBlank(m_ClauseList[k]->GetProductName())) {
            string product_name = main_clause->FindGeneProductName(m_ClauseList[k]);
            if (!NStr::IsBlank(product_name)) {
                m_ClauseList[k]->SetProductName(product_name);
                m_ClauseList[k]->Label(suppress_allele);
            }
        }
    }
}

void CAutoDefFeatureClause_Base::SetProductName(string product_name)
{
    m_ProductName = product_name;
    m_ProductNameChosen = true;
    m_DescriptionChosen = false;
}


void CAutoDefFeatureClause_Base::CountUnknownGenes()
{
    CAutoDefUnknownGeneList *unknown_list = new CAutoDefUnknownGeneList();
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


void CAutoDefFeatureClause_Base::SuppressMobileElementAndInsertionSequenceSubfeatures()
{
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        if (m_ClauseList[k]->IsMobileElement() || m_ClauseList[k]->IsInsertionSequence()) {
            m_ClauseList[k]->SuppressSubfeatures();
        } else {
            m_ClauseList[k]->SuppressMobileElementAndInsertionSequenceSubfeatures();
        }
    }
}


// This method groups alt-spliced exons that do not have a coding region
void CAutoDefFeatureClause_Base::GroupAltSplicedExons(CBioseq_Handle bh)
{    
    if (m_ClauseList.size() == 0) {
        return;
    }
    for (unsigned int k = 0; k < m_ClauseList.size() - 1; k++) {
        if (m_ClauseList[k] == NULL || m_ClauseList[k]->IsMarkedForDeletion()
            || m_ClauseList[k]->GetMainFeatureSubtype() != CSeqFeatData::eSubtype_exon
            || ! m_ClauseList[k]->IsAltSpliced()) {
            continue;
        }
        bool found_any = false;
        unsigned int j = k + 1;
        while (j < m_ClauseList.size() && ! found_any) {
            if (m_ClauseList[j] != NULL && !m_ClauseList[j]->IsMarkedForDeletion()
                && m_ClauseList[j]->GetMainFeatureSubtype() == CSeqFeatData::eSubtype_exon
                && m_ClauseList[j]->IsAltSpliced()
                && m_ClauseList[j]->CompareLocation(*(m_ClauseList[k]->GetLocation())) != sequence::eNoOverlap ) {
                found_any = true;
            } else {
                j++;
            }
        }
        if (!found_any) {
            continue;
        }
        CAutoDefExonListClause *new_clause = new CAutoDefExonListClause (bh);
        
        new_clause->AddSubclause(m_ClauseList[k]);
        new_clause->AddSubclause(m_ClauseList[j]);
        m_ClauseList[j] = NULL;
        j++;
        while (j < m_ClauseList.size()) {
            if (m_ClauseList[j] != NULL && ! m_ClauseList[j]->IsMarkedForDeletion()
                && m_ClauseList[j]->GetMainFeatureSubtype() == CSeqFeatData::eSubtype_exon
                && m_ClauseList[j]->IsAltSpliced()
                && m_ClauseList[j]->CompareLocation(*(m_ClauseList[k]->GetLocation())) == sequence::eOverlap) {
                new_clause->AddSubclause(m_ClauseList[j]);
                m_ClauseList[j] = NULL;
            }
            j++;
        }
        m_ClauseList[k] = new_clause;
    }
}


void CAutoDefFeatureClause_Base::ExpandExonLists()
{
    unsigned int k = 0;
    
    while (k < m_ClauseList.size()) {
        if (m_ClauseList[k]->IsExonList()) {
            TClauseList remaining;
            remaining.clear();
            for (unsigned int j = k + 1; j < m_ClauseList.size(); j++) {
                remaining.push_back(m_ClauseList[j]);
                m_ClauseList[j] = NULL;
            }
            TClauseList subclauses;
            subclauses.clear();
            m_ClauseList[k]->TransferSubclauses(subclauses);
            delete m_ClauseList[k];
            for (unsigned int j = 0; j < subclauses.size(); j++) {
                if (k + j < m_ClauseList.size()) {
                    m_ClauseList[k + j] = subclauses[j];
                } else {
                    m_ClauseList.push_back(subclauses[j]);
                }
                subclauses[j] = NULL;
            }
            for (unsigned int j = 0; j < remaining.size(); j++) {
                if (k + subclauses.size() + j < m_ClauseList.size()) {
                    m_ClauseList[k + subclauses.size() + j] = remaining[j];
                } else {
                    m_ClauseList.push_back(remaining[j]);
                }
                remaining[j] = NULL;
            }
            k += subclauses.size();
            subclauses.clear();
            remaining.clear();
        } else {
            m_ClauseList[k]->ExpandExonLists();
            k++;
        }
    }
}


void CAutoDefFeatureClause_Base::ReverseCDSClauseLists()
{
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        m_ClauseList[k]->ReverseCDSClauseLists();
    }
}


void CAutoDefFeatureClause_Base::GroupConsecutiveExons(CBioseq_Handle bh)
{
    if (m_ClauseList.size() > 1) {
        CAutoDefExonListClause *last_new_clause = NULL;
        unsigned int last_new_clause_position = 0;
        for (unsigned int k=0; k < m_ClauseList.size() - 1; k++) {
            if (m_ClauseList[k] == NULL || m_ClauseList[k]->IsMarkedForDeletion()
                || m_ClauseList[k]->GetMainFeatureSubtype() != CSeqFeatData::eSubtype_exon) {
                continue;
            }
            m_ClauseList[k]->Label(false);
            string sequence = m_ClauseList[k]->GetDescription();
            unsigned int seq_num = 0, next_num;
            try {
                seq_num = NStr::StringToUInt(sequence);
            } catch ( ... ) {
                continue;
            }
        
            CAutoDefExonListClause *new_clause = NULL;
            unsigned int j = k + 1;
            while (j < m_ClauseList.size()) {
                if (m_ClauseList[j] != NULL && !m_ClauseList[j]->IsMarkedForDeletion()) {
                    if (m_ClauseList[j]->GetMainFeatureSubtype() == CSeqFeatData::eSubtype_exon) {
                        m_ClauseList[j]->Label(false);
                        sequence = m_ClauseList[j]->GetDescription();
                        try {
                            next_num = NStr::StringToUInt (sequence);
                        } catch ( ... ) {
                            // next is exon but not in consecutive list and splice info matches, suppress final and
                            if (new_clause != NULL 
                                && ((m_ClauseList[j]->IsAltSpliced() && m_ClauseList[k]->IsAltSpliced())
                                    || (!m_ClauseList[j]->IsAltSpliced() && !m_ClauseList[k]->IsAltSpliced()))) {
                                new_clause->SetSuppressFinalAnd(true);
                            }
                            break;
                        }
                        if (next_num == seq_num + 1) {
                            if (new_clause == NULL) {
                                new_clause = new CAutoDefExonListClause(bh);
                                new_clause->AddSubclause(m_ClauseList[k]);
                                last_new_clause = new_clause;
                                last_new_clause_position = k;
                            }
                            new_clause->AddSubclause(m_ClauseList[j]);
                            m_ClauseList[j] = NULL;
                            seq_num = next_num;
                        } else {
                            // next is exon but not in consecutive list and splice info matches, suppress final and
                            if (new_clause != NULL 
                                && ((m_ClauseList[j]->IsAltSpliced() && m_ClauseList[k]->IsAltSpliced())
                                    || (!m_ClauseList[j]->IsAltSpliced() && !m_ClauseList[k]->IsAltSpliced()))) {
                                new_clause->SetSuppressFinalAnd(true);
                            }
                            break;
                        }
                    } else {
                        break;
                    }
                } else {
                    j++;
                }
            }
            if (new_clause != NULL) {
                m_ClauseList[k] = new_clause;
                new_clause->Label(false);
            }
        }
        if (last_new_clause != NULL) {
            bool is_last = true;
            for (unsigned int j = last_new_clause_position + 1;
                 j < m_ClauseList.size() && is_last;
                 j++) {
                if (m_ClauseList[j] != NULL 
                    && !m_ClauseList[j]->IsMarkedForDeletion()
                    && m_ClauseList[j]->GetMainFeatureSubtype() == CSeqFeatData::eSubtype_exon) {
                    is_last = false;
                }
            }
            if (is_last) {
                last_new_clause->SetSuppressFinalAnd(false);
            }
        }
    }
    
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        if (m_ClauseList[k] != NULL && ! m_ClauseList[k]->IsMarkedForDeletion()
            && m_ClauseList[k]->GetMainFeatureSubtype() != CSeqFeatData::eSubtype_exon) {
            m_ClauseList[k]->GroupConsecutiveExons(bh);
        }
    }
}


// This function should combine CDSs on a segmented set that do not have a joined location
// but are part of the same gene and have the same protein name.
void CAutoDefFeatureClause_Base::GroupSegmentedCDSs (bool suppress_allele)
{
    if (m_ClauseList.size() > 1) {
        for (unsigned int k = 0; k < m_ClauseList.size() - 1; k++) {
            if (m_ClauseList[k] == NULL
                || m_ClauseList[k]->IsMarkedForDeletion()
                || m_ClauseList[k]->GetMainFeatureSubtype() != CSeqFeatData::eSubtype_cdregion) {
                continue;
            }
            m_ClauseList[k]->Label(suppress_allele);
            for (unsigned int j = k + 1; j < m_ClauseList.size(); j++) {
                if (m_ClauseList[j] == NULL 
                    || m_ClauseList[j]->IsMarkedForDeletion()
                    || m_ClauseList[j]->GetMainFeatureSubtype() != CSeqFeatData::eSubtype_cdregion) {
                    continue;
                }
                m_ClauseList[j]->Label(suppress_allele);
                if (NStr::Equal(m_ClauseList[k]->GetProductName(), m_ClauseList[j]->GetProductName())
                    && !NStr::IsBlank(m_ClauseList[k]->GetGeneName())
                    && NStr::Equal(m_ClauseList[k]->GetGeneName(), m_ClauseList[j]->GetGeneName())
                    && NStr::Equal(m_ClauseList[k]->GetAlleleName(), m_ClauseList[j]->GetAlleleName())) {
                    // note - we'd actually like to make sure thate the genes are the same and don't just
                    // have matching names
                    
                    // Add subfeatures from new clause to last clause
                    TClauseList new_subfeatures;
                    new_subfeatures.clear();
                    m_ClauseList[j]->TransferSubclauses(new_subfeatures);
                    for (unsigned int n = 0; n < new_subfeatures.size(); n++) {
                        m_ClauseList[k]->AddSubclause(new_subfeatures[n]);
                        new_subfeatures[n] = NULL;
                    }
                    new_subfeatures.clear();
            
                    // Add new clause location to last clause
                    m_ClauseList[k]->AddToLocation(m_ClauseList[j]->GetLocation());
                    
                    // remove repeated clause
                    m_ClauseList[j]->MarkForDeletion();
                }
            }
        }
    }
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        if (m_ClauseList[k] != NULL && !m_ClauseList[k]->IsMarkedForDeletion()) {
            m_ClauseList[k]->GroupSegmentedCDSs(suppress_allele);
        }
    }
}


void CAutoDefFeatureClause_Base::RemoveFeaturesByType(unsigned int feature_type)
{
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        if ((unsigned int)m_ClauseList[k]->GetMainFeatureSubtype() == feature_type) {
            m_ClauseList[k]->MarkForDeletion();
        } else if (!m_ClauseList[k]->IsMarkedForDeletion()) {
            m_ClauseList[k]->RemoveFeaturesByType(feature_type);
        }        
    }
}


bool CAutoDefFeatureClause_Base::IsFeatureTypeLonely(unsigned int feature_type)
{
    unsigned int k, subtype;
    bool         is_lonely = true;
    
    for (k=0; k < m_ClauseList.size() && is_lonely; k++) {
        subtype = m_ClauseList[k]->GetMainFeatureSubtype();
        if (subtype == feature_type) {
            // do nothing
        } else if (subtype == CSeqFeatData::eSubtype_gene 
                   || subtype == CSeqFeatData::eSubtype_mRNA) {
            is_lonely = m_ClauseList[k]->IsFeatureTypeLonely(feature_type);
        } else {
            is_lonely = false;
        }
    }
    return is_lonely;    
}


void CAutoDefFeatureClause_Base::RemoveFeaturesInmRNAsByType(unsigned int feature_type)
{
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        if (m_ClauseList[k]->HasmRNA() 
            || m_ClauseList[k]->GetMainFeatureSubtype() == CSeqFeatData::eSubtype_mRNA) {
            m_ClauseList[k]->RemoveFeaturesByType(feature_type);
        }
    }
}


bool CAutoDefFeatureClause_Base::ShouldRemoveExons()
{
    return false;
}


void CAutoDefFeatureClause_Base::RemoveUnwantedExons()
{
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        if (m_ClauseList[k]->ShouldRemoveExons()) {
            m_ClauseList[k]->RemoveFeaturesByType(CSeqFeatData::eSubtype_exon);
        } else if (m_ClauseList[k]->GetMainFeatureSubtype() == CSeqFeatData::eSubtype_exon) {
            m_ClauseList[k]->MarkForDeletion();
        } else {
            m_ClauseList[k]->RemoveUnwantedExons();
        }
    }
}


bool CAutoDefFeatureClause_Base::IsBioseqPrecursorRNA()
{
    if (m_ClauseList.size() != 1) {
        return false;
    } else if (m_ClauseList[0]->IsBioseqPrecursorRNA()) {
        return true;
    } else {
        return false;
    }
}


void CAutoDefFeatureClause_Base::RemoveBioseqPrecursorRNAs()
{
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        if (m_ClauseList[k]->IsBioseqPrecursorRNA()) {
            m_ClauseList[k]->MarkForDeletion();
        }
    }        
}


void CAutoDefFeatureClause_Base::RemoveTransSplicedLeaders()
{
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        unsigned int subtype = m_ClauseList[k]->GetMainFeatureSubtype();
        if ((subtype == CSeqFeatData::eSubtype_otherRNA
             || subtype == CSeqFeatData::eSubtype_misc_RNA)
            && NStr::Find(m_ClauseList[k]->GetDescription(), "trans-spliced leader") != NCBI_NS_STD::string::npos) {
            m_ClauseList[k]->MarkForDeletion();
        } else {
            m_ClauseList[k]->RemoveTransSplicedLeaders();
        }
    }
}


void CAutoDefFeatureClause_Base::RemoveuORFs()
{
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        unsigned int subtype = m_ClauseList[k]->GetMainFeatureSubtype();
        if (subtype == CSeqFeatData::eSubtype_cdregion &&
            IsuORF(m_ClauseList[k]->GetProductName())) {
            m_ClauseList[k]->MarkForDeletion();
        } else {
            m_ClauseList[k]->RemoveuORFs();
        }
    }
}

void CAutoDefFeatureClause_Base::RemoveOptionalMobileElements()
{
    for (unsigned int k = 0; k < m_ClauseList.size(); k++) {
        CAutoDefMobileElementClause* clause = dynamic_cast<CAutoDefMobileElementClause *>(m_ClauseList[k]);        
        if (clause && clause->IsOptional()) {
            m_ClauseList[k]->MarkForDeletion();
        } else {
            m_ClauseList[k]->RemoveOptionalMobileElements();
        }
    }
}


CAutoDefUnknownGeneList::CAutoDefUnknownGeneList()
                  : CAutoDefFeatureClause_Base()
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


void CAutoDefUnknownGeneList::Label(bool suppress_allele)
{
    if (m_ClauseList.size() > 1) {
        m_MakePlural = true;
    }
    m_Description = "unknown";
    m_DescriptionChosen = true;
}


CAutoDefExonListClause::CAutoDefExonListClause(CBioseq_Handle bh)
                  : CAutoDefFeatureClause_Base(),
                    m_SuppressFinalAnd(false),
                    m_BH(bh)
{
    m_Typeword = "exon";
    m_TypewordChosen = true;
    m_ShowTypewordFirst = true;
    m_ClauseLocation = new CSeq_loc();
        
}


CSeqFeatData::ESubtype CAutoDefExonListClause::GetMainFeatureSubtype() const
{ 
    return CSeqFeatData::eSubtype_exon; 
}


// Note - because we are grouping alt-spliced exons, we already know that the locations overlap
CRef<CSeq_loc> CAutoDefExonListClause::SeqLocIntersect (CRef<CSeq_loc> loc1, CRef<CSeq_loc> loc2)
{
    CRef<CSeq_loc> intersect_loc;
    bool           first = true;
    
    intersect_loc = new CSeq_loc();
    
    for (CSeq_loc_CI loc_iter1(*loc1); loc_iter1;  ++loc_iter1) {
        ENa_strand strand1 = loc_iter1.GetStrand();
        CSeq_loc_CI::TRange range1 = loc_iter1.GetRange();
        
        for (CSeq_loc_CI loc_iter2(*loc2); loc_iter2;  ++loc_iter2) {
            CSeq_loc_CI::TRange range2 = loc_iter2.GetRange();
            
            unsigned int intersect_start = max(range1.GetFrom(), range2.GetFrom());
            unsigned int intersect_stop = min(range1.GetTo(), range2.GetTo());
            if (intersect_start < intersect_stop) {
                CRef<CSeq_id> this_id(new CSeq_id());
                this_id->Assign(*loc1->GetId());
                if (first) {
                    intersect_loc = new CSeq_loc(*this_id, intersect_start, intersect_stop, strand1);
                    first = false;
                } else {
                    CSeq_loc add(*this_id, intersect_start, intersect_stop, strand1);
                    intersect_loc = Seq_loc_Add (*intersect_loc, add, CSeq_loc::fSort | CSeq_loc::fMerge_All, &(m_BH.GetScope()));
                }
            }
        }
    }
    return intersect_loc;         
}


void CAutoDefExonListClause::AddSubclause (CAutoDefFeatureClause_Base *subclause)
{
    CAutoDefFeatureClause_Base::AddSubclause(subclause);
    if (m_ClauseList.size() == 1) {
        m_ClauseLocation = Seq_loc_Add(*m_ClauseLocation, *subclause->GetLocation(), 
                                       CSeq_loc::fSort | CSeq_loc::fMerge_All, 
                                       &(m_BH.GetScope()));
    } else {
        m_ClauseLocation = SeqLocIntersect(m_ClauseLocation, subclause->GetLocation());
    }
    if (NStr::IsBlank(m_GeneName)) {
        m_GeneName = subclause->GetGeneName();
    }
    if (NStr::IsBlank(m_AlleleName)) {
        m_AlleleName = subclause->GetAlleleName();
    }
    m_GeneIsPseudo |= subclause->GetGeneIsPseudo();    
}
    

void CAutoDefExonListClause::Label(bool suppress_allele)
{
    if (m_ClauseList.size() > 2) {
        m_Description = m_ClauseList[0]->GetDescription() + " through " + m_ClauseList[m_ClauseList.size() - 1]->GetDescription();
    } else {
        m_Description = ListClauses(false, m_SuppressFinalAnd, suppress_allele);
        if (NStr::StartsWith(m_Description, "exons")) {
            m_Description = m_Description.substr(5);
        } else if (NStr::StartsWith(m_Description, "exon")) {
            m_Description = m_Description.substr(4);
        }
        NStr::TruncateSpacesInPlace(m_Description);
    }
    if (!NStr::IsBlank(m_Description)) {
        m_DescriptionChosen = true;
    }
}


// All other feature matches must be that the feature to
// go into the clause must fit inside the location of the
// other clause.
bool CAutoDefExonListClause::OkToGroupUnderByLocation(CAutoDefFeatureClause_Base *parent_clause, bool gene_cluster_opp_strand)
{
    if (parent_clause == NULL) {
        return false;
    }
    
    sequence::ECompare loc_compare = parent_clause->CompareLocation(*m_ClauseLocation);
    
    if (loc_compare == sequence::eContained || loc_compare == sequence::eSame) {
        if (parent_clause->SameStrand(*m_ClauseLocation)) {
            return true;
        } 
    }
    return false;
}


bool CAutoDefExonListClause::OkToGroupUnderByType(CAutoDefFeatureClause_Base *parent_clause)
{
    bool ok_to_group = false;
    
    if (parent_clause == NULL) {
        return false;
    }
    CSeqFeatData::ESubtype parent_subtype = parent_clause->GetMainFeatureSubtype();
    
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
    return ok_to_group;
}


// Some misc_RNA clauses have a comment that actually lists multiple
// features.  This function creates a list of each element in the
// comment.
static string kRNAMiscWords[] = {
    "internal transcribed spacer",
    "external transcribed spacer",
    "ribosomal RNA intergenic spacer",
    "ribosomal RNA",
    "intergenic spacer"
};

CAutoDefFeatureClause_Base::ERnaMiscWord CAutoDefFeatureClause_Base::x_GetRnaMiscWordType(const string& phrase)
{
    // find first of the recognized words to occur in the string
    for (size_t i = eMiscRnaWordType_InternalSpacer; i < eMiscRnaWordType_Unrecognized; i++) {
        if (NStr::Find(phrase, kRNAMiscWords[i]) != string::npos) {
            return (ERnaMiscWord)i;
        }
    }
    return eMiscRnaWordType_Unrecognized;
}

const string& CAutoDefFeatureClause_Base::x_GetRnaMiscWord(ERnaMiscWord word_type)
{
    if (word_type == eMiscRnaWordType_Unrecognized) {
        return kEmptyStr;
    } else {
        return kRNAMiscWords[word_type];
    }
}


bool CAutoDefFeatureClause_Base::x_AddOneMiscWordElement(const string& phrase, vector<string>& elements)
{
    string val = phrase;
    NStr::TruncateSpacesInPlace(val);
    ERnaMiscWord word_type = x_GetRnaMiscWordType(val);
    if (word_type == eMiscRnaWordType_Unrecognized) {
        elements.clear();
        return false;
    } else {
        elements.push_back(val);
    }
}


vector<string> CAutoDefFeatureClause_Base::GetMiscRNAElements(const string& comment)
{
    vector<string> elements;
    vector<string> parts;
    NStr::Tokenize(comment, ",", parts, NStr::eMergeDelims);
    if (parts.empty()) {
        return elements;
    }
    ITERATE(vector<string>, it, parts) {
        size_t pos = NStr::Find(*it, " and ");
        if (pos == string::npos) {
            if (!x_AddOneMiscWordElement(*it, elements)) {
                break;
            }
        } else {
            if (pos > 0) {
                if (!x_AddOneMiscWordElement((*it).substr(0, pos), elements)) {
                    break;
                }
            }
            if (!x_AddOneMiscWordElement((*it).substr(pos + 5), elements)) {
                break;
            }
        }
    }
    return elements;
}


END_SCOPE(objects)
END_NCBI_SCOPE
