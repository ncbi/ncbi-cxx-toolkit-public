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
#include <objtools/eutils/api/efetch.hpp>
#include <objtools/eutils/api/esearch.hpp>
#include <objtools/eutils/esearch/IdList.hpp>
#include <objects/pubmed/Pubmed_entry.hpp>
#include <objects/medline/Medline_entry.hpp>
#include <objects/pub/Pub.hpp>

#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Author.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/general/Name_std.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Date_std.hpp>

#include <array>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

enum eCitMatchFlags {
    e_J = 1 << 0, // Journal
    e_V = 1 << 1, // Volume
    e_P = 1 << 2, // Page
    e_Y = 1 << 3, // Year
    e_A = 1 << 4, // Author
    e_I = 1 << 5, // Issue
    e_T = 1 << 6, // Title
};

eCitMatchFlags constexpr operator|(eCitMatchFlags a, eCitMatchFlags b)
{
    return static_cast<eCitMatchFlags>(static_cast<int>(a) | static_cast<int>(b));
}


class CECitMatch_Request : public CESearch_Request
{
public:
    CECitMatch_Request(CRef<CEUtils_ConnContext>& ctx) :
        CESearch_Request("pubmed", ctx)
    {
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

    static void FillFromArticle(SCitMatch& cm, const CCit_art& A)
    {
        if (A.IsSetAuthors()) {
            cm.Author = GetFirstAuthor(A.GetAuthors());
        }

        if (A.IsSetFrom() && A.GetFrom().IsJournal()) {
            const CCit_jour& J = A.GetFrom().GetJournal();
            if (J.IsSetTitle()) {
                const CTitle& T = J.GetTitle();
                if (T.IsSet() && ! T.Get().empty()) {
                    cm.Journal = T.GetTitle();
                }
            }
            if (J.IsSetImp()) {
                const CImprint& I = J.GetImp();
                if (I.IsSetDate()) {
                    const CDate& D = I.GetDate();
                    if (D.IsStd()) {
                        auto year = D.GetStd().GetYear();
                        if (year > 0) {
                            cm.Year = to_string(year);
                        }
                    }
                }
                if (I.IsSetVolume()) {
                    cm.Volume = I.GetVolume();
                }
                if (I.IsSetPages()) {
                    cm.Page  = I.GetPages();
                    auto pos = cm.Page.find('-');
                    if (pos != string::npos) {
                        cm.Page.resize(pos);
                    }
                }
                if (I.IsSetIssue()) {
                    cm.Issue = I.GetIssue();
                }
                if (I.IsSetPrepub()) {
                    cm.InPress = (I.GetPrepub() == CImprint::ePrepub_in_press);
                }
            }
        }

        if (A.IsSetTitle()) {
            const CTitle& T = A.GetTitle();
            if (T.IsSet() && ! T.Get().empty()) {
                cm.Title = T.GetTitle();
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
            v.push_back(cm.Journal + "[Journal]");
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
            v.push_back(cm.Title + "[title]");
        }
        term = NStr::Join(v, " AND ");
        return true;
    }

    TEntrezId GetResponse(EPubmedError& err)
    {
        err = eError_val_operational_error;

        CRef<esearch::CESearchResult> result = this->GetESearchResult();
        if (result && result->IsSetData()) {
            auto& D = result->GetData();
            if (D.IsInfo() && D.GetInfo().IsSetContent() && D.GetInfo().GetContent().IsSetIdList()) {
                const auto& idList = D.GetInfo().GetContent().GetIdList();
                if (idList.IsSetId()) {
                    const auto& ids = idList.GetId();
                    if (ids.size() == 0) {
                        err = eError_val_not_found;
                    } else if (ids.size() == 1) {
                        string id = ids.front();
                        TIntId pmid;
                        if (NStr::StringToNumeric(id, &pmid, NStr::fConvErr_NoThrow)) {
                            return ENTREZ_ID_FROM(TIntId, pmid);
                        } else {
                            err = eError_val_not_found;
                        }
                    } else {
                        err = eError_val_citation_ambiguous;
                    }
                } else {
                    err = eError_val_not_found;
                }
            }
        }

        return ZERO_ENTREZ_ID;
    }
};


CEUtilsUpdater::CEUtilsUpdater()
{
    m_Ctx.Reset(new CEUtils_ConnContext);
}


TEntrezId CEUtilsUpdater::CitMatch(const CPub& pub, EPubmedError* perr)
{
    if (pub.IsArticle()) {
        SCitMatch cm;
        CECitMatch_Request::FillFromArticle(cm, pub.GetArticle());
        return CitMatch(cm, perr);
    } else {
        if (perr) {
            *perr = eError_val_not_found;
        }
        return ZERO_ENTREZ_ID;
    }
}

TEntrezId CEUtilsUpdater::CitMatch(const SCitMatch& cm, EPubmedError* perr)
{
    unique_ptr<CECitMatch_Request> req(new CECitMatch_Request(m_Ctx));
    req->SetRetMax(2);
    req->SetUseHistory(false);
    EPubmedError err = eError_val_citation_not_found;

    // clang-format off
    constexpr array<eCitMatchFlags, 5> ruleset_single = {
        e_J | e_V | e_P       | e_A | e_I,
        e_J | e_V | e_P       | e_A,
        e_J | e_V | e_P,
        e_J       | e_P | e_Y | e_A,
        e_J                   | e_A       | e_T,
    };

    constexpr array<eCitMatchFlags, 5> ruleset_in_press = {
        e_J | e_V | e_P | e_Y | e_A,
        e_J | e_V | e_P | e_Y,
        e_J | e_V       | e_Y | e_A       | e_T,
        e_J | e_V       | e_Y             | e_T,
        e_J             | e_Y | e_A       | e_T,
    };
    // clang-format on

    const auto& ruleset = cm.InPress ? ruleset_in_press : ruleset_single;

    for (eCitMatchFlags r : ruleset) {
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


CRef<CPub> CEUtilsUpdater::GetPub(TEntrezId pmid, EPubmedError* perr)
{
    unique_ptr<CEFetch_Request> req(
        new CEFetch_Literature_Request(CEFetch_Literature_Request::eDB_pubmed, m_Ctx)
    );

    req->SetRequestMethod(CEUtils_Request::eHttp_Get);
    req->GetId().AddId(NStr::NumericToString(pmid));
    req->SetRetMode(CEFetch_Request::eRetMode_xml);

    eutils::CPubmedArticleSet pas;
    string content;
    req->Read(&content);
    try {
        CNcbiIstrstream(content) >> MSerial_Xml >> pas;
    } catch (...) {
        if (perr) {
            *perr = EError_val::eError_val_citation_not_found;
        }
        return {};
    }

    const auto& pp = pas.GetPP().GetPP();
    if (! pp.empty()) {
        const auto& ppf = *pp.front();
        if (ppf.IsPubmedArticle()) {
            const eutils::CPubmedArticle& article = ppf.GetPubmedArticle();
            CRef<CPubmed_entry> pme = article.ToPubmed_entry();
            if (pme->IsSetMedent()) {
                const CMedline_entry& mle = pme->GetMedent();
                if (mle.IsSetCit()) {
                    CRef<CPub> cit_art(new CPub);
                    cit_art->SetArticle().Assign(mle.GetCit());
                    return cit_art;
                }
            }
        } else if (ppf.IsPubmedBookArticle()) {
            const eutils::CPubmedBookArticle& article = ppf.GetPubmedBookArticle();
            CRef<CPubmed_entry> pme = article.ToPubmed_entry();
            if (pme->IsSetMedent()) {
                const CMedline_entry& mle = pme->GetMedent();
                if (mle.IsSetCit()) {
                    CRef<CPub> cit_art(new CPub);
                    cit_art->SetArticle().Assign(mle.GetCit());
                    return cit_art;
                }
            }
        }
    }

    if (perr) {
        *perr = EError_val::eError_val_citation_not_found;
    }
    return {};
}


string CEUtilsUpdater::GetTitle(const string&)
{
    // Not yet implemented
    return {};
}

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE
