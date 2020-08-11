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
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/RNA_gen.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Prot_ref.hpp>
#include <objects/seqloc/Seq_id.hpp>
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

static void SetSubSource (objects::CBioSource& src, objects::CSubSource::TSubtype subtype, string val)
{
    if (NStr::IsBlank(val)) {
        if (src.IsSetSubtype()) {
            objects::CBioSource::TSubtype::iterator it = src.SetSubtype().begin();
            while (it != src.SetSubtype().end()) {
                if ((*it)->IsSetSubtype() && (*it)->GetSubtype() == subtype) {
                    it = src.SetSubtype().erase(it);
                } else {
                    ++it;
                }
            }
        }
    } else {
        CRef<objects::CSubSource> sub(new objects::CSubSource(subtype, val));
        if (NStr::EqualNocase(val, "true")) {
            sub->SetName("");
        }
        src.SetSubtype().push_back(sub);
    }
}

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
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("114 degrees 51 degrees 56' 51 ''", true), "51.9475 N 114 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("114 14' 51", true), "51 N 114.23 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("114 degrees, 51 degrees 56' 51 ''", true), "51.9475 N 114 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("114 degrees 14' 42'' 51 degrees 56' 51 ''", true), "51.9475 N 114.2450 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("114 degrees 14' 51 degrees 56' 51 ''", true), "51.9475 N 114.23 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("114 degrees 14' 42'', 51 degrees 56' 51 ''", true), "51.9475 N 114.2450 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("114 degrees 14', 51 degrees 56' 51 ''", true), "51.9475 N 114.23 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("114 14' 42'' 51 56' 51''", true), "51.9475 N 114.2450 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("114 14' 42'' 51 56'", true), "51.93 N 114.2450 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("114 14' 42'' 51", true), "51 N 114.2450 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("114 51", true), "51 N 114 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("114 14' 51 56'", true), "51.93 N 114.23 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("114 14' 42'' 51 56'", true), "51.93 N 114.2450 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("114 42'' 51 56'", true), "51.93 N 114.0117 E");

    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("+60.500280-145.866244", true), "60.500280 N 145.866244 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("-60.500280+145.866244", true), "60.500280 S 145.866244 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("-60.500280-145.866244", true), "60.500280 S 145.866244 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("+60.500280+145.866244", true), "60.500280 N 145.866244 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("\"36.222459,139.636522\"", true), "36.222459 N 139.636522 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("35 57' 18'' N 79 43' 34\" W", true), "35.9550 N 79.7261 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("35 57' 18'' N 79 43' 34' W", true), "35.9550 N 79.7261 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("37:22N 005:59W", true), "37.37 N 5.98 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("60.42.726'N, 05.05.595E", true), "");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("15.48 N -90.23 W", true), "15.48 N 90.23 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("25 . 46 N 108 . 59 W", false), "25.46 N 108.59 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("30.1.49N31.11.44E", false), "30.0303 N 31.1956 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("27 degrees 22'50'' N 88 degrees 13'16'' E", false), "27.3806 N 88.2211 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("27 degrees 22 50  N 88 degrees 13 16 E", false), "27.3806 N 88.2211 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("27 degrees 22'50 N 88 degrees 13'16 E", false), "27.3806 N 88.2211 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("34 deg 00.465 N 77 deg 41.514 E", false), "34.00775 N 77.69190 E");

    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("-50.4498 N - 59 74.8912 E", true), "");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("50.4498 N 59 74.8912 E", true), "");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("9.93N\xC2\xB0 and 78.12\xC2\xB0\x45", true), "9.93 N 78.12 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("N03'00'12.1 E101'39'33'1", true), "");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("43.098333, -89.405278", true), "43.098333 N 89.405278 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("43.098333, -91.00231", true), "43.098333 N 91.00231 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("14.60085 and 144.77629", true), "14.60085 N 144.77629 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("53.43.20 N 7.43.20 E", true), "53.7222 N 7.7222 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("53.43.20.30 N 7.43.20.6 E", true), "");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("91.00 N 5.00 E", true), "");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("83.00 N 181.00 E", true), "");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("91.00 S 5.00 E", true), "");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("83.00 S 181.00 W", true), "");

    string degree = "\xB0";
    string to_fix = "9.93" + degree + "N and 78.12" + degree + "E";
    string fixed = CSubSource::FixLatLonFormat(to_fix, true);
    BOOST_CHECK_EQUAL(fixed, "9.93 N 78.12 E");

    to_fix = "Lattitude: 25.790544; longitude: -80.214930";
    fixed = CSubSource::FixLatLonFormat(to_fix, true);
    BOOST_CHECK_EQUAL(fixed, "25.790544 N 80.214930 W");

    to_fix = "34.1030555556 , -118.357777778  34 degrees 6' 11'' North , 118 degrees 21' 28'' West";
    fixed = CSubSource::FixLatLonFormat(to_fix, true);
    BOOST_CHECK_EQUAL(fixed, "");

    to_fix = "0031.02 N 00.01E";
    fixed = CSubSource::FixLatLonFormat(to_fix, true);
    BOOST_CHECK_EQUAL(fixed, "31.02 N 0.01 E");

    to_fix = "000.00 N 0.12 E";
    fixed = CSubSource::FixLatLonFormat(to_fix, true);
    BOOST_CHECK_EQUAL(fixed, "0.00 N 0.12 E");

    to_fix = "0.0023 N 0 E";
    fixed = CSubSource::FixLatLonFormat(to_fix, true);
    BOOST_CHECK_EQUAL(fixed, "0.0023 N 0 E");

    to_fix = "78.32 -0092.25";
    fixed = CSubSource::FixLatLonFormat(to_fix, true);
    BOOST_CHECK_EQUAL(fixed, "78.32 N 92.25 W");

    to_fix = "-0008.34 0.85";
    fixed = CSubSource::FixLatLonFormat(to_fix, true);
    BOOST_CHECK_EQUAL(fixed, "8.34 S 0.85 E");

    to_fix = "0.067682S_76.39885W";
    fixed = CSubSource::FixLatLonFormat(to_fix, true);
    BOOST_CHECK_EQUAL(fixed, "0.067682 S 76.39885 W");

    to_fix = "34degrees 20' 13'' N,47degrees 03' 24''E";
    fixed = CSubSource::FixLatLonFormat(to_fix, true);
    BOOST_CHECK_EQUAL(fixed, "34.3369 N 47.0567 E");

    to_fix = "34degrees 20' 13' N,47deg 03' 24' E";
    fixed = CSubSource::FixLatLonFormat(to_fix, true);
    BOOST_CHECK_EQUAL(fixed, "34.3369 N 47.0567 E");

    to_fix = "8 degrees 28 'N & 77 degrees 41 'E";
    fixed = CSubSource::FixLatLonFormat(to_fix, true);
    BOOST_CHECK_EQUAL(fixed, "8.47 N 77.68 E");

    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("0 08 36.0S 63 49 00.0W", true), "0.14333 S 63.81667 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("0 08 36.1S 63 49 00.1W", true), "0.14336 S 63.81669 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("0 12 45.5N 50 58 21.7W", true), "0.21264 N 50.97269 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("0 17 28.4N 50 54 07.2W", true), "0.29122 N 50.90200 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("0 4 60.0S 63 07 60.0W", true), "");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("0 46 56.6S 60 03 37.7W", true), "0.78239 S 60.06047 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("0 50 08.0N 51 12 31.4W", true), "0.83556 N 51.20872 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("0 53 38.85N 52 0 40.37W", true), "0.894125 N 52.011214 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("1 00 07.2S 57 07 35.6W", true), "1.00200 S 57.12656 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("1 30 59.1N 50 55 01.7W", true), "1.51642 N 50.91714 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("1 46 56.6S 60 03 37.7W", true), "1.78239 S 60.06047 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("10 0 30.21S 67 50 46.22W", true), "10.008392 S 67.846172 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("10 14 14.05S 67 48 42.93W", true), "10.237236 S 67.811925 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("2 46 56.6S 60 03 37.7W", true), "2.78239 S 60.06047 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("3 53 07.9S 59 04 41.7W", true), "3.88553 S 59.07825 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("7 39 52.3S 65 04 11.1W", true), "7.66453 S 65.06975 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("8 43 5.46S 63 51 24.49W", true), "8.718183 S 63.856803 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("9 30 3.81S 68 53 46.38W", true), "9.501058 S 68.896217 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("23.3600 degree N, 92.0000 degree E", true), "23.3600 N 92.0000 E");

    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("42:43:13N 01:0015W", true), "42.7203 N 1.25 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("42:24:37.9 N 85:22:11.7 W", true), "42.41053 N 85.36992 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("41deg30'' S 145deg37' E", true), "41.0083 S 145.62 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("38 11 44.66 North 0 35 01.93 West", true), "38.195739 N 0.583869 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("37deg27N 121deg52'W", true), "37.45 N 121.87 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("01deg31'25''N 66''33'31''W", true), "");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("07deg33'30''N 69deg20'W", true), "7.5583 N 69.33 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("10.8439,-85.6138", true), "10.8439 N 85.6138 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("11.03,-85.527", true), "11.03 N 85.527 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("35deg48'50'' N; 82deg5658'' W", true), "");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("45deg34.18''N, 122deg12.00 'W", true), "45.009494 N 122.2000 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("37deg27N, 121deg52'W", true), "37.45 N 121.87 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("02 deg 28' 29# S, 56 deg 6' 31# W", true), "2.4747 S 56.1086 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("86 deg. 30'S, 147 deg. W", true), "86.50 S 147 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("E148deg44.8',N19deg24.1", true), "19.402 N 148.747 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("-0.000 N 2.415 W", true),  "0.000 N 2.415 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("N 22.122 E 106.733", true),  "22.122 N 106.733 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("45#42'N 13#42'E", true),  "45.70 N 13.70 E");

    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("lat 48 deg 32'N, long 58 deg 33'W", true),   "48.53 N 58.55 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("N 42deg 23.31'; W 111deg 35.12'", true),   "42.3885 N 111.5853 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("37.9 N lat, 85.8 W long", true),   "37.9 N 85.8 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("37.918 degrees N latitude, 119.258 degrees W longitude", true),   "37.918 N 119.258 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("02 37'S, 38 09'E", true),   "2.62 S 38.15 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("53 deg 23'N 167 deg 50W", true),   "53.38 N 167.83 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("E148deg44.8',N19deg24.1'", true),   "19.402 N 148.747 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("N 22 14; W 60 29 28", true),   "22.23 N 60.4911 W");

    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("35 degrees 29'N; 120 degrees 49-51'W", true),   "");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("44 deg N 71 deg 30'W", true),   "44 N 71.50 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("64 deg 41-43'N 165 deg 45'W", true),   "");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("N 38deg 25.93'; Wdeg 123 04.88'", true),   "38.4322 N 123.0813 W");

    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("51 56' 51'' -114 14' 42''", true), "51.9475 N 114.2450 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("-51 56' 51'' 114 14' 42''", true), "51.9475 S 114.2450 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("51 -56' 51'' 114 14' 42''", true), "");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("0 56' 51'' -114 14' 42''", true), "0.9475 N 114.2450 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("-0 56' 51'' -114 14' 42''", true), "0.9475 S 114.2450 W");

    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("0.000 S 2.417 W", true), "0.000 N 2.417 W");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("N 16 degree 46 min 57 sec; E 99 degree 01 min 08 sec", true), "16.7825 N 99.0189 E");
    BOOST_CHECK_EQUAL(CSubSource::FixLatLonFormat("N 16 degree 30 min 16 sec, E 99 degree 09 min 40 sec", true), "16.5044 N 99.1611 E");

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
    pub2->SetGen().SetMuid(ENTREZ_ID_FROM(TIntId, 40));
    BOOST_CHECK_EQUAL(pub1->SameCitation(*pub2), false);
    pub2->Assign(*pub1);
    pub1->SetGen().SetMuid(ENTREZ_ID_FROM(TIntId, 42));
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
    pmid2->SetPmid().Set(ENTREZ_ID_FROM(TIntId, 4));
    eq2->Set().push_back(pmid2);
    BOOST_CHECK_EQUAL(eq1->SameCitation(*eq2), true);

    // but not if first has different article
    s_ChangeAuthorLastName(eq1->Set().front()->SetArticle().SetAuthors());
    BOOST_CHECK_EQUAL(eq1->SameCitation(*eq2), false);

    // won't match even if PubMed IDs match
    CRef<CPub> pmid1(new CPub());
    pmid1->SetPmid().Set(ENTREZ_ID_FROM(TIntId, 4));
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
    org1->SetTaxId(TAX_ID_CONST(1));
    org2->SetTaxId(TAX_ID_CONST(2));
    common = org1->MakeCommon(*org2);
    if (common) {
        BOOST_ASSERT("common OrgRef should not have been created");
    }

    org2->SetTaxId(TAX_ID_CONST(1));
    common = org1->MakeCommon(*org2);
    BOOST_CHECK_EQUAL(common->GetTaxId(), TAX_ID_CONST(1));
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

    src1->SetOrg().SetTaxId(TAX_ID_CONST(1));
    src2->SetOrg().SetTaxId(TAX_ID_CONST(1));
    common = src1->MakeCommon(*src2);
    BOOST_CHECK_EQUAL(common->GetOrg().GetTaxId(), TAX_ID_CONST(1));

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

BOOST_AUTO_TEST_CASE(Test_BioSource_GetRepliconName_CXX_10657)
{
    CRef<CBioSource> src1(new CBioSource());

    src1->SetOrg().SetTaxId(TAX_ID_CONST(1));

    SetSubSource(*src1,CSubSource::eSubtype_altitude,"X");
    BOOST_CHECK_EQUAL(src1->IsSetSubtype(), true);
    // chromosome-name
    SetSubSource(*src1,CSubSource::eSubtype_chromosome,"X");
    BOOST_CHECK_EQUAL(src1->GetRepliconName(), "X");
    // remove chromosome setting
    src1->ResetSubtype();
    // Plasmid-name
    SetSubSource(*src1,CSubSource::eSubtype_plasmid_name,"plasmid1");
    BOOST_CHECK_EQUAL(src1->GetRepliconName(), "plasmid1");
    src1->ResetSubtype();

    // Plastid-name
    SetSubSource(*src1,CSubSource::eSubtype_plastid_name,"pltd1");
    BOOST_CHECK_EQUAL(src1->GetRepliconName(), "pltd1");
    src1->ResetSubtype();

    // endogenous-virus-name
    SetSubSource(*src1,CSubSource::eSubtype_endogenous_virus_name,"virus1");
    BOOST_CHECK_EQUAL(src1->GetRepliconName(), "virus1");
    src1->ResetSubtype();

    // linkage-group
    SetSubSource(*src1,CSubSource::eSubtype_linkage_group,"LG2");
    src1->SetGenome(CBioSource_Base::eGenome_chromosome);
    BOOST_CHECK_EQUAL(src1->GetRepliconName(), "LG2");

    // cleanup for next test
    src1->ResetSubtype();
    src1->ResetGenome();

    // segment
    // {
    //   subtype segment,
    //   name "DNA-U1"
    // }
    src1->SetOrg().SetOrgname().SetLineage("Viruses; ssDNA viruses; Nanoviridae; Nanovirus");
    SetSubSource(*src1,CSubSource::eSubtype_segment,"DNA-U1");
    BOOST_CHECK_EQUAL(src1->GetRepliconName(), "DNA-U1");
    // reset for next tests
    src1->ResetSubtype();
    src1->ResetGenome();

    src1->SetGenome(CBioSource::eGenome_unknown);
    SetSubSource(*src1,CSubSource::eSubtype_insertion_seq_name,"insername");
    BOOST_CHECK_EQUAL(NStr::IsBlank(src1->GetRepliconName()), true);
    src1->ResetSubtype();
    src1->ResetGenome();
    //
    // default values
    //
    src1->SetGenome(CBioSource::eGenome_plasmid);
    BOOST_CHECK_EQUAL(src1->GetRepliconName(), "unnamed");
    src1->SetGenome(CBioSource::eGenome_plasmid_in_mitochondrion);
    BOOST_CHECK_EQUAL(src1->GetRepliconName(), "unnamed");
    src1->SetGenome(CBioSource::eGenome_plasmid_in_plastid);
    BOOST_CHECK_EQUAL(src1->GetRepliconName(), "unnamed");
    // chromosome
    src1->SetGenome(CBioSource::eGenome_chromosome);
    BOOST_CHECK_EQUAL(src1->GetRepliconName(), "ANONYMOUS");
    // kinetoplast
    src1->SetGenome(CBioSource::eGenome_kinetoplast);
    BOOST_CHECK_EQUAL(src1->GetRepliconName(), "kinetoplast");
    // plastid
    src1->SetGenome(CBioSource::eGenome_plastid);
    BOOST_CHECK_EQUAL(src1->GetRepliconName(), "Pltd");
    src1->SetGenome(CBioSource::eGenome_chloroplast);
    BOOST_CHECK_EQUAL(src1->GetRepliconName(), "Pltd");
    src1->SetGenome(CBioSource::eGenome_chromoplast);
    BOOST_CHECK_EQUAL(src1->GetRepliconName(), "Pltd");
    src1->SetGenome(CBioSource::eGenome_apicoplast);
    BOOST_CHECK_EQUAL(src1->GetRepliconName(), "Pltd");
    src1->SetGenome(CBioSource::eGenome_leucoplast);
    BOOST_CHECK_EQUAL(src1->GetRepliconName(), "Pltd");
    src1->SetGenome(CBioSource::eGenome_proplastid);
    BOOST_CHECK_EQUAL(src1->GetRepliconName(), "Pltd");
    src1->SetGenome(CBioSource::eGenome_chromatophore);
    BOOST_CHECK_EQUAL(src1->GetRepliconName(), "Pltd");
    //
    // mitochondrion
    //
    src1->SetGenome(CBioSource::eGenome_mitochondrion);
    BOOST_CHECK_EQUAL(src1->GetRepliconName(), "MT");
    src1->SetGenome(CBioSource::eGenome_hydrogenosome);
    BOOST_CHECK_EQUAL(src1->GetRepliconName(), "MT");
    // blank
    src1->ResetGenome();
    BOOST_CHECK_EQUAL(NStr::IsBlank(src1->GetRepliconName()), true);
}


BOOST_AUTO_TEST_CASE(Test_Regulatory)
{
    BOOST_CHECK_EQUAL(CSeqFeatData::IsRegulatory(CSeqFeatData::eSubtype_attenuator), true);
    BOOST_CHECK_EQUAL(CSeqFeatData::IsRegulatory(CSeqFeatData::eSubtype_cdregion), false);
    BOOST_CHECK_EQUAL(CSeqFeatData::GetRegulatoryClass(CSeqFeatData::eSubtype_RBS), "ribosome_binding_site");
    BOOST_CHECK_EQUAL(CSeqFeatData::GetRegulatoryClass(CSeqFeatData::eSubtype_terminator), "terminator");
    BOOST_CHECK_EQUAL(CSeqFeatData::IsRegulatory(CSeqFeatData::eSubtype_LTR), true);
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

    ss->SetName("[BankIt_cultured16S_wizard]");
    src->RemoveCultureNotes();
    BOOST_CHECK_EQUAL(src->IsSetSubtype(), false);

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

    msg = CSubSource::CheckCellLine("IPRI-OL-7", "Orgyia leucostigma");
    BOOST_CHECK_EQUAL(msg, "The International Cell Line Authentication Committee database indicates that IPRI-OL-7 from Orgyia leucostigma is known to be contaminated by IPRI-CF-124 from Choristoneura fumiferana. Please see http://iclac.org/databases/cross-contaminations/ for more information and references.");
}


BOOST_AUTO_TEST_CASE(Test_FixStrain)
{
    BOOST_CHECK_EQUAL(COrgMod::FixStrain("DSM1235"), "DSM 1235");
    BOOST_CHECK_EQUAL(COrgMod::FixStrain("DSM/1235"), "DSM 1235");
    BOOST_CHECK_EQUAL(COrgMod::FixStrain("dsm/1235"), "DSM 1235");
    BOOST_CHECK_EQUAL(COrgMod::FixStrain("dsm:1235"), "DSM 1235");
    BOOST_CHECK_EQUAL(COrgMod::FixStrain("dsm : 1235"), "DSM 1235");
    BOOST_CHECK_EQUAL(COrgMod::FixStrain("DSM"), "DSM");

    BOOST_CHECK_EQUAL(COrgMod::FixStrain("ATCC1235"), "ATCC 1235");
    BOOST_CHECK_EQUAL(COrgMod::FixStrain("ATCC/1235"), "ATCC 1235");
    BOOST_CHECK_EQUAL(COrgMod::FixStrain("atcc/1235"), "ATCC 1235");
    BOOST_CHECK_EQUAL(COrgMod::FixStrain("atcc:1235"), "ATCC 1235");
    BOOST_CHECK_EQUAL(COrgMod::FixStrain("atcc : 1235"), "ATCC 1235");
    BOOST_CHECK_EQUAL(COrgMod::FixStrain("ATCC"), "ATCC");
}

BOOST_AUTO_TEST_CASE(Test_AllowedFeatureLocation)
{
    BOOST_CHECK_EQUAL(CSeqFeatData::AllowedFeatureLocation(CSeqFeatData::eSubtype_gene), CSeqFeatData::eFeatureLocationAllowed_NucOnly);
    BOOST_CHECK_EQUAL(CSeqFeatData::AllowedFeatureLocation(CSeqFeatData::eSubtype_mat_peptide_aa), CSeqFeatData::eFeatureLocationAllowed_ProtOnly);
    BOOST_CHECK_EQUAL(CSeqFeatData::AllowedFeatureLocation(CSeqFeatData::eSubtype_site), CSeqFeatData::eFeatureLocationAllowed_Any);
    BOOST_CHECK_EQUAL(CSeqFeatData::AllowedFeatureLocation(CSeqFeatData::eSubtype_bad), CSeqFeatData::eFeatureLocationAllowed_Error);
}


BOOST_AUTO_TEST_CASE(Test_SQD_2180)
{
    string msg = CSubSource::CheckCellLine("Yamada", "Canis lupus familiaris");
    BOOST_CHECK_EQUAL(msg, "The International Cell Line Authentication Committee database indicates that Yamada from Canis lupus familiaris is known to be contaminated by Unknown from Mouse. Please see http://iclac.org/databases/cross-contaminations/ for more information and references.");

}


BOOST_AUTO_TEST_CASE(Test_SQD_2183)
{
    BOOST_CHECK_EQUAL(CSubSource::FixAltitude("100 meters"), "100 m");
    BOOST_CHECK_EQUAL(CSubSource::FixAltitude("100 meter"), "100 m");
    BOOST_CHECK_EQUAL(CSubSource::FixAltitude("100 m"), "100 m");
    BOOST_CHECK_EQUAL(CSubSource::FixAltitude("100 feet"), "30 m");
    BOOST_CHECK_EQUAL(CSubSource::FixAltitude("100 foot"), "30 m");

    //for VR-823
    BOOST_CHECK_EQUAL(CSubSource::FixAltitude("1,950 m"), "1950 m");
}


BOOST_AUTO_TEST_CASE(Test_SQD_2164)
{
    BOOST_CHECK_EQUAL(CCountries::CountryFixupItem("Mediterranean, Malvarrosa Beach (Valencia, Spain)", false),
                      "Spain: Mediterranean, Malvarrosa Beach (Valencia)");

}


BOOST_AUTO_TEST_CASE(Test_GB_4111)
{
    BOOST_CHECK_EQUAL(CCountries::CountryFixupItem("China:, Guangdong Province, Guangzhou City, Tianlu Lake Forest Park", false),
                      "China: Guangdong Province, Guangzhou City, Tianlu Lake Forest Park");

    BOOST_CHECK_EQUAL(CCountries::CountryFixupItem("China", false), "China");
    BOOST_CHECK_EQUAL(CCountries::CountryFixupItem("China: ,", false), "China");
    BOOST_CHECK_EQUAL(CCountries::CountryFixupItem("China: Guangdong Province", false), "China: Guangdong Province");
}


BOOST_AUTO_TEST_CASE(Test_GB_3965)
{
    bool ambig = false;
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("06/11/11", true, ambig), "11-Jun-2011");
    BOOST_CHECK_EQUAL(ambig, true);
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("06/11/11"), "");

    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("11/8/12"), "");
    ambig = false;
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("11/8/12", true, ambig), "08-Nov-2012");
    ambig = false;
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("11/8/12", false, ambig), "11-Aug-2012");

    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("Nov/8/12"), "08-Nov-2012");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("11/Aug/12"), "11-Aug-2012");
}


BOOST_AUTO_TEST_CASE(Test_GB_5458)
{
    bool ambig = false;
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("15-Jul-13", false, ambig), "15-Jul-2013");
    BOOST_CHECK_EQUAL(ambig, false);
}

BOOST_AUTO_TEST_CASE(Test_SQD_2319)
{
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("2012-01-52"), "");
}


BOOST_AUTO_TEST_CASE(Test_GB_5391)
{
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("June2011"), "Jun-2011");
}


BOOST_AUTO_TEST_CASE(Test_SQD_3603)
{
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("11-2009"), "Nov-2009");
}

BOOST_AUTO_TEST_CASE(Test_GB_6371)
{
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("13-Sept-2012"), "13-Sep-2012");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("13-J-2012"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("13-Ja-2012"), "");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("13-May-2012"), "13-May-2012");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("13.Septemb.2012"), "13-Sep-2012");
    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("13-Setp-2012"), "");
}

struct SCollDateInfo {
    const char * date_to_fix;
    const char * expected_result;
};

BOOST_AUTO_TEST_CASE(Test_FixDateFormat_for_BI_2614)
{
    // all these formats should be acceptable, according to
    // https://intranet.ncbi.nlm.nih.gov/ieb/DIRSUB/FT/
    // (as of 10-Nov-2015), except a few which can be fixed by
    // CSubSource::FixDateFormat
    //
    // In most cases CSubSource::FixDateFormat doesn't do anything,
    // but there are a few formats which are not normally accepted but
    // which CSubSource::FixDateFormat can fix.
    static SCollDateInfo kGoodCollectionDates[] = {
        // ISO date/times stay the same after CSubSource::FixDateFormat
        { "1952", "1952" },
        { "1952-10-21T11:43Z", "1952-10-21T11:43Z"},
        { "1952-10-21T11Z", "1952-10-21T11Z" },
        { "1952-10-21","1952-10-21"  },
        { "1952-10", "1952-10" },
        { "1952/1953", "1952/1953" },
        { "1952-10-21/1953-02-15", "1952-10-21/1953-02-15" },
        { "1952-10/1953-02", "1952-10/1953-02" },
        { "1952-10-21T11:43Z/1952-10-21T17:43Z",
          "1952-10-21T11:43Z/1952-10-21T17:43Z"},

        // Dates already in DD-Mmm-YYY format also remain unchanged
        // after CSubSource::FixDateFormat
        { "21-Oct-1952", "21-Oct-1952" },
        { "Oct-1952", "Oct-1952" },
        { "21-Oct-1952/15-Feb-1953", "21-Oct-1952/15-Feb-1953" },
        { "Oct-1952/Feb-1953", "Oct-1952/Feb-1953" },

        // A few formats can be corrected
        { "1-1-1952", "01-Jan-1952" },
        { "1-1-1952/2-2-1952", "01-Jan-1952/02-Feb-1952" }
    };
    ITERATE_0_IDX(idx, ArraySize(kGoodCollectionDates)) {
        // check CSubSource::FixDateFormat
        const string fixed_date =
            CSubSource::FixDateFormat(kGoodCollectionDates[idx].date_to_fix);
        const string expected_date(
            kGoodCollectionDates[idx].expected_result);
        BOOST_CHECK_EQUAL(fixed_date, expected_date);

        // check that fixed dates do pass CSubSource::IsCorrectDateFormat
        bool bad_format = false;
        bool in_future = false;
        CSubSource::IsCorrectDateFormat(
            fixed_date, bad_format, in_future);
        BOOST_CHECK_MESSAGE( ! bad_format, fixed_date );
        BOOST_CHECK_MESSAGE( ! in_future, fixed_date );

        // check that fixed_date also passes
        // CSubSource::GetCollectionDateProblem
        BOOST_CHECK_MESSAGE(
            "" == CSubSource::GetCollectionDateProblem(fixed_date),
            fixed_date);

        // divide into pieces for the functions that cannot handle a "/"
        vector<string> date_pieces_strs;
        NStr::Split(fixed_date, "/", date_pieces_strs);

        ITERATE( vector<string>, date_piece_ci, date_pieces_strs ) {
            const string & date_piece_str = *date_piece_ci;

            // make sure fixed dates are acceptable to
            // CSubSource::DateFromCollectionDate
            BOOST_CHECK_MESSAGE(
                CSubSource::DateFromCollectionDate(date_piece_str),
                date_piece_str);
            BOOST_CHECK_MESSAGE(
                CSubSource::DateFromCollectionDate(date_piece_str),
                date_piece_str);
        }
    }
}

BOOST_AUTO_TEST_CASE(Test_GetRNAProduct)
{
    CRef<CRNA_ref> rna(new CRNA_ref());
    rna->SetType(CRNA_ref::eType_mRNA);
    BOOST_CHECK_EQUAL(rna->GetRnaProductName(), kEmptyStr);

    string product("mRNA product");
    rna->SetExt().SetName(product);
    BOOST_CHECK_EQUAL(rna->GetRnaProductName(), product);

    rna->SetType(CRNA_ref::eType_miscRNA);
    CRef<CRNA_gen> rna_gen(new CRNA_gen());
    rna->SetExt().SetGen(*rna_gen);
    BOOST_CHECK_EQUAL(rna->GetRnaProductName(), kEmptyStr);

    product.assign("miscRNA product");
    rna_gen->SetProduct(product);
    BOOST_CHECK_EQUAL(rna->GetRnaProductName(), product);

    // the type of RNA does not affect the product name
    rna->SetType(CRNA_ref::eType_ncRNA);
    BOOST_CHECK_EQUAL(rna->GetRnaProductName(), product);

    rna->SetType(CRNA_ref::eType_tRNA);
    product.assign("tRNA-Ser");
    rna->SetExt().SetTRNA().SetAa().SetNcbieaa(83);
    BOOST_CHECK_EQUAL(rna->GetRnaProductName(), product);

    rna->SetType(CRNA_ref::eType_ncRNA);
    BOOST_CHECK_EQUAL(rna->GetRnaProductName(), product);

    product.assign("tRNA-Met");
    rna->SetType(CRNA_ref::eType_tRNA);
    rna->SetExt().SetTRNA().SetAa().SetIupacaa(77);
    BOOST_CHECK_EQUAL(rna->GetRnaProductName(), product);

    product.assign("tRNA-OTHER");
    rna->SetExt().SetTRNA().SetAa().SetIupacaa(88);
    BOOST_CHECK_EQUAL(rna->GetRnaProductName(), product);

    rna->SetExt().SetTRNA().SetAa().SetNcbieaa(88);
    BOOST_CHECK_EQUAL(rna->GetRnaProductName(), product);

    product.assign("tRNA-TERM");
    rna->SetExt().SetTRNA().SetAa().SetNcbieaa(42);
    BOOST_CHECK_EQUAL(rna->GetRnaProductName(), product);

    rna->SetExt().SetTRNA().SetAa().SetIupacaa(42);
    BOOST_CHECK_EQUAL(rna->GetRnaProductName(), product);

    product.clear();
    rna->SetExt().SetTRNA().SetAa().SetIupacaa(43);
    BOOST_CHECK_EQUAL(rna->GetRnaProductName(), product);
}

