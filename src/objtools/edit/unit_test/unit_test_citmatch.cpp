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
 * Author: Vitaly Stakhovsky, NCBI
 *
 * File Description:
 *  Test CitMatch implementations: MedArch vs EUtils
 *
 */

#include <ncbi_pch.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <objects/general/Date_std.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/pub/Pub.hpp>

#include <objtools/edit/eutils_updater.hpp>

#include <array>

#include <common/test_assert.h> /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(edit);

static CEUtilsUpdater upd_eutils;

static array<IPubmedUpdater*, 1> updaters = { &upd_eutils };

BOOST_AUTO_TEST_CASE(Test_EmptyASN)
{
    static const string TEST_PUB = R"(
Pub ::= article {
  from journal {
    title {
    },
    imp {
      date std {
        year 0
      }
    }
  }
}
)";

    CPub pub;
    TEST_PUB >> pub;

    for (auto upd : updaters) {
        TEntrezId pmid = upd->CitMatch(pub);
        BOOST_CHECK_EQUAL(pmid, ZERO_ENTREZ_ID);
    }
}

BOOST_AUTO_TEST_CASE(Test_Empty)
{
    CPub  pub;
    auto& J = pub.SetArticle().SetFrom().SetJournal();
    J.SetTitle();
    J.SetImp().SetDate().SetStd().SetYear(0);

    for (auto upd : updaters) {
        EPubmedError err;
        TEntrezId    pmid = upd->CitMatch(pub, &err);
        BOOST_CHECK_EQUAL(pmid, ZERO_ENTREZ_ID);
        BOOST_CHECK_EQUAL(err, EPubmedError::citation_not_found);
    }
}

BOOST_AUTO_TEST_CASE(Test1_ASN)
{
    static const string TEST_PUB = R"(
Pub ::= article {
  from journal {
    title {
      name "Virus Res."
    },
    imp {
      date std {
        year 2007
      },
      volume "130",
      pages "162-166"
    }
  }
}
)";

    CPub pub;
    TEST_PUB >> pub;

    for (auto upd : updaters) {
        TEntrezId pmid = upd->CitMatch(pub);
        BOOST_CHECK_EQUAL(pmid, ENTREZ_ID_CONST(17'659'802));
    }
}

BOOST_AUTO_TEST_CASE(Test1)
{
    SCitMatch cm;
    cm.Journal = "Virus Res.";
    cm.Year    = "2007";
    cm.Volume  = "130";
    cm.Page    = "162-166";

    for (auto upd : updaters) {
        TEntrezId pmid = upd->CitMatch(cm);
        BOOST_CHECK_EQUAL(pmid, ENTREZ_ID_CONST(17'659'802));
    }
}

BOOST_AUTO_TEST_CASE(Test_Known_PMID)
{
    TEntrezId pmid = ENTREZ_ID_CONST(12'345);
    for (auto upd : updaters) {
        auto      pub      = upd->GetPub(pmid);
        TEntrezId pmid_new = upd->CitMatch(*pub);
        BOOST_CHECK_EQUAL(pmid_new, pmid);
    }
}

BOOST_AUTO_TEST_CASE(Test2)
{
    SCitMatch cm;
    cm.Journal = "Science";
    cm.Year    = "1996";
    cm.Volume  = "291";
    cm.Page    = "252";

    for (auto upd : updaters) {
        TEntrezId pmid = upd->CitMatch(cm);
        BOOST_CHECK_EQUAL(pmid, ENTREZ_ID_CONST(11'253'208));
    }
}

BOOST_AUTO_TEST_CASE(Test3)
{
    SCitMatch cm;
    cm.Journal = "Genome Announc";
    cm.Title   = "Genome Sequence of the Yeast Cyberlindnera fabianii (Hansenula fabianii)";
    cm.Author  = "Freel KC";

    for (auto upd : updaters) {
        TEntrezId pmid = upd->CitMatch(cm);
        BOOST_CHECK_EQUAL(pmid, ENTREZ_ID_CONST(25'103'752));
    }
}

BOOST_AUTO_TEST_CASE(Test4)
{
    SCitMatch cm;
    cm.Journal = "J.Gen.Virol.";
    cm.Title   = "Human herpesvirus 6 (strain U1102) encodes homologues of the conserved herpesvirus glycoprotein gM and the alphaherpesvirus origin-binding protein";
    cm.Author  = "Lawrence GL";
    cm.Year    = "1994";
    cm.Volume  = "0";

    for (auto upd : updaters) {
        EPubmedError err;

        cm.InPress = false;
        TEntrezId pmid = upd->CitMatch(cm);
        BOOST_CHECK_EQUAL(pmid, ENTREZ_ID_CONST(7'844'524));

        cm.InPress = true;
        pmid = upd->CitMatch(cm, &err);
        BOOST_CHECK_EQUAL(pmid, ZERO_ENTREZ_ID);
    }
}

BOOST_AUTO_TEST_CASE(Test5)
{
    SCitMatch cm;
    cm.Journal = "J. Mol. Evol.";
    cm.Volume  = "57";
    cm.Page    = "3";
    cm.Year    = "2003";
    cm.Author  = "Nilsson MA";
    cm.Title   = "Radiation of marsupials after the K/T boundary: evidence from complete mitochondrial genomes";

    for (auto upd : updaters) {
        TEntrezId pmid = upd->CitMatch(cm);
        BOOST_CHECK_EQUAL(pmid, ENTREZ_ID_CONST(15'008'398));
    }
}

BOOST_AUTO_TEST_CASE(HF933230)
{
    SCitMatch cm;
    cm.Journal = "G3 (Bethesda)";
    cm.Volume  = "4";
    cm.Page    = "433";
    cm.Year    = "2014";
    cm.Author  = "Spivakov M";
    cm.Issue   = "3";
    cm.Title   = "Genomic and phenotypic characterization of a wild medaka population: towards the establishment of an isogenic population genetic resource in fish";

    for (auto upd : updaters) {
        TEntrezId pmid = upd->CitMatch(cm);
        BOOST_CHECK_EQUAL(pmid, ENTREZ_ID_CONST(24'408'034));
    }
}

BOOST_AUTO_TEST_CASE(AJ251216)
{
    SCitMatch cm;
    cm.Journal = "Genomics";
    cm.Title   = "Characterization of the human analogue of a scrapie-responsive gene";
    cm.Author  = "Dron M";
    cm.Year    = "2000";
    cm.Volume  = "703";
    cm.Page    = "140";

    for (auto upd : updaters) {
        TEntrezId pmid = upd->CitMatch(cm);
        BOOST_CHECK_EQUAL(pmid, ENTREZ_ID_CONST(11'087'671));
    }
}

BOOST_AUTO_TEST_CASE(Test_Hydra)
{
    SCitMatch cm;
    cm.Author = "Wang JX";
    cm.Title = "Beta-Arrestins negatively regulate the Toll pathway in "
        "shrimp by preventing Dorsal translocation and inhibiting Dorsal "
        "transcriptional activity";
    cm.Option1 = true;

    
    TEntrezId pmid = upd_eutils.CitMatch(cm);
    BOOST_CHECK_EQUAL(pmid, ENTREZ_ID_CONST(26846853));
}

