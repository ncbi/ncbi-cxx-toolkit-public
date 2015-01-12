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
 * Author:  Michael Kornbluh, NCBI
 *
 * File Description:
 *   Unit test for CSeqFeatData and some closely related code
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <objects/general/Name_std.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/biblio/Cit_gen.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/Cit_proc.hpp>
#include <objects/biblio/Cit_let.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/biblio/Author.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Affil.hpp>
#include <corelib/ncbimisc.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/test_boost.hpp>

#include <boost/test/parameterized_test.hpp>
#include <util/util_exception.hpp>
#include <util/util_misc.hpp>
#include <util/random_gen.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

namespace {
    bool s_TestSubtype(CSeqFeatData::ESubtype eSubtype) {
        const string & sNameOfSubtype = 
            CSeqFeatData::SubtypeValueToName(eSubtype);
        if( sNameOfSubtype.empty() ) {
            return false;
        }
        // make sure it goes in the other direction, too
        const CSeqFeatData::ESubtype eReverseSubtype =
            CSeqFeatData::SubtypeNameToValue(sNameOfSubtype);
        BOOST_CHECK( eReverseSubtype != CSeqFeatData::eSubtype_bad );
        BOOST_CHECK_EQUAL(eSubtype, eReverseSubtype);
        return true;
    }
}

BOOST_AUTO_TEST_CASE(s_TestSubtypeMaps)
{
    typedef set<CSeqFeatData::ESubtype> TSubtypeSet;
    // this set holds the subtypes where we expect it to fail
    // when we try to convert to string
    TSubtypeSet subtypesExpectedToFail;

#ifdef ESUBTYPE_SHOULD_FAIL
#  error ESUBTYPE_SHOULD_FAIL already defined
#endif

#define ESUBTYPE_SHOULD_FAIL(name) \
    subtypesExpectedToFail.insert(CSeqFeatData::eSubtype_##name);

    // this list seems kind of long.  Maybe some of these
    // should be added to the lookup maps so they
    // do become valid.
    ESUBTYPE_SHOULD_FAIL(bad);
    ESUBTYPE_SHOULD_FAIL(EC_number);
    ESUBTYPE_SHOULD_FAIL(Imp_CDS);
    ESUBTYPE_SHOULD_FAIL(allele);
    ESUBTYPE_SHOULD_FAIL(imp);
    ESUBTYPE_SHOULD_FAIL(misc_RNA);
    ESUBTYPE_SHOULD_FAIL(mutation);
    ESUBTYPE_SHOULD_FAIL(org);
    ESUBTYPE_SHOULD_FAIL(precursor_RNA);
    ESUBTYPE_SHOULD_FAIL(source);
    ESUBTYPE_SHOULD_FAIL(max);

#undef ESUBTYPE_SHOULD_FAIL

    ITERATE_0_IDX(iSubtypeAsInteger, CSeqFeatData::eSubtype_max) 
    {
        CSeqFeatData::ESubtype eSubtype = 
            static_cast<CSeqFeatData::ESubtype>(iSubtypeAsInteger);

        // subtypesExpectedToFail tells us which ones are
        // expected to fail.  Others are expected to succeed
        if( subtypesExpectedToFail.find(eSubtype) == 
            subtypesExpectedToFail.end() ) 
        {
            NCBITEST_CHECK(s_TestSubtype(eSubtype));
        } else {
            NCBITEST_CHECK( ! s_TestSubtype(eSubtype) );
        }
    }

    // a couple values we haven't covered that should
    // also be tested.
    NCBITEST_CHECK( ! s_TestSubtype(CSeqFeatData::eSubtype_max) );
    NCBITEST_CHECK( ! s_TestSubtype(CSeqFeatData::eSubtype_any) );

    BOOST_CHECK_EQUAL( CSeqFeatData::SubtypeNameToValue("Gene"), CSeqFeatData::eSubtype_gene);
}


BOOST_AUTO_TEST_CASE(Test_CapitalizationFix)
{

    BOOST_CHECK_EQUAL(COrgMod::FixHostCapitalization("SQUASH"), "squash");
    BOOST_CHECK_EQUAL(COrgMod::FixHostCapitalization("SOUR cherry"), "sour cherry");
    CRef<COrgMod> m(new COrgMod());
    m->SetSubtype(COrgMod::eSubtype_nat_host);
    m->SetSubname("Turkey");
    m->FixCapitalization();
    BOOST_CHECK_EQUAL(m->GetSubname(), "turkey");

    CRef<CSubSource> s(new CSubSource());
    s->SetSubtype(CSubSource::eSubtype_sex);
    s->SetName("Dioecious");
    s->FixCapitalization();
    BOOST_CHECK_EQUAL(s->GetName(), "dioecious");


}


BOOST_AUTO_TEST_CASE(Test_FixLatLonFormat)
{
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("27 degrees 22'50'' N 88 degrees 13'16'' E", false), "27.3806 N 88.2211 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("27 degrees 22 50  N 88 degrees 13 16 E", false), "27.3806 N 88.2211 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("27 degrees 22'50 N 88 degrees 13'16 E", false), "27.3806 N 88.2211 E");


}


