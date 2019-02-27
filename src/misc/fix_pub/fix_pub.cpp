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
 * Author:  Alexey Dobronadezhdin
 *
 * File Description:
 *   Code for fixing up publications.
 *   MedArch lookup and post-processing utilities.
 *   Based on medutil.c written by James Ostell.
 */

#include <ncbi_pch.hpp>

#include <objects/biblio/ArticleId.hpp>
#include <objects/biblio/ArticleIdSet.hpp>
#include <objects/biblio/Author.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/biblio/Title.hpp>
#include <objects/general/Name_std.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/medline/Medline_entry.hpp>

#include <objects/pub/Pub.hpp>

#include <../src/objtools/cleanup/cleanup_utils.hpp>

#include "fix_pub.hpp"
#include "fix_pub_aux.hpp"

#include <objects/mla/Title_msg.hpp>
#include <objects/mla/Title_msg_list.hpp>
#include <objects/mla/mla_client.hpp>
#include <misc/eutils_client/eutils_client.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

#define ERR_POST_TO_LISTENER(listener, severity, code, subcode, message) \
{ \
    if (listener) { \
        CNcbiOstrstream ostr; \
        ostr << message; \
        CMessage_Basic msg(ostr.str(), severity, code, subcode); \
        listener->PostMessage(msg); \
    } \
};

#define ERR_REFERENCE  1
#define ERR_REFERENCE_MuidNotFound  1
#define ERR_REFERENCE_SuccessfulMuidLookup  2
#define ERR_REFERENCE_OldInPress  3
#define ERR_REFERENCE_No_reference  4
#define ERR_REFERENCE_Multiple_ref  5
#define ERR_REFERENCE_Multiple_muid  6
#define ERR_REFERENCE_MedlineMatchIgnored  7
#define ERR_REFERENCE_MuidMissmatch  8
#define ERR_REFERENCE_NoConsortAuthors  9
#define ERR_REFERENCE_DiffConsortAuthors  10
#define ERR_REFERENCE_PmidMissmatch  11
#define ERR_REFERENCE_Multiple_pmid  12
#define ERR_REFERENCE_FailedToGetPub  13
#define ERR_REFERENCE_MedArchMatchIgnored  14
#define ERR_REFERENCE_SuccessfulPmidLookup  15
#define ERR_REFERENCE_PmidNotFound  16
#define ERR_REFERENCE_NoPmidJournalNotInPubMed  17
#define ERR_REFERENCE_PmidNotFoundInPress  18
#define ERR_REFERENCE_NoPmidJournalNotInPubMedInPress  19

#define ERR_PRINT  2
#define ERR_PRINT_Failed  1

struct SErrorSubcodes
{
    string m_error_str;
    map<int, string> m_sub_errors;
};

static map<int, SErrorSubcodes> ERROR_CODE_STR =
{
    { ERR_REFERENCE,{ "REFERENCE",
    {
        { ERR_REFERENCE_MuidNotFound, "MuidNotFound" },
        { ERR_REFERENCE_SuccessfulMuidLookup, "SuccessfulMuidLookup" },
        { ERR_REFERENCE_OldInPress, "OldInPress" },
        { ERR_REFERENCE_No_reference, "No_reference" },
        { ERR_REFERENCE_Multiple_ref, "Multiple_ref" },
        { ERR_REFERENCE_Multiple_muid, "Multiple_muid" },
        { ERR_REFERENCE_MedlineMatchIgnored, "MedlineMatchIgnored" },
        { ERR_REFERENCE_MuidMissmatch, "MuidMissmatch" },
        { ERR_REFERENCE_NoConsortAuthors, "NoConsortAuthors" },
        { ERR_REFERENCE_DiffConsortAuthors, "DiffConsortAuthors" },
        { ERR_REFERENCE_PmidMissmatch, "PmidMissmatch" },
        { ERR_REFERENCE_Multiple_pmid, "Multiple_pmid" },
        { ERR_REFERENCE_FailedToGetPub, "FailedToGetPub" },
        { ERR_REFERENCE_MedArchMatchIgnored, "MedArchMatchIgnored" },
        { ERR_REFERENCE_SuccessfulPmidLookup, "SuccessfulPmidLookup" },
        { ERR_REFERENCE_PmidNotFound, "PmidNotFound" },
        { ERR_REFERENCE_NoPmidJournalNotInPubMed, "NoPmidJournalNotInPubMed" },
        { ERR_REFERENCE_PmidNotFoundInPress, "PmidNotFoundInPress" },
        { ERR_REFERENCE_NoPmidJournalNotInPubMedInPress, "NoPmidJournalNotInPubMedInPress" }
    }
    } },
    { ERR_PRINT,{ "PRINT",
    {
        { ERR_PRINT_Failed, "Failed" }
    }
    } }
};

string CPubFixing::GetErrorId(int err_code, int err_sub_code)
{
    string ret;

    const auto& err_category = ERROR_CODE_STR.find(err_code);
    if (err_category != ERROR_CODE_STR.end()) {

        const auto& error_sub_code_str = err_category->second.m_sub_errors.find(err_sub_code);
        if (error_sub_code_str != err_category->second.m_sub_errors.end()) {
            ret = err_category->second.m_error_str;
            ret += '.';
            ret += error_sub_code_str->second;
        }
    }

    return ret;
}

//   MedlineToISO(tmp)
//       converts a MEDLINE citation to ISO/GenBank style

void MedlineToISO(CCit_art& cit_art)
{
    if (cit_art.IsSetAuthors()) {

        CAuth_list& auths = cit_art.SetAuthors();
        if (auths.IsSetNames()) {
            if (auths.GetNames().IsMl()) {
                ConvertAuthorContainerMlToStd(auths);
            }
            else if (auths.GetNames().IsStd()) {
                for(auto& auth : auths.SetNames().SetStd()) {
                    if (auth->IsSetName() && auth->GetName().IsMl()) {
                        auth->SetName().Assign(*ConvertMltoSTD(auth->GetName().GetMl()));
                    }
                }
            }
        }
    }

    if (!cit_art.IsSetFrom() || !cit_art.GetFrom().IsJournal())
        return;

    // from a journal - get iso_jta
    CCit_jour& journal = cit_art.SetFrom().SetJournal();

    auto IsIso_jta = [](const CRef<CTitle::C_E>& title) -> bool { return title->IsIso_jta(); };

    if (journal.IsSetTitle() && journal.GetTitle().IsSet()) {

        auto& titles = journal.SetTitle().Set();

        if (find_if(titles.begin(), titles.end(), IsIso_jta) == titles.end()) {
            // no iso_jta
            
            CTitle::C_E& first_title = *titles.front();
            const string& title_str = journal.SetTitle().GetTitle(first_title);

            CRef<CTitle> title_new(new CTitle);
            CRef<CTitle::C_E> type_new(new CTitle::C_E);
            type_new->SetIso_jta(title_str);
            title_new->Set().push_back(type_new);

            CRef<CTitle_msg> msg_new(new CTitle_msg);
            msg_new->SetType(eTitle_type_iso_jta);
            msg_new->SetTitle(*title_new);

            CRef<CTitle_msg_list> msg_list_new;
            try {
                CMLAClient mla;
                msg_list_new = mla.AskGettitle(*msg_new);
            }
            catch (exception &) {
                // msg_list_new stays empty
            }

            if (msg_list_new.NotEmpty() && msg_list_new->IsSetTitles()) {

                bool gotit = false;
                for (auto& item : msg_list_new->GetTitles())
                {
                    const CTitle &cur_title = item->GetTitle();

                    if (cur_title.IsSet()) {

                        auto iso_jta_title = find_if(cur_title.Get().begin(), cur_title.Get().end(), IsIso_jta);
                        if (iso_jta_title != cur_title.Get().end()) {
                            gotit = true;
                            first_title.SetIso_jta((*iso_jta_title)->GetIso_jta());
                            break;
                        }
                    }

                    if (gotit)
                        break;
                }
            }
        }
    }

    if (journal.IsSetImp()) {
        // remove Eng language
        if (journal.GetImp().IsSetLanguage() && journal.GetImp().GetLanguage() == "Eng")
            journal.SetImp().ResetLanguage();
    }
}

//   SplitMedlineEntry(mep)
//      splits a medline entry into 2 pubs (1 muid, 1 Cit-art)
//      converts Cit-art to ISO/GenBank style
//      deletes original medline entry
void SplitMedlineEntry(CPub_equiv::Tdata& medlines)
{
    if (medlines.size() != 1) {
        return;
    }

    CPub& pub = *medlines.front();
    CMedline_entry& medline = pub.SetMedline();
    if (!medline.IsSetCit() && medline.IsSetPmid() && medline.GetPmid() < 0) {
        return;
    }

    CRef<CPub> pmid;
    if (medline.GetPmid() > 0) {
        pmid.Reset(new CPub);
        pmid->SetPmid(medline.GetPmid());
    }

    CRef<CPub> cit_art;
    if (medline.IsSetCit()) {
        cit_art.Reset(new CPub);
        cit_art->SetArticle(medline.SetCit());
        MedlineToISO(cit_art->SetArticle());
    }

    medlines.clear();

    if (pmid.NotEmpty())
        medlines.push_back(pmid);

    if (cit_art.NotEmpty())
        medlines.push_back(cit_art);
}

bool IsInpress(const CCit_art& cit_art)
{
    if (!cit_art.IsSetFrom())
        return false;

    bool ret = false;
    if (cit_art.GetFrom().IsJournal()) {
        const CCit_jour& journal = cit_art.GetFrom().GetJournal();
        ret = journal.IsSetImp() && journal.GetImp().IsSetPrepub() && journal.GetImp().GetPrepub() == CImprint::ePrepub_in_press;
    }
    else if (cit_art.GetFrom().IsBook() || cit_art.GetFrom().IsProc()) {
        const CCit_book& book = cit_art.GetFrom().GetBook();
        ret = book.IsSetImp() && book.GetImp().IsSetPrepub() && book.GetImp().GetPrepub() == CImprint::ePrepub_in_press;
    }
    return ret;
}


bool MULooksLikeISSN(const string& str)
{
    static const size_t ISSN_SIZE = 9;
    static const size_t ISSN_DASH_POS = 4;
    static const size_t ISSN_X_POS = 8;

    if (NStr::IsBlank(str) || str.size() != ISSN_SIZE || str[ISSN_DASH_POS] != '-') {
        return false;
    }

    for (size_t i = 0; i < ISSN_SIZE; ++i) {
        char ch = str[i];
        if (isdigit(ch) || (ch == '-' && i == ISSN_DASH_POS) || (ch == 'X' && i == ISSN_X_POS)) {
            continue;
        }
        return false;
    }

    return true;
}


bool MUIsJournalIndexed(const string* journal)
{
    if (journal == nullptr || journal->empty()) {
        return false;
    }

    string title;
    transform(journal->begin(), journal->end(), title.end(),
        [](char c) {
        return (c == '(' || c == ')' || c == '.') ? ' ' : c;
    });

    title = NStr::Sanitize(title);

    CEutilsClient eutils;

    static const int MAX_ITEMS = 200;
    eutils.SetMaxReturn(MAX_ITEMS);

    vector<string> ids;

    static const string EUTILS_DATABASE("nlmcatalog");

    try {
        if (MULooksLikeISSN(title)) {
            eutils.Search(EUTILS_DATABASE, title + "[issn]", ids);
        }

        NStr::ReplaceInPlace(title, " ", "+");
        if (ids.empty()) {
            eutils.Search(EUTILS_DATABASE, title + "[multi]+AND+ncbijournals[sb]", ids);
        }

        if (ids.empty()) {
            eutils.Search(EUTILS_DATABASE, title + "[jo]", ids);
        }
    }
    catch (CException&) {
        return false;
    }

    if (ids.size() != 1) {
        return false;
    }


    // getting the indexing status of the journal found
    static const string SUMMARY_VERSION("2.0");
    xml::document doc;
    eutils.Summary(EUTILS_DATABASE, ids, doc, SUMMARY_VERSION);

    const xml::node& root_node = doc.get_root_node();
    xml::node_set nodes(root_node.run_xpath_query("//DocumentSummarySet/DocumentSummary/CurrentIndexingStatus/text()"));

    string status;
    if (nodes.size() == 1) {
        status = nodes.begin()->get_content();
    }

    return string(status) == "Y";
}


