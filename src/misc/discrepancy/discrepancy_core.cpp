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
#include <algorithm>
#include <sstream>
#include <objmgr/object_manager.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <serial/objcopy.hpp>
#include <util/compress/stream_util.hpp>
#include <util/format_guess.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

CSafeStatic<map<string, CDiscrepancyConstructor*> > CDiscrepancyConstructor::sm_Table;
CSafeStatic<map<string, string> > CDiscrepancyConstructor::sm_DescrTable;
CSafeStatic<map<string, TGroup> > CDiscrepancyConstructor::sm_GroupTable;
CSafeStatic<map<string, string> > CDiscrepancyConstructor::sm_AliasTable;
CSafeStatic<map<string, vector<string> > > CDiscrepancyConstructor::sm_AliasListTable;


string CDiscrepancyConstructor::GetDiscrepancyCaseName(const string& name)
{
    map<string, CDiscrepancyConstructor*>& Table = GetTable();
    map<string, string>& AliasTable = GetAliasTable();
    if (Table.find(name) != Table.end()) {
        return name;
    }
    if (AliasTable.find(name) != AliasTable.end()) {
        return AliasTable[name];
    }
    if (name.substr(0, 5) == "DISC_") {
        return GetDiscrepancyCaseName(name.substr(5));
    }
    return "";
}


const CDiscrepancyConstructor* CDiscrepancyConstructor::GetDiscrepancyConstructor(const string& name)
{
    string str = GetDiscrepancyCaseName(name);
    return str.empty() ? nullptr : GetTable()[str];
}


string GetDiscrepancyCaseName(const string& name)
{
    return CDiscrepancyConstructor::GetDiscrepancyCaseName(name);
}


string GetDiscrepancyDescr(const string& name)
{
    string str = GetDiscrepancyCaseName(name);
    return str.empty() ? "" : CDiscrepancyConstructor::GetDescrTable()[str];
}


TGroup GetDiscrepancyGroup(const string& name)
{
    string str = GetDiscrepancyCaseName(name);
    return str.empty() ? 0 : CDiscrepancyConstructor::GetGroupTable()[str];
}


vector<string> GetDiscrepancyNames(TGroup group)
{
    map<string, CDiscrepancyConstructor*>& Table = CDiscrepancyConstructor::GetTable();
    map<string, TGroup>& Group = CDiscrepancyConstructor::GetGroupTable();
    vector<string> V;
    for (const auto& J : Table) {
        if (J.first[0] != '_' && (Group[J.first] & group) == group) {
            V.push_back(J.first);
        }
    }
    return V;
}


vector<string> GetDiscrepancyAliases(const string& name)
{
    map<string, vector<string> >& AliasListTable = CDiscrepancyConstructor::GetAliasListTable();
    return AliasListTable.find(name)!=AliasListTable.end() ? AliasListTable[name] : vector<string>();
}


CReportNode& CReportNode::operator[](const string& name)
{
    if (m_Map.find(name) == m_Map.end()) {
        m_Map[name] = CRef<CReportNode>(new CReportNode(name));
    }
    return *m_Map[name];
}


void CReportNode::Add(TReportObjectList& list, TReportObjectSet& hash, CReportObj& obj, bool unique)
{
    // BIG FILE
    if (unique && hash.find(&obj) != hash.end()) {
        return;
    }
    list.push_back(CRef<CReportObj>(&obj));
    hash.insert(&obj);
}


void CReportNode::Add(TReportObjectList& list, TReportObjectSet& hash, TReportObjectList& objs, bool unique)
{
    for (auto& it : objs) {
        Add(list, hash, *it, unique);
    }
}


void CReportNode::Copy(CRef<CReportNode> other)
{
    m_Map = other->m_Map;
    m_Objs = other->m_Objs;
    m_Hash = other->m_Hash;
    m_Severity = other->m_Severity;
    m_Autofix = other->m_Autofix;
    m_Ext = other->m_Ext;
    m_Summ = other->m_Summ;
    m_NoRec = other->m_NoRec;
}


bool CReportNode::Promote()
{
    if (m_Map.size() == 1) {
        Copy(m_Map.begin()->second);
        return true;
    }
    return false;
}


