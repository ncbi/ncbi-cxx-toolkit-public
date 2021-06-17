/*
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
* Author:  Alexey Dobronadezhdin
*
* File Description:
*
* ===========================================================================
*/

#ifndef UNPUB_REPORT_H
#define UNPUB_REPORT_H

#include <string>
#include <set>

#include <corelib/ncbistre.hpp>

#include "base_report.hpp"

namespace ncbi
{
    class CHydraSearch;
    class CEutilsClient;
    namespace objects
    {
        class CDate_std;
        class CPub;
        class CPubmed_entry;
    }
}

USING_NCBI_SCOPE;
USING_SCOPE(objects);

namespace pub_report
{

class CPubData;

class CUnpublishedReport : public CBaseReport
{
public:

    CUnpublishedReport(ncbi::CNcbiOstream& out, int max_date_check);

    virtual void CompleteReport();
    virtual void SetCurrentSeqId(const std::string& name);
    virtual void ClearData();

    void ReportUnpublished(const CPub& pub);

    bool IsSetDate() const;
    const CDate_std& GetDate() const;

    void SetDate(const CDate_std& date);
    void ResetDate();

private:

    CHydraSearch& GetHydraSearch() const;
    CEutilsClient& GetEUtils() const;

    int RetrievePMid(const CPubData& data, CPubmed_entry& pubmed_entry) const;

    typedef std::list<std::shared_ptr<CPubData>> TPubs;

    TPubs m_pubs,
          m_pubs_need_id,
          m_pubs_need_date;

    ncbi::CNcbiOstream& m_out;
    mutable shared_ptr<CHydraSearch> m_hydra_search;
    mutable shared_ptr<CEutilsClient> m_eutils;

    CRef<CDate_std> m_date;

    int m_max_date_check;
};

}

#endif // UNPUB_REPORT_H