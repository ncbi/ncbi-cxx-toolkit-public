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
#include <objects/biblio/Cit_proc.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/biblio/Title.hpp>
#include <objects/general/Name_std.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Date_std.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/medline/Medline_entry.hpp>

#include <objects/pub/Pub.hpp>

#include <objtools/edit/pub_fix.hpp>

#include "pub_fix_aux.hpp"

#include <objects/mla/Title_msg.hpp>
#include <objects/mla/Title_msg_list.hpp>
#include <objects/mla/mla_client.hpp>

#include <corelib/ncbi_message.hpp>
#include <objtools/eutils/api/esearch.hpp>
#include <objtools/eutils/esearch/IdList.hpp>
#include <objtools/eutils/api/esummary.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

#define ERR_POST_TO_LISTENER(listener, severity, code, subcode, message) \
do { \
    if (listener) { \
        ostringstream ostr; \
        ostr << message; \
        string text = ostr.str(); \
        CMessage_Basic msg(text, severity, code, subcode); \
        listener->PostMessage(msg); \
    } \
} while (false)

namespace fix_pub
{
struct SErrorSubcodes
{
    string m_error_str;
    map<int, string> m_sub_errors;
};

static map<int, SErrorSubcodes> ERROR_CODE_STR =
{
    // I'm using it in blob_maint application. The string REFERENCE is not informative, changing to FixPub.
    { err_Reference,{ "FixPub",
    {
        { err_Reference_MuidNotFound, "MuidNotFound" },
        { err_Reference_SuccessfulMuidLookup, "SuccessfulMuidLookup" },
        { err_Reference_OldInPress, "OldInPress" },
        { err_Reference_No_reference, "No_reference" },
        { err_Reference_Multiple_ref, "Multiple_ref" },
        { err_Reference_Multiple_muid, "Multiple_muid" },
        { err_Reference_MedlineMatchIgnored, "MedlineMatchIgnored" },
        { err_Reference_MuidMissmatch, "MuidMissmatch" },
        { err_Reference_NoConsortAuthors, "NoConsortAuthors" },
        { err_Reference_DiffConsortAuthors, "DiffConsortAuthors" },
        { err_Reference_PmidMissmatch, "PmidMissmatch" },
        { err_Reference_Multiple_pmid, "Multiple_pmid" },
        { err_Reference_FailedToGetPub, "FailedToGetPub" },
        { err_Reference_MedArchMatchIgnored, "MedArchMatchIgnored" },
        { err_Reference_SuccessfulPmidLookup, "SuccessfulPmidLookup" },
        { err_Reference_PmidNotFound, "PmidNotFound" },
        { err_Reference_NoPmidJournalNotInPubMed, "NoPmidJournalNotInPubMed" },
        { err_Reference_PmidNotFoundInPress, "PmidNotFoundInPress" },
        { err_Reference_NoPmidJournalNotInPubMedInPress, "NoPmidJournalNotInPubMedInPress" }
    }
    } },
    { err_Print,{ "PRINT",
    {
        { err_Print_Failed, "Failed" }
    }
    } },
    { err_AuthList,{ "AuthList",
    {
        { err_AuthList_SignificantDrop, "SignificantDrop" },
        { err_AuthList_PreserveGB, "PreserveGB" },
        { err_AuthList_LowMatch, "LowMatch" }
    }
    } }
};
}

string CPubFix::GetErrorId(int err_code, int err_sub_code)
{
    string ret;

    const auto& err_category = fix_pub::ERROR_CODE_STR.find(err_code);
    if (err_category != fix_pub::ERROR_CODE_STR.end()) {

        const auto& error_sub_code_str = err_category->second.m_sub_errors.find(err_sub_code);
        if (error_sub_code_str != err_category->second.m_sub_errors.end()) {
            ret = err_category->second.m_error_str;
            ret += '.';
            ret += error_sub_code_str->second;
        }
    }

    return ret;
}


