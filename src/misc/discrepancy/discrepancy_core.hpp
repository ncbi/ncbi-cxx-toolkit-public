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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)

/// Housekeeping classes
class CDiscrepancyCaseConstructor;

class CDiscrepancyTable  // singleton!
{
friend class CDiscrepancyCaseConstructor;
private:
    static map<string, CDiscrepancyCaseConstructor*> *sm_Table; // = 0
    static map<string, string> *sm_AliasTable;  // = 0
    static map<string, vector<string> > *sm_AliasListTable; // = 0

    CDiscrepancyTable(){}
    ~CDiscrepancyTable()
    {
        delete sm_Table;
        delete sm_AliasTable;
        delete sm_AliasListTable;
    }
};


class NCBI_DISCREPANCY_EXPORT CDiscrepancyCaseConstructor
{
protected:
    virtual CRef<CDiscrepancyCase> Create() const = 0;
    virtual bool NotImplemented() const { return false;}
    static void Register(const string& name, CDiscrepancyCaseConstructor* ptr);
    static string GetDiscrepancyCaseName(const string&);
    static const CDiscrepancyCaseConstructor* GetDiscrepancyCaseConstructor(const string&);
    static map<string, CDiscrepancyCaseConstructor*>& GetTable();
    static map<string, string>& GetAliasTable();
    static map<string, vector<string> >& GetAliasListTable();
    static CDiscrepancyTable m_Table;
friend string GetDiscrepancyCaseName(const string&);
friend CRef<CDiscrepancyCase> GetDiscrepancyCase(const string&);
friend bool DiscrepancyCaseNotImplemented(const string&);
friend vector<string> GetDiscrepancyNames();
friend vector<string> GetDiscrepancyAliases(const string&);
friend class CDiscrepancyAlias;
};


inline void CDiscrepancyCaseConstructor::Register(const string& name, CDiscrepancyCaseConstructor* ptr)
{
    GetTable()[name] = ptr;
    GetAliasListTable()[name] = vector<string>();
}


class CDiscrepancyAlias : public CDiscrepancyCaseConstructor
{
protected:
    CRef<CDiscrepancyCase> Create() const { return CRef<CDiscrepancyCase>(0);}
    static void Register(const string& name, const string& alias)
    {
        map<string, string>& AliasTable = GetAliasTable();
        if (AliasTable.find(alias) != AliasTable.end()) return;
        AliasTable[alias] = name;
        map<string, vector<string> >& AliasListTable = GetAliasListTable();
        AliasListTable[name].push_back(alias);
    }
};


/// CDiscrepancyItem and CReportObject

class CDiscrepancyObject : public CReportObject
{
public:
    CDiscrepancyObject(CConstRef<CObject> obj, CScope& scope, const string& filename, bool keep_ref) : CReportObject(obj)
    {
        SetFilename(filename);
        SetText(scope);
        if(!keep_ref) DropReference();
    }
};


class CDiscrepancyItem : public CReportItem
{
public:
    CDiscrepancyItem(const string& s) : m_Msg(s) {}
    string GetMsg(void) const { return m_Msg;}
    TReportObjectList GetDetails(void) const { return m_Objs;}
    void SetDetails(const TReportObjectList& list){ m_Objs = list;}
protected:
    string m_Msg;
    TReportObjectList m_Objs;
};


/// CDiscrepancyCase core and special cases

class CDiscrepancyCaseCore : public CDiscrepancyCase
{
public:
    bool Parse(objects::CSeq_entry_Handle, const CContext&);
    virtual bool Process(objects::CSeq_entry_Handle, const CContext&){ return true;}
    virtual TReportItemList GetReport() const { return m_ReportItems;}
    static void Add(TReportObjectList&, CRef<CDiscrepancyObject>);
protected:
    TReportObjectList m_Objs;
    TReportItemList m_ReportItems;
};


class CDiscrepancyCaseNotImpl : public CDiscrepancyCaseCore
{
public:
    virtual string GetName() const { return m_Name;}
protected:
    CDiscrepancyCaseNotImpl(const string& name) : m_Name(name) { m_ReportItems.push_back(CRef<CReportItem>(new CDiscrepancyItem((m_Name + ": Not implemented"))));}
    string m_Name;
friend class CDiscrepancyCaseNotImplConstructor;
};


class CDiscrepancyCaseNotImplConstructor : public CDiscrepancyCaseConstructor // not for direct use
{
public:
    CDiscrepancyCaseNotImplConstructor(const string& name) : m_Name(name) { Register(name, this);}
protected:
    CRef<CDiscrepancyCase> Create() const { return CRef<CDiscrepancyCase>(new CDiscrepancyCaseNotImpl(m_Name));}
    virtual bool NotImplemented() const { return true;}
    string m_Name;
};


// MACRO definitions

#define DISCREPANCY_LINK_MODULE(name) \
    struct CDiscrepancyModule_##name { static void* dummy; CDiscrepancyModule_##name(){ dummy=0;} };            \
    static CDiscrepancyModule_##name module_##name;

#define DISCREPANCY_MODULE(name) \
    struct CDiscrepancyModule_##name { static void* dummy; CDiscrepancyModule_##name(){ dummy=0;} };            \
    void* CDiscrepancyModule_##name::dummy=0;


#define DISCREPANCY_NOT_IMPL(name) \
    class CDiscrepancyCase_##name {};  /* prevent name conflicts */                                             \
    class CDiscrepancyCase_A_##name {};                                                                         \
    static CDiscrepancyCaseNotImplConstructor DiscrepancyCaseNotImplConstructor_##name(#name);


#define DISCREPANCY_CASE(name,...) \
    class CDiscrepancyCase_##name : public CDiscrepancyCaseCore                                                 \
    {                                                                                                           \
    public:                                                                                                     \
        bool Process(objects::CSeq_entry_Handle, const CContext&);                                              \
        string GetName() const { return #name;}                                                                 \
    protected:                                                                                                  \
        __VA_ARGS__;                                                                                            \
    };                                                                                                          \
    class CDiscrepancyCaseConstructor_##name : public CDiscrepancyCaseConstructor                               \
    {                                                                                                           \
    public:                                                                                                     \
        CDiscrepancyCaseConstructor_##name(){ Register(#name, this);}                                           \
    protected:                                                                                                  \
        CRef<CDiscrepancyCase> Create() const { return CRef<CDiscrepancyCase>(new CDiscrepancyCase_##name);}    \
    };                                                                                                          \
    static CDiscrepancyCaseConstructor_##name DiscrepancyCaseConstructor_##name;                                \
    bool CDiscrepancyCase_##name::Process(objects::CSeq_entry_Handle seh, const CContext& context)


#define DISCREPANCY_AUTOFIX(name) \
    class CDiscrepancyCaseA_##name : public CDiscrepancyCase_##name                                             \
    {                                                                                                           \
    public:                                                                                                     \
        bool Autofix(CScope& scope);                                                                            \
    };                                                                                                          \
    class CDiscrepancyCaseAConstructor_##name : public CDiscrepancyCaseConstructor                              \
    {                                                                                                           \
    public:                                                                                                     \
        CDiscrepancyCaseAConstructor_##name(){ Register(#name, this);}                                          \
    protected:                                                                                                  \
        CRef<CDiscrepancyCase> Create() const { return CRef<CDiscrepancyCase>(new CDiscrepancyCaseA_##name);}   \
    };                                                                                                          \
    static CDiscrepancyCaseAConstructor_##name DiscrepancyCaseAConstructor_##name;                              \
    bool CDiscrepancyCaseA_##name::Autofix(CScope& scope)


#define DISCREPANCY_ALIAS(name, alias) \
    class CDiscrepancyCase_##alias { void* name; }; /* prevent name conflicts */                                \
    class CDiscrepancyAlias_##alias : public CDiscrepancyAlias                                                  \
    {                                                                                                           \
    public:                                                                                                     \
        CDiscrepancyAlias_##alias() { Register(#name, #alias);}                                                 \
    };                                                                                                          \
    static CDiscrepancyAlias_##alias DiscrepancyAlias_##alias;


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE

#endif  // _MISC_DISCREPANCY_DISCREPANCY_CORE_H_