BOOST_AUTO_TEST_CASE(Test_SetRnaProductName)
{
    CRef<CRNA_ref> rna(new CRNA_ref());

    string remainder;
    string product("mRNA product");
    rna->SetType(CRNA_ref::eType_mRNA);
    rna->SetRnaProductName(product, remainder);
    BOOST_CHECK_EQUAL(rna->IsSetExt(), true);
    BOOST_CHECK_EQUAL(rna->GetExt().GetName(), product);
    BOOST_CHECK_EQUAL(remainder, kEmptyStr);

    product.resize(0);
    rna->SetRnaProductName(product, remainder);
    BOOST_CHECK_EQUAL(rna->IsSetExt(), false);
    BOOST_CHECK_EQUAL(remainder, kEmptyStr);

    product.assign("rRNA product");
    rna->SetType(CRNA_ref::eType_rRNA);
    rna->SetRnaProductName(product, remainder);
    BOOST_CHECK_EQUAL(rna->IsSetExt(), true);
    BOOST_CHECK_EQUAL(rna->GetExt().GetName(), product);
    BOOST_CHECK_EQUAL(remainder, kEmptyStr);

    product.resize(0);
    rna->SetRnaProductName(product, remainder);
    BOOST_CHECK_EQUAL(rna->IsSetExt(), false);
    BOOST_CHECK_EQUAL(remainder, kEmptyStr);

    product.assign("miscRNA product");
    rna->SetType(CRNA_ref::eType_miscRNA);
    rna->SetRnaProductName(product, remainder);
    BOOST_CHECK_EQUAL(rna->IsSetExt(), true);
    BOOST_CHECK_EQUAL(rna->GetExt().GetGen().GetProduct(), product);
    BOOST_CHECK_EQUAL(remainder, kEmptyStr);

    product.resize(0);
    rna->SetRnaProductName(product, remainder);
    BOOST_CHECK_EQUAL(rna->IsSetExt(), false);
    BOOST_CHECK_EQUAL(remainder, kEmptyStr);

    // name containing underscore is not recognized
    product.assign("tRNA_Ala");
    rna->SetType(CRNA_ref::eType_tRNA);
    rna->SetRnaProductName(product, remainder);
    BOOST_CHECK_EQUAL(rna->GetExt().IsTRNA(), true);
    BOOST_CHECK_EQUAL(rna->GetExt().GetTRNA().IsSetAa(), false);
    BOOST_CHECK_EQUAL(remainder, product);

    // it is also case-sensitive
    product.assign("TRNA-Ala");
    rna->SetRnaProductName(product, remainder);
    BOOST_CHECK_EQUAL(rna->GetExt().IsTRNA(), true);
    BOOST_CHECK_EQUAL(rna->GetExt().GetTRNA().IsSetAa(), false);
    BOOST_CHECK_EQUAL(remainder, product);

    product.assign("tRNA-Ala");
    rna->SetRnaProductName(product, remainder);
    BOOST_CHECK_EQUAL(rna->GetExt().GetTRNA().IsSetAa(), true);
    BOOST_CHECK_EQUAL(rna->GetExt().GetTRNA().GetAa().GetNcbieaa(), 65);
    BOOST_CHECK_EQUAL(remainder, kEmptyStr);

    product.clear();
    rna->SetRnaProductName(product, remainder);
    BOOST_CHECK_EQUAL(rna->IsSetExt(), true);
    BOOST_CHECK_EQUAL(rna->GetExt().GetTRNA().IsSetAa(), false);
    BOOST_CHECK_EQUAL(rna->GetRnaProductName(), kEmptyStr);
    BOOST_CHECK_EQUAL(remainder, kEmptyStr);

    product.assign("Ser");
    rna->SetRnaProductName(product, remainder);
    BOOST_CHECK_EQUAL(rna->GetExt().IsTRNA(), true);
    BOOST_CHECK_EQUAL(rna->GetExt().GetTRNA().GetAa().GetNcbieaa(), 83);
    BOOST_CHECK_EQUAL(remainder, kEmptyStr);

    product.assign("TERM");
    rna->SetRnaProductName(product, remainder);
    BOOST_CHECK_EQUAL(rna->GetExt().IsTRNA(), true);
    BOOST_CHECK_EQUAL(rna->GetExt().GetTRNA().GetAa().GetNcbieaa(), 42);
    BOOST_CHECK_EQUAL(remainder, kEmptyStr);

    product.assign("tRNA-other");
    rna->SetRnaProductName(product, remainder);
    BOOST_CHECK_EQUAL(rna->GetExt().IsTRNA(), true);
    BOOST_CHECK_EQUAL(rna->GetExt().GetTRNA().GetAa().GetNcbieaa(), 88);
    BOOST_CHECK_EQUAL(remainder, kEmptyStr);

    // also take single-letter codes
    product.assign("tRNA-A(gct)");
    rna->SetRnaProductName(product, remainder);
    BOOST_CHECK_EQUAL(rna->GetExt().IsTRNA(), true);
    BOOST_CHECK_EQUAL(rna->GetExt().GetTRNA().GetAa().GetNcbieaa(), 65);
    BOOST_CHECK_EQUAL(remainder, kEmptyStr);

    product.assign("tRNA-*(gct)");
    rna->SetRnaProductName(product, remainder);
    BOOST_CHECK_EQUAL(rna->GetExt().IsTRNA(), true);
    BOOST_CHECK_EQUAL(rna->GetExt().GetTRNA().GetAa().GetNcbieaa(), 42);
    BOOST_CHECK_EQUAL(remainder, kEmptyStr);

}


BOOST_AUTO_TEST_CASE(Test_IsIllegalQualName)
{
    BOOST_CHECK_EQUAL(CGb_qual::IsIllegalQualName("exception"), true);
    BOOST_CHECK_EQUAL(CGb_qual::IsIllegalQualName("number"), false);
}


BOOST_AUTO_TEST_CASE(Test_IsECNumberSplit)
{
    BOOST_CHECK_EQUAL(CProt_ref::IsECNumberSplit("1.1.1.246"), true);
    BOOST_CHECK_EQUAL(CProt_ref::IsECNumberSplit("1.1.1.63"), false);
    BOOST_CHECK_EQUAL(CProt_ref::IsECNumberSplit("1.1.1.128"), false);
}


BOOST_AUTO_TEST_CASE(Test_FileTrack)
{
    CRef<CUser_object> obj(new CUser_object());

    obj->SetFileTrackUploadId("7azalbch/brev2_motif_summary.csv");
    BOOST_CHECK_EQUAL(obj->GetObjectType(), CUser_object::eObjectType_FileTrack);
    BOOST_CHECK_EQUAL(obj->GetData().front()->GetLabel().GetStr(), "BaseModification-FileTrackURL");
    BOOST_CHECK_EQUAL(obj->GetData().front()->GetData().GetStr(), "https://submit.ncbi.nlm.nih.gov/ft/byid/7azalbch/brev2_motif_summary.csv");
}


BOOST_AUTO_TEST_CASE(Test_EnvSampleCleanup)
{
    CRef<CBioSource> src(new CBioSource());

    BOOST_CHECK_EQUAL(src->FixEnvironmentalSample(), false);
    src->SetOrg().SetTaxname("uncultured x");
    BOOST_CHECK_EQUAL(src->FixEnvironmentalSample(), true);
    BOOST_CHECK_EQUAL(src->GetSubtype().front()->GetSubtype(), CSubSource::eSubtype_environmental_sample);
    BOOST_CHECK_EQUAL(src->FixEnvironmentalSample(), false);

    src->ResetSubtype();
    src->SetOrg().SetTaxname("Homo sapiens");
    BOOST_CHECK_EQUAL(src->FixEnvironmentalSample(), false);
    src->SetSubtype().push_back(CRef<CSubSource>(new CSubSource(CSubSource::eSubtype_metagenomic, " ")));
    BOOST_CHECK_EQUAL(src->FixEnvironmentalSample(), true);
    BOOST_CHECK_EQUAL(src->GetSubtype().back()->GetSubtype(), CSubSource::eSubtype_environmental_sample);
    BOOST_CHECK_EQUAL(src->FixEnvironmentalSample(), false);

    src->ResetSubtype();
    src->SetOrg().SetOrgname().SetDiv("ENV");
    BOOST_CHECK_EQUAL(src->FixEnvironmentalSample(), true);
    BOOST_CHECK_EQUAL(src->GetSubtype().front()->GetSubtype(), CSubSource::eSubtype_environmental_sample);
    BOOST_CHECK_EQUAL(src->FixEnvironmentalSample(), false);

    src->ResetSubtype();
    src->SetOrg().SetOrgname().ResetDiv();
    src->SetOrg().SetOrgname().SetLineage("metagenomes");
    BOOST_CHECK_EQUAL(src->FixEnvironmentalSample(), true);
    BOOST_CHECK_EQUAL(src->GetSubtype().front()->GetSubtype(), CSubSource::eSubtype_environmental_sample);
    BOOST_CHECK_EQUAL(src->GetSubtype().back()->GetSubtype(), CSubSource::eSubtype_metagenomic);
    BOOST_CHECK_EQUAL(src->FixEnvironmentalSample(), false);

    src->ResetSubtype();
    src->SetOrg().SetOrgname().ResetLineage();
    src->SetOrg().SetOrgname().SetMod().push_back(CRef<COrgMod>(new COrgMod(COrgMod::eSubtype_metagenome_source, "X")));
    BOOST_CHECK_EQUAL(src->FixEnvironmentalSample(), true);
    BOOST_CHECK_EQUAL(src->GetSubtype().front()->GetSubtype(), CSubSource::eSubtype_environmental_sample);
    BOOST_CHECK_EQUAL(src->GetSubtype().back()->GetSubtype(), CSubSource::eSubtype_metagenomic);
    BOOST_CHECK_EQUAL(src->FixEnvironmentalSample(), false);

}


