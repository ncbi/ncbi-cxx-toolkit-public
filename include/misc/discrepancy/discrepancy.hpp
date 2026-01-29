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

#ifndef _MISC_DISCREPANCY_DISCREPANCY_H_
#define _MISC_DISCREPANCY_DISCREPANCY_H_

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbistd.hpp>
#include <util/compile_time.hpp>

#define NEW_DISCREPANCY_API

BEGIN_NCBI_SCOPE

class CSerialObject;
class CObjectIStream;


BEGIN_SCOPE(objects)
class CSuspect_rule_set;
class CSuspect_rule;
class CScope;
class CSeq_feat;

END_SCOPE(objects)

BEGIN_SCOPE(NDiscrepancy)

#include "testnames.inc"

enum class eTestTypes
{
    SEQUENCE,
    FEAT,
    STRING,
    SEQ_SET,
    DESC,
    BIOSRC,
    AUTHORS,
    SUBMIT,
    PUBDESC,
    max_num_types,
};

using TTestNamesSet = ct::const_bitset<static_cast<size_t>(eTestNames::max_test_names), eTestNames>;
template<eTestTypes _type>
using TTypeTag = std::integral_constant<eTestTypes, _type>;

class NCBI_DISCREPANCY_EXPORT CReportObj : public CObject
{
public:
    enum EType {
        eType_feature,
        eType_descriptor,
        eType_sequence,
        eType_seq_set,
        eType_submit_block,
        eType_string
    };
    virtual ~CReportObj(){}

    virtual string GetBioseqLabel() const = 0;
    virtual string GetText() const = 0;
    virtual string GetPath() const = 0;
    virtual string GetFeatureType() const = 0;
    virtual string GetProductName() const = 0;
    virtual string GetLocation() const = 0;
    virtual string GetLocusTag() const = 0;
    virtual string GetShort() const = 0;
    virtual EType GetType() const = 0;
    virtual bool CanAutofix() const = 0;
    virtual bool IsFixed() const = 0;
    virtual void SetMoreInfo(CObject* data) = 0;
};
typedef vector<CRef<CReportObj> > TReportObjectList;


class NCBI_DISCREPANCY_EXPORT CAutofixReport : public CObject
{
public:
    CAutofixReport(const string&s, unsigned int n) : S(s), N(n) {}
    void AddSubitems(const vector<CRef<CAutofixReport>>& v) { copy(v.begin(), v.end(), back_inserter(V)); }
    string GetS() const { return S; }
    unsigned int GetN() const { return N; }
    const vector<CRef<CAutofixReport>>& GetSubitems() { return V; }
protected:
    string S;
    unsigned int N;
    vector<CRef<CAutofixReport>> V;
};


class NCBI_DISCREPANCY_EXPORT CReportItem : public CObject
{
public:
    enum ESeverity {
        eSeverity_info    = 0,
        eSeverity_warning = 1,
        eSeverity_error   = 2
    };
    virtual ~CReportItem(){}

    virtual string_view GetTitle() const = 0;
    virtual string GetStr() const = 0;
    virtual string GetMsg() const = 0;
    virtual string GetXml() const = 0;
    virtual string GetUnit() const = 0;
    virtual size_t GetCount() const = 0;
    virtual TReportObjectList GetDetails() const = 0;
    virtual vector<CRef<CReportItem> > GetSubitems() const = 0;
    virtual bool CanAutofix() const = 0;
    virtual ESeverity GetSeverity() const = 0;
    virtual bool IsFatal() const = 0;
    virtual bool IsInfo() const = 0;
    virtual bool IsExtended() const = 0;
    virtual bool IsSummary() const = 0;
    virtual bool IsReal() const = 0;
};
typedef vector<CRef<CReportItem> > TReportItemList;


class NCBI_DISCREPANCY_EXPORT CReportItemFactory
{
public:
    static CRef<CReportItem> Create(const string& test_name, const string& name, const CReportObj& main_obj, const TReportObjectList& report_objs, bool autofix = false);
};

class NCBI_DISCREPANCY_EXPORT CDiscrepancyCase : public CObject
{
public:
    virtual ~CDiscrepancyCase(){}
    virtual eTestNames GetName() const = 0;
    virtual string_view GetSName() const = 0;
    virtual eTestTypes GetType() const = 0;
    virtual string_view GetDescription() const = 0;
    virtual const TReportItemList& GetReport() const = 0;
    virtual TReportObjectList GetObjects() const = 0;
};

class NCBI_DISCREPANCY_EXPORT CDiscrepancyProduct: public CObject
{
public:
    virtual void OutputText(CNcbiOstream& out, unsigned short flags, char group = 0) = 0;
    virtual void OutputXML(CNcbiOstream& out, unsigned short flags) = 0;
    virtual void Merge(CDiscrepancyProduct& other) = 0;
    virtual void Summarize() = 0;
};

typedef map<eTestNames, CRef<CDiscrepancyCase> > TDiscrepancyCaseMap;


class NCBI_DISCREPANCY_EXPORT CDiscrepancySet: public CObject
{
protected:
    CDiscrepancySet() {}
public:
    virtual ~CDiscrepancySet(){}

    virtual CRef<CDiscrepancyProduct> RunTests(const TTestNamesSet& testnames, const CSerialObject& object, const string& filename) = 0;
    virtual CRef<CDiscrepancyProduct> GetProduct() = 0;

