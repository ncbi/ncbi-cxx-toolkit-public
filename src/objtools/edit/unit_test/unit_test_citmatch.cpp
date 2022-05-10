#include <ncbi_pch.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <objects/general/Date.hpp>
#include <objects/general/Date_std.hpp>
#include <objects/biblio/Cit_art.hpp>
#include <objects/biblio/Cit_jour.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/pub/Pub.hpp>

#include <objtools/edit/mla_updater.hpp>
#include <objtools/edit/eutils_updater.hpp>

#include <array>

#include <common/test_assert.h> /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(edit);

static CMLAUpdater    upd_mla;
static CEUtilsUpdater upd_eutils;

static array<IPubmedUpdater*, 2> updaters = { &upd_mla, &upd_eutils };

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
        TEntrezId pmid = upd->CitMatch(pub);
        BOOST_CHECK_EQUAL(pmid, ZERO_ENTREZ_ID);
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
    string title = "Virus Res.";
    int    year  = 2007;
    int    vol   = 130;
    int    page  = 162;

    CPub pub;
    {
        CCit_art& A = pub.SetArticle();
        {
            CCit_jour& J = A.SetFrom().SetJournal();
            {
                CTitle& T = J.SetTitle();
                {
                    CRef<CTitle::C_E> t(new CTitle::C_E);
                    t->SetName(title);
                    T.Set().push_back(t);
                }
            }
            {
                CImprint& I = J.SetImp();
                {
                    CDate& D = I.SetDate();
                    D.SetStd().SetYear(year);
                }
                I.SetVolume(to_string(vol));
                I.SetPages(to_string(page));
            }
        }
    }

    for (auto upd : updaters) {
        TEntrezId pmid = upd->CitMatch(pub);
        BOOST_CHECK_EQUAL(pmid, ENTREZ_ID_CONST(17'659'802));
    }
}

BOOST_AUTO_TEST_CASE(Test_Known_PMID)
{
    TEntrezId pmid = ENTREZ_ID_CONST(12'345);
    for (auto upd : updaters) {
        auto      pub      = upd->GetPub(pmid);
        TEntrezId pmid_new = upd->CitMatch(*pub);
        BOOST_CHECK_EQUAL(pmid, pmid_new);
    }
}

BOOST_AUTO_TEST_CASE(Test2_ASN)
{
    static const string TEST_PUB = R"(
Pub ::= article {
  from journal {
    title {
      name "Science"
    },
    imp {
      date std {
        year 1996
      },
      volume "291",
      pages "252"
    }
  }
}
)";

    CPub pub;
    TEST_PUB >> pub;

    for (auto upd : updaters) {
        TEntrezId pmid = upd->CitMatch(pub);
        BOOST_CHECK_EQUAL(pmid, ENTREZ_ID_CONST(11'253'208));
    }
}