namespace fix_pub
{
//   MedlineToISO(tmp)
//       converts a MEDLINE citation to ISO/GenBank style

void MedlineToISO(CCit_art& cit_art)
{
    if (cit_art.IsSetAuthors()) {

        CAuth_list& auths = cit_art.SetAuthors();
        if (auths.IsSetNames()) {
            if (auths.GetNames().IsMl()) {
                auths.ConvertMlToStandard();
            }
            else if (auths.GetNames().IsStd()) {
                for (auto& auth : auths.SetNames().SetStd()) {
                    if (auth->IsSetName() && auth->GetName().IsMl()) {
                        auth = CAuthor::ConvertMlToStandard(*auth);
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
                for (auto& item : msg_list_new->GetTitles()) {
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
    if (!medline.IsSetCit() && medline.IsSetPmid() && medline.GetPmid() < ZERO_ENTREZ_ID) {
        return;
    }

    CRef<CPub> pmid;
    if (medline.GetPmid() > ZERO_ENTREZ_ID) {
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
    else if (cit_art.GetFrom().IsBook()) {
        const CCit_book& book = cit_art.GetFrom().GetBook();
        ret = book.IsSetImp() && book.GetImp().IsSetPrepub() && book.GetImp().GetPrepub() == CImprint::ePrepub_in_press;
    }
    else if (cit_art.GetFrom().IsProc() && cit_art.GetFrom().GetProc().IsSetBook()) {
        const CCit_book& book = cit_art.GetFrom().GetProc().GetBook();
        ret = book.IsSetImp() && book.GetImp().IsSetPrepub() && book.GetImp().GetPrepub() == CImprint::ePrepub_in_press;
    }
    return ret;
}


bool MULooksLikeISSN(const string& str)
{
    // ISSN: nnnn-nnnn or nnnn-nnnX, where n -> '0'-'9', i.e. 0123-5566
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

/*
bool MUIsJournalIndexed(const string& journal)
{
    if (journal.empty()) {
        return false;
    }

    string title(journal);
    NStr::ReplaceInPlace(title, "(", " ");
    NStr::ReplaceInPlace(title, ")", " ");
    NStr::ReplaceInPlace(title, ".", " ");

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

        if (ids.empty()) {
            eutils.Search(EUTILS_DATABASE, title + "[multi] AND ncbijournals[sb]", ids);
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

    return status == "Y";
}
*/

static void s_GetESearchIds(CESearch_Request& req,
                            const string& term,
                            list<string>& ids) {
    // error handling is modeled on that of CEUtilsClient::x_Search()
    req.SetArgument("term", term);
    for (int retry=0; retry<10; ++retry) {
        try {
            auto& istr = dynamic_cast<CConn_HttpStream&>(req.GetStream());
            auto pRes = Ref(new esearch::CESearchResult());
            istr >> MSerial_Xml >> *pRes;

            if (istr.GetStatusCode() == 200) {
                if (pRes->IsSetData()) {
                    if (pRes->GetData().IsInfo() &&
                        pRes->GetData().GetInfo().IsSetContent() &&
                        pRes->GetData().GetInfo().GetContent().IsSetIdList()) {

                        const auto& idList = pRes->GetData().GetInfo().GetContent().GetIdList();
                        if (idList.IsSetId()) {
                            ids = idList.GetId();
                        }
                        req.Disconnect();
                        return;
                    }
                    else
                    if (pRes->GetData().IsERROR()) {
                        NCBI_THROW(CException, eUnknown,
                                pRes->GetData().GetERROR());
                    }
                } // pRest->IsSetData()
            } // istr.GetStatusCode() == 200
        }
        catch(CException& e) {
            ERR_POST(Warning << "failed on attempt " << retry + 1
                    << ": " << e);
        }
        req.Disconnect();

        int sleepSeconds = sqrt(retry);
        if (sleepSeconds) {
            SleepSec(sleepSeconds);
        }
    } // retry

    NCBI_THROW(CException, eUnknown,
            "failed to execute query: " + term);
}


static bool s_IsIndexed(CRef<CEUtils_ConnContext> pContext,
        const string& id) {

    // error handling is modeled on that of CEUtilsClient::x_Summary()
    CESummary_Request request("nlmcatalog", pContext);
    request.GetId().AddId(id);
    request.SetArgument("version", "2.0");
    string xmlOutput;
    bool success=false;
    for (int retry=0; retry<10; ++retry) {
        try {
            auto& istr = dynamic_cast<CConn_HttpStream&>(request.GetStream());
            NcbiStreamToString(&xmlOutput, istr);
            if (istr.GetStatusCode() == 200) {
                success = true;
                break;
            }
        }
        catch (...) {
        }
        request.Disconnect();

        int sleepSeconds = sqrt(retry);
        if (sleepSeconds) {
            SleepSec(sleepSeconds);
        }
    }

    if (!success) {
        NCBI_THROW(CException, eUnknown,
                "failed to execute esummary request: " + request.GetQueryString());
    }

    static const string indexingElement { "<CurrentIndexingStatus>Y</CurrentIndexingStatus>" };
    auto firstPos = NStr::Find(xmlOutput, indexingElement, NStr::eNocase);
    if (firstPos == NPOS) {
        return false;
    }
    auto lastPos = NStr::Find(xmlOutput, indexingElement, NStr::eNocase, NStr::eReverseSearch);

    return firstPos == lastPos;
}



bool MUIsJournalIndexed(const string& journal)
{
    if (journal.empty()) {
        return false;
    }

    string title(journal);
    NStr::ReplaceInPlace(title, "(", " ");
    NStr::ReplaceInPlace(title, ")", " ");
    NStr::ReplaceInPlace(title, ".", " ");

    title = NStr::Sanitize(title);

    list<string> ids;
    auto pContext = Ref(new CEUtils_ConnContext());
    CESearch_Request req("nlmcatalog", pContext);
    req.SetRetMax(2);
    req.SetUseHistory(false);
    try {
        if (MULooksLikeISSN(title)) {
            s_GetESearchIds(req, title + "[issn]", ids);
        }

        if (ids.empty()) {
            s_GetESearchIds(req, title + "[multi] AND ncbijournals[sb]", ids);
        }

        if (ids.empty()) {
            s_GetESearchIds(req, title + "[jo]", ids);
        }
    }
    catch (CException&) {
        return false;
    }

    if (ids.size() != 1) {
        return false;
    }

    return s_IsIndexed(pContext, ids.front());
}



void PrintPub(const CCit_art& cit_art, bool found, bool auth, long muid, IMessageListener* err_log)
{
    string first_name,
        last_name;

    if (cit_art.IsSetAuthors() && cit_art.GetAuthors().IsSetNames()) {

        if (cit_art.GetAuthors().GetNames().IsStd()) {

            const CAuthor& first_author = *cit_art.GetAuthors().GetNames().GetStd().front();

            if (first_author.IsSetName()) {
                if (first_author.GetName().IsName()) {
                    const CName_std& namestd = first_author.GetName().GetName();
                    if (namestd.IsSetLast()) {
                        last_name = namestd.GetLast();
                    }
                    if (namestd.IsSetInitials()) {
                        first_name = namestd.GetInitials();
                    }
                }
                else if (first_author.GetName().IsConsortium()) {
                    last_name = first_author.GetName().GetConsortium();
                }
            }
        }
        else {
            last_name = cit_art.GetAuthors().GetNames().GetStr().front();
        }
    }
    else {
        ERR_POST_TO_LISTENER(err_log, eDiag_Warning, err_Print, err_Print_Failed, "Authors NULL");
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
    string title_str(UNKNOWN_JOURNAL);

    if (title && title->IsSet() && !title->Get().empty()) {

        const CTitle::C_E& first_title = *title->Get().front();
        const string& str = title->GetTitle(first_title);

        if (!str.empty())
            title_str = str;
    }


    static const string NO_PAGE("no page number");
    static const string NO_VOL("no volume number");

    string vol(NO_VOL),
        page(NO_PAGE);

    int year = 0;
    bool in_press = false;

    if (imprint) {

        if (imprint->IsSetVolume()) {
            vol = imprint->GetVolume();
        }

        if (imprint->IsSetPages()) {
            page = imprint->GetPages();
        }

        if (imprint->IsSetDate() && imprint->GetDate().IsStd() && imprint->GetDate().GetStd().IsSetYear()) {
            year = imprint->GetDate().GetStd().GetYear();
        }

        in_press = imprint->IsSetPrepub() && imprint->GetPrepub() == CImprint::ePrepub_in_press;
    }

    if (auth) {
        ERR_POST_TO_LISTENER(err_log, eDiag_Error, err_Reference, err_Reference_MedArchMatchIgnored,
            "Too many author name differences: " << muid << "|" << last_name << " " << first_name << "|" << title_str << "|(" << year << ")|" << vol << "|" << page);
        return;
    }

    if (in_press) {

        int cur_year = CDate_std(CTime(CTime::eCurrent)).GetYear();
        static const int YEAR_MAX_DIFF = 2;

        if (year && cur_year - year > YEAR_MAX_DIFF) {
            ERR_POST_TO_LISTENER(err_log, eDiag_Warning, err_Reference, err_Reference_OldInPress,
                "encountered in-press article more than 2 years old: " << last_name << " " << first_name << "|" << title_str << "|(" << year << ")|" << vol << "|" << page);
        }
    }

    if (found) {
        ERR_POST_TO_LISTENER(err_log, eDiag_Info, err_Reference, err_Reference_SuccessfulPmidLookup,
            muid << "|" << last_name << " " << first_name << "|" << title_str << "|(" << year << ")|" << vol << "|" << page);
    }
    else if (MUIsJournalIndexed(title_str)) {
        if (muid) {
            ERR_POST_TO_LISTENER(err_log, eDiag_Warning, err_Reference, in_press ? err_Reference_PmidNotFoundInPress : err_Reference_PmidNotFound,
                ">>" << muid << "<<|" << last_name << " " << first_name << "|" << title_str << "|(" << year << ")|" << vol << "|" << page);
        }
        else {
            ERR_POST_TO_LISTENER(err_log, eDiag_Warning, err_Reference, in_press ? err_Reference_PmidNotFoundInPress : err_Reference_PmidNotFound,
                last_name << " " << first_name << "|" << title_str << "|(" << year << ")|" << vol << "|" << page);
        }
    }
    else {
        if (muid) {
            ERR_POST_TO_LISTENER(err_log, eDiag_Info, err_Reference, in_press ? err_Reference_NoPmidJournalNotInPubMedInPress : err_Reference_NoPmidJournalNotInPubMed,
                ">>" << muid << "<<|" << last_name << " " << first_name << "|" << title_str << "|(" << year << ")|" << vol << "|" << page);
        }
        else {
            ERR_POST_TO_LISTENER(err_log, eDiag_Info, err_Reference, in_press ? err_Reference_NoPmidJournalNotInPubMedInPress : err_Reference_NoPmidJournalNotInPubMed,
                last_name << " " << first_name << "|" << title_str << "|(" << year << ")|" << vol << "|" << page);
        }
    }
}


bool IsFromBook(const CCit_art& art)
{
    return art.IsSetFrom() && art.GetFrom().IsBook();
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

    extracted.sort([](const string& a, const string& b) { return NStr::CompareNocase(a, b) == -1;  });

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

            ERR_POST_TO_LISTENER(err_log, eDiag_Warning, err_Reference, err_Reference_NoConsortAuthors,
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
            if (!NStr::EqualNocase(old_cons_list, new_cons_list)) {
                ERR_POST_TO_LISTENER(err_log, eDiag_Warning, err_Reference, err_Reference_DiffConsortAuthors,
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
                    return NStr::EqualNocase(last_name, cur_name);
                }) != new_author_names.end()) {

                ++match;
            }
        }
    }

    size_t min_num_names = min(num_names, new_author_names.size());
    if (min_num_names > MAX_MATCH_COEFF * match) {
        return false;
    }

    bool replace_authors = new_num_names == 0;
    if (!replace_authors && new_num_names < num_names) {
        // Check the last author from PubMed. If it is "et al" - leave the old authors list
        const CAuthor& last_author = *new_cit.GetAuthors().GetNames().GetStd().back();
        if (last_author.IsSetName() && last_author.GetName().IsName()) {

            const CName_std& name = last_author.GetName().GetName();
            string last_name = name.IsSetLast() ? name.GetLast() : "",
                   initials = name.IsSetInitials() ? name.GetInitials() : "";

            replace_authors = NStr::EqualNocase(last_name, "et") &&
                              NStr::EqualNocase(initials, "al");
        }

        // If the last author does not contain "et al", look at the amount of authors
        // This is done according to the next document:
        // ~cavanaug/WORK/MedArch/doc.medarch.4genbank.txt
        //
        //    If the MedArchCitArt has zero Name-std Author.name ...
        //
        //    Or if the InputCitArt has more than 10 Name - std Author.name while
        //    the MedArchCitArt has less than 12 ...
        //
        //    Or if the InputCitArt has more than 25 Name - std Author.name while
        //    the MedArchCitArt has less than 27 ...
        //
        //    Then free the Auth - list of the MedArchCitArt and replace it with
        //     the Auth - list of the InputCitArt, and **null out** the Auth - list
        //     of the MedArchCitArt .
        if (!replace_authors)
        {
            static const int MIN_FIRST_AUTHORS_THRESHOLD_1995 = 10;
            static const int MAX_FIRST_AUTHORS_THRESHOLD_1995 = 12;

            static const int MIN_SECOND_AUTHORS_THRESHOLD_1999 = 25;
            static const int MAX_SECOND_AUTHORS_THRESHOLD_1999 = 27;

            replace_authors = (new_num_names < MAX_FIRST_AUTHORS_THRESHOLD_1995 && num_names > MIN_FIRST_AUTHORS_THRESHOLD_1995) ||
                              (new_num_names < MAX_SECOND_AUTHORS_THRESHOLD_1999 && num_names > MIN_SECOND_AUTHORS_THRESHOLD_1999);
        }
    }

    if (replace_authors) {
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


bool NeedToPropagateInJournal(const CCit_art& cit_art)
{
    if (!cit_art.IsSetFrom() || !cit_art.GetFrom().IsJournal() ||
        !cit_art.GetFrom().GetJournal().IsSetTitle() || !cit_art.GetFrom().GetJournal().GetTitle().IsSet() ||
        cit_art.GetFrom().GetJournal().GetTitle().Get().empty()) {
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

    if (!cit_art.IsSetFrom() || !NeedToPropagateInJournal(cit_art)) {
        return;
    }

    CImprint* imprint = nullptr;

    switch (cit_art.GetFrom().Which()) {

    case CCit_art::C_From::e_Journal:
        if (cit_art.GetFrom().GetJournal().IsSetImp()) {
            imprint = &cit_art.SetFrom().SetJournal().SetImp();
        }
        break;

    case CCit_art::C_From::e_Book:
        if (cit_art.GetFrom().GetBook().IsSetImp()) {
            imprint = &cit_art.SetFrom().SetBook().SetImp();
        }
        break;

    case CCit_art::C_From::e_Proc:
        if (cit_art.GetFrom().GetProc().IsSetBook() && cit_art.GetFrom().GetProc().GetBook().IsSetImp()) {
            imprint = &cit_art.SetFrom().SetProc().SetBook().SetImp();
        }
        break;

    default:; // do nothing
    }

    if (imprint) {
        imprint->SetPrepub(CImprint::ePrepub_in_press);
    }
}

}

using namespace fix_pub;

void CPubFix::FixPubEquiv(CPub_equiv& pub_equiv)
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
            ERR_POST_TO_LISTENER(m_err_log, eDiag_Warning, err_Reference, err_Reference_Multiple_ref, "More than one Medline entry in Pub-equiv");
            medlines.resize(1);
        }

        SplitMedlineEntry(medlines);
        pub_list.splice(pub_list.end(), medlines);
    }

    TEntrezId oldpmid = ZERO_ENTREZ_ID;
    if (!pmids.empty()) {

        oldpmid = pmids.front()->GetPmid();

        // check if more than one
        for (auto& pub: pmids) {
            if (pub->GetPmid() != oldpmid) {
                ERR_POST_TO_LISTENER(m_err_log, eDiag_Warning, err_Reference, err_Reference_Multiple_pmid,
                    "Two different pmids in Pub-equiv [" << oldpmid << "] [" << pub->GetPmid() << "]");
            }
        }
        pmids.resize(1);
    }

    TEntrezId oldmuid = ZERO_ENTREZ_ID;
    if (!muids.empty()) {

        oldmuid = muids.front()->GetMuid();

        // check if more than one
        for (auto& pub : muids) {
            if (pub->GetMuid() != oldmuid) {
                ERR_POST_TO_LISTENER(m_err_log, eDiag_Warning, err_Reference, err_Reference_Multiple_pmid,
                    "Two different muids in Pub-equiv  [" << oldmuid << "] [" << pub->GetMuid() << "]");
            }
        }
        muids.resize(1);
    }

    if (!cit_arts.empty()) {
        if (cit_arts.size() > 1) {
            // ditch extras
            ERR_POST_TO_LISTENER(m_err_log, eDiag_Warning, err_Reference, err_Reference_Multiple_ref, "More than one Cit-art in Pub-equiv");
            cit_arts.resize(1);
        }

        CCit_art* cit_art = &cit_arts.front()->SetArticle();
        bool inpress = IsInpress(*cit_art);

        CRef<CPub> new_pub(new CPub);
        new_pub->SetArticle(*cit_art);

        TEntrezId pmid = ZERO_ENTREZ_ID;
        try {
            CMLAClient mla;
            pmid = ENTREZ_ID_FROM(int, mla.AskCitmatchpmid(*new_pub));
        }
        catch (exception &) {
            // pmid == 0
        }

        if ( pmid != ZERO_ENTREZ_ID ) {

            PrintPub(*cit_art, true, false, ENTREZ_ID_TO(long, pmid), m_err_log);

            if (oldpmid > ZERO_ENTREZ_ID && oldpmid != pmid) {
                // already had a pmid
                ERR_POST_TO_LISTENER(m_err_log, eDiag_Error, err_Reference, err_Reference_PmidMissmatch,
                    "OldPMID=" << oldpmid << " doesn't match lookup (" << pmid << "). Keeping lookup.");
            }

            bool set_pmid = true;
            if (m_replace_cit) {

                CRef<CCit_art> new_cit_art = FetchPubPmId(pmid);

                if (new_cit_art.NotEmpty()) {

                    bool new_cit_is_valid(false);
                    if (CAuthListValidator::enabled) {
                        CAuthListValidator::EOutcome outcome = m_authlist_validator.validate(*cit_art, *new_cit_art);
                        switch (outcome) {
                        case CAuthListValidator::eAccept_pubmed:
                            new_cit_is_valid = true;
                            break;
                        case CAuthListValidator::eKeep_genbank:
                            new_cit_art->SetAuthors(cit_art->SetAuthors());
                            cit_art->ResetAuthors();
                            new_cit_is_valid = true;
                            break;
                        case CAuthListValidator::eFailed_validation:
                            new_cit_is_valid = false;
                            break;
                        default:
                            throw logic_error("Invalid outcome returned by CAuthListValidator::validate(): " + std::to_string(outcome));
                        }
                    }
                    else {
                        new_cit_is_valid = TenAuthorsProcess(*cit_art, *new_cit_art, m_err_log);
                    }

                    if (new_cit_is_valid) {
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

                        PrintPub(*cit_art, false, true, ENTREZ_ID_TO(long, pmid), m_err_log);
                        pub_list.splice(pub_list.end(), cit_arts);
                    }

                    set_pmid = false;
                }
                else {
                    ERR_POST_TO_LISTENER(m_err_log, eDiag_Error, err_Reference, err_Reference_FailedToGetPub,
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

            PropagateInPress(inpress, *cit_art);
            return;
        }

        PrintPub(*cit_art, false, false, ENTREZ_ID_TO(long, oldpmid), m_err_log);
        PropagateInPress(inpress, *cit_art);
        pub_list.splice(pub_list.end(), cit_arts);

        return;
    }

    if (oldpmid != ZERO_ENTREZ_ID) {
        // have a pmid but no cit-art

        CRef<CCit_art> new_cit_art = FetchPubPmId(oldpmid);

        if (new_cit_art.NotEmpty()) {

            pub_list.splice(pub_list.end(), pmids);

            if (m_replace_cit) {
                MedlineToISO(*new_cit_art);
                CRef<CPub> cit_pub(new CPub);
                cit_pub->SetArticle(*new_cit_art);
                pub_list.push_back(cit_pub);
            }

            return;
        }
        ERR_POST_TO_LISTENER(m_err_log, eDiag_Warning, err_Reference, err_Reference_No_reference,
            "Cant find article for pmid [" << oldpmid << "]");
    }

    if (oldpmid > ZERO_ENTREZ_ID) {
        pub_list.splice(pub_list.end(), pmids);
    }
    else if (oldmuid > ZERO_ENTREZ_ID) {
        pub_list.splice(pub_list.end(), muids);
    }
}


// Tries to make any Pub into muid / cit - art
void CPubFix::FixPub(CPub& pub)
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

        TEntrezId pmid = ZERO_ENTREZ_ID;
        try {
            CMLAClient mla;
            pmid = ENTREZ_ID_FROM(int, mla.AskCitmatchpmid(pub));
        }
        catch (exception &) {
            // pmid == 0;
        }

        if (pmid > ZERO_ENTREZ_ID) {
            PrintPub(cit_art, true, false, ENTREZ_ID_TO(long, pmid), m_err_log);
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
                        PrintPub(cit_art, false, true, ENTREZ_ID_TO(long, pmid), m_err_log);
                        MedlineToISO(cit_art);
                    }
                }
            }
            else {
                PrintPub(cit_art, false, false, ENTREZ_ID_TO(long, pmid), m_err_log);
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

CRef<CCit_art> CPubFix::FetchPubPmId(TEntrezId pmid)
{
    CRef<CCit_art> cit_art;
    if (pmid < ZERO_ENTREZ_ID)
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

bool CAuthListValidator::enabled = true; // Verified in ID-6550, so set to use it by default
                                         // Setting it to false would lead to a few bugs
bool CAuthListValidator::configured = false;
double CAuthListValidator::cfg_matched_to_min = 0.3333;
double CAuthListValidator::cfg_removed_to_gb = 0.3333;
void CAuthListValidator::Configure(const CNcbiRegistry& cfg, const string& section)
{
    enabled = cfg.GetBool(section, "enabled", enabled);
    cfg_matched_to_min = cfg.GetDouble(section, "matched_to_min", cfg_matched_to_min);
    cfg_removed_to_gb = cfg.GetDouble(section, "removed_to_gb", cfg_removed_to_gb);
    configured = true;
}

CAuthListValidator::CAuthListValidator(IMessageListener* err_log)
    : outcome(eNotSet), pub_year(0), reported_limit("not initialized"), m_err_log(err_log)
{
    if (! configured) {
        Configure(CNcbiApplication::Instance()->GetConfig(), "auth_list_validator");
    }
}

CAuthListValidator::EOutcome CAuthListValidator::validate(const CCit_art& gb_art, const CCit_art& pm_art)
{
    outcome = eNotSet;
    pub_year = 0;
    pub_year = pm_art.GetFrom().GetJournal().GetImp().GetDate().GetStd().GetYear();
    if (pub_year < 1900 || pub_year > 3000) {
        throw logic_error("Publication from PubMed has invalid year: " + std::to_string(pub_year));
    }
    gb_type = CAuth_list::C_Names::SelectionName(gb_art.GetAuthors().GetNames().Which());
    get_lastnames(gb_art.GetAuthors(), removed, gb_auth_string);
    pm_type = CAuth_list::C_Names::SelectionName(pm_art.GetAuthors().GetNames().Which());
    get_lastnames(pm_art.GetAuthors(), added, pm_auth_string);
    matched.clear();
    compare_lastnames();
    actual_matched_to_min = double(cnt_matched) / cnt_min;
    actual_removed_to_gb = double(cnt_removed) / cnt_gb;
    if (actual_removed_to_gb > cfg_removed_to_gb) {
        ERR_POST_TO_LISTENER(m_err_log, eDiag_Warning, err_AuthList, err_AuthList_SignificantDrop,
            "Too many authors removed (" << cnt_removed << ") compared to total Genbank authors (" << cnt_gb << ")");
    }
    // determine outcome according to ID-6514 (see fix_pub.hpp)
    if (pub_year > 1999) {
        reported_limit = "Unlimited";
        outcome = eAccept_pubmed;
    }
    else if (pub_year > 1995) {
        reported_limit = "25 authors";
        if (cnt_gb > 25) {
            ERR_POST_TO_LISTENER(m_err_log, eDiag_Warning, err_AuthList, err_AuthList_PreserveGB,
                "Preserving original " << cnt_gb << " GB authors, ignoring " << cnt_pm << " PubMed authors "
                << "(PubMed limit was " << reported_limit << " in pub.year " << pub_year << ")");
            outcome = eKeep_genbank;
        }
        else {
            outcome = eAccept_pubmed;
        }
    }
    else { // pub_year < 1996
        reported_limit = "10 authors";
        if (cnt_gb > 10) {
            ERR_POST_TO_LISTENER(m_err_log, eDiag_Warning, err_AuthList, err_AuthList_PreserveGB,
                "Preserving original " << cnt_gb << " GB authors, ignoring " << cnt_pm << " PubMed authors "
                << "(PubMed limit was " << reported_limit << " in pub.year " << pub_year << ")");
            outcome = eKeep_genbank;
        }
        else {
            outcome = eAccept_pubmed;
        }
    }
    // check minimum required # of matching authors
    if (actual_matched_to_min < cfg_matched_to_min) {
        ERR_POST_TO_LISTENER(m_err_log, eDiag_Error, err_AuthList, err_AuthList_LowMatch,
            "Only " << cnt_matched << " authors matched between " << cnt_gb << " Genbank and "
            << cnt_pm << " PubMed. Match/Min ratio " << fixed << setprecision(2) << actual_matched_to_min
            << " is below threshold " << fixed << setprecision(2) << cfg_matched_to_min);
        outcome = eFailed_validation;
    }
    return outcome;
}

void CAuthListValidator::DebugDump(CNcbiOstream& out) const
{
    out << "\n--- Debug Dump of CAuthListValidator object ---\n";
    out << "pub_year: " << pub_year << "\n";
    out << "PubMed Auth-list limit in " << pub_year << ": " << reported_limit << "\n";
    out << "Configured ratio 'matched' to 'min(gb,pm)': " << cfg_matched_to_min
        << "; actual: " << actual_matched_to_min << "\n";
    out << "Configured ratio 'removed' to 'gb': " << cfg_removed_to_gb
        << "; actual: " << actual_removed_to_gb << "\n";
    out << "GB author list type: " << gb_type << "; # of entries: " << cnt_gb << "\n";
    out << "PM author list type: " << pm_type << "; # of entries: " << cnt_pm << "\n";
    dumplist("Matched", matched, out);
    dumplist("Added", added, out);
    dumplist("Removed", removed, out);
    const char* outcome_names[] = {"NotSet", "Failed_validation", "Accept_pubmed", "Keep_genbank"};
    out << "Outcome reported: " << outcome_names[outcome] << "(" << outcome << ")\n";
    out << "--- End of Debug Dump of CAuthListValidator object ---\n\n";
}

void CAuthListValidator::dumplist(const char* hdr, const list<string>& lst, CNcbiOstream& out) const
{
    out << lst.size() << " " << hdr << " authors:\n";
    for (const auto& a : lst)
        out << "    " << a << "\n";
}

void CAuthListValidator::compare_lastnames()
{
    auto gbit = removed.begin();
    while (gbit != removed.end()) {
        list<string>::iterator gbnext(gbit);
        ++gbnext;
        list<string>::iterator pmit = std::find(added.begin(), added.end(), *gbit);
        if (pmit != added.end()) {
            matched.push_back(*gbit);
            removed.erase(gbit++);
            added.erase(pmit);
        }
        gbit = gbnext;
    }
    cnt_matched = matched.size();
    cnt_removed = removed.size();
    cnt_added = added.size();
    cnt_gb = cnt_matched + cnt_removed;
    cnt_pm = cnt_matched + cnt_added;
    cnt_min = min(cnt_gb, cnt_pm);
}


void CAuthListValidator::get_lastnames(const CAuth_list& authors, list<string>& lastnames, string& auth_string)
{
    lastnames.clear();
    switch (authors.GetNames().Which()) {
    case CAuth_list::C_Names::e_Std:
        get_lastnames(authors.GetNames().GetStd(), lastnames);
        break;
    case CAuth_list::C_Names::e_Ml:
        {{
            CRef< CAuth_list > authlist_std;
            authlist_std->Assign(authors);
            authlist_std->ConvertMlToStandard();
            get_lastnames(authlist_std->GetNames().GetStd(), lastnames);
        }}
        break;
    case CAuth_list::C_Names::e_Str:
        get_lastnames(authors.GetNames().GetStr(), lastnames);
        break;
    default:
        throw logic_error("Unexpected CAuth_list::C_Name choice: " + CAuth_list::C_Names::SelectionName(authors.GetNames().Which()));
    }
    auth_string = NStr::Join(lastnames, "; ");
}

void CAuthListValidator::get_lastnames(const CAuth_list::C_Names::TStd& authors, list<string>& lastnames)
{
    for (auto& name : authors) {
        if (name->IsSetName() && name->GetName().IsName() && name->GetName().GetName().IsSetLast()) {
            string lname(name->GetName().GetName().GetLast());
            lastnames.push_back(NStr::ToLower(lname));
        }
    }
}

void CAuthListValidator::get_lastnames(const CAuth_list::C_Names::TStr& authors, list<string>& lastnames)
{
    const char* alpha = "abcdefghijklmnopqrstuvwxyz";
    for (auto auth : authors) {
        size_t eow = NStr::ToLower(auth).find_first_not_of(alpha);
        lastnames.push_back(auth.substr(0, eow));
    }
}

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE
