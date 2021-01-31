/*  $Id$
 * =========================================================================
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
 * =========================================================================
 *
 * Authors: Colleen Bollin, based on similar discrepancy tests
 *
 */

#include <ncbi_pch.hpp>
#include <sstream>
#include <objects/general/User_object.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Cit_let.hpp>
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/Cit_pat.hpp>
#include <objects/biblio/Cit_proc.hpp>
#include <objects/biblio/Title.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Author.hpp>
#include <objects/biblio/Affil.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/general/Name_std.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objects/submit/Contact_info.hpp>
#include <corelib/ncbistre.hpp>
#include <objmgr/seqdesc_ci.hpp>

#include "discrepancy_core.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(NDiscrepancy)
USING_SCOPE(objects);

DISCREPANCY_MODULE(pub_tests);


// TITLE_AUTHOR_CONFLICT

const string kTitleAuthorConflictStart = "[n] articles have title '";
const string kTitleAuthorConflictEnd = "' but do not have the same author list";

string GetAuthorString(const CName_std& name_std)
{
    string author = kEmptyStr;

    if (name_std.IsSetInitials() && !NStr::IsBlank(name_std.GetInitials())) {
        author += name_std.GetInitials();
    }
    if (name_std.IsSetLast() && !NStr::IsBlank(name_std.GetLast())) {
        author += name_std.GetLast();
    }
    return author;
}


string GetAuthorString(const CAuthor& author)
{
    if (!author.IsSetName()) {
        return kEmptyStr;
    }
    string rval = kEmptyStr;
    switch (author.GetName().Which()) {
        case CPerson_id::e_Name:
            rval = GetAuthorString(author.GetName().GetName());
            break;
        case CPerson_id::e_Ml:
            rval = author.GetName().GetMl();
            break;
        case CPerson_id::e_Str:
            rval = author.GetName().GetStr();
            break;
        case CPerson_id::e_Consortium:
            rval = author.GetName().GetConsortium();
            break;
        default:
            break;
    }
    return rval;
}


string GetAuthorString(const CAuth_list& auth_list)
{
    string authors = kEmptyStr;
    if (auth_list.IsSetNames()) {
        switch (auth_list.GetNames().Which()) {
            case CAuth_list::C_Names::e_Std:
                for (const auto& it : auth_list.GetNames().GetStd()) {
                    string a = GetAuthorString(*it);
                    if (!NStr::IsBlank(a)) {
                        if (!NStr::IsBlank(authors)) {
                            authors += ", ";
                        }
                        authors += a;
                    }
                }
                break;
            case CAuth_list::C_Names::e_Ml:
                for (const string& it : auth_list.GetNames().GetMl()) {
                    if (!NStr::IsBlank(it)) {
                        if (!NStr::IsBlank(authors)) {
                            authors += ", ";
                        }
                        authors += it;
                    }
                }
                break;
            case CAuth_list::C_Names::e_Str:
                for (const string& it : auth_list.GetNames().GetStr()) {
                    if (!NStr::IsBlank(it)) {
                        if (!NStr::IsBlank(authors)) {
                            authors += ", ";
                        }
                        authors += it;
                    }
                }
                break;
            default:
                break;
        }
    }
    return authors;
}


void GetPubTitleAndAuthors(const CPub& pub, string& title, string & authors)
{
    switch (pub.Which()) {
        case CPub::e_Gen:
            if (pub.GetGen().IsSetTitle()) {
                title = pub.GetGen().GetTitle();
            }
            if (pub.GetGen().IsSetAuthors()) {
                authors = GetAuthorString(pub.GetGen().GetAuthors());
            }
            break;
        case CPub::e_Article:
            if (pub.GetArticle().IsSetTitle()) {
                title = pub.GetArticle().GetTitle().GetTitle();
            }
            if (pub.GetArticle().IsSetAuthors()) {
                authors = GetAuthorString(pub.GetArticle().GetAuthors());
            }
            break;
        case CPub::e_Patent:
            if (pub.GetPatent().IsSetTitle()) {
                title = pub.GetPatent().GetTitle();
            }
            if (pub.GetPatent().IsSetAuthors()) {
                authors = GetAuthorString(pub.GetPatent().GetAuthors());
            }
            break;
        case CPub::e_Man:
            if (pub.GetMan().IsSetCit()) {
                if (pub.GetMan().GetCit().IsSetTitle()) {
                    title = pub.GetMan().GetCit().GetTitle().GetTitle();
                }
                if (pub.GetMan().GetCit().IsSetAuthors()) {
                    authors = GetAuthorString(pub.GetMan().GetCit().GetAuthors());
                }
            }
            break;
        case CPub::e_Book:
            if (pub.GetBook().IsSetTitle()) {
                title = pub.GetBook().GetTitle().GetTitle();
            }
            if (pub.GetBook().IsSetAuthors()) {
                authors = GetAuthorString(pub.GetBook().GetAuthors());
            }
            break;
        default:
            break;
    }
}


void GetPubTitleAndAuthors(const CPubdesc& pubdesc, string& title, string& authors)
{
    title = kEmptyStr;
    authors = kEmptyStr;
    if (pubdesc.IsSetPub()) {
        for (const auto& it : pubdesc.GetPub().Get()) {
            GetPubTitleAndAuthors(*it, title, authors);
            if (!NStr::IsBlank(title)) {
                break;
            }
        }
    }
}


