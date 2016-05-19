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
#include <objects/biblio/Imprint.hpp>
#include <objects/general/Name_std.hpp>
#include <objects/general/Person_id.hpp>
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


END_SCOPE(NDiscrepancy)
END_NCBI_SCOPE
