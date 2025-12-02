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
 * Author:  Alexey Dobronadezhdin, Vitaly Stakhovsky
 *
 * File Description:
 *
 * ===========================================================================
 */
#include <ncbi_pch.hpp>

#include <iostream>

#include <corelib/ncbistr.hpp>

#include <objects/pub/Pub.hpp>

#include <objects/general/Person_id.hpp>
#include <objects/general/Name_std.hpp>
#include <objects/general/Date_std.hpp>

#include <objects/biblio/Cit_gen.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Author.hpp>
#include <objects/biblio/Imprint.hpp>

#include <objtools/edit/eutils_updater.hpp>
#include <misc/eutils_client/eutils_client.hpp>

#include <objects/pubmed/Pubmed_entry.hpp>
#include <objects/medline/Medline_entry.hpp>
#include <objects/medline/Medline_si.hpp>

#include <objtools/eutils/efetch/PubmedArticle.hpp>
#include <objtools/eutils/efetch/PubmedArticleSet.hpp>
#include <objtools/eutils/efetch/PubmedBookArticle.hpp>
#include <objtools/eutils/efetch/PubmedBookArticleSet.hpp>

#include <connect/ncbi_conn_stream.hpp>

#include "utils.hpp"
#include "unpub_report.hpp"


/////////////////////////////////////////////////
// CUnpublishedReport

namespace pub_report
{

using namespace std;

static void ProcessInitials(string& initials)
{
    string ret;

    for (char c : initials) {
        if (c != ' ' && c != '.' && c != ',') {
            ret += c;
        }
    }

    ret.swap(initials);
}

static void GetAuthorsFromList(list<string>& authors, const CAuth_list& auth_list)
{
    if (auth_list.IsSetNames()) {
        const CAuth_list::C_Names& names = auth_list.GetNames();
        if (names.IsStd()) {
            for (const CRef<CAuthor>& auth : names.GetStd()) {
                if (auth->IsSetName()) {
                    string            cur_auth, cur_initials;
                    const CPerson_id& person = auth->GetName();
                    if (person.IsName()) {
                        const CName_std& std_name = person.GetName();
                        if (std_name.IsSetLast()) {
                            cur_auth = std_name.GetLast();
                            if (std_name.IsSetInitials()) {
                                cur_initials = std_name.GetInitials();
                                ProcessInitials(cur_initials);
                            }
                        }
                    } else if (person.IsStr()) {
                        cur_auth = person.GetStr();
                    } else if (person.IsMl()) {
                        cur_auth = person.GetMl();
                    }

                    if (! cur_auth.empty()) {
                        if (! cur_initials.empty()) {
                            cur_auth += ' ' + cur_initials;
                        }
                        authors.push_back(cur_auth);
                    }
                }
            }
        } else {
            for (const string& auth : names.IsStr() ? names.GetStr() : names.GetMl()) {
                if (! auth.empty()) {
                    authors.push_back(auth);
                }
            }
        }
    }
}


/////////////////////////////////////////////////
// CPubData
class CPubData
{
public:
    typedef set<string> TSeqIds;

    CPubData() :
        m_title_words_set(false),
        m_full_title_set(false)
    {
    }

    const list<string>& GetAuthors() const
    {
        return m_authors;
    }

    const TSeqIds& GetSeqIds() const
    {
        return m_seq_ids;
    }

    const list<string>& GetTitleWords() const
    {
        if (! m_title_words_set) {
            CreateTitleWords();
            m_title_words_set = true;
        }
        return m_titlewords;
    }

    const string& GetFullTitle() const
    {
        if (! m_full_title_set) {
            CreateFullTitle();
            m_full_title_set = true;
        }
        return m_full_title;
    }

    const string& GetTitle() const
    {
        return m_title;
    }

    const string& GetJournal() const
    {
        return m_journal;
    }

    const string& GetUnique() const
    {
        return m_unique;
    }

    const string& GetVolume() const
    {
        return m_volume;
    }

    const string& GetPages() const
    {
        return m_pages;
    }

    void SetAuthors(const CAuth_list& auth_list)
    {
        m_authors.clear();
        GetAuthorsFromList(m_authors, auth_list);
    }

    void SetTitle(const string& title)
    {
        m_title           = title;
        m_title_words_set = false;
        m_full_title_set  = false;

        m_full_title.clear();
        m_titlewords.clear();
    }

    void SetJournal(const string& journal)
    {
        m_journal = journal;
    }

    void SetUnique(const string& unique)
    {
        m_unique = unique;
    }

    void SetDate(const CDate_std& date)
    {
        if (date.IsSetYear() && date.IsSetMonth()) {
            m_date.Reset(new CDate_std);
            m_date->Assign(date);
        }
    }

    bool IsSetDate() const
    {
        return m_date.NotEmpty();
    }

    int GetYear() const
    {
        if (IsSetDate() && m_date->IsSetYear()) {
            return m_date->GetYear();
        }
        return 0;
    }

    int GetMonth() const
    {
        if (IsSetDate() && m_date->IsSetMonth()) {
            return m_date->GetMonth();
        }
        return 0;
    }

    void AddSeqId(const string& seq_id)
    {
        m_seq_ids.insert(seq_id);
    }

    void SetVolume(const string& volume)
    {
        m_volume = volume;
    }

    void SetPages(const string& pages)
    {
        m_pages = pages;
    }

private:
    list<string> m_authors;
    TSeqIds      m_seq_ids;

    mutable list<string> m_titlewords;

    string m_journal;
    string m_title;
    string m_unique;

    mutable string  m_full_title;
    CRef<CDate_std> m_date;

    mutable bool m_title_words_set,
        m_full_title_set;

    string m_volume,
        m_pages;

    void CreateTitleWords() const
    {
        string title = NStr::Sanitize(m_title);
        NStr::ReplaceInPlace(title, ",", " ");
        NStr::ReplaceInPlace(title, ":", " ");
        NStr::ReplaceInPlace(title, "(", " ");
        NStr::ReplaceInPlace(title, ")", " ");
        NStr::ReplaceInPlace(title, "-", " ");
        NStr::ReplaceInPlace(title, ".", " ");

        NStr::Split(title, " ", m_titlewords, NStr::fSplit_Tokenize);
    }

    void CreateFullTitle() const
    {
        m_full_title = NStr::Sanitize(m_title);

        if (! m_full_title.empty() && m_full_title.front() == '[' && m_full_title.back() == ']') {
            m_full_title = m_full_title.substr(1, m_full_title.size() - 2);
            m_full_title = NStr::Sanitize(m_full_title);
        }

        if (m_full_title.back() == '.')
            m_full_title.pop_back();
    }
};

/////////////////////////////////////////////////


CUnpublishedReport::CUnpublishedReport(CNcbiOstream& out, int max_date_check, bool nohydra) :
    m_out(out),
    m_max_date_check(max_date_check),
    m_nohydra(nohydra)
{
}

static void CollectDataGen(const CCit_gen& cit, CPubData& data)
{
    if (cit.IsSetAuthors()) {
        data.SetAuthors(cit.GetAuthors());
    }

    if (cit.IsSetJournal()) {
        data.SetJournal(GetBestTitle(cit.GetJournal()));
    }

    if (cit.IsSetTitle()) {
        data.SetTitle(cit.GetTitle());
    }

    if (cit.IsSetDate() && cit.GetDate().IsStd()) {
        data.SetDate(cit.GetDate().GetStd());
    }
}

static void CollectDataArt(const CCit_art& cit, CPubData& data)
{
    NCBI_ASSERT(cit.IsSetFrom() && cit.GetFrom().IsJournal(), "cit should be a journal");
    const CCit_jour& from_journal = cit.GetFrom().GetJournal();

    NCBI_ASSERT(from_journal.IsSetImp(), "Imprint should be set");
    const CImprint& imprint = from_journal.GetImp();

    if (cit.IsSetAuthors()) {
        data.SetAuthors(cit.GetAuthors());
    }

    if (from_journal.IsSetTitle()) {
        data.SetJournal(GetBestTitle(from_journal.GetTitle()));
    }

    if (cit.IsSetTitle() && cit.GetTitle().IsSet()) {
        for (const auto& cur_title : cit.GetTitle().Get()) {
            if (cur_title->IsName()) {
                data.SetTitle(cur_title->GetName());
                break;
            }
        }
    }

    if (imprint.IsSetDate() && imprint.GetDate().IsStd()) {
        data.SetDate(imprint.GetDate().GetStd());
    }

    if (imprint.IsSetVolume()) {
        data.SetVolume(imprint.GetVolume());
    }

    if (imprint.IsSetPages()) {
        data.SetPages(imprint.GetPages());
    }
}

static void CollectData(const CPub& pub, CPubData& data)
{
    if (pub.IsGen()) {
        CollectDataGen(pub.GetGen(), data);
    } else if (pub.IsArticle()) {
        CollectDataArt(pub.GetArticle(), data);
    }

    string label;
    pub.GetLabel(&label, CPub::fLabel_Unique);
    data.SetUnique(label);
}

enum AuthorNameMatch {
    eNoMatch,
    eLastNameMatch,
    eOneInitialMatch,
    eTwoInitialsMatch,
    eNoHyphenMatch,
    eFullMatch,
    eLastValue // should always be the last enum item
};

static AuthorNameMatch CompareAuthorNames(string first, string second)
{
    if (NStr::EqualNocase(first, second)) {
        return eFullMatch;
    }

    auto pred = [](char c) { return c == '-'; };
    first.erase(remove_if(first.begin(), first.end(), pred), first.end());
    second.erase(remove_if(second.begin(), second.end(), pred), second.end());

    if (NStr::EqualNocase(first, second)) {
        return eNoHyphenMatch;
    }

    size_t space_pos_first = first.find(' ');
    if (space_pos_first != string::npos && space_pos_first + 2 < first.size()) {
        first.resize(space_pos_first + 3);
    }

    size_t space_pos_second = second.find(' ');
    if (space_pos_second != string::npos && space_pos_second + 2 < second.size()) {
        second.resize(space_pos_second + 3);
    }

    if (NStr::EqualNocase(first, second)) {
        return eTwoInitialsMatch;
    }

    if (space_pos_first != string::npos && space_pos_first + 1 < first.size()) {
        first.resize(space_pos_first + 2);
    }
    if (space_pos_second != string::npos && space_pos_second + 1 < second.size()) {
        second.resize(space_pos_second + 2);
    }

    if (NStr::EqualNocase(first, second)) {
        return eOneInitialMatch;
    }

    if (space_pos_first != string::npos) {
        first.resize(space_pos_first);
    }
    if (space_pos_second != string::npos) {
        second.resize(space_pos_second);
    }

    if (NStr::EqualNocase(first, second)) {
        return eLastNameMatch;
    }

    return eNoMatch;
}

static AuthorNameMatch CompareAuthors(const list<string>& first, const list<string>& second)
{
    if (first.size() != second.size()) {
        return eNoMatch;
    }

    auto first_it  = first.begin(),
            second_it = second.begin();

    AuthorNameMatch ret = eFullMatch;
    for (; ret != eNoMatch && first_it != first.end(); ++first_it, ++second_it) {
        AuthorNameMatch cur = CompareAuthorNames(*first_it, *second_it);
        if (cur < ret) {
            ret = cur;
        }
    }

    return ret;
}

void CUnpublishedReport::ReportUnpublished(const CPub& pub)
{
    const string& cur_seq_id = GetCurrentSeqId();

    shared_ptr<CPubData> data(new CPubData);
    CollectData(pub, *data);

    bool need_to_add = true;
    for (const auto& cur_pub : m_pubs) {
        AuthorNameMatch match = CompareAuthors(cur_pub->GetAuthors(), data->GetAuthors());
        if (match == eFullMatch &&
            NStr::EqualNocase(cur_pub->GetFullTitle(), data->GetFullTitle()) &&
            NStr::EqualNocase(cur_pub->GetUnique(), data->GetUnique())) {

            if (! cur_seq_id.empty()) {
                cur_pub->AddSeqId(cur_seq_id);
            } else {
                m_pubs_need_id.push_back(cur_pub);
            }

            need_to_add = false;
            break;
        }
    }

    if (need_to_add) {
        m_pubs.push_back(data);

        if (cur_seq_id.empty()) {
            m_pubs_need_id.push_back(data);
        } else {
            data->AddSeqId(cur_seq_id);
        }

        if (! data->IsSetDate()) {
            m_pubs_need_date.push_back(data);
        }
    }
}

CEutilsClient& CUnpublishedReport::GetEUtils() const
{
    if (! m_eutils) {
        m_eutils.reset(new CEutilsClient);
    }
    return *m_eutils;
}

static void GetOneInitialAuthorName(const string& author, string& name)
{
    size_t space = author.rfind(' ');
    if (space == string::npos) {
        name = author;
    } else {
        name = author.substr(0, space + 1);
        if (space + 1 < author.size()) {
            name += author[space + 1];
        }
    }
}

static void GetNameFromStdName(const CPerson_id& person, string& name)
{
    if (person.IsName()) {
        if (person.GetName().IsSetLast()) {
            name = person.GetName().GetLast();

            if (person.GetName().IsSetInitials()) {
                name += ' ' + person.GetName().GetInitials();
            }
        }
    } else if (person.IsMl()) {
        name = person.GetMl();
    } else if (person.IsStr()) {
        name = person.GetStr();
    }
}

static bool FirstOrLastAuthorMatches(const list<string>& authors, const CAuth_list::C_Names& pubmed_authors)
{
    string first_author,
        last_author;

    if (authors.size()) {
        GetOneInitialAuthorName(authors.front(), first_author);
    }

    if (authors.size() > 1) {
        GetOneInitialAuthorName(authors.back(), last_author);
    }

    bool ret = false;
    if (pubmed_authors.IsStd()) {
        for (const auto& auth : pubmed_authors.GetStd()) {
            if (auth->IsSetName()) {
                string name;
                GetNameFromStdName(auth->GetName(), name);
                string cur_name;
                GetOneInitialAuthorName(name, cur_name);
                if (NStr::EqualNocase(cur_name, first_author) || NStr::EqualNocase(cur_name, last_author)) {
                    ret = true;
                    break;
                }
            }
        }
    } else {
        const list<string>& names = pubmed_authors.IsMl() ? pubmed_authors.GetMl() : pubmed_authors.GetStr();
        for (const string& name : names) {
            string cur_name;
            GetOneInitialAuthorName(name, cur_name);
            if (cur_name == first_author || cur_name == last_author) {
                ret = true;
                break;
            }
        }
    }

    return ret;
}

static bool CheckRefs(const CMedline_entry& medline_entry, const CPubData::TSeqIds& seq_ids)
{
    bool ret = true;

    if (medline_entry.IsSetXref()) {
        ret = false;
        for (const auto& xref : medline_entry.GetXref()) {
            if (xref->IsSetCit()) {
                if (seq_ids.find(xref->GetCit()) != seq_ids.end()) {
                    return true;
                }
            }
        }
    }

    return ret;
}

static bool CheckDate(int year, int month, int max_date_check, const CCit_jour& juornal)
{
    bool ret = true;
    if (max_date_check && year && juornal.IsSetImp() && juornal.GetImp().IsSetDate()) {
        const CDate& pub_date = juornal.GetImp().GetDate();
        if (pub_date.IsStd() && pub_date.GetStd().IsSetYear()) {

            const CDate_std& std_pub_date = pub_date.GetStd();

            CDate_std date_before,
                date_after;

            date_before.SetYear(year);
            if (month > 1) {
                date_before.SetMonth(month - 1);
            } else {
                date_before.SetYear(year - 1);
                date_before.SetMonth(12);
            }

            date_after.SetYear(year + max_date_check);
            date_after.SetMonth(month);

            CDate::ECompare before = date_before.Compare(std_pub_date),
                            after  = date_after.Compare(std_pub_date);

            ret = (before == CDate::eCompare_before || before == CDate::eCompare_same) &&
                    (after == CDate::eCompare_after || after == CDate::eCompare_same);
        }
    }

    return ret;
}


static TEntrezId DoHydraSearch(const CPubData& data)
{
    vector<string> query;

    // title
    for (const string& w : data.GetTitleWords()) {
        query.push_back(w);
    }

    // authors
    for (const string& author : data.GetAuthors()) {
        list<CTempString> names;
        NStr::Split(author, " ", names, NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate);
        if (! names.empty()) {
            query.push_back(names.front());
        }
    }

    vector<TEntrezId> uids;

    try {
        edit::CEUtilsUpdater::DoPubSearch(query, uids);
    } catch (CException& e) {
        ERR_POST(Warning << "failed while Hydra search: " << e);
    }

    TEntrezId pmid = ZERO_ENTREZ_ID;
    if (uids.size() == 1) {
        pmid = uids.front();
    }

    return pmid;
}

static TEntrezId ConvertPMCtoPMID(TEntrezId pmc)
{
    static const string BASE_URL = "https://pmc.ncbi.nlm.nih.gov/tools/idconv/api/v1/articles/?tool=pub_report&format=xml&ids=PMC";
    static const size_t BUF_SIZE = 1024;

    string url = BASE_URL + NStr::IntToString(ENTREZ_ID_TO(int, pmc));

    TEntrezId pmid = ZERO_ENTREZ_ID;

    for (int attempt = 1; attempt <= 5; attempt++) {
        try {
            CConn_HttpStream https(url);

            string result;

            vector<char> buf(BUF_SIZE);
            while (! https.fail()) {
                https.read(&buf[0], BUF_SIZE);
                result.append(&buf[0], https.gcount());
            }

            if (NStr::StartsWith(result, "<pmcids status=\"ok\">")) {
                if (result.find("status = \"error\"") == string::npos && result.find("<errmsg>") == string::npos) {
                    static const char pmid_start[] = "pmid=\"";
                    size_t            pmid_pos     = result.find(pmid_start);
                    if (pmid_pos != string::npos) {
                        pmid = ENTREZ_ID_FROM(int, NStr::StringToInt(result.c_str() + pmid_pos + sizeof(pmid_start) - 1, NStr::fAllowTrailingSymbols));
                    }
                }

                break;
            }

        } catch (CException& e) {
            ERR_POST(Warning << "failed on attempt " << attempt
                                << ": " << e);
        }
    }

    return pmid;
}

static void NormalizeTitle(string& s)
{
    for (char& c : s) {
        switch (c) {
        case '.':
        case '"':
        case '(':
        case ')':
        case '[':
        case ']':
        case ':':
            c = ' ';
            break;
        default:
            if (isupper(c)) {
                c = tolower(c);
            }
            break;
        }
    }
}

static TEntrezId DoEUtilsSearch(CEutilsClient& eutils, const string& database, const string& term)
{
    vector<TEntrezId> uids;
    eutils.Search(database, term, uids);

    TEntrezId pmid = ZERO_ENTREZ_ID;
    if (uids.size() == 1) {
        pmid = uids.front();
    }

    return pmid;
}

TEntrezId CUnpublishedReport::RetrievePMid(const CPubData& data) const
{
    CEutilsClient& eutils = GetEUtils();

    // TODO: CEUtilsUpdater::CitMatch()
    string title = data.GetTitle();
    NormalizeTitle(title);
    string term = title + "[title]";

    TEntrezId pmid = ZERO_ENTREZ_ID;

    if (! term.empty()) {
        try {
            pmid = DoEUtilsSearch(eutils, "pubmed", term);
            if (pmid == ZERO_ENTREZ_ID) {
                pmid = DoEUtilsSearch(eutils, "pmc", term);
                if (pmid != ZERO_ENTREZ_ID) {
                    pmid = ConvertPMCtoPMID(pmid);
                }
            }
        } catch (...) {
            pmid = ZERO_ENTREZ_ID;
        }
    }

    if (pmid == ZERO_ENTREZ_ID && ! m_nohydra) {
        pmid = DoHydraSearch(data);
    }

    return pmid;
}

bool CUnpublishedReport::FetchPub(TEntrezId pmid, const CPubData& data, CRef<CPubmed_entry>& pubmed_entry) const
{
    CNcbiStrstream xml_stream;
    eutils::CPubmedArticleSet pas;

    CEutilsClient& eutils = GetEUtils();
    vector<TEntrezId> uids { pmid };

    try {
        eutils.Fetch("PubMed", uids, xml_stream);
        if (xml_stream) {
            xml_stream >> MSerial_Xml >> pas;
        }
    } catch (CException& e) {
        // skips exceptions those may occur during Fetch(...) and '>>'
        ERR_POST(Warning << "failed while fetching data from PubMed: " << e);
        return false;
    }

    const auto& pp = pas.GetPP().GetPP();
    if (! pp.empty()) {
        const auto& ppf = *pp.front();
        if (ppf.IsPubmedArticle()) {
            const eutils::CPubmedArticle& article = ppf.GetPubmedArticle();
            pubmed_entry.Reset(article.ToPubmed_entry());
        } else if (ppf.IsPubmedBookArticle()) {
            const eutils::CPubmedBookArticle& article = ppf.GetPubmedBookArticle();
            pubmed_entry.Reset(article.ToPubmed_entry());
        }
    }

    if (pubmed_entry && pubmed_entry->IsSetMedent() && pubmed_entry->GetMedent().IsSetCit()) {
        const CCit_art& cit_art = pubmed_entry->GetMedent().GetCit();
        if (cit_art.IsSetFrom() && cit_art.GetFrom().IsJournal()) {
            bool proceed = CheckDate(data.GetYear(), data.GetMonth(), m_max_date_check, cit_art.GetFrom().GetJournal()) && CheckRefs(pubmed_entry->GetMedent(), data.GetSeqIds());
            if (proceed && cit_art.IsSetAuthors()) {
                const CAuth_list& authors = cit_art.GetAuthors();
                if (authors.IsSetNames() && FirstOrLastAuthorMatches(data.GetAuthors(), authors.GetNames())) {
                    return true;
                }
            }
        }
    }

    return false;
}

static AuthorNameMatch IsAuthorInList(const list<string>& auths, const string& author)
{
    AuthorNameMatch res = eFullMatch;

    bool found = false;
    for (const string& cur_author : auths) {
        AuthorNameMatch cur_cmp_res = CompareAuthorNames(cur_author, author);
        if (cur_cmp_res == eNoMatch) {
            continue;
        }

        found = true;
        if (cur_cmp_res < res) {
            res = cur_cmp_res;
        }
    }

    return found ? res : eNoMatch;
}

static void ReportSeqIds(CNcbiOstream& out, const CPubData::TSeqIds& ids)
{
    for (const string& id : ids) {
        out << "SEQID |" << id << "|\t";
    }
}

static string authors_cmp_result_label[] = {
    "AUTH_MISMATCH", "LAST_NAMES", "ONE_INIT", "TWO_INITS", "NO_HYPHENS", "FULL_NAMES"
};

static string GetAuthorsCmpResultStr(AuthorNameMatch res)
{
    NCBI_ASSERT(res < eLastValue, "Invalid res value");
    return authors_cmp_result_label[res];
}

static bool ReportAuthorDiff(CNcbiOstream& out, const list<string>& pubmed_auths, const list<string>& auths)
{
    AuthorNameMatch best_match = eFullMatch;

    size_t matches = 0;
    for (const string& author : auths) {
        AuthorNameMatch cur_match = IsAuthorInList(pubmed_auths, author);
        if (cur_match == eNoMatch) {
            continue;
        }

        ++matches;
        if (cur_match < best_match) {
            best_match = cur_match;
        }
    }

    bool both_ok = true;

    size_t pubmed_size = pubmed_auths.size(),
            cur_size    = auths.size();
    string result_str  = GetAuthorsCmpResultStr(best_match);

    if (! auths.empty() && matches == cur_size) {
        if (cur_size < 3 && pubmed_size > 4) {
            out << "AUTHORS_QUESTIONABLE [" << result_str << "] " << cur_size << " -> " << pubmed_size << '\t';
            both_ok = false;
        } else if (cur_size < pubmed_size) {
            out << "AUTHORS_ADDED [" << result_str << "] " << pubmed_size - cur_size << '\t';
        } else {
            out << "AUTHORS_REORDERED [" << result_str << "]\t";
        }
    } else {
        out << "AUTHORS_CHANGED [" << result_str << "] " << matches << " / " << pubmed_size << '\t';
        both_ok = false;
    }

    return both_ok;
}

static bool ReportTitleDiff(CNcbiOstream& out, const list<string>& pubmed_title_words, const list<string>& title_words)
{
    size_t matches = 0;
    for (const string& word : title_words) {
        if (NStr::FindNoCase(pubmed_title_words, word)) {
            ++matches;
        }
    }

    bool both_ok = true;

    size_t pubmed_size = pubmed_title_words.size(),
            cur_size    = title_words.size();

    if (cur_size < 3 && pubmed_size > 4) {
        out << "TITLE_QUESTIONABLE " << cur_size << " -> " << pubmed_size << '\t';
        both_ok = false;
    } else if (pubmed_size && cur_size && NStr::EqualNocase(pubmed_title_words.front(), title_words.front()) &&
                matches == pubmed_size) {
        out << "TITLE_SAME [SIMILAR] " << matches << '\t';
    } else if (pubmed_size && matches == pubmed_size) {
        out << "TITLE_ALTERED " << matches << '\t';
        both_ok = false;
    } else {
        out << "TITLE_DIFFERS " << matches << " / " << pubmed_size << '\t';
        both_ok = false;
    }

    return both_ok;
}

static void ReportAuththors(CNcbiOstream& out, const char* prefix, const list<string>& auths)
{
    out << prefix;
    if (! auths.empty()) {

        out << auths.front();
        auto auth = auths.begin();
        for (++auth; auth != auths.end(); ++auth)
            out << ", " << *auth;
    }
    out << '\t';
}

static void ReportTitle(CNcbiOstream& out, const char* prefix, const CPubData& data)
{
    out << prefix;

    if (! data.GetFullTitle().empty()) {
        out << data.GetFullTitle();
    } else {

        const list<string>& words = data.GetTitleWords();
        if (! words.empty()) {
            out << words.front();
            auto word = words.begin();
            for (++word; word != words.end(); ++word)
                out << ' ' << *word;
        }
    }
    out << '\t';
}

static void ReportJournal(CNcbiOstream& out, const char* prefix, const CPubData& data)
{
    out << prefix;
    int year = data.GetYear();

    if (data.GetJournal().empty()) {

        out << "Unpublished";
        if (year) {
            out << " [" << year << ']';
        }
    } else {
        out << data.GetJournal();

        if (year) {
            out << " [" << year << ']';
        }

        if (! data.GetVolume().empty()) {
            out << ' ' << data.GetVolume();
        }

        if (! data.GetPages().empty()) {
            out << " : " << data.GetPages();
        }
    }
}

static void ReportOnePub(CNcbiOstream& out, const CCit_art& pubmed_cit_art, const CPubData& data, TEntrezId pmid)
{
    CPubData pubmed_data;
    CollectDataArt(pubmed_cit_art, pubmed_data);

    if (! pubmed_data.GetAuthors().empty() && ! pubmed_data.GetTitleWords().empty()) {

        AuthorNameMatch authors_cmp_res = CompareAuthors(pubmed_data.GetAuthors(), data.GetAuthors());
        bool            title_same      = NStr::EqualNocase(pubmed_data.GetFullTitle(), data.GetFullTitle());

        out << "PMID " << ENTREZ_ID_TO(int, pmid) << '\t';
        ReportSeqIds(out, data.GetSeqIds());

        if (data.GetUnique().empty()) {
            out << "?\t";
        } else {
            out << "UNIQ_CIT " << data.GetUnique() << '\t';
        }

        bool both_ok = true;
        if (authors_cmp_res == eNoMatch) {
            both_ok = ReportAuthorDiff(out, pubmed_data.GetAuthors(), data.GetAuthors());
        } else {
            out << "AUTHORS_SAME [" << GetAuthorsCmpResultStr(authors_cmp_res) << "]\t";
        }

        if (title_same) {
            out << "TITLE_SAME [IDENTICAL]\t";
        } else {
            if (! ReportTitleDiff(out, pubmed_data.GetTitleWords(), data.GetTitleWords()))
                both_ok = false;
        }

        out << (both_ok ? "PROBABLE\t" : "POSSIBLE\t");

        ReportAuththors(out, "OLD_AUTH ", data.GetAuthors());
        ReportAuththors(out, "NEW_AUTH ", pubmed_data.GetAuthors());

        ReportTitle(out, "OLD_TITL ", data);
        ReportTitle(out, "NEW_TITL ", pubmed_data);

        ReportJournal(out, "OLD_JOUR ", data);
        out << '\t';
        ReportJournal(out, "NEW_JOUR ", pubmed_data);

        out << " <" << ENTREZ_ID_TO(int, pmid) << '>';
        out << '\n';
    }
}

void CUnpublishedReport::CompleteReport()
{
    m_out << "Trying " << m_pubs.size() << " Entrez Queries\n\n";
    for (const auto& pub : m_pubs) {
        TEntrezId pmid = RetrievePMid(*pub);
        if (pmid != ZERO_ENTREZ_ID) {
            CRef<CPubmed_entry> pubmed_entry;
            if (FetchPub(pmid, *pub, pubmed_entry)) {
                NCBI_ASSERT(pubmed_entry->IsSetMedent() && pubmed_entry->GetMedent().IsSetCit(),
                            "MedEntry and MedEntry.Cit should be present at this point");
                ReportOnePub(m_out, pubmed_entry->GetMedent().GetCit(), *pub, pmid);
            }
        }
    }
}

void CUnpublishedReport::SetCurrentSeqId(const std::string& name)
{
    if (! name.empty() && ! m_pubs_need_id.empty()) {
        for (const auto& pub_need_id : m_pubs_need_id) {
            pub_need_id->AddSeqId(name);
        }
        m_pubs_need_id.clear();
    }

    CBaseReport::SetCurrentSeqId(name);
}

void CUnpublishedReport::ClearData()
{
    if (IsSetDate()) {
        for (const auto& pub : m_pubs_need_date) {
            if (! pub->IsSetDate()) {
                pub->SetDate(GetDate());
            }
        }
    }

    m_pubs_need_date.clear();
    ResetDate(); // clears the current date
}

bool CUnpublishedReport::IsSetDate() const
{
    return m_date.NotEmpty();
}

void CUnpublishedReport::SetDate(const CDate_std& date)
{
    if (date.IsSetYear() && date.IsSetMonth()) {
        m_date.Reset(new CDate_std);
        m_date->Assign(date);
    }
}

const CDate_std& CUnpublishedReport::GetDate() const
{
    static CDate_std invalid_date;
    return IsSetDate() ? *m_date : invalid_date;
}

void CUnpublishedReport::ResetDate()
{
    m_date.Reset();
}

}
