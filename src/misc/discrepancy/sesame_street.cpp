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
 * Authors: Sema Kachalo
 *
 */

#include <ncbi_pch.hpp>
#include "discrepancy_core.hpp"
#include "utils.hpp"
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/SubSource.hpp>
//#include <objects/seqfeat/Imp_feat.hpp>
//#include <objects/seqfeat/SeqFeatXref.hpp>
//#include <objects/macro/String_constraint.hpp>
//#include <objects/misc/sequence_util_macros.hpp>
//#include <objects/seq/seqport_util.hpp>
//#include <objmgr/bioseq_ci.hpp>
//#include <objmgr/feat_ci.hpp>
//#include <objmgr/seq_vector.hpp>
//#include <objmgr/util/feature.hpp>
//#include <objmgr/util/sequence.hpp>
//#include <sstream>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE;
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(sesame_street);

// Some animals are more equal than others...


DISCREPANCY_CASE(SOURCE_QUALS, CBioSource, eNone, "Some animals are more equal than others...")
{
    CRef<CDiscrepancyObject> disc_obj(new CDiscrepancyObject(context.GetCurrentBioseq(), context.GetScope(), context.GetFile(), context.GetKeepRef()));
    m_Objs["all"].Add(*disc_obj);
    if (obj.CanGetGenome()) {
        m_Objs["location"][context.GetGenomeName(obj.GetGenome())].Add(*disc_obj);
    }
    if (obj.CanGetOrg()) {
        const COrg_ref& org_ref = obj.GetOrg();
        if (org_ref.CanGetTaxname()) {
            m_Objs["taxname"][org_ref.GetTaxname()].Add(*disc_obj);
        }
        m_Objs["taxid"][NStr::IntToString(org_ref.GetTaxId())].Add(*disc_obj);
    }
    if (obj.CanGetSubtype()) {
        ITERATE(list<CRef<CSubSource> >, it, obj.GetSubtype()) {
            if ((*it)->CanGetName()) {
                m_Objs[(*it)->GetSubtypeName((*it)->GetSubtype(), CSubSource::eVocabulary_insdc)][(*it)->GetName()].Add(*disc_obj);
            }
        }
    }
    if (obj.IsSetOrgMod()) {
        ITERATE(list<CRef<COrgMod> >, it, obj.GetOrgname().GetMod()) {
            m_Objs[(*it)->GetSubtypeName((*it)->GetSubtype(), COrgMod::eVocabulary_insdc)][(*it)->GetSubname()].Add(*disc_obj);
        }
    }
}


class CSourseQualsAutofixData : public CObject
{
public:
    string m_Qualifier;
    mutable string m_Value;
    vector<string> m_Choice;
    mutable bool m_Ask;
    void* m_User;
    CSourseQualsAutofixData() : m_Ask(0), m_User(0) {}
};
typedef map<const CReportObj*, CRef<CReportObj> > TReportObjPtrMap;
typedef map<string, vector<CRef<CReportObj> > > TStringObjVectorMap;
typedef map<string, TStringObjVectorMap > TStringStringObjVectorMap;


