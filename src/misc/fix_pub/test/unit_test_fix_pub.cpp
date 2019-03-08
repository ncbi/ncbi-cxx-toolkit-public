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
 * Author:  Alexey Dobronadezhdin
 *
 * File Description:
 *   Unit tests for CPubFixing.
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <corelib/ncbi_message.hpp>
#include <corelib/ncbimisc.hpp>

#include <objects/biblio/Author.hpp>
#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_book.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Cit_proc.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/biblio/Title.hpp>
#include <objects/biblio/ArticleIdSet.hpp>
#include <objects/biblio/ArticleId.hpp>
#include <objects/biblio/PubMedId.hpp>
#include <objects/general/Name_std.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Date_std.hpp>
#include <objects/medline/Medline_entry.hpp>
#include <objects/general/Dbtag.hpp>


#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>


#include "../fix_pub_aux.hpp"

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

BOOST_AUTO_TEST_CASE(Test_IsFromBook)
{
    CCit_art art;

    BOOST_CHECK_EQUAL(fix_pub::IsFromBook(art), false);

    art.SetFrom();
    BOOST_CHECK_EQUAL(fix_pub::IsFromBook(art), false);

    art.SetFrom().SetBook();
    BOOST_CHECK_EQUAL(fix_pub::IsFromBook(art), true);
}

BOOST_AUTO_TEST_CASE(Test_IsInpress)
{
    CCit_art art;

    BOOST_CHECK_EQUAL(fix_pub::IsInpress(art), false);

    art.SetFrom();
    BOOST_CHECK_EQUAL(fix_pub::IsInpress(art), false);

    art.SetFrom().SetBook();
    BOOST_CHECK_EQUAL(fix_pub::IsInpress(art), false);

    art.SetFrom().SetBook().SetImp();
    BOOST_CHECK_EQUAL(fix_pub::IsInpress(art), false);

    art.SetFrom().SetBook().SetImp().SetPrepub(CImprint::ePrepub_in_press);
    BOOST_CHECK_EQUAL(fix_pub::IsInpress(art), true);

    art.SetFrom().SetProc();
    BOOST_CHECK_EQUAL(fix_pub::IsInpress(art), false);

    art.SetFrom().SetProc().SetBook().SetImp().SetPrepub(CImprint::ePrepub_in_press);
    BOOST_CHECK_EQUAL(fix_pub::IsInpress(art), true);

    art.SetFrom().SetJournal();
    BOOST_CHECK_EQUAL(fix_pub::IsInpress(art), false);

    art.SetFrom().SetJournal().SetImp().SetPrepub(CImprint::ePrepub_in_press);
    BOOST_CHECK_EQUAL(fix_pub::IsInpress(art), true);
}

BOOST_AUTO_TEST_CASE(Test_NeedToPropagateInJournal)
{
    CCit_art art;

    BOOST_CHECK_EQUAL(fix_pub::NeedToPropagateInJournal(art), true);

    art.SetFrom();
    BOOST_CHECK_EQUAL(fix_pub::NeedToPropagateInJournal(art), true);

    art.SetFrom().SetBook();
    BOOST_CHECK_EQUAL(fix_pub::NeedToPropagateInJournal(art), true);

    art.SetFrom().SetJournal();
    BOOST_CHECK_EQUAL(fix_pub::NeedToPropagateInJournal(art), true);

    CRef<CTitle::C_E> title(new CTitle::C_E);
    title->SetName("journal");
    art.SetFrom().SetJournal().SetTitle().Set().push_back(title);
    BOOST_CHECK_EQUAL(fix_pub::NeedToPropagateInJournal(art), true);

    art.SetFrom().SetJournal().SetImp().SetVolume("1");
    art.SetFrom().SetJournal().SetImp().SetPages("2");

    art.SetFrom().SetJournal().SetImp().SetDate().SetStd();

    BOOST_CHECK_EQUAL(fix_pub::NeedToPropagateInJournal(art), false);

    art.SetFrom().SetJournal().ResetTitle();
    BOOST_CHECK_EQUAL(fix_pub::NeedToPropagateInJournal(art), true);
}

