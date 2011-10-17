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
#include <objects/pub/Pub_set.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/Cit_pat.hpp>
#include <objects/biblio/Cit_let.hpp>
#include <objects/biblio/Cit_proc.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Id_pat.hpp>
#include <objects/medline/Medline_entry.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Affil.hpp>
#include <objects/biblio/Author.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/biblio/Title.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/general/Name_std.hpp>
#include <objects/seqfeat/Gb_qual.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/annot_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seq_feat_handle.hpp>

#include <objtools/cleanup/cleanup.hpp>
#include "cleanup_utils.hpp"

#include "cleanupp.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// CPubdesc cleanup


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


//  ----------------------------------------------------------------------------
//  CCleanup implementation:
//  ----------------------------------------------------------------------------
void CCleanup_imp::BasicCleanup(CPubdesc& pd)
{
    if (pd.IsSetPub()) {
        BasicCleanup(pd.SetPub());
    }
    
    CLEAN_STRING_MEMBER(pd, Name);
    
    if (pd.IsSetComment()) {
        CleanDoubleQuote(pd.SetComment());
    }

    CLEAN_STRING_MEMBER(pd, Comment);
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
            ChangeMade(CCleanupChange::eChangePublication);
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
    case CPub::e_Medline:
        BasicCleanup(pub.SetMedline(), fix_initials);
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
        if (NStr::StartsWith(cit, "unpublished", NStr::eNocase) && !NStr::StartsWith (cit, "Unpublished", NStr::eCase)) {
            cit[0] = 'U';
            ChangeMade(CCleanupChange::eChangePublication);
        }
        if (!cg.IsSetJournal()
            && (cg.IsSetVolume() || cg.IsSetPages() || cg.IsSetIssue())) {
            cg.ResetVolume();
            cg.ResetPages();
            cg.ResetIssue();
            ChangeMade(CCleanupChange::eChangePublication);
        }
        size_t cit_size = cit.size();
        NStr::TruncateSpacesInPlace(cit);
        if (cit_size != cit.size()) {
            ChangeMade(CCleanupChange::eChangePublication);
        }
    }
    if (cg.IsSetPages()) {
        if (RemoveSpaces(cg.SetPages()) ) {
            ChangeMade(CCleanupChange::eChangePublication);
        }
    }
    
    //!!! TO DO: serial
    /*
     if (stripSerial) {
         cgp->serial_number = -1;
     }
     
     compare labels before and after doing the above.
     if label changed : (sqnutil1.c, 6156)
     ValNodeCopyStr (publist, 1, buf1);
     ValNodeCopyStr (publist, 2, buf2);

     */
    if (cg.IsSetDate()) {
        BasicCleanup (cg.SetDate());
    }
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
            ChangeMade(CCleanupChange::eChangePublication);
        }
        if (! cs.IsSetDate()  &&  imp.IsSetDate()) {
             cs.SetDate().Assign(imp.GetDate());
            imp.ResetDate();
            ChangeMade(CCleanupChange::eChangePublication);
        }
        if (!imp.IsSetPub()) {
            cs.ResetImp();
            ChangeMade(CCleanupChange::eChangePublication);
        }
    }
    if (authors  &&  authors->IsSetAffil()) {
        //!!! TO DO
        CCit_sub::TAuthors::TAffil& affil = authors->SetAffil();
        if (affil.IsStr()) {
            string str = affil.SetStr();
            if (NStr::StartsWith(str, "to the ", NStr::eNocase) &&
                str.size() >= 34 &&
                NStr::StartsWith(str.substr(24), " databases", NStr::eNocase) ) {
                if (str[35] == '.') {
                    str = str.substr(35);
                } else {
                    str = str.substr(34);
                }
                affil.SetStr(str);
                ChangeMade(CCleanupChange::eChangePublication);
                BasicCleanup(affil);
            }
        }
    }

    if (cs.IsSetDate()) {
        BasicCleanup (cs.SetDate());
    }
}


void CCleanup_imp::BasicCleanup(CCit_jour &j)
{
    if (j.IsSetImp()) {
        BasicCleanup (j.SetImp());
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
        } else if (from.IsJournal()) {
            BasicCleanup(from.SetJournal());
        }
    }
}


void CCleanup_imp::BasicCleanup(CCit_book& cb, bool fix_initials)
{
    if (cb.IsSetAuthors()) {
        BasicCleanup(cb.SetAuthors(), fix_initials);
    }
    if (cb.IsSetImp()) {
        BasicCleanup(cb.SetImp());
    }
}


void CCleanup_imp::BasicCleanup(CMedline_entry& ml, bool fix_initials)
{
    if ( ! ml.IsSetCit() || ! ml.GetCit().IsSetAuthors() ) {
        return;
    }
    BasicCleanup( ml.SetCit().SetAuthors(), fix_initials);
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
    if (cp.IsSetApp_date()) {
        BasicCleanup (cp.SetApp_date());
    }
    if (cp.IsSetDate_issue()) {
        BasicCleanup (cp.SetDate_issue());
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


void CCleanup_imp::BasicCleanup (CImprint& imp)
{
    if (imp.IsSetDate()) {
        BasicCleanup (imp.SetDate());
    }

    if (imp.IsSetPubstatus() && imp.GetPubstatus() == ePubStatus_aheadofprint
        && (!imp.IsSetPrepub() || imp.GetPrepub() != CImprint::ePrepub_in_press)) {
        if (!imp.IsSetVolume() || NStr::IsBlank (imp.GetVolume())
            || !imp.IsSetPages() || NStr::IsBlank (imp.GetPages())) {
            imp.SetPrepub (CImprint::ePrepub_in_press);
            ChangeMade (CCleanupChange::eChangePublication);
        }
    }
    if (imp.IsSetPubstatus() && imp.GetPubstatus() == ePubStatus_aheadofprint
        && imp.IsSetPrepub() && imp.GetPrepub() == CImprint::ePrepub_in_press) {
        if (imp.IsSetVolume() && !NStr::IsBlank (imp.GetVolume())
            && imp.IsSetPages() && !NStr::IsBlank (imp.GetPages())) {
            imp.ResetPrepub();
            ChangeMade (CCleanupChange::eChangePublication);
        }
    }

    if (imp.IsSetPubstatus() && imp.GetPubstatus() == ePubStatus_epublish
        && imp.IsSetPrepub() && imp.GetPrepub() == CImprint::ePrepub_in_press) {
        imp.ResetPrepub();
    }

}


static bool s_IsEmpty(const CAuth_list::TAffil& affil)
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


static bool s_IsEmpty(const CAuthor& auth)
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


// when we reset author names, we need to put in a place holder - otherwise the ASN.1 becomes invalid
static
void s_ResetAuthorNames (CAuth_list::TNames& names) 
{
    list< string > auth_list;

    names.Reset();
    auth_list = names.SetStr();
    auth_list.clear();
    auth_list.push_back("?");
}


void CCleanup_imp::BasicCleanup(CAuth_list& al, bool fix_initials)
{
    if (al.IsSetAffil()) {
        BasicCleanup(al.SetAffil());
        if (s_IsEmpty(al.GetAffil())) {
            al.ResetAffil();
            ChangeMade(CCleanupChange::eChangePublication);
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
                    if (s_IsEmpty(**it)) {
                        it = std.erase(it);
                        ChangeMade(CCleanupChange::eChangePublication);
                    } else {
                        ++it;
                    }
                }
                if (std.empty()) {
                    s_ResetAuthorNames (names);
                    //al.ResetNames();
                    ChangeMade(CCleanupChange::eChangePublication);
                }
                break;
            }}
            case TNames::e_Ml:
            {{
                if (ConvertAuthorContainerMlToStd(al)) {
                    ChangeMade(CCleanupChange::eChangePublication);
                }
                break;
            }}
            case TNames::e_Str:
            {{
                if (CleanStringContainer(names.SetStr())) {
                    ChangeMade(CCleanupChange::eChangePublication);
                }
                if (names.GetStr().empty()) {
                    s_ResetAuthorNames (names);
                    ChangeMade(CCleanupChange::eChangePublication);
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
        ChangeMade(CCleanupChange::eChangePublication);
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
            if (std.CanGetCountry() ) {
                if (NStr::CompareNocase (std.GetCountry(), "U.S.A.") == 0) {
                    std.SetCountry("USA");
                    ChangeMade (CCleanupChange::eChangePublication);
                }
            }
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


// ExtendedCleanup methods



// remove additional equivalent CitSub descriptors
// Was MergeEquivalentCitSubs in C Toolkit

static bool s_IsCitSubPub(const CPubdesc& pd)
{
    bool has_sub = false;
    bool has_gen = false;

    FOR_EACH_PUB_ON_PUBDESC (it, pd) {
        if ((*it)->Which() == CPub::e_Sub) {
            if (has_sub) {
                // already has sub
                return false;
            } else {
                has_sub = true;
            }
        } else if ((*it)->Which() == CPub::e_Gen) {
            if (has_gen) {
                // already has gen
                return false;
            } else if ((*it)->GetGen().IsSetAuthors()
                       || (*it)->GetGen().IsSetCit()
                       || (*it)->GetGen().IsSetDate()
                       || (*it)->GetGen().IsSetIssue()
                       || (*it)->GetGen().IsSetIssue()
                       || (*it)->GetGen().IsSetJournal()
                       || (*it)->GetGen().IsSetMuid()
                       || (*it)->GetGen().IsSetPages()
                       || (*it)->GetGen().IsSetPmid()
                       || (*it)->GetGen().IsSetTitle()
                       || (*it)->GetGen().IsSetVolume()
                       || !(*it)->GetGen().IsSetSerial_number()) {
               // can only have serial number set
               return false;
            }
            has_gen = true;
        } else {
            // can have one sub and one gen only
            return false;
        }
    }
    return has_sub;
}


bool CCleanup_imp::x_IsCitSubPub(const CSeqdesc& sd)
{
    if (sd.Which() == CSeqdesc::e_Pub && s_IsCitSubPub (sd.GetPub())) {
        return true;
    } else {
        return false;
    }
}


bool CCleanup_imp::x_MergeCitSubs(CSeqdesc& sd1, CSeqdesc& sd2)
{
    if (x_IsCitSubPub(sd1) && x_IsCitSubPub(sd2)) {
        int serial_number1 = 0;
        int serial_number2 = 0;        
        CRef<CCit_sub> sub1(new CCit_sub);
        CRef<CCit_sub> sub2(new CCit_sub);
        CRef<CCit_gen> gen1(new CCit_gen);
        CRef<CCit_gen> gen2(new CCit_gen);

        FOR_EACH_PUB_ON_PUBDESC (it1, sd1.GetPub()) {
            if ((*it1)->Which() == CPub::e_Gen) {
                gen1->Assign((*it1)->GetGen());
                serial_number1 = (*it1)->GetGen().GetSerial_number();
            } else if ((*it1)->Which() == CPub::e_Sub) {
                sub1->Assign ((*it1)->GetSub());
            }
        }
        FOR_EACH_PUB_ON_PUBDESC (it2, sd2.GetPub()) {
            if ((*it2)->Which() == CPub::e_Gen) {
                gen2->Assign((*it2)->GetGen());
                serial_number2 = (*it2)->GetGen().GetSerial_number();
            } else if ((*it2)->Which() == CPub::e_Sub) {
                sub2->Assign ((*it2)->GetSub());
            }
        }
        if ((serial_number1 == 0 || serial_number2 == 0 || serial_number1 == serial_number2)
            && CitSubsMatch (*sub1, *sub2)) {
            // if no serial number on first but present on second, add to first
            if (serial_number1 == 0 && serial_number2 != 0) {
                bool found = false;
                EDIT_EACH_PUB_ON_PUBDESC (pub_it, sd1.SetPub()) {
                    if ((*pub_it)->Which() == CPub::e_Gen) {
                        (*pub_it)->SetGen().SetSerial_number(serial_number2);
                        found = true;
                    }
                }
                if (!found) {
                    CRef<CPub> pub(new CPub);
                    pub->SetGen(*gen2);
                    sd1.SetPub().SetPub().Set().push_back(pub);
                }
            }
            // if no affiliation on first, copy from second
            if (!sub1->GetAuthors().IsSetAffil() && sub2->GetAuthors().IsSetAffil()) {
                CRef<CAffil> affil(new CAffil);
                affil->Assign(sub2->GetAuthors().GetAffil());
                EDIT_EACH_PUB_ON_PUBDESC (pub_it, sd1.SetPub()) {
                    if ((*pub_it)->Which() == CPub::e_Sub) {
                        (*pub_it)->SetSub().SetAuthors().SetAffil(*affil);
                    }
                }
            }
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}


#define IS_NONEMPTY_STRING_MEMBER(o, x) \
((o).IsSet##x() && !NStr::IsBlank((o).Get##x()))

#define IS_NONZERO_INT_MEMBER(o, x) \
((o).IsSet##x() && (o).Get##x() != 0)


#define IS_NONEMPTY_MEMBER(o, x) \
((o).IsSet##x() && ! s_IsEmpty (o.Get##x()))

static bool s_IsEmpty (const CTitle& title)
{
    if (title.IsSet() && !NStr::IsBlank (title.GetTitle())) {
        return false;
    } else {
        return true;
    }
}


static bool s_IsEmpty (const list< string >& str_list)
{
    if (str_list.size() == 0) return true;
    ITERATE (list <string>, it, str_list) {
        if (!NStr::IsBlank (*it)) {
            return false;
        }
    }
    return true;
}


static bool s_IsEmpty (const CAuth_list& auth_list) 
{
    if (IS_NONEMPTY_MEMBER (auth_list, Affil)) {
        return false;
    }
    if (auth_list.IsSetNames()) {
        if (auth_list.GetNames().IsMl() && !s_IsEmpty(auth_list.GetNames().GetMl())) {
            return false;
        } else if (auth_list.GetNames().IsStr() && !s_IsEmpty (auth_list.GetNames().GetStr())) {
            return false;
        } else if (auth_list.GetNames().IsStd()) {
            ITERATE (list < CRef< CAuthor > >, it, auth_list.GetNames().GetStd()) {
                if (!s_IsEmpty (**it)) {
                    return false;
                }
            }
        }
    }
    return true; 
}


static bool s_IsEmpty (const CCit_gen& gen)
{
    if (IS_NONEMPTY_STRING_MEMBER(gen, Cit)
        || IS_NONEMPTY_MEMBER (gen, Authors)
        || IS_NONZERO_INT_MEMBER(gen, Muid)
        || IS_NONEMPTY_MEMBER(gen, Journal)
        || IS_NONEMPTY_STRING_MEMBER(gen, Volume)
        || IS_NONEMPTY_STRING_MEMBER(gen, Issue)
        || IS_NONEMPTY_STRING_MEMBER(gen, Pages)
        || gen.IsSetDate()
        || IS_NONZERO_INT_MEMBER(gen, Serial_number)
        || IS_NONEMPTY_STRING_MEMBER(gen, Title)
        || IS_NONZERO_INT_MEMBER(gen, Pmid)) {
        return false;
    } else {
        return true;
    }
}


static bool s_IsEmpty (const CCit_sub& sub)
{
    if (IS_NONEMPTY_MEMBER (sub, Authors)
        || sub.IsSetDate()
        || IS_NONEMPTY_STRING_MEMBER(sub, Descr)
        || sub.IsSetImp()
        || sub.IsSetMedium()) {
        return false;
    } else {
        return true;
    }
}


static bool s_IsEmpty (const CCit_jour& jour)
{
    if (IS_NONEMPTY_MEMBER(jour, Title)
        || jour.IsSetImp()) {
        return false;
    } else {
        return true;
    }
}


static bool s_IsEmpty (const CCit_book& book)
{
    if (IS_NONEMPTY_MEMBER (book, Title)
        || IS_NONEMPTY_MEMBER (book, Coll)
        || IS_NONEMPTY_MEMBER (book, Authors)
        || book.IsSetImp()) {
        return false;
    } else {
        return true;
    }
}


static bool s_IsEmpty (const CCit_proc& proc)
{
    if ((proc.IsSetBook() && !s_IsEmpty (proc.GetBook()))
        || proc.IsSetMeet()) {
        return false;
    } else {
        return true;
    }
}


static bool s_IsEmpty (const CCit_art& art)
{
    if (IS_NONEMPTY_MEMBER(art, Title)
        || IS_NONEMPTY_MEMBER (art, Authors)
        || art.IsSetIds()) {
        return false;
    }
    if (art.IsSetFrom()) {
        if (art.GetFrom().IsBook() && !s_IsEmpty (art.GetFrom().GetBook())) {
            return false;
        } else if (art.GetFrom().IsJournal() && !s_IsEmpty (art.GetFrom().GetJournal())) {
            return false;
        } else if (art.GetFrom().IsProc() && !s_IsEmpty (art.GetFrom().GetProc())) {
            return false;
        }
    }
    return true;
        
}


static bool s_IsEmpty (const CMedline_entry& ml)
{
    if (IS_NONZERO_INT_MEMBER (ml, Uid)
        || ml.IsSetEm()
        || IS_NONEMPTY_MEMBER (ml, Cit)
        || IS_NONEMPTY_STRING_MEMBER (ml, Abstract)
        || ml.IsSetMesh()
        || ml.IsSetSubstance()
        || ml.IsSetXref()
        || IS_NONEMPTY_MEMBER (ml, Idnum)
        || IS_NONEMPTY_MEMBER (ml, Gene)
        || IS_NONZERO_INT_MEMBER (ml, Pmid)
        || IS_NONEMPTY_MEMBER (ml, Pub_type)
        || ml.IsSetMlfield()
        || (ml.IsSetStatus() && ml.GetStatus() != CMedline_entry::eStatus_medline)) {
        return false;
    } else {
        return true;
    }
}


static bool s_IsEmpty (const CCit_pat& pat)
{
    if (IS_NONEMPTY_STRING_MEMBER (pat, Title)
        || IS_NONEMPTY_MEMBER (pat, Authors)
        || IS_NONEMPTY_STRING_MEMBER (pat, Country)
        || IS_NONEMPTY_STRING_MEMBER (pat, Doc_type)
        || IS_NONEMPTY_STRING_MEMBER (pat, Number)
        || pat.IsSetDate_issue()
        || IS_NONEMPTY_MEMBER (pat, Class)
        || IS_NONEMPTY_STRING_MEMBER (pat, App_number)
        || pat.IsSetApp_date()
        || IS_NONEMPTY_MEMBER (pat, Applicants)
        || IS_NONEMPTY_MEMBER (pat, Assignees)
        || pat.IsSetPriority()
        || IS_NONEMPTY_STRING_MEMBER (pat, Abstract)) {
        return false;
    } else {
        return true;
    }
}


static bool s_IsEmpty (const CId_pat& id_pat)
{
    if (IS_NONEMPTY_STRING_MEMBER (id_pat, Country)
        || IS_NONEMPTY_STRING_MEMBER (id_pat, Doc_type)) {
        return false;
    } 
    if (id_pat.IsSetId()) {
        if (id_pat.GetId().IsNumber()
            && !NStr::IsBlank (id_pat.GetId().GetNumber())) {
            return false;
        } else if (id_pat.GetId().IsApp_number()
                   && !NStr::IsBlank (id_pat.GetId().GetApp_number())) {
            return false;
        }
    }
    return true;
}
    

static bool s_IsEmpty (const CCit_let& let)
{
    if (IS_NONEMPTY_MEMBER (let, Cit)
        || IS_NONEMPTY_STRING_MEMBER (let, Man_id)
        || let.IsSetType()) {
        return false;
    } else {
        return true;
    }
}


static bool s_IsEmpty (const CPub& pub);

static bool s_IsEmpty (const CPub_equiv& pub_equiv) 
{
    ITERATE (list < CRef < CPub > >, it, pub_equiv.Get()) {
        if (!s_IsEmpty (**it)) {
            return false;
        }
    }
    return true;
}


static bool s_IsEmpty (const CPub& pub) 
{
    if ((pub.IsArticle() && s_IsEmpty (pub.GetArticle()))
        || (pub.IsBook() && s_IsEmpty (pub.GetBook()))
        || (pub.IsEquiv() && s_IsEmpty (pub.GetEquiv()))
        || (pub.IsGen() && s_IsEmpty (pub.GetGen()))
        || (pub.IsJournal() && s_IsEmpty (pub.GetJournal()))
        || (pub.IsMan() && s_IsEmpty (pub.GetMan()))
        || (pub.IsMedline() && s_IsEmpty (pub.GetMedline()))
        || (pub.IsMuid() && pub.GetMuid() == 0)
        || (pub.IsPat_id() && s_IsEmpty (pub.GetPat_id()))
        || (pub.IsPatent() && s_IsEmpty (pub.GetPatent()))
        || (pub.IsPmid() && pub.GetPmid() == 0)
        || (pub.IsProc() && s_IsEmpty (pub.GetProc()))
        || (pub.IsSub() && s_IsEmpty (pub.GetSub()))) {
        return true;
    } else {
        return false;
    }
}


CRef<CPub> CCleanup_imp::x_MinimizePub (const CPub& pub)
{
    CRef<CPub> min_pub (new CPub);
    
    if (pub.IsEquiv()) {
        ITERATE (list <CRef <CPub> >, pub_it, pub.GetEquiv().Get()) {
            if (pub.IsMuid()) {
                if (min_pub->Which() == CPub::e_not_set) {
                    min_pub->Assign(**pub_it);
                } else {
                    CRef<CPub> epub (new CPub);
                    epub->SetEquiv();
                    CRef<CPub> apub (new CPub);
                    apub->Assign(**pub_it);
                    epub->SetEquiv().Set().push_back(min_pub);
                    epub->SetEquiv().Set().push_back(apub);
                    return epub;
                }
            } else if (pub.IsPmid()) {
                if (min_pub->Which() == CPub::e_not_set) {
                    min_pub->Assign(**pub_it);
                } else {
                    CRef<CPub> epub (new CPub);
                    epub->SetEquiv();
                    CRef<CPub> apub (new CPub);
                    apub->Assign(**pub_it);
                    epub->SetEquiv().Set().push_back(min_pub);
                    epub->SetEquiv().Set().push_back(apub);
                    return epub;
                }
            }
        }
        if (min_pub->Which() != CPub::e_not_set) {
            return min_pub;
        }
        
        ITERATE (list <CRef <CPub> >, pub_it, pub.GetEquiv().Get()) {
            if (!(*pub_it)->IsGen() || (*pub_it)->GetGen().GetSerial_number() == -1) {
                return x_MinimizePub (**pub_it);
            }
        }
        return x_MinimizePub (*(pub.GetEquiv().Get().front()));
    } 
    
    if (pub.IsMuid() || pub.IsPmid()) {
        min_pub->Assign (pub);
        return min_pub;
    }
    
    string pub_unique;
    string pub_label;
    
    pub.GetLabel(&pub_unique, CPub::eContent, true);
    pub.GetLabel(&pub_label, CPub::eContent, false);
    if (NStr::Equal (pub_unique, pub_label) || NStr::IsBlank (pub_label)) {
        min_pub->Assign (pub);
        return min_pub;
    }
    min_pub->SetGen().SetCit(pub_unique);
    return min_pub;
}


void CCleanup_imp::x_ChangeCitationQualToCitationPub(CBioseq_Handle bs)
{
    // first, make a list of all the publications (descriptors, source features and imp features with citations)
    // for this Bioseq
    vector <CConstRef <CPub> > pub_list;
    
    // collect descriptors first
    CSeqdesc_CI desc_it (bs, CSeqdesc::e_Pub);
    while (desc_it) {
        FOR_EACH_PUB_ON_PUBDESC (pub_it, desc_it->GetPub()) {
            const CPub& pub = **pub_it;
            pub_list.push_back (CConstRef<CPub>(&pub));
        }
        ++desc_it;
    }
    
    // collect publication features and imp features with citations next
    CFeat_CI feat_ci(bs);    
    while (feat_ci) {
        if (feat_ci->GetFeatSubtype() == CSeqFeatData::eSubtype_pub) {
            FOR_EACH_PUB_ON_PUBDESC (pub_it, feat_ci->GetData().GetPub()) {
                const CPub& pub = **pub_it;
                pub_list.push_back (CConstRef<CPub>(&pub));
            }
        } else if (feat_ci->GetFeatType() == CSeqFeatData::e_Imp
                && feat_ci->IsSetCit()) {
            ITERATE( CPub_set::TPub, pi, feat_ci->GetCit().GetPub() ) {
                const CPub& pub = **pi;
                pub_list.push_back(CConstRef<CPub>(&pub));
            }
        }
        ++feat_ci;
    }
    
    // now find the features that have cit quals and replace them
    feat_ci.Rewind();
    
    while (feat_ci) {
        if (feat_ci->IsSetQual()) {
            bool has_cit = false;
            FOR_EACH_GBQUAL_ON_FEATURE (it, *feat_ci) {
                if ((*it)->CanGetQual()
                    && NStr::Equal ((*it)->GetQual(), "citation")) {
                    has_cit = true;
                }
            }
            if (has_cit) {
                CRef<CSeq_feat> feat(new CSeq_feat);
                feat->Assign(feat_ci->GetOriginalFeature());
                EDIT_EACH_GBQUAL_ON_SEQFEAT (it, *feat) {
                    CGb_qual& gb_qual = **it;
                    if (gb_qual.CanGetQual()
                        && NStr::Equal(gb_qual.GetQual(), "citation")) {
                        try {
                            if (gb_qual.IsSetVal()) {
                                // find the publication that goes with this citation
                                int citnum = NStr::StringToInt (gb_qual.GetVal());
                                if ((size_t)citnum < pub_list.size()) {
                                    // add to the citation list for the feature
                                    feat->SetCit().SetPub().push_back(x_MinimizePub(*pub_list[citnum]));
                                    //remove the qual
                                    ERASE_GBQUAL_ON_SEQFEAT (it, *feat);
                                }
                            }
                        } catch (...) {
                        }
                    }
                }
                CSeq_feat_EditHandle efh (feat_ci->GetSeq_feat_Handle());
                efh.Replace (*feat);
            }                    
        }
        ++feat_ci;
    }
}


// was CheckSitSubNew in C Toolkit
bool CCleanup_imp::x_ChangeCitSub(CPub& pub)
{
    bool changed = false;
    if (pub.IsSub()) {    
        CCit_sub& sub = pub.SetSub();
    
        if (sub.CanGetAuthors()
            && !sub.GetAuthors().CanGetAffil()
            && sub.CanGetImp()
            && sub.GetImp().CanGetPub()) {
            sub.SetAuthors().SetAffil(sub.SetImp().SetPub());
            sub.SetImp().ResetPub();
            changed = true;
        }
        if (!sub.CanGetDate() && sub.CanGetImp() && sub.GetImp().CanGetDate()) {
            sub.SetDate(sub.SetImp().SetDate());
            sub.SetImp().ResetDate();
            changed = true;
        }    
        if (sub.CanGetImp() && ! sub.CanGetImp()) {
            sub.ResetImp();
            changed = true;
        }
    }
    return changed;
}


void CCleanup_imp::x_ChangeCitSub (CBioseq_Handle bh)
{
    if (bh.CanGetInst_Repr()
        && bh.GetInst_Repr() != CSeq_inst::eRepr_raw
        && bh.GetInst_Repr() != CSeq_inst::eRepr_const) {
        return;
    }
    
    if (bh.CanGetDescr()) {
        CBioseq_EditHandle eh = bh.GetEditHandle();
        EDIT_EACH_DESCRIPTOR_ON_BIOSEQ (desc_it, eh) {
            if ((*desc_it)->IsPub()) {
                EDIT_EACH_PUB_ON_PUBDESC (pub_it, (*desc_it)->SetPub()) {
                    x_ChangeCitSub(**pub_it);
                }
            }
        }
    }
}


void CCleanup_imp::x_ChangeCitSub (CBioseq_set_Handle bh)
{
    if (bh.CanGetDescr()) {
        CBioseq_set_EditHandle eh = bh.GetEditHandle();
        EDIT_EACH_DESCRIPTOR_ON_BIOSEQ (desc_it, eh) {
            if ((*desc_it)->IsPub()) {
                EDIT_EACH_PUB_ON_PUBDESC (pub_it, (*desc_it)->SetPub()) {
                    x_ChangeCitSub(**pub_it);
                }
            }
        }
    }

    FOR_EACH_SEQENTRY_ON_SEQSET (it, *(bh.GetCompleteBioseq_set())) {
        x_ChangeCitSub(m_Scope->GetSeq_entryHandle(**it));
    }
}


void CCleanup_imp::x_ChangeCitSub (CSeq_entry_Handle seh)
{
    if (seh.Which() == CSeq_entry::e_Seq) {
        x_ChangeCitSub(CBioseq_Handle(seh.GetSeq()));
    } else if (seh.Which() == CSeq_entry::e_Set) {
        x_ChangeCitSub(CBioseq_set_Handle(seh.GetSet()));
    }
}


bool s_PubdescMatch (const CPubdesc& p1, const CPubdesc& p2)
{
    // first check fields on pubdesc other than pub
    if (!MATCH_STRING_VALUE (p1, p2, Name)) {
        return false;
    }
    MATCH_BOOL_VALUE (p1, p2, Numexc);
    MATCH_BOOL_VALUE (p1, p2, Poly_a);
    if (!MATCH_STRING_VALUE (p1, p2, Seq_raw)) {
        return false;
    }
    if (!MATCH_INT_VALUE (p1, p2, Align_group)) {
        return false;
    }
    if (!MATCH_STRING_VALUE (p1, p2, Comment)) {
        return false;
    }
    if (!MATCH_INT_VALUE (p1, p2, Reftype)) {
        return false;
    }

    MERGEABLE_STRING_VALUE(p1, p2, Fig);
    MERGEABLE_STRING_VALUE(p1, p2, Maploc);
    if (p1.IsSetNum() && p2.IsSetNum() && !p1.GetNum().Equals(p2.GetNum())) {
        return false;
    }

    if (!p1.IsSetPub() && !p2.IsSetPub()) {
        return true;
    } else if (!p1.IsSetPub() || !p2.IsSetPub()) {
        return false;
    } else if (p1.GetPub().Get().size() != p2.GetPub().Get().size()) {
        return false;
    }
    
    list <CRef <CPub> >::const_iterator it1 = p1.GetPub().Get().begin();
    list <CRef <CPub> >::const_iterator it2 = p2.GetPub().Get().begin();
    while (it1 != p1.GetPub().Get().end()
           && it2 != p2.GetPub().Get().end()
           && (*it1)->Equals(**it2)) {
        ++it1;
        ++it2;;
    }
    if (it1 != p1.GetPub().Get().end() || it2 != p2.GetPub().Get().end()) {
        return false;
    } else {
        return true;
    }    

}


bool s_PubdescMatch (CSeq_entry_Handle seh, const CPubdesc& pd)
{
    CSeqdesc_CI desc_it(seh, CSeqdesc::e_Pub);
    while (desc_it) {
        if (s_PubdescMatch(desc_it->GetPub(), pd)) {
            return true;
        }
        ++desc_it;
    }
    return false;
}


void CCleanup_imp::x_RemovePubMatch(const CSeq_entry& se, CPubdesc& pd)
{
    CSeq_descr::Tdata remove_list;
    if (se.Which() == CSeq_entry::e_Seq) {
        CBioseq_EditHandle bh = (m_Scope->GetBioseqHandle(se.GetSeq())).GetEditHandle();
        EDIT_EACH_DESCRIPTOR_ON_BIOSEQ (it, bh) {
            if ((*it)->Which() == CSeqdesc::e_Pub
                && s_PubdescMatch ((*it)->GetPub(), pd)) {
                x_MergeDuplicatePubs (pd, (*it)->SetPub());
                remove_list.push_back (*it);
            }
        }
        for (CSeq_descr::Tdata::iterator it = remove_list.begin();
                 it != remove_list.end(); ++it) { 
            bh.RemoveSeqdesc(**it);
            ChangeMade(CCleanupChange::eRemoveDescriptor);
        }
    } else if (se.Which() == CSeq_entry::e_Set) {
        CBioseq_set_EditHandle bh = (m_Scope->GetBioseq_setHandle(se.GetSet())).GetEditHandle();
        EDIT_EACH_DESCRIPTOR_ON_SEQSET (it, bh) {
            if ((*it)->Which() == CSeqdesc::e_Pub
                && s_PubdescMatch ((*it)->GetPub(), pd)) {
                x_MergeDuplicatePubs (pd, (*it)->SetPub());
                remove_list.push_back (*it);
            }
        }
        for (CSeq_descr::Tdata::iterator it = remove_list.begin();
                 it != remove_list.end(); ++it) { 
            bh.RemoveSeqdesc(**it);
            ChangeMade(CCleanupChange::eRemoveDescriptor);
        }
    }
}


static bool s_IsRefSeq (CBioseq_Handle bh)
{
    ITERATE (CBioseq_Handle::TId, it, bh.GetId()) {
        CSeq_id::EAccessionInfo info = (*it).GetSeqId()->IdentifyAccession();
        if (info == CSeq_id::eAcc_refseq_chromosome
            || info == CSeq_id::eAcc_refseq_contig
            || info == CSeq_id::eAcc_refseq_genome
            || info == CSeq_id::eAcc_refseq_genomic
            || info == CSeq_id::eAcc_refseq_mrna
            || info == CSeq_id::eAcc_refseq_mrna_predicted
            || info == CSeq_id::eAcc_refseq_ncrna
            || info == CSeq_id::eAcc_refseq_ncrna_predicted
            || info == CSeq_id::eAcc_refseq_prot
            || info == CSeq_id::eAcc_refseq_prot_predicted
            || info == CSeq_id::eAcc_refseq_unreserved
            || info == CSeq_id::eAcc_refseq_wgs_intermed
            || info == CSeq_id::eAcc_refseq_wgs_nuc
            || info == CSeq_id::eAcc_refseq_wgs_prot) {
            return true;
        }
    }
    return false;
}

static bool s_IsRefSeq (CBioseq_set_Handle bh)
{
    if (!bh.CanGetClass() || bh.GetClass() != CBioseq_set::eClass_segset) {
        return false;
    }
    CBioseq_CI b_ci (bh);
    while (b_ci) {
        if (s_IsRefSeq (*b_ci)) {
            return true;
        }
        ++b_ci;
    }
    return false;
}


void CCleanup_imp::x_MergeAndMovePubs(CBioseq_set_Handle bsh)
{
    if (!bsh.GetCompleteBioseq_set()->IsSetSeq_set()) {
        return;
    }
        
    if (!bsh.CanGetClass() || !bsh.GetCompleteBioseq_set()->IsSetSeq_set()) {
        // no merging
    } else if (bsh.GetClass() == CBioseq_set::eClass_nuc_prot) {
        // move any publications that are on the nucleotide sequence or nucleotide segmented set
        // to the nuc-prot set
        CBioseq_set_EditHandle bseh = bsh.GetEditHandle();
        const CSeq_entry& se = **(bsh.GetCompleteBioseq_set()->GetSeq_set().begin());
        CSeq_descr::Tdata remove_list;
        bool moved_descriptor = false;
        if (se.IsSeq()) {
            CBioseq_Handle nbh = m_Scope->GetBioseqHandle(se.GetSeq());
            if (!s_IsRefSeq (nbh)) {
                CBioseq_EditHandle nbeh = nbh.GetEditHandle();
                EDIT_EACH_DESCRIPTOR_ON_BIOSEQ (it, nbeh) {
                    if ((*it)->Which() == CSeqdesc::e_Pub) {
                        bseh.AddSeqdesc(**it);
                        remove_list.push_back(*it);
                        moved_descriptor = true;
                    }
                }
                for (CSeq_descr::Tdata::iterator it1 = remove_list.begin();
                     it1 != remove_list.end(); ++it1) { 
                    nbeh.RemoveSeqdesc(**it1);
                }
            }
        } else if (se.IsSet()) {
            CBioseq_set_Handle nbh = m_Scope->GetBioseq_setHandle(se.GetSet());
            if (!s_IsRefSeq(nbh)) {
                CBioseq_set_EditHandle nbeh = nbh.GetEditHandle();
                EDIT_EACH_DESCRIPTOR_ON_SEQSET (it, nbeh) {
                    if ((*it)->Which() == CSeqdesc::e_Pub) {
                        bseh.AddSeqdesc(**it);
                        moved_descriptor = true;
                        remove_list.push_back(*it);
                    }
                }
                for (CSeq_descr::Tdata::iterator it1 = remove_list.begin();
                     it1 != remove_list.end(); ++it1) { 
                    nbeh.RemoveSeqdesc(**it1);
                }
            }
        }
        if (moved_descriptor) {
            ChangeMade (CCleanupChange::eMoveDescriptor);
        }        
    } else if (bsh.GetClass() == CBioseq_set::eClass_segset) {
        // if any publication is on all of the parts in the parts set, 
        // put the publication on the segset and remove it from the parts   
        FOR_EACH_SEQENTRY_ON_SEQSET (it, *( bsh.GetCompleteBioseq_set())) {
            if ((*it)->Which() == CSeq_entry::e_Set) {
                CBioseq_set_EditHandle parts = (m_Scope->GetBioseq_setHandle((*it)->GetSet())).GetEditHandle();
                if (parts.CanGetClass() 
                    && parts.GetClass() == CBioseq_set::eClass_parts
                    && parts.GetCompleteBioseq_set()->IsSetSeq_set()) {                    
                    list<CRef <CSeq_entry> >::const_iterator part_it = parts.GetCompleteBioseq_set()->GetSeq_set().begin();
                    const CSeq_entry& se_first = **part_it;
                    CSeq_descr::Tdata remove_list;
                    if (se_first.Which() == CSeq_entry::e_Seq) {
                        CBioseq_EditHandle first_eh = (m_Scope->GetBioseqHandle(se_first.GetSeq())).GetEditHandle();
                        EDIT_EACH_DESCRIPTOR_ON_BIOSEQ (first_descr_it, first_eh) {
                            if ((*first_descr_it)->Which() == CSeqdesc::e_Pub) {
                                bool all_present = true;
                                list<CRef <CSeq_entry> >::const_iterator remainder_it = part_it;
                                ++remainder_it;
                                while (remainder_it != parts.GetCompleteBioseq_set()->GetSeq_set().end() && all_present) {
                                    all_present = s_PubdescMatch(m_Scope->GetSeq_entryHandle(**remainder_it), (*first_descr_it)->GetPub());
                                    ++remainder_it;
                                }
                                if (all_present) {
                                    remainder_it = part_it;
                                    ++remainder_it;
                                    while (remainder_it != parts.GetCompleteBioseq_set()->GetSeq_set().end()) {
                                        x_RemovePubMatch(**remainder_it, (*first_descr_it)->SetPub());
                                        ++remainder_it;
                                    }
                                    CBioseq_set_EditHandle segset_edit = bsh.GetEditHandle();
                                    segset_edit.AddSeqdesc(**first_descr_it);
                                    ChangeMade (CCleanupChange::eMoveDescriptor);
                                    remove_list.push_back(*first_descr_it);
                                }
                            }                            
                        }
                        for (CSeq_descr::Tdata::iterator it1 = remove_list.begin();
                             it1 != remove_list.end(); ++it1) { 
                            first_eh.RemoveSeqdesc(**it1);
                            ChangeMade (CCleanupChange::eRemoveDescriptor);
                        }                        
                    } else if (se_first.Which() == CSeq_entry::e_Set) {
                        CBioseq_set_EditHandle first_eh = (m_Scope->GetBioseq_setHandle(se_first.GetSet())).GetEditHandle();
                        EDIT_EACH_DESCRIPTOR_ON_SEQSET (first_descr_it, first_eh) {
                            if ((*first_descr_it)->Which() == CSeqdesc::e_Pub) {
                                bool all_present = true;
                                list<CRef <CSeq_entry> >::const_iterator remainder_it = part_it;
                                ++remainder_it;
                                while (remainder_it != parts.GetCompleteBioseq_set()->GetSeq_set().end() && all_present) {
                                    all_present = s_PubdescMatch(m_Scope->GetSeq_entryHandle(**remainder_it), (*first_descr_it)->GetPub());
                                    ++remainder_it;
                                }
                                if (all_present) {
                                    remainder_it = part_it;
                                    ++remainder_it;
                                    while (remainder_it != parts.GetCompleteBioseq_set()->GetSeq_set().end()) {
                                        x_RemovePubMatch(**remainder_it, (*first_descr_it)->SetPub());
                                    }
                                    CBioseq_set_EditHandle segset_edit = bsh.GetEditHandle();
                                    segset_edit.AddSeqdesc(**first_descr_it);
                                    ChangeMade (CCleanupChange::eMoveDescriptor);
                                    remove_list.push_back(*first_descr_it);
                                }
                            }                            
                        }
                        for (CSeq_descr::Tdata::iterator it1 = remove_list.begin();
                             it1 != remove_list.end(); ++it1) { 
                            first_eh.RemoveSeqdesc(**it1);
                            ChangeMade (CCleanupChange::eRemoveDescriptor);
                        }                        
                    }
                    ++part_it;
                }
            }
        }
    }
}


void CCleanup_imp::x_RemoveDuplicatePubsFromBioseqsInSet(CBioseq_set_Handle bsh)
{
    if (!bsh.IsSetDescr()) {
        return;
    }
    FOR_EACH_DESCRIPTOR_ON_DESCR (pubdesc, bsh.GetDescr()) {
        if (!(*pubdesc)->IsPub()) continue;
        bool any_pubs_left = false;
        CSeq_descr::Tdata remove_list;

        CBioseq_CI bs_ci (bsh);
        while (bs_ci) {
            if (bs_ci->CanGetInst()
                && (bs_ci->GetInst_Repr() == CSeq_inst::eRepr_raw
                    || bs_ci->GetInst_Repr() == CSeq_inst::eRepr_const)
                && bs_ci->CanGetDescr()) {
                remove_list.clear();
                FOR_EACH_DESCRIPTOR_ON_DESCR (it, bs_ci->GetDescr()) {
                    if ((*it)->Which() == CSeqdesc::e_Pub) {
                        if (s_PubdescMatch((*pubdesc)->GetPub(), (*it)->GetPub())) {
                            
                            remove_list.push_back (*it);
                        } else {
                            any_pubs_left = true;
                        }
                    }
                }
            
                if (remove_list.size() > 0) {
                    CBioseq_EditHandle eh = (*bs_ci).GetEditHandle();
                    for (CSeq_descr::Tdata::iterator it1 = remove_list.begin();
                        it1 != remove_list.end(); ++it1) { 
                        eh.RemoveSeqdesc(**it1);
                    }
                }
            }        
            ++bs_ci;
        }
        if (!any_pubs_left) return;
    }
}


bool CCleanup_imp::x_MergeDuplicatePubs (CPubdesc &dst_pub, CPubdesc &rm_pub)
{
    if (!s_PubdescMatch (dst_pub, rm_pub)) {
        return false;
    }

    // some values may be missing, so if present on pub to be removed and not pub to keep, add

    if (!dst_pub.IsSetFig() && rm_pub.IsSetFig()) {
        dst_pub.SetFig(rm_pub.GetFig());
    }
    if (!dst_pub.IsSetMaploc() && rm_pub.IsSetMaploc()) {
        dst_pub.SetMaploc(rm_pub.GetMaploc());
    }
    if (!dst_pub.IsSetNum() && rm_pub.IsSetNum()) {
        CRef<CNumbering> num(new CNumbering);
        num->Assign(rm_pub.GetNum());
        dst_pub.SetNum(*num);
    }
    return true;
}


void CCleanup_imp::x_MergeDuplicatePubs(CBioseq_set_Handle bsh)
{
    if (bsh.CanGetDescr()) {
        CBioseq_set_EditHandle eh = bsh.GetEditHandle();
        EDIT_EACH_DESCRIPTOR_ON_DESCR (it1, eh.SetDescr()) {
            if (!(*it1)->IsPub()) continue;
            CSeq_descr::Tdata remove_list;
            bool any_pubs_left = false;            
            list< CRef< CSeqdesc > >::iterator it2 = it1;
            ++it2;
            while (it2 != eh.SetDescr().Set().end()) {
                if ((*it2)->IsPub()) {
                    if (s_PubdescMatch((*it1)->GetPub(), (*it2)->GetPub())) {
                        x_MergeDuplicatePubs ((*it1)->SetPub(), (*it2)->SetPub());
                        remove_list.push_back (*it2);
                    } else {
                        any_pubs_left = true;
                    }
                }
                ++it2;
            }
            if (remove_list.size() > 0) {
                for (CSeq_descr::Tdata::iterator it3 = remove_list.begin();
                    it3 != remove_list.end(); ++it3) { 
                    eh.RemoveSeqdesc(**it3);
                }
            }
            if (!any_pubs_left) return;
            ++it1;
        }      
    }
}


void CCleanup_imp::x_MergeDuplicatePubs(CBioseq_Handle bsh)
{
    if (bsh.CanGetDescr()) {
        CBioseq_EditHandle eh = bsh.GetEditHandle();
        EDIT_EACH_DESCRIPTOR_ON_DESCR (it1, eh.SetDescr()) {
            if (!(*it1)->IsPub()) continue;
            CSeq_descr::Tdata remove_list;
            bool any_pubs_left = false;            
            list< CRef< CSeqdesc > >::iterator it2 = it1;
            ++it2;
            while (it2 != eh.SetDescr().Set().end()) {
                if ((*it2)->IsPub()) {
                    if (s_PubdescMatch((*it1)->GetPub(), (*it2)->GetPub())) {
                        x_MergeDuplicatePubs ((*it1)->SetPub(), (*it2)->SetPub());
                        remove_list.push_back (*it2);
                    } else {
                        any_pubs_left = true;
                    }
                }
                ++it2;
            }
            if (remove_list.size() > 0) {
                for (CSeq_descr::Tdata::iterator it3 = remove_list.begin();
                    it3 != remove_list.end(); ++it3) { 
                    eh.RemoveSeqdesc(**it3);
                }
            }
            if (!any_pubs_left) return;
            ++it1;
        }      
    }
}


bool CCleanup_imp::x_RemoveEmptyPubs (CPubdesc& pubdesc)
{
    bool changes_made = false;
    
    EDIT_EACH_PUB_ON_PUBDESC (pub_it, pubdesc) {
        if (s_IsEmpty (**pub_it)) {
            ERASE_PUB_ON_PUBDESC (pub_it, pubdesc);
            ChangeMade(CCleanupChange::eChangePublication);
            changes_made = true;
        }
    }
           
    return changes_made;
}


void CCleanup_imp::x_RemoveEmptyPubs (CSeq_annot_Handle sa)
{
    if (sa.IsFtable()) {
        CFeat_CI feat_ci(sa);
        while (feat_ci) {
            if (feat_ci->GetData().IsPub()) {
                CSeq_feat_EditHandle efh (feat_ci->GetSeq_feat_Handle());
                CRef <CSeq_feat> new_feat(new CSeq_feat);
                new_feat->Assign (feat_ci->GetOriginalFeature());  
                if (x_RemoveEmptyPubs (new_feat->SetData().SetPub())) {
                    if (new_feat->GetData().GetPub().GetPub().Get().empty()) {
                        efh.Remove();
                        ChangeMade (CCleanupChange::eRemoveFeat);
                        ChangeMade (CCleanupChange::eRemoveEmptyPub);
                    } else {
                        efh.Replace (*new_feat);
                    }
                } else if (feat_ci->GetData().GetPub().GetPub().Get().empty()) {
                    efh.Remove();
                    ChangeMade (CCleanupChange::eRemoveFeat);
                    ChangeMade (CCleanupChange::eRemoveEmptyPub);
                }
            }
            ++feat_ci;                
        }
    }
}

void CCleanup_imp::x_RemoveEmptyPubs(CSeq_descr& sdr, CSeq_descr::Tdata& remove_list)
{
    EDIT_EACH_DESCRIPTOR_ON_DESCR (it, sdr) {
        if ((*it)->Which() == CSeqdesc::e_Pub) {     
            CPubdesc& pubdesc = (*it)->SetPub();  
            x_RemoveEmptyPubs (pubdesc);
            if (pubdesc.GetPub().Get().empty()) {
                remove_list.push_back(*it);
                ChangeMade (CCleanupChange::eRemoveEmptyPub);
            }
        }
    }
}


void CCleanup_imp::x_ExtendedCleanupPubs (CBioseq_set_Handle bss)
{
    // apply rules to members of set
   FOR_EACH_SEQENTRY_ON_SEQSET (it, *(bss.GetCompleteBioseq_set())) {
        switch ((**it).Which()) {
            case CSeq_entry::e_Seq:
                x_ExtendedCleanupPubs(m_Scope->GetBioseqHandle((**it).GetSeq()));
                break;
            case CSeq_entry::e_Set:
            {
                CBioseq_set_Handle bssh = m_Scope->GetBioseq_setHandle((**it).GetSet());
                x_ExtendedCleanupPubs(bssh);
            }
                break;
            case CSeq_entry::e_not_set:
            default:
                break;
        }
    }

    // Now apply rules to this set
    
    // remove empty pub descriptors   
    x_ActOnDescriptors (bss, &ncbi::objects::CCleanup_imp::x_RemoveEmptyPubs);
 
    // remove empty pub features
    x_ActOnSeqAnnots (bss, &ncbi::objects::CCleanup_imp::x_RemoveEmptyPubs);   
    
    
    // convert full-length publication features to descriptors
    x_ActOnSeqAnnots (bss, &ncbi::objects::CCleanup_imp::x_ConvertFullLenPubFeatureToDescriptor);   
    
    // merge equivalent cit-sub pub descriptors
    x_ActOnDescriptorsForMerge(bss, &ncbi::objects::CCleanup_imp::x_IsCitSubPub, 
                                      &ncbi::objects::CCleanup_imp::x_MergeCitSubs);

    //MoveSegmPubs
    //MoveNPPubs
    x_MergeAndMovePubs(bss);
            
    //unique pubs
    x_MergeDuplicatePubs(bss);
            
    // check pubs in Bioseqs, delete if they are already on the top
    x_RemoveDuplicatePubsFromBioseqsInSet(bss);
                                  
    // remove all Site-ref imp feats
    x_ActOnSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_RemoveSiteRefImpFeats);
}


void CCleanup_imp::x_ExtendedCleanupPubs (CBioseq_Handle bss)
{
    x_ChangeCitationQualToCitationPub(bss);

    x_RecurseForDescriptors(bss, &ncbi::objects::CCleanup_imp::x_RemoveEmptyPubs);
    x_RecurseForSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_RemoveEmptyPubs);  
    x_RecurseForSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_ConvertFullLenPubFeatureToDescriptor);          
    x_RecurseDescriptorsForMerge(bss, &ncbi::objects::CCleanup_imp::x_IsCitSubPub, 
                                      &ncbi::objects::CCleanup_imp::x_MergeCitSubs);
    //unique pubs
    x_MergeDuplicatePubs(bss);

    // remove all Site-ref imp feats
    x_RecurseForSeqAnnots(bss, &ncbi::objects::CCleanup_imp::x_RemoveSiteRefImpFeats);
}


void CCleanup_imp::x_ExtendedCleanupPubs (CSeq_annot_Handle sa)
{
    x_RemoveEmptyPubs (sa);
    x_ConvertFullLenPubFeatureToDescriptor(sa);   
    x_RemoveSiteRefImpFeats (sa);
}



END_SCOPE(objects)
END_NCBI_SCOPE