void PrintPub(const CCit_art& cit_art, bool found, bool auth, long muid, IMessageListener* err_log)
{
    const string* first_name = nullptr,
                * last_name = nullptr;

    if (cit_art.IsSetAuthors() && cit_art.GetAuthors().IsSetNames()) {

        if (cit_art.GetAuthors().GetNames().IsStd()) {

            const CAuthor& first_author = *cit_art.GetAuthors().GetNames().GetStd().front();

            if (first_author.IsSetName()) {
                if (first_author.GetName().IsName()) {
                    const CName_std& namestd = first_author.GetName().GetName();
                    if (namestd.IsSetLast()) {
                        last_name = &namestd.GetLast();
                    }
                    if (namestd.IsSetInitials()) {
                        first_name = &namestd.GetInitials();
                    }
                }
                else if (first_author.GetName().IsConsortium()) {
                    last_name = &first_author.GetName().GetConsortium();
                }
            }
        }
        else {
            last_name = &cit_art.GetAuthors().GetNames().GetStr().front();
        }
    }
    else {
        ERR_POST_TO_LISTENER(err_log, eDiag_Warning, ERR_PRINT, ERR_PRINT_Failed, "Authors NULL");
    }

    const CImprint* imprint = nullptr;
    const CTitle* title = nullptr;

    if (cit_art.IsSetFrom()) {
        if (cit_art.GetFrom().IsJournal()) {
            const CCit_jour& journal = cit_art.GetFrom().GetJournal();

            if (journal.IsSetTitle()) {
                title = &journal.GetTitle();
            }

            if (journal.IsSetImp()) {
                imprint = &journal.GetImp();
            }
        }
        else if (cit_art.GetFrom().IsBook()) {
            const CCit_book& book = cit_art.GetFrom().GetBook();

            if (book.IsSetTitle()) {
                title = &book.GetTitle();
            }

            if (book.IsSetImp()) {
                imprint = &book.GetImp();
            }
        }
    }

    static const string UNKNOWN_JOURNAL("journal unknown");
    const string* title_str = &UNKNOWN_JOURNAL;

    if (title && title->IsSet() && !title->Get().empty()) {

        const CTitle::C_E& first_title = *title->Get().front();
        const string& str = title->GetTitle(first_title);

        if (!str.empty())
            title_str = &str;
    }


    static const string NO_PAGE("no page number");
    static const string NO_VOL("no volume number");

    const string* vol = &NO_VOL,
                * page = &NO_PAGE;
    int year = 0;
    bool in_press = false;

    if (imprint) {

        if (imprint->IsSetVolume()) {
            vol = &imprint->GetVolume();
        }

        if (imprint->IsSetPages()) {
            page = &imprint->GetPages();
        }

        if (imprint->IsSetDate() && imprint->GetDate().IsStd() && imprint->GetDate().GetStd().IsSetYear()) {
            year = imprint->GetDate().GetStd().GetYear();
        }

        in_press = imprint->IsSetPrepub() && imprint->GetPrepub() == CImprint::ePrepub_in_press;
    }

    if (auth) {
        ERR_POST_TO_LISTENER(err_log, eDiag_Error, ERR_REFERENCE, ERR_REFERENCE_MedArchMatchIgnored,
            "Too many author name differences: " << muid << "|" << last_name << " " << first_name << "|" << title_str << "|(" << year << ")|" << vol << "|" << page);
        return;
    }

    if (in_press) {

        int cur_year = CDate_std(CTime(CTime::eCurrent)).GetYear();
        static const int YEAR_MAX_DIFF = 2;

        if (year && cur_year - year > YEAR_MAX_DIFF) {
            ERR_POST_TO_LISTENER(err_log, eDiag_Warning, ERR_REFERENCE, ERR_REFERENCE_OldInPress,
                "encountered in-press article more than 2 years old: " << last_name << " " << first_name << "|" << title_str << "|(" << year << ")|" << vol << "|" << page);
        }
    }

    if (found) {
        ERR_POST_TO_LISTENER(err_log, eDiag_Info, ERR_REFERENCE, ERR_REFERENCE_SuccessfulPmidLookup,
            muid << "|" << last_name << " " << first_name << "|" << title_str << "|(" << year << ")|" << vol << "|" << page);
    }
    else if (MUIsJournalIndexed(title_str)) {
        if (muid) {
            ERR_POST_TO_LISTENER(err_log, eDiag_Warning, ERR_REFERENCE, in_press ? ERR_REFERENCE_PmidNotFoundInPress : ERR_REFERENCE_PmidNotFound,
                ">>" << muid << "<<|" << last_name << " " << first_name << "|" << title_str << "|(" << year << ")|" << vol << "|" << page);
        }
        else {
            ERR_POST_TO_LISTENER(err_log, eDiag_Warning, ERR_REFERENCE, in_press ? ERR_REFERENCE_PmidNotFoundInPress : ERR_REFERENCE_PmidNotFound,
                last_name << " " << first_name << "|" << title_str << "|(" << year << ")|" << vol << "|" << page);
        }
    }
    else {
        if (muid) {
            ERR_POST_TO_LISTENER(err_log, eDiag_Info, ERR_REFERENCE, in_press ? ERR_REFERENCE_NoPmidJournalNotInPubMedInPress : ERR_REFERENCE_NoPmidJournalNotInPubMed,
                ">>" << muid << "<<|" << last_name << " " << first_name << "|" << title_str << "|(" << year << ")|" << vol << "|" << page);
        }
        else {
            ERR_POST_TO_LISTENER(err_log, eDiag_Warning, ERR_REFERENCE, in_press ? ERR_REFERENCE_NoPmidJournalNotInPubMedInPress : ERR_REFERENCE_NoPmidJournalNotInPubMed,
                last_name << " " << first_name << "|" << title_str << "|(" << year << ")|" << vol << "|" << page);
        }
    }
}


bool IsFromBook(const CCit_art& art)
{
    return art.IsSetFrom() && art.GetFrom().IsBook();
}


CRef<CCit_art> CPubFixing::FetchPubPmId(int pmid)
{
    CRef<CCit_art> cit_art;
    if (pmid < 0)
        return cit_art;

    CRef<CPub> pub;
    try {
        CMLAClient mla;
        pub = mla.AskGetpubpmid(CPubMedId(pmid));
    }
    catch (exception &) {
        pub.Reset();
    }

    if (pub.NotEmpty() && pub->IsArticle()) {
        cit_art.Reset(new CCit_art);
        cit_art->Assign(pub->GetArticle());
        MedlineToISO(*cit_art);
    }

    return cit_art;
}

static const size_t MAX_MATCH_COEFF = 3;

bool TenAuthorsCompare(CCit_art& cit_old, CCit_art& cit_new)
{
    _ASSERT(cit_old.IsSetAuthors() && cit_new.IsSetAuthors() &&
        cit_old.GetAuthors().IsSetNames() && cit_new.GetAuthors().IsSetNames() && "Both arguments should have valid author's names at this point");

    const CAuth_list::C_Names& old_names = cit_old.GetAuthors().GetNames();
    const CAuth_list::C_Names& new_names = cit_new.GetAuthors().GetNames();

    auto StrNotEmpty = [](const string& str) -> bool { return !str.empty();  };
    size_t new_num_of_authors = count_if(new_names.GetStr().begin(), new_names.GetStr().end(), StrNotEmpty),
           num_of_authors = count_if(old_names.GetStr().begin(), old_names.GetStr().end(), StrNotEmpty);

    size_t match = 0;
    for (auto& name : old_names.GetStr()) {

        if (!name.empty()) {
            if (NStr::FindNoCase(new_names.GetStr(), name) != nullptr) {
                ++match;
            }
        }
    }

    size_t min_num_of_authors = min(num_of_authors, new_num_of_authors);

    if (min_num_of_authors > MAX_MATCH_COEFF * match) {
        return false;
    }

    static const size_t MAX_AUTHORS = 10;
    if (min_num_of_authors > MAX_AUTHORS) {
        cit_new.SetAuthors(cit_old.SetAuthors());
        cit_old.ResetAuthors();
    }

    return true;
}

size_t ExtractConsortiums(const CAuth_list::C_Names::TStd& names, CAuth_list::C_Names::TStr& extracted)
{
    size_t num_of_names = 0;

    for (auto& name: names)
    {
        const CAuthor& auth = *name;
        if (auth.IsSetName() && auth.GetName().IsName()) {
            ++num_of_names;
        }
        else if (auth.IsSetName() && auth.GetName().IsConsortium()) {

            const string& cur_consortium = auth.GetName().GetConsortium();
            extracted.push_back(cur_consortium);
        }
    }

    extracted.sort([](const string& a, const string& b) { return NStr::CompareNocase(a, b);  });

    return num_of_names;
}


void GetFirstTenNames(const CAuth_list::C_Names::TStd& names, list<CTempString>& res)
{
    static const size_t MAX_EXTRACTED = 10;
    size_t extracted = 0;

    for (auto& name : names) {
        if (name->IsSetName() && name->GetName().IsName() && name->GetName().GetName().IsSetLast()) {
            res.push_back(name->GetName().GetName().GetLast());
            ++extracted;

            if (extracted == MAX_EXTRACTED) {
                break;
            }
        }
    }
}


bool TenAuthorsProcess(CCit_art& cit, CCit_art& new_cit, IMessageListener* err_log)
{
    if (!new_cit.IsSetAuthors() || !new_cit.GetAuthors().IsSetNames()) {
        if (cit.IsSetAuthors()) {
            new_cit.SetAuthors(cit.SetAuthors());
            cit.ResetAuthors();
        }
        return true;
    }

    if (!cit.IsSetAuthors() || !cit.GetAuthors().IsSetNames() ||
        cit.GetAuthors().GetNames().Which() != new_cit.GetAuthors().GetNames().Which()) {
        return true;
    }

    if (!cit.GetAuthors().GetNames().IsStd()) {
        return TenAuthorsCompare(cit, new_cit);
    }

    CAuth_list::C_Names::TStr old_consortiums;
    size_t num_names = ExtractConsortiums(cit.GetAuthors().GetNames().GetStd(), old_consortiums);

    CAuth_list::C_Names::TStr new_consortiums;
    size_t new_num_names = ExtractConsortiums(new_cit.GetAuthors().GetNames().GetStd(), new_consortiums);

    if (!old_consortiums.empty()) {

        string old_cons_list = NStr::Join(old_consortiums, ";");
        if (new_consortiums.empty()) {

            ERR_POST_TO_LISTENER(err_log, eDiag_Warning, ERR_REFERENCE, ERR_REFERENCE_NoConsortAuthors,
                "Publication as returned by MedArch lacks consortium authors of the original publication : \"" << old_cons_list << "\".");

            for_each(old_consortiums.begin(), old_consortiums.end(),
                [&new_cit](const string& consortium) {

                CRef<CAuthor> auth(new CAuthor);
                auth->SetName().SetConsortium(consortium);

                new_cit.SetAuthors().SetNames().SetStd().push_front(auth);
            });
        }
        else {

            string new_cons_list = NStr::Join(new_consortiums, ";");
            if (NStr::CompareNocase(old_cons_list, new_cons_list)) {
                ERR_POST_TO_LISTENER(err_log, eDiag_Warning, ERR_REFERENCE, ERR_REFERENCE_DiffConsortAuthors,
                    "Consortium author names differ. Original is \"" << old_cons_list << "\". MedArch's is \"" << new_cons_list << "\".");
            }
        }

        if (num_names == 0) {
            return true;
        }
    }

    list<CTempString> new_author_names;
    GetFirstTenNames(new_cit.GetAuthors().GetNames().GetStd(), new_author_names);

    size_t match = 0;

    for (auto& name: cit.GetAuthors().GetNames().GetStd())
    {
        const CAuthor& auth = *name;
        if (auth.IsSetName() && auth.GetName().IsName() && auth.GetName().GetName().IsSetLast()) {

            const string& last_name = auth.GetName().GetName().GetLast();
            if (find_if(new_author_names.begin(), new_author_names.end(), 
                [&last_name](const CTempString& cur_name)
                {
                    return NStr::CompareNocase(last_name, cur_name) == 0;
                }) != new_author_names.end()) {

                match++;
            }
        }
    }

    size_t min_num_names = min(num_names, new_num_names);
    if (min_num_names > MAX_MATCH_COEFF * match) {
        return false;
    }

    // TODO: figure out what is the sense of all magic numbers below
    if (new_num_names == 0 || (num_names > 10 && new_num_names < 12) || (num_names > 25 && new_num_names < 27)) {
        new_cit.SetAuthors(cit.SetAuthors());
        cit.ResetAuthors();
    }

    return true;
}


void MergeNonPubmedPubIds(const CCit_art& cit_old, CCit_art& cit_new)
{
    if (!cit_old.IsSetIds()) {
        return;
    }

    const CArticleIdSet& old_ids = cit_old.GetIds();

    for (auto& cur_id: old_ids.Get()) {

        if (!cur_id->IsDoi() && !cur_id->IsOther()) {
            continue;
        }

        bool found = false;
        if (cit_new.IsSetIds()) {

            auto& new_ids = cit_new.GetIds().Get();
            found = find_if(new_ids.begin(), new_ids.end(),
                [&cur_id](const CRef<CArticleId>& new_id)
            {
                if (cur_id->Which() != new_id->Which()) {
                    return false;
                }

                if (new_id->IsDoi()) {
                    return true;
                }

                bool res = cur_id->GetOther().IsSetDb() == new_id->GetOther().IsSetDb();
                if (res && cur_id->GetOther().IsSetDb()) {
                    res = cur_id->GetOther().GetDb() == new_id->GetOther().GetDb();
                }
                return res;
            }) != new_ids.end();
        }

        if (!found) {
            cit_new.SetIds().Set().push_front(cur_id);
        }
    }
}


bool IsInPress(const CCit_art& cit_art)
{
    if (!cit_art.IsSetFrom() || !cit_art.GetFrom().IsJournal() || !cit_art.GetFrom().GetJournal().IsSetTitle()) {
        return true;
    }

    const CCit_jour& journal = cit_art.GetFrom().GetJournal();
    if (!journal.IsSetImp()) {
        return true;
    }

    if (!journal.GetImp().IsSetVolume() || !journal.GetImp().IsSetPages() || !journal.GetImp().IsSetDate()) {
        return true;
    }

    return false;
}


void PropagateInPress(bool inpress, CCit_art& cit_art)
{
    if (!inpress)
        return;

    if (!IsInPress(cit_art) || !cit_art.IsSetFrom()) {
        return;
    }

    if (cit_art.GetFrom().IsJournal()) {
        if (cit_art.GetFrom().GetJournal().IsSetImp())
            cit_art.SetFrom().SetJournal().SetImp().SetPrepub(CImprint::ePrepub_in_press);
    }
    else if (cit_art.GetFrom().IsBook() || cit_art.GetFrom().IsProc()) {
        if (cit_art.GetFrom().GetBook().IsSetImp())
            cit_art.SetFrom().SetBook().SetImp().SetPrepub(CImprint::ePrepub_in_press);
    }
}


void FixPubEquivAppendPmid(long muid, CPub_equiv::Tdata& pmids, IMessageListener* err_log)
{
    long oldpmid = pmids.empty() ? 0 : pmids.front()->GetPmid();

    long newpmid = -1;
    try {
        CMLAClient mla;
        newpmid = mla.AskUidtopmid(muid);
    }
    catch (exception &) {
        // newpmid == -1;
    }

    if (oldpmid < 1 && newpmid < 1) {
        return;
    }

    if (oldpmid > 0 && newpmid > 0 && oldpmid != newpmid) {
        ERR_POST_TO_LISTENER(err_log, eDiag_Error, ERR_REFERENCE, ERR_REFERENCE_PmidMissmatch,
            "OldPMID=" << oldpmid << " doesn't match lookup (" << newpmid << "). Keeping lookup.");
    }

    if (pmids.empty()) {
        CRef<CPub> pmid_pub(new CPub);
        pmids.push_back(pmid_pub);
    }

    pmids.front()->SetPmid().Set((newpmid > 0) ? newpmid : oldpmid);
}