CRef<CReportItem> CReportNode::Export(CDiscrepancyCase& test, bool unique) const
{
    TReportObjectList objs = m_Objs;
    TReportObjectSet hash = m_Hash;
    TReportItemList subs;
    bool autofix = false;
    CReportItem::ESeverity severity = m_Severity;
    string unit;
    for (const auto& it : m_Map) {
        CRef<CReportItem> sub = it.second->Export(test, unique);
        if (severity < it.second->m_Severity) {
            severity = it.second->m_Severity;
        }
        if (severity < sub->GetSeverity()) {
            severity = sub->GetSeverity();
        }
        autofix |= sub->CanAutofix();
        if (unit.empty()) {
            unit = sub->GetUnit();
        }
        subs.push_back(sub);
        if (!m_NoRec) {
            TReportObjectList details = sub->GetDetails();
            for (auto& ob : details) {
                Add(objs, hash, *ob, unique);
            }
        }
    }
    for (auto& ob : objs) {
        if (ob->CanAutofix()) {
            static_cast<CDiscrepancyObject&>(*ob).m_Case.Reset(&test);
            autofix = true;
        }
    }
    string str = m_Name;
    NStr::TruncateSpacesInPlace(str);
    for (size_t n = NStr::Find(str, "[*"); n != NPOS; n = NStr::Find(str, "[*")) {
        size_t k = NStr::Find(str, "*]");
        if (k != NPOS) {
            str.erase(n, k - n + 2);
        }
        else {
            str.erase(n);
        }
    }
    string msg = str;
    string xml = str;
    size_t count = m_Count ? m_Count : objs.size();

    NStr::ReplaceInPlace(msg, "[n]", NStr::Int8ToString(count));
    NStr::ReplaceInPlace(msg, "[n/2]", NStr::Int8ToString(count / 2));
    NStr::ReplaceInPlace(msg, "[s]", count == 1 ? "" : "s");  // nouns
    NStr::ReplaceInPlace(msg, "[S]", count == 1 ? "s" : "");  // verbs
    NStr::ReplaceInPlace(msg, "[is]", count == 1 ? "is" : "are");
    NStr::ReplaceInPlace(msg, "[does]", count == 1 ? "does" : "do");
    NStr::ReplaceInPlace(msg, "[has]", count == 1 ? "has" : "have");
    NStr::ReplaceInPlace(msg, "[(]", "");
    NStr::ReplaceInPlace(msg, "[)]", "");

    NStr::ReplaceInPlace(xml, "[n]", "##");
    NStr::ReplaceInPlace(xml, "[n/2]", "##");
    NStr::ReplaceInPlace(xml, "[s]", "s");
    NStr::ReplaceInPlace(xml, "[S]", "");
    NStr::ReplaceInPlace(xml, "[is]", "are");
    NStr::ReplaceInPlace(xml, "[does]", "do");
    NStr::ReplaceInPlace(xml, "[has]", "have");
    NStr::ReplaceInPlace(xml, "[(]", "");
    NStr::ReplaceInPlace(xml, "[)]", "");

    size_t n = str.find("[n]");
    if (n != string::npos) {
        str = str.substr(n + 4);
    }
    else if ((n = str.find("[n/2]")) != string::npos) {
        str = str.substr(n + 6);
        count /= 2;
    }
    if (n != string::npos) {
        if ((n = str.find("[s]")) != string::npos) {
            unit = str.substr(0, n);
        }
        else if (!str.find("CDS ")) {
            unit = "CDS";
        }
        else if ((n = str.find("s ")) != string::npos) {
            unit = str.substr(0, n);
        }
    }
	CRef<CDiscrepancyItem> item(new CDiscrepancyItem(test, m_Name, msg, xml, unit, count));
    item->m_Autofix = autofix;
    item->m_Severity = severity;
    item->m_Ext = m_Ext;
    item->m_Summ = m_Summ;
    item->m_Subs = subs;
    item->m_Objs = objs;
    return CRef<CReportItem>((CReportItem*)item);
}


