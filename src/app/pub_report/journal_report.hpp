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

#ifndef JOURNAL_REPORT_H
#define JOURNAL_REPORT_H

#include <string>
#include <list>

#include <corelib/ncbistre.hpp>

#include "base_report.hpp"

namespace pub_report
{

class CJournalReport : public CBaseReport
{
public:

    CJournalReport(ncbi::CNcbiOstream& out);

    virtual void SetCurrentSeqId(const std::string& name);

    void ReportJournal(const std::string& name);

private:

    std::list<std::string> m_set_level_journals;
    ncbi::CNcbiOstream& m_out;
};

}

#endif // JOURNAL_REPORT_H