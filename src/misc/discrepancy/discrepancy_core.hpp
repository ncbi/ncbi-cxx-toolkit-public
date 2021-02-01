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
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/macro/Suspect_rule_set.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
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

friend NCBI_DISCREPANCY_EXPORT string GetDiscrepancyCaseName(const string& s);
friend NCBI_DISCREPANCY_EXPORT string GetDiscrepancyDescr(const string& s);
friend NCBI_DISCREPANCY_EXPORT TGroup GetDiscrepancyGroup(const string& s);
friend NCBI_DISCREPANCY_EXPORT vector<string> GetDiscrepancyNames(TGroup group);
friend NCBI_DISCREPANCY_EXPORT vector<string> GetDiscrepancyAliases(const string& name);
friend class CDiscrepancyAlias;
friend class CDiscrepancyContext;
friend class CReportItem;
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
    CRef<CDiscrepancyCase> Create(void) const { return CRef<CDiscrepancyCase>(); }
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
class CDiscrepancyObject;

template<typename T> struct CSimpleTypeObject : public CObject
{
    CSimpleTypeObject() {};
    CSimpleTypeObject(const T& v) : Value(v) {};
    T Value;
};


class CReportNode;

class CDiscrepancyItem : public CReportItem
{
public:
    CDiscrepancyItem(const string& s) : m_Msg(s), m_Count(0), m_Autofix(false), m_Severity(eSeverity_warning), m_Ext(false), m_Summ(false) {}
    CDiscrepancyItem(CDiscrepancyCase& t, const string& s, const string& m, const string& x, const string& o, size_t n) : m_Str(s), m_Msg(m), m_Xml(x), m_Unit(o), m_Count(n), m_Autofix(false), m_Severity(eSeverity_warning), m_Ext(false), m_Summ(false), m_Test(&t) {}
    string GetTitle() const override { return m_Test ? m_Test->GetName() : kEmptyStr; }
    string GetStr() const override { return m_Str; }
    string GetMsg() const override { return m_Msg; }
    string GetXml() const override { return m_Xml; }
    string GetUnit() const override { return m_Unit; }
    size_t GetCount() const override { return m_Count; }
    TReportObjectList GetDetails() const override { return m_Objs; }
    TReportItemList GetSubitems() const override { return m_Subs;}
    bool CanAutofix() const override { return m_Autofix; }
    ESeverity GetSeverity() const override { return m_Severity; }
    bool IsFatal() const override { return m_Severity == eSeverity_error; }
    bool IsInfo() const override { return m_Severity == eSeverity_info; }
    bool IsExtended() const override { return m_Ext; }
    bool IsSummary() const override { return m_Summ; }
    bool IsReal() const override { return !m_Test.Empty(); }

protected:
    string m_Str;
    string m_Msg;
    string m_Xml;
    string m_Unit;
    size_t m_Count;
    mutable bool m_Autofix;
    ESeverity m_Severity;
    bool m_Ext;
    bool m_Summ;
    TReportObjectList m_Objs;
    TReportItemList m_Subs;
    CRef<CDiscrepancyCase> m_Test;
friend class CReportNode;
friend class CDiscrepancyGroup;
friend class CReportItem;
friend class CDiscrepancyObject;
};


/// CDiscrepancyCore and CDiscrepancyVisitor - parents for CDiscrepancyCase_* classes

struct CReportObjPtr
{
    const CReportObj* P;
    CReportObjPtr(const CReportObj* p) : P(p) {}
    friend bool operator<(const CReportObjPtr& one, const CReportObjPtr& another);
};
typedef set<CReportObjPtr> TReportObjectSet;


class CReportNode : public CObject
{
public:
    typedef map<string, CRef<CReportNode> > TNodeMap;

    CReportNode(const string& name = kEmptyStr) : m_Name(name), m_Severity(CReportItem::eSeverity_warning), m_Autofix(false), m_Ext(false), m_Summ(false), m_NoRec(false), m_Count(0) {}

    CReportNode& operator[](const string& name);

    CReportNode& Severity(CReportItem::ESeverity s) { m_Severity = s; return *this; }
    CReportNode& Fatal() { m_Severity = CReportItem::eSeverity_error; return *this; }
    CReportNode& Info() { m_Severity = CReportItem::eSeverity_info; return *this; }
    CReportNode& Ext(bool b = true) { m_Ext = b; return *this; }
    CReportNode& Summ(bool b = true) { m_Summ = b; return *this; }
    CReportNode& NoRec(bool b = true) { m_NoRec = b; return *this; }
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
    size_t GetCount() { return m_Count ? m_Count : m_Objs.size(); }
    void SetCount(size_t n) { m_Count = n; }
    CRef<CReportItem> Export(CDiscrepancyCase& test, bool unique = true) const;
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
    CReportItem::ESeverity m_Severity;
    bool m_Autofix;
    bool m_Ext;
    bool m_Summ;
    bool m_NoRec;   // don't collect objects recursively
    size_t m_Count;
};


class CDiscrepancyCore : public CDiscrepancyCase
{
public:
    CDiscrepancyCore() : m_Count(0) {}
    virtual void Summarize(CDiscrepancyContext& context){}
    TReportItemList GetReport() const override { return m_ReportItems;}
    TReportObjectList GetObjects() const override;
    virtual CRef<CAutofixReport> Autofix(CDiscrepancyObject* obj, CDiscrepancyContext& context) const { return CRef<CAutofixReport>(); }
protected:
    CReportNode m_Objs;
    TReportItemList m_ReportItems;
    size_t m_Count;
};


