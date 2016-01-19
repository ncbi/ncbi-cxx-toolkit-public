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
 * Authors:  Vahram Avagyan
 *
 * File Description:
 *   CSeqAlignFilter unit test.
 *
 */

#include <ncbi_pch.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/general/Object_id.hpp>

#include <algo/blast/api/seqinfosrc_seqdb.hpp>
#include <algo/blast/api/blast_seqinfosrc_aux.hpp>

#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <objtools/align_format/seqalignfilter.hpp>

#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/iterator.hpp>
#include <sstream>
#undef NCBI_BOOST_NO_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(align_format);

template<class ASNOBJ>
void s_Stringify(const ASNOBJ & a, string & s)
{
    CNcbiOstrstream oss;
    oss << MSerial_AsnText << a;
    s = CNcbiOstrstreamToString(oss);
}

template<class ASNOBJ>
void s_Unstringify(const string & s, ASNOBJ & a)
{
    istringstream iss;
    iss.str(s);
    iss >> MSerial_AsnText >> a;
}

template<class ASNOBJ>
CRef<ASNOBJ> s_Duplicate(const ASNOBJ & a)
{
    CRef<ASNOBJ> newobj(new ASNOBJ);
    
    string s;
    s_Stringify(a, s);
    s_Unstringify(s, *newobj);
    
    return newobj;
}

/////////////////////////////////////////////////////////////////////////////
// List-based static helper functions

static void s_GetUseThisGiEntries(CRef<CSeq_align> sa, list<TGi>& list_gis)
{
    list_gis.clear();

    CSeq_align::TScore& score_entries = sa->SetScore();
    CSeq_align::TScore::iterator iter_score = score_entries.begin();
    while (iter_score != score_entries.end())
    {
        CRef<CScore> score_entry = *iter_score++;
        if (score_entry->CanGetId() && score_entry->GetId().IsStr())
        {
            string str_id = score_entry->GetId().GetStr();
            if (str_id == "use_this_gi")
            {
                bool bIsLegalGiEntry = score_entry->CanGetValue() && score_entry->GetValue().IsInt();
                BOOST_REQUIRE(bIsLegalGiEntry);

                list_gis.push_back(GI_FROM(int, score_entry->GetValue().GetInt()));
            }
        }
    }
}

static TGi s_GetAlignedSeqGi(CRef<CSeq_align> sa)
{
    CConstRef<CSeq_id> id(&(sa->GetSeq_id(1)));

    BOOST_REQUIRE(id->IsGi());
    return id->GetGi();
}

static void s_GetFullGiList(CRef<CSeq_align> sa, list<TGi>& list_gis)
{
    s_GetUseThisGiEntries(sa, list_gis);
    list_gis.push_back(s_GetAlignedSeqGi(sa));
}

static bool s_IsGiInList(TGi gi, list<TGi>& list_gis)
{
    return find(list_gis.begin(), list_gis.end(), gi) != list_gis.end();
}

static bool s_IsListSubset(list<TGi>& list_all, list<TGi>& list_sub)
{
    bool is_missing = false;

    list<TGi>::iterator it;
    for (it = list_sub.begin(); it != list_sub.end() && !is_missing; it++)
    {
        is_missing = !s_IsGiInList(*it, list_all);
    }

    return !is_missing;
}

static bool s_AreListsEqual(list<TGi>& list1, list<TGi>& list2)
{
    return s_IsListSubset(list1, list2) && s_IsListSubset(list2, list1);
}

/////////////////////////////////////////////////////////////////////////////
// Vector-based static helper functions

static bool s_IsGiInVector(TGi gi, vector<TGi>& vec_gis)
{
    return binary_search(vec_gis.begin(), vec_gis.end(), gi);
}

static bool s_GetFilteredGiList(CRef<CSeq_align> sa, vector<TGi>& vec_all_gis,
                               list<TGi>& list_sa_filtered)
{
    list<TGi> list_sa_full;
    s_GetFullGiList(sa, list_sa_full);

    for (list<TGi>::iterator it = list_sa_full.begin();
         it != list_sa_full.end(); it++)
    {
        if (s_IsGiInVector(*it, vec_all_gis))
        {
            list_sa_filtered.push_back(*it);
        }
    }

    return !list_sa_filtered.empty();
}

/////////////////////////////////////////////////////////////////////////////
// Functions to test filtering results for individual seqaligns

static void 
s_Check_GiListConsistency(CRef<CSeq_align> /*sa_orig*/, 
                          CRef<CSeq_align> sa_new,
                          list<TGi>& list_orig_filtered, 
                          list<TGi>& list_new_filtered)
{
    list<TGi> list_new;
    s_GetFullGiList(sa_new, list_new);

    BOOST_REQUIRE(s_AreListsEqual(list_new, list_new_filtered));    // new list is indeed filtered
    BOOST_REQUIRE(s_IsListSubset(list_new, list_orig_filtered));    // all original gi's who survived filtering
                                                            // are included in the new list
}