CRef<CAuth_list> s_MakeAuthList()
{
    CRef<CAuth_list> auth_list(new CAuth_list());

    auth_list->SetAffil().SetStd().SetAffil("Murdoch University");
    auth_list->SetAffil().SetStd().SetDiv("School of Veterinary and Life Sciences");
    auth_list->SetAffil().SetStd().SetCity("Murdoch");
    auth_list->SetAffil().SetStd().SetSub("Western Australia");
    auth_list->SetAffil().SetStd().SetCountry("Australia");
    auth_list->SetAffil().SetStd().SetStreet("90 South Street");
    auth_list->SetAffil().SetStd().SetPostal_code("6150");
    CRef<CAuthor> auth1(new CAuthor());
    auth1->SetName().SetName().SetLast("Yang");
    auth1->SetName().SetName().SetFirst("Rongchang");
    auth1->SetName().SetName().SetInitials("R.");
    auth_list->SetNames().SetStd().push_back(auth1);
    CRef<CAuthor> auth2(new CAuthor());
    auth2->SetName().SetName().SetLast("Ryan");
    auth2->SetName().SetName().SetFirst("Una");
    auth2->SetName().SetName().SetInitials("U.");
    auth_list->SetNames().SetStd().push_back(auth2);

    return auth_list;
}

void s_ChangeAuthorFirstName(CAuth_list& auth_list)
{
    if (auth_list.SetNames().SetStd().size() == 0) {
        CRef<CAuthor> auth(new CAuthor());
        auth->SetName().SetName().SetLast("Last");
        auth_list.SetNames().SetStd().push_back(auth);
    }
    auth_list.SetNames().SetStd().back()->SetName().SetName().SetFirst("Uan");
}


void s_ChangeAuthorLastName(CAuth_list& auth_list)
{
    if (auth_list.SetNames().SetStd().size() == 0) {
        CRef<CAuthor> auth(new CAuthor());
        auth->SetName().SetName().SetLast("Last");
        auth_list.SetNames().SetStd().push_back(auth);
    }
    auth_list.SetNames().SetStd().back()->SetName().SetName().SetLast("Nyar");
}


CRef<CAuth_list> s_SetAuthList(CPub& pub)
{
    CRef<CAuth_list> rval(NULL);
    switch (pub.Which()) {
        case CPub::e_Article:
            rval.Reset(&(pub.SetArticle().SetAuthors()));
            break;
        case CPub::e_Gen:
            rval.Reset(&(pub.SetGen().SetAuthors()));
            break;
        case CPub::e_Sub:
            rval.Reset(&(pub.SetSub().SetAuthors()));
            break;
        case CPub::e_Book:
            rval.Reset(&(pub.SetBook().SetAuthors()));
            break;
        case CPub::e_Proc:
            rval.Reset(&(pub.SetProc().SetBook().SetAuthors()));
            break;
        case CPub::e_Man:
            rval.Reset(&(pub.SetMan().SetCit().SetAuthors()));
            break;
        default:
            rval = null;
            break;
    }
    return rval;
}


bool s_ChangeAuthorFirstName(CPub& pub)
{
    bool rval = false;
    CRef<CAuth_list> auth_list = s_SetAuthList(pub);
    if (auth_list) {
        s_ChangeAuthorFirstName(*auth_list);
        rval = true;
    }
    return rval;
}


bool s_ChangeAuthorLastName(CPub& pub)
{
    bool rval = true;
    CRef<CAuth_list> auth_list = s_SetAuthList(pub);
    if (auth_list) {
        s_ChangeAuthorLastName(*auth_list);
        rval = true;
    }
    return rval;
}


CRef<CImprint> s_MakeImprint()
{
    CRef<CImprint> imp(new CImprint());
    imp->SetDate().SetStr("?");
    imp->SetPrepub(CImprint::ePrepub_in_press);
    return imp;
}


