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
#include <objects/pub/Pub_set.hpp>
#include <objects/mla/mla_client.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include <misc/fix_pub/fix_pub.hpp>

#include <corelib/ncbi_message.hpp>

#include "wgs_med.hpp"
#include "wgs_utils.hpp"
#include "wgs_pubs.hpp"
#include "wgs_errors.hpp"

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

        ERR_POST_EX(ERR_REFERENCE, ERR_REFERENCE_UnusualPubStatus, Warning << "An unusual Publication Status comment exists for this record: \"" << comment <<
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

static void LogMessages(const CMessageListener_Basic& listener)
{
    static const MDiagModule this_module("medarch");

    size_t num_of_msgs = listener.Count();
    for (size_t i = 0; i < num_of_msgs; ++i) {
        const IMessage& msg = listener.GetMessage(i);

        switch (msg.GetSeverity())
        {
        case eDiag_Info:
            ERR_POST_EX(msg.GetCode(), msg.GetSubCode(), this_module << Info << msg.GetText());
            break;
        case eDiag_Warning:
            ERR_POST_EX(msg.GetCode(), msg.GetSubCode(), this_module << Warning << msg.GetText());
            break;
        case eDiag_Error:
            ERR_POST_EX(msg.GetCode(), msg.GetSubCode(), this_module << Error << msg.GetText());
            break;
        case eDiag_Critical:
            ERR_POST_EX(msg.GetCode(), msg.GetSubCode(), this_module << Critical << msg.GetText());
            break;
        case eDiag_Fatal:
            ERR_POST_EX(msg.GetCode(), msg.GetSubCode(), this_module << Fatal << msg.GetText());
            break;
        default:
            ERR_POST_EX(msg.GetCode(), msg.GetSubCode(), this_module << Trace << msg.GetText());
        }
    }
}

void SinglePubLookup(CPubdesc& pubdescr)
{
    CMessageListener_Basic listener;
    CPubFixing fixing(true, true, false, &listener);
    fixing.FixPubEquiv(pubdescr.SetPub());

    LogMessages(listener);

    StripErRemarks(pubdescr);
}


static CRef<CPubdesc> ProcessPubdesc(CPubdesc& pubdesc, CPubCollection& pubs)
{
    CRef<CPubdesc> ret;
    if (!HasPubOfChoice(pubdesc, CPub::e_Sub)) {

        string pubdesc_key = pubs.AddPub(pubdesc, true);

        ret = pubs.GetPubInfo(pubdesc_key).m_desc;
        _ASSERT(ret.NotEmpty());
    }

    return ret;
}

static void FindPubsInDescrs(CSeq_descr* descrs, CPubCollection& pubs)
{
    if (descrs && descrs->IsSet()) {

        for (auto& descr : descrs->Set()) {
            if (descr->IsPub()) {

                CRef<CPubdesc> pubdesc = ProcessPubdesc(descr->SetPub(), pubs);

                if (pubdesc.NotEmpty())
                    descr->SetPub(*pubdesc);
            }
        }
    }
}

static void FixArticles(CPub_set& pub_set, CPubFixing& fixing)
{
    CPub_set::TPub pubs;

    for (auto& cit_art : pub_set.SetArticle()) {

        CRef<CPub> article(new CPub);
        article->SetArticle(*cit_art);
        fixing.FixPub(*article);
        pubs.push_back(article);
    }

    pub_set.SetPub().swap(pubs);
}

static void FixMedlines(CPub_set& pub_set, CPubFixing& fixing)
{
    CPub_equiv::Tdata pub_equives;

    for (auto& pub : pub_set.SetMedline()) {

        CRef<CPub> medline(new CPub);
        medline->SetMedline(*pub);
        fixing.FixPub(*medline);

        pub_equives.push_back(medline);
    }

    pub_set.SetPub().swap(pub_equives);
}

static void FixPubs(CPub_set& pub_set, CPubFixing& fixing)
{
    for (auto& pub : pub_set.SetPub()) {
        if (pub->IsEquiv() && pub->GetEquiv().IsSet()) {
            fixing.FixPubEquiv(pub->SetEquiv());
        }
    }
}

static void FixPub(CPub_set& pub_set)
{
    CMessageListener_Basic listener;
    CPubFixing fixing(true, true, false, &listener);

    if (pub_set.IsArticle()) {
        FixArticles(pub_set, fixing);
    }
    else if (pub_set.IsMedline()) {
        FixMedlines(pub_set, fixing);
    }
    else if (pub_set.IsPub()) {
        FixPubs(pub_set, fixing);
    }

    LogMessages(listener);
}

static void FindPubsInFeatures(CSeq_annot::C_Data::TFtable& ftable, CPubCollection& pubs)
{
    for (auto& feat : ftable) {

        if (feat->IsSetData() && feat->GetData().IsPub()) {

            CPubdesc& cur_pub = feat->SetData().SetPub();
            CRef<CPubdesc> pubdesc = ProcessPubdesc(cur_pub, pubs);

            if (pubdesc.NotEmpty() && pubdesc != &cur_pub)
                cur_pub.Assign(*pubdesc);
        }

        if (feat->IsSetCit()) {

            FixPub(feat->SetCit());
        }
    }
}

static void FindPubsInAnnots(CBioseq::TAnnot* annots, CPubCollection& pubs)
{
    if (annots) {
        for (auto& annot : *annots) {
            if (annot->IsFtable()) {
                FindPubsInFeatures(annot->SetData().SetFtable(), pubs);
            }
        }
    }
}

void PerformMedlineLookup(CSeq_entry& entry, CPubCollection& pubs)
{
    CSeq_descr* descrs = nullptr;
    if (GetNonConstDescr(entry, descrs)) {
        FindPubsInDescrs(descrs, pubs);
    }

    CBioseq::TAnnot* annots = nullptr;
    if (GetNonConstAnnot(entry, annots)) {
        FindPubsInAnnots(annots, pubs);
    }

    if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for (auto& cur_entry : entry.SetSet().SetSeq_set()) {
            PerformMedlineLookup(*cur_entry, pubs);
        }
    }
}

}