TReportObjectList CDiscrepancyCore::GetObjects(void) const
{
    TReportObjectList ret;
    TReportObjectSet hash;
    TReportItemList items = GetReport();
    for (const auto& rep : items) {
        TReportObjectList objs = rep->GetDetails();
        for (auto& obj : objs) {
            CReportNode::Add(ret, hash, *obj);
        }
    }
    return ret;
}


CRef<CReportItem> CReportItem::CreateReportItem(const string& test, const CReportObj& obj, const string& msg, bool autofix)
{
    CRef<CDiscrepancyCase> t = CDiscrepancyConstructor::GetDiscrepancyConstructor(test)->Create();
    string s = msg;
    NStr::ReplaceInPlace(s, "[(]", "");
    NStr::ReplaceInPlace(s, "[)]", "");
    CRef<CDiscrepancyItem> item(new CDiscrepancyItem(*t, msg, s, s, kEmptyCStr, 0));
    item->m_Autofix = autofix;
    auto dobj = static_cast<const CDiscrepancyObject&>(obj);
    auto x = CRef<CDiscrepancyObject>(new CDiscrepancyObject(dobj.m_Ref));
    x->m_Case = t;
    if (autofix) {
        x->m_Fix = dobj.m_Ref;
    }
    item->m_Objs.push_back(CRef<CReportObj>(x));
    return CRef<CReportItem>((CReportItem*)item);
}


// need to rewrite as a DiscrepancyContext method
CReportObj* CDiscrepancyObject::Clone(bool fix, CConstRef<CObject> data) const
{
    CDiscrepancyObject* obj = new CDiscrepancyObject(*this);
    if (fix) {
        obj->m_Fix.Reset(obj->m_Ref);
    }
    obj->m_More = data;
    return obj;
}


template<typename T> void CDiscrepancyVisitor<T>::Call(const T& obj, CDiscrepancyContext& context)
{
    try {
        Visit(obj, context);
    }
    catch (CException& e) { // LCOV_EXCL_START
        string ss = "EXCEPTION caught: "; ss += e.what();
        m_Objs[ss];
    } // LCOV_EXCL_STOP
}


CRef<CDiscrepancySet> CDiscrepancySet::New(CScope& scope) { return CRef<CDiscrepancySet>(new CDiscrepancyContext(scope)); }


string CDiscrepancySet::Format(const string& s, unsigned int count)
{
    string str = s;
    NStr::TruncateSpacesInPlace(str);
    NStr::ReplaceInPlace(str, "[n]", NStr::Int8ToString(count));
    NStr::ReplaceInPlace(str, "[n/2]", NStr::Int8ToString(count / 2));
    NStr::ReplaceInPlace(str, "[s]", count == 1 ? "" : "s");  // nouns
    NStr::ReplaceInPlace(str, "[S]", count == 1 ? "s" : "");  // verbs
    NStr::ReplaceInPlace(str, "[is]", count == 1 ? "is" : "are");
    NStr::ReplaceInPlace(str, "[does]", count == 1 ? "does" : "do");
    NStr::ReplaceInPlace(str, "[has]", count == 1 ? "has" : "have");
    NStr::ReplaceInPlace(str, "[(]", "");
    NStr::ReplaceInPlace(str, "[)]", "");
    for (size_t n = NStr::Find(str, "[*"); n != NPOS; n = NStr::Find(str, "[*")) {
        size_t k = NStr::Find(str, "*]");
        if (k != NPOS) {
            str.erase(n, k - n + 2);
        }
        else {
            str.erase(n);
        }
    }
    return str;
}


bool CDiscrepancyContext::AddTest(const string& name)
{
    string str = GetDiscrepancyCaseName(name);
    if (str.empty()) {
        return false; // no such test
    }
    if (m_Tests.find(str) != m_Tests.end()) {
        return false;  // already there
    }
    CRef<CDiscrepancyCase> test = CDiscrepancyConstructor::GetDiscrepancyConstructor(str)->Create();
    m_Tests[str] = test;

#define REGISTER_DISCREPANCY_TYPE(type) \
    if (dynamic_cast<CDiscrepancyVisitor<type>* >(test.GetPointer())) {                         \
        m_All_##type.push_back(dynamic_cast<CDiscrepancyVisitor<type>* >(test.GetPointer()));   \
        m_Enable_##type = true;                                                                 \
        return true;                                                                            \
    }
    //REGISTER_DISCREPANCY_TYPE(CSeq_inst)
    //REGISTER_DISCREPANCY_TYPE(CSeqdesc)
    //REGISTER_DISCREPANCY_TYPE(CSeq_feat)
    //REGISTER_DISCREPANCY_TYPE(CSubmit_block)
    //REGISTER_DISCREPANCY_TYPE(CSeqFeatData)
    //REGISTER_DISCREPANCY_TYPE(CSeq_feat_BY_BIOSEQ)
    //REGISTER_DISCREPANCY_TYPE(COverlappingFeatures)
    //REGISTER_DISCREPANCY_TYPE(CBioSource)
    //REGISTER_DISCREPANCY_TYPE(COrgName)
    //REGISTER_DISCREPANCY_TYPE(CSeq_annot)
    //REGISTER_DISCREPANCY_TYPE(CPubdesc)
    //REGISTER_DISCREPANCY_TYPE(CAuth_list)
    //REGISTER_DISCREPANCY_TYPE(CPerson_id)
    REGISTER_DISCREPANCY_TYPE(string)

/// BIG FILE
REGISTER_DISCREPANCY_TYPE(SEQUENCE)
REGISTER_DISCREPANCY_TYPE(SEQ_SET)
REGISTER_DISCREPANCY_TYPE(FEAT)
REGISTER_DISCREPANCY_TYPE(DESC)
REGISTER_DISCREPANCY_TYPE(BIOSRC)
REGISTER_DISCREPANCY_TYPE(PUBDESC)
REGISTER_DISCREPANCY_TYPE(AUTHORS)
REGISTER_DISCREPANCY_TYPE(SUBMIT)
REGISTER_DISCREPANCY_TYPE(STRING)

    return false;
}


void CDiscrepancyContext::Push(const CSerialObject& root, const string& fname)
{
    const CBioseq* bs = dynamic_cast<const CBioseq*>(&root);
    const CBioseq_set* st = dynamic_cast<const CBioseq_set*>(&root);
    const CSeq_entry* se = dynamic_cast<const CSeq_entry*>(&root);
    const CSeq_submit* ss = dynamic_cast<const CSeq_submit*>(&root);

    if (!fname.empty()) {
        m_RootNode.Reset(new CParseNode(eFile, 0));
        m_RootNode->m_Ref->m_Text = fname;
    }
    else if (!m_RootNode) {
        m_RootNode.Reset(new CParseNode(eNone, 0));
    }
    m_NodeMap[m_RootNode->m_Ref] = &*m_RootNode;
    m_CurrentNode.Reset(m_RootNode);

    if (bs) {
        ParseObject(*bs);
    }
    else if (st) {
        ParseObject(*st);
    }
    else if (se) {
        ParseObject(*se);
    }
    else if (ss) {
        ParseObject(*ss);
    }
}


unsigned CDiscrepancyContext::Summarize()
{
    unsigned severity = 0;
    for (auto& tt : m_Tests) {
        CDiscrepancyCore& test = static_cast<CDiscrepancyCore&>(*tt.second);
        test.Summarize(*this);
        for (const auto& rep : test.GetReport()) {
            unsigned sev = rep->GetSeverity();
            severity = sev > severity ? sev : severity;
        }
    }
    return severity;
}


void CDiscrepancyContext::TestString(const string& str)
{
    for (auto* it : m_All_string) {
        Call(*it, str);
    }
}


