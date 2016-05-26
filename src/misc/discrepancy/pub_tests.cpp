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
                ITERATE(CAuth_list::C_Names::TStd, it, auth_list.GetNames().GetStd()) {
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
                ITERATE(CAuth_list::C_Names::TMl, it, auth_list.GetNames().GetMl()) {
                    if (!NStr::IsBlank(*it)) {
                        if (!NStr::IsBlank(authors)) {
                            authors += ", ";
                        }
                        authors += *it;
                    }
                }
                break;
            case CAuth_list::C_Names::e_Str:
                ITERATE(CAuth_list::C_Names::TStr, it, auth_list.GetNames().GetStr()) {
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
        ITERATE(CPubdesc::TPub::Tdata, it, pubdesc.GetPub().Get()) {
            GetPubTitleAndAuthors(**it, title, authors);
            if (!NStr::IsBlank(title)) {
                break;
            }
        }
    }
}


//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(TITLE_AUTHOR_CONFLICT, CSeqdesc, eDisc | eOncaller, "Publications with the same titles should have the same authors")
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
                NON_CONST_ITERATE(TReportObjectList, robj, m_Objs["titles"][it->first][it2->first].GetObjects())
                {
                    const CDiscrepancyObject* other_disc_obj = dynamic_cast<CDiscrepancyObject*>(robj->GetNCPointer());
                    CConstRef<CSeqdesc> pub_desc(dynamic_cast<const CSeqdesc*>(other_disc_obj->GetObject().GetPointer()));

                    m_Objs[top]["[n] article[s] [has] title '" + it->first + "' and author list '" + it2->first + "'"].Add(*context.NewDiscObj(pub_desc), false).Fatal();
                }
                ++it2;
            }
        }
        ++it;
    }
    m_Objs.GetMap().erase("titles");

    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
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
    ITERATE(CPubdesc::TPub::Tdata, it, pubdesc.GetPub().Get()) {
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
DISCREPANCY_CASE(UNPUB_PUB_WITHOUT_TITLE, CPubdesc, eDisc | eOncaller, "Unpublished pubs should have titles")
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
    if (m_Objs.empty()) {
        return;
    }
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
    ITERATE(CPubdesc::TPub::Tdata, it, pubdesc.GetPub().Get()) {
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
    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
}


// CITSUBAFFIL_CONFLICT

static void AddAffilFieldObject(CDiscrepancyContext& context, const string& field_name, const string& field_value, CReportNode& objs)
{
    if (context.GetCurrentSeqdesc() != NULL) {
        // We ask to keep the reference because we do need the actual object to stick around so we can deal with them later.
        CRef<CDiscrepancyObject> this_disc_obj(context.NewDiscObj(context.GetCurrentSeqdesc(), eKeepRef));
        objs[field_name][field_value].Add(*this_disc_obj, false);
    } else if (context.GetCurrentSeq_feat() != NULL) {
        // We ask to keep the reference because we do need the actual object to stick around so we can deal with them later.
        CRef<CDiscrepancyObject> this_disc_obj(context.NewDiscObj(context.GetCurrentSeq_feat(), eKeepRef));
        objs[field_name][field_value].Add(*this_disc_obj, false);
    }
}


#define ADD_ONE_AFFIL_FIELD(Fieldname) \
    if(affil.GetStd().IsSet##Fieldname() && !NStr::IsBlank(affil.GetStd().Get##Fieldname())) { \
        AddAffilFieldObject(context, #Fieldname, affil.GetStd().Get##Fieldname(), objs); \
    } else { \
        AddAffilFieldObject(context, #Fieldname, kEmptyStr, objs); \
    }

void AddAffilFields(CDiscrepancyContext& context, const CAffil& affil, CReportNode& objs)
{
    if (affil.IsStr()) {
        AddAffilFieldObject(context, "Affil", affil.GetStr(), objs);
        // add blanks for remaining fields
        AddAffilFieldObject(context, "Div", kEmptyStr, objs);
        AddAffilFieldObject(context, "City", kEmptyStr, objs);
        AddAffilFieldObject(context, "Sub", kEmptyStr, objs);
        AddAffilFieldObject(context, "Country", kEmptyStr, objs);
        AddAffilFieldObject(context, "Street", kEmptyStr, objs);
        AddAffilFieldObject(context, "Postal_code", kEmptyStr, objs);
    } else if (affil.IsStd()) {
        ADD_ONE_AFFIL_FIELD(Affil)
        ADD_ONE_AFFIL_FIELD(Div)
        ADD_ONE_AFFIL_FIELD(City)
        ADD_ONE_AFFIL_FIELD(Sub)
        ADD_ONE_AFFIL_FIELD(Country)
        ADD_ONE_AFFIL_FIELD(Street)
        ADD_ONE_AFFIL_FIELD(Postal_code)
    } else {
        // add blanks for all fields
        AddAffilFieldObject(context, "Affil", kEmptyStr, objs);
    }
}


#define ADD_TO_AFFIL_SUMMARY(Fieldname) \
    if(affil.IsSet##Fieldname() && !NStr::IsBlank(affil.Get##Fieldname())) { \
        if (!NStr::IsBlank(rval)) { \
            rval += ", "; \
        } \
        rval += affil.Get##Fieldname(); \
    }

string SummarizeAffiliation(const CAffil::C_Std& affil)
{
    string rval = kEmptyStr;
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

//  ----------------------------------------------------------------------------
DISCREPANCY_CASE(CITSUBAFFIL_CONFLICT, CPubdesc, eDisc | eOncaller, "All Cit-subs should have identical affiliations")
//  ----------------------------------------------------------------------------
{
    if (!obj.IsSetPub()) {
        return;
    }
    ITERATE(CPubdesc::TPub::Tdata, it, obj.GetPub().Get()) {
        if ((*it)->IsSub()) {
            if (!(*it)->GetSub().IsSetAuthors() || !(*it)->GetSub().GetAuthors().IsSetAffil()) {
                AddAffilFieldObject(context, kSummaries, kEmptyStr, m_Objs);
            } else {
                AddAffilFields(context, (*it)->GetSub().GetAuthors().GetAffil(), m_Objs);
                AddAffilFieldObject(context, 
                                    kSummaries,
                                    SummarizeAffiliation((*it)->GetSub().GetAuthors().GetAffil()), 
                                    m_Objs);
            }
        }
    }
}


static void SummarizeAffilField
(CReportNode& objs,
 CDiscrepancyContext& context, 
 const string& field_name, 
 const string& alias,
 const string leading_spaces)
{
    if (objs[field_name].GetMap().size() > 1) {
        string one = leading_spaces + "Affiliations have different values for " + alias;
        CReportNode::TNodeMap::iterator it = objs[field_name].GetMap().begin();
        while (it != objs[field_name].GetMap().end()) {
            string two = "[n] affiliation[s] [has] " + alias + " value '" + it->first + "'";
            NON_CONST_ITERATE(TReportObjectList, robj, objs[field_name][it->first].GetObjects())
            {
                const CDiscrepancyObject* other_disc_obj = dynamic_cast<CDiscrepancyObject*>(robj->GetNCPointer());
                CConstRef<CSeqdesc> pub_desc(dynamic_cast<const CSeqdesc*>(other_disc_obj->GetObject().GetPointer()));

                objs[kCitSubSummary][one][two].Add(*context.NewDiscObj(pub_desc), false);
            }
            ++it;
        }
    }
    objs.GetMap().erase(field_name);

}


//  ----------------------------------------------------------------------------
DISCREPANCY_SUMMARIZE(CITSUBAFFIL_CONFLICT)
//  ----------------------------------------------------------------------------
{
    // Note - using leading spaces to control order of messages
    if (m_Objs.empty()) {
        m_Objs[kCitSubSummary]["          No citsubs were found!"].Fatal();
    }

    if (m_Objs[kSummaries].GetMap().size() > 1) {
        CReportNode::TNodeMap::iterator it = m_Objs[kSummaries].GetMap().begin();
        while (it != m_Objs[kSummaries].GetMap().end()) {
            string two;
            if (NStr::IsBlank(it->first)) {
                two = "         [n] Cit-sub[s] [has] no affiliation";
            } else {
                two = "        [n] CitSub[s] [has] affiliation " + it->first;
            }
            NON_CONST_ITERATE(TReportObjectList, robj, m_Objs[kSummaries][it->first].GetObjects())
            {
                const CDiscrepancyObject* other_disc_obj = dynamic_cast<CDiscrepancyObject*>(robj->GetNCPointer());
                CConstRef<CSeqdesc> pub_desc(dynamic_cast<const CSeqdesc*>(other_disc_obj->GetObject().GetPointer()));

                m_Objs[kCitSubSummary][two].Add(*context.NewDiscObj(pub_desc), false);
            }
            ++it;
        }
    }
    m_Objs.GetMap().erase(kSummaries);

    SummarizeAffilField(m_Objs, context, "Affil", "institution", "       ");
    SummarizeAffilField(m_Objs, context, "Div", "department", "      ");
    SummarizeAffilField(m_Objs, context, "City", "city", "     ");
    SummarizeAffilField(m_Objs, context, "Sub", "state/province", "    ");
    SummarizeAffilField(m_Objs, context, "Country", "country", "   ");
    SummarizeAffilField(m_Objs, context, "Street", "street", "  ");
    SummarizeAffilField(m_Objs, context, "Postal_code", "postal code", " ");

    if (m_Objs.empty()) {
        return;
    }
    m_ReportItems = m_Objs.Export(*this)->GetSubitems();
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
DISCREPANCY_CASE(SUBMITBLOCK_CONFLICT, CSubmit_block, eDisc | eOncaller, "Records should have identical submit-blocks")
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
        NON_CONST_ITERATE(TReportObjectList, robj, m_Objs["blocks"].GetObjects())
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


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