CRef<CImprint> s_SetImprint(CPub& pub)
{
    CRef<CImprint> imp(NULL);
    switch (pub.Which()) {
        case CPub::e_Article:
            if (pub.GetArticle().IsSetFrom()) {
                if (pub.GetArticle().GetFrom().IsJournal()) {
                    imp.Reset(&(pub.SetArticle().SetFrom().SetJournal().SetImp()));
                } else if (pub.GetArticle().GetFrom().IsBook()) {
                    imp.Reset(&(pub.SetArticle().SetFrom().SetBook().SetImp()));
                }
            }
            break;
        case CPub::e_Sub:
            imp.Reset(&(pub.SetSub().SetImp()));
            break;
        case CPub::e_Book:
            imp.Reset(&(pub.SetBook().SetImp()));
            break;
        case CPub::e_Journal:
            imp.Reset(&(pub.SetJournal().SetImp()));
            break;
        case CPub::e_Proc:
            imp.Reset(&(pub.SetProc().SetBook().SetImp()));
            break;
        case CPub::e_Man:
            imp.Reset(&(pub.SetMan().SetCit().SetImp()));
            break;
        default:
            break;
    }

    return imp;
}


void s_AddNameTitle(CTitle& title)
{
    CRef<CTitle::C_E> t(new CTitle::C_E());
    t->SetName("a random title");
    title.Set().push_back(t);
}


void s_ChangeNameTitle(CTitle& title)
{
    if (title.Set().size() == 0) {
        s_AddNameTitle(title);
    }
    title.Set().front()->SetName() += " plus";
}


void s_AddJTATitle(CTitle& title)
{
    CRef<CTitle::C_E> t(new CTitle::C_E());
    t->SetJta("a random title");
    title.Set().push_back(t);
}


void s_ChangeJTATitle(CTitle& title)
{
    if (title.Set().size() == 0) {
        s_AddJTATitle(title);
    }
    title.Set().front()->SetJta() += " plus";
}

void s_ChangeTitle(CPub& pub)
{
    switch (pub.Which()) {
        case CPub::e_Article:
            s_ChangeNameTitle(pub.SetArticle().SetTitle());
            break;
        case CPub::e_Book:
            s_ChangeNameTitle(pub.SetBook().SetTitle());
            break;
        case CPub::e_Journal:
            s_ChangeJTATitle(pub.SetJournal().SetTitle());
            break;
        case CPub::e_Proc:
            s_ChangeNameTitle(pub.SetProc().SetBook().SetTitle());
            break;
        case CPub::e_Man:
            s_ChangeNameTitle(pub.SetMan().SetCit().SetTitle());
            break;
        case CPub::e_Gen:
            pub.SetGen().SetCit() += " plus";
            break;
        default:
            break;
    }
}


void s_ChangeDate(CDate& date) 
{
    date.SetStd().SetYear(2014);
}


void s_ChangeImprintNoMatch(CImprint& imp, int change_no)
{
    switch (change_no) {
        case 0:
            s_ChangeDate(imp.SetDate());
            break;
        case 1:
            imp.SetVolume("123");
            break;
        default:
            break;
    }
}


void s_ChangeImprintMatch(CImprint& imp, int change_no)
{
    switch (change_no) {
        case 0:
            imp.SetPrepub(CImprint::ePrepub_submitted);
            break;
        case 1:
            imp.SetPub().SetStr("Publisher");
            break;
        default:
            break;
    }
}


bool s_ChangeImprintNoMatch(CPub& pub, int change_no)
{
    bool rval = false;
    CRef<CImprint> imp = s_SetImprint(pub);
    if (imp) {
        s_ChangeImprintNoMatch(*imp, change_no);
        rval = true;
    }
    return rval;
}


bool s_ChangeImprintMatch(CPub& pub, int change_no)
{
    bool rval = false;
    CRef<CImprint> imp = s_SetImprint(pub);
    if (imp) {
        s_ChangeImprintMatch(*imp, change_no);
        rval = true;
    }
    return rval;
}


BOOST_AUTO_TEST_CASE(Test_AuthList_SameCitation)
{
    CRef<CAuth_list> auth_list1 = s_MakeAuthList();
    CRef<CAuth_list> auth_list2(new CAuth_list());
    auth_list2->Assign(*auth_list1);

    // should match if identical
    BOOST_CHECK_EQUAL(auth_list1->SameCitation(*auth_list2), true);

    // should match if only difference is author first name
    s_ChangeAuthorFirstName(*auth_list2);
    BOOST_CHECK_EQUAL(auth_list1->SameCitation(*auth_list2), true);

    // should not match if different last name
    s_ChangeAuthorLastName(*auth_list2);
    BOOST_CHECK_EQUAL(auth_list1->SameCitation(*auth_list2), false);
}