void CPubFixing::FixPubEquiv(CPub_equiv& pub_equiv)
{
    CPub_equiv::Tdata muids,
        pmids,
        medlines,
        others,
        cit_arts;

    if (pub_equiv.IsSet()) {
        for (auto& pub: pub_equiv.Set())
        {
            if (pub->IsMuid()) {
                muids.push_back(pub);
            }
            else if (pub->IsPmid()) {
                pmids.push_back(pub);
            }
            else if (pub->IsArticle()) {
                if (IsFromBook(pub->GetArticle())) {
                    others.push_back(pub);
                }
                else {
                    cit_arts.push_back(pub);
                }
            }
            else if (pub->IsMedline()) {
                medlines.push_back(pub);
            }
            else {
                others.push_back(pub);
            }
        }
    }

    auto& pub_list = pub_equiv.Set();
    pub_list.clear();

    if ((!muids.empty() || !pmids.empty()) && !m_always_lookup) {
        // pmid or muid is present
        pub_list.splice(pub_list.end(), cit_arts);
        pub_list.splice(pub_list.end(), muids);
        pub_list.splice(pub_list.end(), pmids);
        pub_list.splice(pub_list.end(), medlines);
        pub_list.splice(pub_list.end(), others);
        return;
    }

    pub_list.splice(pub_list.end(), others);

    if (!medlines.empty())
    {
        if (medlines.size() > 1) {
            ERR_POST_TO_LISTENER(m_err_log, eDiag_Warning, ERR_REFERENCE, ERR_REFERENCE_Multiple_ref, "More than one Medline entry in Pub-equiv");
            medlines.resize(1);
        }

        SplitMedlineEntry(medlines);
        pub_list.splice(pub_list.end(), medlines);
    }

    long oldpmid = 0;
    if (pmids.size() > 1) {

        // more than one, just take the first
        oldpmid = pmids.front()->GetPmid();
        for (auto& pub: pmids) {
            if (pub->GetPmid() != oldpmid) {
                ERR_POST_TO_LISTENER(m_err_log, eDiag_Warning, ERR_REFERENCE, ERR_REFERENCE_Multiple_pmid,
                    "Two different pmids in Pub-equiv [" << oldpmid << "] [" << pub->GetPmid() << "]");
            }
        }
        pmids.resize(1);
    }

    long oldmuid = 0;
    if (muids.size() > 1) {
        // more than one, just take the first
        oldmuid = muids.front()->GetMuid();
        for (auto& pub : muids) {
            if (pub->GetMuid() != oldmuid) {
                ERR_POST_TO_LISTENER(m_err_log, eDiag_Warning, ERR_REFERENCE, ERR_REFERENCE_Multiple_pmid,
                    "Two different muids in Pub-equiv  [" << oldmuid << "] [" << pub->GetMuid() << "]");
            }
        }
        muids.resize(1);
    }

    if (!cit_arts.empty()) {
        if (cit_arts.size() > 1) {
            // ditch extras
            ERR_POST_TO_LISTENER(m_err_log, eDiag_Warning, ERR_REFERENCE, ERR_REFERENCE_Multiple_ref, "More than one Cit-art in Pub-equiv");
            cit_arts.resize(1);
        }

        CCit_art* cit_art = &cit_arts.front()->SetArticle();
        bool inpress = IsInpress(*cit_art);

        CRef<CPub> new_pub(new CPub);
        new_pub->SetArticle(*cit_art);

        long pmid = 0;
        try {
            CMLAClient mla;
            pmid = mla.AskCitmatchpmid(*new_pub);
        }
        catch (exception &) {
            // pmid == 0
        }

        if (pmid) {

            PrintPub(*cit_art, true, false, pmid, m_err_log);

            if (oldpmid > 0 && oldpmid != pmid) {
                // already had a pmid
                ERR_POST_TO_LISTENER(m_err_log, eDiag_Error, ERR_REFERENCE, ERR_REFERENCE_PmidMissmatch,
                    "OldPMID=" << oldpmid << " doesn't match lookup (" << pmid << "). Keeping lookup.");
            }

            bool set_pmid = true;
            if (m_replace_cit) {

                CRef<CCit_art> new_cit_art = FetchPubPmId(pmid);

                if (new_cit_art.NotEmpty()) {

                    if (TenAuthorsProcess(*cit_art, *new_cit_art, m_err_log)) {
                        if (pmids.empty()) {
                            CRef<CPub> pmid_pub(new CPub);
                            pmids.push_back(pmid_pub);
                        }

                        pmids.front()->SetPmid().Set(pmid);
                        pub_list.splice(pub_list.end(), pmids);

                        CRef<CPub> cit_pub(new CPub);
                        cit_pub->SetArticle(*new_cit_art);
                        pub_list.push_back(cit_pub);

                        if (m_merge_ids) {
                            MergeNonPubmedPubIds(*cit_art, cit_pub->SetArticle());
                        }

                        cit_arts.clear();
                        cit_arts.push_back(cit_pub);
                        cit_art = new_cit_art;
                    }
                    else {
                        pmids.clear();

                        PrintPub(*cit_art, false, true, pmid, m_err_log);
                        pub_list.splice(pub_list.end(), cit_arts);
                    }

                    set_pmid = false;
                }
                else {
                    ERR_POST_TO_LISTENER(m_err_log, eDiag_Error, ERR_REFERENCE, ERR_REFERENCE_FailedToGetPub,
                        "Failed to get pub from MedArch server for pmid = " << pmid << ". Input one is preserved.");
                }
            }

            if (set_pmid) {
                if (pmids.empty()) {
                    CRef<CPub> pmid_pub(new CPub);
                    pmids.push_back(pmid_pub);
                }

                pmids.front()->SetPmid().Set(pmid);
                pub_list.splice(pub_list.end(), pmids);

                MedlineToISO(*cit_art);

                pub_list.splice(pub_list.end(), cit_arts);
            }

            muids.clear();
            PropagateInPress(inpress, *cit_art);
            return;
        }

        PrintPub(*cit_art, false, false, oldpmid, m_err_log);
        PropagateInPress(inpress, *cit_art);
        pub_list.splice(pub_list.end(), cit_arts);

        return;
    }

    if (oldpmid) {
        // have a pmid but no cit-art

        CRef<CCit_art> new_cit_art = FetchPubPmId(oldpmid);

        if (new_cit_art.NotEmpty()) {
            if (m_replace_cit) {
                MedlineToISO(*new_cit_art);
                CRef<CPub> cit_pub(new CPub);
                cit_pub->SetArticle(*new_cit_art);
                pub_list.push_back(cit_pub);
            }

            pub_list.splice(pub_list.end(), pmids);
            return;
        }
        ERR_POST_TO_LISTENER(m_err_log, eDiag_Warning, ERR_REFERENCE, ERR_REFERENCE_No_reference,
            "Cant find article for pmid [" << oldpmid << "]");
    }

    if (oldpmid > 0) {
        pub_list.splice(pub_list.end(), pmids);
    }
    else if (oldmuid > 0) {
        pub_list.splice(pub_list.end(), muids);
        FixPubEquivAppendPmid(oldmuid, pmids, m_err_log);
        pub_list.splice(pub_list.end(), pmids);
    }
}