BOOST_AUTO_TEST_CASE(Test_PropagateInPress)
{
    CCit_art art,
             orig_art;

    fix_pub::PropagateInPress(true, art);
    BOOST_CHECK_EQUAL(orig_art.Equals(art), true);

    art.SetFrom().SetBook();
    orig_art.Assign(art);
    fix_pub::PropagateInPress(true, art);

    BOOST_CHECK_EQUAL(orig_art.Equals(art), false);

    orig_art.SetFrom().SetBook().SetImp().SetPrepub(CImprint::ePrepub_in_press);
    BOOST_CHECK_EQUAL(orig_art.Equals(art), true);

    art.SetFrom().SetJournal();
    orig_art.Assign(art);
    fix_pub::PropagateInPress(true, art);

    BOOST_CHECK_EQUAL(orig_art.Equals(art), false);

    orig_art.SetFrom().SetJournal().SetImp().SetPrepub(CImprint::ePrepub_in_press);
    BOOST_CHECK_EQUAL(orig_art.Equals(art), true);

    art.SetFrom().SetProc().SetBook();
    orig_art.Assign(art);
    fix_pub::PropagateInPress(true, art);

    BOOST_CHECK_EQUAL(orig_art.Equals(art), false);

    orig_art.SetFrom().SetProc().SetBook().SetImp().SetPrepub(CImprint::ePrepub_in_press);
    BOOST_CHECK_EQUAL(orig_art.Equals(art), true);


    art.SetFrom().SetProc().SetMeet();
    orig_art.Assign(art);
    fix_pub::PropagateInPress(true, art);

    BOOST_CHECK_EQUAL(orig_art.Equals(art), true);

    art.SetFrom().SetJournal();
    orig_art.Assign(art);
    fix_pub::PropagateInPress(false, art);

    BOOST_CHECK_EQUAL(orig_art.Equals(art), true);
}


BOOST_AUTO_TEST_CASE(Test_MergeNonPubmedPubIds)
{
    CCit_art orig_art,
             modified_art,
             old_art;

    fix_pub::MergeNonPubmedPubIds(old_art, modified_art);
    BOOST_CHECK_EQUAL(orig_art.Equals(modified_art), true);

    CRef<CArticleId> art_id(new CArticleId);

    // PMID will not be merged
    static const int PMID = 2626;
    art_id->SetPubmed().Set(PMID);
    old_art.SetIds().Set().push_back(art_id);

    fix_pub::MergeNonPubmedPubIds(old_art, modified_art);
    BOOST_CHECK_EQUAL(orig_art.Equals(modified_art), true);

    // Doi ID should be merged
    static const string DOI_ID = "2727";
    art_id.Reset(new CArticleId);
    art_id->SetDoi().Set(DOI_ID);
    old_art.SetIds().Set().push_back(art_id);

    fix_pub::MergeNonPubmedPubIds(old_art, modified_art);
    BOOST_CHECK_EQUAL(orig_art.Equals(modified_art), false);

    orig_art.SetIds().Set().push_front(art_id);
    BOOST_CHECK_EQUAL(orig_art.Equals(modified_art), true);

    // Other ID should be merged
    static const string TEST_DB = "Test DB";
    art_id.Reset(new CArticleId);
    art_id->SetOther().SetDb(TEST_DB);
    old_art.SetIds().Set().push_back(art_id);

    fix_pub::MergeNonPubmedPubIds(old_art, modified_art);
    BOOST_CHECK_EQUAL(orig_art.Equals(modified_art), false);

    orig_art.SetIds().Set().push_front(art_id);
    BOOST_CHECK_EQUAL(orig_art.Equals(modified_art), true);
}


