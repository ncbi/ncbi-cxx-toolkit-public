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
 * Authors:  Sema
 * Created:  01/29/2015
 */

#ifndef _MISC_DISCREPANCY_DISCREPANCY_CORE_H_
#define _MISC_DISCREPANCY_DISCREPANCY_CORE_H_

#include <misc/discrepancy/discrepancy.hpp>
#include "report_object.hpp"
#include <objects/macro/Suspect_rule_set.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/RNA_ref.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)

/// Housekeeping classes
class CDiscrepancyConstructor;


class CDiscrepancyConstructor
{
protected:
    virtual CRef<CDiscrepancyCase> Create(void) const = 0;
    static void Register(const string& name, const string& descr, TGroup group, CDiscrepancyConstructor& obj);
    static string GetDiscrepancyCaseName(const string& s);
    static const CDiscrepancyConstructor* GetDiscrepancyConstructor(const string& name);
    static map<string, CDiscrepancyConstructor*>& GetTable(void) { return sm_Table.Get();}
    static map<string, string>& GetDescrTable(void) { return sm_DescrTable.Get(); }
    static map<string, TGroup>& GetGroupTable(void) { return sm_GroupTable.Get(); }
    static map<string, string>& GetAliasTable(void) { return sm_AliasTable.Get(); }
    static map<string, vector<string> >& GetAliasListTable(void) { return sm_AliasListTable.Get(); }
private:
    static CSafeStatic<map<string, CDiscrepancyConstructor*> > sm_Table;
    static CSafeStatic<map<string, string> > sm_DescrTable;
    static CSafeStatic<map<string, TGroup> > sm_GroupTable;
    static CSafeStatic<map<string, string> > sm_AliasTable;
    static CSafeStatic<map<string, vector<string> > > sm_AliasListTable;

friend string GetDiscrepancyCaseName(const string& s);
friend string GetDiscrepancyDescr(const string& s);
friend TGroup GetDiscrepancyGroup(const string& s);
friend vector<string> GetDiscrepancyNames(void);
friend vector<string> GetDiscrepancyAliases(const string& name);
friend class CDiscrepancyAlias;
friend class CDiscrepancyContext;
};


inline void CDiscrepancyConstructor::Register(const string& name, const string& descr, TGroup group, CDiscrepancyConstructor& obj)
{
    GetTable()[name] = &obj;
    GetDescrTable()[name] = descr;
    GetGroupTable()[name] = group;
    GetAliasListTable()[name] = vector<string>();
}


class CDiscrepancyAlias : public CDiscrepancyConstructor
{
protected:
    CRef<CDiscrepancyCase> Create(void) const { return CRef<CDiscrepancyCase>(0);}
    static void Register(const string& name, const string& alias)
    {
        map<string, string>& AliasTable = GetAliasTable();
        if (AliasTable.find(alias) != AliasTable.end()) {
            return;
        }
        AliasTable[alias] = name;
        map<string, vector<string> >& AliasListTable = GetAliasListTable();
        AliasListTable[name].push_back(alias);
    }
};


/// CDiscrepancyItem and CReportObject

class CDiscrepancyObject : public CReportObject
{
public:
    CDiscrepancyObject(CConstRef<CBioseq> obj, CScope& scope, const string& filename, bool keep_ref, bool autofix = false, CObject* more = 0) : CReportObject(obj, scope), m_Autofix(autofix), m_More(more)
    {
        SetFilename(filename);
        SetText(scope);
        if(!keep_ref) {
            DropReference();
        }
    }
    CDiscrepancyObject(CConstRef<CSeq_feat> obj, CScope& scope, const string& filename, bool keep_ref, bool autofix = false, CObject* more = 0) : CReportObject(obj, scope), m_Autofix(autofix), m_More(more)
    {
        SetFilename(filename);
        SetText(scope);
        if(!keep_ref) {
            DropReference();
        }
    }
    bool CanAutofix(void) const { return m_Autofix; }
    CConstRef<CObject> GetMoreInfo() { return m_More; }
protected:
    bool m_Autofix;
    CRef<CDiscrepancyCase> m_Case;
    CConstRef<CObject> m_More;
};


class CReportNode;

class CDiscrepancyItem : public CReportItem
{
public:
    CDiscrepancyItem(CDiscrepancyCase& t, const string& s) : m_Msg(s), m_Autofix(false), m_Ext(false), m_Test(&t) {}
    string GetTitle(void) const { return m_Test->GetName();}
    string GetMsg(void) const { return m_Msg;}
    TReportObjectList GetDetails(void) const { return m_Objs;}
    TReportItemList GetSubitems(void) const { return m_Subs;}
    bool CanAutofix(void) const { return m_Autofix; }
    bool IsExtended(void) const { return m_Ext; }
    void Autofix(CScope& scope);
protected:
    string m_Msg;
    bool m_Autofix;
    bool m_Ext;
    TReportObjectList m_Objs;
    TReportItemList m_Subs;
    CRef<CDiscrepancyCase> m_Test;
friend class CReportNode;
};


/// CDiscrepancyCore and CDiscrepancyVisitor - parents for CDiscrepancyCase_* classes

class CDiscrepancyContext;
//typedef map<string, vector<CRef<CReportObj> > > TReportObjectMap; // sema: dont forget to remove it!

class CReportNode : public CObject
{
public:
    typedef map<string, CRef<CReportNode> > TNodeMap;

    CReportNode(const string& name = kEmptyStr) : m_Name(name), m_Fatal(false), m_Autofix(false), m_Ext(false) {}
    
    CReportNode& operator[](const string& name);
    
    CReportNode& Fatal(bool b = true) { m_Fatal = b; return *this; }
    CReportNode& Autofix(bool b = true) { m_Autofix = b; return *this; }
    CReportNode& Ext(bool b = true) { m_Ext = b; return *this; }

    static void Add(TReportObjectList& list, CReportObj& obj, bool unique = true);
    CReportNode& Add(CReportObj& obj, bool unique = true) { Add(m_Objs, obj, unique);  return *this; }
    static void Add(TReportObjectList& list, TReportObjectList& objs, bool unique = true);
    CReportNode& Add(TReportObjectList& objs, bool unique = true) { Add(m_Objs, objs, unique);  return *this; }
    
    TReportObjectList& GetObjects() { return m_Objs; }
    TNodeMap& GetMap() { return m_Map; }
    CRef<CReportItem> Export(CDiscrepancyCase& test, bool unique = true);
    
    bool empty() { return m_Map.empty() && m_Objs.empty(); }
    void clear() { m_Map.clear(); m_Objs.clear(); }
protected:
    string m_Name;
    TNodeMap m_Map;
    TReportObjectList m_Objs;
    bool m_Fatal;
    bool m_Autofix;
    bool m_Ext;
};


class CDiscrepancyCore : public CDiscrepancyCase
{
public:
    CDiscrepancyCore() : m_Count(0) {}
    virtual void Summarize(CDiscrepancyContext& context){}
    virtual TReportItemList GetReport(void) const { return m_ReportItems;}
    virtual void Autofix(const CDiscrepancyItem* item, CScope& scope) {}
protected:
    CReportNode m_Objs;
    TReportItemList m_ReportItems;
    size_t m_Count;
};


inline void CDiscrepancyItem::Autofix(CScope& scope)
{
    if (m_Autofix) {
        ((CDiscrepancyCore&)*m_Test).Autofix(this, scope);
        m_Autofix = false;
    }
}


template<typename T> class CDiscrepancyVisitor : public CDiscrepancyCore
{
public:
    void Call(const T& node, CDiscrepancyContext& context);
    virtual void Visit(const T& node, CDiscrepancyContext& context) = 0;
};


class CSeq_feat_BY_BIOSEQ : public CSeq_feat {};

struct CSeqSummary
{
    CSeqSummary() : Len(0), A(0), C(0), G(0), T(0), N(0) {}
    void clear(void) { Len = 0; A = 0; C = 0; G = 0; T = 0; N = 0; }
    size_t Len;
    size_t A;
    size_t C;
    size_t G;
    size_t T;
    size_t N;
    // add more counters if needed
};

/// CDiscrepancyContext - manage and run the list of tests

class CDiscrepancyContext : public CDiscrepancySet
{
public:
    CDiscrepancyContext(objects::CScope& scope) :
            m_Scope(&scope),
            m_Count_Bioseq(0),
            m_Count_Seq_feat(0),
#define INIT_DISCREPANCY_TYPE(type) m_Enable_##type(false)
            INIT_DISCREPANCY_TYPE(CSeq_inst),
            INIT_DISCREPANCY_TYPE(CSeqFeatData),
            INIT_DISCREPANCY_TYPE(CSeq_feat_BY_BIOSEQ),
            INIT_DISCREPANCY_TYPE(CBioSource),
            INIT_DISCREPANCY_TYPE(CRNA_ref),
            INIT_DISCREPANCY_TYPE(COrgName),
            INIT_DISCREPANCY_TYPE(CSeq_annot)
        {}
    bool AddTest(const string& name);
    void Parse(const objects::CSeq_entry_Handle& handle);
    void Summarize(void);
    const vector<CRef<CDiscrepancyCase> >& GetTests(void){ return m_Tests;}

    template<typename T> void Call(CDiscrepancyVisitor<T>& disc, const T& obj){ disc.Call(obj, *this);}

    CConstRef<CBioseq> GetCurrentBioseq(void) const;
    CConstRef<CBioseq_set> GetCurrentBioseq_set(void) const { return m_Bioseq_set_Stack.empty() ? CConstRef<CBioseq_set>(0) : m_Bioseq_set_Stack.back(); }
    const vector<CConstRef<CBioseq_set> > &Get_Bioseq_set_Stack(void) const { return m_Bioseq_set_Stack; }
    CConstRef<CSeq_feat> GetCurrentSeq_feat(void) const { return m_Current_Seq_feat; }
    size_t GetCountBioseq(void) const { return m_Count_Bioseq;}
    size_t GetCountSeq_feat(void) const { return m_Count_Seq_feat;}
    objects::CScope& GetScope(void) const { return const_cast<objects::CScope&>(*m_Scope);}

    void SetSuspectRules(const string& name);
    CConstRef<CSuspect_rule_set> GetProductRules(void);
    CConstRef<CSuspect_rule_set> GetOrganelleProductRules(void);
    const TReportObjectList& GetNaSeqs(void) const { return m_NaSeqs; }

    // Lazy
    const CBioSource* GetCurrentBiosource(void);
    CBioSource::TGenome GetCurrentGenome(void);
    bool IsEukaryotic(void);
    bool IsCurrentRnaInGenProdSet(void);
    bool SequenceHasFarPointers(void);
    const CSeqSummary& GetNucleotideCount(void);
    bool HasFeatures(void) const { return m_Feat_CI; }
    static string GetGenomeName(int n);
    bool IsBadLocusTagFormat(const string& locus_tag);

protected:
    void Update_Bioseq_set_Stack(CTypesConstIterator& it);
    CRef<objects::CScope> m_Scope;
    set<string> m_Names;
    vector<CRef<CDiscrepancyCase> > m_Tests;
    vector<CConstRef<CBioseq_set> > m_Bioseq_set_Stack;
    CConstRef<CBioseq> m_Current_Bioseq;
    CConstRef<CSeq_feat> m_Current_Seq_feat;
    CConstRef<CSuspect_rule_set> m_ProductRules;
    CConstRef<CSuspect_rule_set> m_OrganelleProductRules;
    string m_SuspectRules;
    size_t m_Count_Bioseq;
    size_t m_Count_Seq_feat;
    bool m_Feat_CI;
    TReportObjectList m_NaSeqs;

#define ADD_DISCREPANCY_TYPE(type) bool m_Enable_##type; vector<CDiscrepancyVisitor<type>* > m_All_##type;
    ADD_DISCREPANCY_TYPE(CSeq_inst)
    ADD_DISCREPANCY_TYPE(CSeqFeatData)
    ADD_DISCREPANCY_TYPE(CSeq_feat_BY_BIOSEQ)
    ADD_DISCREPANCY_TYPE(CBioSource)
    ADD_DISCREPANCY_TYPE(CRNA_ref)
    ADD_DISCREPANCY_TYPE(COrgName)
    ADD_DISCREPANCY_TYPE(CSeq_annot)
};


// MACRO definitions

// The following two macros are required to make sure all modules get loaded from the library
// Use this in discrepancy_core.cpp when adding a new module
#define DISCREPANCY_LINK_MODULE(name) \
    struct CDiscrepancyModule_##name { static void* dummy; CDiscrepancyModule_##name(void){ dummy=0;} };            \
    static CDiscrepancyModule_##name module_##name;

// Use this in the new module
#define DISCREPANCY_MODULE(name) \
    struct CDiscrepancyModule_##name { static void* dummy; CDiscrepancyModule_##name(void){ dummy=0;} };            \
    void* CDiscrepancyModule_##name::dummy=0;


#define DISCREPANCY_CASE(name, type, group, descr, ...) \
    class CDiscrepancyCase_##name : public CDiscrepancyVisitor<type>                                                \
    {                                                                                                               \
    public:                                                                                                         \
        void Visit(const type&, CDiscrepancyContext&);                                                              \
        void Summarize(CDiscrepancyContext&);                                                                       \
        string GetName(void) const { return #name;}                                                                 \
        string GetType(void) const { return #type;}                                                                 \
    protected:                                                                                                      \
        __VA_ARGS__;                                                                                                \
    };                                                                                                              \
    class CDiscrepancyConstructor_##name : public CDiscrepancyConstructor                                           \
    {                                                                                                               \
    public:                                                                                                         \
        CDiscrepancyConstructor_##name(void){ Register(#name, descr, group, *this);}                                \
    protected:                                                                                                      \
        CRef<CDiscrepancyCase> Create(void) const { return CRef<CDiscrepancyCase>(new CDiscrepancyCase_##name);}    \
    };                                                                                                              \
    static CDiscrepancyConstructor_##name DiscrepancyConstructor_##name;                                            \
    static const char* descr_for_##name = descr;                                                                    \
    static TGroup group_for_##name = group;                                                                         \
    void CDiscrepancyCase_##name::Visit(const type& obj, CDiscrepancyContext& context)


#define DISCREPANCY_SUMMARIZE(name) \
    void CDiscrepancyCase_##name::Summarize(CDiscrepancyContext& context)


#define DISCREPANCY_AUTOFIX(name) \
    class CDiscrepancyCaseA_##name : public CDiscrepancyCase_##name                                                 \
    {                                                                                                               \
    public:                                                                                                         \
        void Autofix(const CDiscrepancyItem* item, CScope& scope);                                                  \
    };                                                                                                              \
    class CDiscrepancyCaseAConstructor_##name : public CDiscrepancyConstructor                                      \
    {                                                                                                               \
    public:                                                                                                         \
        CDiscrepancyCaseAConstructor_##name(void){ Register(#name, descr_for_##name, group_for_##name, *this);}     \
    protected:                                                                                                      \
        CRef<CDiscrepancyCase> Create(void) const { return CRef<CDiscrepancyCase>(new CDiscrepancyCaseA_##name);}   \
    };                                                                                                              \
    static CDiscrepancyCaseAConstructor_##name DiscrepancyCaseAConstructor_##name;                                  \
    void CDiscrepancyCaseA_##name::Autofix(const CDiscrepancyItem* item, CScope& scope)


#define DISCREPANCY_ALIAS(name, alias) \
    class CDiscrepancyCase_##alias { void* name; }; /* prevent name conflicts */                                    \
    class CDiscrepancyAlias_##alias : public CDiscrepancyAlias                                                      \
    {                                                                                                               \
    public:                                                                                                         \
        CDiscrepancyAlias_##alias(void) { Register(#name, #alias);}                                                 \
    };                                                                                                              \
    static CDiscrepancyAlias_##alias DiscrepancyAlias_##alias;


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE

#endif  // _MISC_DISCREPANCY_DISCREPANCY_CORE_H_