BOOST_AUTO_TEST_CASE(Test_RemoveNullTerms)
{
    CRef<CBioSource> src(new CBioSource());

    src->SetSubtype().push_back(CRef<CSubSource>(new CSubSource(CSubSource::eSubtype_altitude, "N/A")));
    src->SetOrg().SetOrgname().SetMod().push_back(CRef<COrgMod>(new COrgMod(COrgMod::eSubtype_acronym, "Missing")));
    src->SetOrg().SetOrgname().SetMod().push_back(CRef<COrgMod>(new COrgMod(COrgMod::eSubtype_anamorph, "not Missing")));

    BOOST_CHECK_EQUAL(src->RemoveNullTerms(), true);
    BOOST_CHECK_EQUAL(src->IsSetSubtype(), false);
    BOOST_CHECK_EQUAL(src->GetOrg().GetOrgname().GetMod().size(), 1);
    BOOST_CHECK_EQUAL(src->GetOrg().GetOrgname().GetMod().front()->GetSubtype(), COrgMod::eSubtype_anamorph);
}


BOOST_AUTO_TEST_CASE(Test_RemoveAbbreviation)
{
    CRef<COrgMod> om(new COrgMod(COrgMod::eSubtype_serovar, "serovar x"));
    BOOST_CHECK_EQUAL(om->RemoveAbbreviation(), true);
    BOOST_CHECK_EQUAL(om->GetSubname(), "x");

    CRef<COrgMod> om2(new COrgMod(COrgMod::eSubtype_sub_species, "subsp. y"));
    BOOST_CHECK_EQUAL(om2->RemoveAbbreviation(), true);
    BOOST_CHECK_EQUAL(om2->GetSubname(), "y");

}


BOOST_AUTO_TEST_CASE(Test_FixSexMatingTypeInconsistencies)
{
    CRef<CBioSource> src(new CBioSource());

    src->SetOrg().SetOrgname().SetLineage("Viruses; foo");
    BOOST_CHECK_EQUAL(src->AllowSexQualifier(), false);
    BOOST_CHECK_EQUAL(src->AllowMatingTypeQualifier(), false);
    src->SetOrg().SetOrgname().SetLineage("Bacteria; foo");
    BOOST_CHECK_EQUAL(src->AllowSexQualifier(), false);
    BOOST_CHECK_EQUAL(src->AllowMatingTypeQualifier(), true);
    src->SetOrg().SetOrgname().SetLineage("Archaea; foo");
    BOOST_CHECK_EQUAL(src->AllowSexQualifier(), false);
    BOOST_CHECK_EQUAL(src->AllowMatingTypeQualifier(), true);
    src->SetOrg().SetOrgname().SetLineage("Eukaryota; Fungi; foo");
    BOOST_CHECK_EQUAL(src->AllowSexQualifier(), false);
    BOOST_CHECK_EQUAL(src->AllowMatingTypeQualifier(), true);
    src->SetOrg().SetOrgname().SetLineage("Eukaryota; Metazoa; foo");
    BOOST_CHECK_EQUAL(src->AllowSexQualifier(), true);
    BOOST_CHECK_EQUAL(src->AllowMatingTypeQualifier(), false);
    src->SetOrg().SetOrgname().SetLineage("Eukaryota; Viridiplantae; Streptophyta; Embryophyta; foo");
    BOOST_CHECK_EQUAL(src->AllowSexQualifier(), true);
    BOOST_CHECK_EQUAL(src->AllowMatingTypeQualifier(), false);
    src->SetOrg().SetOrgname().SetLineage("Eukaryota; Rhodophyta; foo");
    BOOST_CHECK_EQUAL(src->AllowSexQualifier(), true);
    BOOST_CHECK_EQUAL(src->AllowMatingTypeQualifier(), false);
    src->SetOrg().SetOrgname().SetLineage("Eukaryota; stramenopiles; Phaeophyceae; foo");
    BOOST_CHECK_EQUAL(src->AllowSexQualifier(), true);
    BOOST_CHECK_EQUAL(src->AllowMatingTypeQualifier(), false);

    CRef<CSubSource> s(new CSubSource(CSubSource::eSubtype_sex, "f"));
    src->SetSubtype().push_back(s);
    BOOST_CHECK_EQUAL(src->FixSexMatingTypeInconsistencies(), false);
    BOOST_CHECK_EQUAL(src->GetSubtype().size(), 1);

    s->SetSubtype(CSubSource::eSubtype_mating_type);
    BOOST_CHECK_EQUAL(src->FixSexMatingTypeInconsistencies(), true);
    BOOST_CHECK_EQUAL(src->GetSubtype().size(), 1);
    BOOST_CHECK_EQUAL(src->GetSubtype().front()->GetSubtype(), CSubSource::eSubtype_sex);

    s->SetSubtype(CSubSource::eSubtype_mating_type);
    s->SetName("foo");
    BOOST_CHECK_EQUAL(src->FixSexMatingTypeInconsistencies(), true);
    BOOST_CHECK_EQUAL(src->IsSetSubtype(), false);

    src->SetOrg().SetOrgname().SetLineage("Viruses; foo");
    src->SetSubtype().push_back(s);
    BOOST_CHECK_EQUAL(src->FixSexMatingTypeInconsistencies(), true);
    BOOST_CHECK_EQUAL(src->IsSetSubtype(), false);

    s->SetSubtype(CSubSource::eSubtype_sex);
    src->SetSubtype().push_back(s);
    BOOST_CHECK_EQUAL(src->FixSexMatingTypeInconsistencies(), true);
    BOOST_CHECK_EQUAL(src->IsSetSubtype(), false);

}


BOOST_AUTO_TEST_CASE(Test_RemoveUnexpectedViralQualifiers)
{
    CRef<CBioSource> src(new CBioSource());

    src->SetOrg().SetOrgname().SetMod().push_back(CRef<COrgMod>(new COrgMod(COrgMod::eSubtype_breed, "x")));
    src->SetOrg().SetOrgname().SetMod().push_back(CRef<COrgMod>(new COrgMod(COrgMod::eSubtype_cultivar, "y")));
    src->SetOrg().SetOrgname().SetMod().push_back(CRef<COrgMod>(new COrgMod(COrgMod::eSubtype_specimen_voucher, "z")));
    src->SetOrg().SetOrgname().SetMod().push_back(CRef<COrgMod>(new COrgMod(COrgMod::eSubtype_acronym, "a")));

    BOOST_CHECK_EQUAL(src->RemoveUnexpectedViralQualifiers(), false);
    BOOST_CHECK_EQUAL(src->GetOrg().GetOrgname().GetMod().size(), 4);
    src->SetOrg().SetOrgname().SetLineage("Viruses; foo");
    BOOST_CHECK_EQUAL(src->RemoveUnexpectedViralQualifiers(), true);
    BOOST_CHECK_EQUAL(src->GetOrg().GetOrgname().GetMod().size(), 1);
    BOOST_CHECK_EQUAL(src->GetOrg().GetOrgname().GetMod().front()->GetSubtype(), COrgMod::eSubtype_acronym);

}


BOOST_AUTO_TEST_CASE(Test_FixGenomeForQualifiers)
{
    CRef<CBioSource> src(new CBioSource());

    BOOST_CHECK_EQUAL(src->FixGenomeForQualifiers(), false);
    src->SetSubtype().push_back(CRef<CSubSource>(new CSubSource(CSubSource::eSubtype_plasmid_name, "X")));
    BOOST_CHECK_EQUAL(src->FixGenomeForQualifiers(), true);
    BOOST_CHECK_EQUAL(src->GetGenome(), CBioSource::eGenome_plasmid);
    BOOST_CHECK_EQUAL(src->FixGenomeForQualifiers(), false);
    src->SetGenome(CBioSource::eGenome_apicoplast);
    BOOST_CHECK_EQUAL(src->FixGenomeForQualifiers(), false);
    BOOST_CHECK_EQUAL(src->GetGenome(), CBioSource::eGenome_apicoplast);
    src->SetGenome(CBioSource::eGenome_unknown);
    BOOST_CHECK_EQUAL(src->FixGenomeForQualifiers(), true);
    BOOST_CHECK_EQUAL(src->GetGenome(), CBioSource::eGenome_plasmid);
}


BOOST_AUTO_TEST_CASE(Test_AllowXref)
{
    BOOST_CHECK_EQUAL(CSeqFeatData::AllowXref(CSeqFeatData::eSubtype_gene, CSeqFeatData::eSubtype_cdregion), true);
    BOOST_CHECK_EQUAL(CSeqFeatData::AllowXref(CSeqFeatData::eSubtype_cdregion, CSeqFeatData::eSubtype_regulatory), false);
    BOOST_CHECK_EQUAL(CSeqFeatData::AllowXref(CSeqFeatData::eSubtype_intron, CSeqFeatData::eSubtype_exon), false);
    BOOST_CHECK_EQUAL(CSeqFeatData::ProhibitXref(CSeqFeatData::eSubtype_gene, CSeqFeatData::eSubtype_cdregion), false);
    BOOST_CHECK_EQUAL(CSeqFeatData::ProhibitXref(CSeqFeatData::eSubtype_intron, CSeqFeatData::eSubtype_regulatory), true);
    BOOST_CHECK_EQUAL(CSeqFeatData::ProhibitXref(CSeqFeatData::eSubtype_intron, CSeqFeatData::eSubtype_exon), true);
}

BOOST_AUTO_TEST_CASE(Test_recombination_class)
{
    BOOST_CHECK_EQUAL(CSeqFeatData::IsLegalQualifier(CSeqFeatData::eSubtype_misc_recomb, CSeqFeatData::eQual_recombination_class), true);
    const CGb_qual::TLegalRecombinationClassSet recomb_values = CGb_qual::GetSetOfLegalRecombinationClassValues();
    BOOST_ASSERT(recomb_values.find("chromosome_breakpoint") != recomb_values.end());


    string old_recomb_value("meiotic_recombination");
    BOOST_CHECK_EQUAL(CGb_qual::FixRecombinationClassValue(old_recomb_value), true);
    BOOST_CHECK_EQUAL(old_recomb_value, "meiotic");

    old_recomb_value = ("other:non_allelic_homologous_recombination");
    BOOST_CHECK_EQUAL(CGb_qual::FixRecombinationClassValue(old_recomb_value), true);
    BOOST_CHECK_EQUAL(old_recomb_value, "non_allelic_homologous");

    string valid_recomb_value("mitotic");
    BOOST_CHECK_EQUAL(CGb_qual::FixRecombinationClassValue(valid_recomb_value), false);
    BOOST_CHECK_EQUAL(valid_recomb_value, "mitotic");
}


BOOST_AUTO_TEST_CASE(Test_OrgMod_IsDiscouraged)
{
    BOOST_CHECK_EQUAL(COrgMod::IsDiscouraged(COrgMod::eSubtype_metagenome_source), true);
    BOOST_CHECK_EQUAL(COrgMod::IsDiscouraged(COrgMod::eSubtype_metagenome_source, true), false);
}


void CheckViruses(CBioSource& src)
{
    src.SetOrg().SetOrgname().SetLineage("viruses");
    BOOST_CHECK_EQUAL(src.GetBioprojectType(), "eSegment");
    BOOST_CHECK_EQUAL(src.GetBioprojectLocation(), "eVirionPhage");
    src.SetOrg().SetOrgname().SetLineage("viroids");
    BOOST_CHECK_EQUAL(src.GetBioprojectType(), "eSegment");
    BOOST_CHECK_EQUAL(src.GetBioprojectLocation(), "eViroid");
    src.SetOrg().SetOrgname().ResetLineage();
}


void CheckPlasmid(CBioSource& src)
{
    CRef<CSubSource> p(new CSubSource(CSubSource::eSubtype_plasmid_name, "foo"));
    src.SetSubtype().push_back(p);
    BOOST_CHECK_EQUAL(src.GetBioprojectType(), "ePlasmid");
    src.ResetSubtype();
}


void CheckBioProjectLocationVals(CBioSource::EGenome genome, const string& bioprojectlocation)
{
    CRef<CBioSource> src(new CBioSource());
    src->SetGenome(genome);
    BOOST_CHECK_EQUAL(src->GetBioprojectType(), "eChromosome");
    BOOST_CHECK_EQUAL(src->GetBioprojectLocation(), bioprojectlocation);
    CheckViruses(*src);
    CheckPlasmid(*src);
}


BOOST_AUTO_TEST_CASE(Test_GetBioProjectTypeAndLocation)
{
    CRef<CBioSource> src(new CBioSource());
    BOOST_CHECK_EQUAL(src->GetBioprojectType(), "eChromosome");
    BOOST_CHECK_EQUAL(src->GetBioprojectLocation(), "eNuclearProkaryote");
    CheckViruses(*src);

    src->SetGenome(CBioSource::eGenome_unknown);
    BOOST_CHECK_EQUAL(src->GetBioprojectType(), "eChromosome");
    BOOST_CHECK_EQUAL(src->GetBioprojectLocation(), "eNuclearProkaryote");
    CheckViruses(*src);

    src->SetGenome(CBioSource::eGenome_chromosome);
    BOOST_CHECK_EQUAL(src->GetBioprojectType(), "eChromosome");
    BOOST_CHECK_EQUAL(src->GetBioprojectLocation(), "eNuclearProkaryote");
    src->SetSubtype().push_back(CRef<CSubSource>(new CSubSource(CSubSource::eSubtype_linkage_group, "x")));
    BOOST_CHECK_EQUAL(src->GetBioprojectType(), "eLinkageGroup");
    BOOST_CHECK_EQUAL(src->GetBioprojectLocation(), "eNuclearProkaryote");
    src->ResetSubtype();

    src->SetGenome(CBioSource::eGenome_extrachrom);
    BOOST_CHECK_EQUAL(src->GetBioprojectType(), "eExtrachrom");
    BOOST_CHECK_EQUAL(src->GetBioprojectLocation(), "eNuclearProkaryote");

    CheckBioProjectLocationVals(CBioSource::eGenome_apicoplast, "eApicoplast");
    CheckBioProjectLocationVals(CBioSource::eGenome_chloroplast, "eChloroplast");
    CheckBioProjectLocationVals(CBioSource::eGenome_chromatophore, "eChromatophore");
    CheckBioProjectLocationVals(CBioSource::eGenome_chromoplast, "eChromoplast");
    CheckBioProjectLocationVals(CBioSource::eGenome_cyanelle, "eCyanelle");
    CheckBioProjectLocationVals(CBioSource::eGenome_hydrogenosome, "eHydrogenosome");
    CheckBioProjectLocationVals(CBioSource::eGenome_kinetoplast, "eKinetoplast");
    CheckBioProjectLocationVals(CBioSource::eGenome_leucoplast, "eLeucoplast");
    CheckBioProjectLocationVals(CBioSource::eGenome_macronuclear, "eMacronuclear");
    CheckBioProjectLocationVals(CBioSource::eGenome_mitochondrion, "eMitochondrion");
    CheckBioProjectLocationVals(CBioSource::eGenome_nucleomorph, "eNucleomorph");
    CheckBioProjectLocationVals(CBioSource::eGenome_plastid, "ePlastid");
    CheckBioProjectLocationVals(CBioSource::eGenome_proplastid, "eProplastid");
    CheckBioProjectLocationVals(CBioSource::eGenome_proviral, "eProviralProphage");
    CheckBioProjectLocationVals(CBioSource::eGenome_endogenous_virus, "eProviralProphage");
    CheckBioProjectLocationVals(CBioSource::eGenome_transposon, "eOther");
    CheckBioProjectLocationVals(CBioSource::eGenome_insertion_seq, "eOther");

}


BOOST_AUTO_TEST_CASE(Test_OrgRefLookup)
{
    string taxname = "Zea mays";

    CConstRef<COrg_ref> lookup = COrg_ref::TableLookup(taxname);
    if (! lookup) {
        BOOST_CHECK_EQUAL("", taxname);
        return;
    }
    BOOST_CHECK_EQUAL(lookup->GetTaxname(), taxname);
    BOOST_CHECK_EQUAL(lookup->GetOrgname().GetDiv(), "PLN");
}


BOOST_AUTO_TEST_CASE(Test_CleanupAndRepairInference)
{
    BOOST_CHECK_EQUAL(CGb_qual::CleanupAndRepairInference("similar to sequence : UniProtKB : P39748"), "similar to sequence:UniProtKB:P39748");
    BOOST_CHECK_EQUAL(CGb_qual::CleanupAndRepairInference("similar to RNA sequence, mRNA: INSDC:AY262280.1"), "similar to RNA sequence, mRNA:INSDC:AY262280.1");
    BOOST_CHECK_EQUAL(CGb_qual::CleanupAndRepairInference("similar to AA: UniProtKB : P39748"), "similar to AA sequence:UniProtKB:P39748");
    BOOST_CHECK_EQUAL(CGb_qual::CleanupAndRepairInference("similar to AA sequence: UniProtKB : P39748"), "similar to AA sequence:UniProtKB:P39748");
}


BOOST_AUTO_TEST_CASE(Test_SQD_4173)
{
    BOOST_CHECK_EQUAL(CBioSource::ShouldIgnoreConflict("lat-lon", "12.12345 N 23.123456 W", "12.12 N 23.12 W"), false);
}


BOOST_AUTO_TEST_CASE(Test_VR_665)
{
    string voucher_type;
    bool is_miscapitalized;
    string correct_cap;
    bool needs_country;
    bool erroneous_country;
    string inst_code = "ARBH";
    BOOST_CHECK_EQUAL(COrgMod::IsInstitutionCodeValid(inst_code, voucher_type, is_miscapitalized, correct_cap, needs_country, erroneous_country), true);
    BOOST_CHECK_EQUAL(voucher_type, "s");
    BOOST_CHECK_EQUAL(is_miscapitalized, false);
    BOOST_CHECK_EQUAL(correct_cap, "ARBH");
    BOOST_CHECK_EQUAL(needs_country, false);
    BOOST_CHECK_EQUAL(erroneous_country, false);

    inst_code = "NMNH";
    BOOST_CHECK_EQUAL(COrgMod::IsInstitutionCodeValid(inst_code, voucher_type, is_miscapitalized, correct_cap, needs_country, erroneous_country), true);
    BOOST_CHECK_EQUAL(voucher_type, "sb");
    BOOST_CHECK_EQUAL(is_miscapitalized, false);
    BOOST_CHECK_EQUAL(correct_cap, "NMNH");
    BOOST_CHECK_EQUAL(needs_country, false);
    BOOST_CHECK_EQUAL(erroneous_country, false);

    inst_code = "ZMM";
    BOOST_CHECK_EQUAL(COrgMod::IsInstitutionCodeValid(inst_code, voucher_type, is_miscapitalized, correct_cap, needs_country, erroneous_country), true);
    BOOST_CHECK_EQUAL(voucher_type, "s");
    BOOST_CHECK_EQUAL(is_miscapitalized, false);
    BOOST_CHECK_EQUAL(correct_cap, "ZMM");
    BOOST_CHECK_EQUAL(needs_country, false);
    BOOST_CHECK_EQUAL(erroneous_country, false);

    inst_code = "ZMUM";
    BOOST_CHECK_EQUAL(COrgMod::IsInstitutionCodeValid(inst_code, voucher_type, is_miscapitalized, correct_cap, needs_country, erroneous_country), true);
    BOOST_CHECK_EQUAL(voucher_type, "s");
    BOOST_CHECK_EQUAL(is_miscapitalized, false);
    BOOST_CHECK_EQUAL(correct_cap, "ZMUM");
    BOOST_CHECK_EQUAL(needs_country, false);
    BOOST_CHECK_EQUAL(erroneous_country, false);
}


BOOST_AUTO_TEST_CASE(Test_VR_693)
{
    BOOST_CHECK_EQUAL(CSubSource::FixDevStageCapitalization("FOO"), "FOO");
    BOOST_CHECK_EQUAL(CSubSource::FixDevStageCapitalization("LARVA"), "larva");

    BOOST_CHECK_EQUAL(CSubSource::FixCellTypeCapitalization("FOO"), "FOO");
    BOOST_CHECK_EQUAL(CSubSource::FixCellTypeCapitalization("Lymphocyte"), "lymphocyte");

    BOOST_CHECK_EQUAL(CSubSource::FixTissueTypeCapitalization("CLINICAL"), "clinical");

    BOOST_CHECK_EQUAL(CSubSource::FixIsolationSourceCapitalization("Bovine (feces)"), "bovine feces");
    BOOST_CHECK_EQUAL(CSubSource::FixIsolationSourceCapitalization("BLOOD"), "blood");

    BOOST_CHECK_EQUAL(CSubSource::FixIsolationSourceCapitalization("Leaf"), "leaf");
    BOOST_CHECK_EQUAL(CSubSource::FixIsolationSourceCapitalization("Roots"), "roots");

}


BOOST_AUTO_TEST_CASE(Test_IsLegalClass)
{
    BOOST_CHECK_EQUAL(CRNA_gen::IsLegalClass("lncRNA"), true);
    BOOST_CHECK_EQUAL(CRNA_gen::IsLegalClass("babble"), false);
    BOOST_CHECK_EQUAL(CRNA_gen::GetncRNAClassList().size(), 23);
}


BOOST_AUTO_TEST_CASE(Test_LegalMobileElement)
{
    BOOST_CHECK_EQUAL(CGb_qual::IsLegalMobileElementValue("foo"), false);
    BOOST_CHECK_EQUAL(CGb_qual::IsLegalMobileElementValue("integron"), true);

    string val = "p-element";
    BOOST_CHECK_EQUAL(CGb_qual::FixMobileElementValue(val), true);
    BOOST_CHECK_EQUAL(val, "P-element");
}


BOOST_AUTO_TEST_CASE(Test_FixImportKey)
{
    string val = "Exon";
    BOOST_CHECK_EQUAL(CSeqFeatData::FixImportKey(val), true);
    BOOST_CHECK_EQUAL(val, "exon");
    BOOST_CHECK_EQUAL(CSeqFeatData::FixImportKey(val), false);
}


BOOST_AUTO_TEST_CASE(Test_IsTypeMaterialValid)
{
    BOOST_CHECK_EQUAL(COrgMod::IsValidTypeMaterial("holotype X"), true);
    BOOST_CHECK_EQUAL(COrgMod::IsINSDCValidTypeMaterial("holotype X"), true);
    BOOST_CHECK_EQUAL(COrgMod::IsValidTypeMaterial("culture from epitype Y"), true);
    BOOST_CHECK_EQUAL(COrgMod::IsINSDCValidTypeMaterial("culture from epitype Y"), true);
    BOOST_CHECK_EQUAL(COrgMod::IsValidTypeMaterial("ex-syntype Z"), true);
    BOOST_CHECK_EQUAL(COrgMod::IsINSDCValidTypeMaterial("ex-syntype Z"), true);


}


BOOST_AUTO_TEST_CASE(Test_VR_730)
{
    int hour, min, sec;
    // succeed
    BOOST_CHECK_EQUAL(CSubSource::IsISOFormatTime("11:13:00Z", hour, min, sec), true);
    BOOST_CHECK_EQUAL(hour, 11);
    BOOST_CHECK_EQUAL(min, 13);
    BOOST_CHECK_EQUAL(sec, 0);
    // fail because no time zone specified
    BOOST_CHECK_EQUAL(CSubSource::IsISOFormatTime("11:13:00", hour, min, sec), false);
    // succeed because time zone not required
    BOOST_CHECK_EQUAL(CSubSource::IsISOFormatTime("11:13:00", hour, min, sec, false), true);
    BOOST_CHECK_EQUAL(hour, 11);
    BOOST_CHECK_EQUAL(min, 13);
    BOOST_CHECK_EQUAL(sec, 0);

    BOOST_CHECK_EQUAL(CSubSource::FixDateFormat("2012-10-26T11:13:00"), "2012-10-26");
}


BOOST_AUTO_TEST_CASE(Test_RefGeneTracking)
{
    CRef<CUser_object> user(new CUser_object());
    user->SetObjectType(CUser_object::eObjectType_RefGeneTracking);
    BOOST_CHECK_EQUAL(user->GetType().GetStr(), "RefGeneTracking");
    BOOST_CHECK_EQUAL(user->GetObjectType(), CUser_object::eObjectType_RefGeneTracking);
    BOOST_CHECK_EQUAL(user->IsRefGeneTracking(), true);

    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingStatus(), CUser_object::eRefGeneTrackingStatus_NotSet);
    BOOST_CHECK_EQUAL(user->IsSetRefGeneTrackingStatus(), false);
    user->SetRefGeneTrackingStatus(CUser_object::eRefGeneTrackingStatus_PIPELINE);
    BOOST_CHECK_EQUAL(user->IsSetRefGeneTrackingStatus(), true);
    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingStatus(), CUser_object::eRefGeneTrackingStatus_PIPELINE);
    BOOST_CHECK_EQUAL(user->GetData().back()->GetLabel().GetStr(), "Status");
    BOOST_CHECK_EQUAL(user->GetData().back()->GetData().GetStr(), "PIPELINE");
    user->SetRefGeneTrackingStatus(CUser_object::eRefGeneTrackingStatus_INFERRED);
    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingStatus(), CUser_object::eRefGeneTrackingStatus_INFERRED);
    BOOST_CHECK_EQUAL(user->GetData().back()->GetLabel().GetStr(), "Status");
    BOOST_CHECK_EQUAL(user->GetData().back()->GetData().GetStr(), "INFERRED");
    BOOST_CHECK_EQUAL(user->GetData().size(), 1);

    BOOST_CHECK_EQUAL(user->IsSetRefGeneTrackingGenomicSource(), false);
    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingGenomicSource(), kEmptyStr);
    user->SetRefGeneTrackingGenomicSource("XXX");
    BOOST_CHECK_EQUAL(user->IsSetRefGeneTrackingGenomicSource(), true);
    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingGenomicSource(), "XXX");
    BOOST_CHECK_EQUAL(user->GetData().back()->GetLabel().GetStr(), "GenomicSource");
    BOOST_CHECK_EQUAL(user->GetData().back()->GetData().GetStr(), "XXX");
    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingStatus(), CUser_object::eRefGeneTrackingStatus_INFERRED);
    user->SetRefGeneTrackingGenomicSource("XXX2");
    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingGenomicSource(), "XXX2");
    BOOST_CHECK_EQUAL(user->GetData().back()->GetLabel().GetStr(), "GenomicSource");
    BOOST_CHECK_EQUAL(user->GetData().back()->GetData().GetStr(), "XXX2");
    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingStatus(), CUser_object::eRefGeneTrackingStatus_INFERRED);
    BOOST_CHECK_EQUAL(user->GetData().size(), 2);

    BOOST_CHECK_EQUAL(user->IsSetRefGeneTrackingCollaborator(), false);
    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingCollaborator(), kEmptyStr);
    user->SetRefGeneTrackingCollaborator("YYY");
    BOOST_CHECK_EQUAL(user->IsSetRefGeneTrackingCollaborator(), true);
    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingCollaborator(), "YYY");
    BOOST_CHECK_EQUAL(user->GetData().back()->GetLabel().GetStr(), "Collaborator");
    BOOST_CHECK_EQUAL(user->GetData().back()->GetData().GetStr(), "YYY");
    user->SetRefGeneTrackingCollaborator("YYY2");
    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingCollaborator(), "YYY2");
    BOOST_CHECK_EQUAL(user->GetData().back()->GetLabel().GetStr(), "Collaborator");
    BOOST_CHECK_EQUAL(user->GetData().back()->GetData().GetStr(), "YYY2");
    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingStatus(), CUser_object::eRefGeneTrackingStatus_INFERRED);
    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingGenomicSource(), "XXX2");
    BOOST_CHECK_EQUAL(user->GetData().size(), 3);

    BOOST_CHECK_EQUAL(user->IsSetRefGeneTrackingCollaboratorURL(), false);
    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingCollaboratorURL(), kEmptyStr);
    user->SetRefGeneTrackingCollaboratorURL("ZZZ");
    BOOST_CHECK_EQUAL(user->IsSetRefGeneTrackingCollaboratorURL(), true);
    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingCollaboratorURL(), "ZZZ");
    BOOST_CHECK_EQUAL(user->GetData().back()->GetLabel().GetStr(), "CollaboratorURL");
    BOOST_CHECK_EQUAL(user->GetData().back()->GetData().GetStr(), "ZZZ");
    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingStatus(), CUser_object::eRefGeneTrackingStatus_INFERRED);
    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingGenomicSource(), "XXX2");
    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingCollaborator(), "YYY2");
    BOOST_CHECK_EQUAL(user->GetData().size(), 4);

    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingGenerated(), false);
    user->SetRefGeneTrackingGenerated(true);
    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingGenerated(), true);
    BOOST_CHECK_EQUAL(user->GetData().back()->GetLabel().GetStr(), "Generated");
    BOOST_CHECK_EQUAL(user->GetData().back()->GetData().GetBool(), true);

    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingStatus(), CUser_object::eRefGeneTrackingStatus_INFERRED);
    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingGenomicSource(), "XXX2");
    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingCollaborator(), "YYY2");
    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingCollaboratorURL(), "ZZZ");
    BOOST_CHECK_EQUAL(user->GetData().size(), 5);

    BOOST_CHECK_EQUAL(user->IsSetRefGeneTrackingIdenticalTo(), false);
    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingIdenticalTo(), CConstRef<CUser_object::CRefGeneTrackingAccession>(NULL));
    CRef<CUser_object::CRefGeneTrackingAccession> ident(new CUser_object::CRefGeneTrackingAccession("AY12345"));
    user->SetRefGeneTrackingIdenticalTo(*ident);
    BOOST_CHECK_EQUAL(user->IsSetRefGeneTrackingIdenticalTo(), true);
    CConstRef<CUser_object::CRefGeneTrackingAccession> r_ident = user->GetRefGeneTrackingIdenticalTo();
    BOOST_CHECK_EQUAL(r_ident->GetAccession(), "AY12345");
    BOOST_CHECK_EQUAL(user->GetData().size(), 6);

    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingAssembly().size(), 0);
    vector<CConstRef<CUser_object::CRefGeneTrackingAccession> > assembly;
    assembly.push_back(CConstRef<CUser_object::CRefGeneTrackingAccession>
                       (new CUser_object::CRefGeneTrackingAccession
                        ("XXX", GI_CONST(123), 0, 100, "comment1", "name1")));
    assembly.push_back(CConstRef<CUser_object::CRefGeneTrackingAccession>
                       (new CUser_object::CRefGeneTrackingAccession
                        ("YYY", GI_CONST(124), 10, 1100, "comment2", "name2")));
    user->SetRefGeneTrackingAssembly(assembly);
    BOOST_CHECK_EQUAL(user->GetData().size(), 7);

    CUser_object::TRefGeneTrackingAccessions r_assembly = user->GetRefGeneTrackingAssembly();
    BOOST_CHECK_EQUAL(r_assembly.size(), 2);
    BOOST_CHECK_EQUAL(r_assembly.front()->GetAccession(), "XXX");
    BOOST_CHECK_EQUAL(r_assembly.front()->GetGI(), GI_CONST(123));
    BOOST_CHECK_EQUAL(r_assembly.front()->GetFrom(), 0);
    BOOST_CHECK_EQUAL(r_assembly.front()->GetTo(), 100);
    BOOST_CHECK_EQUAL(r_assembly.front()->GetName(), "name1");
    BOOST_CHECK_EQUAL(r_assembly.front()->GetComment(), "comment1");
    BOOST_CHECK_EQUAL(r_assembly.back()->GetAccession(), "YYY");
    BOOST_CHECK_EQUAL(r_assembly.back()->GetGI(), GI_CONST(124));
    BOOST_CHECK_EQUAL(r_assembly.back()->GetFrom(), 10);
    BOOST_CHECK_EQUAL(r_assembly.back()->GetTo(), 1100);
    BOOST_CHECK_EQUAL(r_assembly.back()->GetName(), "name2");
    BOOST_CHECK_EQUAL(r_assembly.back()->GetComment(), "comment2");

    user->ResetRefGeneTrackingAssembly();
    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingAssembly().size(), 0);
    BOOST_CHECK_EQUAL(user->GetData().size(), 6);
    user->ResetRefGeneTrackingIdenticalTo();
    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingIdenticalTo(), CConstRef<CUser_object::CRefGeneTrackingAccession>(NULL));
    BOOST_CHECK_EQUAL(user->GetData().size(), 5);
    user->ResetRefGeneTrackingCollaboratorURL();
    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingCollaboratorURL(), kEmptyStr);
    BOOST_CHECK_EQUAL(user->GetData().size(), 4);
    user->ResetRefGeneTrackingCollaborator();
    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingCollaborator(), kEmptyStr);
    BOOST_CHECK_EQUAL(user->GetData().size(), 3);
    user->ResetRefGeneTrackingGenerated();
    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingGenerated(), false);
    BOOST_CHECK_EQUAL(user->GetData().size(), 2);
    user->ResetRefGeneTrackingGenomicSource();
    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingGenomicSource(), kEmptyStr);
    BOOST_CHECK_EQUAL(user->GetData().size(), 1);
    user->ResetRefGeneTrackingStatus();
    BOOST_CHECK_EQUAL(user->GetRefGeneTrackingStatus(), CUser_object::eRefGeneTrackingStatus_NotSet);
    BOOST_CHECK_EQUAL(user->GetData().size(), 0);

    BOOST_CHECK_EQUAL(user->GetObjectType(), CUser_object::eObjectType_RefGeneTracking);
}


BOOST_AUTO_TEST_CASE(Test_IsValidEcNumberFormat)
{
    BOOST_CHECK_EQUAL(CProt_ref::IsValidECNumberFormat("1.1.2.4"), true);
    BOOST_CHECK_EQUAL(CProt_ref::IsValidECNumberFormat("1.1.2.-"), true);
    BOOST_CHECK_EQUAL(CProt_ref::IsValidECNumberFormat("1.1.2.n"), true);
    BOOST_CHECK_EQUAL(CProt_ref::IsValidECNumberFormat("11.22.33.44"), true);
    BOOST_CHECK_EQUAL(CProt_ref::IsValidECNumberFormat("11.22.n33.44"), false);
    BOOST_CHECK_EQUAL(CProt_ref::IsValidECNumberFormat("1.2.3.10"), true);
    BOOST_CHECK_EQUAL(CProt_ref::IsValidECNumberFormat("1"), false);
    BOOST_CHECK_EQUAL(CProt_ref::IsValidECNumberFormat("1.2"), false);
    BOOST_CHECK_EQUAL(CProt_ref::IsValidECNumberFormat("1.2.3"), false);

}


BOOST_AUTO_TEST_CASE(Test_IsValidLocalID)
{
    BOOST_CHECK_EQUAL(CSeq_id::IsValidLocalID(""), false);
    BOOST_CHECK_EQUAL(CSeq_id::IsValidLocalID("ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXY"), false);
    BOOST_CHECK_EQUAL(CSeq_id::IsValidLocalID("ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWX"), true);
    BOOST_CHECK_EQUAL(CSeq_id::IsValidLocalID("nuc1"), true);
    BOOST_CHECK_EQUAL(CSeq_id::IsValidLocalID("nuc 1"), false);
    BOOST_CHECK_EQUAL(CSeq_id::IsValidLocalID("nuc>1"), false);
    BOOST_CHECK_EQUAL(CSeq_id::IsValidLocalID("nuc[1"), false);
    BOOST_CHECK_EQUAL(CSeq_id::IsValidLocalID("nuc]1"), false);
    BOOST_CHECK_EQUAL(CSeq_id::IsValidLocalID("nuc|1"), false);
    BOOST_CHECK_EQUAL(CSeq_id::IsValidLocalID("nuc=1"), true);
    BOOST_CHECK_EQUAL(CSeq_id::IsValidLocalID("nuc\"1"), false);
    BOOST_CHECK_EQUAL(CSeq_id::IsValidLocalID("nuc$1"), true);
    BOOST_CHECK_EQUAL(CSeq_id::IsValidLocalID("nuc@1"), true);
    BOOST_CHECK_EQUAL(CSeq_id::IsValidLocalID("nuc{1"), true);
}


BOOST_AUTO_TEST_CASE(Test_Unverified)
{
    CRef<CUser_object> obj(new CUser_object());
    BOOST_CHECK_EQUAL(obj->IsUnverified(), false);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedContaminant(), false);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedFeature(), false);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedMisassembled(), false);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedOrganism(), false);

    obj->SetObjectType(CUser_object::eObjectType_Unverified);
    BOOST_CHECK_EQUAL(obj->IsUnverified(), true);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedContaminant(), false);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedFeature(), false);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedMisassembled(), false);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedOrganism(), false);

    obj->AddUnverifiedContaminant();
    BOOST_CHECK_EQUAL(obj->IsUnverified(), true);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedContaminant(), true);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedFeature(), false);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedMisassembled(), false);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedOrganism(), false);

    obj->AddUnverifiedFeature();
    BOOST_CHECK_EQUAL(obj->IsUnverified(), true);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedContaminant(), true);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedFeature(), true);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedMisassembled(), false);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedOrganism(), false);

    obj->AddUnverifiedMisassembled();
    BOOST_CHECK_EQUAL(obj->IsUnverified(), true);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedContaminant(), true);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedFeature(), true);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedMisassembled(), true);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedOrganism(), false);

    obj->AddUnverifiedOrganism();
    BOOST_CHECK_EQUAL(obj->IsUnverified(), true);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedContaminant(), true);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedFeature(), true);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedMisassembled(), true);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedOrganism(), true);

    obj->RemoveUnverifiedContaminant();
    BOOST_CHECK_EQUAL(obj->IsUnverified(), true);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedContaminant(), false);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedFeature(), true);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedMisassembled(), true);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedOrganism(), true);

    obj->RemoveUnverifiedFeature();
    BOOST_CHECK_EQUAL(obj->IsUnverified(), true);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedContaminant(), false);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedFeature(), false);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedMisassembled(), true);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedOrganism(), true);

    obj->RemoveUnverifiedMisassembled();
    BOOST_CHECK_EQUAL(obj->IsUnverified(), true);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedContaminant(), false);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedFeature(), false);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedMisassembled(), false);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedOrganism(), true);

    obj->RemoveUnverifiedOrganism();
    BOOST_CHECK_EQUAL(obj->IsUnverified(), true);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedContaminant(), false);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedFeature(), false);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedMisassembled(), false);
    BOOST_CHECK_EQUAL(obj->IsUnverifiedOrganism(), false);

}


BOOST_AUTO_TEST_CASE(Test_LegalQualsAny)
{
    size_t count = 0;
    auto all_quals = CSeqFeatData::GetLegalQualifiers(CSeqFeatData::eSubtype_any);
    BOOST_CHECK_EQUAL(all_quals.size(), 134);
    for (auto b : all_quals)
        ++count;
    BOOST_CHECK_EQUAL(count, 134);

    auto empty_quals = CSeqFeatData::GetLegalQualifiers(CSeqFeatData::eSubtype_clone);
    BOOST_CHECK_EQUAL(empty_quals.size(), 0);
    count = 0;
    for (auto b : empty_quals)
        ++count;
    BOOST_CHECK_EQUAL(count, 0);

    auto mandatory = CSeqFeatData::GetMandatoryQualifiers(CSeqFeatData::eSubtype_assembly_gap);

    for (auto rec: mandatory)
    {
        std::cout << rec << std::endl;
    }
}

BOOST_AUTO_TEST_CASE(Test_GetQualifierTypeAndCheckCase)
{
    auto test1 = CSeqFeatData::GetQualifierTypeAndValue("host");
    BOOST_CHECK(test1.first == CSeqFeatData::eQual_host);
    BOOST_CHECK_EQUAL(test1.second, "host");
    auto test2 = CSeqFeatData::GetQualifierTypeAndValue("Host");
    BOOST_CHECK(test2.first == CSeqFeatData::eQual_host);
    BOOST_CHECK_EQUAL(test2.second, "host");
    auto test3 = CSeqFeatData::GetQualifierTypeAndValue("specific_host");
    BOOST_CHECK(test3.first == CSeqFeatData::eQual_host);
    BOOST_CHECK_EQUAL(test3.second, "specific_host");
    auto test4 = CSeqFeatData::GetQualifierTypeAndValue("Specific_host");
    BOOST_CHECK(test4.first == CSeqFeatData::eQual_host);
    BOOST_CHECK_EQUAL(test4.second, "specific_host");

    auto test5 = CSeqFeatData::GetQualifierAsString(CSeqFeatData::eQual_host);
    BOOST_CHECK_EQUAL(test5, "host");
}
BOOST_AUTO_TEST_CASE(Test_x_ExhonerateQualifier)
{
    auto& orig = CSeqFeatData::GetMandatoryQualifiers(CSeqFeatData::eSubtype_assembly_gap);
    size_t orig_size = orig.size();
    BOOST_CHECK(orig_size == 2);
    std::vector<CSeqFeatData::EQualifier> mandatory = orig;
    CSeqFeatData::EQualifier not_mandatory = CSeqFeatData::eQual_estimated_length;
    bool res = false;
    ERASE_ITERATE(std::vector<CSeqFeatData::EQualifier>, it, mandatory) {
        if (*it == not_mandatory) {
            res = true;
            VECTOR_ERASE(it, mandatory);
        }
    }

    BOOST_CHECK(mandatory.size() == 1);

    auto mandatory_copy = orig;
    bool res2 = mandatory_copy.reset(not_mandatory);
    BOOST_CHECK(res == res2);
    BOOST_CHECK(mandatory_copy.size() == 1);

    bool res3 = mandatory_copy.set(not_mandatory);
    BOOST_CHECK(res != res3);
    BOOST_CHECK(mandatory_copy.size() == 2);

}