// Tries to make any Pub into muid / cit - art
void CPubFixing::FixPub(CPub& pub)
{
    switch (pub.Which()) {

    case CPub::e_Medline:
    {
        CRef<CPub_equiv> pub_equiv(new CPub_equiv);
        pub_equiv->Set().push_back(CRef<CPub>(new CPub));
        pub_equiv->Set().front()->Assign(pub);

        SplitMedlineEntry(pub_equiv->Set());
        pub.SetEquiv().Assign(*pub_equiv);
    }
    break;

    case CPub::e_Article:
    {
        CCit_art& cit_art = pub.SetArticle();
        if (cit_art.IsSetFrom() && cit_art.GetFrom().IsBook()) {
            return;
        }

        int pmid = 0;
        try {
            CMLAClient mla;
            pmid = mla.AskCitmatchpmid(pub);
        }
        catch (exception &) {
            // pmid == 0;
        }

        if (pmid > 0) {
            PrintPub(cit_art, true, false, pmid, m_err_log);
            if (m_replace_cit) {
                CRef<CCit_art> new_cit_art = FetchPubPmId(pmid);

                if (new_cit_art.NotEmpty()) {
                    if (TenAuthorsProcess(cit_art, *new_cit_art, m_err_log)) {

                        if (m_merge_ids) {
                            MergeNonPubmedPubIds(*new_cit_art, cit_art);
                        }

                        CRef<CPub> new_pub(new CPub);
                        new_pub->SetArticle(*new_cit_art);
                        pub.SetEquiv().Set().push_back(new_pub);

                        new_pub.Reset(new CPub);
                        new_pub->SetPmid().Set(pmid);
                        pub.SetEquiv().Set().push_back(new_pub);
                    }
                    else {
                        PrintPub(cit_art, false, true, pmid, m_err_log);
                        MedlineToISO(cit_art);
                    }
                }
            }
            else {
                PrintPub(cit_art, false, false, pmid, m_err_log);
                MedlineToISO(cit_art);
            }
        }
    }
    break;

    case CPub::e_Equiv:
        FixPubEquiv(pub.SetEquiv());
        break;

    default:; // do nothing
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE
