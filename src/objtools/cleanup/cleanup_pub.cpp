/* $Id$
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
 * Author:  Colleen Bollin
 *
 * File Description:
 *   Code for cleaning up publications
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <serial/serialbase.hpp>

#include <objects/biblio/Affil.hpp>
#include <objects/biblio/ArticleId.hpp>
#include <objects/biblio/ArticleIdSet.hpp>
#include <objects/biblio/Author.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/general/Name_std.hpp>
#include <objects/general/Person_id.hpp>

#include <objects/seq/Pubdesc.hpp>
#include <objects/pub/Pub_equiv.hpp>

#include <objtools/cleanup/cleanup.hpp>
#include <objtools/cleanup/cleanup_pub.hpp>
#include "cleanup_utils.hpp"
#include <objmgr/util/objutil.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


bool CCleanupPub::x_CleanPubdescComment(string& str)
{
    bool any_change = false;
    if (CleanDoubleQuote(str)) {
        any_change = true;
    }
    if (CleanVisString(str)) {
        any_change = true;
    }
    return any_change;
}

bool CCleanupPub::CleanPubdesc(CPubdesc& pubdesc, bool strip_serial)
{
    bool any_change = false;
    if (pubdesc.IsSetComment()) {
        string& comment = pubdesc.SetComment();
        any_change |= x_CleanPubdescComment(comment);
        if (comment.empty()) {
            pubdesc.ResetComment();
            any_change = true;
        }
    }

    if (pubdesc.IsSetPub()) {
        CPubEquivCleaner cleaner(pubdesc.SetPub());
        bool fix_initials = CPubEquivCleaner::ShouldWeFixInitials(pubdesc.GetPub());
        if (cleaner.Clean(fix_initials, strip_serial)) {
            any_change = true;
        }
    }
    return any_change;
}


static size_t s_PubPriority(CPub::E_Choice val)
{
    size_t priority = 0;
    switch (val) {
    case CPub::e_not_set:
        priority = 0;
        break;
    case CPub::e_Gen:
        priority = 3;
        break;
    case CPub::e_Sub:
        priority = 4;
        break;
    case CPub::e_Medline:
        priority = 13;
        break;
    case CPub::e_Muid:
        priority = 2;
        break;
    case CPub::e_Article:
        priority = 5;
        break;
    case CPub::e_Journal:
        priority = 6;
        break;
    case CPub::e_Book:
        priority = 7;
        break;
    case CPub::e_Proc:
        priority = 8;
        break;
    case CPub::e_Patent:
        priority = 9;
        break;
    case CPub::e_Pat_id:
        priority = 10;
        break;
    case CPub::e_Man:
        priority = 11;
        break;
    case CPub::e_Equiv:
        priority = 12;
        break;
    case CPub::e_Pmid:
        priority = 1;
        break;
    }
    return priority;
}

inline
static
bool s_PubWhichCompare(CRef<CPub> pub1, CRef<CPub> pub2) {
    size_t pr1 = s_PubPriority(pub1->Which());
    size_t pr2 = s_PubPriority(pub2->Which());
    return (pr1 < pr2);
}


struct SPMIDMatch {
    const CPubMedId& m_ID;

    bool operator()(CRef< CArticleId > other_id)
    {
        return (other_id->IsPubmed() && other_id->GetPubmed() == m_ID);
    }
};

void RemoveDuplicatePubMedArticleIds(CArticleIdSet::Tdata& id_set)
{
    auto it = id_set.begin();
    while (it != id_set.end()) {
        while (it != id_set.end() && !(*it)->IsPubmed()) {
            ++it;
        }
        if (it != id_set.end()) {
            auto it2 = it;
            ++it2;
            SPMIDMatch matcher{ (*it)->GetPubmed() };
            id_set.erase(std::remove_if(it2, id_set.end(), matcher), id_set.end());
            ++it;
        }
    }

}

bool CPubEquivCleaner::Clean(bool fix_initials, bool strip_serial)
{
    bool change = false;

    if (!m_Equiv.IsSet()) {
        return change;
    }

    if (s_Flatten(m_Equiv)) {
        change = true;
    }

    // we keep the last of these because we might transfer one
    // to the other as necessary to fill in gaps.
    TEntrezId last_pmid = ZERO_ENTREZ_ID;
    TEntrezId last_article_pubmed_id = ZERO_ENTREZ_ID; // the last from a journal
    CRef<CCit_art> last_article;

    auto& pe_set = m_Equiv.Set();

    pe_set.sort(s_PubWhichCompare);

    auto it = pe_set.begin();
    while (it != pe_set.end()) {
        CPub &pub = **it;

        CRef<CPubCleaner> cleaner = PubCleanerFactory(pub);
        if (cleaner) {
            if (cleaner->Clean(fix_initials, strip_serial)) {
                change = true;
            }
            if (cleaner->IsEmpty()) {
                it = pe_set.erase(it);
                continue;
            }
        }

        // storing these so at the end we'll know the last values
        if (pub.IsPmid()) {
            last_pmid = pub.GetPmid().Get();
        }
        if (pub.IsArticle()) {
            last_article.Reset(&pub.SetArticle());
            if (last_article->IsSetIds()) {
                auto& ids = last_article->SetIds().Set();
                size_t old_size = ids.size();
                RemoveDuplicatePubMedArticleIds(last_article->SetIds());
                change = (ids.size() != old_size);
                // find last article pubmed_id
                auto id_it = ids.rbegin();
                while (id_it != ids.rend()) {
                    if ((*id_it)->IsPubmed()) {
                        last_article_pubmed_id = (*id_it)->GetPubmed();
                        break;
                    }
                    ++id_it;
                }
            }
        }
        ++it;
    }

    // Now, we might have to transfer data to fill in missing information
    if (last_pmid == ZERO_ENTREZ_ID && last_article_pubmed_id > ZERO_ENTREZ_ID) {
        CRef<CPub> new_pub(new CPub);
        new_pub->SetPmid().Set(last_article_pubmed_id);
        m_Equiv.Set().insert(m_Equiv.Set().begin(), new_pub);
        change = true;
    }
    else if (last_pmid > ZERO_ENTREZ_ID && last_article_pubmed_id == ZERO_ENTREZ_ID && last_article) {
        CRef<CArticleId> new_article_id(new CArticleId);
        new_article_id->SetPubmed().Set(last_pmid);
        last_article->SetIds().Set().push_back(new_article_id);
        change = true;
    }
    return change;
}


bool CPubEquivCleaner::IsEmpty()
{
    return !m_Equiv.IsSet() || m_Equiv.Get().empty();
}

bool CPubEquivCleaner::ShouldWeFixInitials(const CPub_equiv& equiv)
{
    if (!equiv.IsSet()) {
        return false;
    }
#if 0
    bool has_id = false,
        has_art = false;

    for (auto it : equiv.Get()) {
        if ((it->IsPmid() && it->GetPmid() > 0) ||
            (it->IsMuid() && it->GetMuid() > 0)) {
            has_id = true;
        }
        else if (it->IsArticle()) {
            has_art = true;
        }
    }
    // return !(has_art  &&  has_id);
#endif
    return true;
}


bool CPubEquivCleaner::s_Flatten(CPub_equiv& pub_equiv)
{
    bool any_change = false;
    CPub_equiv::Tdata& data = pub_equiv.Set();

    auto it = data.begin();
    while (it != data.end()) {
        if ((*it)->IsEquiv()) {
            CPub_equiv& sub_equiv = (*it)->SetEquiv();
            s_Flatten(sub_equiv);
            copy(sub_equiv.Set().begin(), sub_equiv.Set().end(), back_inserter(data));
            it = data.erase(it);
            any_change = true;
        }
        else {
            ++it;
        }
    }
    return any_change;
}







CRef<CPubCleaner> PubCleanerFactory(CPub& pub)
{
    switch (pub.Which()) {
    case CPub::e_Gen:
        return CRef<CPubCleaner>(new CCitGenCleaner(pub.SetGen()));
        break;
    case CPub::e_Equiv:
        return CRef<CPubCleaner>(new CPubEquivCleaner(pub.SetEquiv()));
        break;
    case CPub::e_Sub:
        return CRef<CPubCleaner>(new CCitSubCleaner(pub.SetSub()));
        break;
    case CPub::e_Article:
        return CRef<CPubCleaner>(new CCitArtCleaner(pub.SetArticle()));
        break;
    case CPub::e_Journal:
        return CRef<CPubCleaner>(new CCitJourCleaner(pub.SetJournal()));
        break;
    case CPub::e_Book:
        return CRef<CPubCleaner>(new CCitBookCleaner(pub.SetBook()));
        break;
    case CPub::e_Proc:
        return CRef<CPubCleaner>(new CCitProcCleaner(pub.SetProc()));
        break;
    case CPub::e_Patent:
        return CRef<CPubCleaner>(new CCitPatCleaner(pub.SetPatent()));
        break;
    case CPub::e_Man:
        return CRef<CPubCleaner>(new CCitLetCleaner(pub.SetMan()));
        break;
    case CPub::e_Medline:
        return CRef<CPubCleaner>(new CMedlineEntryCleaner(pub.SetMedline()));
        break;
    default:
        return CRef<CPubCleaner>(NULL);
    }
}


bool CCitGenCleaner::Clean(bool fix_initials, bool strip_serial)
{
    bool rval = false;
    if (m_Gen.IsSetAuthors()) {
        if (CCleanup::CleanupAuthList(m_Gen.SetAuthors(), fix_initials)) {
            rval = true;
        }
    }
    if (m_Gen.IsSetCit()) {
        CCit_gen::TCit& cit = m_Gen.SetCit();
        if (NStr::StartsWith(cit, "unpublished", NStr::eNocase) && cit[0] != 'U') {
            cit[0] = 'U';
            rval = true;
        }
        if (!m_Gen.IsSetJournal()
            && (m_Gen.IsSetVolume() || m_Gen.IsSetPages() || m_Gen.IsSetIssue()))
        {
            m_Gen.ResetVolume();
            m_Gen.ResetPages();
            m_Gen.ResetIssue();
            rval = true;
        }
        const size_t old_cit_size = cit.size();
        NStr::TruncateSpacesInPlace(cit);
        if (old_cit_size != cit.size()) {
            rval = true;
        }
    }
    if (m_Gen.IsSetPages()) {
        if (RemoveSpaces(m_Gen.SetPages())) {
            rval = true;
        }
    }

    // title strstripspaces (see 8728 in sqnutil1.c, Mar 11, 2011)
    if (m_Gen.IsSetTitle() && StripSpaces(m_Gen.SetTitle())) {
        rval = true;
    }

    if (strip_serial && m_Gen.IsSetSerial_number()) {
        m_Gen.ResetSerial_number();
        rval = true;
    }

    // erase if the Cit-gen is now entirely blank
    return rval;
}


bool CCitGenCleaner::IsEmpty()
{
    return (!m_Gen.IsSetCit()) &&
        !m_Gen.IsSetAuthors() &&
        (!m_Gen.IsSetMuid() || m_Gen.GetMuid() <= ZERO_ENTREZ_ID) &&
        !m_Gen.IsSetJournal() &&
        (!m_Gen.IsSetVolume() || m_Gen.GetVolume().empty()) &&
        (!m_Gen.IsSetIssue() || m_Gen.GetIssue().empty()) &&
        (!m_Gen.IsSetPages() || m_Gen.GetPages().empty()) &&
        !m_Gen.IsSetDate() &&
        (!m_Gen.IsSetSerial_number() || m_Gen.GetSerial_number() <= 0) &&
        (!m_Gen.IsSetTitle() || m_Gen.GetTitle().empty()) &&
        (!m_Gen.IsSetPmid() || m_Gen.GetPmid().Get() <= ZERO_ENTREZ_ID);
}


bool CCitSubCleaner::Clean(bool fix_initials, bool strip_serial)
{
    bool any_change = false;

    if (m_Sub.IsSetAuthors()) {
        auto& authors = m_Sub.SetAuthors();
        if (CCleanup::CleanupAuthList(authors, fix_initials)) {
            any_change = true;
        }
        if (!authors.IsSetAffil() && m_Sub.IsSetImp()) {
            auto& imp = m_Sub.SetImp();
            if (imp.IsSetPub()) {
                authors.SetAffil(imp.SetPub());
                imp.ResetPub();
                any_change = true;
            }
        }
        if (authors.IsSetAffil()) {
            auto& affil = authors.SetAffil();
            if (affil.IsStr()) {
                string &str = affil.SetStr();
                static const string& kBadAffil1 = "to the DDBJ/EMBL/GenBank databases";
                static const string& kBadAffil2 = "to the INSDC databases";
                if (NStr::StartsWith(str, kBadAffil1)) {
                    str = str.substr(kBadAffil1.length());
                    NStr::TrimPrefixInPlace(str, ".");
                    any_change = true;
                }
                if (NStr::StartsWith(str, kBadAffil2)) {
                    str = str.substr(kBadAffil2.length());
                    NStr::TrimPrefixInPlace(str, ".");
                    any_change = true;
                }

                if (CCleanup::CleanupAffil(affil)) {
                    any_change = true;
                }
                if (CCleanup::IsEmpty(affil)) {
                    authors.ResetAffil();
                    any_change = true;
                }
            }

        }
    }
    if (m_Sub.IsSetImp() && !m_Sub.IsSetDate()) {
        auto& imp = m_Sub.SetImp();
        if (imp.IsSetDate()) {
            m_Sub.SetDate().Assign(imp.GetDate());
            m_Sub.ResetImp();
        }
        any_change = true;
    }

    return any_change;
}


bool CCitSubCleaner::IsEmpty()
{
    return false;
}


bool CCitArtCleaner::Clean(bool fix_initials, bool strip_serial)
{
    bool change = false;
    if (m_Art.IsSetAuthors()) {
        if (CCleanup::CleanupAuthList(m_Art.SetAuthors(), fix_initials)) {
            change = true;
        }
    }
    if (m_Art.IsSetFrom()) {
        auto& from = m_Art.SetFrom();
        if (from.IsBook()) {
            CCitBookCleaner cleaner(from.SetBook());
            change |= cleaner.Clean(fix_initials, strip_serial);
        } else if (from.IsProc()) {
            CCitProcCleaner cleaner(from.SetProc());
            change |= cleaner.Clean(fix_initials, strip_serial);
        } else if (from.IsJournal()) {
            CCitJourCleaner cleaner(from.SetJournal());
            change |= cleaner.Clean(fix_initials, strip_serial);
        }
    }

    return change;
}


bool CCitBookCleaner::Clean(bool fix_initials, bool strip_serial)
{
    bool change = false;
    if (m_Book.IsSetAuthors() && CCleanup::CleanupAuthList(m_Book.SetAuthors(), fix_initials)) {
        change = true;
    }
    if (m_Book.IsSetImp() && CleanImprint(m_Book.SetImp(), eImprintBC_ForbidStatusChange)) {
        change = true;
    }

    return change;
}


bool CCitJourCleaner::Clean(bool fix_initials, bool strip_serial)
{
    bool change = false;
    if (m_Jour.IsSetImp()) {
        change |= CleanImprint(m_Jour.SetImp(), eImprintBC_AllowStatusChange);
    }

    return change;
}


bool CCitProcCleaner::Clean(bool fix_initials, bool strip_serial)
{
    bool change = false;
    if (m_Proc.IsSetBook()) {
        CCitBookCleaner cleaner(m_Proc.SetBook());
        change = cleaner.Clean(fix_initials, strip_serial);
    }
    return change;
}


bool CPubCleaner::CleanImprint(CImprint& imprint, EImprintBC is_status_change_allowed)
{
    bool any_change = false;
    if (is_status_change_allowed == eImprintBC_AllowStatusChange) {
        if (imprint.IsSetPubstatus()) {
            auto pubstatus = imprint.GetPubstatus();
            switch (pubstatus) {
            case ePubStatus_aheadofprint:
                if (!imprint.IsSetPrepub() || imprint.GetPrepub() != CImprint::ePrepub_in_press)
                {
                    if (!imprint.IsSetVolume() || NStr::IsBlank(imprint.GetVolume())
                        || !imprint.IsSetPages() || NStr::IsBlank(imprint.GetPages())) {
                        imprint.SetPrepub(CImprint::ePrepub_in_press);
                        any_change = true;
                    }
                }
                else if (imprint.IsSetVolume() && !NStr::IsBlank(imprint.GetVolume())
                    && imprint.IsSetPages() && !NStr::IsBlank(imprint.GetPages())) {
                    imprint.ResetPrepub();
                    any_change = true;
                }
                break;
            case ePubStatus_epublish:
                if (imprint.IsSetPrepub() && imprint.GetPrepub() == CImprint::ePrepub_in_press) {
                    imprint.ResetPrepub();
                    any_change = true;
                }
                break;
            default:
                break;
            }
        }
    }
#define FIX_IMPRINT_FIELD(x) \
    if (imprint.IsSet##x()) { \
        string& str = imprint.Set##x(); \
        const size_t old_len = str.length(); \
        Asn2gnbkCompressSpaces(str); \
        CleanVisString(str); \
        if( old_len != str.length() ) { \
            any_change = true; \
        } \
        if (NStr::IsBlank(str)) { \
            imprint.Reset##x(); \
            any_change = true; \
        } \
    }

    FIX_IMPRINT_FIELD(Volume);
    FIX_IMPRINT_FIELD(Issue);
    FIX_IMPRINT_FIELD(Pages);
    FIX_IMPRINT_FIELD(Section);
    FIX_IMPRINT_FIELD(Part_sup);
    FIX_IMPRINT_FIELD(Language);
    FIX_IMPRINT_FIELD(Part_supi);
#undef FIX_IMPRINT_FIELD
    return any_change;
}


bool CCitPatCleaner::Clean(bool fix_initials, bool strip_serial)
{
    bool change = false;
    if (m_Pat.IsSetAuthors() && CCleanup::CleanupAuthList(m_Pat.SetAuthors(), fix_initials)) {
        change = true;
    }
    if (m_Pat.IsSetApplicants() && CCleanup::CleanupAuthList(m_Pat.SetApplicants(), fix_initials)) {
        change = true;
    }
    if (m_Pat.IsSetAssignees() && CCleanup::CleanupAuthList(m_Pat.SetAssignees(), fix_initials)) {
        change = true;
    }

    if (m_Pat.IsSetCountry()) {
        if (NStr::Equal(m_Pat.GetCountry(), "USA")) {
            m_Pat.SetCountry("US");
            change = true;
        }
    }

    return change;
}


bool CCitLetCleaner::Clean(bool fix_initials, bool strip_serial)
{
    bool change = false;
    if (m_Let.IsSetCit() && m_Let.IsSetType() && m_Let.GetType() == CCit_let::eType_thesis) {
        CCitBookCleaner cleaner(m_Let.SetCit());
        if (cleaner.Clean(fix_initials, strip_serial)) {
            change = true;
        }
    }

    return change;
}


bool CMedlineEntryCleaner::Clean(bool fix_initials, bool strip_serial)
{
    bool change = false;
    if (m_Men.IsSetCit() && m_Men.GetCit().IsSetAuthors()) {
        change = CCleanup::CleanupAuthList(m_Men.SetCit().SetAuthors(), fix_initials);
    }

    return change;
}


END_SCOPE(objects)
END_NCBI_SCOPE