DISCREPANCY_CASE(TITLE_AUTHOR_CONFLICT, DESC, eDisc | eOncaller | eSmart | eFatal, "Publications with the same titles should have the same authors")
{
    for (const auto& desc : context.GetSeqdesc()) {
        if (desc.IsPub()) {
            string title;
            string authors;
            GetPubTitleAndAuthors(desc.GetPub(), title, authors);
            if (!title.empty()) {
                m_Objs[kEmptyStr][title][authors].Add(*context.SeqdescObjRef(desc));
            }
        }
    }
}


static const char* kTitleAuthorConflict = "Publication Title/Author Inconsistencies";

DISCREPANCY_SUMMARIZE(TITLE_AUTHOR_CONFLICT)
{
    if (m_Objs.empty()) {
        return;
    }
    for (auto& it : m_Objs[kEmptyStr].GetMap()) {
        if (it.second->GetMap().size() > 1) {
            string top = "[n] articles have title [(]'" + it.first + "'[)] but do not have the same author list";
            for (auto& aa : it.second->GetMap()) {
                string label = "[n] article[s] [has] title [(]'" + it.first + "'[)] and author list [(]'" + aa.first + "'";
                for (auto& obj: aa.second->GetObjects()) {
                    m_Objs[kTitleAuthorConflict][top][label].Add(*obj).Fatal();
                }
            }
        }
    }
    m_Objs.GetMap().erase(kEmptyStr);
    if (m_Objs.Exist(kTitleAuthorConflict) && m_Objs[kTitleAuthorConflict].GetMap().size() == 1) {
        m_ReportItems = m_Objs.GetMap().cbegin()->second->Export(*this)->GetSubitems();
    }
    else {
        m_ReportItems = m_Objs.Export(*this)->GetSubitems();
    }
}


// UNPUB_PUB_WITHOUT_TITLE

bool IsPubUnpublished(const CImprint& imp)
{
    bool is_unpublished = false;
    if (!imp.IsSetPrepub() || imp.GetPrepub() == CImprint::ePrepub_other) {
        is_unpublished = true;
    }
    return is_unpublished;
}


bool IsPubUnpublished(const CCit_jour& journal)
{
    bool is_unpublished = false;
    if (journal.IsSetImp()) {
        is_unpublished = IsPubUnpublished(journal.GetImp());
    }
    return is_unpublished;
}


bool IsPubUnpublished(const CCit_book& book)
{
    bool is_unpublished = false;
    if (book.IsSetImp()) {
        is_unpublished = IsPubUnpublished(book.GetImp());
    }
    return is_unpublished;
}


bool IsPubUnpublished(const CCit_proc& proc)
{
    bool is_unpublished = false;
    if (proc.IsSetBook()) {
        is_unpublished = IsPubUnpublished(proc.GetBook());
    }
    return is_unpublished;
}


bool IsPubUnpublished(const CCit_let& let)
{
    bool is_unpublished = false;
    if (let.IsSetCit() && let.GetCit().IsSetImp()) {
        is_unpublished = IsPubUnpublished(let.GetCit().GetImp());
    }
    return is_unpublished;
}


bool IsPubUnpublished(const CPub& pub)
{
    bool is_unpublished = false;

    switch (pub.Which()) {
        case CPub::e_Gen:
            if (pub.GetGen().IsSetCit() && NStr::FindNoCase(pub.GetGen().GetCit(), "unpublished") != string::npos) {
                is_unpublished = true;
            }
            break;
        case CPub::e_Article:
            if (pub.GetArticle().IsSetFrom()) {
                if (pub.GetArticle().GetFrom().IsJournal()) {
                    is_unpublished = IsPubUnpublished(pub.GetArticle().GetFrom().GetJournal());
                } else if (pub.GetArticle().GetFrom().IsBook()) {
                    is_unpublished = IsPubUnpublished(pub.GetArticle().GetFrom().GetBook());
                } else if (pub.GetArticle().GetFrom().IsProc()) {
                    is_unpublished = IsPubUnpublished(pub.GetArticle().GetFrom().GetProc());
                }
            }
            break;
        case CPub::e_Book:
            is_unpublished = IsPubUnpublished(pub.GetBook());
            break;
        case CPub::e_Journal:
            is_unpublished = IsPubUnpublished(pub.GetJournal());
            break;
        case CPub::e_Proc:
            is_unpublished = IsPubUnpublished(pub.GetProc());
            break;
        case CPub::e_Patent:
            is_unpublished = true;
            break;
        case CPub::e_Man:
            is_unpublished = IsPubUnpublished(pub.GetMan());
            break;
        default:
            break;
    }

    return is_unpublished;
}


bool HasUnpubWithoutTitle(const CPubdesc& pubdesc)
{
    if (!pubdesc.IsSetPub()) {
        return false;
    }
    for (const auto& it : pubdesc.GetPub().Get()) {
        if (IsPubUnpublished(*it)) {
            string title = kEmptyStr;
            string authors = kEmptyStr;
            GetPubTitleAndAuthors(*it, title, authors);
            if (NStr::IsBlank(title) || NStr::EqualNocase(title, "Direct Submission")) {
                return true;
            }
        }
    }
    return false;
}


DISCREPANCY_CASE(UNPUB_PUB_WITHOUT_TITLE, PUBDESC, eDisc | eOncaller | eSubmitter | eSmart | eBig | eFatal, "Unpublished pubs should have titles")
{
    for (const CPubdesc* pubdesc : context.GetPubdescs()) {
        if (HasUnpubWithoutTitle(*pubdesc)) {
            m_Objs["[n] unpublished pub[s] [has] no title"].Add(*context.PubdescObjRef(*pubdesc)).Fatal();
        }
    }
}