    virtual unsigned Summarize() = 0;
    virtual map<string, size_t> Autofix() = 0;
    virtual void ParseStream(CObjectIStream& stream, const string& fname, bool skip, const string& default_header = kEmptyStr) = 0;
    virtual void AddTest(eTestNames name) = 0;
    NCBI_DEPRECATED virtual void AddTest(string_view name) = 0;
    virtual void ParseStrings(const string& fname) = 0;
    NCBI_DEPRECATED virtual void Push(const CSerialObject& root, const string& fname = kEmptyStr) = 0;
    NCBI_DEPRECATED virtual void Parse() = 0;
    virtual TDiscrepancyCaseMap GetTests() const = 0;
    NCBI_DEPRECATED virtual void Autofix(TReportObjectList& tofix, map<string, size_t>& rep, const string& default_header = kEmptyStr) = 0;
#if 0
    template<typename Container>
    bool AddTests(const Container& cont)
    {
        bool success = true;
        for_each(cont.begin(), cont.end(), [this, &success](const string& test_name) { success &= this->AddTest(test_name); });
        return success;
    }

    virtual void Parse(const CSerialObject& root, const string& fname = kEmptyStr) { Push(root, fname); Parse(); }
    virtual void TestString(const string& str) = 0;
    virtual void OutputText(CNcbiOstream& out, unsigned short flags, char group = 0) = 0;
    virtual void OutputXML(CNcbiOstream& out, unsigned short flags) = 0;
#endif

    enum EOutput {
        eOutput_Summary = 1 << 0,   // only summary
        eOutput_Fatal   = 1 << 1,   // print FATAL
        eOutput_Ext     = 1 << 2,   // extended output
        eOutput_Files   = 1 << 3    // print file name
    };

    void SetUserData(void* p){ m_UserData = p; }
    void* GetUserData() const { return m_UserData; }

    static void SetGui(bool b){ m_Gui = b; }
    static bool IsGui() { return m_Gui; }

    const string& GetLineage() const { return m_Lineage; }
    void SetLineage(const string& s) { m_Lineage = s; }

    //float GetSesameStreetCutoff() const { return m_SesameStreetCutoff; }
    //void SetSesameStreetCutoff(float f){ m_SesameStreetCutoff = f; }

    virtual void SetSuspectRules(const string& name, bool read = true) = 0;
    static CRef<CDiscrepancySet> New(objects::CScope& scope);
    static string Format(const string& str, unsigned int count);
    virtual const CSerialObject* FindObject(CReportObj& obj, bool alt = false) = 0;

protected:
    string m_Lineage;
    //float m_SesameStreetCutoff = 0.75;
    static std::atomic<bool> m_Gui;
    void* m_UserData = nullptr;
};


// This is GUI structure, consider moving to it to appropriate location
class NCBI_DISCREPANCY_EXPORT CDiscrepancyGroup : public CObject
{
public:
    CDiscrepancyGroup(const string& name = "", const string& test = "");// : m_Name(name), m_Test(test) {}
    void Add(CRef<CDiscrepancyGroup> child) { m_List.push_back(child); }
    TReportItemList Collect(TDiscrepancyCaseMap& tests, bool all = true) const;
    const CDiscrepancyGroup& operator[](size_t n) const { return *m_List[n]; }

protected:
    string m_Name;
    eTestNames m_Test;
    vector<CRef<CDiscrepancyGroup> > m_List;
};


enum EGroup {
    eAll = 0,
    eDisc = 1,
    eOncaller = 2,
    eSubmitter = 4,
    eSmart = 8,
    eBig = 16,
    eTSA = 32,
    eFatal = 64,
    eAutofix = 128,
};
typedef unsigned short TGroup;


NCBI_DISCREPANCY_EXPORT eTestNames GetDiscrepancyCaseName(string_view);
NCBI_DISCREPANCY_EXPORT string_view GetDiscrepancyCaseName(eTestNames name);
NCBI_DISCREPANCY_EXPORT string_view GetDiscrepancyDescr(eTestNames name);
NCBI_DISCREPANCY_EXPORT TGroup GetDiscrepancyGroup(eTestNames name);
NCBI_DISCREPANCY_EXPORT TTestNamesSet GetDiscrepancyTests(TGroup group);
NCBI_DISCREPANCY_EXPORT vector<string_view> GetDiscrepancyAliases(eTestNames name);

NCBI_DEPRECATED NCBI_DISCREPANCY_EXPORT string_view GetDiscrepancyDescr(string_view name);
NCBI_DEPRECATED NCBI_DISCREPANCY_EXPORT TGroup GetDiscrepancyGroup(string_view name);
NCBI_DEPRECATED NCBI_DISCREPANCY_EXPORT vector<string> GetDiscrepancyNames(TGroup group = EGroup::eAll);

NCBI_DISCREPANCY_EXPORT bool IsShortrRNA(const objects::CSeq_feat& f, objects::CScope* scope);

typedef std::function < CRef<objects::CSeq_feat>() > GetFeatureFunc;
NCBI_DISCREPANCY_EXPORT string FixProductName(const objects::CSuspect_rule* rule, objects::CScope& scope, string& prot_name, GetFeatureFunc get_mrna, GetFeatureFunc get_cds);

NCBI_DISCREPANCY_EXPORT CConstRef<objects::CSuspect_rule_set> GetOrganelleProductRules(const string& name = "");
NCBI_DISCREPANCY_EXPORT CConstRef<objects::CSuspect_rule_set> GetProductRules(const string& name = "");

NCBI_DISCREPANCY_EXPORT std::ostream& operator<<(std::ostream& str, NDiscrepancy::eTestNames name);

NCBI_DISCREPANCY_EXPORT unsigned int g_CheckProductName(const string& product_name, const objects::CSuspect_rule_set& rules, CNcbiOstream& ostream);

END_SCOPE(NDiscrepancy)

END_NCBI_SCOPE


#endif  // _MISC_DISCREPANCY_DISCREPANCY_H_
