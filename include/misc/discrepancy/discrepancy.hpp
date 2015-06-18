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

#include <serial/iterator.hpp>
#include <corelib/ncbistd.hpp>
#include <serial/serialbase.hpp>
#include <objmgr/scope.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)


class NCBI_DISCREPANCY_EXPORT CReportObj : public CObject
{
public:
    virtual ~CReportObj(){}
    virtual const string& GetText() const = 0;
    virtual const string& GetShort() const = 0;
};
typedef vector<CRef<CReportObj> > TReportObjectList;


class NCBI_DISCREPANCY_EXPORT CReportItem : public CObject
{
public:
    virtual ~CReportItem(){}
    virtual string GetTitle(void) const = 0;
    virtual string GetMsg(void) const = 0;
    virtual TReportObjectList GetDetails(void) const = 0;
};
typedef vector<CRef<CReportItem> > TReportItemList;


class CDiscrepancySet;


class NCBI_DISCREPANCY_EXPORT CDiscrepancyCase : public CObject
{
public:
    virtual ~CDiscrepancyCase(){}
    virtual string GetName(void) const = 0;
    virtual string GetType(void) const = 0;
    virtual void Summarize() = 0;
    virtual bool Autofix(objects::CScope&){ return false;}
    virtual TReportItemList GetReport() const = 0;
};


class NCBI_DISCREPANCY_EXPORT CDiscrepancySet : public CObject
{
public:
    CDiscrepancySet() : m_KeepRef(false) {}
    virtual ~CDiscrepancySet(){}
    virtual bool AddTest(const string&) = 0;
    virtual void Parse(objects::CSeq_entry_Handle) = 0;
    virtual void Summarize() = 0;
    virtual const vector<CRef<CDiscrepancyCase> >& GetTests() = 0;

    const string& GetFile() const { return m_File;}
    const string& GetLineage() const { return m_Lineage;}
    bool GetKeepRef() const { return m_KeepRef;}
    void SetFile(const string& s){ m_File = s;}
    void SetLineage(const string& s){ m_Lineage = s;}
    virtual void SetSuspectRules(const string&) = 0;
    void SetKeepRef(bool b){ m_KeepRef = b;}
    static CRef<CDiscrepancySet> New(objects::CScope&);

protected:
    string m_File;
    string m_Lineage;
    bool m_KeepRef;     // set true to allow autofix
};


NCBI_DISCREPANCY_EXPORT string GetDiscrepancyCaseName(const string&);
NCBI_DISCREPANCY_EXPORT vector<string> GetDiscrepancyNames();
NCBI_DISCREPANCY_EXPORT vector<string> GetDiscrepancyAliases(const string&);

END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE

#endif  // _MISC_DISCREPANCY_DISCREPANCY_H_
