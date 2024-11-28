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
#include <valarray>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

class CCaseRegistry
{
public:
    static constexpr size_t num_test_cases = static_cast<size_t>(eTestNames::max_test_names);
    using TArray = std::array<const CDiscrepancyCaseProps**, num_test_cases>;

    using TAliasMap = map<string_view, eTestNames>;

protected:
    template<size_t i>
    static constexpr auto xGetProps()
    {
#ifdef NCBI_COMPILER_ANY_CLANG
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wundefined-var-template"
#endif
        return &CDiscrepancyCasePropsRef<static_cast<eTestNames>(i)>::props;
#ifdef NCBI_COMPILER_ANY_CLANG
#  pragma GCC diagnostic pop
#endif
    }

    template<std::size_t... I>
    static constexpr TArray xAssembleArray(std::index_sequence<I...>)
    {
        return {xGetProps<I>()...};
    }

public:

    static constexpr TArray PopulateTests()
    {
        return xAssembleArray(std::make_index_sequence<num_test_cases>{});
    }
    static const TTestNamesSet& GetAutofixTests()
    {
        static constexpr TTestNamesSet autofix_names{DISC_AUTOFIX_TESTNAMES};
        return autofix_names;
    }

    static const TAliasMap& GetAliasMap();
    static const CDiscrepancyCaseProps& GetProps(eTestNames name);
};

static constexpr CCaseRegistry::TArray g_test_registry = CCaseRegistry::PopulateTests();

static CCaseRegistry::TAliasMap xPopulateAliases()
{
    CCaseRegistry::TAliasMap s_alias_map;
    for (size_t i=0; i<g_test_registry.size(); ++i) {
        auto aliases = (**g_test_registry[i]).Aliases;
        if (aliases)
            for (auto alias_name: *aliases) {
                s_alias_map[alias_name] = static_cast<eTestNames>(i);
            }
    }
    return s_alias_map;
}


const CCaseRegistry::TAliasMap& CCaseRegistry::GetAliasMap()
{
    static TAliasMap g_alias_map = xPopulateAliases();
    return g_alias_map;
}

const CDiscrepancyCaseProps& CCaseRegistry::GetProps(eTestNames name)
{
    if (name < eTestNames::max_test_names) {
        auto prop_ref = g_test_registry[static_cast<size_t>(name)];
        if (prop_ref && *prop_ref) {
            const CDiscrepancyCaseProps& props = **prop_ref;
            return props;
        }
    }
    throw std::out_of_range("eTestNames");
}

std::ostream& operator<<(std::ostream& str, NDiscrepancy::eTestNames name)
{
    str << GetDiscrepancyCaseName(name);
    return str;
}

eTestNames GetDiscrepancyCaseName(string_view name)
{
    for (size_t i=0; i<g_test_registry.size(); ++i) {
        const CDiscrepancyCaseProps& props = **(g_test_registry[i]);
        if (props.sName == name)
            return static_cast<eTestNames>(i);
    }

    auto it = CCaseRegistry::GetAliasMap().find(name);
    if (it != CCaseRegistry::GetAliasMap().end())
        return it->second;

    if (NStr::StartsWith(name, "DISC_")) {
        return GetDiscrepancyCaseName(name.substr(5));
    }

    return eTestNames::notset;
}

string_view GetDiscrepancyCaseName(eTestNames name)
{
    return CCaseRegistry::GetProps(name).sName;
}

vector<string_view> GetDiscrepancyAliases(eTestNames name)
{
    vector<string_view> V;
    auto alias_names = CCaseRegistry::GetProps(name).Aliases;
    if (alias_names) {
        V.reserve(alias_names->size());
        for (auto rec: *alias_names)
        {
            V.push_back(rec);
        }
    }
    return V;
}


string_view GetDiscrepancyDescr(string_view name)
{
    return GetDiscrepancyDescr(GetDiscrepancyCaseName(name));
}

string_view GetDiscrepancyDescr(eTestNames name)
{
    return CCaseRegistry::GetProps(name).Descr;
}

TGroup GetDiscrepancyGroup(eTestNames name)
{
    return CCaseRegistry::GetProps(name).Group;
}

TGroup GetDiscrepancyGroup(string_view name)
{
    return GetDiscrepancyGroup(GetDiscrepancyCaseName(name));
}

vector<string> GetDiscrepancyNames(TGroup group)
{
    auto tests = GetDiscrepancyTests(group);
    vector<string> names; names.reserve(tests.size());
    for (auto tn: tests) {
        names.push_back(std::string(GetDiscrepancyCaseName(tn)));
    }

    return names;
}

TTestNamesSet GetDiscrepancyTests(TGroup group)
{
    TTestNamesSet names;
    if (group == eAutofix) {
        names = CCaseRegistry::GetAutofixTests();
    } else {
        for (auto rec: g_test_registry)
        {
            auto props = *rec;
            if (props->sName[0] != '_' && (props->Group & group) == group) {
                names.set(props->Name);
            }
        }
    }
    return names;
}


CReportNode& CReportNode::operator[](const string& name)
{
    auto& node = m_Map[name];
    if (!node)
        node = Ref(new CReportNode(name));
    return *node;
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

CReportNode& CReportNode::Merge(CReportNode& other)
{
    _ASSERT(m_Name == other.m_Name);
    #if 0
    if (!m_Name.empty())
    std::cerr << m_Name
        << ":" << m_Count << ":" << other.m_Count
        << ":" << m_Map.size() << ":" << other.m_Map.size()
        << ":" << m_Objs.size() << ":" << other.m_Objs.size()
        << ":" << m_Hash.size() << ":" << other.m_Hash.size()
        << "\n";
    #endif

    for (auto it: other.m_Map) {
        auto& rec = m_Map[it.first];
        if (rec)
            rec->Merge(*it.second);
        else
            rec = it.second;
    }

    m_Count += other.m_Count;
    m_Objs.insert(m_Objs.end(), other.m_Objs.begin(), other.m_Objs.end());
    m_Hash.merge(other.m_Hash);

    return *this;
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
        CRef<CReportNode> other = m_Map.begin()->second;
        Copy(other);
        return true;
    }
    return false;
}


CRef<CReportItem> CReportNode::Export(CDiscrepancyCore& test, bool unique) const
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
        autofix = autofix || sub->CanAutofix();
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
    size_t count = m_Count > 0 ? m_Count : objs.size();

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
        else if (0 == str.find("CDS ")) {
            unit = "CDS";
        }
        else if ((n = str.find("s ")) != string::npos) {
            unit = str.substr(0, n);
        }
    }
    CRef<CDiscrepancyItem> item(new CDiscrepancyItem(test.GetSName(), m_Name, msg, xml, unit, count));
    item->m_Autofix = autofix;
    item->m_Severity = severity;
    item->m_Ext = m_Ext;
    item->m_Summ = m_Summ;
    item->m_Subs = subs;
    item->m_Objs = objs;
    return item;
}

void CDiscrepancyCore::xSummarize()
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}

TReportObjectList CDiscrepancyCore::GetObjects() const
{
    TReportObjectList ret;
    TReportObjectSet hash;
    auto items = GetReport();
    for (const auto& rep : items) {
        TReportObjectList objs = rep->GetDetails();
        for (auto& obj : objs) {
            CReportNode::Add(ret, hash, *obj);
        }
    }
    return ret;
}

CRef<CReportItem> CReportItemFactory::Create(const string& test_name, const string& name, const CReportObj& main_obj, const TReportObjectList& report_objs, bool autofix)
{
    auto test = CCaseRegistry::GetProps(GetDiscrepancyCaseName(test_name)).Constructor();
    string msg = name;
    NStr::ReplaceInPlace(msg, "[(]", "");
    NStr::ReplaceInPlace(msg, "[)]", "");

    CDiscrepancyItem* disc_item = new CDiscrepancyItem(test->GetSName(), name, msg, msg, kEmptyStr, 0);
    disc_item->SetAutofix(autofix);

    CRef<CReportObj> new_obj = CReportObjFactory::Create(test, main_obj, autofix);
    disc_item->SetDetails().push_back(new_obj);
    for (const auto& it : report_objs) {
        disc_item->SetDetails().push_back(it);
    }

    return CRef<CReportItem>(disc_item);
}

CRef<CReportObj> CReportObjFactory::Create(CRef<CDiscrepancyCore> disc_core, const CReportObj& obj, bool autofix)
{
    auto disc_obj = dynamic_cast<const CDiscrepancyObject&>(obj);
    auto ref = disc_obj.RefNode();

    CDiscrepancyObject* new_obj = CDiscrepancyObject::CreateInternal(ref.GetNCPointerOrNull(), disc_core, autofix);
    return CRef<CReportObj>(new_obj);
}


CDiscrepancyObject* CDiscrepancyObject::CreateInternal(CDiscrepancyContext::CRefNode* ref, CRef<CDiscrepancyCore> disc_core, bool autofix)
{
    CDiscrepancyObject* disc_obj = new CDiscrepancyObject(ref);
    disc_obj->m_Case = disc_core;
    if (autofix) {
        disc_obj->m_Fix = ref;
    }
    return disc_obj;
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

void CDiscrepancyCore::Call(CDiscrepancyContext& context)
{
    try {
        Visit(context);
    }
    catch (const CException& e) {
        string ss = "EXCEPTION caught: "; ss += e.what();
        m_Objs[ss];
    }
}


std::atomic<bool> CDiscrepancySet::m_Gui = false;

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

void CDiscrepancyContext::AddTest(string_view name)
{
    AddTest(GetDiscrepancyCaseName(name));
}

void CDiscrepancyContext::AddTest(eTestNames name)
{
    if (!m_Tests[name].Empty())
        return;

    auto test = CCaseRegistry::GetProps(name).Constructor();
    m_Tests[name] = test;
    m_Enabled.set(test->GetType());

#define REGISTER_DISCREPANCY_TYPE(type)                            \
    if (test->GetType() == eTestTypes::type) {                     \
        auto* p = test.GetPointer();                               \
        m_All_##type.push_back(p);                                 \
        return;                                                    \
    }

REGISTER_DISCREPANCY_TYPE(SEQUENCE)
REGISTER_DISCREPANCY_TYPE(SEQ_SET)
REGISTER_DISCREPANCY_TYPE(FEAT)
REGISTER_DISCREPANCY_TYPE(DESC)
REGISTER_DISCREPANCY_TYPE(BIOSRC)
REGISTER_DISCREPANCY_TYPE(PUBDESC)
REGISTER_DISCREPANCY_TYPE(AUTHORS)
REGISTER_DISCREPANCY_TYPE(SUBMIT)
REGISTER_DISCREPANCY_TYPE(STRING)

}


void CDiscrepancyContext::Push(const CSerialObject& root, const string& fname)
{
    if (!fname.empty()) {
        m_RootNode.Reset(new CParseNode(eFile, 0));
        m_RootNode->m_Ref->m_Text = fname;
    }
    else if (!m_RootNode) {
        m_RootNode.Reset(new CParseNode(eNone, 0));
    }
    m_NodeMap[m_RootNode->m_Ref] = &*m_RootNode;
    m_CurrentNode.Reset(m_RootNode);

    auto* pTypeInfo = root.GetThisTypeInfo();
    if (!pTypeInfo) {
        NCBI_THROW(CException, eUnknown, "Object has unknown type");
    }


    if (pTypeInfo == CBioseq::GetTypeInfo()) {
        ParseObject(*CTypeConverter<CBioseq>::SafeCast(&root));
        return;
    }

    if (pTypeInfo == CBioseq_set::GetTypeInfo()) {
        ParseObject(*CTypeConverter<CBioseq_set>::SafeCast(&root));
        return;
    }

    if (pTypeInfo == CSeq_entry::GetTypeInfo()) {
        ParseObject(*CTypeConverter<CSeq_entry>::SafeCast(&root));
        return;
    }

    if (pTypeInfo == CSeq_submit::GetTypeInfo()) {
        ParseObject(*CTypeConverter<CSeq_submit>::SafeCast(&root));
        return;
    }

    NCBI_THROW(CException, eUnknown,
            "Unsupported type - " + pTypeInfo->GetName());
}


void CDiscrepancyProductImpl::Summarize()
{
    for (auto& tt : m_Tests) {
        auto& test = *tt.second;
        test.Summarize();
    }
}

unsigned CDiscrepancyContext::Summarize()
{
    unsigned severity = 0;
    for (auto& tt : m_Tests) {
        auto& test = *tt.second;
        test.Summarize();
        for (const auto& rep : test.GetReport()) {
            unsigned sev = rep->GetSeverity();
            severity = sev > severity ? sev : severity;
        }
    }
    return severity;
}


void CDiscrepancyCore::Merge(CDiscrepancyCore& other)
{
    _ASSERT(m_ReportItems.empty());
    m_Objs.Merge(other.m_Objs);
}


void CDiscrepancyProductImpl::Merge(CDiscrepancyProduct& other)
{
    auto other_ptr = static_cast<CDiscrepancyProductImpl&>(other);
    for (auto it: other_ptr.m_Tests) {
        if (it.second && !it.second->Empty())
        {
            auto& current = m_Tests[it.first];
            if (current && !current->Empty())
                current->Merge(*it.second);
            else
                current = it.second;
        }
    }
}


#if 0
void CDiscrepancyContext::TestString(const string& str)
{
    for (auto* it : m_All_STRING) {
        Call(*it, str);
    }
}
#endif


CDiscrepancyGroup::CDiscrepancyGroup(const string& name, const string& test)
    : m_Name(name)
{
    m_Test = GetDiscrepancyCaseName(test);
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
    if (m_Test != eTestNames::notset && tests.find(m_Test) != tests.end()) {
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
            if (!details.empty() || tt->GetCount() > 0) {
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
            for (const auto& it2 : list) {
                out.push_back(it2);
            }
        }
    }

    return out;
}

void CDiscrepancyContext::RunTests()
{
    if (m_CurrentNode->m_Type == eBioseq) {
        ClearFeatureList();
        for (const auto& feat : GetAllFeat()) {
            CollectFeature(feat);
        }
        for (auto* test : m_All_SEQUENCE) {
            test->Call(*this);
        }
        for (auto* test : m_All_FEAT) {
            test->Call(*this);
        }
        for (auto* test : m_All_DESC) {
            test->Call(*this);
        }
        if (!m_CurrentNode->m_Pubdescs.empty()) {
            for (auto* test : m_All_PUBDESC) {
                test->Call(*this);
            }
            for (auto* test : m_All_AUTHORS) {
                test->Call(*this);
            }
        }
        if (m_CurrentNode->m_Biosource) {
            for (auto* test : m_All_BIOSRC) {
                test->Call(*this);
            }
        }
    }
    else if (IsSeqSet(m_CurrentNode->m_Type)) {
        for (auto* test : m_All_SEQ_SET) {
            test->Call(*this);
        }
        for (auto* test : m_All_FEAT) {
            test->Call(*this);
        }
        for (auto* test : m_All_DESC) {
            test->Call(*this);
        }
        if (!m_CurrentNode->m_Pubdescs.empty()) {
            for (auto* test : m_All_PUBDESC) {
                test->Call(*this);
            }
            for (auto* test : m_All_AUTHORS) {
                test->Call(*this);
            }
        }
        if (m_CurrentNode->m_Biosource) {
            for (auto* test : m_All_BIOSRC) {
                test->Call(*this);
            }
        }
    }
    else if (m_CurrentNode->m_Type == eSubmit) {
        for (auto* test : m_All_SUBMIT) {
            test->Call(*this);
        }
        if (!m_CurrentNode->m_Authors.empty()) {
            for (auto* test : m_All_AUTHORS) {
                test->Call(*this);
            }
        }
    }
    else if (m_CurrentNode->m_Type == eString) {
        for (auto* test : m_All_STRING) {
            test->Call(*this);
        }
    }
    else if (m_CurrentNode->m_Type == eFile) {
        return;
    }
    else if (m_CurrentNode->m_Obj) {
        ERR_POST(Info << "Tests for "
                 << m_CurrentNode->m_Obj->GetThisTypeInfo()->GetName()
                 << " are not yet implemented...");
    }
    else if (m_CurrentNode->m_Parent &&
             (m_CurrentNode->m_Parent->m_Type != eFile)) {
        _ASSERT(m_CurrentNode->m_Type == eNone);
        // Only the root node or a child node of a file
        // are permitted to have type None
        NCBI_THROW(CException, eUnknown, "Node has unspecified type");
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


// CDiscrepancyContext
map<string, size_t> CDiscrepancyContext::Autofix()
{
    TReportObjectList tofix;
    map<string, size_t> report;
    for (const auto& tst : m_Tests) {
        const TReportItemList& list = tst.second->GetReport();
        for (const auto& it : list) {
            for (auto& obj : it->GetDetails()) {
                if (obj->CanAutofix()) {
                    tofix.push_back(obj);
                }
            }
        }
    }
    Autofix(tofix, report);
    return report;
}

TDiscrepancyCaseMap CDiscrepancyContext::GetTests() const
{
    TDiscrepancyCaseMap retval;
    for (auto rec: m_Tests)
    {
        retval[rec.first] = rec.second;
    }
    return retval;
}

END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