//inline CRef<CAutofixReport> CDiscrepancyItem::Autofix(CScope& scope) const
//{
//    if (m_Autofix) {
//        CRef<CAutofixReport> ret = ((CDiscrepancyCore&)*m_Test).Autofix(this, scope);
//        m_Autofix = false;
//        return ret;
//    }
//    return CRef<CAutofixReport>();
//}


template<typename T> class CDiscrepancyVisitor : public CDiscrepancyCore
{
public:
    void Call(const T& node, CDiscrepancyContext& context);
    virtual void Visit(const T& node, CDiscrepancyContext& context) = 0;
};


/// BIG FILE
/// will change to enum when possible
class SEQUENCE {};
class SEQ_SET {};
class FEAT {};
class DESC {};
class BIOSRC {};
class PUBDESC {};
class AUTHORS {};
class SUBMIT {};
class STRING {};


struct CSeqSummary
{
    static const size_t WINDOW_SIZE = 30;
    CSeqSummary() { clear(); }
    void clear(void) {
        Len = 0; A = 0; C = 0; G = 0; T = 0; N = 0; Other = 0; Gaps = 0;
        First = true; StartsWithGap = false; EndsWithGap = false; HasRef = false;
        Label.clear(); Stats.clear(); NRuns.clear();
        MaxN = 0; MinQ = 0;
        _Pos = 0; _Ns = 0;
        _QS = 0; _CBscore[0] = 0; _CBposition[0] = 0; _CBread = 0; _CBwrite = 0;
    }
    size_t Len;
    size_t A;
    size_t C;
    size_t G;
    size_t T;
    size_t N;
    size_t Other;
    size_t Gaps;
    size_t MaxN;
    size_t MinQ;
    bool First;
    bool StartsWithGap;
    bool EndsWithGap;
    bool HasRef;
    string Label;
    // add more counters if needed
    vector<pair<size_t, size_t>> NRuns;
    size_t _QS;
    size_t _CBscore[WINDOW_SIZE];
    size_t _CBposition[WINDOW_SIZE];
    size_t _CBread; // read pointer
    size_t _CBwrite; // write pointer
    size_t _Pos;
    size_t _Ns;

    mutable string Stats;
    string GetStats() const;
};

/// CDiscrepancyContext - manage and run the list of tests

// GENE_PRODUCT_CONFLICT
typedef list<pair<CRef<CDiscrepancyObject>, string>> TGenesList;
typedef map<string, TGenesList> TGeneLocusMap;

/// BIG FILE
class CReadHook_Bioseq_set;
class CReadHook_Bioseq;
class CCopyHook_Bioseq_set;
class CCopyHook_Bioseq;
class CCopyHook_Seq_descr;
class CCopyHook_Seq_annot;

class CDiscrepancyContext : public CDiscrepancySet
{
protected:
    struct CParseNode;
    struct CRefNode;

public:
    enum EFlag {
        eHasRearranged = 1 << 0,
        eHasSatFeat = 1 << 1,
        eHasNonSatFeat = 1 << 2
    };

    CDiscrepancyContext(objects::CScope& scope) :
            m_Scope(&scope),
#define INIT_DISCREPANCY_TYPE(type) m_Enable_##type(false)
            INIT_DISCREPANCY_TYPE(string),

// BIG FILE
INIT_DISCREPANCY_TYPE(SEQUENCE),
INIT_DISCREPANCY_TYPE(SEQ_SET),
INIT_DISCREPANCY_TYPE(FEAT),
INIT_DISCREPANCY_TYPE(DESC),
INIT_DISCREPANCY_TYPE(BIOSRC),
INIT_DISCREPANCY_TYPE(PUBDESC),
INIT_DISCREPANCY_TYPE(AUTHORS),
INIT_DISCREPANCY_TYPE(SUBMIT),
INIT_DISCREPANCY_TYPE(STRING)
    { }

    bool AddTest(const string& name) override;
    void Push(const CSerialObject& root, const string& fname) override;
    void Parse() override { ParseAll(*m_RootNode); }
    //void Parse(const CSerialObject& root, const string& fname) override;
    void ParseObject(const CBioseq& root);
    void ParseObject(const CBioseq_set& root);
    void ParseObject(const CSeq_entry& root);
    void ParseObject(const CSeq_submit& root);
    void ParseStream(CObjectIStream& stream, const string& fname, bool skip, const string& default_header = kEmptyStr) override;
    void ParseStrings(const string& fname) override;
    void TestString(const string& str) override;
    unsigned Summarize() override;
    void Autofix(TReportObjectList& tofix, map<string, size_t>& rep, const string& default_header = kEmptyStr) override;
    void AutofixFile(vector<CDiscrepancyObject*>&fixes, const string& default_header);
    const TDiscrepancyCaseMap& GetTests() override { return m_Tests; }
    void OutputText(CNcbiOstream& out, unsigned short flags, char group) override;
    void OutputXML(CNcbiOstream& out, unsigned short flags) override;
    CParseNode* FindNode(const CRefNode& obj);
    const CObject* GetMore(CReportObj& obj);
    const CSerialObject* FindObject(CReportObj& obj, bool alt = false) override;
    void ReplaceObject(CReportObj& obj, CSerialObject*, bool alt = false);
    void ReplaceSeq_feat(CReportObj& obj, const CSeq_feat& old_feat, CSeq_feat& new_feat, bool alt = false);
    CBioseq_set_Handle GetBioseq_setHandle(const CBioseq_set& bss) { return m_Scope->GetBioseq_setHandle(bss); }
    CBioseq_EditHandle GetBioseqHandle(const CBioseq& bs) { return m_Scope->GetBioseqEditHandle(bs); }

    const string& CurrentText(void) const { return m_CurrentNode->m_Ref->m_Text; }
    const CBioseq& CurrentBioseq(void) const { return *dynamic_cast<const CBioseq*>(&*m_CurrentNode->m_Obj); }
    const CBioseq_set& CurrentBioseq_set(void) const { return *dynamic_cast<const CBioseq_set*>(&*m_CurrentNode->m_Obj); }
    const CSeqSummary& CurrentBioseqSummary(void) const;
    unsigned char ReadFlags(void) const { return m_CurrentNode->m_Flags; };
    void PropagateFlags(unsigned char f) { for (CParseNode* node = m_CurrentNode; node; node = node->m_Parent) node->m_Flags |= f; }

    template<typename T> void Call(CDiscrepancyVisitor<T>& disc, const T& obj){ disc.Call(obj, *this);}

    objects::CScope& GetScope(void) const { return const_cast<objects::CScope&>(*m_Scope);}

    void SetFile(const string& fname);
    void SetSuspectRules(const string& name, bool read = true) override;
    CConstRef<CSuspect_rule_set> GetProductRules(void);
    CConstRef<CSuspect_rule_set> GetOrganelleProductRules(void);

    // Lazy
    static bool IsUnculturedNonOrganelleName(const string& taxname);
    static bool HasLineage(const CBioSource& biosrc, const string& def_lineage, const string& type);
    bool HasLineage(const CBioSource* biosrc, const string& lineage);
    static bool IsOrganelle(const CBioSource* biosrc);
    bool IsEukaryotic(const CBioSource* biosrc);
    bool IsBacterial(const CBioSource* biosrc);
    bool IsViral(const CBioSource* biosrc);
    const CSeqSummary& GetSeqSummary(void) { return *m_CurrentNode->m_BioseqSummary; }
    void BuildSeqSummary(const CBioseq& bs, CSeqSummary& summary);
    bool InGenProdSet(void) const { return m_CurrentNode->InGenProdSet(); }
    CConstRef<CSeqdesc> GetTitle() const { return m_CurrentNode->GetTitle(); }
    CConstRef<CSeqdesc> GetMolinfo() const { return m_CurrentNode->GetMolinfo(); }
    CConstRef<CSeqdesc> GetBiosource() const { return m_CurrentNode->GetBiosource(); }
    const vector<CConstRef<CSeqdesc>>& GetSetBiosources() const { return m_CurrentNode->m_SetBiosources; }

    static string GetGenomeName(int n);
    static string GetAminoacidName(const CSeq_feat& feat); // from tRNA
    bool IsBadLocusTagFormat(const string& locus_tag);
    bool IsRefseq(void);
    bool IsBGPipe(void);
    bool IsPseudo(const CSeq_feat& feat);
    const CSeq_feat* GetGeneForFeature(const CSeq_feat& feat);
    string GetProdForFeature(const CSeq_feat& feat);
    void CollectFeature(const CSeq_feat& feat);
    void ClearFeatureList(void);
    const vector<const CSeq_feat*>& FeatAll() { return m_FeatAll; }
    const vector<const CSeq_feat*>& FeatGenes() { return m_FeatGenes; }
    const vector<const CSeq_feat*>& FeatPseudo() { return m_FeatPseudo; }
    const vector<const CSeq_feat*>& FeatCDS() { return m_FeatCDS; }
    const vector<const CSeq_feat*>& FeatMRNAs() { return m_FeatMRNAs; }
    const vector<const CSeq_feat*>& FeatRRNAs() { return m_FeatRRNAs; }
    const vector<const CSeq_feat*>& FeatTRNAs() { return m_FeatTRNAs; }
    const vector<const CSeq_feat*>& Feat_RNAs() { return m_Feat_RNAs; }
    const vector<const CSeq_feat*>& FeatExons() { return m_FeatExons; }
    const vector<const CSeq_feat*>& FeatIntrons() { return m_FeatIntrons; }
    const vector<const CSeq_feat*>& FeatMisc() { return m_FeatMisc; }

    sequence::ECompare Compare(const CSeq_loc& loc1, const CSeq_loc& loc2) const;

/// BIG FILE
    enum EFixType {
        eFixNone,
        eFixSelf,
        eFixParent,
        eFixSet
    };