DISCREPANCY_SUMMARIZE(UNPUB_PUB_WITHOUT_TITLE)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// MISSING_AFFIL

bool HasNoAffiliation(const CAffil& affil)
{
    bool rval = false;
    if (affil.IsStr()) {
        if (NStr::IsBlank(affil.GetStr())) {
            rval = true;
        }
    } else if (affil.IsStd()) {
        if (!affil.GetStd().IsSetAffil() ||
            NStr::IsBlank(affil.GetStd().GetAffil())) {
            rval = true;
        }
    } else {
        rval = true;
    }
    return rval;
}


bool IsCitSubMissingAffiliation(const CPubdesc& pubdesc)
{
    if (!pubdesc.IsSetPub()) {
        return false;
    }
    bool rval = false;
    for (const auto& it : pubdesc.GetPub().Get()) {
        if (it->IsSub()) {
            if (!it->GetSub().IsSetAuthors() || 
                !it->GetSub().GetAuthors().IsSetAffil() ||
                HasNoAffiliation(it->GetSub().GetAuthors().GetAffil())) {
                rval = true;
                break;
            } 
        }
    }
    return rval;
}


DISCREPANCY_CASE(MISSING_AFFIL, PUBDESC, eDisc | eOncaller | eFatal, "Missing affiliation")
{
    for (auto& pubdesc : context.GetPubdescs()) {
        if (IsCitSubMissingAffiliation(*pubdesc)) {
            m_Objs["[n] citsub[s] [is] missing affiliation"].Add(*context.PubdescObjRef(*pubdesc)).Fatal();
        }
    }
}


DISCREPANCY_SUMMARIZE(MISSING_AFFIL)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// CITSUBAFFIL_CONFLICT

#define ADD_TO_AFFIL_SUMMARY(Fieldname) \
    if(affil.IsSet##Fieldname() && !NStr::IsBlank(affil.Get##Fieldname())) { \
        if (!NStr::IsBlank(rval)) { \
            rval += ", "; \
        } \
        rval += affil.Get##Fieldname(); \
    }

string SummarizeAffiliation(const CAffil::C_Std& affil)
{
    string rval;
    ADD_TO_AFFIL_SUMMARY(Div)
    ADD_TO_AFFIL_SUMMARY(Affil)
    ADD_TO_AFFIL_SUMMARY(Street)
    ADD_TO_AFFIL_SUMMARY(City)
    ADD_TO_AFFIL_SUMMARY(Sub)
    if (affil.IsSetPostal_code() && !NStr::IsBlank(affil.GetPostal_code())) {
        if (!NStr::IsBlank(rval)) {
            rval += " ";
        }
        rval += affil.GetPostal_code();
    }
    ADD_TO_AFFIL_SUMMARY(Country)

    return rval;
}


string SummarizeAffiliation(const CAffil& affil)
{
    if (affil.IsStd()) {
        return SummarizeAffiliation(affil.GetStd());
    } else if (affil.IsStr()) {
        return affil.GetStr();
    } else {
        return kEmptyStr;
    }
}


const string kCitSubSummary = "Citsub affiliation conflicts found";
const string kSummaries = "summaries";


DISCREPANCY_CASE(CITSUBAFFIL_CONFLICT, AUTHORS, eDisc | eOncaller | eSmart | eFatal, "All Cit-subs should have identical affiliations")
{
    for (auto& authors : context.GetAuthors()) {
        const CPub* pub = context.AuthPub(authors);
        if (pub && !pub->IsSub()) {
            continue;
        }
        CRef<CDiscrepancyObject> repobj = context.AuthorsObjRef(*authors);
        if (authors->IsSetAffil()) {
            const CAffil& affil = authors->GetAffil();
            if (affil.IsStr()) {
                m_Objs["Affil"][affil.GetStr()].Add(*repobj);
                m_Objs["Div"][kEmptyStr].Add(*repobj);
                m_Objs["City"][kEmptyStr].Add(*repobj);
                m_Objs["Sub"][kEmptyStr].Add(*repobj);
                m_Objs["Country"][kEmptyStr].Add(*repobj);
                m_Objs["Street"][kEmptyStr].Add(*repobj);
                m_Objs["Postal_code"][kEmptyStr].Add(*repobj);
            }
            else if (affil.IsStd()) {
                m_Objs["Affil"][affil.GetStd().IsSetAffil() && !NStr::IsBlank(affil.GetStd().GetAffil()) ? affil.GetStd().GetAffil() : kEmptyStr].Add(*repobj);
                m_Objs["Div"][affil.GetStd().IsSetDiv() && !NStr::IsBlank(affil.GetStd().GetDiv()) ? affil.GetStd().GetDiv() : kEmptyStr].Add(*repobj);
                m_Objs["City"][affil.GetStd().IsSetCity() && !NStr::IsBlank(affil.GetStd().GetCity()) ? affil.GetStd().GetCity() : kEmptyStr].Add(*repobj);
                m_Objs["Sub"][affil.GetStd().IsSetSub() && !NStr::IsBlank(affil.GetStd().GetSub()) ? affil.GetStd().GetSub() : kEmptyStr].Add(*repobj);
                m_Objs["Country"][affil.GetStd().IsSetCountry() && !NStr::IsBlank(affil.GetStd().GetCountry()) ? affil.GetStd().GetCountry() : kEmptyStr].Add(*repobj);
                m_Objs["Street"][affil.GetStd().IsSetStreet() && !NStr::IsBlank(affil.GetStd().GetStreet()) ? affil.GetStd().GetStreet() : kEmptyStr].Add(*repobj);
                m_Objs["Postal_code"][affil.GetStd().IsSetPostal_code() && !NStr::IsBlank(affil.GetStd().GetPostal_code()) ? affil.GetStd().GetPostal_code() : kEmptyStr].Add(*repobj);
            }
            else {
                m_Objs["Affil"][kEmptyStr].Add(*repobj);
            }
            m_Objs[kSummaries][SummarizeAffiliation(affil)].Add(*repobj);
        }
        else {
            m_Objs[kSummaries][kEmptyStr].Add(*repobj);
        }
    }
}


