/*  $Id$
 * =========================================================================
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
 * =========================================================================
 *
 * Author:  Colleen Bollin
 *
 * File Description:
 *   class for storing tests for Discrepancy Report
 *
 * $Log:$ 
 */

#include <ncbi_pch.hpp>

#include <misc/discrepancy_report/src_qual_problem.hpp>

#include <objmgr/bioseq_ci.hpp>

#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/PCRReactionSet.hpp>
#include <objects/seqfeat/PCRReaction.hpp>
#include <objects/seqfeat/PCRPrimerSet.hpp>
#include <objects/seqfeat/PCRPrimer.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objmgr/seqdesc_ci.hpp>


BEGIN_NCBI_SCOPE;

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(DiscRepNmSpc);




CSrcQualProblem::CSrcQualProblem()
{
}

void CSrcQualProblem::CollectData(CSeq_entry_Handle seh, const CReportMetadata& metadata)
{
    string filename = metadata.GetFilename();
    size_t index = m_Objs.size();
    CBioseq_CI bi(seh, CSeq_inst::eMol_na);
    while (bi) {
        CSeqdesc_CI di(*bi, CSeqdesc::e_Source);
        while (di) {
            x_AddValuesFromSrc(di->GetSource(), index);            
            CRef<CReportObject> obj(new CReportObject(CConstRef<CObject>(&(*di))));
            obj->SetFilename(filename);
            m_Objs.push_back(obj);
            index++;
            ++di;
        }
        ++bi;
    }
}


void CSrcQualProblem::CollateData()
{
    NON_CONST_ITERATE(TQualReportList, it, m_QualReports) {
        for(size_t i = 0; i < m_Objs.size(); i++) {
            it->second->CheckForMissing(i);
        }
    }
}


void CSrcQualProblem::ResetData()
{
    m_QualReports.clear();
    m_MultiQualItems.clear();
    m_Objs.clear();
}


TReportItemList CSrcQualProblem::GetReportItems(CReportConfig& cfg) const
{
    TReportItemList list;

    if (cfg.IsEnabled(kDISC_SRC_QUAL_PROBLEM)) {
        ITERATE(TQualReportList, it, m_QualReports) {
            list.push_back(it->second->MakeReportItem(kDISC_SRC_QUAL_PROBLEM, m_Objs));
        }
    } else if (cfg.IsEnabled(kDISC_SOURCE_QUALS_ASNDISC)) {
        ITERATE(TQualReportList, it, m_QualReports) {
            list.push_back(it->second->MakeReportItem(kDISC_SOURCE_QUALS_ASNDISC, m_Objs));
        }
    }

    if (cfg.IsEnabled(kDISC_DUP_SRC_QUAL) && m_MultiQualItems.size() > 0) { 
        CRef<CReportItem> item(new CReportItem());
        item->SetSettingName(kDISC_DUP_SRC_QUAL);
        ITERATE(TMultiQualItemList, it, m_MultiQualItems) {
            CRef<CReportItem> sub(new CReportItem());
            sub->SetSettingName(kDISC_DUP_SRC_QUAL);
            sub->SetObjects().push_back(m_Objs[it->second]);
            item->SetObjects().push_back(m_Objs[it->second]);
            sub->SetText("BioSource has value '" + 
                                  it->first.first +
                                  "' for these qualifiers: " + 
                                  it->first.second);
            item->SetSubitems().push_back(sub);
        }
        item->SetTextWithHas("source", "two or more qualifiers with the same value");
        list.push_back(item);
    }

    return list;
}


vector<string> CSrcQualProblem::Handles() const
{
    vector<string> list;
    list.push_back(kDISC_SRC_QUAL_PROBLEM);
    list.push_back(kDISC_MISSING_SRC_QUAL);
    list.push_back(kDISC_SOURCE_QUALS_ASNDISC);
    list.push_back(kDISC_DUP_SRC_QUAL);
    return list;
}


void CSrcQualProblem::DropReferences(CScope& scope)
{
    NON_CONST_ITERATE(TReportObjectList, it, m_Objs) {
        (*it)->DropReference(scope);
    }
}


void CSrcQualProblem::x_AddToMaps(const string& qual_name, const string& qual_val, 
                                  TSameValDiffQualList& vals_seen, 
                                  size_t idx)
{
    if (!m_QualReports[qual_name]) {
        m_QualReports[qual_name] = new CSrcQualItem(qual_name);
    }
    m_QualReports[qual_name]->AddValue(qual_val, idx);
    vals_seen[qual_val].push_back(qual_name);
}

void CSrcQualProblem::x_AddValuesFromSrc(const CBioSource& src, size_t index)
{
    TSameValDiffQualList values_seen;

    if ( src.IsSetTaxname() && !NStr::IsBlank(src.GetTaxname())) {
        x_AddToMaps("taxname", src.GetTaxname(), values_seen, index);
    }
    if ( int taxid = src.GetOrg().GetTaxId() ) {
        x_AddToMaps("taxid", NStr::NumericToString(taxid), values_seen, index);
    }

    // add subtypes & orgmods
    if ( src.IsSetSubtype() ) {
        ITERATE (list <CRef <CSubSource> >, jt, src.GetSubtype()) {
            string value = (*jt)->GetName();
            CSubSource::TSubtype subtype = (*jt)->GetSubtype();
            if (CSubSource::NeedsNoText(subtype) && NStr::IsBlank(value)) {
                value = "TRUE";
            }
            x_AddToMaps((*jt)->GetSubtypeName(subtype, 
                                            CSubSource::eVocabulary_insdc),
                        value, 
                        values_seen, index);
        }
    }

    if ( src.IsSetOrgMod() ) {
        ITERATE (list <CRef <COrgMod> >, jt, src.GetOrgname().GetMod() ) {
            string qual_name = (*jt)->GetSubtypeName((*jt)->GetSubtype(), 
                                            COrgMod::eVocabulary_insdc);
            if (qual_name != "old_name" 
                    && qual_name != "old_lineage" && qual_name != "gb_acronym"
                    && qual_name != "gb_anamorph" && qual_name != "gb_synonym") {
                x_AddToMaps(qual_name,
                            (*jt)->GetSubname(), 
                            values_seen, index);
            }
        }
    }
   
    // add PCR primers
    if ( src.IsSetPcr_primers() ) {
        ITERATE (list <CRef <CPCRReaction> >, jt, src.GetPcr_primers().Get()) {
            if ( (*jt)->IsSetForward() ) {
                ITERATE (list <CRef <CPCRPrimer> >, kt,(*jt)->GetForward().Get()){
                    if ( (*kt)->IsSetName() && !NStr::IsBlank((*kt)->GetName().Get())) {
                        x_AddToMaps("fwd_primer_name",
                            (*kt)->GetName().Get(), 
                            values_seen, index);
                    }
                    if ( (*kt)->IsSetSeq() && !NStr::IsBlank((*kt)->GetSeq().Get())) {
                        x_AddToMaps("fwd_primer_seq",
                            (*kt)->GetSeq().Get(), 
                            values_seen, index);
                    }
                }
            }
            if ( (*jt)->IsSetReverse() ) {
                ITERATE (list <CRef <CPCRPrimer> >, kt,(*jt)->GetReverse().Get()){
                    if ( (*kt)->IsSetName() && !NStr::IsBlank((*kt)->GetName().Get())) {
                        x_AddToMaps("rev_primer_name",
                            (*kt)->GetName().Get(), 
                            values_seen, index);
                    }
                    if ( (*kt)->IsSetSeq() && !NStr::IsBlank((*kt)->GetSeq().Get())) {
                        x_AddToMaps("rev_primer_seq",
                            (*kt)->GetSeq().Get(), 
                            values_seen, index);
                    }
                }
            }
        }
    }
    
    // genomic
    if (src.IsSetGenome() && src.GetGenome() != CBioSource::eGenome_unknown) {
        x_AddToMaps("location",
            CBioSource::GetOrganelleByGenome(src.GetGenome()), 
            values_seen, index);
    }

    ITERATE(TSameValDiffQualList, it, values_seen) {
        // add values to map
        if (it->second.size() > 1) {
            string qual_list = NStr::Join(it->second, ", ");
            m_MultiQualItems.push_back(
                    TMultiQualItem(TMultiQualDesc(it->first, qual_list),
                                    index));
        }
    }

}


