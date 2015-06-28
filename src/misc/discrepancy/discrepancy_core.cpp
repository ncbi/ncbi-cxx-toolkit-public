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
 * Authors: Sema
 *
 */

#include <ncbi_pch.hpp>
#include "discrepancy_core.hpp"
#include <sstream>
#include <objmgr/util/sequence.hpp>


BEGIN_NCBI_SCOPE;
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

CSafeStatic<map<string, CDiscrepancyConstructor*> > CDiscrepancyConstructor::sm_Table;
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
    return str.empty() ? 0 : GetTable()[str];
}


string GetDiscrepancyCaseName(const string& name)
{
    return CDiscrepancyConstructor::GetDiscrepancyCaseName(name);
}


vector<string> GetDiscrepancyNames()
{
    typedef map<string, CDiscrepancyConstructor*> MyMap;
    map<string, CDiscrepancyConstructor*>& Table = CDiscrepancyConstructor::GetTable();
    vector<string> V;
    ITERATE (MyMap, J, Table) {
        if (J->first == "NOT_IMPL") {
            continue;
        }
        V.push_back(J->first);
    }
    return V;
}


vector<string> GetDiscrepancyAliases(const string& name)
{
    map<string, vector<string> >& AliasListTable = CDiscrepancyConstructor::GetAliasListTable();
    return AliasListTable.find(name)!=AliasListTable.end() ? AliasListTable[name] : vector<string>();
}


template<typename T> void CDiscrepancyVisitor<T>::Call(const T& obj, CDiscrepancyContext& context)
{
    try {
        Visit(obj, context);
    }
    catch (CException& e) {
        string ss = "EXCEPTION caught: "; ss += e.what();
        AddItem(*(new CDiscrepancyItem(GetName(), ss)));
    }
}


void CDiscrepancyCore::Add(const string& s, CDiscrepancyObject& obj)
{
    m_Objs[s].push_back(CRef<CReportObj>(&obj));
}


void CDiscrepancyCore::AddUnique(const string& s, CDiscrepancyObject& obj)
{
    ITERATE(TReportObjectList, it, m_Objs[s]) {
        if (obj.Equal(**it)) {
            return;
        }
    }
    m_Objs[s].push_back(CRef<CReportObj>(&obj));
}


CRef<CDiscrepancySet> CDiscrepancySet::New(CScope& scope){ return CRef<CDiscrepancySet>(new CDiscrepancyContext(scope));}


bool CDiscrepancyContext::AddTest(const string& name)
{
    string str = GetDiscrepancyCaseName(name);
    if (str.empty()) {
        return false; // no such test
    }
    if (m_Names.find(str)!=m_Names.end()) {
        return false;  // already there
    }
    CRef<CDiscrepancyCase> test = CDiscrepancyConstructor::GetDiscrepancyConstructor(str)->Create();
    m_Names.insert(str);
    m_Tests.push_back(test);

#define REGISTER_DISCREPANCY_TYPE(type) \
    if (dynamic_cast<CDiscrepancyVisitor<type>* >(test.GetPointer())) {                         \
        m_All_##type.push_back(dynamic_cast<CDiscrepancyVisitor<type>* >(test.GetPointer()));   \
        m_Enable_##type = true;                                                                 \
        return true;                                                                            \
    }
    REGISTER_DISCREPANCY_TYPE(CSeq_inst)
    REGISTER_DISCREPANCY_TYPE(CSeqFeatData)
    return false;
}


void CDiscrepancyContext::Parse(const CSeq_entry_Handle& handle)
{
    CTypesConstIterator i;
    CType<CBioseq>::AddTo(i);
    CType<CSeq_feat>::AddTo(i);
#define ENABLE_DISCREPANCY_TYPE(type) if (m_Enable_##type) CType<type>::AddTo(i);
    ENABLE_DISCREPANCY_TYPE(CSeq_inst)
    ENABLE_DISCREPANCY_TYPE(CSeqFeatData)

    for (i = Begin(*handle.GetCompleteSeq_entry()); i; ++i) {
        if (CType<CBioseq>::Match(i)) {
            m_Current_Bioseq.Reset(m_Scope->GetBioseqHandle(*CType<CBioseq>::Get(i)).GetCompleteBioseq());
            m_Count_Bioseq++;
        }
        else if (CType<CSeq_feat>::Match(i)) {
            m_Current_Seq_feat.Reset(m_Scope->GetSeq_featHandle(*CType<CSeq_feat>::Get(i)).GetSeq_feat());
            m_Count_Seq_feat++;
        }
#define HANDLE_DISCREPANCY_TYPE(type) \
        else if (m_Enable_##type && CType<type>::Match(i)) {                                    \
            const type& obj = *CType<type>::Get(i);                                             \
            NON_CONST_ITERATE(vector<CDiscrepancyVisitor<type>* >, it, m_All_##type) {          \
                Call(**it, obj);                                                                \
            }                                                                                   \
        }
        HANDLE_DISCREPANCY_TYPE(CSeq_inst)  // no semicolon!
        HANDLE_DISCREPANCY_TYPE(CSeqFeatData)
    }
}


void CDiscrepancyContext::Summarize()
{
    NON_CONST_ITERATE(vector<CRef<CDiscrepancyCase> >, it, m_Tests) {
        (*it)->Summarize();
    }
}


void CDiscrepancyContext::SetSuspectRules(const string& name)
{
    m_SuspectRules = name;
    if (!m_ProductRules) {
        m_ProductRules = CSuspect_rule_set::GetProductRules(m_SuspectRules);
    }
}


CConstRef<CSuspect_rule_set> CDiscrepancyContext::GetProductRules()
{
    if (!m_ProductRules) {
        m_ProductRules = CSuspect_rule_set::GetProductRules(m_SuspectRules);
    }
    return m_ProductRules;
}


CConstRef<CSuspect_rule_set> CDiscrepancyContext::GetOrganelleProductRules()
{
    if (!m_OrganelleProductRules) {
        m_OrganelleProductRules = CSuspect_rule_set::GetOrganelleProductRules();
    }
    return m_OrganelleProductRules;
}


const CBioSource* CDiscrepancyContext::GetCurrentBiosource()
{
    static const CBioSource* biosrc;
    static size_t count = 0;
    if (count != m_Count_Bioseq) {
        count = m_Count_Bioseq;
        biosrc = sequence::GetBioSource(*m_Current_Bioseq);
    }
    return biosrc;
}


CBioSource::TGenome CDiscrepancyContext::GetCurrentGenome()
{
    static CBioSource::TGenome genome;
    static size_t count = 0;
    if (count != m_Count_Bioseq) {
        count = m_Count_Bioseq;
        const CBioSource* biosrc = GetCurrentBiosource();
        genome = biosrc->GetGenome();
    }
    return genome;
}


DISCREPANCY_LINK_MODULE(discrepancy_case);
DISCREPANCY_LINK_MODULE(suspect_product_names);

END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
