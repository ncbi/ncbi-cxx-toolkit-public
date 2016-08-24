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
                ITERATE (CAuth_list::C_Names::TStd, it, auth_list.GetNames().GetStd()) {
                    string a = GetAuthorString(**it);
                    if (!NStr::IsBlank(a)) {
                        if (!NStr::IsBlank(authors)) {
                            authors += ", ";
                        }
                        authors += a;
                    }
                }
                break;
            case CAuth_list::C_Names::e_Ml:
                ITERATE (CAuth_list::C_Names::TMl, it, auth_list.GetNames().GetMl()) {
                    if (!NStr::IsBlank(*it)) {
                        if (!NStr::IsBlank(authors)) {
                            authors += ", ";
                        }
                        authors += *it;
                    }
                }
                break;
            case CAuth_list::C_Names::e_Str:
                ITERATE (CAuth_list::C_Names::TStr, it, auth_list.GetNames().GetStr()) {
                    if (!NStr::IsBlank(*it)) {
                        if (!NStr::IsBlank(authors)) {
                            authors += ", ";
                        }
                        authors += *it;
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
        default:
            break;
    }
}


void GetPubTitleAndAuthors(const CPubdesc& pubdesc, string& title, string& authors)
{
    title = kEmptyStr;
    authors = kEmptyStr;
    if (pubdesc.IsSetPub()) {
        ITERATE (CPubdesc::TPub::Tdata, it, pubdesc.GetPub().Get()) {
            GetPubTitleAndAuthors(**it, title, authors);
            if (!NStr::IsBlank(title)) {
                break;
            }
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(TITLE_AUTHOR_CONFLICT, CSeqdesc, eDisc | eOncaller | eSmart, "Publications with the same titles should have the same authors")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsPub()) {
        return;
    }
    string title;
    string authors;
    GetPubTitleAndAuthors(obj.GetPub(), title, authors);
    if (NStr::IsBlank(title)) {
        return;
    }
    // We ask to keep the reference because we do need the actual object to stick around so we can deal with them later.
    CRef<CDiscrepancyObject> this_disc_obj(context.NewDiscObj(CConstRef<CSeqdesc>(&obj), eKeepRef));

    m_Objs["titles"][title][authors].Add(*this_disc_obj);
}


static const char* kTitleAuthorConflict = "Publication Title/Author Inconsistencies";

//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(TITLE_AUTHOR_CONFLICT)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
    CReportNode::TNodeMap::iterator it = m_Objs["titles"].GetMap().begin();
    while (it != m_Objs["titles"].GetMap().end()) {
        // do all pubs have the same authors?
        if (m_Objs["titles"][it->first].GetMap().size() > 1) {
            string top = "[n] articles have title '" + it->first + "' but do not have the same author list";
            CReportNode::TNodeMap::iterator it2 = m_Objs["titles"][it->first].GetMap().begin();
            while (it2 != m_Objs["titles"][it->first].GetMap().end()) {
                NON_CONST_ITERATE (TReportObjectList, robj, m_Objs["titles"][it->first][it2->first].GetObjects()) {
                    const CDiscrepancyObject* other_disc_obj = dynamic_cast<CDiscrepancyObject*>(robj->GetNCPointer());
                    CConstRef<CSeqdesc> pub_desc(dynamic_cast<const CSeqdesc*>(other_disc_obj->GetObject().GetPointer()));
                    m_Objs[kTitleAuthorConflict][top]["[n] article[s] [has] title '" + it->first + "' and author list '" + it2->first + "'"].Add(*context.NewDiscObj(pub_desc), false).Fatal();
                }
                ++it2;
            }
        }
        ++it;
    }
    m_Objs.GetMap().erase("titles");
    if (m_Objs.Exist(kTitleAuthorConflict) && m_Objs[kTitleAuthorConflict].GetMap().size() == 1) {
        m_ReportItems = m_Objs.GetMap().begin()->second->Export(*this)->GetSubitems();
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
    bool rval = false;
    ITERATE (CPubdesc::TPub::Tdata, it, pubdesc.GetPub().Get()) {
        if (IsPubUnpublished(**it)) {
            string title = kEmptyStr;
            string authors = kEmptyStr;
            GetPubTitleAndAuthors(**it, title, authors);
            if (NStr::IsBlank(title) || NStr::EqualNocase(title, "Direct Submission")) {
                rval = true;
                break;
            }
        }
    }
    return rval;
}


const string kUnpubPubWithoutTitle = "[n] unpublished pub[s] [has] no title";
//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(UNPUB_PUB_WITHOUT_TITLE, CPubdesc, eDisc | eOncaller | eSubmitter | eSmart, "Unpublished pubs should have titles")
//  ----------------------------------------------------------------------------
{
    if (HasUnpubWithoutTitle(obj)) {
        if (context.GetCurrentSeqdesc() != NULL) {
            m_Objs[kUnpubPubWithoutTitle].Add(*context.NewDiscObj(context.GetCurrentSeqdesc()), false).Fatal();
        } else if (context.GetCurrentSeq_feat() != NULL) {
            m_Objs[kUnpubPubWithoutTitle].Add(*context.NewDiscObj(context.GetCurrentSeq_feat()), false).Fatal();
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(UNPUB_PUB_WITHOUT_TITLE)
//  ----------------------------------------------------------------------------
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
    ITERATE (CPubdesc::TPub::Tdata, it, pubdesc.GetPub().Get()) {
        if ((*it)->IsSub()) {
            if (!(*it)->GetSub().IsSetAuthors() || 
                !(*it)->GetSub().GetAuthors().IsSetAffil() ||
                HasNoAffiliation((*it)->GetSub().GetAuthors().GetAffil())) {
                rval = true;
                break;
            } 
        }
    }
    return rval;
}


const string kMissingAffil = "[n] citsub[s] [is] missing affiliation";
//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(MISSING_AFFIL, CPubdesc, eDisc | eOncaller, "Missing affiliation")
//  ----------------------------------------------------------------------------
{
    if (IsCitSubMissingAffiliation(obj)) {
        if (context.GetCurrentSeqdesc() != NULL) {
            m_Objs[kMissingAffil].Add(*context.NewDiscObj(context.GetCurrentSeqdesc()), false).Fatal();
        } else if (context.GetCurrentSeq_feat() != NULL) {
            m_Objs[kMissingAffil].Add(*context.NewDiscObj(context.GetCurrentSeq_feat()), false).Fatal();
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(MISSING_AFFIL)
//  ----------------------------------------------------------------------------
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


DISCREPANCY_CASE(CITSUBAFFIL_CONFLICT, CAuth_list, eDisc | eOncaller | eSmart, "All Cit-subs should have identical affiliations")
{
    CConstRef<CPub> pub = context.GetCurrentPub();
    if (pub && !pub->IsSub()) {
        return;
    }
    CRef<CDiscrepancyObject> repobj = context.NewFeatOrDescOrSubmitBlockObj();
    if (obj.IsSetAffil()) {
        const CAffil& affil = obj.GetAffil();
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


DISCREPANCY_SUMMARIZE(CITSUBAFFIL_CONFLICT)
{
    CReportNode out;
    if (m_Objs.empty()) {
        out[kCitSubSummary]["No citsubs were found!"].Fatal();
    }
    if (m_Objs[kSummaries].GetMap().size() > 1) {
        ITERATE (CReportNode::TNodeMap, it, m_Objs[kSummaries].GetMap()) {
            string two = NStr::IsBlank(it->first) ? "[*0*][n] Cit-sub[s] [has] no affiliation" : "[*1*][n] CitSub[s] [has] affiliation " + it->first;
            NON_CONST_ITERATE (TReportObjectList, robj, m_Objs[kSummaries][it->first].GetObjects()) {
                out[kCitSubSummary][two].Add(**robj, false);
            }
        }
    }
#define REPORT_CITSUBAFFIL_CONFLICT(order, field, alias) \
    if (m_Objs[#field].GetMap().size() > 1) {\
        ITERATE (CReportNode::TNodeMap, it, m_Objs[#field].GetMap()) {\
            string two = "[n] affiliation[s] [has] "#alias" value '" + it->first + "'";\
            NON_CONST_ITERATE (TReportObjectList, robj, m_Objs[#field][it->first].GetObjects()) {\
                out[kCitSubSummary]["[*"#order"*]Affiliations have different values for "#alias][two].Ext().Add(**robj, false);\
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

#define COMPARE_STRING_FIELD(Fieldname) \
    if (!c1.IsSet##Fieldname() && c2.IsSet##Fieldname()) { \
        return -1;\
    } else if (c1.IsSet##Fieldname() && !c2.IsSet##Fieldname()) { \
        return 1; \
    } else if (c1.IsSet##Fieldname() && c2.IsSet##Fieldname()) { \
        int cmp = NStr::Compare(c1.Get##Fieldname(), c2.Get##Fieldname()); \
        if (cmp < 0) { \
            return -1; \
        } else if (cmp > 0) { \
            return 1; \
        } \
    }


template <typename Type>
int CompareByAsnString(const Type& c1, const Type& c2)
{
    CNcbiOstrstream ostr;
    ostr << MSerial_AsnText << c1;
    string l1 = CNcbiOstrstreamToString(ostr);

    ostr << MSerial_AsnText << c2;
    string l2 = CNcbiOstrstreamToString(ostr);

    return NStr::Compare(l1, l2);
}


#define FIELD_SET_COMPARE(Fieldname) \
    if (!c1.IsSet##Fieldname() && c2.IsSet##Fieldname()) { \
        return -1;\
            } else if (c1.IsSet##Fieldname() && !c2.IsSet##Fieldname()) { \
        return 1; \
    } else if (c1.IsSet##Fieldname() && c2.IsSet##Fieldname()) { \
        int cmp = CompareByAsnString(c1, c2); \
        if (cmp != 0) { \
            return cmp; \
        } \
    } 

#define FIELD_COMPARE_VAL(Fieldname) \
    if (!c1.IsSet##Fieldname() && c2.IsSet##Fieldname()) { \
        return -1;\
    } else if (c1.IsSet##Fieldname() && !c2.IsSet##Fieldname()) { \
        return 1; \
    } else if (c1.IsSet##Fieldname() && c2.IsSet##Fieldname()) { \
        if (c1.Get##Fieldname() < c2.Get##Fieldname()) { \
            return -1; \
        } else if (c1.Get##Fieldname() > c2.Get##Fieldname()) { \
            return 1; \
        } \
    } 

int s_CitSubCompareWithoutDate(const CCit_sub& c1, const CCit_sub& c2)
{
    int cmp = 0;

    FIELD_SET_COMPARE(Authors) 
    FIELD_SET_COMPARE(Imp) 
    FIELD_COMPARE_VAL(Medium)
    COMPARE_STRING_FIELD(Descr)

    return 0;
}


int s_SubmitBlockCompare(const CSubmit_block& c1, const CSubmit_block& c2)
{
    FIELD_SET_COMPARE(Contact)
    if (!c1.IsSetCit() && c2.IsSetCit()) {
        return -1;
    } else if (c1.IsSetCit() && !c2.IsSetCit()) {
        return 1;
    } else if (c1.IsSetCit() && c2.IsSetCit()) {
        int tmp = s_CitSubCompareWithoutDate(c1.GetCit(), c2.GetCit());
        if (tmp != 0) {
            return tmp;
        }
    }
    bool hup1 = (c1.IsSetHup() && c1.GetHup());
    bool hup2 = (c2.IsSetHup() && c2.GetHup());
    if (!hup1 && hup2) {
        return -1;
    } else if (hup1 && !hup2) {
        return 1;
    }
    FIELD_SET_COMPARE(Reldate)
    FIELD_COMPARE_VAL(Subtype)
    COMPARE_STRING_FIELD(Tool)
    COMPARE_STRING_FIELD(User_tag)
    COMPARE_STRING_FIELD(Comment)
    return 0;
}

static bool s_SortSubmitBlock(CRef<CSubmit_block> sb1, CRef<CSubmit_block> sb2)
{
    if (s_SubmitBlockCompare(*sb1, *sb2) < 0) {
        return true;
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(SUBMITBLOCK_CONFLICT, CSubmit_block, eDisc | eOncaller | eSmart, "Records should have identical submit-blocks")
//  ----------------------------------------------------------------------------
{
    // We ask to keep the reference because we do need the actual object to stick around so we can deal with them later.
//    CRef<CDiscrepancyObject> this_disc_obj(context.NewDiscObj(CConstRef<CSubmit_block>(&obj), eKeepRef));
//    m_Objs["blocks"].Add(*this_disc_obj, false);
}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(SUBMITBLOCK_CONFLICT)
//  ----------------------------------------------------------------------------
{
    if (m_Objs.empty()) {
        return;
    }
/*/
    vector < CConstRef<CSubmit_block> > blocks;
    CReportNode::TNodeMap::iterator it = m_Objs["blocks"].GetMap().begin();
    while (it != m_Objs["blocks"].GetMap().end()) {
        NON_CONST_ITERATE (TReportObjectList, robj, m_Objs["blocks"].GetObjects())
        {
            const CDiscrepancyObject* other_disc_obj = dynamic_cast<CDiscrepancyObject*>(robj->GetNCPointer());
            CConstRef<CSubmit_block> obj(dynamic_cast<const CSubmit_block*>(other_disc_obj->GetObject().GetPointer()));
            blocks.push_back(obj);
        }
    }
    if (blocks.size() < 2) {
        return;
    }
    stable_sort(blocks.begin(), blocks.end(), s_SortSubmitBlock);
    size_t group_num = 1;
    string label_start = "[n] records have identical submit-blocks (";
    string label = label_start + NStr::NumericToString(group_num) + ")";
    vector < CConstRef<CSubmit_block> >::iterator it1 = blocks.begin();
//    m_Objs[label].Add(*context.NewDiscObj(*it1), false);
    vector < CConstRef<CSubmit_block> >::iterator it2 = blocks.begin();
    ++it2;
    while (it2 != blocks.end()) {
        if (s_SubmitBlockCompare(**it1, **it2) != 0) {
            group_num++;
            label = label_start + NStr::NumericToString(group_num) + ")";
        }
//        m_Objs["[n] records have identical submit-blocks (" + label + ")"].Add(*context.NewDiscObj(*it2), false);
        ++it1;
        ++it2;
    }
    m_Objs.GetMap().erase("blocks");
    if (m_Objs.GetMap().size() > 1) {
        m_ReportItems = m_Objs.Export(*this)->GetSubitems();
    }
/*/
}


// CONSORTIUM

DISCREPANCY_CASE(CONSORTIUM, CPerson_id, eOncaller, "Submitter blocks and publications have consortiums")
{
    if (obj.IsConsortium() && !context.GetCurrentSeq_feat() && !context.GetCurrentSeqdesc()) {
        m_Objs["[n] publication[s]/submitter block[s] [has] consortium"].Add(*context.NewFeatOrDescOrSubmitBlockObj());
    }
}


DISCREPANCY_SUMMARIZE(CONSORTIUM)
{
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// CHECK_AUTH_NAME

const string kMissingAuthorsName = "[n] pub[s] missing author\'s first or last name";

DISCREPANCY_CASE(CHECK_AUTH_NAME, CAuth_list, eDisc | eOncaller | eSubmitter | eSmart, "Missing authors or first/last author's names")
{
    if (context.IsPubMed()) {
        return;
    }
    if (obj.IsSetNames() && obj.GetNames().IsStd()) {
        const CAuth_list::C_Names::TStd& names = obj.GetNames().GetStd();
        if (names.empty()) {
            m_Objs[kMissingAuthorsName].Add(*context.NewFeatOrDescOrSubmitBlockObj());
        }
        else {
            ITERATE (CAuth_list::C_Names::TStd, auth, obj.GetNames().GetStd()) {
                if (!(*auth)->IsSetName() || !(*auth)->GetName().IsName()
                        || !(*auth)->GetName().GetName().CanGetFirst() || !(*auth)->GetName().GetName().CanGetLast()
                        || (*auth)->GetName().GetName().GetFirst().empty() || (*auth)->GetName().GetName().GetLast().empty()) {
                    m_Objs[kMissingAuthorsName].Add(*context.NewFeatOrDescOrSubmitBlockObj());
                    break;
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

static void AddParentObject(CReportNode& objs, CDiscrepancyContext& context, const string& category, EKeepRef keepref, bool autofix, CObject* more)
{
    CConstRef<CSeq_feat> feat = context.GetCurrentSeq_feat();
    CConstRef<CSeqdesc> desc = context.GetCurrentSeqdesc();
    if (feat) {
        objs[category].Add(*context.NewDiscObj(feat, keepref, autofix, more));
    }
    else if (desc) {
        objs[category].Add(*context.NewDiscObj(desc, keepref, autofix, more));
    }
    else {  // C Toolkit does not test submit blocks; C++ Toolkit - does!
        objs[category].Add(*context.NewSubmitBlockObj(keepref, autofix, more));
    }
}

const string kCitPubDupText = "[n] Cit-sub pubs have duplicate affil text";

static const CCit_sub* GetCitSubFromPub(const CPub& pub)
{
    const CCit_sub* ret = nullptr;

    if (pub.IsEquiv() && pub.GetEquiv().IsSet()) {
        ITERATE(CPub_equiv::Tdata, cur_pub, pub.GetEquiv().Get()) {
            ret = GetCitSubFromPub(**cur_pub);
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

DISCREPANCY_CASE(CITSUB_AFFIL_DUP_TEXT, CPubdesc, eOncaller, "Cit-sub affiliation street contains text from other affiliation fields")
{
    if (!obj.IsSetPub()) {
        return;
    }

    const CCit_sub* cit_sub = nullptr;
    ITERATE(CPubdesc::TPub::Tdata, it, obj.GetPub().Get()) {
        cit_sub = GetCitSubFromPub(**it);
        if (cit_sub) {
            break;
        }
    }

    if (cit_sub && cit_sub->IsSetAuthors() && cit_sub->GetAuthors().IsSetAffil()) {

        const CAffil& affil = cit_sub->GetAuthors().GetAffil();
        if (AffilStreetContainsDup(affil)) {
            AddParentObject(m_Objs, context, kCitPubDupText, eKeepRef, true, const_cast<CCit_sub*>(cit_sub));
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
    TReportObjectList list = item->GetDetails();
    unsigned int n = 0;
    NON_CONST_ITERATE(TReportObjectList, it, list) {

        const CObject* obj = dynamic_cast<CDiscrepancyObject*>(it->GetNCPointer())->GetMoreInfo();
        if (obj) {
            CCit_sub* cit_sub = dynamic_cast<CCit_sub*>(const_cast<CObject*>(obj));
            if (RemoveAffilDup(cit_sub)) {
                ++n;
            }
        }
    }

    return CRef<CAutofixReport>(n ? new CAutofixReport("CITSUB_AFFIL_DUP_TEXT: [n] Cit-sub affiliation street duplication[s] [is] removed", n) : 0);
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

static const string kMissingState = "[n] cit-sub[s] [is] missing state abbreviations";

DISCREPANCY_CASE(USA_STATE, CPubdesc, eDisc | eOncaller | eSmart, "For country USA, state should be present and abbreviated")
{
    if (!obj.IsSetPub()) {
        return;
    }

    const CCit_sub* cit_sub = nullptr;
    ITERATE(CPubdesc::TPub::Tdata, it, obj.GetPub().Get()) {
        cit_sub = GetCitSubFromPub(**it);
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
                    AddParentObject(m_Objs, context, kMissingState, eKeepRef, true, const_cast<CAffil*>(&affil));
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
    TReportObjectList list = item->GetDetails();
    unsigned int n = 0;
    NON_CONST_ITERATE(TReportObjectList, it, list) {

        const CObject* obj = dynamic_cast<CDiscrepancyObject*>(it->GetNCPointer())->GetMoreInfo();
        if (obj) {
            CAffil* affil = dynamic_cast<CAffil*>(const_cast<CObject*>(obj));
            if (ReplaceStateAbbreviation(affil)) {
                ++n;
            }
        }
    }

    return CRef<CAutofixReport>(n ? new CAutofixReport("USA_STATE: [n] Cit-sub[s] [is] changed to contain a correct US state abbreviation", n) : 0);
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
                        string::const_iterator start = name.begin() + i;
                        string::const_iterator end = start + 1;
                        while (end != name.end() && *end != ' ') {
                            ++end;
                        }

                        string short_name(start, end);
                        set<string>::const_iterator it = kShortNames.find(short_name);
                        if (it == kShortNames.end()) {
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

        ITERATE(string, cur, initials) {
            if (isalpha(*cur) && islower(*cur)) {
                ret = false;
                break;
            }
        }
    }

    return ret;
}

DISCREPANCY_CASE(CHECK_AUTH_CAPS, CAuth_list, eDisc | eOncaller | eSmart, "Check for correct capitalization in author names")
{
    if (obj.IsSetNames() && obj.GetNames().IsStd()) {

        ITERATE(CAuth_list::C_Names::TStd, auth, obj.GetNames().GetStd()) {

            if ((*auth)->IsSetName() && (*auth)->GetName().IsName()) {

                const CName_std& name = (*auth)->GetName().GetName();

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

                    m_Objs[kIncorrectCap].Add(*context.NewFeatOrDescOrSubmitBlockObj(eNoRef, true, const_cast<CAuth_list*>(&obj)));
                    break;
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

    NON_CONST_ITERATE(string, cur, name) {
        if (isalpha(*cur)) {
            if (to_lower && isupper(*cur)) {
                *cur = tolower(*cur);
                ret = true;
            }

            if (!to_lower) {

                if (islower(*cur)) {
                    *cur = toupper(*cur);
                    ret = true;
                }
                to_lower = true;
            }
        }
        else if (apostroph && *cur == '\'') {
            to_lower = false;
        }
    }

    return ret;
}

static bool FixCapitalization(CAuth_list* auth_list)
{
    bool ret = false;
    
    NON_CONST_ITERATE(CAuth_list::C_Names::TStd, auth, auth_list->SetNames().SetStd()) {

        CName_std& name = (*auth)->SetName().SetName();

        if (name.IsSetFirst() && FixCapitalization(name.SetFirst(), false)) {
            ret = true;
        }

        if (name.IsSetLast() && FixCapitalization(name.SetLast(), true)) {
            ret = true;
        }

        if (name.IsSetInitials()) {
            NON_CONST_ITERATE(string, cur, name.SetInitials()) {
                if (isalpha(*cur) && islower(*cur)) {
                    *cur = toupper(*cur);
                    ret = true;
                }
            }
        }
    }
            
    return ret;
}

DISCREPANCY_AUTOFIX(CHECK_AUTH_CAPS)
{
    TReportObjectList list = item->GetDetails();
    unsigned int n = 0;
    NON_CONST_ITERATE(TReportObjectList, it, list) {

        const CObject* obj = dynamic_cast<CDiscrepancyObject*>(it->GetNCPointer())->GetMoreInfo();
        if (obj) {
            CAuth_list* auth_list = dynamic_cast<CAuth_list*>(const_cast<CObject*>(obj));
            if (FixCapitalization(auth_list)) {
                ++n;
            }
        }
    }

    return CRef<CAutofixReport>(n ? new CAutofixReport("CHECK_AUTH_CAPS: capitalization of [n] author[s] is fixed", n) : 0);
}


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