CRef<CCit_jour> s_MakeJournal()
{
    CRef<CCit_jour> jour(new CCit_jour());

    CRef<CTitle::C_E> j_title(new CTitle::C_E());
    j_title->SetJta("Experimental Parasitology");
    jour->SetTitle().Set().push_back(j_title);
    jour->SetImp().Assign(*s_MakeImprint());

    return jour;
}


CRef<CPub> s_MakeJournalArticlePub()
{
    CRef<CPub> pub(new CPub());
    CRef<CTitle::C_E> title(new CTitle::C_E());
    title->SetName("Isospora streperae n. sp. (Apicomplexa: Eimeriidae) from a Grey Currawong (Strepera versicolour plumbea) (Passeriformes: Artamidae) in Western Australia");
    pub->SetArticle().SetTitle().Set().push_back(title);
    pub->SetArticle().SetFrom().SetJournal().Assign(*s_MakeJournal());
    pub->SetArticle().SetAuthors().Assign(*s_MakeAuthList());
    return pub;
}


CRef<CCit_book> s_MakeBook()
{
    CRef<CCit_book> book(new CCit_book());
    CRef<CTitle::C_E> b_title(new CTitle::C_E());
    b_title->SetName("Book Title");
    book->SetTitle().Set().push_back(b_title);
    book->SetImp().Assign(*s_MakeImprint());
    book->SetAuthors().Assign(*s_MakeAuthList());

    return book;
}


CRef<CPub> s_MakeBookChapterPub()
{
    CRef<CPub> pub(new CPub());
    CRef<CTitle::C_E> title(new CTitle::C_E());
    title->SetName("Isospora streperae n. sp. (Apicomplexa: Eimeriidae) from a Grey Currawong (Strepera versicolour plumbea) (Passeriformes: Artamidae) in Western Australia");
    pub->SetArticle().SetTitle().Set().push_back(title);
    pub->SetArticle().SetFrom().SetBook().Assign(*s_MakeBook());
    pub->SetArticle().SetAuthors().Assign(*s_MakeAuthList());
    return pub;
}


void s_TestAuthorChanges(CPub& pub)
{
    CRef<CPub> other(new CPub());
    other->Assign(pub);
    // should match if only difference is author first name
    s_ChangeAuthorFirstName(*other);
    BOOST_CHECK_EQUAL(pub.SameCitation(*other), true);

    // should not match if difference in last name
    other->Assign(pub);
    s_ChangeAuthorLastName(*other);
    BOOST_CHECK_EQUAL(pub.SameCitation(*other), false);
}


void s_TestImprintChanges(CPub& pub)
{
    CRef<CPub> other(new CPub());
    other->Assign(pub);

    if (!s_SetImprint(pub)) {
        if (pub.IsGen()) {
            // test dates
            s_ChangeDate(other->SetGen().SetDate());
            BOOST_CHECK_EQUAL(pub.SameCitation(*other), false);
        }
        return;
    }


    // should match if noncritical Imprint change
    s_ChangeImprintMatch(*other, 0);
    BOOST_CHECK_EQUAL(pub.SameCitation(*other), true);
    other->Assign(pub);
    s_ChangeImprintMatch(*other, 1);
    BOOST_CHECK_EQUAL(pub.SameCitation(*other), true);

    // should not match if cricital Imprint change
    other->Assign(pub);
    s_ChangeImprintNoMatch(*other, 0);
    BOOST_CHECK_EQUAL(pub.SameCitation(*other), false);
    other->Assign(pub);
    s_ChangeImprintNoMatch(*other, 1);
    BOOST_CHECK_EQUAL(pub.SameCitation(*other), false);

}


void s_TestTitleChange(CPub& pub)
{
    // should not match if extra text in article title
    CRef<CPub> other(new CPub());
    other->Assign(pub);
    s_ChangeTitle(*other);
    BOOST_CHECK_EQUAL(pub.SameCitation(*other), false);
}


