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
#include <misc/discrepancy/report_object.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/macro/Suspect_rule_set.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/submit/Submit_block.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)

/// Housekeeping classes
class CDiscrepancyConstructor;


class CDiscrepancyConstructor
{
protected:
    virtual ~CDiscrepancyConstructor() {}
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
friend vector<string> GetDiscrepancyNames(TGroup group);
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
class CDiscrepancyContext;

class CDiscrepancyObject : public CReportObject
{
protected:
    CDiscrepancyObject(CConstRef<CBioseq> obj, CScope& scope, const string& filename, bool keep_ref, bool autofix = false, CObject* more = 0) : CReportObject(obj, scope), m_Autofix(autofix), m_More(more)
    {
        SetFilename(filename);
    }
    CDiscrepancyObject(CConstRef<CSeqdesc> obj, CScope& scope, const string& filename, bool keep_ref, bool autofix = false, CObject* more = 0) : CReportObject(obj, scope), m_Autofix(autofix), m_More(more)
    {
        SetFilename(filename);
    }
    CDiscrepancyObject(CConstRef<CSeq_feat> obj, CScope& scope, const string& filename, bool keep_ref, bool autofix = false, CObject* more = 0) : CReportObject(obj, scope), m_Autofix(autofix), m_More(more)
    {
        SetFilename(filename);
    }
    CDiscrepancyObject(CConstRef<CSubmit_block> obj, CScope& scope, const string& text, const string& filename, bool keep_ref, bool autofix = false, CObject* more = 0) : CReportObject(obj, scope, text), m_Autofix(autofix), m_More(more)
    {
        SetFilename(filename);
    }
    CDiscrepancyObject(CConstRef<CBioseq_set> obj, CScope& scope, const string& filename, bool keep_ref, bool autofix = false, CObject* more = 0) : CReportObject(obj, scope), m_Autofix(autofix), m_More(more)
    {
        SetFilename(filename);
    }
    CDiscrepancyObject(const CDiscrepancyObject& other) : CReportObject(other), m_Autofix(other.m_Autofix), m_Case(other.m_Case), m_More(other.m_More) {}

public:
    bool CanAutofix(void) const { return m_Autofix; }
    CConstRef<CObject> GetMoreInfo() { return m_More; }
    CReportObj* Clone(bool autofixable, CConstRef<CObject> data) const;

protected:
    bool m_Autofix;
    CRef<CDiscrepancyCase> m_Case;
    CConstRef<CObject> m_More;
friend class CDiscrepancyContext;
};


struct CStringObj : public CObject
{
    string S;
};


class CSubmitBlockDiscObject : public CDiscrepancyObject
{
protected:
    CSubmitBlockDiscObject(CConstRef<CSubmit_block> obj, CRef<CStringObj> label, CScope& scope, const string& text, const string& filename, bool keep_ref, bool autofix = false, CObject* more = 0) : CDiscrepancyObject(obj, scope, text, filename, keep_ref, autofix, more) { m_Label.Reset(label); }
    CConstRef<CStringObj> m_Label;
public:
    virtual const string& GetText() const { return m_Label->S.empty() ? m_Text : m_Label->S; }
friend class CDiscrepancyContext;
};


class CReportNode;

class CDiscrepancyItem : public CReportItem
{
public:
    CDiscrepancyItem(const string& s) : m_Msg(s), m_Autofix(false), m_Fatal(false), m_Ext(false), m_Summ(false) {}
    CDiscrepancyItem(CDiscrepancyCase& t, const string& m, const string& s) : m_Msg(m), m_Str(s), m_Autofix(false), m_Fatal(false), m_Ext(false), m_Summ(false), m_Test(&t) {}
    string GetTitle(void) const { return m_Test->GetName(); }
    string GetMsg(void) const { return m_Msg; }
    string GetStr(void) const { return m_Str; }
    TReportObjectList GetDetails(void) const { return m_Objs; }
    TReportItemList GetSubitems(void) const { return m_Subs;}
    bool CanAutofix(void) const { return m_Autofix; }
    bool IsFatal(void) const { return m_Fatal; }
    bool IsExtended(void) const { return m_Ext; }
    bool IsSummary(void) const { return m_Summ; }
    bool IsReal(void) const { return !m_Test.Empty(); }
    CRef<CAutofixReport> Autofix(CScope& scope) const;
protected:
    string m_Msg;
    string m_Str;
    mutable bool m_Autofix;
    bool m_Fatal;
    bool m_Ext;
    bool m_Summ;
    TReportObjectList m_Objs;
    TReportItemList m_Subs;
    CRef<CDiscrepancyCase> m_Test;
friend class CReportNode;
friend class CDiscrepancyGroup;
};


/// CDiscrepancyCore and CDiscrepancyVisitor - parents for CDiscrepancyCase_* classes

struct CReportObjPtr
{
    const CReportObj* P;
    CReportObjPtr(const CReportObj* p) : P(p) {}
    bool operator<(const CReportObjPtr&) const;
};
typedef set<CReportObjPtr> TReportObjectSet;


class CReportNode : public CObject
{
public:
    typedef map<string, CRef<CReportNode> > TNodeMap;

    CReportNode(const string& name = kEmptyStr) : m_Name(name), m_Fatal(false), m_Autofix(false), m_Ext(false), m_Summ(0), m_Count(0) {}

    CReportNode& operator[](const string& name);

    CReportNode& Fatal(bool b = true) { m_Fatal = b; return *this; }
    CReportNode& Ext(bool b = true) { m_Ext = b; return *this; }
    CReportNode& Summ(bool b = true) { m_Summ = b; return *this; }
    CReportNode& Incr() { m_Count++; return *this; }

    static bool Exist(TReportObjectList& list, TReportObjectSet& hash, CReportObj& obj) { return hash.find(&obj) != hash.end(); }
    bool Exist(const string& name) { return m_Map.find(name) != m_Map.end(); }
    bool Exist(CReportObj& obj) { return Exist(m_Objs, m_Hash, obj); }
    static void Add(TReportObjectList& list, TReportObjectSet& hash, CReportObj& obj, bool unique = true);
    CReportNode& Add(CReportObj& obj, bool unique = true) { Add(m_Objs, m_Hash, obj, unique);  return *this; }
    static void Add(TReportObjectList& list, TReportObjectSet& hash, TReportObjectList& objs, bool unique = true);
    CReportNode& Add(TReportObjectList& objs, bool unique = true) { Add(m_Objs, m_Hash, objs, unique);  return *this; }

    TReportObjectList& GetObjects() { return m_Objs; }
    TNodeMap& GetMap() { return m_Map; }
    CRef<CReportItem> Export(CDiscrepancyCase& test, bool unique = true);
    void Copy(CRef<CReportNode> other);
    bool Promote();

    bool empty() { return m_Map.empty() && m_Objs.empty(); }
    void clear() { m_Map.clear(); m_Objs.clear(); m_Hash.clear(); }
    void clearObjs() { m_Objs.clear(); }
protected:
    string m_Name;
    TNodeMap m_Map;
    TReportObjectList m_Objs;
    TReportObjectSet m_Hash;
    bool m_Fatal;
    bool m_Autofix;
    bool m_Ext;
    bool m_Summ;
    size_t m_Count;
};


class CDiscrepancyCore : public CDiscrepancyCase
{
public:
    CDiscrepancyCore() : m_Count(0) {}
    virtual void Summarize(CDiscrepancyContext& context){}
    virtual TReportItemList GetReport(void) const { return m_ReportItems;}
    virtual CRef<CAutofixReport> Autofix(const CDiscrepancyItem* item, CScope& scope) const { return CRef<CAutofixReport>(0); }
    virtual bool SetHook(TAutofixHook func) { return false;}
protected:
    CReportNode m_Objs;
    TReportItemList m_ReportItems;
    size_t m_Count;
};


inline CRef<CAutofixReport> CDiscrepancyItem::Autofix(CScope& scope) const
{
    if (m_Autofix) {
        CRef<CAutofixReport> ret = ((CDiscrepancyCore&)*m_Test).Autofix(this, scope);
        m_Autofix = false;
        return ret;
    }
    return CRef<CAutofixReport>(0);
}


template<typename T> class CDiscrepancyVisitor : public CDiscrepancyCore
{
public:
    void Call(const T& node, CDiscrepancyContext& context);
    virtual void Visit(const T& node, CDiscrepancyContext& context) = 0;
};


class CSeq_feat_BY_BIOSEQ : public CSeq_feat {};
class CSeqdesc_BY_BIOSEQ : public CSeqdesc {};
class COverlappingFeatures : public CBioseq {};

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

enum EKeepRef {
    eNoRef = 0,
    eKeepRef = 1
};

class CDiscrepancyContext : public CDiscrepancySet
{
public:
    CDiscrepancyContext(objects::CScope& scope) :
            m_Scope(&scope),
            m_Count_Bioseq(0),
            m_Count_Seq_feat(0),
            m_Count_Pub(0),
            m_Count_Pub_equiv(0),
#define INIT_DISCREPANCY_TYPE(type) m_Enable_##type(false)
            INIT_DISCREPANCY_TYPE(CSeq_inst),
            INIT_DISCREPANCY_TYPE(CSeqdesc),
            INIT_DISCREPANCY_TYPE(CSeq_feat),
            INIT_DISCREPANCY_TYPE(CSeqFeatData),
            INIT_DISCREPANCY_TYPE(CSeq_feat_BY_BIOSEQ),
            INIT_DISCREPANCY_TYPE(CSeqdesc_BY_BIOSEQ),
            INIT_DISCREPANCY_TYPE(COverlappingFeatures),
            INIT_DISCREPANCY_TYPE(CBioSource),
            INIT_DISCREPANCY_TYPE(CRNA_ref),
            INIT_DISCREPANCY_TYPE(COrgName),
            INIT_DISCREPANCY_TYPE(CSeq_annot),
            INIT_DISCREPANCY_TYPE(CPubdesc),
            INIT_DISCREPANCY_TYPE(CAuth_list),
            INIT_DISCREPANCY_TYPE(CPerson_id),
            INIT_DISCREPANCY_TYPE(CBioseq_set)
        {}
    bool AddTest(const string& name);
    bool SetAutofixHook(const string& name, TAutofixHook func);
    void Parse(const CSerialObject& root);
    void Summarize(void);
    const TDiscrepancyCaseMap& GetTests(void){ return m_Tests; }
    void OutputText(ostream& out, bool fatal, bool summary, bool ext);
    void OutputXML(ostream& out, bool ext);

    template<typename T> void Call(CDiscrepancyVisitor<T>& disc, const T& obj){ disc.Call(obj, *this);}

    CConstRef<CBioseq> GetCurrentBioseq(void) const;
    CConstRef<CBioseq_set> GetCurrentBioseq_set(void) const { return m_Bioseq_set_Stack.empty() ? CConstRef<CBioseq_set>(0) : m_Bioseq_set_Stack.back(); }
    const vector<CConstRef<CBioseq_set> > &Get_Bioseq_set_Stack(void) const { return m_Bioseq_set_Stack; }
    CConstRef<CSubmit_block> GetCurrentSubmit_block(void) const { return m_Current_Submit_block; }
    CConstRef<CSeqdesc> GetCurrentSeqdesc(void) const { return m_Current_Seqdesc; }
    CConstRef<CSeq_feat> GetCurrentSeq_feat(void) const { return m_Current_Seq_feat; }
    CConstRef<CPub> GetCurrentPub(void) const { return m_Current_Pub; }
    CConstRef<CPub_equiv> GetCurrentPub_equiv(void) const { return m_Current_Pub_equiv; }
    size_t GetCountBioseq(void) const { return m_Count_Bioseq; }
    size_t GetCountSeq_feat(void) const { return m_Count_Seq_feat;}
    objects::CScope& GetScope(void) const { return const_cast<objects::CScope&>(*m_Scope);}

    void SetSuspectRules(const string& name);
    CConstRef<CSuspect_rule_set> GetProductRules(void);
    CConstRef<CSuspect_rule_set> GetOrganelleProductRules(void);
    const TReportObjectList& GetNaSeqs(void) const { return m_NaSeqs; }

    // Lazy
    const CBioSource* GetCurrentBiosource(void);
    const CMolInfo* GetCurrentMolInfo(void);
    bool IsCurrentSequenceMrna(void);
    CBioSource::TGenome GetCurrentGenome(void);
    static bool IsUnculturedNonOrganelleName(const string& taxname);
    static bool HasLineage(const CBioSource& biosrc, const string& def_lineage, const string& type);
    bool HasLineage(const string& lineage);
    bool IsDNA(void);
    bool IsOrganelle(void);
    bool IsEukaryotic(void);
    bool IsBacterial(void);
    bool IsViral(void);
    bool IsPubMed(void);
    bool IsCurrentRnaInGenProdSet(void);
    bool SequenceHasFarPointers(void);
    const CSeqSummary& GetNucleotideCount(void);
    bool HasFeatures(void) const { return m_Feat_CI; }
    static string GetGenomeName(int n);
    static string GetAminoacidName(const CSeq_feat& feat); // from tRNA
    bool IsBadLocusTagFormat(const string& locus_tag);
    const CSeq_id * GetProteinId(void);
    bool IsRefseq(void);
    bool IsBGPipe(void);
    const CSeq_feat* GetCurrentGene(void);
    void CollectFeature(const CSeq_feat& feat);
    void ClearFeatureList(void);
    const vector<CConstRef<CSeq_feat> >& FeatGenes() { return m_FeatGenes; }
    const vector<CConstRef<CSeq_feat> >& FeatPseudo() { return m_FeatPseudo; }
    const vector<CConstRef<CSeq_feat> >& FeatCDS() { return m_FeatCDS; }
    const vector<CConstRef<CSeq_feat> >& FeatMRNAs() { return m_FeatMRNAs; }
    const vector<CConstRef<CSeq_feat> >& FeatRRNAs() { return m_FeatRRNAs; }
    const vector<CConstRef<CSeq_feat> >& FeatTRNAs() { return m_FeatTRNAs; }
    const vector<CConstRef<CSeq_feat> >& Feat_RNAs() { return m_Feat_RNAs; }
    const vector<CConstRef<CSeq_feat> >& FeatExons() { return m_FeatExons; }
    const vector<CConstRef<CSeq_feat> >& FeatIntrons() { return m_FeatIntrons; }

    CRef<CDiscrepancyObject> NewDiscObj(CConstRef<CBioseq> obj, EKeepRef keep_ref = eNoRef, bool autofix = false, CObject* more = 0);
    CRef<CDiscrepancyObject> NewDiscObj(CConstRef<CSeqdesc> obj, EKeepRef keep_ref = eNoRef, bool autofix = false, CObject* more = 0);
    CRef<CDiscrepancyObject> NewDiscObj(CConstRef<CSeq_feat> obj, EKeepRef keep_ref = eNoRef, bool autofix = false, CObject* more = 0);
    CRef<CDiscrepancyObject> NewDiscObj(CConstRef<CBioseq_set> obj, EKeepRef keep_ref = eNoRef, bool autofix = false, CObject* more = 0);
    CRef<CDiscrepancyObject> NewSubmitBlockObj(EKeepRef keep_ref = eNoRef, bool autofix = false, CObject* more = 0);
    CRef<CDiscrepancyObject> NewFeatOrDescObj(EKeepRef keep_ref = eNoRef, bool autofix = false, CObject* more = 0);
    CRef<CDiscrepancyObject> NewFeatOrDescOrSubmitBlockObj(EKeepRef keep_ref = eNoRef, bool autofix = false, CObject* more = 0);

protected:
    void Update_Bioseq_set_Stack(CTypesConstIterator& it);
    CRef<objects::CScope> m_Scope;
    TDiscrepancyCaseMap m_Tests;
    vector<CConstRef<CBioseq_set> > m_Bioseq_set_Stack;
    CBioseq_Handle m_Current_Bioseq_Handle;
    CConstRef<CBioseq> m_Current_Bioseq;
    CConstRef<CSubmit_block> m_Current_Submit_block;
    CRef<CStringObj> m_Current_Submit_block_StringObj;
    CConstRef<CSeqdesc> m_Current_Seqdesc;
    CConstRef<CSeq_feat> m_Current_Seq_feat;
    CConstRef<CPub> m_Current_Pub;
    CConstRef<CPub_equiv> m_Current_Pub_equiv;
    CConstRef<CSuspect_rule_set> m_ProductRules;
    CConstRef<CSuspect_rule_set> m_OrganelleProductRules;
    string m_SuspectRules;
    size_t m_Count_Bioseq;
    size_t m_Count_Seqdesc;
    size_t m_Count_Seq_feat;
    size_t m_Count_Pub;
    size_t m_Count_Pub_equiv;
    bool m_Feat_CI;
    TReportObjectList m_NaSeqs;
    vector<CConstRef<CSeq_feat> > m_FeatGenes;
    vector<CConstRef<CSeq_feat> > m_FeatPseudo;
    vector<CConstRef<CSeq_feat> > m_FeatCDS;
    vector<CConstRef<CSeq_feat> > m_FeatMRNAs;
    vector<CConstRef<CSeq_feat> > m_FeatRRNAs;
    vector<CConstRef<CSeq_feat> > m_FeatTRNAs;
    vector<CConstRef<CSeq_feat> > m_Feat_RNAs;  // other RNA
    vector<CConstRef<CSeq_feat> > m_FeatExons;
    vector<CConstRef<CSeq_feat> > m_FeatIntrons;
    map<const CSerialObject*, string> m_TextMap;
    map<const CSerialObject*, string> m_TextMapShort;

#define ADD_DISCREPANCY_TYPE(type) bool m_Enable_##type; vector<CDiscrepancyVisitor<type>* > m_All_##type;
    ADD_DISCREPANCY_TYPE(CSeq_inst)
    ADD_DISCREPANCY_TYPE(CSeqdesc)
    ADD_DISCREPANCY_TYPE(CSeq_feat)
    ADD_DISCREPANCY_TYPE(CSeqFeatData)
    ADD_DISCREPANCY_TYPE(CSeq_feat_BY_BIOSEQ)
    ADD_DISCREPANCY_TYPE(CSeqdesc_BY_BIOSEQ)
    ADD_DISCREPANCY_TYPE(COverlappingFeatures)
    ADD_DISCREPANCY_TYPE(CBioSource)
    ADD_DISCREPANCY_TYPE(CRNA_ref)
    ADD_DISCREPANCY_TYPE(COrgName)
    ADD_DISCREPANCY_TYPE(CSeq_annot)
    ADD_DISCREPANCY_TYPE(CPubdesc)
    ADD_DISCREPANCY_TYPE(CAuth_list)
    ADD_DISCREPANCY_TYPE(CPerson_id)
    // CBioseq_set should be used only for examining top-level
    // features of the set, and never to subvert the visitor pattern
    // by iterating over the contents oneself
    ADD_DISCREPANCY_TYPE(CBioseq_set)
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
        CDiscrepancyCaseA_##name() : m_Hook(0) {}                                                                   \
        CRef<CAutofixReport> Autofix(const CDiscrepancyItem* item, CScope& scope) const;                            \
        bool SetHook(TAutofixHook func) { m_Hook = func; return true; }                                             \
        TAutofixHook m_Hook;                                                                                        \
    };                                                                                                              \
    class CDiscrepancyCaseAConstructor_##name : public CDiscrepancyConstructor                                      \
    {                                                                                                               \
    public:                                                                                                         \
        CDiscrepancyCaseAConstructor_##name(void){ Register(#name, descr_for_##name, group_for_##name, *this);}     \
    protected:                                                                                                      \
        CRef<CDiscrepancyCase> Create(void) const { return CRef<CDiscrepancyCase>(new CDiscrepancyCaseA_##name);}   \
    };                                                                                                              \
    static CDiscrepancyCaseAConstructor_##name DiscrepancyCaseAConstructor_##name;                                  \
    CRef<CAutofixReport> CDiscrepancyCaseA_##name::Autofix(const CDiscrepancyItem* item, CScope& scope) const


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