TReportItemList CDiscrepancyGroup::Collect(TDiscrepancyCaseMap& tests, bool all) const
{
    TReportItemList out;
    for (const auto& it : m_List) {
        TReportItemList tmp = it->Collect(tests, false);
        for (const auto& tt : tmp) {
            out.push_back(tt);
        }
    }
    if (!m_Test.empty() && tests.find(m_Test) != tests.end()) {
        TReportItemList tmp = tests[m_Test]->GetReport();
        for (const auto& tt : tmp) {
            out.push_back(tt);
        }
        tests.erase(m_Test);
    }
    if (!m_Name.empty()) {
        TReportObjectList objs;
        TReportObjectSet hash;
        CRef<CDiscrepancyItem> di(new CDiscrepancyItem(m_Name));
        di->m_Subs = out;
        bool empty = true;
        for (const auto& tt : out) {
            TReportObjectList details = tt->GetDetails();
            if (!details.empty() || tt->GetCount()) {
                empty = false;
            }
            for (auto& ob : details) {
                CReportNode::Add(objs, hash, *ob);
            }
            if (tt->CanAutofix()) {
                di->m_Autofix = true;
            }
            if (tt->IsInfo()) {
                di->m_Severity = CDiscrepancyItem::eSeverity_info;
            }
            else if (tt->IsFatal()) {
                di->m_Severity = CDiscrepancyItem::eSeverity_error;
            }
        }
        di->m_Objs = objs;
        out.clear();
        if (!empty) {
            out.push_back(CRef<CReportItem>(di));
        }
    }
    if (all) {
        for (const auto& it : tests) {
            TReportItemList list = it.second->GetReport();
            for (const auto& it : list) {
                out.push_back(it);
            }
        }
    }
    return out;
}


void CDiscrepancyContext::RunTests()
{
    SEQUENCE dummy_seq;
    SEQ_SET dummy_set;
    FEAT dummy_feat;
    DESC dummy_desc;
    BIOSRC dummy_biosrc;
    PUBDESC dummy_pubdesc;
    AUTHORS dummy_authors;
    SUBMIT dummy_submit;
    STRING dummy_string;
    if (m_CurrentNode->m_Type == eBioseq) {
        ClearFeatureList();
        for (const auto& feat : GetAllFeat()) {
            CollectFeature(feat);
        }
        for (auto* test : m_All_SEQUENCE) {
            Call(*test, dummy_seq);
        }
        for (auto* test : m_All_FEAT) {
            Call(*test, dummy_feat);
        }
        for (auto* test : m_All_DESC) {
            Call(*test, dummy_desc);
        }
        if (!m_CurrentNode->m_Pubdescs.empty()) {
            for (auto* test : m_All_PUBDESC) {
                Call(*test, dummy_pubdesc);
            }
            for (auto* test : m_All_AUTHORS) {
                Call(*test, dummy_authors);
            }
        }
        if (m_CurrentNode->m_Biosource) {
            for (auto* test : m_All_BIOSRC) {
                Call(*test, dummy_biosrc);
            }
        }
    }
    else if (IsSeqSet(m_CurrentNode->m_Type)) {
        for (auto* test : m_All_SEQ_SET) {
            Call(*test, dummy_set);
        }
        for (auto* test : m_All_FEAT) {
            Call(*test, dummy_feat);
        }
        for (auto* test : m_All_DESC) {
            Call(*test, dummy_desc);
        }
        if (!m_CurrentNode->m_Pubdescs.empty()) {
            for (auto* test : m_All_PUBDESC) {
                Call(*test, dummy_pubdesc);
            }
            for (auto* test : m_All_AUTHORS) {
                Call(*test, dummy_authors);
            }
        }
        if (m_CurrentNode->m_Biosource) {
            for (auto* test : m_All_BIOSRC) {
                Call(*test, dummy_biosrc);
            }
        }
    }
    else if (m_CurrentNode->m_Type == eSubmit) {
        for (auto* test : m_All_SUBMIT) {
            Call(*test, dummy_submit);
        }
        if (!m_CurrentNode->m_Authors.empty()) {
            for (auto* test : m_All_AUTHORS) {
                Call(*test, dummy_authors);
            }
        }
    }
    else if (m_CurrentNode->m_Type == eString) {
        for (auto* test : m_All_STRING) {
            Call(*test, dummy_string);
        }
    }
    else if (m_CurrentNode->m_Type == eFile) {
        return;
    }
    else {
        ERR_POST(Info << "Tests for "
                 << TypeName(m_CurrentNode->m_Type)
                 << " are not yet implemented...");
    }
}


string CDiscrepancyContext::CRefNode::GetText() const
{
    if (m_Type == eBioseq) {
        size_t brk = m_Text.find('\n');
        return brk == string::npos ? m_Text : m_Text.substr(0, brk) + " " + m_Text.substr(brk + 1);
    }
    else if (IsSeqSet(m_Type)) {
        switch (m_Type) {
            case eSeqSet_NucProt:
                return "np|" + (m_Text.empty() ? "(EMPTY BIOSEQ-SET)" : m_Text);
            case eSeqSet_SegSet:
                return "ss|" + (m_Text.empty() ? "(EMPTY BIOSEQ-SET)" : m_Text);
            default:
                return m_Text.empty() ? "BioseqSet" : "Set containing " + m_Text;
        }
    }
    else if (m_Type == eSubmit) {
        return m_Text.empty() ? "Cit-sub" : "Cit-sub for " + m_Text;
    }
    else if (m_Type == eSeqDesc) {
        string label = GetBioseqLabel();
        return label.empty() ? m_Text : label + ":" + m_Text;
    }
    else if (m_Type == eSeqFeat) {
        return m_Text;
    }
    else if (m_Type == eString) {
        return m_Text;
    }
    return CDiscrepancyContext::TypeName(m_Type) + " - coming soon...";
}


string CDiscrepancyContext::CRefNode::GetBioseqLabel() const
{
    for (const CRefNode* node = this; node; node = node->m_Parent) {
        if (node->m_Type == eBioseq) {
            size_t brk = node->m_Text.find('\n');
            return brk == string::npos ? kEmptyStr : node->m_Text.substr(0, brk);
        }
        if (IsSeqSet(node->m_Type) || node->m_Type == eSubmit) {
            return node->m_Text;
        }
    }
    return kEmptyStr;
}


bool CDiscrepancyContext::CompareRefs(CRef<CReportObj> a, CRef<CReportObj> b) {
    vector<const CRefNode*> A, B;
    for (const CRefNode* node = static_cast<CDiscrepancyObject&>(*a).m_Ref; node; node = node->m_Parent) {
        A.push_back(node);
    }
    reverse(A.begin(), A.end());
    for (const CRefNode* node = static_cast<CDiscrepancyObject&>(*b).m_Ref; node; node = node->m_Parent) {
        B.push_back(node);
    }
    reverse(B.begin(), B.end());
    size_t n = min(A.size(), B.size());
    for (size_t i = 0; i < n; i++) {
        if (A[i] != B[i]) {
            if (A[i]->m_Type == eSeqFeat && B[i]->m_Type != eSeqFeat) {
                return true;
            }
            if (B[i]->m_Type == eSeqFeat && A[i]->m_Type != eSeqFeat) {
                return false;
            }
            if (A[i]->m_Type == eSeqDesc && B[i]->m_Type != eSeqDesc) {
                return true;
            }
            if (B[i]->m_Type == eSeqDesc && A[i]->m_Type != eSeqDesc) {
                return false;
            }
            if (A[i]->m_Index != B[i]->m_Index) {
                return A[i]->m_Index < B[i]->m_Index;
            }
        }
    }
    return A.size() == B.size() ? &*a < &*b : A.size() < B.size();
}


DISCREPANCY_LINK_MODULE(discrepancy_case);
DISCREPANCY_LINK_MODULE(suspect_product_names);
DISCREPANCY_LINK_MODULE(division_code_conflicts);
DISCREPANCY_LINK_MODULE(feature_per_bioseq);
DISCREPANCY_LINK_MODULE(seqdesc_per_bioseq);
DISCREPANCY_LINK_MODULE(gene_names);
DISCREPANCY_LINK_MODULE(rna_names);
DISCREPANCY_LINK_MODULE(transl_too_long);
DISCREPANCY_LINK_MODULE(overlapping_features);
DISCREPANCY_LINK_MODULE(sesame_street);
DISCREPANCY_LINK_MODULE(transl_note);
DISCREPANCY_LINK_MODULE(feature_tests);
DISCREPANCY_LINK_MODULE(sequence_tests);
DISCREPANCY_LINK_MODULE(pub_tests);
DISCREPANCY_LINK_MODULE(biosource_tests);

END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