BOOST_AUTO_TEST_CASE(Test_Pub_SameCitation)
{
    CRef<CPub> pub1 = s_MakeJournalArticlePub();
    CRef<CPub> pub2(new CPub());
    pub2->Assign(*pub1);

    // journal article
    // should match if identical
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), true);

    // change journal title
    s_ChangeJTATitle(pub2->SetArticle().SetFrom().SetJournal().SetTitle());
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), false);

    s_TestAuthorChanges(*pub1);
    s_TestImprintChanges(*pub1);
    s_TestTitleChange(*pub1);

    // journal pub
    pub1->SetJournal().Assign(*s_MakeJournal());
    pub2->Assign(*pub1);

    // should match if identical
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), true);

    s_TestImprintChanges(*pub1);
    s_TestTitleChange(*pub1);

    // book chapter
    pub1 = s_MakeBookChapterPub();
    pub2->Assign(*pub1);

    // should match if identical
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), true);

    // change book title
    s_ChangeNameTitle(pub2->SetArticle().SetFrom().SetBook().SetTitle());
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), false);

    s_TestAuthorChanges(*pub1);
    s_TestImprintChanges(*pub1);
    s_TestTitleChange(*pub1);

    // Book
    pub1->SetBook().Assign(*s_MakeBook());
    pub2->Assign(*pub1);

    // should match if identical
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), true);

    s_TestAuthorChanges(*pub1);
    s_TestImprintChanges(*pub1);
    s_TestTitleChange(*pub1);

    // Proc
    pub1->SetProc().SetBook().Assign(*s_MakeBook());
    pub2->Assign(*pub1);

    // should match if identical
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), true);

    s_TestAuthorChanges(*pub1);
    s_TestImprintChanges(*pub1);
    s_TestTitleChange(*pub1);

    // Man
    pub1->SetMan().SetCit().Assign(*s_MakeBook());
    pub2->Assign(*pub1);

    // should match if identical
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), true);

    s_TestAuthorChanges(*pub1);
    s_TestImprintChanges(*pub1);
    s_TestTitleChange(*pub1);


    // Gen
    pub1->SetGen().SetCit("citation title");
    pub1->SetGen().SetVolume("volume");
    pub1->SetGen().SetIssue("issue");
    pub1->SetGen().SetPages("pages");
    pub1->SetGen().SetTitle("title");

    pub1->SetGen().SetAuthors().Assign(*s_MakeAuthList());
    pub2->Assign(*pub1);

    // should match if identical
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), true);

    // change other fields
    // volume
    pub2->SetGen().SetVolume("x");
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), false);
    pub1->SetGen().SetVolume("y");
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), false);
    pub2->Assign(*pub1);
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), true);
    // issue
    pub2->SetGen().SetIssue("x");
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), false);
    pub1->SetGen().SetIssue("y");
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), false);
    pub2->Assign(*pub1);
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), true);
    // pages
    pub2->SetGen().SetPages("x");
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), false);
    pub1->SetGen().SetPages("y");
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), false);
    pub2->Assign(*pub1);
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), true);
    // title
    pub2->SetGen().SetTitle("x");
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), false);
    pub1->SetGen().SetTitle("y");
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), false);
    pub2->Assign(*pub1);
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), true);
    // muid
    pub2->SetGen().SetMuid(40);
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), false);
    pub2->Assign(*pub1);
    pub1->SetGen().SetMuid(42);
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), false);
    pub2->Assign(*pub1);
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), true);
    // serial number
    pub2->SetGen().SetSerial_number(40);
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), false);
    pub1->SetGen().SetSerial_number(42);
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), false);
    pub2->Assign(*pub1);
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), true);

    // journal
    s_AddNameTitle(pub2->SetGen().SetJournal());
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), false);
    // should still be false if title types are different
    s_AddJTATitle(pub1->SetGen().SetJournal());
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), false);
    // but ok now
    s_AddJTATitle(pub2->SetGen().SetJournal());
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), true);

    s_TestAuthorChanges(*pub1);
    s_TestImprintChanges(*pub1);
    s_TestTitleChange(*pub1);

    // sub
    pub1->SetSub().SetAuthors().Assign(*s_MakeAuthList());
    pub1->SetSub().SetImp().Assign(*s_MakeImprint());
    pub2->Assign(*pub1);

    // should match if identical
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), true);

    s_TestAuthorChanges(*pub1);
    s_TestImprintChanges(*pub1);

}


BOOST_AUTO_TEST_CASE(Test_PubEquiv_SameCitation)
{
    CRef<CPub_equiv> eq1(new CPub_equiv());

    CRef<CPub> pub = s_MakeJournalArticlePub();
    eq1->Set().push_back(pub);

    CRef<CPub_equiv> eq2(new CPub_equiv());
    eq2->Assign(*eq1);

    // should match if identical
    BOOST_CHECK_EQUAL(eq1->SameCitation(*eq2), true);

    // should match if second also has PubMed ID (but first does not)
    CRef<CPub> pmid2(new CPub());
    pmid2->SetPmid().Set(4);
    eq2->Set().push_back(pmid2);
    BOOST_CHECK_EQUAL(eq1->SameCitation(*eq2), true);

    // but not if first has different article    
    s_ChangeAuthorLastName(eq1->Set().front()->SetArticle().SetAuthors());
    BOOST_CHECK_EQUAL(eq1->SameCitation(*eq2), false);

    // won't match even if PubMed IDs match
    CRef<CPub> pmid1(new CPub());
    pmid1->SetPmid().Set(4);
    eq1->Set().push_back(pmid1);
    BOOST_CHECK_EQUAL(eq1->SameCitation(*eq2), false);
}


