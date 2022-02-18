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

#include <objtools/eutils/api/efetch.hpp>
#include <objects/pubmed/Pubmed_entry.hpp>
#include <objects/medline/Medline_entry.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/mla/Title_msg.hpp>
#include <objects/mla/Title_msg_list.hpp>

#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Date_std.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)


class CECitMatch_Request : public CEUtils_Request
{
public:
    CECitMatch_Request(CRef<CEUtils_ConnContext>& ctx)
      : CEUtils_Request(ctx, "ecitmatch.cgi")
    {
        SetDatabase("pubmed");
    }

    const string& GetJournal() const { return m_journal; }
    void SetJournal(const string& s)
    {
        Disconnect();
        m_journal = s;
    }

    int GetYear() const { return m_year; }
    void SetYear(int n)
    {
        Disconnect();
        m_year = n;
    }

    string GetVol() const { return m_vol; }
    void SetVol(string s)
    {
        Disconnect();
        m_vol = s;
    }

    string GetPage() const { return m_page; }
    void SetPage(string s)
    {
        Disconnect();
        m_page = s;
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

        ostringstream bdata;
        bdata << NStr::URLEncode(GetJournal(), NStr::eUrlEnc_ProcessMarkChars) << '|';
        if (GetYear() > 0) {
            bdata << GetYear();
        }
        bdata << '|';
        bdata << GetVol() << '|';
        bdata << GetPage() << '|';
        bdata << "||";

        args += "&bdata=";
        args += bdata.str();
        return args;
    }

    void SetFromArticle(const CCit_art& A)
    {
        if (A.IsSetFrom() && A.GetFrom().IsJournal()) {
            const CCit_jour& J = A.GetFrom().GetJournal();
            if (J.IsSetTitle()) {
                const CTitle& T = J.GetTitle();
                if (T.IsSet() && !T.Get().empty()) {
                    for (const auto& it : T.Get()) {
                        if (it->IsName()) {
                            this->SetJournal(it->GetName());
                            break;
                        }
                    }
                }
            }
            if (J.IsSetImp()) {
                const CImprint& I = J.GetImp();
                if (I.IsSetDate()) {
                    const CDate& D = I.GetDate();
                    if (D.IsStd()) {
                        this->SetYear(D.GetStd().GetYear());
                    }
                }
                if (I.IsSetVolume()) {
                    this->SetVol(I.GetVolume());
                }
                if (I.IsSetPages()) {
                    string page, page_end;
                    NStr::SplitInTwo(I.GetPages(), "-", page, page_end);
                    this->SetPage(page);
                }
            }
        }
    }

private:
    string m_journal;
    int m_year = 0;
    string m_vol;
    string m_page;
    ERetMode m_RetMode = eRetMode_none;

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


TEntrezId CEUtilsUpdater::CitMatch(const CPub& pub)
{
    unique_ptr<CECitMatch_Request> req(new CECitMatch_Request(m_Ctx));

    req->SetRequestMethod(CEUtils_Request::eHttp_Get);
    req->SetRetMode(CECitMatch_Request::eRetMode_text);
    if (pub.IsArticle()) {
        req->SetFromArticle(pub.GetArticle());
    }

    string resp;
    try {
        string content;
        req->Read(&content);
        NStr::TruncateSpacesInPlace(content);
        vector<string> v;
        NStr::Split(content, "|", v);
        if (v.size() >= 7) {
            resp = NStr::TruncateSpaces(v[6]);
        }
    } catch (...) {
    }

    if (!resp.empty()) {
        if (!isalpha(resp.front())) {
            TIntId pmid;
            if (NStr::StringToNumeric(resp, &pmid, NStr::fConvErr_NoThrow)) {
                return ENTREZ_ID_FROM(TIntId, pmid);
            }
        }
    }

    return ZERO_ENTREZ_ID;
}


CRef<CPub> CEUtilsUpdater::GetPub(TEntrezId pmid, EPubmedError* perr)
{
    unique_ptr<CEFetch_Request> req(
        new CEFetch_Literature_Request(CEFetch_Literature_Request::eDB_pubmed, m_Ctx)
    );

    req->SetRequestMethod(CEUtils_Request::eHttp_Get);
    req->GetId().AddId(to_string(pmid));
    req->SetRetMode(CEFetch_Request::eRetMode_asn);

    CPubmed_entry pme;
    string content;
    req->Read(&content);
    content >> pme;

    if (pme.IsSetMedent()) {
        CMedline_entry& mle = pme.SetMedent();
        if (mle.IsSetCit()) {
            CRef<CPub> cit_art;
            cit_art.Reset(new CPub);
            cit_art->SetArticle(mle.SetCit());
            return cit_art;
        }
    }

    return {};
}


CRef<CTitle_msg_list> CEUtilsUpdater::GetTitle(const CTitle_msg& msg)
{
    // Not yet implemented
    return {};
}

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE
