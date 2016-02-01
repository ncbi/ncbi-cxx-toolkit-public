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
        NON_CONST_ITERATE (CReportNode::TNodeMap, jj, sub) {
            TReportObjectList& obj = jj->second->GetObjects();
            bins++;
            num += obj.size();
            uniq += obj.size() == 1 ? 1 : 0;
            ITERATE (TReportObjectList, o, obj) {
                missing.erase(o->GetPointer());
            }
        }
        cout << ":):):) " << qual << " " << missing.size() << endl;
        string diagnosis = it->first;
        diagnosis += " (";
        diagnosis += num == total ? "all present" : "some missing";
        diagnosis += ", ";
        diagnosis += bins == 1 ? "all same" : uniq == num ? "all unique" : "some duplicates";
        diagnosis += ")";
        report[diagnosis];
        if (num < total && bins == 1) { // some missing all same
            if (num / (float)total >= context.GetSesameStreetCutoff()) { // autofixable
                CSourseQualsAutofixData* fix = new CSourseQualsAutofixData;
                CRef<CObject> more(fix);
                fix->m_Qualifier = it->first;
                fix->m_Value = sub.begin()->first;
                fix->m_User = context.GetUserData();
                ITERATE(TReportObjPtrMap, o, missing) {
                    string mq = "Missing qualifier " + it->first + " (" + sub.begin()->first + ")";
                    CConstRef<CBioseq> bs((const CBioseq*)o->second->GetObject().GetPointer());
                    report[diagnosis][mq].Add(*new CDiscrepancyObject(bs, context.GetScope(), context.GetFile(), context.GetKeepRef(), true, more));
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
    NON_CONST_ITERATE(TReportObjectList, it, list) {
        CDiscrepancyObject& obj = *dynamic_cast<CDiscrepancyObject*>((*it).GetNCPointer());
        if (!fix) {
            fix = dynamic_cast<const CSourseQualsAutofixData*>(obj.GetMoreInfo().GetPointer());
            CAutofixHookRegularArguments arg;
            arg.m_User = fix->m_User;
            if (m_Hook) {
                m_Hook(&arg);
            }
        }
    }
    return CRef<CAutofixReport>(new CAutofixReport("SOURCE_QUALS: [n] missing qualifier[s] " + fix->m_Qualifier + " (" + fix->m_Value + ") added", list.size()));
}


DISCREPANCY_ALIAS(SOURCE_QUALS, SOURCE_QUALS_ASNDISC)


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