BOOST_AUTO_TEST_CASE(Test_MedlineToISO)
{
    CCit_art art,
             expected_art;

    fix_pub::MedlineToISO(art);
    BOOST_CHECK_EQUAL(expected_art.Equals(art), true);

    // ML list of authors
    art.SetAuthors().SetNames().SetMl().push_back("Doe J");
    art.SetAuthors().SetNames().SetMl().push_back("Author S");

    fix_pub::MedlineToISO(art);

    CRef<CAuthor> author(new CAuthor);
    author->SetName().SetName().SetLast("Doe");
    author->SetName().SetName().SetInitials("J.");
    expected_art.SetAuthors().SetNames().SetStd().push_back(author);

    author.Reset(new CAuthor);
    author->SetName().SetName().SetLast("Author");
    author->SetName().SetName().SetInitials("S.");
    expected_art.SetAuthors().SetNames().SetStd().push_back(author);

    BOOST_CHECK_EQUAL(expected_art.Equals(art), true);


    // Std list of authors with ML format of authors' names
    art.ResetAuthors();

    author.Reset(new CAuthor);
    author->SetName().SetMl("Doe J");
    art.SetAuthors().SetNames().SetStd().push_back(author);

    author.Reset(new CAuthor);
    author->SetName().SetMl("Author S");
    art.SetAuthors().SetNames().SetStd().push_back(author);

    fix_pub::MedlineToISO(art);
    BOOST_CHECK_EQUAL(expected_art.Equals(art), true);

    // Cit_art is from a journal
    CRef<CTitle::C_E> title(new CTitle::C_E);
    title->SetName("Nature");
    art.SetFrom().SetJournal().SetTitle().Set().push_back(title);

    title.Reset(new CTitle::C_E);
    title->SetIso_jta("Nature");
    expected_art.SetFrom().SetJournal().SetTitle().Set().push_back(title);

    fix_pub::MedlineToISO(art);
    BOOST_CHECK_EQUAL(expected_art.Equals(art), true);


    // MedlineToISO also removes the language if it is "Eng"
    art.SetFrom().SetJournal().SetImp().SetLanguage("Eng");
    fix_pub::MedlineToISO(art);

    BOOST_CHECK_EQUAL(expected_art.Equals(art), true);
}


BOOST_AUTO_TEST_CASE(Test_SplitMedlineEntry)
{
    CPub_equiv::Tdata medlines;
    CRef<CPub> pub(new CPub);

    // Set medline
    static const int TEST_PMID = 1;
    pub->SetMedline().SetCit().SetAuthors().SetNames().SetMl().push_back("Doe J");

    CRef<CTitle::C_E> title(new CTitle::C_E);
    title->SetName("Nature");
    pub->SetMedline().SetCit().SetFrom().SetJournal().SetTitle().Set().push_back(title);
    pub->SetMedline().SetPmid().Set(TEST_PMID);

    medlines.push_back(pub);

    fix_pub::SplitMedlineEntry(medlines);

    // medlines should contain two items now
    BOOST_CHECK_EQUAL(medlines.size(), 2);

    if (medlines.size() == 2) {
        // first one is CPub->pmid
        auto it = medlines.begin();

        pub.Reset(new CPub);
        pub->SetPmid().Set(1);
        BOOST_CHECK_EQUAL((*it)->Equals(*pub), true);

        // second one is CPub->cit-art
        ++it;
        pub.Reset(new CPub);
        title.Reset(new CTitle::C_E);
        title->SetIso_jta("Nature");
        pub->SetArticle().SetFrom().SetJournal().SetTitle().Set().push_back(title);

        CRef<CAuthor> author(new CAuthor);
        author->SetName().SetName().SetLast("Doe");
        author->SetName().SetName().SetInitials("J.");
        pub->SetArticle().SetAuthors().SetNames().SetStd().push_back(author);
        BOOST_CHECK_EQUAL((*it)->Equals(*pub), true);
    }
}

