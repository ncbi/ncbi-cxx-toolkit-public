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
 *  and reliability of the software and data,  the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties,  express or implied,  including
 *  warranties of performance,  merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Colleen Bollin
 * Created:  17/09/2013 15:38:53
 */


#ifndef _ORDER_POLICY_H_
#define _ORDER_POLICY_H_

#include <corelib/ncbistd.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <misc/discrepancy_report/report_item.hpp>

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);
BEGIN_SCOPE(DiscRepNmSpc);


class NCBI_DISCREPANCY_REPORT_EXPORT COrderPolicy : public CObject
{
public:
    COrderPolicy() {};
    ~COrderPolicy() {};
    virtual void Order(TReportItemList& list);
};


class NCBI_DISCREPANCY_REPORT_EXPORT COncallerOrderPolicy : public COrderPolicy
{
public:
    COncallerOrderPolicy() {};
    ~COncallerOrderPolicy() {};

    virtual void Order(TReportItemList& list);

};


class NCBI_DISCREPANCY_REPORT_EXPORT CCommandLineOrderPolicy : public COrderPolicy
{
public:
    CCommandLineOrderPolicy() {};
    ~CCommandLineOrderPolicy() {};

    virtual void Order(TReportItemList& list);

};


END_SCOPE(DiscRepNmSpc);
END_NCBI_SCOPE

#endif 
