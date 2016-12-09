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

#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>

#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_jour.hpp>

#include <objects/seq/Pubdesc.hpp>

#include <misc/eutils_client/eutils_client.hpp>

#include <objtools/validator/utilities.hpp>


#include "journal_report.hpp"
#include "utils.hpp"
#include "journal_hook.hpp"


USING_NCBI_SCOPE;
USING_SCOPE(objects);

namespace pub_report
{

static bool IsJournal(const CPub& pub)
{
    return pub.IsArticle() &&
        pub.GetArticle().IsSetFrom() &&
        pub.GetArticle().GetFrom().IsJournal();
}

CSkipPubJournalHook::CSkipPubJournalHook(CJournalReport& report) :
m_report(report)
{};

CEutilsClient& CSkipPubJournalHook::GetEUtils()
{
    if (m_eutils.get() == nullptr) {
        m_eutils.reset(new CEutilsClient);
    }

    return *m_eutils;
}

void CSkipPubJournalHook::SkipObject(CObjectIStream &in, const CObjectTypeInfo &info)
{
    CSeqdesc desc;
    DefaultRead(in, ObjectInfo(desc));

    if (desc.IsPub() && desc.GetPub().IsSetPub()) {

        const CPub_equiv& pubs = desc.GetPub().GetPub();
        ITERATE(CPub_equiv::Tdata, pub, pubs.Get()) {

            if (IsJournal(**pub)) {
                ProcessJournal((*pub)->GetArticle().GetFrom().GetJournal());
            }
        }
    }
}

bool CSkipPubJournalHook::IsJournalMissing(const string& title)
{
    bool ret = false;
    string term = title;
    validator::ConvertToEntrezTerm(term);

    map<string, bool>::iterator cached_term = m_term_cache.find(term);

    if (cached_term != m_term_cache.end()) {
        ret = cached_term->second;
    }
    else {

        Uint8 count = GetEUtils().Count("nlmcatalog", term + "[issn]");

        if (count == 0) {
            count = GetEUtils().Count("nlmcatalog", term + "[multi] AND ncbijournals[sb]");
        }

        if (count == 0) {
            count = GetEUtils().Count("nlmcatalog", term + "[jour]");
        }

        ret = count == 0;
        m_term_cache[term] = ret;
    }

    return ret;
}

void CSkipPubJournalHook::ProcessJournal(const CCit_jour& journal)
{
    bool need_process = true;
    string title;
    if (journal.IsSetTitle()) {

        title = GetBestTitle(journal.GetTitle());
        need_process = IsJournalMissing(title);
    }

    if (need_process) {
        m_report.ReportJournal(title);
    }
}

} // namespace pub_report