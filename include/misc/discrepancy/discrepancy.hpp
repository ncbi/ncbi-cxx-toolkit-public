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

#include <corelib/ncbistd.hpp>
#include <serial/serialbase.hpp>
#include <objmgr/scope.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)


class NCBI_DISCREPANCY_EXPORT CReportObject : public CObject
{
public:
    virtual ~CReportObject(){}
    virtual string GetText() const = 0;
};
typedef vector<CRef<CReportObject> > TReportObjectList;


class NCBI_DISCREPANCY_EXPORT CReportItem : public CObject
{
public:
    virtual ~CReportItem(){}
    virtual string GetMsg(void) const = 0;
    virtual TReportObjectList GetDetails(void) const = 0;
};
typedef vector<CRef<CReportItem> > TReportItemList;


/// Pass default values to the test
struct NCBI_DISCREPANCY_EXPORT CContext
{
    string m_File;
    string m_Lineage;
    bool m_KeepRef;     // set true to allow autofix
};


class NCBI_DISCREPANCY_EXPORT CDiscrepancyCase : public CObject
{
public:
    virtual ~CDiscrepancyCase(){}
    virtual string GetName(void) const = 0;
    virtual bool Parse(objects::CSeq_entry_Handle, const CContext& context) = 0;
    virtual bool Autofix(objects::CScope&){ return false;}
    virtual TReportItemList GetReport() const = 0;
};


NCBI_DISCREPANCY_EXPORT string GetDiscrepancyCaseName(const string&);
NCBI_DISCREPANCY_EXPORT CRef<CDiscrepancyCase> GetDiscrepancyCase(const string&);
NCBI_DISCREPANCY_EXPORT bool DiscrepancyCaseNotImplemented(const string&);
NCBI_DISCREPANCY_EXPORT vector<string> GetDiscrepancyNames();
NCBI_DISCREPANCY_EXPORT vector<string> GetDiscrepancyAliases(const string&);

END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE

#endif  // _MISC_DISCREPANCY_DISCREPANCY_H_
