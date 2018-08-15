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

#include <objects/biblio/Imprint.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_jour.hpp>

#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/mla/mla_client.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include "wgs_med.hpp"
#include "wgs_utils.hpp"

namespace wgsparse
{

CMLAClient& GetMLA()
{
    static CRef<CMLAClient> mla;

    if (mla.Empty()) {
        mla.Reset(new CMLAClient);
        mla->AskInit();
    }

    return *mla;
}

int GetPMID(const CPub& pub)
{
    int pmid = 0;
    
    try {
        pmid = GetMLA().AskCitmatchpmid(pub);
    }
    catch (const CException& ) {
        // Failed lookup is not an error
    }

    return pmid;
}

static void StripPubComment(string& comment)
{
    static const string PUB_STATUS[] = {
        "Publication Status: Available-Online prior to print",
        "Publication Status : Available-Online prior to print",
        "Publication_Status: Available-Online prior to print",
        "Publication_Status : Available-Online prior to print",
        "Publication-Status: Available-Online prior to print",
        "Publication-Status : Available-Online prior to print",
        "Publication Status: Online-Only",
        "Publication Status : Online-Only",
        "Publication_Status: Online-Only",
        "Publication_Status : Online-Only",
        "Publication-Status: Online-Only",
        "Publication-Status : Online-Only",
        "Publication Status: Available-Online",
        "Publication Status : Available-Online",
        "Publication_Status: Available-Online",
        "Publication_Status : Available-Online",
        "Publication-Status: Available-Online",
        "Publication-Status : Available-Online"
    };

    for (auto& cur_status : PUB_STATUS) {

        SIZE_TYPE pos = NStr::FindNoCase(comment, cur_status, 0);

        while (pos != NPOS) {

            size_t len = comment.size(),
                   offset = pos + cur_status.size();
            while (offset < len && (comment[offset] == ';' || comment[offset] == ' ')) {
                ++offset;
            }
            comment = comment.substr(0, pos) + comment.substr(offset);
            pos = NStr::FindNoCase(comment, cur_status, pos);
        }
    }

    if (NStr::StartsWith(comment, "Publication Status", NStr::eNocase) ||
        NStr::StartsWith(comment, "Publication-Status", NStr::eNocase) ||
        NStr::StartsWith(comment, "Publication_Status", NStr::eNocase)) {

        ERR_POST_EX(0, 0, Info << "An unusual Publication Status comment exists for this record: \"" << comment <<
                    "\". If it is a new variant of the special comments used to indicate ahead-of-print or online-only articles, then the comment must be added to the appropriate table of the parser.");
    }
}

static bool NeedToStripComment(const CCit_jour& journal)
{
    return journal.IsSetImp() && journal.GetImp().IsSetPubstatus() &&
        (journal.GetImp().GetPubstatus() == ePubStatus_epublish ||
        journal.GetImp().GetPubstatus() == ePubStatus_ppublish ||
        journal.GetImp().GetPubstatus() == ePubStatus_aheadofprint);
}

static void StripErRemarks(CPubdesc& pubdescr)
{
    if (pubdescr.IsSetPub() && pubdescr.GetPub().IsSet() && pubdescr.IsSetComment()) {

        auto& pubs = pubdescr.SetPub().Set();
        for (auto& pub : pubs) {

            if (pub->IsArticle() && pub->GetArticle().IsSetFrom() && pub->GetArticle().GetFrom().IsJournal()) {

                CCit_jour& journal = pub->SetArticle().SetFrom().SetJournal();
                if (NeedToStripComment(journal)) {
                    string& comment = pubdescr.SetComment();
                    StripPubComment(comment);
                    if (comment.empty()) {
                        pubdescr.ResetComment();
                    }
                    break;
                }
            }
        }
    }
}

int SinglePubLookup(CPubdesc& pubdescr)
{
    int pmid = 0;
    if (pubdescr.IsSetPub() && pubdescr.GetPub().IsSet()) {

        for (auto& pub : pubdescr.GetPub().Get()) {
            if (pub->IsPmid()) {
                pmid = pub->GetPmid();
                break;
            }
        }

        if (pmid == 0) {
            for (auto& pub : pubdescr.GetPub().Get()) {

                if (pub->IsArticle()) {

                    pmid = GetPMID(*pub);

                    if (pmid) {
                        CRef<CPub> pmid_pub(new CPub);
                        pmid_pub->SetPmid().Set(pmid);
                        pubdescr.SetPub().Set().push_back(pmid_pub);
                    }
                    break;
                }
            }
        }

        if (pmid) {
            CRef<CPub> pub = GetMLA().AskGetpub(pmid);
            if (pub.NotEmpty()) {
            }
        }

        StripErRemarks(pubdescr);
    }

    return pmid;
}

bool PerformMedlineLookup(CSeq_entry& entry)
{
    CSeq_descr* descrs = nullptr;
    if (GetNonConstDescr(entry, descrs) && descrs && descrs->IsSet()) {

        for (auto& pub : descrs->Set()) {
            if (pub->IsPub()) {
                // TODO porocess pub
            }
        }
    }

    CBioseq::TAnnot* annots = nullptr;
    if (GetNonConstAnnot(entry, annots) && annots) {

        auto feat_table_it = find_if(annots->begin(), annots->end(), [](CRef<CSeq_annot>& annot) { return annot->IsFtable(); });

        if (feat_table_it != annots->end()) {

            auto& feat_table = (*feat_table_it)->SetData().SetFtable();

            for (auto& feat : feat_table) {
                if (feat->IsSetData() && feat->GetData().IsPub()) {
                    // TODO porocess pub
                }
            }
        }
    }

    if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for (auto& cur_entry : entry.SetSet().SetSeq_set()) {

            if (!PerformMedlineLookup(*cur_entry)) {
                return false;
            }
        }
    }

    return true;
}

}
