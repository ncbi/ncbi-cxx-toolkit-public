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
#if 0
    e_I = 1 << 5, // Issue
    e_T = 1 << 6, // Title
#endif
};

eCitMatchFlags constexpr operator|(eCitMatchFlags a, eCitMatchFlags b)
{
    return static_cast<eCitMatchFlags>(static_cast<int>(a) | static_cast<int>(b));
}


class CECitMatch_Request : public CEUtils_Request
{
public:
    CECitMatch_Request(CRef<CEUtils_ConnContext>& ctx)
      : CEUtils_Request(ctx, "ecitmatch.cgi")
    {
        SetDatabase("pubmed");
    }

    void SetRequestParams(const SCitMatch& cm)
    {
        Disconnect();
        m_req = cm;
    }

    enum ERetMode {
        eRetMode_none = 0,
        eRetMode_xml,
        eRetMode_text,
    };

    ERetMode GetRetMode() const { return m_RetMode; }
    void SetRetMode(ERetMode retmode)
    {
        Disconnect();
        m_RetMode = retmode;
    }

    ESerialDataFormat GetSerialDataFormat() const override
    {
        switch (m_RetMode) {
        case eRetMode_xml:
            return eSerial_Xml;
        default:
            return eSerial_None;
        }
    }

    string GetQueryString() const override
    {
        string args = CEUtils_Request::GetQueryString();
        if (m_RetMode != eRetMode_none) {
            args += "&retmode=" + NStr::URLEncode(x_GetRetModeName(), NStr::eUrlEnc_ProcessMarkChars);
        }

        // "journal_title|year|volume|first_page|author_name|your_key|"
        ostringstream bdata;

        // Journal
        if (m_rule & e_J) {
            bdata << NStr::URLEncode(m_req.Journal, NStr::eUrlEnc_ProcessMarkChars);
        }
        bdata << '|';

        // Year
        if (m_rule & e_Y) {
            bdata << m_req.Year;
        }
        bdata << '|';

        // Volume
        if (m_rule & e_V) {
            bdata << NStr::URLEncode(m_req.Volume, NStr::eUrlEnc_ProcessMarkChars);
        }
        bdata << '|';

        // Page
        if (m_rule & e_P) {
            bdata << m_req.Page;
        }
        bdata << '|';

        // Author
        if (m_rule & e_A) {
            bdata << NStr::URLEncode(m_req.Author, NStr::eUrlEnc_ProcessMarkChars);
        }
        bdata << '|';

        // Key
        bdata << '|';

        args += "&bdata=";
        args += bdata.str();
        return args;
    }

    static string GetFirstAuthor(const CAuth_list& authors)
    {
        if (authors.IsSetNames()) {
            const auto& N(authors.GetNames());
            if (N.IsStd()) {
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
            } else if (N.IsMl()) {
                if (! N.GetMl().empty()) {
                    return N.GetMl().front();
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
                    string page, page_end;
                    NStr::SplitInTwo(I.GetPages(), "-", page, page_end);
                    cm.Page = page;
                }
            }
        }
    }

    bool SetRule(eCitMatchFlags rule)
    {
        if ((rule & e_J) && m_req.Journal.empty()) {
            return false;
        }
        if ((rule & e_V) && m_req.Volume.empty()) {
            return false;
        }
        if ((rule & e_P) && m_req.Page.empty()) {
            return false;
        }
        if ((rule & e_Y) && m_req.Year.empty()) {
            return false;
        }
        if ((rule & e_A) && m_req.Author.empty()) {
            return false;
        }

        m_rule = rule;
        return true;
    }

    TEntrezId GetResponse(eCitMatchFlags rule)
    {
        if (! this->SetRule(rule)) {
            m_error = eError_val_citation_not_found;
            return ZERO_ENTREZ_ID;
        }

        string resp;
        try {
            string content;
            this->Read(&content);
            NStr::TruncateSpacesInPlace(content);
            vector<string> v;
            NStr::Split(content, "|", v);
            if (v.size() >= 7) {
                resp = NStr::TruncateSpaces(v[6]);
            }
        } catch (...) {
        }

        if (! resp.empty()) {
            if (! isalpha(resp.front())) {
                TIntId pmid;
                if (NStr::StringToNumeric(resp, &pmid, NStr::fConvErr_NoThrow)) {
                    return ENTREZ_ID_FROM(TIntId, pmid);
                } else {
                    m_error = eError_val_operational_error;
                }
            } else {
                if (NStr::StartsWith(resp, "NOT_FOUND", NStr::eNocase)) {
                    m_error = eError_val_not_found;
                } else if (NStr::StartsWith(resp, "AMBIGUOUS", NStr::eNocase)) {
                    m_error = eError_val_citation_ambiguous;
                }
            }
        }

        return ZERO_ENTREZ_ID;
    }

    EPubmedError GetError() const { return m_error; }

private:
    SCitMatch      m_req;
    ERetMode       m_RetMode = eRetMode_none;
    eCitMatchFlags m_rule    = e_J | e_V | e_P | e_Y | e_A;
    EPubmedError   m_error;

    const char* x_GetRetModeName() const
    {
        switch (m_RetMode) {
        default:
        case eRetMode_none:
            return "none";
        case eRetMode_xml:
            return "xml";
        case eRetMode_text:
            return "text";
        }
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

    req->SetRequestMethod(CEUtils_Request::eHttp_Get);
    req->SetRetMode(CECitMatch_Request::eRetMode_text);
    req->SetRequestParams(cm);

    constexpr array<eCitMatchFlags, 2> ruleset = { e_J | e_V | e_P | e_A, e_J | e_V | e_P };

    for (eCitMatchFlags r : ruleset) {
        TEntrezId pmid = req->GetResponse(r);
        if (pmid != ZERO_ENTREZ_ID) {
            return pmid;
        }
    }

    if (perr) {
        *perr = req->GetError();
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