#define CHECK_COMMON_FIELD(o1,o2,c,Field,val1,val2) \
    o1->Set##Field(val1); \
    o2->Reset##Field(); \
    c = o1->MakeCommon(*o2); \
    BOOST_CHECK_EQUAL(c->IsSet##Field(), false); \
    o2->Set##Field(val2); \
    c = o1->MakeCommon(*o2); \
    BOOST_CHECK_EQUAL(c->IsSet##Field(), false); \
    o2->Set##Field(val1); \
    c = o1->MakeCommon(*o2); \
    BOOST_CHECK_EQUAL(c->IsSet##Field(), true); \
    BOOST_CHECK_EQUAL(c->Get##Field(), o1->Get##Field());
    

BOOST_AUTO_TEST_CASE(Test_OrgName_MakeCommon)
{
    CRef<COrgName> on1(new COrgName());
    CRef<COrgName> on2(new COrgName());
    CRef<COrgName> common = on1->MakeCommon(*on2);
    if (common) {
        BOOST_ASSERT("common OrgName should not have been created");
    }

    on1->SetDiv("bacteria");
    common = on1->MakeCommon(*on2);
    if (common) {
        BOOST_ASSERT("common OrgName should not have been created");
    }
    on2->SetDiv("archaea");
    common = on1->MakeCommon(*on2);
    if (common) {
        BOOST_ASSERT("common OrgName should not have been created");
    }
    on2->SetDiv("bacteria");
    common = on1->MakeCommon(*on2);
    BOOST_CHECK_EQUAL(common->GetDiv(), "bacteria");

    // one orgmod on 1, no orgmods on 2, should not add orgmods to common
    CRef<COrgMod> m1(new COrgMod());
    m1->SetSubtype(COrgMod::eSubtype_acronym);
    m1->SetSubname("x");
    on1->SetMod().push_back(m1);
    common = on1->MakeCommon(*on2);
    BOOST_CHECK_EQUAL(common->IsSetMod(), false);
    // one orgmod on 1, one orgmods on 2 of different type, should not add orgmods to common
    CRef<COrgMod> m2(new COrgMod());
    m2->SetSubtype(COrgMod::eSubtype_anamorph);
    m2->SetSubname("x");
    on2->SetMod().push_back(m2);

    common = on1->MakeCommon(*on2);
    BOOST_CHECK_EQUAL(common->IsSetMod(), false);
    // same orgmod on both, should add
    m2->SetSubtype(COrgMod::eSubtype_acronym);
    common = on1->MakeCommon(*on2);
    BOOST_CHECK_EQUAL(common->IsSetMod(), true);
    BOOST_CHECK_EQUAL(common->GetMod().size(), 1);
    BOOST_CHECK_EQUAL(common->GetMod().front()->Equals(*m2), true);

    CHECK_COMMON_FIELD(on1,on2,common,Attrib,"x","y");
    CHECK_COMMON_FIELD(on1,on2,common,Lineage,"x","y");
    CHECK_COMMON_FIELD(on1,on2,common,Gcode,1,2);
    CHECK_COMMON_FIELD(on1,on2,common,Mgcode,3,4);
    CHECK_COMMON_FIELD(on1,on2,common,Pgcode,5,6);

}


#define CHECK_COMMON_STRING_LIST(o1,o2,c,Field,val1,val2) \
    o1->Set##Field().push_back(val1); \
    c = o1->MakeCommon(*o2); \
    BOOST_CHECK_EQUAL(c->IsSet##Field(), false); \
    o2->Set##Field().push_back(val2); \
    c = o1->MakeCommon(*o2); \
    BOOST_CHECK_EQUAL(c->IsSet##Field(), false); \
    o2->Set##Field().push_back(val1); \
    c = o1->MakeCommon(*o2); \
    BOOST_CHECK_EQUAL(c->IsSet##Field(), true); \
    BOOST_CHECK_EQUAL(c->Get##Field().size(), 1); \
    BOOST_CHECK_EQUAL(c->Get##Field().front(), val1);

BOOST_AUTO_TEST_CASE(Test_OrgRef_MakeCommon)
{
    CRef<COrg_ref> org1(new COrg_ref());
    CRef<COrg_ref> org2(new COrg_ref());
    CRef<COrg_ref> common = org1->MakeCommon(*org2);
    if (common) {
        BOOST_ASSERT("common OrgRef should not have been created");
    }
    org1->SetTaxId(1);
    org2->SetTaxId(2);
    common = org1->MakeCommon(*org2);
    if (common) {
        BOOST_ASSERT("common OrgRef should not have been created");
    }

    org2->SetTaxId(1);
    common = org1->MakeCommon(*org2);
    BOOST_CHECK_EQUAL(common->GetTaxId(), 1);
    BOOST_CHECK_EQUAL(common->IsSetTaxname(), false);

    CHECK_COMMON_FIELD(org1,org2,common,Taxname,"A","B");
    CHECK_COMMON_FIELD(org1,org2,common,Common,"A","B");

    CHECK_COMMON_STRING_LIST(org1,org2,common,Mod,"a","b");
    CHECK_COMMON_STRING_LIST(org1,org2,common,Syn,"a","b");

}


BOOST_AUTO_TEST_CASE(Test_BioSource_MakeCommon)
{
    CRef<CBioSource> src1(new CBioSource());
    CRef<CBioSource> src2(new CBioSource());
    CRef<CBioSource> common = src1->MakeCommon(*src2);
    if (common) {
        BOOST_ASSERT("common BioSource should not have been created");
    }

    src1->SetOrg().SetTaxId(1);
    src2->SetOrg().SetTaxId(1);
    common = src1->MakeCommon(*src2);
    BOOST_CHECK_EQUAL(common->GetOrg().GetTaxId(), 1);

    CRef<CSubSource> s1(new CSubSource());
    s1->SetSubtype(CSubSource::eSubtype_altitude);
    s1->SetName("x");
    src1->SetSubtype().push_back(s1);
    common = src1->MakeCommon(*src2);
    BOOST_CHECK_EQUAL(common->IsSetSubtype(), false);

    CRef<CSubSource> s2(new CSubSource());
    s2->SetSubtype(CSubSource::eSubtype_altitude);
    s2->SetName("y");
    src2->SetSubtype().push_back(s2);
    common = src1->MakeCommon(*src2);
    BOOST_CHECK_EQUAL(common->IsSetSubtype(), false);

    s2->SetName("x");
    common = src1->MakeCommon(*src2);
    BOOST_CHECK_EQUAL(common->IsSetSubtype(), true);
    BOOST_CHECK_EQUAL(common->GetSubtype().size(), 1);
    BOOST_CHECK_EQUAL(common->GetSubtype().front()->Equals(*s2), true);

    CHECK_COMMON_FIELD(src1,src2,common,Genome,CBioSource::eGenome_apicoplast,CBioSource::eGenome_chloroplast);
    CHECK_COMMON_FIELD(src1,src2,common,Origin,CBioSource::eOrigin_artificial,CBioSource::eOrigin_mut);
}


BOOST_AUTO_TEST_CASE(Test_Regulatory)
{
    BOOST_CHECK_EQUAL(CSeqFeatData::IsRegulatory(CSeqFeatData::eSubtype_attenuator), true);
    BOOST_CHECK_EQUAL(CSeqFeatData::IsRegulatory(CSeqFeatData::eSubtype_cdregion), false);
    BOOST_CHECK_EQUAL(CSeqFeatData::GetRegulatoryClass(CSeqFeatData::eSubtype_RBS), "ribosome_binding_site");
    BOOST_CHECK_EQUAL(CSeqFeatData::GetRegulatoryClass(CSeqFeatData::eSubtype_terminator), "terminator");
}


BOOST_AUTO_TEST_CASE(Test_RmCultureNotes)
{
    CRef<CSubSource> ss(new CSubSource());
    ss->SetSubtype(CSubSource::eSubtype_other);
    ss->SetName("a; [mixed bacterial source]; b");
    ss->RemoveCultureNotes();
    BOOST_CHECK_EQUAL(ss->GetName(), "a; b");
    ss->SetName("[uncultured (using species-specific primers) bacterial source]");
    ss->RemoveCultureNotes();
    BOOST_CHECK_EQUAL(ss->GetName(), "amplified with species-specific primers");
    ss->SetName("[BankIt_uncultured16S_wizard]; [universal primers]; [tgge]");
    ss->RemoveCultureNotes();
    BOOST_CHECK_EQUAL(ss->IsSetName(), false);
    ss->SetName("[BankIt_uncultured16S_wizard]; [species_specific primers]; [dgge]");
    ss->RemoveCultureNotes();
    BOOST_CHECK_EQUAL(ss->GetName(), "amplified with species-specific primers");

    CRef<CBioSource> src(new CBioSource());
    ss->SetName("a; [mixed bacterial source]; b");
    src->SetSubtype().push_back(ss);
    src->RemoveCultureNotes();
    BOOST_CHECK_EQUAL(ss->GetName(), "a; b");
    ss->SetName("[BankIt_uncultured16S_wizard]; [universal primers]; [tgge]");
    src->RemoveCultureNotes();
    BOOST_CHECK_EQUAL(src->IsSetSubtype(), false);

    ss->SetName("[BankIt_uncultured16S_wizard]; [species_specific primers]; [dgge]");
    ss->RemoveCultureNotes(false);
    BOOST_CHECK_EQUAL(ss->GetName(), "[BankIt_uncultured16S_wizard]; [species_specific primers]; [dgge]");
}

BOOST_AUTO_TEST_CASE(Test_DiscouragedEnums)
{
    // check for enums that pass
    // 
    // make sure to pick subtypes and quals that are
    // very unlikely to be deprecated in the future

    // check for discouraged enums
    BOOST_CHECK(
        CSeqFeatData::IsDiscouragedSubtype(CSeqFeatData::eSubtype_conflict));
    BOOST_CHECK(
        CSeqFeatData::IsDiscouragedQual(CSeqFeatData::eQual_insertion_seq));

    BOOST_CHECK(
        ! CSeqFeatData::IsDiscouragedSubtype(CSeqFeatData::eSubtype_gene));
    BOOST_CHECK(
        ! CSeqFeatData::IsDiscouragedQual(CSeqFeatData::eQual_host));
}


BOOST_AUTO_TEST_CASE(Test_CheckCellLine)
{
    string msg = CSubSource::CheckCellLine("222", "Homo sapiens");
    BOOST_CHECK_EQUAL(msg, "The International Cell Line Authentication Committee database indicates that 222 from Homo sapiens is known to be contaminated by PA1 from Human. Please see http://iclac.org/databases/cross-contaminations/ for more information and references.");

    msg = CSubSource::CheckCellLine("223", "Homo sapiens");
    BOOST_CHECK_EQUAL(msg, "");

    msg = CSubSource::CheckCellLine("222", "Canis familiaris");
    BOOST_CHECK_EQUAL(msg, "");

    msg = CSubSource::CheckCellLine("ARO81-1", "Homo sapiens");
    BOOST_CHECK_EQUAL(msg, "The International Cell Line Authentication Committee database indicates that ARO81-1 from Homo sapiens is known to be contaminated by HT-29 from Human. Please see http://iclac.org/databases/cross-contaminations/ for more information and references.");

    msg = CSubSource::CheckCellLine("aRO81-1", "Homo sapiens");
    BOOST_CHECK_EQUAL(msg, "The International Cell Line Authentication Committee database indicates that aRO81-1 from Homo sapiens is known to be contaminated by HT-29 from Human. Please see http://iclac.org/databases/cross-contaminations/ for more information and references.");
    
}


BOOST_AUTO_TEST_CASE(Test_FixStrain)
{
    BOOST_CHECK_EQUAL(COrgMod::FixStrain("DSM1235"), "DSM 1235");
    BOOST_CHECK_EQUAL(COrgMod::FixStrain("DSM/1235"), "DSM 1235");
    BOOST_CHECK_EQUAL(COrgMod::FixStrain("dsm/1235"), "DSM 1235");
    BOOST_CHECK_EQUAL(COrgMod::FixStrain("dsm:1235"), "DSM 1235");
    BOOST_CHECK_EQUAL(COrgMod::FixStrain("dsm : 1235"), "DSM 1235");

    BOOST_CHECK_EQUAL(COrgMod::FixStrain("ATCC1235"), "ATCC 1235");
    BOOST_CHECK_EQUAL(COrgMod::FixStrain("ATCC/1235"), "ATCC 1235");
    BOOST_CHECK_EQUAL(COrgMod::FixStrain("atcc/1235"), "ATCC 1235");
    BOOST_CHECK_EQUAL(COrgMod::FixStrain("atcc:1235"), "ATCC 1235");
    BOOST_CHECK_EQUAL(COrgMod::FixStrain("atcc : 1235"), "ATCC 1235");
}