    CRefNode* ContainingSet(CRefNode& ref);
    CRef<CDiscrepancyObject> BioseqObjRef(EFixType fix = eFixNone, const CObject* more = nullptr);
    CRef<CDiscrepancyObject> BioseqSetObjRef(bool fix = false, const CObject* more = nullptr);
    CRef<CDiscrepancyObject> SubmitBlockObjRef(bool fix = false, const CObject* more = nullptr);
    CRef<CDiscrepancyObject> SeqFeatObjRef(const CSeq_feat& feat, EFixType fix = eFixNone, const CObject* more = nullptr);
    CRef<CDiscrepancyObject> SeqFeatObjRef(const CSeq_feat& feat, const CObject* fix, const CObject* more = nullptr);
    CRef<CDiscrepancyObject> SeqdescObjRef(const CSeqdesc& desc, const CObject* fix = nullptr, const CObject* more = nullptr);
    CRef<CDiscrepancyObject> BiosourceObjRef(const CBioSource& biosrc, bool fix = false, const CObject* more = nullptr);
    CRef<CDiscrepancyObject> PubdescObjRef(const CPubdesc& pubdesc, bool fix = false, const CObject* more = nullptr);
    CRef<CDiscrepancyObject> AuthorsObjRef(const CAuth_list& authors, bool fix = false, const CObject* more = nullptr);
    CRef<CDiscrepancyObject> StringObjRef(const CObject* fix = nullptr, const CObject* more = nullptr);
    bool IsBioseq() const { return m_CurrentNode->m_Type == eBioseq; }
    const CPub* AuthPub(const CAuth_list* a) const { auto& apm = m_CurrentNode->m_AuthorPubMap; auto it = apm.find(a); return it == apm.end() ? nullptr : it->second; }

    // GENE_PRODUCT_CONFLICT
    TGeneLocusMap& GetGeneLocusMap() { return m_GeneLocusMap;}

    struct CSeqdesc_vec
    {
        CParseNode& node;
        CSeqdesc_vec(CParseNode& n) : node(n) {}
        struct iterator
        {
            vector<CRef<CParseNode>>::iterator it;
            iterator(vector<CRef<CParseNode>>::iterator x) : it(x) {}
            void operator++() { ++it; }
            bool operator!=(const iterator& x) const { return it != x.it; }
            bool operator==(const iterator& x) const { return !(x != *this); }
            const CSeqdesc& operator*() { return static_cast<const CSeqdesc&>(*(*it)->m_Obj); }
        };
        iterator begin() { return iterator(node.m_Descriptors.begin()); }
        iterator end() { return iterator(node.m_Descriptors.end()); }
    };

    struct CSeq_feat_vec
    {
        CParseNode& node;
        CSeq_feat_vec(CParseNode& n) : node(n) {}
        struct iterator
        {
            vector<CRef<CParseNode>>::iterator it;
            iterator(vector<CRef<CParseNode>>::iterator x) : it(x) {}
            void operator++() { ++it; }
            bool operator!=(const iterator& x) const { return it != x.it; }
            bool operator==(const iterator& x) const { return !(x != *this); }
            const CSeq_feat& operator*() { return static_cast<const CSeq_feat&>(*(*it)->m_Obj); }
        };
        iterator begin() { return iterator(node.m_Features.begin()); }
        iterator end() { return iterator(node.m_Features.end()); }
    };

    struct CSeqdesc_run
    {
        CParseNode& node;
        CSeqdesc_run(CParseNode& n) : node(n) {}
        struct iterator
        {
            CParseNode* node;
            vector<CRef<CParseNode>>::iterator it;
            iterator(CParseNode* n) : node(n) {
                while (node) {
                    it = node->m_Descriptors.begin();
                    if (it != node->m_Descriptors.end()) return;
                    node = node->m_Parent;
                }
            }
            void operator++() {
                ++it;
                while (node) {
                    if (it != node->m_Descriptors.end()) return;
                    node = node->m_Parent;
                    if (node) it = node->m_Descriptors.begin();
                }
            }
            bool operator!=(const iterator& x) const { return node != x.node || (node && it != x.it); }
            bool operator==(const iterator& x) const { return !(x != *this); }
            const CSeqdesc& operator*() { return static_cast<const CSeqdesc&>(*(*it)->m_Obj); }
        };
        iterator begin() { return iterator(&node); }
        iterator end() { return iterator(nullptr); }
    };

    struct CSeq_feat_run
    {
        CParseNode& node;
        CSeq_feat_run(CParseNode& n) : node(n) {}
        struct iterator
        {
            CParseNode* node;
            bool follow;
            vector<CRef<CParseNode>>::iterator it;
            iterator(CParseNode* n) : node(n), follow(n && n->m_Type == eBioseq && static_cast<const CBioseq&>(*n->m_Obj).IsNa()) {
                while (node) {
                    it = node->m_Features.begin();
                    if (it != node->m_Features.end()) return;
                    node = follow ? node->m_Parent : nullptr;
                }
            }
            void operator++() {
                ++it;
                while (node) {
                    if (it != node->m_Features.end()) return;
                    node = follow ? node->m_Parent : nullptr;
                    if (node) it = node->m_Features.begin();
                }
            }
            bool operator!=(const iterator& x) const { return node != x.node || (node && it != x.it); }
            bool operator==(const iterator& x) const { return !(x != *this); }
            const CSeq_feat& operator*() { return static_cast<const CSeq_feat&>(*(*it)->m_Obj); }
        };
        iterator begin() { return iterator(&node); }
        iterator end() { return iterator(nullptr); }
    };
    CSeqdesc_vec GetSeqdesc() { return CSeqdesc_vec(*m_CurrentNode); }
    CSeq_feat_vec GetFeat() { return CSeq_feat_vec(*m_CurrentNode); }
    CSeqdesc_run GetAllSeqdesc() { return CSeqdesc_run(*m_CurrentNode); }
    CSeq_feat_run GetAllFeat() { return CSeq_feat_run(*m_CurrentNode); }
    vector<const CBioSource*>& GetBiosources() { return m_CurrentNode->m_Biosources; }
    vector<const CPubdesc*>& GetPubdescs() { return m_CurrentNode->m_Pubdescs; }
    vector<const CAuth_list*>& GetAuthors() { return m_CurrentNode->m_Authors; }
    const CPerson_id* GetPerson_id();
    const CSubmit_block* GetSubmit_block();

protected:
    CRef<objects::CScope> m_Scope;
    CRef<feature::CFeatTree> m_FeatTree;
    TDiscrepancyCaseMap m_Tests;
    vector<CConstRef<CBioseq_set> > m_Bioseq_set_Stack;
    CBioseq_Handle m_Current_Bioseq_Handle;
    CConstRef<CBioseq> m_Current_Bioseq;
    CConstRef<CSubmit_block> m_Current_Submit_block;
    CRef<CSimpleTypeObject<string> > m_Current_Submit_block_StringObj;
    CRef<CSimpleTypeObject<string> > m_Current_Cit_sub_StringObj;
    CConstRef<CSeqdesc> m_Current_Seqdesc;
    CConstRef<CSeq_feat> m_Current_Seq_feat;
    CConstRef<CPub> m_Current_Pub;
    CConstRef<CPub_equiv> m_Current_Pub_equiv;
    CConstRef<CSuspect_rule_set> m_ProductRules;
    CConstRef<CSuspect_rule_set> m_OrganelleProductRules;
    string m_SuspectRules;
    vector<const CSeq_feat*> m_FeatAll;
    vector<const CSeq_feat*> m_FeatGenes;
    vector<const CSeq_feat*> m_FeatPseudo;
    vector<const CSeq_feat*> m_FeatCDS;
    vector<const CSeq_feat*> m_FeatMRNAs;
    vector<const CSeq_feat*> m_FeatRRNAs;
    vector<const CSeq_feat*> m_FeatTRNAs;
    vector<const CSeq_feat*> m_Feat_RNAs;  // other RNA
    vector<const CSeq_feat*> m_FeatExons;
    vector<const CSeq_feat*> m_FeatIntrons;
    vector<const CSeq_feat*> m_FeatMisc;
    map<const CRefNode*, CParseNode*> m_NodeMap;
    vector<CDiscrepancyObject*>* m_Fixes;

    CParseNode* FindLocalNode(const CParseNode& node, const CSeq_feat& feat) const;
    CParseNode* FindNode(const CObject* obj) const;
    CParseNode* FindNode(const CSeq_feat& feat) const;
    CParseNode* FindNode(const CSeqdesc& desc) const;
    static bool InNucProtSet(const CParseNode* node) { for (; node; node = node->m_Parent) if (node->m_Type == eSeqSet_NucProt) return true; return false; }
    static bool InGenProdSet(const CParseNode* node) { for (; node; node = node->m_Parent) if (node->m_Type == eSeqSet_GenProd) return true; return false; }
    bool CanFix();
    bool CanFixBioseq_set();
    bool CanFixBioseq();
    bool CanFixSeq_annot();
    bool CanFixSeqdesc();
    bool CanFixSubmit_block();
    bool CanFixBioseq(CRefNode& refnode);
    bool CanFixBioseq_set(CRefNode& refnode);
    bool CanFixFeat(CRefNode& refnode);
    bool CanFixDesc(CRefNode& refnode);
    bool CanFixSubmit_block(CRefNode& refnode);
    void AutofixBioseq();
    void AutofixBioseq_set();
    void AutofixSeq_annot();
    void AutofixSeq_descr();
    void AutofixSubmit_block();
    CRef<CBioseq_set> m_AF_Bioseq_set;
    CRef<CBioseq> m_AF_Bioseq;
    CRef<CSeq_annot> m_AF_Seq_annot;
    CRef<CSeq_descr> m_AF_Seq_descr;
    CRef<CSubmit_block> m_AF_Submit_block;

    CRef<CDiscrepancyGroup> m_Order;
    const CDiscrepancyGroup& x_OutputOrder();

    //map<const CSerialObject*, CConstRef<CReportObjectData>> m_DataMap;

    // GENE_PRODUCT_CONFLICT
    TGeneLocusMap m_GeneLocusMap;

#define ADD_DISCREPANCY_TYPE(type) bool m_Enable_##type; vector<CDiscrepancyVisitor<type>* > m_All_##type;
    //ADD_DISCREPANCY_TYPE(CSeq_inst)
    //ADD_DISCREPANCY_TYPE(CSeqdesc)
    //ADD_DISCREPANCY_TYPE(CSeq_feat)
    //ADD_DISCREPANCY_TYPE(CSubmit_block)
    //ADD_DISCREPANCY_TYPE(CSeqFeatData)
    //ADD_DISCREPANCY_TYPE(CSeq_feat_BY_BIOSEQ)
    //ADD_DISCREPANCY_TYPE(COverlappingFeatures)
    //ADD_DISCREPANCY_TYPE(CBioSource)
    //ADD_DISCREPANCY_TYPE(COrgName)
    //ADD_DISCREPANCY_TYPE(CSeq_annot)
    //ADD_DISCREPANCY_TYPE(CPubdesc)
    //ADD_DISCREPANCY_TYPE(CAuth_list)
    //ADD_DISCREPANCY_TYPE(CPerson_id)
    ADD_DISCREPANCY_TYPE(string)

/// BIG FILE
ADD_DISCREPANCY_TYPE(SEQUENCE)
ADD_DISCREPANCY_TYPE(SEQ_SET)
ADD_DISCREPANCY_TYPE(FEAT)
ADD_DISCREPANCY_TYPE(DESC)
ADD_DISCREPANCY_TYPE(BIOSRC)
ADD_DISCREPANCY_TYPE(PUBDESC)
ADD_DISCREPANCY_TYPE(AUTHORS)
ADD_DISCREPANCY_TYPE(SUBMIT)
ADD_DISCREPANCY_TYPE(STRING)

    mutable TReportItemList m_Group0;
    mutable TReportItemList m_Group1;

/// BIG FILE
friend class CReadHook_Bioseq_set;
friend class CReadHook_Bioseq_set_class;
friend class CReadHook_Bioseq;
friend class CCopyHook_Bioseq_set;
friend class CCopyHook_Bioseq;
friend class CCopyHook_Seq_descr;
friend class CCopyHook_Seq_annot;
friend class CCopyHook_Submit_block;

    enum EObjType {
        eNone,
        eFile,
        eSubmit,
        eSeqSet,
        eSeqSet_NucProt,
        eSeqSet_GenProd,
        eSeqSet_SegSet,
        eSeqSet_Genome,
        eSeqSet_Funny,
        eBioseq,
        eSeqFeat,
        eSeqDesc,
        eSubmitBlock,
        eString
    };

    static bool IsSeqSet(EObjType n) {
        switch (n) {
            case eSeqSet:
            case eSeqSet_NucProt:
            case eSeqSet_GenProd:
            case eSeqSet_SegSet:
            case eSeqSet_Genome:
            case eSeqSet_Funny:
                return true;
            default:
                return false;
        }
    }
    static string TypeName(EObjType n) {
        static const char* names[] =
        {
            "None",
            "File",
            "Submit",
            "SeqSet",
            "SeqSet_NucProt",
            "SeqSet_GenProd",
            "SeqSet_SegSet",
            "SeqSet_Genome",
            "SeqSet_Funny",
            "Bioseq",
            "SeqFeat",
            "SeqDesc",
            "SubmitBlock",
            "String"
        };
        return names[n];
    }

    struct CRefNode : public CObject
    {
        CRefNode(EObjType type, unsigned index) : m_Type(type), m_Index(index) {}

        EObjType m_Type;
        size_t m_Index;
        CRef<CRefNode> m_Parent;
        mutable string m_Text;

        string GetText() const;
        string GetBioseqLabel() const;
        //bool operator<(const CRefNode&) const;
    };

    struct CParseNode : public CObject
    {
        enum EInfo {
            eIsPseudo = 1 << 0,
            eKnownPseudo = 1 << 1,
            eKnownGene = 1 << 2,
            eKnownProduct = 1 << 3
        };

        CParseNode(EObjType type, unsigned index, CParseNode* parent = nullptr) : m_Type(type), m_Index(index), m_Repeat(false), m_Info(0), m_Flags(0), m_Parent(parent) {
            m_Ref.Reset(new CRefNode(type, index));
            if (parent) {
                m_Ref->m_Parent.Reset(parent->m_Ref);
            }
        }

        CParseNode& AddDescriptor(const CSeqdesc& seqdesc) {
            CRef<CParseNode> new_node(new CParseNode(eSeqDesc, (unsigned)m_Descriptors.size(), this));
            new_node->m_Obj = &seqdesc;
            m_Descriptors.push_back(new_node);
            m_DescriptorMap[&seqdesc] = new_node;
            if (seqdesc.IsSource()) {
                const CBioSource* biosrc = &seqdesc.GetSource();
                m_Biosources.push_back(biosrc);
                m_BiosourceMap[biosrc] = new_node;
            }
            if (seqdesc.IsPub()) {
                const CPubdesc* pub = &seqdesc.GetPub();
                m_Pubdescs.push_back(pub);
                m_PubdescMap[pub] = new_node;
                if (pub->IsSetPub()) {
                    for (auto& it : pub->GetPub().Get()) {
                        if (it->IsSetAuthors()) {
                            const CAuth_list* auth = &it->GetAuthors();
                            m_Authors.push_back(auth);
                            m_AuthorMap[auth] = new_node;
                            m_AuthorPubMap[auth] = &*it;
                        }
                    }
                }
            }
            return *new_node;
        }

        CParseNode& AddFeature(const CSeq_feat& feat) {
            CRef<CParseNode> new_node(new CParseNode(eSeqFeat, (unsigned)m_Features.size(), this));
            new_node->m_Obj = &feat;
            m_Features.push_back(new_node);
            m_FeatureMap[&feat] = new_node;
            if (feat.IsSetData() && feat.GetData().IsBiosrc()) {
                const CBioSource* biosrc = &feat.GetData().GetBiosrc();
                m_Biosources.push_back(biosrc);
                m_BiosourceMap[biosrc] = new_node;
            }
            if (feat.IsSetData() && feat.GetData().IsPub()) {
                const CPubdesc* pub = &feat.GetData().GetPub();
                m_Pubdescs.push_back(pub);
                m_PubdescMap[pub] = new_node;
                if (pub->IsSetPub()) {
                    for (auto& it : pub->GetPub().Get()) {
                        if (it->IsSetAuthors()) {
                            const CAuth_list* auth = &it->GetAuthors();
                            m_Authors.push_back(auth);
                            m_AuthorMap[auth] = new_node;
                            m_AuthorPubMap[auth] = &*it;
                        }
                    }
                }
            }
            return *new_node;
        }

        void SetType(EObjType type) { m_Type = type; m_Ref->m_Type = type; }

        CConstRef<CSeqdesc> GetTitle() const { return (m_Title || !m_Parent) ? m_Title : m_Parent->GetTitle(); }
        CConstRef<CSeqdesc> GetMolinfo() const { return (m_Molinfo || !m_Parent) ? m_Molinfo : m_Parent->GetMolinfo(); }
        CConstRef<CSeqdesc> GetBiosource() const { return (m_Biosource || !m_Parent) ? m_Biosource : m_Parent->GetBiosource(); }
        bool InGenProdSet() const { return m_Type == eSeqSet_GenProd ? true : m_Parent ? m_Parent->InGenProdSet() : false; }

        EObjType m_Type;
        size_t m_Index;
        bool m_Repeat;
        mutable unsigned char m_Info;
        unsigned char m_Flags;
        CNcbiStreampos m_Pos;
        CRef<CRefNode> m_Ref;
        CConstRef<CObject> m_Obj;
        CParseNode* m_Parent;
        mutable const CParseNode* m_Gene;
        mutable string m_Product;
        vector<CRef<CParseNode>> m_Children;
        vector<CRef<CParseNode>> m_Descriptors;
        vector<CRef<CParseNode>> m_Features;
        vector<const CBioSource*> m_Biosources;
        vector<const CPubdesc*> m_Pubdescs;
        vector<const CAuth_list*> m_Authors;
        map<const CSeqdesc*, CParseNode*> m_DescriptorMap;
        map<const CSeq_feat*, CParseNode*> m_FeatureMap;
        map<const CBioSource*, CParseNode*> m_BiosourceMap;
        map<const CPubdesc*, CParseNode*> m_PubdescMap;
        map<const CAuth_list*, CParseNode*> m_AuthorMap;
        map<const CAuth_list*, const CPub*> m_AuthorPubMap;
        shared_ptr<CSeqSummary> m_BioseqSummary;
        CConstRef<CSeqdesc> m_Title;
        CConstRef<CSeqdesc> m_Molinfo;
        CConstRef<CSeqdesc> m_Biosource;
        vector<CConstRef<CSeqdesc>> m_SetBiosources;

string Path() { return m_Parent ? m_Parent->Path() + " => " + TypeName(m_Type) + ": " + to_string(m_Index) : TypeName(m_Type) + ": " + to_string(m_Index); }
    };

    bool Skip();
    void Extend(CParseNode& node, CObjectIStream& stream);
    void ParseAll(CParseNode& node);
    void Populate(CParseNode& node);
    void PopulateBioseq(CParseNode& node);
    void PopulateSeqSet(CParseNode& node);
    void PopulateSubmit(CParseNode& node);
    void RunTests();

    void PushNode(EObjType);
    void PopNode() { m_CurrentNode.Reset(m_CurrentNode->m_Parent); }

    bool m_Skip;
    CRef<CParseNode> m_RootNode;
    CRef<CParseNode> m_CurrentNode;
    bool IsPseudo(const CParseNode& node);
    const CParseNode* GeneForFeature(const CParseNode& node);
    string ProdForFeature(const CParseNode& node);
    static bool CompareRefs(CRef<CReportObj> a, CRef<CReportObj> b);

friend class CDiscrepancyObject;
};


class CDiscrepancyObject : public CReportObj
{
protected:
    CDiscrepancyObject(CDiscrepancyContext::CRefNode* ref, CDiscrepancyContext::CRefNode* fix = nullptr, const CObject* more = nullptr) : m_Ref(ref), m_Fix(fix), m_More(more), m_Fixed(false) {}

public:
    const string GetText() const override { return m_Ref->GetText(); }
    const string GetPath() const override { for (auto ref = m_Ref; ref; ref = ref->m_Parent) if (ref->m_Type == CDiscrepancyContext::eFile) return ref->m_Text; return kEmptyStr; }
    const string GetFeatureType() const override;
    const string GetProductName() const override;
    const string GetLocation() const override;
    const string GetLocusTag() const override;
    const string GetShort() const override { return m_Ref->GetBioseqLabel(); }
    void SetMoreInfo(CObject* data) override { m_More.Reset(data); }

    EType GetType(void) const // Can we use the same enum?
    {
        //eType_feature,
        //eType_descriptor,
        //eType_sequence,
        //eType_seq_set,
        //eType_submit_block,
        //eType_string

        switch (m_Ref->m_Type) {
            case CDiscrepancyContext::eSeqSet: return eType_seq_set;
            case CDiscrepancyContext::eBioseq: return eType_sequence;
            case CDiscrepancyContext::eSeqFeat: return eType_feature;
            case CDiscrepancyContext::eSeqDesc: return eType_descriptor;
            case CDiscrepancyContext::eSubmitBlock: return eType_submit_block;
            case CDiscrepancyContext::eString: return eType_string;
            default: break;
        }
        return eType_string;
    }

    bool CanAutofix(void) const { return m_Fix && !m_Fixed; }
    bool IsFixed(void) const { return m_Fixed; }
    void SetFixed(void) { m_Fixed = true; }
    CConstRef<CObject> GetMoreInfo() { return m_More; }
    CReportObj* Clone(bool fix, CConstRef<CObject> data) const;

