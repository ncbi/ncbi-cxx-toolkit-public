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
* Author:  Christiam Camacho
*
* File Description:
*   Unit tests for functionality to convert CLUSTALW-style MSAs to PSSMs
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/msa_pssm_input.hpp>

#include <corelib/test_boost.hpp>

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;

BOOST_AUTO_TEST_SUITE(msa2pssm)

BOOST_AUTO_TEST_CASE(QueryNotFoundInMsa)
{
    ifstream in("data/msa.clustalw.txt");

    const unsigned int  kQuerySize = 10;
    const unsigned char kQuery[] = { 3, 9, 14, 20, 6, 23, 1, 7, 16, 5 };

    CPSIBlastOptions opts;
    PSIBlastOptionsNew(&opts);
    auto_ptr<CPsiBlastInputClustalW> pssm_input;
    BOOST_REQUIRE_THROW(pssm_input.reset(new CPsiBlastInputClustalW(in, *opts,
                                                                    0, 0,
                                                                    kQuery,
                                                                    kQuerySize,
                                                                    0, 0)),
                        CBlastException);
}

BOOST_AUTO_TEST_CASE(AllUpperCaseMsa)
{
    ifstream in("data/msa.clustalw.txt");

    CPSIBlastOptions opts;
    PSIBlastOptionsNew(&opts);
    CPsiBlastInputClustalW pssm_input(in, *opts);
    pssm_input.Process();

    PSIMsa* msa = pssm_input.GetData();
    BOOST_REQUIRE_EQUAL((Uint4)151, msa->dimensions->query_length);
    BOOST_REQUIRE_EQUAL((Uint4)24, msa->dimensions->num_seqs);

    // Ensure there are no gaps in the query
    for (TSeqPos i = 0; i < msa->dimensions->query_length; i++) {
        CNcbiOstrstream os;
        os << "Query has gap in position " << i;
        BOOST_REQUIRE_MESSAGE(msa->data[0][i].letter != 
                              AMINOACID_TO_NCBISTDAA[(int)'-'],
                              (string)CNcbiOstrstreamToString(os));
    }
}

BOOST_AUTO_TEST_CASE(AllUpperCaseMsa_WithQuery)
{
    ifstream in("data/msa.clustalw.txt");

    const string kQuerySeq("IVLARIDDRFIHGQILTRWIKVHAADRIIVVSDDIAQDEMRKTLILSVAPSNVKASAVSVSKMAKAFHSPRYEGVTAMLLFENPSDIVSLIEAGVPIKTVNVGGMRFENHRRQITKSVSVTEQDIKAFETLSDKGVKLELRQLPSDASEDF");
    TAutoUint1ArrayPtr query(new Uint1[kQuerySeq.size()]);
    int i = 0;
    ITERATE(string, res, kQuerySeq) {
        query.get()[i] = AMINOACID_TO_NCBISTDAA[(int)*res];
        i++;
    }

    CPSIBlastOptions opts;
    PSIBlastOptionsNew(&opts);
    CPsiBlastInputClustalW pssm_input(in, *opts, 0, 0, query.get(), kQuerySeq.size(), 0, 0);
    pssm_input.Process();

    PSIMsa* msa = pssm_input.GetData();
    BOOST_REQUIRE_EQUAL((Uint4)151, msa->dimensions->query_length);
    BOOST_REQUIRE_EQUAL((Uint4)24, msa->dimensions->num_seqs);

    // Ensure there are no gaps in the query
    for (TSeqPos i = 0; i < pssm_input.GetQueryLength(); i++) {
        CNcbiOstrstream os;
        os << "Query has gap in position " << i;
        BOOST_REQUIRE_MESSAGE(msa->data[0][i].letter != 
                              AMINOACID_TO_NCBISTDAA[(int)'-'],
                              (string)CNcbiOstrstreamToString(os));
    }
}

BOOST_AUTO_TEST_CASE(MsaWithLowerCaseResidues)
{
    ifstream in("data/sample_msa.txt");

    CPSIBlastOptions opts;
    PSIBlastOptionsNew(&opts);
    CPsiBlastInputClustalW pssm_input(in, *opts);
    BOOST_REQUIRE_EQUAL(string(BLAST_DEFAULT_MATRIX),
                        string(pssm_input.GetMatrixName()));
    pssm_input.Process();

    PSIMsa* msa = pssm_input.GetData();
    BOOST_REQUIRE_EQUAL((Uint4)176, msa->dimensions->query_length);
    BOOST_REQUIRE_EQUAL((Uint4)13, msa->dimensions->num_seqs);

    // Check the aligned query regions
    TSeqRange unused_range(23, 93);
    for (TSeqPos i = 0; i < unused_range.GetFrom(); i++) {
        CNcbiOstrstream os;
        os << "Query is not aligned at position " << i;
        BOOST_REQUIRE_MESSAGE(msa->data[0][i].is_aligned == TRUE,
                              (string)CNcbiOstrstreamToString(os));
    }
    for (TSeqPos i = unused_range.GetFrom(); i < unused_range.GetTo(); i++) 
    {
        CNcbiOstrstream os;
        os << "Query is aligned at position " << i;
        BOOST_REQUIRE_MESSAGE(msa->data[0][i].is_aligned == FALSE,
                              (string)CNcbiOstrstreamToString(os));
    }
    for (TSeqPos i = unused_range.GetToOpen(); i < pssm_input.GetQueryLength();
         i++) {
        CNcbiOstrstream os;
        os << "Query is not aligned at position " << i;
        BOOST_REQUIRE_MESSAGE(msa->data[0][i].is_aligned == TRUE,
                              (string)CNcbiOstrstreamToString(os));
    }
    // Ensure there are no gaps in the query
    for (TSeqPos i = 0; i < pssm_input.GetQueryLength(); i++) {
        CNcbiOstrstream os;
        os << "Query has gap in position " << i;
        BOOST_REQUIRE_MESSAGE(msa->data[0][i].letter != 
                              AMINOACID_TO_NCBISTDAA[(int)'-'],
                              (string)CNcbiOstrstreamToString(os));
    }
}

BOOST_AUTO_TEST_SUITE_END()