BOOST_AUTO_TEST_CASE(Test_MULooksLikeISSN)
{
    BOOST_CHECK_EQUAL(fix_pub::MULooksLikeISSN("1234-1234"), true);
    BOOST_CHECK_EQUAL(fix_pub::MULooksLikeISSN("1234-123X"), true);

    BOOST_CHECK_EQUAL(fix_pub::MULooksLikeISSN("X234-1234"), false);
    BOOST_CHECK_EQUAL(fix_pub::MULooksLikeISSN("123-41234"), false);
    BOOST_CHECK_EQUAL(fix_pub::MULooksLikeISSN("123341234"), false);
}

BOOST_AUTO_TEST_CASE(Test_MUIsJournalIndexed)
{
    BOOST_CHECK_EQUAL(fix_pub::MUIsJournalIndexed("Nature (Reviews Molecular Cell Biology)"), true);
    BOOST_CHECK_EQUAL(fix_pub::MUIsJournalIndexed("Molecular Cell"), true);
    BOOST_CHECK_EQUAL(fix_pub::MUIsJournalIndexed("Genome Biology."), true);

    BOOST_CHECK_EQUAL(fix_pub::MUIsJournalIndexed("Nature"), false); // Too many entries found
    BOOST_CHECK_EQUAL(fix_pub::MUIsJournalIndexed("Fake journal"), false);
    BOOST_CHECK_EQUAL(fix_pub::MUIsJournalIndexed("Journal (which does not exist)"), false);
}

struct STestErrorText
{
    fix_pub::EFixPubErrorCategory m_err_code;
    int m_err_subcode;
    EDiagSev m_severity;

    string m_text;
};

void CheckPrintPubProblems(const IMessageListener& log, const STestErrorText* expected)
{
    size_t num_of_errors = log.Count();
    for (size_t i = 0; i < num_of_errors; ++i) {
        const IMessage& msg = log.GetMessage(i);

        BOOST_CHECK_EQUAL(msg.GetCode(), expected[i].m_err_code);
        BOOST_CHECK_EQUAL(msg.GetSubCode(), expected[i].m_err_subcode);
        BOOST_CHECK_EQUAL(msg.GetSeverity(), expected[i].m_severity);
        BOOST_CHECK_EQUAL(msg.GetText(), expected[i].m_text);
    }
}

BOOST_AUTO_TEST_CASE(Test_PrintPub)
{
    CCit_art art;

    CMessageListener_Basic log;
    fix_pub::PrintPub(art, false, false, 0, &log);

    static const STestErrorText expected_1[] =
    {
        { fix_pub::err_Print, fix_pub::err_Print_Failed, eDiag_Warning, "Authors NULL" },
        { fix_pub::err_Reference, fix_pub::err_Reference_NoPmidJournalNotInPubMed, eDiag_Warning, " |journal unknown|(0)|no volume number|no page number" }
    };

    CheckPrintPubProblems(log, expected_1);

    log.Clear();
    CRef<CAuthor> author(new CAuthor);
    author->SetName().SetName().SetLast("Doe");
    author->SetName().SetName().SetInitials("J.");
    art.SetAuthors().SetNames().SetStd().push_back(author);

    fix_pub::PrintPub(art, false, false, 0, &log);

    static const STestErrorText expected_2[] =
    {
        { fix_pub::err_Reference, fix_pub::err_Reference_NoPmidJournalNotInPubMed, eDiag_Warning, "Doe J.|journal unknown|(0)|no volume number|no page number" }
    };

    CheckPrintPubProblems(log, expected_2);

    log.Clear();
    art.SetAuthors().SetNames().SetStr().push_back("Doe J");
    CRef<CTitle::C_E> title(new CTitle::C_E);
    title->SetName("Molecular Cell");
    art.SetFrom().SetJournal().SetTitle().Set().push_back(title);

    fix_pub::PrintPub(art, false, false, 0, &log);

    static const STestErrorText expected_3[] =
    {
        { fix_pub::err_Reference, fix_pub::err_Reference_PmidNotFound, eDiag_Warning, "Doe J |Molecular Cell|(0)|no volume number|no page number" }
    };

    CheckPrintPubProblems(log, expected_3);

    log.Clear();
    art.SetFrom().SetJournal().SetImp().SetDate().SetStd().SetYear(2010);
    art.SetFrom().SetJournal().SetImp().SetVolume("1");
    art.SetFrom().SetJournal().SetImp().SetPages("15");

    fix_pub::PrintPub(art, false, false, 0, &log);

    static const STestErrorText expected_4[] =
    {
        { fix_pub::err_Reference, fix_pub::err_Reference_PmidNotFound, eDiag_Warning, "Doe J |Molecular Cell|(2010)|1|15" }
    };

    CheckPrintPubProblems(log, expected_4);

    log.Clear();
    art.SetFrom().SetJournal().SetImp().SetPrepub(CImprint::ePrepub_in_press);
    fix_pub::PrintPub(art, false, false, 0, &log);

    static const STestErrorText expected_5[] =
    {
        { fix_pub::err_Reference, fix_pub::err_Reference_OldInPress, eDiag_Warning, "encountered in-press article more than 2 years old: Doe J |Molecular Cell|(2010)|1|15" },
        { fix_pub::err_Reference, fix_pub::err_Reference_PmidNotFoundInPress, eDiag_Warning, "Doe J |Molecular Cell|(2010)|1|15" }
    };

    CheckPrintPubProblems(log, expected_5);

    log.Clear();
}

BOOST_AUTO_TEST_CASE(Test_TenAuthorsCompare)
{
    CCit_art art_new,
             art_old;

    art_new.SetAuthors().SetNames().SetStr().push_back("Doe John");
    art_old.Assign(art_new);

    BOOST_CHECK_EQUAL(fix_pub::TenAuthorsCompare(art_old, art_new), true);

    art_new.SetAuthors().SetNames().SetStr().push_back("First Author");
    BOOST_CHECK_EQUAL(fix_pub::TenAuthorsCompare(art_old, art_new), true);

    art_new.SetAuthors().SetNames().SetStr().push_back("Second Author");
    art_new.SetAuthors().SetNames().SetStr().push_back("Forth Author");
    art_new.SetAuthors().SetNames().SetStr().push_back("Fifth Author");

    art_old.SetAuthors().SetNames().SetStr().push_back("Sixth Author");
    art_old.SetAuthors().SetNames().SetStr().push_back("Seventh Author");
    art_old.SetAuthors().SetNames().SetStr().push_back("Eighth Author");
    art_old.SetAuthors().SetNames().SetStr().push_back("Ninth Author");
    BOOST_CHECK_EQUAL(fix_pub::TenAuthorsCompare(art_old, art_new), false);


    // make the list of authors > 10
    art_new.SetAuthors().SetNames().SetStr().push_back("a b");
    art_new.SetAuthors().SetNames().SetStr().push_back("c d");
    art_new.SetAuthors().SetNames().SetStr().push_back("e f");
    art_new.SetAuthors().SetNames().SetStr().push_back("g h");
    art_new.SetAuthors().SetNames().SetStr().push_back("i j");
    art_new.SetAuthors().SetNames().SetStr().push_back("ii jj");

    art_old.SetAuthors().SetNames().SetStr().push_back("First Author");
    art_old.SetAuthors().SetNames().SetStr().push_back("a b");
    art_old.SetAuthors().SetNames().SetStr().push_back("c d");
    art_old.SetAuthors().SetNames().SetStr().push_back("e f");
    art_old.SetAuthors().SetNames().SetStr().push_back("g h");
    art_old.SetAuthors().SetNames().SetStr().push_back("i j");
    art_old.SetAuthors().SetNames().SetStr().push_back("ii jj");

    CCit_art expected;
    expected.Assign(art_old);

    BOOST_CHECK_EQUAL(fix_pub::TenAuthorsCompare(art_old, art_new), true);
    BOOST_CHECK_EQUAL(art_old.IsSetAuthors(), false);
    BOOST_CHECK_EQUAL(art_new.GetAuthors().Equals(expected.GetAuthors()), true);
}