static void s_Check_GiEquivalenceInDB(TGi gi1, TGi gi2, CRef<CSeqDB> db)
{
    int oid1 = -1, oid2 = -1;
    db->GiToOid(gi1, oid1);
    db->GiToOid(gi2, oid2);

    BOOST_REQUIRE(oid1 > 0);
    BOOST_REQUIRE(oid2 > 0);
    BOOST_REQUIRE(oid1 == oid2);
}

/////////////////////////////////////////////////////////////////////////////
// Pre-processing and testing individual seqaligns

static void s_DoConsistencyCheck(CRef<CSeq_align> sa_orig, CRef<CSeq_align> sa_new,
                                        vector<TGi>& vec_all_gis)
{
    list<TGi> list_orig_filtered;
    list<TGi> list_new, list_new_filtered;

    s_GetFilteredGiList(sa_orig, vec_all_gis, list_orig_filtered);
    s_GetFilteredGiList(sa_new, vec_all_gis, list_new_filtered);

    s_Check_GiListConsistency(sa_orig, sa_new,
                            list_orig_filtered, list_new_filtered);
}

static void s_DoEquivalenceCheck(CRef<CSeq_align> sa_new, CRef<CSeqDB> db)
{
    TGi main_gi = s_GetAlignedSeqGi(sa_new);

    list<TGi> list_extra_gis;
    s_GetUseThisGiEntries(sa_new, list_extra_gis);

    for (list<TGi>::iterator it_extra_gi = list_extra_gis.begin();
         it_extra_gi != list_extra_gis.end(); it_extra_gi++)
    {
        s_Check_GiEquivalenceInDB(main_gi, *it_extra_gi, db);
    }
}

/////////////////////////////////////////////////////////////////////////////
// Other pre-processing

static void s_LoadSeqAlignsFromFile(CSeq_align_set& aln_all, const string& fname)
{
    auto_ptr<CObjectIStream> asn_in(CObjectIStream::Open(fname, eSerial_AsnText));
    *asn_in >> aln_all;
}

/////////////////////////////////////////////////////////////////////////////
// Actual test cases

BOOST_AUTO_TEST_SUITE(seqalignfilter)

BOOST_AUTO_TEST_CASE(s_TestSimpleFiltering)
{
    string fname_in = "data/in_test.txt";
    string fname_out = "data/out_test.txt";
    string fname_gis = "data/gilist_test.txt";

	CSeq_align_set aln_all;
    s_LoadSeqAlignsFromFile(aln_all, fname_in);

    CSeqAlignFilter filter;
    filter.FilterSeqaligns(fname_in, fname_out, fname_gis);

    CSeq_align_set aln_filtered;
    s_LoadSeqAlignsFromFile(aln_filtered, fname_out);

    list<TGi> list_gis;
    filter.ReadGiList(fname_gis, list_gis);

    ITERATE(CSeq_align_set::Tdata, iter, aln_all.Get())
    {
        TGi gi = s_GetAlignedSeqGi(*iter);
        if (s_IsGiInList(gi, list_gis))
        {
            bool found_gi = false;
            ITERATE(CSeq_align_set::Tdata, iter_filtered, aln_filtered.Get())
            {
                TGi gi_filtered = s_GetAlignedSeqGi(*iter_filtered);
                if (gi == gi_filtered)
                {
                    found_gi = true;
                    break;
                }
            }
            BOOST_REQUIRE(found_gi);
        }
    }
}

BOOST_AUTO_TEST_CASE(s_TestDBBasedFiltering)
{
    string fname_in = "data/in_test.txt";
    string fname_out = "data/out_test.txt";
    string fname_gis = "data/gilist_test.txt";

    string db_name = "nr";
    bool use_prot = true;

    CSeqAlignFilter filter;
    CRef<CSeqDB> db;

    BOOST_REQUIRE_NO_THROW(db = filter.PrepareSeqDB(db_name, use_prot, fname_gis););
    BOOST_REQUIRE_NO_THROW(filter.FilterSeqalignsExt(fname_in, fname_out, db););

    // check the results

    CSeq_align_set aln_all;
    s_LoadSeqAlignsFromFile(aln_all, fname_in);

    CSeq_align_set aln_filtered;
    s_LoadSeqAlignsFromFile(aln_filtered, fname_out);

    vector<TGi> vec_gis;    // sorted vector of all available gi's
    filter.ReadGiVector(fname_gis, vec_gis, true);

    ITERATE(CSeq_align_set::Tdata, iter, aln_all.Get())
    {
        TGi gi = s_GetAlignedSeqGi(*iter);
        ITERATE(CSeq_align_set::Tdata, iter_filtered, aln_filtered.Get())
        {
            TGi gi_filtered = s_GetAlignedSeqGi(*iter_filtered);
            if (gi == gi_filtered)
            {
                // main gi's coincide - check the concistency of all the gi's
                s_DoConsistencyCheck(*iter, *iter_filtered, vec_gis);
                // check the equivalence of all gi's in the filtered seqalign
                s_DoEquivalenceCheck(*iter_filtered, db);
            }
        }
    }
}
BOOST_AUTO_TEST_SUITE_END()
#endif /* SKIP_DOXYGEN_PROCESSING */