void CSrcQualItem::AddValue(const string& val, size_t obj_num)
{ 
    m_ObjsPerValue[val].push_back(obj_num);
    m_ValuesPerObj[obj_num].push_back(val);

    if (m_ObjsPerValue[val].size() > 1) {
        m_AllUnique = false;
    }
    if (m_ObjsPerValue.size() > 1) {
        m_AllSame = false;
    }
    if (m_ValuesPerObj[obj_num].size() > 1) {
        m_SomeMulti = true;
    }
}


void CSrcQualItem::CheckForMissing(size_t obj_num)
{
    if (m_ValuesPerObj.find(obj_num) == m_ValuesPerObj.end()) {
        AddMissing(obj_num);
    }
}


CRef<CReportItem> CSrcQualItem::MakeReportItem(const string& setting_name, const TReportObjectList& objs) const
{
    CRef<CReportItem> item(new CReportItem());
    item->SetSettingName(setting_name);

    string description = m_QualName;

    if (m_AllPresent) {
        description += " (all present, ";
    } else {
        // some are missing
        description += " (some missing, ";
             
        // add subcategory for missing
        CRef<CReportItem> sub(new CReportItem());
        sub->SetSettingName(setting_name);
        x_AddItemsFromList(*sub, m_Missing, objs);
        sub->SetTextWithIs("source", "missing " + m_QualName);
        item->SetSubitems().push_back(sub);
    }
    if (m_AllUnique) {
        description += "all unique";
        if (m_AllPresent) {
            // add items directly
            ITERATE(TObjToValMap, it, m_ValuesPerObj) {
                item->SetObjects().push_back(objs[it->first]);
            }
        }
    }
    else if (m_AllSame) {
        description += "all same";
        // add subcategory to give value
        CRef<CReportItem> sub(new CReportItem());
        sub->SetSettingName(setting_name);
        x_AddItemsFromList(*sub, m_ObjsPerValue.begin()->second, objs);
        sub->SetTextWithHas("source", 
            "'" + m_ObjsPerValue.begin()->first + "' for " + m_QualName);
        item->SetSubitems().push_back(sub);
    }
    else {
        description += "some duplicate";
        // add one subcategory per value with more than one object
        ITERATE(TValToObjMap, it, m_ObjsPerValue) {
            if (it->second.size() > 1) {
                CRef<CReportItem> sub(new CReportItem());
                sub->SetSettingName(setting_name);
                x_AddItemsFromList(*sub, it->second, objs);
                sub->SetTextWithHas("source",
                     "'" + it->first + "' for " + m_QualName);
                item->SetSubitems().push_back(sub);
            }
        }
    }

    // add multi
    if (m_SomeMulti) {
        description += ", some multi";
        // add subcategory for multi
        CRef<CReportItem> sub(new CReportItem());
        sub->SetSettingName(setting_name);
        ITERATE(TObjToValMap, it, m_ValuesPerObj) {
            if (it->second.size() > 1) {
                sub->SetObjects().push_back(objs[it->first]);
            }
        }
        sub->SetTextWithHas("source",
                            "multiple " + m_QualName + " qualifiers");
        item->SetSubitems().push_back(sub);
    }

    // add unique
    if (!m_AllPresent || !m_AllUnique) {
        CRef<CReportItem> sub_uni(new CReportItem());
        TReportedObj already_seen;
        ITERATE(TValToObjMap, it, m_ObjsPerValue) {
            if (it->second.size() == 1 && !already_seen[it->second[0]]) {
                x_AddItemsFromList(*sub_uni, it->second, objs);
                already_seen[it->second[0]] = true;
            }
        }
        if (sub_uni->GetObjects().size() > 0) {
            sub_uni->SetSettingName(setting_name);
            sub_uni->SetTextWithHas("source", "unique values for " + m_QualName);
            item->SetSubitems().push_back(sub_uni);
        }
    }

    description += ")";

    item->SetText(description);
    return item;
}


void CSrcQualItem::x_AddItemsFromList(CReportItem& item, const vector<size_t>& list, const TReportObjectList& objs) const
{
    ITERATE(vector<size_t>, it, list) {
        item.SetObjects().push_back(objs[*it]);
    }
}



END_NCBI_SCOPE
