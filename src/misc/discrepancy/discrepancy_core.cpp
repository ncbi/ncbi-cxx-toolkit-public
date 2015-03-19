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

BEGIN_NCBI_SCOPE;
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

map<string, CDiscrepancyCaseConstructor*> CDiscrepancyCaseConstructor::sm_Table;
map<string, string> CDiscrepancyCaseConstructor::sm_AliasTable;
map<string, vector<string> > CDiscrepancyCaseConstructor::sm_AliasListTable;


string CDiscrepancyCaseConstructor::GetDiscrepancyCaseName(const string& name)
{
    if (sm_Table.find(name) != sm_Table.end()) return name;
    if (sm_AliasTable.find(name) != sm_AliasTable.end()) return sm_AliasTable[name];
    if (name.substr(0, 5) == "DISC_") return GetDiscrepancyCaseName(name.substr(5));
    return "";
}


const CDiscrepancyCaseConstructor* CDiscrepancyCaseConstructor::GetDiscrepancyCaseConstructor(const string& name)
{
    string str = GetDiscrepancyCaseName(name);
    return str.empty() ? 0 : sm_Table[str];
}


string GetDiscrepancyCaseName(const string& name)
{
    return CDiscrepancyCaseConstructor::GetDiscrepancyCaseName(name);
}


CRef<CDiscrepancyCase> GetDiscrepancyCase(const string& name)
{
    const CDiscrepancyCaseConstructor* constr = CDiscrepancyCaseConstructor::GetDiscrepancyCaseConstructor(name);
    return constr ? constr->Create() : CRef<CDiscrepancyCase>(0);
}


bool DiscrepancyCaseNotImplemented(const string& name)
{
    const CDiscrepancyCaseConstructor* constr = CDiscrepancyCaseConstructor::GetDiscrepancyCaseConstructor(name);
    return constr ? constr->NotImplemented() : true;
}


vector<string> GetDiscrepancyNames()
{
    typedef map<string, CDiscrepancyCaseConstructor*> MyMap;
    vector<string> V;
    ITERATE (MyMap, J, CDiscrepancyCaseConstructor::sm_Table) {
        if (J->first == "NOT_IMPL") continue;
        V.push_back(J->first);
    }
    return V;
}


vector<string> GetDiscrepancyAliases(const string& name)
{
    return CDiscrepancyCaseConstructor::sm_AliasListTable.find(name)!=CDiscrepancyCaseConstructor::sm_AliasListTable.end() ? CDiscrepancyCaseConstructor::sm_AliasListTable[name] : vector<string>();
}


bool CDiscrepancyCaseCore::Parse(objects::CSeq_entry_Handle seq, const CContext& context)
{
    try {
        return Process(seq, context);
    }
    catch (CException& e) {
        stringstream ss;
        string fname = context.m_File.empty() ? "the data" : context.m_File;
        ss << "EXCEPTION caught while processing " << fname << ": " << e.what();
        m_ReportItems.push_back(CRef<CReportItem>(new CDiscrepancyItem(ss.str())));
    }
    return true;
}


void CDiscrepancyCaseCore::Add(TReportObjectList& list, CRef<CDiscrepancyObject> obj)
{
    ITERATE(TReportObjectList, it, list) {
        if(obj->Equal(**it)) return;
    }
    list.push_back(CRef<CReportObject>(obj.Release()));
}

// for testing purpose only:
DISCREPANCY_NOT_IMPL(NOT_IMPL);
DISCREPANCY_ALIAS(NOT_IMPL, NOT_IMPL_ALIAS);

DISCREPANCY_LINK_MODULE(discrepancy_case);

END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
