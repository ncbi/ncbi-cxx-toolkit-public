/*  $Id$
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
 * Authors:  Vitaly Stakhovsky, NCBI
 *
 * File Description:
 *   Implementation of IPubmedUpdater based on EUtils
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <objtools/edit/eutils_updater.hpp>

#include <objtools/eutils/efetch/PubmedArticle.hpp>
#include <objtools/eutils/efetch/PubmedArticleSet.hpp>
#include <objtools/eutils/efetch/PubmedBookArticle.hpp>
#include <objtools/eutils/efetch/PubmedBookArticleSet.hpp>

#include <objtools/eutils/api/esearch.hpp>
#include <objtools/eutils/esearch/IdList.hpp>
#include <objects/pubmed/Pubmed_entry.hpp>
#include <objects/medline/Medline_entry.hpp>
#include <objects/pub/Pub.hpp>

#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Author.hpp>
#include <objects/biblio/ArticleId.hpp>
#include <objects/biblio/ArticleIdSet.hpp>
#include <objects/biblio/PubStatusDateSet.hpp>

#include <objects/general/Person_id.hpp>
#include <objects/general/Name_std.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Date_std.hpp>
#include <objects/general/Dbtag.hpp>

#include <misc/jsonwrapp/jsonwrapp.hpp>

#include <array>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

CNcbiOstream& operator<<(CNcbiOstream& os, EPubmedError err)
{
    const char* error = nullptr;
    switch (err) {
    case EPubmedError::not_found:
        error = "not-found";
        break;
    case EPubmedError::operational_error:
        error = "operational-error";
        break;
    case EPubmedError::citation_not_found:
        error = "citation-not-found";
        break;
    case EPubmedError::citation_ambiguous:
        error = "citation-ambiguous";
        break;
    case EPubmedError::cannot_connect_pmdb:
        error = "cannot-connect-pmdb";
        break;
    case EPubmedError::cannot_connect_searchbackend_pmdb:
        error = "cannot-connect-searchbackend-pmdb";
        break;
    default:
        return os;
    }

    os << error;
    return os;
}

enum eCitMatchFlags {
    e_J = 1 << 0, // Journal
    e_V = 1 << 1, // Volume
    e_P = 1 << 2, // Page
    e_Y = 1 << 3, // Year
    e_A = 1 << 4, // Author
    e_I = 1 << 5, // Issue
    e_T = 1 << 6, // Title
};

inline eCitMatchFlags constexpr operator|(eCitMatchFlags a, eCitMatchFlags b)
{
    return static_cast<eCitMatchFlags>(static_cast<int>(a) | static_cast<int>(b));
}


static string GetFirstAuthor(const CAuth_list& authors)
{
    if (authors.IsSetNames()) {
        const auto& N(authors.GetNames());
        if (N.IsMl()) {
            if (! N.GetMl().empty()) {
                return N.GetMl().front();
            }
        } else if (N.IsStd()) {
            // convert to ML
            if (! N.GetStd().empty()) {
                const CAuthor& first_author(*N.GetStd().front());
                if (first_author.IsSetName()) {
                    const CPerson_id& person(first_author.GetName());
                    if (person.IsName()) {
                        const CName_std& name_std(person.GetName());
                        if (name_std.IsSetLast()) {
                            string name(name_std.GetLast());
                            if (name_std.IsSetInitials()) {
                                name += ' ';
                                for (char c : name_std.GetInitials()) {
                                    if (isalpha(c)) {
                                        if (islower(c)) {
                                            c = toupper(c);
                                        }
                                        name += c;
                                    }
                                }
                            }
                            return name;
                        }
                    }
                }
            }
        }
    }

    return {};
}

void SCitMatch::FillFromArticle(const CCit_art& A)
{
    if (A.IsSetAuthors()) {
        this->Author = GetFirstAuthor(A.GetAuthors());
    }

    if (A.IsSetFrom() && A.GetFrom().IsJournal()) {
        const CCit_jour& J = A.GetFrom().GetJournal();
        if (J.IsSetTitle()) {
            const CTitle& T = J.GetTitle();
            if (T.IsSet() && ! T.Get().empty()) {
                this->Journal = T.GetTitle();
            }
        }
        if (J.IsSetImp()) {
            const CImprint& I = J.GetImp();
            if (I.IsSetDate()) {
                const CDate& D = I.GetDate();
                if (D.IsStd()) {
                    auto year = D.GetStd().GetYear();
                    if (year > 0) {
                        this->Year = to_string(year);
                    }
                }
            }
            if (I.IsSetVolume()) {
                this->Volume = I.GetVolume();
            }
            if (I.IsSetPages()) {
                this->Page  = I.GetPages();
                auto pos = this->Page.find('-');
                if (pos != string::npos) {
                    this->Page.resize(pos);
                }
            }
            if (I.IsSetIssue()) {
                this->Issue = I.GetIssue();
            }
            if (I.IsSetPrepub()) {
                this->InPress = (I.GetPrepub() == CImprint::ePrepub_in_press);
            }
        }
    }

    if (A.IsSetTitle()) {
        const CTitle& T = A.GetTitle();
        if (T.IsSet() && ! T.Get().empty()) {
            this->Title = T.GetTitle();
        }
    }
}

class CECitMatch_Request : public CESearch_Request
{
public:
    CECitMatch_Request(CRef<CEUtils_ConnContext>& ctx) :
        CESearch_Request("pubmed", ctx)
    {
    }

    static void NormalizeJournal(string& s)
    {
        for (char& c : s) {
            switch (c) {
            case '.':
            case ':':
            case '[':
            case ']':
            case '(':
            case ')':
                c = ' ';
                break;
            default:
                break;
            }
        }
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

    static bool BuildSearchTerm(const SCitMatch& cm, eCitMatchFlags rule, string& term)
    {
        if ((rule & e_J) && cm.Journal.empty()) {
            return false;
        }
        if ((rule & e_Y) && cm.Year.empty()) {
            return false;
        }
        if ((rule & e_V) && cm.Volume.empty()) {
            return false;
        }
        if ((rule & e_P) && cm.Page.empty()) {
            return false;
        }
        if ((rule & e_A) && cm.Author.empty()) {
            return false;
        }
        if ((rule & e_I) && cm.Issue.empty()) {
            return false;
        }
        if ((rule & e_T) && cm.Title.empty()) {
            return false;
        }

        vector<string> v;
        if (rule & e_J) {
            string journal = cm.Journal;
            NormalizeJournal(journal);
            v.push_back(journal + "[Journal]");
        }
        if (rule & e_Y) {
            v.push_back(cm.Year + "[pdat]");
        }
        if (rule & e_V) {
            v.push_back(cm.Volume + "[vol]");
        }
        if (rule & e_P) {
            string page = cm.Page;
            auto   pos  = page.find('-');
            if (pos != string::npos) {
                page.resize(pos);
            }
            v.push_back(page + "[page]");
        }
        if (rule & e_A) {
            v.push_back(cm.Author + "[auth]");
        }
        if (rule & e_I) {
            v.push_back(cm.Issue + "[iss]");
        }
        if (rule & e_T) {
            string title = cm.Title;
            NormalizeTitle(title);
            v.push_back(title + "[title]");
        }
        term = NStr::Join(v, " AND ");
        return true;
    }

    TEntrezId GetResponse(EPubmedError& err)
    {
        err = EPubmedError::operational_error;

        CRef<esearch::CESearchResult> result = this->GetESearchResult();
        if (result && result->IsSetData()) {
            auto& D = result->GetData();
            if (D.IsInfo() && D.GetInfo().IsSetContent() && D.GetInfo().GetContent().IsSetIdList()) {
                const auto& idList = D.GetInfo().GetContent().GetIdList();
                if (idList.IsSetId()) {
                    const auto& ids = idList.GetId();
                    if (ids.size() == 0) {
                        err = EPubmedError::not_found;
                    } else if (ids.size() == 1) {
                        string id = ids.front();
                        TIntId pmid;
                        if (NStr::StringToNumeric(id, &pmid, NStr::fConvErr_NoThrow)) {
                            return ENTREZ_ID_FROM(TIntId, pmid);
                        } else {
                            err = EPubmedError::not_found;
                        }
                    } else {
                        err = EPubmedError::citation_ambiguous;
                    }
                } else {
                    err = EPubmedError::not_found;
                }
            }
        }

        return ZERO_ENTREZ_ID;
    }
};


CEUtilsUpdater::CEUtilsUpdater(ENormalize norm) :
    m_Ctx(new CEUtils_ConnContext), m_Norm(norm)
{
}

TEntrezId CEUtilsUpdater::CitMatch(const CPub& pub, EPubmedError* perr)
{
    if (pub.IsArticle()) {
        SCitMatch cm;
        cm.FillFromArticle(pub.GetArticle());
        return CitMatch(cm, perr);
    } else {
        if (perr) {
            *perr = EPubmedError::not_found;
        }
        return ZERO_ENTREZ_ID;
    }
}

TEntrezId CEUtilsUpdater::CitMatch(const SCitMatch& cm, EPubmedError* perr)
{
    unique_ptr<CECitMatch_Request> req(new CECitMatch_Request(m_Ctx));
    req->SetField("title");
    req->SetRetMax(2);
    req->SetUseHistory(false);
    EPubmedError err = EPubmedError::citation_not_found;

    // clang-format off
    constexpr array<eCitMatchFlags, 6> ruleset_single = {
        e_J | e_V | e_P       | e_A | e_I,
        e_J | e_V | e_P       | e_A,
        e_J | e_V | e_P,
        e_J       | e_P | e_Y | e_A,
        e_J                   | e_A       | e_T,
                                e_A       | e_T,
    };

    constexpr array<eCitMatchFlags, 6> ruleset_in_press = {
        e_J | e_V | e_P | e_Y | e_A,
        e_J | e_V | e_P | e_Y,
        e_J | e_V       | e_Y | e_A       | e_T,
        e_J | e_V       | e_Y             | e_T,
        e_J             | e_Y | e_A       | e_T,
                          e_Y | e_A       | e_T,
    };
    // clang-format on

    const auto& ruleset = cm.InPress ? ruleset_in_press : ruleset_single;
    const unsigned n       = cm.Option1 ? 6 : 5;

    for (unsigned i = 0; i < n; ++i) {
        eCitMatchFlags r = ruleset[i];

        string term;
        if (CECitMatch_Request::BuildSearchTerm(cm, r, term)) {
            req->SetArgument("term", term);
            req->SetRetType(CESearch_Request::eRetType_uilist);
            TEntrezId pmid = req->GetResponse(err);
            if (pmid != ZERO_ENTREZ_ID) {
                return pmid;
            }
        }
    }

    if (perr) {
        *perr = err;
    }
    return ZERO_ENTREZ_ID;
}


static void Normalize(CPub& pub)
{
    if (pub.IsMedline() && pub.GetMedline().IsSetCit()) {
        CCit_art& A = pub.SetMedline().SetCit();
#if 0
        // Ensure period at title end; RW-1946
        if (A.IsSetTitle() && ! A.GetTitle().Get().empty()) {
            auto& title = A.SetTitle().Set().front();
            if (title->IsName()) {
                string& name = title->SetName();
                if (! name.empty()) {
                    char ch = name.back();
                    if (ch != '.') {
                        if (ch == ' ') {
                            name.back() = '.';
                        } else {
                            name.push_back('.');
                        }
                    }
                }
            }
        }
#endif
        if (A.IsSetIds()) {
            auto& ids = A.SetIds().Set();

            // Strip ELocationID; RW-1954
            auto IsELocationID = [](const CRef<CArticleId>& id) -> bool {
                if (id->IsOther()) {
                    const CDbtag& dbt = id->GetOther();
                    if (dbt.CanGetDb()) {
                        const string& dbn = dbt.GetDb();
                        if (NStr::StartsWith(dbn, "ELocationID", NStr::eNocase)) {
                            return true;
                        }
                    }
                }
                return false;
            };
            ids.remove_if(IsELocationID);

            // sort ids; RW-1865
            auto pred = [](const CRef<CArticleId>& l, const CRef<CArticleId>& r) -> bool {
                CArticleId::E_Choice chl = l->Which();
                CArticleId::E_Choice chr = r->Which();
                if (chl == CArticleId::e_Other && chr == CArticleId::e_Other) {
                    const CDbtag& dbtl = l->GetOther();
                    const CDbtag& dbtr = r->GetOther();
                    if (dbtl.CanGetDb() && dbtr.CanGetDb()) {
                        const string& dbnl = dbtl.GetDb();
                        const string& dbnr = dbtr.GetDb();
                        // pmc comes first
                        if (NStr::CompareNocase("pmc", dbnr) == 0)
                            return false;
                        if (NStr::CompareNocase(dbnl, "pmc") == 0)
                            return true;
                        // mid comes second
                        if (NStr::CompareNocase("mid", dbnr) == 0)
                            return false;
                        if (NStr::CompareNocase(dbnl, "mid") == 0)
                            return true;
                        return dbnl < dbnr;
                    }
                }
                return chl < chr;
            };
            ids.sort(pred);
        }

        if (A.IsSetFrom() && A.GetFrom().IsBook()) {
            CCit_book& book = A.SetFrom().SetBook();
            if (book.IsSetImp() && book.GetImp().IsSetHistory()) {
                book.SetImp().ResetHistory();
            }
        }
    }
}

namespace
{
    struct CFetch_Request : CEUtils_Request {
        TEntrezId m_pmid;
        CFetch_Request(CRef<CEUtils_ConnContext>& ctx, TEntrezId pmid) :
            CEUtils_Request(ctx, "efetch.fcgi"), m_pmid(pmid)
        {
            SetRequestMethod(CEUtils_Request::eHttp_Get);
        }
        string GetQueryString() const override
        {
            return "db=pubmed&retmode=xml&id="s + to_string(m_pmid);
        }
    };
}

CRef<CPubmed_entry> CEUtilsUpdater::x_GetPubmedEntry(TEntrezId pmid, EPubmedError* perr)
{
    CFetch_Request req(m_Ctx, pmid);

    eutils::CPubmedArticleSet pas;
    string content;
    req.Read(&content);
    try {
        CNcbiIstrstream(content) >> MSerial_Xml >> pas;
    } catch (const CSerialException&) {
        if (perr) {
            *perr = EPubmedError::operational_error;
        }
        return {};
    } catch (...) {
        if (perr) {
            *perr = EPubmedError::citation_not_found;
        }
        return {};
    }

    const auto& pp = pas.GetPP().GetPP();
    if (! pp.empty()) {
        const auto& ppf = *pp.front();

        CRef<CPubmed_entry> pme;
        if (ppf.IsPubmedArticle()) {
            const eutils::CPubmedArticle& article = ppf.GetPubmedArticle();
            pme.Reset(article.ToPubmed_entry());
        } else if (ppf.IsPubmedBookArticle()) {
            const eutils::CPubmedBookArticle& article = ppf.GetPubmedBookArticle();
            pme.Reset(article.ToPubmed_entry());
        }
        return pme;
    }

    if (perr) {
        *perr = EPubmedError::citation_not_found;
    }
    return {};
}

CRef<CPub> CEUtilsUpdater::x_GetPub(TEntrezId pmid, EPubmedError* perr)
{
    CRef<CPubmed_entry> pme = x_GetPubmedEntry(pmid, perr);
    if (pme && pme->IsSetMedent()) {
        CRef<CPub> pub(new CPub);
        pub->SetMedline().Assign(pme->GetMedent());
        if (m_Norm == ENormalize::On) {
            Normalize(*pub);
        }
        if (m_pub_interceptor) {
            m_pub_interceptor(pub);
        }
        return pub;
    }
    return {};
}

CRef<CPub> CEUtilsUpdater::GetPub(TEntrezId pmid, EPubmedError* perr)
{
    CConstRef<CPub> pub = GetPubmedEntry(pmid, perr);
    if (pub && pub->IsMedline() && pub->GetMedline().IsSetCit()) {
        CRef<CPub> ret(new CPub);
        ret->SetArticle().Assign(pub->GetMedline().GetCit());
        return ret;
    }
    return {};
}

CRef<CPub> CEUtilsUpdater::GetPubmedEntry(TEntrezId pmid, EPubmedError* perr)
{
    return x_GetPub(pmid, perr);
}

CRef<CPub> CEUtilsUpdaterWithCache::GetPubmedEntry(TEntrezId pmid, EPubmedError* perr)
{
    m_num_requests++;
    CConstRef<CPub> pub;
    auto it = m_cache.find(pmid);
    if (it == m_cache.end()) {
        pub = x_GetPub(pmid, perr);
        if (pub) {
            m_cache[pmid] = pub;
        } else {
            return {};
        }
    } else {
        m_cache_hits++;
        pub = it->second;
    }

    CRef<CPub> copied(new CPub());
    copied->Assign(*pub);
    return copied;
}

void CEUtilsUpdaterWithCache::ReportStats(std::ostream& os)
{
    os << "CEUtilsUpdater: " << m_cache_hits << " cache_hits out of " << m_num_requests << " requests\n";
}

void CEUtilsUpdaterWithCache::ClearCache()
{
    m_cache.clear();
}


// Hydra replacement using citmatch api; RW-1918,RW-1999

static bool ParseJson(const string& json, vector<TEntrezId>& pmids, string& msg)
{
    CJson_Document doc(json);
    CJson_Object   dobj(doc.SetObject());

    _ASSERT(dobj["version"].GetValue().GetString() == "1.0");
    _ASSERT(dobj["operation"].GetValue().GetString() == "citmatch");
    if (! dobj["success"].GetValue().GetBool()) {
        msg = "no success";
        return false;
    }

    const auto& result = dobj.at("result");
    if (! result.IsObject()) {
        msg = "result is not an object";
        return false;
    }

    const auto& result_obj = result.GetObject();
#ifdef _DEBUG
    const auto& count = result_obj.at("count");
    _ASSERT(count.GetValue().IsNumber());
    const auto& type = result_obj.at("type");
    _ASSERT(type.GetValue().GetString() == "uids");
#endif
    const auto& uids = result_obj.at("uids");
    if (! uids.IsArray()) {
        msg = "uids is not an array";
        return false;
    }

    const auto& uids_array = uids.GetArray();
    for (auto it = uids_array.begin(); it != uids_array.end(); ++it) {
        if (it->IsObject()) {
            const auto& uid_obj = it->GetObject();
            auto it2 = uid_obj.find("pubmed");
            if (it2 != uid_obj.end()) {
                const auto sPubmed = it2->value.GetValue().GetString();
                TEntrezId  pmid    = NStr::StringToNumeric<TEntrezId>(sPubmed);
                pmids.push_back(pmid);
            }
        }
    }

    return true;
}

bool CEUtilsUpdater::DoPubSearch(const vector<string>& query, vector<TEntrezId>& pmids)
{
    static const string hostname = "pubmed.ncbi.nlm.nih.gov";
    static const string path     = "/api/citmatch";
    static const string args     = "method=heuristic&raw-text=";
    const string        params   = args + NStr::URLEncode(NStr::Join(query, "+"));

    for (unsigned attempt = 1; attempt <= 5; ++attempt) {
        try {
            CConn_HttpStream http(hostname, path, params);

            string result;

            char buf[1024];
            while (! http.fail()) {
                http.read(buf, sizeof(buf));
                result.append(buf, http.gcount());
            }

            string msg;
            if (! ParseJson(result, pmids, msg)) {
                ERR_POST(Warning << "error parsing json: " << msg);
                return false;
            } else {
                return ! pmids.empty();
            }
        } catch (CException& e) {
            ERR_POST(Warning << "failed on attempt " << attempt
                             << ": " << e);
        }

        SleepSec(attempt);
    }

    return false;
}

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE
