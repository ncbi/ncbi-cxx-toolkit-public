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

#include <objects/biblio/Cit_gen.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/biblio/Cit_sub.hpp>

#include <objects/general/Date_std.hpp>

#include "utils.hpp"
#include "unpub_report.hpp"

#include "unpub_hook.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);

namespace pub_report
{

CSkipPubUnpublishedHook::CSkipPubUnpublishedHook(CUnpublishedReport& report) :
    m_report(report)
{};

static bool IsGenUnpublished(const CCit_gen& cit)
{
    return cit.IsSetCit() && cit.IsSetTitle() &&
           NStr::EqualNocase(cit.GetCit(), "Unpublished") &&
           !NStr::EqualNocase(cit.GetTitle(), "Direct Submission");
}

static bool IsArtUnpublished(const CCit_art& cit)
{
    bool ret = false;
    if (cit.IsSetFrom() && cit.GetFrom().IsJournal()) {

        const CCit_jour& journal = cit.GetFrom().GetJournal();
        if (journal.IsSetImp()) {

            const CImprint& imprint = journal.GetImp();
            ret = (imprint.IsSetPrepub() && imprint.GetPrepub() == CImprint::ePrepub_in_press) ||
                  (imprint.IsSetPubstatus() && imprint.GetPubstatus() == ePubStatus_aheadofprint);
        }
    }

    return ret;
}

static bool IsUnpublished(const CPub& pub)
{
    bool ret = false;
    if (pub.IsGen()) {
        ret = IsGenUnpublished(pub.GetGen());
    }
    else if (pub.IsArticle()) {
        ret = IsArtUnpublished(pub.GetArticle());
    }

    return ret;
}

void CSkipPubUnpublishedHook::ProcessUnpublished(const CPub& pub)
{
    m_report.ReportUnpublished(pub);
}

void CSkipPubUnpublishedHook::SkipObject(CObjectIStream &in, const CObjectTypeInfo &info)
{
    CPub_equiv pubs;
    DefaultRead(in, ObjectInfo(pubs));

    ITERATE(CPub_equiv::Tdata, pub, pubs.Get()) {

        if ((*pub)->IsSub()) {

            if ((*pub)->GetSub().IsSetDate() && (*pub)->GetSub().GetDate().IsStd()) {

                const CDate_std& sub_date = (*pub)->GetSub().GetDate().GetStd();
                if (m_report.IsSetDate()) {

                    if (m_report.GetDate().Compare(sub_date) == CDate::eCompare_after) {
                        m_report.SetDate(sub_date);
                    }
                }
                else {
                    m_report.SetDate(sub_date);
                }
            }
        }
        else if (IsUnpublished(**pub)) {
            ProcessUnpublished(**pub);
        }
    }
}

} // namespace pub_report