DISCREPANCY_SUMMARIZE(SOURCE_QUALS)
{
    CReportNode report;
    CReportNode::TNodeMap& the_map = m_Objs.GetMap();
    TReportObjectList& all = m_Objs["all"].GetObjects();
    size_t total = all.size();
    TReportObjPtrMap all_missing;
    ITERATE (TReportObjectList, it, all) {
        all_missing[it->GetPointer()] = *it;
    }

    NON_CONST_ITERATE(CReportNode::TNodeMap, it, the_map) {
        if (it->first == "all") {
            continue;
        }
        string qual = it->first;
        size_t bins = 0;
        size_t uniq = 0;
        size_t num = 0;
        TReportObjPtrMap missing = all_missing;
        CReportNode::TNodeMap& sub = it->second->GetMap();
        TStringStringObjVectorMap capital;
        NON_CONST_ITERATE (CReportNode::TNodeMap, jj, sub) {
            TReportObjectList& obj = jj->second->GetObjects();
            bins++;
            num += obj.size();
            uniq += obj.size() == 1 ? 1 : 0;
            string upper = jj->first;
            upper = NStr::ToUpper(upper);
            ITERATE(TReportObjectList, o, obj) {
                missing.erase(o->GetPointer());
                capital[upper][jj->first].push_back(*o);
            }
        }
        string diagnosis = it->first;
        diagnosis += " (";
        diagnosis += num == total ? "all present" : "some missing";
        diagnosis += ", ";
        diagnosis += bins == 1 ? "all same" : uniq == num ? "all unique" : "some duplicates";
        diagnosis += ")";
        report[diagnosis];
        if ((num != total || bins != 1) && (it->first == "collection_date" || it->first == "country" || it->first == "isolation-source" || it->first == "strain")) {
            report[diagnosis].Fatal();
        }
        if ((bins > capital.size() || (num < total && capital.size() == 1))
            && (true || it->first == "country" || it->first == "collection_date" || it->first == "isolation_source")) { // autofixable
            CRef<CSourseQualsAutofixData> fix;
            if (bins > capital.size()) { // capitalization
                ITERATE (TStringStringObjVectorMap, cap, capital) {
                    if (cap->second.size() < 2) {
                        continue;
                    }
                    const TStringObjVectorMap& mmm = cap->second;
                    size_t best_count = 0;
                    string best_str;
                    fix.Reset(new CSourseQualsAutofixData);
                    fix->m_Qualifier = it->first;
                    fix->m_User = context.GetUserData();
                    ITERATE (TStringObjVectorMap, x, mmm) {
                        fix->m_Choice.push_back(x->first);
                        if (best_count < x->second.size()) {
                            best_count = x->second.size();
                            fix->m_Value = best_str;
                        }
                    }
                    ITERATE (TStringObjVectorMap, x, mmm) {
                        const vector<CRef<CReportObj> >& v = x->second;
                        ITERATE (vector<CRef<CReportObj> >, o, v) {
                            report[diagnosis]["[n] biosource[s] may have inconsistent capitalization: " + it->first + " (" + x->first + ")"].Add(*((const CDiscrepancyObject&)**o).Clone(true, CRef<CObject>(fix.GetNCPointer())));
                        }
                    }
                }
            }
            if (num < total && capital.size() == 1) { // some missing all same
                if (num / (float)total >= context.GetSesameStreetCutoff()) { // autofixable
                    if (fix.IsNull()) {
                        fix.Reset(new CSourseQualsAutofixData);
                        fix->m_Qualifier = it->first;
                        fix->m_Value = sub.begin()->first;
                        fix->m_User = context.GetUserData();
                    }
                    ITERATE (TReportObjPtrMap, o, missing) {
                        report[diagnosis]["[n] biosource[s] may have missing qualifier: " + it->first + " (" + sub.begin()->first + ")"].Add(*((const CDiscrepancyObject&)*o->second).Clone(true, CRef<CObject>(fix.GetNCPointer())));
                    }
                }
            }
        }
    }
    m_ReportItems = report.Export(*this)->GetSubitems();
}


DISCREPANCY_AUTOFIX(SOURCE_QUALS)
{
    TReportObjectList list = item->GetDetails();
    const CSourseQualsAutofixData* fix = 0;
    NON_CONST_ITERATE (TReportObjectList, it, list) {
        CDiscrepancyObject& obj = *dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer());
        const CBioseq* bs = dynamic_cast<const CBioseq*>(obj.GetObject().GetPointer());
        const CBioSource* biosrc = sequence::GetBioSource(*bs);
        //if (biosrc) {
        //GetSrcQualValue4FieldType(*biosrc, field_type.GetSource_qual(), &str_cons);
        //}

        //CRef<CBioSource> new_bs(new CBioSource());
        //new_bs->Assign(*biosrc);
        //new_feat->SetData().SetRna().SetExt().SetName(rrna_standard_name[i]);
        //CBioSource_EditHandle eh(scope.GetBioSourceHandle(*biosrc));
        //bseh.Replace(*new_bs);


        if (!fix) {
            fix = dynamic_cast<const CSourseQualsAutofixData*>(obj.GetMoreInfo().GetPointer());
            CAutofixHookRegularArguments arg;
            arg.m_User = fix->m_User;
            if (m_Hook) {
                m_Hook(&arg);
            }
        }


        if (fix->m_Qualifier == "host") {

        }
    }
    return CRef<CAutofixReport>(new CAutofixReport("SOURCE_QUALS: [n] missing qualifier[s] " + fix->m_Qualifier + " (" + fix->m_Value + ") added", list.size()));
}


DISCREPANCY_ALIAS(SOURCE_QUALS, SOURCE_QUALS_ASNDISC)


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
