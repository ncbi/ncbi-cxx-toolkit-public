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
 * Author:  .......
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using the following specifications:
 *   'efetch.xsd'.
 */

// standard includes
#include <ncbi_pch.hpp>
#include <objtools/eutils/efetch/PubmedArticle.hpp>


// generated includes
#include <objtools/eutils/efetch/PubmedBookArticle.hpp>

// generated classes

BEGIN_eutils_SCOPE // namespace eutils::

NCBI_USING_NAMESPACE_STD;
USING_NCBI_SCOPE;
USING_SCOPE(objects);

// destructor
CPubmedBookArticle::~CPubmedBookArticle(void)
{
}


BEGIN_LOCAL_NAMESPACE;

static CRef<CAuth_list> s_GetAuthorList(const CBookDocument::TAuthorList& list_author_list, bool empty_ok)
{
    CRef<CAuth_list> auth_list;
    if (!empty_ok && list_author_list.empty()) return auth_list;
    if (!empty_ok && all_of(list_author_list.begin(), list_author_list.end(),
        [](const CRef<CAuthorList>& author_list_element) -> bool {
            return author_list_element->GetAuthor().empty(); })) return auth_list;

    bool std_format = any_of(list_author_list.begin(), list_author_list.end(),
        [](const CRef<CAuthorList>& author_list_element) -> bool {
        auto list_author = author_list_element->GetAuthor();
        bool attr_editors = author_list_element->GetAttlist().IsSetType() &&
            author_list_element->GetAttlist().GetType() == CAuthorList::C_Attlist::eAttlist_Type_editors;
        return attr_editors || any_of( list_author.begin(), list_author.end(),
            [](const CRef<CAuthor>& author_element) -> bool { return author_element->GetLC().IsCollectiveName(); } );
        });
    CRef<CAuth_list::C_Names> auth_names(new CAuth_list::C_Names());
    if (!std_format) auth_names->SetMl();
    for (const auto& author_list_element : list_author_list) {
        for (const auto& author_element : author_list_element->GetAuthor() ) {
            CRef<CPerson_id> person(new CPerson_id());
            if (author_element->GetLC().IsCollectiveName()) {
                person->SetConsortium(s_CleanupText(
                    s_TextListToString(author_element->GetLC().GetCollectiveName().GetCollectiveName())));
                CRef<objects::CAuthor> auth(new objects::CAuthor());
                auth->SetName(*person);
                if (author_list_element->GetAttlist().GetType() == CAuthorList::C_Attlist::eAttlist_Type_editors)
                    auth->SetRole(objects::CAuthor::eRole_editor);
                auth_names->SetStd().push_back(auth);
            }
            else {
                string medline_name = s_CleanupText(s_GetAuthorMedlineName(*author_element));
                if (std_format) {
                    person->SetMl(medline_name);
                    CRef<objects::CAuthor> auth(new objects::CAuthor());
                    auth->SetName(*person);
                if (author_list_element->GetAttlist().GetType() == CAuthorList::C_Attlist::eAttlist_Type_editors)
                        auth->SetRole(objects::CAuthor::eRole_editor);
                    auth_names->SetStd().push_back(auth);
                }
                else {
                    auth_names->SetMl().push_back(medline_name);
                }
            }
        }
    }
    auth_list.Reset(new CAuth_list());
    auth_list->SetNames(*auth_names);
    return auth_list;
}


static CRef<CTitle> s_GetBookTitle(const CPubmedBookArticle& pubmed_article)
{
    CRef<CTitle> title(new CTitle());
    const auto& book = pubmed_article.GetBookDocument().GetBook();
    string title_str = s_TextListToString(book.GetBookTitle().GetBookTitle());
    CRef<CTitle::C_E> name(new CTitle::C_E());
    name->SetName(s_CleanupText(title_str));
    title->Set().push_back(name);
    if (book.IsSetIsbn()) {
        for (const auto& isbn_element : book.GetIsbn()) {
            CRef<CTitle::C_E> name(new CTitle::C_E());
            name->SetIsbn(s_CleanupText(isbn_element->Get()));
            title->Set().push_back(name);
        }
    }
    return title;
}


static int s_GetPubStatus(CPubMedPubDate::C_Attlist::EAttlist_PubStatus status)
{
    switch (status) {
    case CPubMedPubDate::C_Attlist::eAttlist_PubStatus_received:
        return ePubStatus_received;
    case CPubMedPubDate::C_Attlist::eAttlist_PubStatus_accepted:
        return ePubStatus_accepted;
    case CPubMedPubDate::C_Attlist::eAttlist_PubStatus_epublish:
        return ePubStatus_epublish;
    case CPubMedPubDate::C_Attlist::eAttlist_PubStatus_ppublish:
        return ePubStatus_ppublish;
    case CPubMedPubDate::C_Attlist::eAttlist_PubStatus_revised:
        return ePubStatus_revised;
    case CPubMedPubDate::C_Attlist::eAttlist_PubStatus_aheadofprint:
        return ePubStatus_aheadofprint;
    case CPubMedPubDate::C_Attlist::eAttlist_PubStatus_pmc:
        return ePubStatus_pmc;
    case CPubMedPubDate::C_Attlist::eAttlist_PubStatus_pmcr:
        return ePubStatus_pmcr;
    case CPubMedPubDate::C_Attlist::eAttlist_PubStatus_pubmed:
        return ePubStatus_pubmed;
    case CPubMedPubDate::C_Attlist::eAttlist_PubStatus_pubmedr:
        return ePubStatus_pubmedr;
    case CPubMedPubDate::C_Attlist::eAttlist_PubStatus_premedline:
        return ePubStatus_premedline;
    case CPubMedPubDate::C_Attlist::eAttlist_PubStatus_medline:
        return ePubStatus_medline;
    default:
        return ePubStatus_other;
    }
}


static CRef<CImprint> s_GetImprint(const CPubmedBookArticle& pubmed_article)
{
    const auto& book_document = pubmed_article.GetBookDocument();
    const auto& book = book_document.GetBook();
    const auto& book_data = pubmed_article.GetPubmedBookData();
    CRef<CImprint> imprint(new CImprint());
    imprint->SetDate(*s_GetDateFromPubDate(book.GetPubDate()));
    if (book.IsSetVolume()) {
        imprint->SetVolume(s_CleanupText(book.GetVolume()));
    }
    if (book_document.IsSetPagination()) {
        imprint->SetPages(s_GetPagination(book_document.GetPagination()));
    }
    const auto& list_language = book_document.GetLanguage();
    if (!list_language.empty()) {
        string langiages = accumulate(next(list_language.begin()), list_language.end(), (*list_language.begin())->Get(),
            [](const string& x, const CRef<CLanguage>& e) -> string { return x + "," + e->Get(); } );
        imprint->SetLanguage(langiages);
    }
    imprint->SetPubstatus(s_GetPublicationStatusId(book_data.GetPublicationStatus().Get()));
    if (book_data.IsSetHistory()) {
        CRef<CPubStatusDateSet> date_set(new CPubStatusDateSet());
        for (const auto& pub_date : book_data.GetHistory().GetPubMedPubDate()) {
            CRef<CPubStatusDate> pub_stat_date(new CPubStatusDate());
            pub_stat_date->SetPubstatus(s_GetPubStatus(pub_date->GetAttlist().GetPubStatus()));
            pub_stat_date->SetDate(*s_GetDateFromPubMedPubDate(*pub_date));
            date_set->Set().push_back(pub_stat_date);
        }
        imprint->SetHistory(*date_set);
    }
    return imprint;
}


static CRef<CCit_book> s_GetBook(const CPubmedBookArticle& pubmed_article)
{
    CRef<CCit_book> cit_book(new CCit_book());
    cit_book->SetTitle(*s_GetBookTitle(pubmed_article));
    if (pubmed_article.GetBookDocument().GetBook().IsSetCollectionTitle()) {
        CRef<CTitle::C_E> name(new CTitle::C_E());
        name->SetName(s_CleanupText(
            s_TextListToString(pubmed_article.GetBookDocument().GetBook().GetCollectionTitle().GetCollectionTitle())));
        CRef<CTitle> title(new CTitle());
        title->Set().push_back(name);
        cit_book->SetColl(*title);
    }
    auto auth_list = s_GetAuthorList(pubmed_article.GetBookDocument().GetBook().GetAuthorList(), true );
    if (auth_list) {
        cit_book->SetAuthors(*auth_list);
    }
    cit_book->SetImp(*s_GetImprint(pubmed_article));
    return cit_book;
}


static CRef<CCit_art> s_GetCitation(const CPubmedBookArticle& pubmed_article)
{
    CRef<CCit_art> cit_article(new CCit_art());
    auto title = s_GetTitle(pubmed_article.GetBookDocument());
    if (title) {
        cit_article->SetTitle(*title);
    }
    auto author_list = s_GetAuthorList(pubmed_article.GetBookDocument().GetAuthorList(), false);
    if (author_list) {
        cit_article->SetAuthors(*author_list);
    }
    CRef<CCit_art::C_From> from(new CCit_art::C_From());
    from->SetBook(*s_GetBook(pubmed_article));
    cit_article->SetFrom(*from);
    cit_article->SetIds(*s_GetArticleIdSet(pubmed_article.GetPubmedBookData().GetArticleIdList(), nullptr));
    return cit_article;
}


static CRef<CMedline_entry> s_GetMedlineEntry(const CPubmedBookArticle& pubmed_article)
{
    CRef<CMedline_entry> medline_entry(new CMedline_entry());
    if (pubmed_article.GetPubmedBookData().IsSetHistory()) {
        const auto& dates = pubmed_article.GetPubmedBookData().GetHistory().GetPubMedPubDate();
        auto created_date_element = find_if(dates.begin(), dates.end(),
                [](const CRef<CPubMedPubDate>& date_element) -> bool {
                return date_element->GetAttlist().GetPubStatus() == CPubMedPubDate::C_Attlist::eAttlist_PubStatus_entrez; });
        if (created_date_element != dates.end())
            medline_entry->SetEm(*s_GetDateFromPubMedPubDate(**created_date_element));
    }
    else {
        medline_entry->SetEm(*s_GetDateFromPubDate(pubmed_article.GetBookDocument().GetBook().GetPubDate()));
    }
    medline_entry->SetCit(*s_GetCitation(pubmed_article));
    if (pubmed_article.GetBookDocument().IsSetAbstract()) {
        string abstract_text = s_CleanupText(s_GetAbstractText(pubmed_article.GetBookDocument().GetAbstract()));
        medline_entry->SetAbstract(abstract_text);
    }
    if (pubmed_article.GetBookDocument().IsSetGrantList()) {
        s_FillGrants(medline_entry->SetIdnum(), pubmed_article.GetBookDocument().GetGrantList());
    }
    try {
        medline_entry->SetPmid(CPubMedId(
            NStr::StringToNumeric<TEntrezId>(pubmed_article.GetBookDocument().GetPMID().GetPMID())));
    }
    catch (...) {}
    medline_entry->SetStatus(medline_entry->eStatus_publisher);
    return medline_entry;
}


END_LOCAL_NAMESPACE;


CRef<CPubmed_entry> CPubmedBookArticle::ToPubmed_entry(void) const
{
    CRef<CPubmed_entry> pubmed_entry(new CPubmed_entry());
    try {
        pubmed_entry->SetPmid(CPubMedId(
            NStr::StringToNumeric<TEntrezId>(GetBookDocument().GetPMID().GetPMID())));
    }
    catch (...) {}
    pubmed_entry->SetMedent(*s_GetMedlineEntry(*this));
    return pubmed_entry;
}


END_eutils_SCOPE // namespace eutils::

/* Original file checksum: lines: 53, chars: 1708, CRC32: d8a53f84 */
