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

#include <ncbi_pch.hpp>

#include <algorithm>
#include <functional>
#include <iostream>

#include "journal_report.hpp"

/////////////////////////////////////////////////
// CJournalReport

namespace pub_report
{

using namespace std;
USING_NCBI_SCOPE;

CJournalReport::CJournalReport(CNcbiOstream& out) :
    m_out(out)
{}

void CJournalReport::SetCurrentSeqId(const string& name)
{
    CBaseReport::SetCurrentSeqId(name);

    if (!m_set_level_journals.empty()) {
        auto report_fun = bind(&CJournalReport::ReportJournal, this, placeholders::_1);
        for_each(m_set_level_journals.cbegin(), m_set_level_journals.cend(), report_fun);

        m_set_level_journals.clear();
    }
}

void CJournalReport::ReportJournal(const string& name)
{
    if (GetCurrentSeqId().empty()) {
        m_set_level_journals.push_back(name);
    }
    else {
        m_out << GetCurrentSeqId() << '\t' << name << '\n';
    }
}

}