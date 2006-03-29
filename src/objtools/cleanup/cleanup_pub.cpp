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
* Author:  Robert Smith
*
* File Description:
*   Implementation of private parts of basic cleanup.
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/Cit_pat.hpp>
#include <objects/biblio/Cit_let.hpp>
#include <objects/biblio/Cit_proc.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Affil.hpp>
#include <objects/biblio/Author.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/general/Name_std.hpp>

#include <objtools/cleanup/cleanup.hpp>
#include "cleanup_utils.hpp"

#include "cleanupp.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// CPubdesc cleanup

static bool s_IsOnlinePub(const CPubdesc& pd)
{
    if (pd.IsSetPub()) {
        ITERATE (CPubdesc::TPub::Tdata, it, pd.GetPub().Get()) {
            if ((*it)->IsGen()) {
                const CCit_gen& gen = (*it)->GetGen();
                if (gen.IsSetCit()  &&
                    NStr::StartsWith(gen.GetCit(), "Online Publication", NStr::eNocase)) {
                    return true;
                }
            }
        }
    }
    return false;
}


void CCleanup_imp::BasicCleanup(CPubdesc& pd)
{
    if (pd.IsSetPub()) {
        BasicCleanup(pd.SetPub());
    }
    
    CLEAN_STRING_MEMBER(pd, Name);
    
    if (s_IsOnlinePub(pd)) {
        TRUNCATE_SPACES(pd, Comment);
    } else {
        CLEAN_STRING_MEMBER(pd, Comment);
    }
}


static bool s_FixInitials(const CPub_equiv& equiv)
{
    bool has_id  = false, 
    has_art = false;
    
    ITERATE (CPub_equiv::Tdata, it, equiv.Get()) {
        if ((*it)->IsPmid()  ||  (*it)->IsMuid()) {
            has_id = true;
        } else if ((*it)->IsArticle()) {
            has_art = true;
        }
    }
    return !(has_art  &&  has_id);
}


void CCleanup_imp::BasicCleanup(CPub_equiv& pe)
{
    x_FlattenPubEquiv(pe);
    
    bool fix_initials = s_FixInitials(pe);
    NON_CONST_ITERATE(CPub_equiv::Tdata, it, pe.Set()) {
        BasicCleanup(**it, fix_initials);
    }
}


void CCleanup_imp::x_FlattenPubEquiv(CPub_equiv& pe)
{
    CPub_equiv::Tdata& data = pe.Set();
    
    CPub_equiv::Tdata::iterator it = data.begin();
    while(it != data.end()) {
        if ((*it)->IsEquiv()) {
            CPub_equiv& equiv = (*it)->SetEquiv();
            x_FlattenPubEquiv(equiv);
            copy(equiv.Set().begin(), equiv.Set().end(), back_inserter(data));
            it = data.erase(it);
        } else {
            ++it;
        }
    }
}


void CCleanup_imp::BasicCleanup(CPub& pub, bool fix_initials)
{
    switch (pub.Which()) {
    case CPub::e_Gen:
        BasicCleanup(pub.SetGen(), fix_initials);
        break;
    case CPub::e_Sub:
        BasicCleanup(pub.SetSub(), fix_initials);
        break;
    case CPub::e_Article:
        BasicCleanup(pub.SetArticle(), fix_initials);
        break;
    case CPub::e_Book:
        BasicCleanup(pub.SetBook(), fix_initials);
        break;
    case CPub::e_Patent:
        BasicCleanup(pub.SetPatent(), fix_initials);
        break;
    case CPub::e_Man:
        BasicCleanup(pub.SetMan(), fix_initials);
        break;
        
    default:
        break;
    }
}

// cleanup stuff from biblio directory.

void CCleanup_imp::BasicCleanup(CCit_gen& cg, bool fix_initials)
{
    if (cg.IsSetAuthors()) {
        BasicCleanup(cg.SetAuthors(), fix_initials);
    }
    if (cg.IsSetCit()) {
        CCit_gen::TCit& cit = cg.SetCit();
        if (NStr::StartsWith(cit, "unpublished", NStr::eNocase)) {
            cit[0] = 'U';
        }
        if (!cg.IsSetJournal()) {
            cg.ResetVolume();
            cg.ResetPages();
            cg.ResetIssue();
        }
        NStr::TruncateSpacesInPlace(cit);
    }
    if (cg.IsSetPages()) {
        RemoveSpaces(cg.SetPages());
    }
    
    //!!! TO DO: serial
}


void CCleanup_imp::BasicCleanup(CCit_sub& cs, bool fix_initials)
{
    CRef<CCit_sub::TAuthors> authors;
    if (cs.IsSetAuthors()) {
        authors.Reset(&cs.SetAuthors());
        BasicCleanup( *authors, fix_initials);
    }
    
    if ( cs.IsSetImp()) {
        CCit_sub::TImp& imp =  cs.SetImp();
        if (authors  &&  !authors->IsSetAffil()  &&  imp.IsSetPub()) {
            authors->SetAffil(imp.SetPub());
            imp.ResetPub();
        }
        if (! cs.IsSetDate()  &&  imp.IsSetDate()) {
             cs.SetDate().Assign(imp.GetDate());
            imp.ResetDate();
        }
        if (!imp.IsSetPub()) {
             cs.ResetImp();
        }
    }
    if (authors  &&  authors->IsSetAffil()) {
        //!!! TO DO
    }
}


void CCleanup_imp::BasicCleanup(CCit_art& ca, bool fix_initials)
{
    if (ca.IsSetAuthors()) {
        BasicCleanup(ca.SetAuthors(), fix_initials);
    }
    if (ca.IsSetFrom()) {
        CCit_art::TFrom& from = ca.SetFrom();
        if (from.IsBook()) {
            BasicCleanup(from.SetBook(), fix_initials);
        } else if (from.IsProc()) {
            BasicCleanup(from.SetProc(), fix_initials);
        }
    }
}


void CCleanup_imp::BasicCleanup(CCit_book& cb, bool fix_initials)
{
    if (cb.IsSetAuthors()) {
        BasicCleanup(cb.SetAuthors(), fix_initials);
    }
}


void CCleanup_imp::BasicCleanup(CCit_pat& cp, bool fix_initials)
{
    if (cp.IsSetAuthors()) {
        BasicCleanup(cp.SetAuthors(), fix_initials);
    }
    if (cp.IsSetApplicants()) {
        BasicCleanup(cp.SetApplicants(), fix_initials);
    }
    if (cp.IsSetAssignees()) {
        BasicCleanup(cp.SetAssignees(), fix_initials);
    }
}


void CCleanup_imp::BasicCleanup(CCit_let& cl, bool fix_initials)
{
    if (cl.IsSetCit()  &&  cl.IsSetType()  &&  cl.GetType() == CCit_let::eType_thesis) {
        BasicCleanup(cl.SetCit(), fix_initials);
    }
}


void CCleanup_imp::BasicCleanup(CCit_proc& cp, bool fix_initials)
{
    if (cp.IsSetBook()) {
        BasicCleanup(cp.SetBook(), fix_initials);
    }
}


static bool s_IsEmptyAffil(const CAuth_list::TAffil& affil)
{
    if (affil.IsStr()) {
        return NStr::IsBlank(affil.GetStr());
    } else if (affil.IsStd()) {
        const CAuth_list::TAffil::TStd& std = affil.GetStd();
        return !(std.IsSetAffil()  ||  std.IsSetDiv()      ||  std.IsSetCity()    ||
                 std.IsSetSub()    ||  std.IsSetCountry()  ||  std.IsSetStreet()  ||
                 std.IsSetEmail()  ||  std.IsSetFax()      ||  std.IsSetPhone()   ||
                 std.IsSetPostal_code());
    }
    return true;
}


static bool s_IsEmptyAuthor(const CAuthor& auth)
{
    if (!auth.IsSetName()) {
        return true;
    }
    
    const CAuthor::TName& name = auth.GetName();
    
    const string* str = NULL;
    switch (name.Which()) {
        case CAuthor::TName::e_not_set:
            return true;
            
        case CAuthor::TName::e_Name:
        {{
            const CName_std& nstd = name.GetName();
            if ((!nstd.IsSetLast()      ||  NStr::IsBlank(nstd.GetLast()))      &&
                (!nstd.IsSetFirst()     ||  NStr::IsBlank(nstd.GetFirst()))     &&
                (!nstd.IsSetMiddle()    ||  NStr::IsBlank(nstd.GetMiddle()))    &&
                (!nstd.IsSetFull()      ||  NStr::IsBlank(nstd.GetFull()))      &&
                (!nstd.IsSetInitials()  ||  NStr::IsBlank(nstd.GetInitials()))  &&
                (!nstd.IsSetSuffix()    ||  NStr::IsBlank(nstd.GetSuffix()))    &&
                (!nstd.IsSetTitle()     ||  NStr::IsBlank(nstd.GetTitle()))) {
                return true;
            }
            break;
        }}
            
        case CAuthor::TName::e_Ml:
            str = &name.GetMl();
            break;
        case CAuthor::TName::e_Str:
            str = &name.GetStr();
            break;
        case CAuthor::TName::e_Consortium:
            str = &name.GetConsortium();
            break;
            
        default:
            break;
    };
    if (str != NULL  &&  NStr::IsBlank(*str)) {
        return true;
    }
    return false;
}


void CCleanup_imp::BasicCleanup(CAuth_list& al, bool fix_initials)
{
    if (al.IsSetAffil()) {
        BasicCleanup(al.SetAffil());
        if (s_IsEmptyAffil(al.SetAffil())) {
            al.ResetAffil();
        }
    }
    
    if (al.IsSetNames()) {
        typedef CAuth_list::TNames TNames;
        TNames& names = al.SetNames();
        switch (names.Which()) {
            case TNames::e_Std:
            {{
                // call BasicCleanup for each CAuthor
                TNames::TStd& std = names.SetStd();
                TNames::TStd::iterator it = std.begin();
                while (it != std.end()) {
                    BasicCleanup(**it, fix_initials);
                    if (s_IsEmptyAuthor(**it)) {
                        it = std.erase(it);
                    } else {
                        ++it;
                    }
                }
                if (std.empty()) {
                    names.Reset();
                }
                break;
            }}
            case TNames::e_Ml:
            {{
                CleanStringContainer(names.SetMl());
                if (names.GetMl().empty()) {
                    names.Reset();
                }
                break;
            }}
            case TNames::e_Str:
            {{
                CleanStringContainer(names.SetStr());
                if (names.GetStr().empty()) {
                    names.Reset();
                }
                break;
            }}
            default:
                break;
        }
    }
    // if no remaining authors, put in default author for legal ASN.1
    if (!al.IsSetNames()) {
        al.SetNames().SetStr().push_back("?");
    }
}


void CCleanup_imp::BasicCleanup(CAffil& af)
{
    switch (af.Which()) {
        case CAffil::e_Str:
        {{
            CLEAN_STRING_CHOICE(af, Str);
            break;
        }}
        case CAffil::e_Std:
        {{
            CAffil::TStd& std = af.SetStd();
            CLEAN_STRING_MEMBER(std, Affil);
            CLEAN_STRING_MEMBER(std, Div);
            CLEAN_STRING_MEMBER(std, City);
            CLEAN_STRING_MEMBER(std, Sub);
            CLEAN_STRING_MEMBER(std, Country);
            CLEAN_STRING_MEMBER(std, Street);
            CLEAN_STRING_MEMBER(std, Email);
            CLEAN_STRING_MEMBER(std, Fax);
            CLEAN_STRING_MEMBER(std, Phone);
            CLEAN_STRING_MEMBER(std, Postal_code);
            break;
        }}
        default:
            break;
    }
}


void CCleanup_imp::BasicCleanup(CAuthor& au, bool fix_initials)
{
    if (au.IsSetName()) {
        BasicCleanup(au.SetName(), fix_initials);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.2  2006/03/29 16:35:02  rsmith
 * Move BasicCleanup(CPubdesc) from cleanpp.cpp to here.
 *
 * Revision 1.1  2006/03/14 20:21:50  rsmith
 * Move BasicCleanup functionality from objects to objtools/cleanup
 *
 *
 * ===========================================================================
 */