    static string GetTextObjectDescription(const CSeq_feat& seq_feat, CScope& scope);
    static string GetTextObjectDescription(const CSeq_feat& seq_feat, CScope& scope, const string& product);
    static string GetTextObjectDescription(const CSeqdesc& sd);
    static string GetTextObjectDescription(const CBioseq& bs, CScope& scope);
    static string GetTextObjectDescription(CBioseq_set_Handle bssh);
    static void GetTextObjectDescription(const CSeq_feat& seq_feat, CScope& scope, string &type, string &context, string &location, string &locus_tag);
    static void GetTextObjectDescription(const CSeq_feat& seq_feat, CScope& scope, string &type, string &location, string &locus_tag);

protected:
    CRef<CDiscrepancyCase> m_Case;
    CRef<CDiscrepancyContext::CRefNode> m_Ref;
    CRef<CDiscrepancyContext::CRefNode> m_Fix;
    CConstRef<CObject> m_More;
    bool m_Fixed;
    friend class CDiscrepancyContext;
    friend class CReportNode;
    friend class CReportItem;
    friend bool operator<(const CReportObjPtr& one, const CReportObjPtr& another) { return ((const CDiscrepancyObject*)one.P)->m_Ref < ((const CDiscrepancyObject*)another.P)->m_Ref; }
};


inline const CObject* CDiscrepancyContext::GetMore(CReportObj& obj) { return static_cast<CDiscrepancyObject*>(&obj)->m_More; }

// MACRO definitions

// The following two macros are required to make sure all modules get loaded from the library
// Use this in discrepancy_core.cpp when adding a new module
#define DISCREPANCY_LINK_MODULE(name) \
    struct CDiscrepancyModule_##name { static void* dummy; CDiscrepancyModule_##name(){ dummy=nullptr; } };         \
    static CDiscrepancyModule_##name module_##name;

// Use this in the new module
#define DISCREPANCY_MODULE(name) \
    struct CDiscrepancyModule_##name { static void* dummy; CDiscrepancyModule_##name(){ dummy=nullptr; } };         \
    void* CDiscrepancyModule_##name::dummy=nullptr;


#define DISCREPANCY_CASE(name, type, group, descr, ...) \
    class CDiscrepancyCase_##name : public CDiscrepancyVisitor<type>                                                \
    {                                                                                                               \
    public:                                                                                                         \
        void Visit(const type&, CDiscrepancyContext&) override;                                                     \
        void Summarize(CDiscrepancyContext&) override;                                                              \
        string GetName() const override { return #name; }                                                           \
        string GetType() const override { return #type; }                                                           \
    protected:                                                                                                      \
        __VA_ARGS__;                                                                                                \
    };                                                                                                              \
    static const char* descr_for_##name = descr;                                                                    \
    static TGroup group_for_##name = group;                                                                         \
    class CDiscrepancyConstructor_##name : public CDiscrepancyConstructor                                           \
    {                                                                                                               \
    public:                                                                                                         \
        CDiscrepancyConstructor_##name(void){ Register(#name, descr_for_##name, group_for_##name, *this);}          \
    protected:                                                                                                      \
        CRef<CDiscrepancyCase> Create(void) const { return CRef<CDiscrepancyCase>(new CDiscrepancyCase_##name);}    \
    };                                                                                                              \
    static CDiscrepancyConstructor_##name DiscrepancyConstructor_##name;                                            \
    void CDiscrepancyCase_##name::Visit(const type& obj, CDiscrepancyContext& context)


#define DISCREPANCY_SUMMARIZE(name) \
    void CDiscrepancyCase_##name::Summarize(CDiscrepancyContext& context)


#define DISCREPANCY_AUTOFIX(name) \
    class CDiscrepancyCaseA_##name : public CDiscrepancyCase_##name                                                 \
    {                                                                                                               \
    public:                                                                                                         \
        CDiscrepancyCaseA_##name() {}                                                                               \
        CRef<CAutofixReport> Autofix(CDiscrepancyObject* obj, CDiscrepancyContext& context) const;                  \
    };                                                                                                              \
    class CDiscrepancyCaseAConstructor_##name : public CDiscrepancyConstructor                                      \
    {                                                                                                               \
    public:                                                                                                         \
        CDiscrepancyCaseAConstructor_##name(void){ Register(#name, descr_for_##name, group_for_##name | eAutofix, *this);}     \
    protected:                                                                                                      \
        CRef<CDiscrepancyCase> Create(void) const { return CRef<CDiscrepancyCase>(new CDiscrepancyCaseA_##name);}   \
    };                                                                                                              \
    static CDiscrepancyCaseAConstructor_##name DiscrepancyCaseAConstructor_##name;                                  \
    CRef<CAutofixReport> CDiscrepancyCaseA_##name::Autofix(CDiscrepancyObject* obj, CDiscrepancyContext& context) const


#define DISCREPANCY_ALIAS(name, alias) \
    class CDiscrepancyCase_##alias { void* name; }; /* prevent name conflicts */                                    \
    class CDiscrepancyAlias_##alias : public CDiscrepancyAlias                                                      \
    {                                                                                                               \
    public:                                                                                                         \
        CDiscrepancyAlias_##alias(void) { Register(#name, #alias);}                                                 \
    };                                                                                                              \
    static CDiscrepancyAlias_##alias DiscrepancyAlias_##alias;


// Unit test functions
NCBI_DISCREPANCY_EXPORT void UnitTest_FLATFILE_FIND();


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE

#endif  // _MISC_DISCREPANCY_DISCREPANCY_CORE_H_