DISCREPANCY_SUMMARIZE(CITSUBAFFIL_CONFLICT)
{
    CReportNode out;
    if (m_Objs.empty()) {
        out[kCitSubSummary]["No citsubs were found!"].Fatal();
    }
    if (m_Objs[kSummaries].GetMap().size() > 1) {
        for (const auto& it : m_Objs[kSummaries].GetMap()) {
            string two = NStr::IsBlank(it.first) ? "[*0*][n] Cit-sub[s] [has] no affiliation" : "[*1*][n] CitSub[s] [has] affiliation " + it.first;
            for (auto& robj : m_Objs[kSummaries][it.first].GetObjects()) {
                out[kCitSubSummary][two].Add(*robj, false);
            }
        }
    }
#define REPORT_CITSUBAFFIL_CONFLICT(order, field, alias) \
    if (m_Objs[#field].GetMap().size() > 1) {\
        for (const auto& it : m_Objs[#field].GetMap()) {\
            string two = "[n] affiliation[s] [has] "#alias" value '" + it.first + "'";\
            for (auto& robj : m_Objs[#field][it.first].GetObjects()) {\
                out[kCitSubSummary]["[*"#order"*]Affiliations have different values for "#alias][two].Ext().Add(*robj, false);\
            }\
        }\
    }
    REPORT_CITSUBAFFIL_CONFLICT(3, Affil, institution)
    REPORT_CITSUBAFFIL_CONFLICT(4, Div, department)
    REPORT_CITSUBAFFIL_CONFLICT(5, City, city)
    REPORT_CITSUBAFFIL_CONFLICT(6, Sub, state/province)
    REPORT_CITSUBAFFIL_CONFLICT(7, Country, country)
    REPORT_CITSUBAFFIL_CONFLICT(8, Street, street)
    REPORT_CITSUBAFFIL_CONFLICT(9, Postal_code, postal code)
    m_ReportItems = out.Export(*this)->GetSubitems();
}


// SUBMITBLOCK_CONFLICT

DISCREPANCY_CASE(SUBMITBLOCK_CONFLICT, SUBMIT, eDisc | eOncaller | eSmart, "Records should have identical submit-blocks")
{
    const CSubmit_block* sub = context.GetSubmit_block();
    if (sub) {
        stringstream ss;
        ss << MSerial_AsnBinary << *sub;
        m_Objs[ss.str()].Add(*context.SubmitBlockObjRef());
    }
}


DISCREPANCY_SUMMARIZE(SUBMITBLOCK_CONFLICT)
{
    if (m_Objs.GetMap().size() > 1) {
        CReportNode out;
        CReportNode& outout = out["SubmitBlock Conflicts"];
        size_t count = 0;
        for (auto& it : m_Objs.GetMap()) {
            CReportNode& node = outout["[*" + to_string(count++) + "*][n] record[s] [has] identical submit-blocks"];
            for (auto& obj : it.second->GetObjects()) {
                node.Add(*obj);
            }
        }
        m_ReportItems = out.Export(*this)->GetSubitems();
    }
}


// CONSORTIUM

DISCREPANCY_CASE(CONSORTIUM, AUTHORS, eOncaller, "Submitter blocks and publications have consortiums")
{
    static const string msg = "[n] publication[s]/submitter block[s] [has] consortium";
    for (auto& authors : context.GetAuthors()) {
        if (authors->IsSetNames() && authors->GetNames().IsStd()) {
            const CAuth_list::C_Names::TStd& names = authors->GetNames().GetStd();
            for (auto& auth : names) {
                if (auth->IsSetName() && auth->GetName().IsConsortium()) {
                    m_Objs[msg].Add(*context.AuthorsObjRef(*authors, true));
                }
            }
        }
    }
    const CPerson_id* pid = context.GetPerson_id();
    if (pid && pid->IsConsortium()) {
        m_Objs[msg].Add(*context.SubmitBlockObjRef(true));
    }
}


DISCREPANCY_SUMMARIZE(CONSORTIUM)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


static int RemoveConsortium(CAuth_list& authors)
{
    int n = 0;
    CAuth_list::C_Names::TStd& names = authors.SetNames().SetStd();
    CAuth_list::C_Names::TStd::iterator it = names.begin();
    while (it != names.end()) {
        if ((*it)->CanGetName() && (*it)->GetName().IsConsortium()) {
            names.erase(it++);
            n++;
        }
        else {
            ++it;
        }
    }
    return n;
}


DISCREPANCY_AUTOFIX(CONSORTIUM)
{
    CSerialObject* sobj = const_cast<CSerialObject*>(context.FindObject(*obj));
    CSeq_feat* feat = dynamic_cast<CSeq_feat*>(sobj);
    CSeqdesc* desc = dynamic_cast<CSeqdesc*>(sobj);
    CSubmit_block* subb = dynamic_cast<CSubmit_block*>(sobj);
    unsigned int n = 0;
    if (feat) {
        cout << "CONSORTIUM AUTOFIX: on seq_feat is not implemented\n";
    }
    if (desc && desc->IsPub() && desc->GetPub().CanGetPub() && desc->GetPub().GetPub().CanGet()) {
        CPub_equiv::Tdata& data = desc->SetPub().SetPub().Set();
        for (auto pub : data) {
            if (pub->IsSetAuthors()) {
                n += RemoveConsortium(pub->SetAuthors());
            }
        }
    }
    if (subb && subb->CanGetCit() && subb->GetCit().CanGetAuthors() && subb->GetCit().GetAuthors().CanGetNames()) {
        n += RemoveConsortium(subb->SetCit().SetAuthors());
        // we don't autofix in the cit_sub->contact
    }
    obj->SetFixed();
    return CRef<CAutofixReport>(n ? new CAutofixReport("CONSORTIUM: [n] Consortium[s] [is] removed", n) : nullptr);
}


// CHECK_AUTH_NAME

const string kMissingAuthorsName = "[n] pub[s] missing author\'s first or last name";

DISCREPANCY_CASE(CHECK_AUTH_NAME, AUTHORS, eDisc | eOncaller | eSubmitter | eSmart, "Missing authors or first/last author's names")
{
    //if (context.IsPubMed()) { -- need to rewrite context.IsPubMed()
    //    return;
    //}
    for (auto& authors : context.GetAuthors()) {
        if (authors->IsSetNames() && authors->GetNames().IsStd()) {
            const CAuth_list::C_Names::TStd& names = authors->GetNames().GetStd();
            if (names.empty()) {
                m_Objs[kMissingAuthorsName].Add(*context.AuthorsObjRef(*authors));
            }
            else {
                for (auto& auth : names) {
                    if (!auth->IsSetName() || (auth->GetName().IsName() &&
                        (!auth->GetName().GetName().CanGetFirst() || !auth->GetName().GetName().CanGetLast() || auth->GetName().GetName().GetFirst().empty() || auth->GetName().GetName().GetLast().empty()))) {
                        m_Objs[kMissingAuthorsName].Add(*context.AuthorsObjRef(*authors));
                        break;
                    }
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(CHECK_AUTH_NAME)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// CITSUB_AFFIL_DUP_TEXT

static const CCit_sub* GetCitSubFromPub(const CPub& pub)
{
    const CCit_sub* ret = nullptr;
    if (pub.IsEquiv() && pub.GetEquiv().IsSet()) {
        for (auto cur_pub : pub.GetEquiv().Get()) {
            ret = GetCitSubFromPub(*cur_pub);
            if (ret) {
                break;
            }
        }
    }
    else if (pub.IsSub()) {
        ret = &pub.GetSub();
    }
    return ret;
}


static bool AffilStreetEndsWith(const string& street, const string& tail)
{
    bool ret = false;
    if (street.size() > tail.size() && NStr::EndsWith(street, tail, NStr::eNocase)) {
        size_t delimiter_pos = street.size() - tail.size() - 1;
        if (ispunct(street[delimiter_pos]) || isspace(street[delimiter_pos])) {
            string university_of("University of");
            university_of += street[delimiter_pos] + tail;

            ret = !NStr::EndsWith(street, university_of, NStr::eNocase);
        }
    }
    return ret;
}


static bool AffilStreetContainsDup(const CAffil& affil)
{
    bool ret = false;
    if (affil.IsStd() && affil.GetStd().IsSetStreet() && !affil.GetStd().GetStreet().empty()) {
        const CAffil::C_Std& data = affil.GetStd();
        const string& street = data.GetStreet();
        if (data.IsSetCountry()) {
            ret = AffilStreetEndsWith(street, data.GetCountry());
        }
        if (!ret && data.IsSetPostal_code()) {
            ret = AffilStreetEndsWith(street, data.GetPostal_code());
        }
        if (!ret && data.IsSetSub()) {
            ret = AffilStreetEndsWith(street, data.GetSub());
        }
        if (!ret && data.IsSetCity()) {
            ret = AffilStreetEndsWith(street, data.GetCity());
        }
    }
    return ret;
}


DISCREPANCY_CASE(CITSUB_AFFIL_DUP_TEXT, PUBDESC, eOncaller, "Cit-sub affiliation street contains text from other affiliation fields")
{
    for (auto& pubdesc : context.GetPubdescs()) {
        if (pubdesc->IsSetPub()) {
            const CCit_sub* cit_sub = nullptr;
            for (auto& it : pubdesc->GetPub().Get()) {
                cit_sub = GetCitSubFromPub(*it);
                if (cit_sub) {
                    break;
                }
            }
            if (cit_sub && cit_sub->IsSetAuthors() && cit_sub->GetAuthors().IsSetAffil()) {
                const CAffil& affil = cit_sub->GetAuthors().GetAffil();
                if (AffilStreetContainsDup(affil)) {
                    m_Objs["[n] Cit-sub pubs have duplicate affil text"].Add(*context.PubdescObjRef(*pubdesc, true)); // , const_cast<CCit_sub*>(cit_sub)* / ));
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(CITSUB_AFFIL_DUP_TEXT)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


static bool RemoveAffilStreetEnd(string& street, const string& tail, bool country)
{
    bool ret = AffilStreetEndsWith(street, tail);
    if (ret) {
        size_t off = street.size() - tail.size();
        static const string kChina = "China";
        static const string kChinaPR = "P.R. China";
        if (country && NStr::EqualNocase(tail, kChina) && NStr::EndsWith(street, kChinaPR, NStr::eNocase)) {
            off = street.size() - kChinaPR.size();
        }
        string new_street = street.substr(0, off);
        street = NStr::Sanitize(new_street, NStr::fSS_NoTruncate_Begin | NStr::fSS_alnum);
    }
    return ret;
}


static bool RemoveAffilDup(CCit_sub* cit_sub)
{
    bool ret = false;
    if (cit_sub) {
        CAffil& affil = cit_sub->SetAuthors().SetAffil();
        CAffil::C_Std& data = affil.SetStd();
        string& street = data.SetStreet();
        if (data.IsSetCountry()) {
            ret = RemoveAffilStreetEnd(street, data.GetCountry(), true);
        }
        if (!ret && data.IsSetPostal_code()) {
            ret = RemoveAffilStreetEnd(street, data.GetPostal_code(), false);
        }
        if (!ret && data.IsSetSub()) {
            ret = RemoveAffilStreetEnd(street, data.GetSub(), false);
        }
        if (!ret && data.IsSetCity()) {
            ret = RemoveAffilStreetEnd(street, data.GetCity(), false);
        }
    }
    return ret;
}


DISCREPANCY_AUTOFIX(CITSUB_AFFIL_DUP_TEXT)
{
    CSerialObject* sobj = const_cast<CSerialObject*>(context.FindObject(*obj));
    CSeq_feat* feat = dynamic_cast<CSeq_feat*>(sobj);
    CSeqdesc* desc = dynamic_cast<CSeqdesc*>(sobj);

    unsigned int n = 0;
    if (feat) {
        cout << "CITSUB_AFFIL_DUP_TEXT AUTOFIX on seq_feat -- coming soon!\n";
    }
    if (desc && desc->IsPub() && desc->GetPub().CanGetPub() && desc->GetPub().GetPub().CanGet()) {
        CPub_equiv::Tdata& data = desc->SetPub().SetPub().Set();
        CCit_sub* cit_sub = nullptr;
        for (auto pub : data) {
            cit_sub = const_cast<CCit_sub*>(GetCitSubFromPub(*pub));
            if (cit_sub) {
                break;
            }
        }
        if (cit_sub && cit_sub->IsSetAuthors() && cit_sub->GetAuthors().IsSetAffil()) {
            const CAffil& affil = cit_sub->GetAuthors().GetAffil();
            if (AffilStreetContainsDup(affil) && RemoveAffilDup(cit_sub)) {
                obj->SetFixed();
                n++;
            }
        }
    }
    return CRef<CAutofixReport>(n ? new CAutofixReport("CITSUB_AFFIL_DUP_TEXT: [n] Cit-sub affiliation street duplication[s] [is] removed", n) : nullptr);
}


// USA_STATE

static pair<string, string> us_state_abbreviations[] = {
    { "AL", "Alabama" },
    { "AL", "Ala" },
    { "AK", "Alaska" },
    { "AK", "Alas" },
    { "AZ", "Arizona" },
    { "AZ", "Ariz" },
    { "AR", "Arkansas" },
    { "AR", "Ark" },
    { "CA", "California" },
    { "CA", "Calif" },
    { "CA", "Cali" },
    { "CA", "Cal" },
    { "CO", "Colorado" },
    { "CO", "Colo" },
    { "CO", "Col" },
    { "CT", "Connecticut" },
    { "CT", "Conn" },
    { "DE", "Delaware" },
    { "DE", "Del" },
    { "FL", "Florida" },
    { "FL", "Fla" },
    { "GA", "Georgia" },
    { "HI", "Hawaii" },
    { "ID", "Idaho" },
    { "ID", "Ida" },
    { "IL", "Illinois" },
    { "IL", "Ill" },
    { "IN", "Indiana" },
    { "IN", "Ind" },
    { "IA", "Iowa" },
    { "KS", "Kansas" },
    { "KS", "Kans" },
    { "KS", "Kan" },
    { "KY", "Kentucky" },
    { "KY", "Kent" },
    { "KY", "Ken" },
    { "LA", "Louisiana" },
    { "ME", "Maine" },
    { "MD", "Maryland" },
    { "MA", "Massachusetts" },
    { "MA", "Mass" },
    { "MI", "Michigan" },
    { "MI", "Mich" },
    { "MN", "Minnesota" },
    { "MN", "Minn" },
    { "MS", "Mississippi" },
    { "MS", "Miss" },
    { "MO", "Missouri" },
    { "MT", "Montana" },
    { "MT", "Mont" },
    { "NE", "Nebraska" },
    { "NE", "Nebr" },
    { "NE", "Neb" },
    { "NV", "Nevada" },
    { "NV", "Nev" },
    { "NH", "New Hampshire" },
    { "NJ", "New Jersey" },
    { "NM", "New Mexico" },
    { "NY", "New York" },
    { "NC", "North Carolina" },
    { "NC", "N Car" },
    { "ND", "North Dakota" },
    { "ND", "N Dak" },
    { "OH", "Ohio" },
    { "OK", "Oklahoma" },
    { "OK", "Okla" },
    { "OR", "Oregon" },
    { "OR", "Oreg" },
    { "OR", "Ore" },
    { "PA", "Pennsylvania" },
    { "PA", "Penna" },
    { "PA", "Penn" },
    { "PR", "Puerto Rico" },
    { "RI", "Rhode Island" },
    { "SC", "South Carolina" },
    { "SC", "S Car" },
    { "SD", "South Dakota" },
    { "SD", "S Dak" },
    { "TN", "Tennessee" },
    { "TN", "Tenn" },
    { "TX", "Texas" },
    { "TX", "Tex" },
    { "UT", "Utah" },
    { "VT", "Vermont" },
    { "VA", "Virginia" },
    { "VA", "Virg" },
    { "WA", "Washington" },
    { "WA", "Wash" },
    { "WV", "West Virginia" },
    { "WI", "Wisconsin" },
    { "WI", "Wisc" },
    { "WI", "Wis" },
    { "WY", "Wyoming" },
    { "WY", "Wyo" }
};

static size_t kNumOfAbbreviations = sizeof(us_state_abbreviations) / sizeof(us_state_abbreviations[0]);

static bool IsValidStateAbbreviation(const string& state)
{
    for (size_t i = 0; i < kNumOfAbbreviations; ++i) {
        if (state == us_state_abbreviations[i].first) {
            return true;
        }
    }
    return false;
}


DISCREPANCY_CASE(USA_STATE, PUBDESC, eDisc | eOncaller | eSmart, "For country USA, state should be present and abbreviated")
{
    for (auto& pubdesc : context.GetPubdescs()) {
        if (pubdesc->IsSetPub()) {
        }
        const CCit_sub* cit_sub = nullptr;
        for (auto& it : pubdesc->GetPub().Get()) {
            cit_sub = GetCitSubFromPub(*it);
            if (cit_sub) {
                break;
            }
        }
        if (cit_sub && cit_sub->IsSetAuthors() && cit_sub->GetAuthors().IsSetAffil()) {
            const CAffil& affil = cit_sub->GetAuthors().GetAffil();
            if (affil.IsStd() && affil.GetStd().IsSetCountry()) {
                const string& country = affil.GetStd().GetCountry();
                if (country == "USA") {
                    bool report = !affil.GetStd().IsSetSub();
                    if (!report) {
                        const string& state = affil.GetStd().GetSub();
                        report = !IsValidStateAbbreviation(state);
                    }
                    if (report) {
                        m_Objs["[n] cit-sub[s] [is] missing state abbreviations"].Add(*context.PubdescObjRef(*pubdesc, true)); // , const_cast<CAffil*>(&affil)* / ));
                    }
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(USA_STATE)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


static bool ReplaceStateAbbreviation(CAffil* affil)
{
    if (affil == nullptr) {
        return false;
    }
    if (affil->GetStd().IsSetSub()) {
        const string& state = affil->GetStd().GetSub();
        for (size_t i = 0; i < kNumOfAbbreviations; ++i) {
            if (NStr::EqualNocase(state, us_state_abbreviations[i].second) || NStr::EqualNocase(state, us_state_abbreviations[i].first)) {
                affil->SetStd().SetSub(us_state_abbreviations[i].first);
                return true;
            }
        }
    }
    return false;
}


DISCREPANCY_AUTOFIX(USA_STATE)
{
    CSerialObject* sobj = const_cast<CSerialObject*>(context.FindObject(*obj));
    CSeq_feat* feat = dynamic_cast<CSeq_feat*>(sobj);
    CSeqdesc* desc = dynamic_cast<CSeqdesc*>(sobj);

    unsigned int n = 0;
    if (feat) {
        cout << "USA_STATE AUTOFIX on seq_feat -- coming soon!\n";
    }
    if (desc && desc->IsPub() && desc->GetPub().CanGetPub() && desc->GetPub().GetPub().CanGet()) {
        CPub_equiv::Tdata& data = desc->SetPub().SetPub().Set();
        CCit_sub* cit_sub = nullptr;
        for (auto pub : data) {
            cit_sub = const_cast<CCit_sub*>(GetCitSubFromPub(*pub));
            if (cit_sub) {
                break;
            }
        }
        if (cit_sub && cit_sub->IsSetAuthors() && cit_sub->GetAuthors().IsSetAffil()) {
            CAffil& affil = const_cast<CAffil&>(cit_sub->GetAuthors().GetAffil());
            if (affil.IsStd() && affil.GetStd().IsSetCountry()) {
                const string& country = affil.GetStd().GetCountry();
                if (country == "USA") {
                    bool report = !affil.GetStd().IsSetSub();
                    if (!report) {
                        const string& state = affil.GetStd().GetSub();
                        report = !IsValidStateAbbreviation(state);
                    }
                    if (report && ReplaceStateAbbreviation(&affil)) {
                        obj->SetFixed();
                        n++;
                    }
                }
            }
        }
    }
    return CRef<CAutofixReport>(n ? new CAutofixReport("USA_STATE: [n] Cit-sub[s] [is] changed to contain a correct US state abbreviation", n) : nullptr);
}


// CHECK_AUTH_CAPS

static const string kIncorrectCap = "[n] pub[s] [has] incorrect author capitalization";

static bool IsCapNameCorrect(const string& name)
{
    static const set<string> kShortNames = {
        "del",
        "de",
        "da",
        "du",
        "dos",
        "la",
        "le",
        "van",
        "von",
        "der",
        "den",
        "di", };
    bool ret = true;

    if (!name.empty()) {
        bool need_cap = true;
        bool need_lower = false;
        bool found_lower = true;
        size_t len = name.size();
        for (size_t i = 0; i < len; ++i) {
            if (isalpha(name[i])) {
                if (need_cap) {
                    if (islower(name[i])) {
                        // check if it is a short name
                        string::const_iterator start = name.cbegin() + i;
                        string::const_iterator end = start + 1;
                        while (end != name.cend() && *end != ' ') {
                            ++end;
                        }
                        string short_name(start, end);
                        set<string>::const_iterator it = kShortNames.find(short_name);
                        if (it == kShortNames.cend()) {
                            ret = false;
                            break;
                        }
                        i += it->size() - 1;
                    }
                    need_cap = false;
                }
                else {

                    need_lower = true;
                    found_lower = islower(name[i]) != 0;
                }
            }
            else {
                need_cap = true;
            }
        }
        if (need_lower && !found_lower) {
            ret = false;
        }
    }
    return ret;
}


static bool IsCapInitialsCorrect(const string& initials)
{
    bool ret = true;
    if (!initials.empty()) {
        for (auto& cur : initials) {
            if (isalpha(cur) && islower(cur)) {
                ret = false;
                break;
            }
        }
    }
    return ret;
}


DISCREPANCY_CASE(CHECK_AUTH_CAPS, AUTHORS, eDisc | eOncaller | eSmart, "Check for correct capitalization in author names")
{
    for (auto& authors : context.GetAuthors()) {
        if (authors->IsSetNames() && authors->GetNames().IsStd()) {
            for (auto& auth : authors->GetNames().GetStd()) {
                if (auth->IsSetName() && auth->GetName().IsName()) {
                    const CName_std& name = auth->GetName().GetName();
                    bool correct = true;
                    if (name.IsSetLast()) {
                        correct = IsCapNameCorrect(name.GetLast());
                    }
                    if (correct && name.IsSetFirst()) {
                        correct = IsCapNameCorrect(name.GetFirst());
                    }
                    if (correct && name.IsSetInitials()) {
                        correct = IsCapInitialsCorrect(name.GetInitials());
                    }
                    if (!correct) {
                        m_Objs[kIncorrectCap].Add(*context.AuthorsObjRef(*authors, true));
                        break;
                    }
                }
            }
        }
    }
}


DISCREPANCY_SUMMARIZE(CHECK_AUTH_CAPS)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


static bool FixCapitalization(string& name, bool apostroph)
{
    bool ret = false;
    bool to_lower = false;
    for (auto& cur : name) {
        if (isalpha(cur)) {
            if (to_lower && isupper(cur)) {
                cur = tolower(cur);
                ret = true;
            }
            if (!to_lower) {
                if (islower(cur)) {
                    cur = toupper(cur);
                    ret = true;
                }
                to_lower = true;
            }
        }
        else if (apostroph && cur == '\'') {
            to_lower = false;
        }
        else if (cur == ' ' || cur == '-') {
            to_lower = false;
        }
    }
    return ret;
}


static bool FixCapitalization(CAuth_list* auth_list)
{
    bool ret = false;
    if (! auth_list) {
        return false;
    }
    for (auto auth : auth_list->SetNames().SetStd()) {
        if (auth->GetName().IsName()) {
            CName_std& name = auth->SetName().SetName();
            if (name.IsSetFirst() && !IsCapNameCorrect(name.GetFirst()) && FixCapitalization(name.SetFirst(), false)) {
                ret = true;
            }
            if (name.IsSetLast() && !IsCapNameCorrect(name.GetLast()) && FixCapitalization(name.SetLast(), true)) {
                ret = true;
            }
            if (name.IsSetInitials() && !IsCapInitialsCorrect(name.GetInitials())) {
                for (auto& cur : name.SetInitials()) {
                    if (isalpha(cur) && islower(cur)) {
                        cur = toupper(cur);
                        ret = true;
                    }
                }
            }
        }
    }
    return ret;
}


DISCREPANCY_AUTOFIX(CHECK_AUTH_CAPS)
{
    CSerialObject* sobj = const_cast<CSerialObject*>(context.FindObject(*obj));
    CSeq_feat* feat = dynamic_cast<CSeq_feat*>(sobj);
    CSeqdesc* desc = dynamic_cast<CSeqdesc*>(sobj);
    CSubmit_block* subb = dynamic_cast<CSubmit_block*>(sobj);

    unsigned int n = 0;
    if (feat) {
        cout << "CHECK_AUTH_CAPS AUTOFIX on seq_feat -- coming soon!\n";
    }
    if (desc) {
        for (auto& authors : desc->SetPub().SetPub().Set()) {
            if (authors->IsSetAuthors()) {
                if (FixCapitalization(&authors->SetAuthors())) {
                    obj->SetFixed();
                    n++;
                }
            }
        }
    }
    if (subb) {
        if (subb->SetCit().IsSetAuthors()) {
            if (FixCapitalization(&subb->SetCit().SetAuthors())) {
                obj->SetFixed();
                n++;
            }
        }
    }
    return CRef<CAutofixReport>(n ? new CAutofixReport("CHECK_AUTH_CAPS: capitalization of [n] author[s] is fixed", n) : nullptr);
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
