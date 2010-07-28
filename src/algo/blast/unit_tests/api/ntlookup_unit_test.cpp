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
* Author:  Tom Madden
*
* File Description:
*   Unit test module for the nucleotide lookup tables.
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

#include <corelib/ncbitime.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <serial/serial.hpp>
#include <serial/iterator.hpp>
#include <serial/objostr.hpp>

#include <algo/blast/api/bl2seq.hpp>
#include <blast_objmgr_priv.hpp>

#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/blast_prot_options.hpp>
#include <algo/blast/api/blastx_options.hpp>
#include <algo/blast/api/tblastn_options.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include <algo/blast/api/disc_nucl_options.hpp>
#include <algo/blast/core/blast_nalookup.h>
#include <algo/blast/core/lookup_util.h>

#include "test_objmgr.hpp"
#include "blast_test_util.hpp"

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;
using namespace TestUtil;

#define NULL_NUCL_SENTINEL 0xf
#define SMALL_QUERY_GI 1945386
#define LARGE_QUERY_GI 19572546

struct NtlookupTestFixture {

	BLAST_SequenceBlk *query_blk;
	BlastSeqLoc* lookup_segments;

    NtlookupTestFixture() {
        query_blk = NULL;
        lookup_segments = NULL;
    }

    ~NtlookupTestFixture() {
        query_blk = BlastSequenceBlkFree(query_blk);
        lookup_segments = BlastSeqLocFree(lookup_segments);
    }

    void SetUpQuery(Uint4 query_gi)
    {
        char buf[64];
        Int4 status;
        // load the query
        sprintf(buf, "gi|%d", query_gi);
        CSeq_id id(buf);
        
        auto_ptr<SSeqLoc> ssl(CTestObjMgr::Instance().CreateSSeqLoc(
                                                   id, eNa_strand_both));
    
        SBlastSequence sequence(
                    GetSequence(*ssl->seqloc,
                                eBlastEncodingNucleotide,
                                ssl->scope,
                                eNa_strand_both,
                                eSentinels));
    
        // create the sequence block. The size to pass in
        // must not include the sentinel bytes on either
        // end of the sequence
    
        query_blk = NULL;
        status = BlastSeqBlkNew(&query_blk);
        BOOST_REQUIRE_EQUAL(0, status);
        status = BlastSeqBlkSetSequence(query_blk, sequence.data.release(),
                               sequence.length - 2);
        BOOST_REQUIRE_EQUAL(0, status);

        BOOST_REQUIRE(query_blk != NULL);
        BOOST_REQUIRE(query_blk->sequence != NULL);
        BOOST_REQUIRE(query_blk->length > 0);

        BOOST_REQUIRE(query_blk != NULL);
        BOOST_REQUIRE(query_blk->sequence[0] != NULL_NUCL_SENTINEL);
        BOOST_REQUIRE(query_blk->sequence[query_blk->length - 1] != 
                                                NULL_NUCL_SENTINEL);
        BOOST_REQUIRE(query_blk->sequence_start[0] == NULL_NUCL_SENTINEL);
        BOOST_REQUIRE(query_blk->sequence_start[query_blk->length + 1] == 
                                                NULL_NUCL_SENTINEL);

        lookup_segments = 0;
        if (query_gi == SMALL_QUERY_GI) {
            BlastSeqLocNew(&lookup_segments, 0, 1649);
            BlastSeqLocNew(&lookup_segments, 1656, 2756);
            BlastSeqLocNew(&lookup_segments, 2789, 3889);
            BlastSeqLocNew(&lookup_segments, 3896, 5544);
        }
        else {
            BlastSeqLocNew(&lookup_segments, 0, (query_blk->length - 1)/2-1);
            BlastSeqLocNew(&lookup_segments, (query_blk->length - 1) / 2 + 1, 
                                            query_blk->length - 1);
        }
    }

    // word_size is word-size
    // alphabet_size is alphabet size (typically 4 for nucleotides).
    void debruijnInit(int word_size, int alphabet_size) {

        // get length of sequence.
        int len = iexp(alphabet_size,word_size) + (word_size-1);	

        /* leave room for and pad with sentinels */
        Uint1* sequence = (Uint1*) malloc(len + 2);
        sequence[0] = NULL_NUCL_SENTINEL;
        sequence[len+1] = NULL_NUCL_SENTINEL;

        debruijn(word_size,alphabet_size,sequence+1,0);  // generate sequence

        for(int i=1;i<word_size;i++)
          sequence[len-word_size+1+i] = sequence[i];

        /* create sequence block */
        query_blk = 0;
        BlastSetUp_SeqBlkNew(sequence, len, &query_blk, TRUE);

        /* indicate region of query to index */
        lookup_segments = 0;
        BlastSeqLocNew(&lookup_segments, 0, len-1);

    }
};

BOOST_FIXTURE_TEST_SUITE(ntlookup, NtlookupTestFixture)

BOOST_AUTO_TEST_CASE(testStdLookupTable) {
    SetUpQuery(SMALL_QUERY_GI);

	LookupTableOptions* lookup_options;
	LookupTableOptionsNew(eBlastTypeBlastn, &lookup_options);
	BLAST_FillLookupTableOptions(lookup_options, 
                                     eBlastTypeBlastn, 
                                     FALSE, 0, 0);

		
    QuerySetUpOptions* query_options;
    BlastQuerySetUpOptionsNew(&query_options);
	LookupTableWrap* lookup_wrap_ptr;
 	BOOST_REQUIRE_EQUAL((int)LookupTableWrapInit(query_blk, 
                             lookup_options, query_options, lookup_segments, 
                             0, &lookup_wrap_ptr, NULL, NULL), 0);
    query_options = BlastQuerySetUpOptionsFree(query_options);
    BOOST_REQUIRE_EQUAL(eSmallNaLookupTable,
                        (ELookupTableType)lookup_wrap_ptr->lut_type);

	BlastSmallNaLookupTable* lookup = 
                        (BlastSmallNaLookupTable*) lookup_wrap_ptr->lut;
	BOOST_REQUIRE_EQUAL(65536, lookup->backbone_size); 
	BOOST_REQUIRE_EQUAL(4, lookup->longest_chain); 
	BOOST_REQUIRE_EQUAL(1444, lookup->overflow_size); 
	BOOST_REQUIRE_EQUAL((Int2)2819, lookup->final_backbone[48]);
	BOOST_REQUIRE_EQUAL((Int2)754, lookup->final_backbone[42889]);
	BOOST_REQUIRE_EQUAL((Int2)(-345), lookup->final_backbone[21076]);

	lookup_wrap_ptr = LookupTableWrapFree(lookup_wrap_ptr);
	lookup_options = LookupTableOptionsFree(lookup_options);
}

BOOST_AUTO_TEST_CASE(testMegablastLookupTable)
{
    SetUpQuery(LARGE_QUERY_GI);

	LookupTableOptions* lookup_options;
	LookupTableOptionsNew(eBlastTypeBlastn, &lookup_options);
	BLAST_FillLookupTableOptions(lookup_options, eBlastTypeBlastn, 
                                     TRUE, 0, 0);

    QuerySetUpOptions* query_options;
    BlastQuerySetUpOptionsNew(&query_options);
	LookupTableWrap* lookup_wrap_ptr;
 	BOOST_REQUIRE_EQUAL((int)LookupTableWrapInit(query_blk, 
                                       lookup_options, query_options, lookup_segments, 
                                       0, &lookup_wrap_ptr, NULL, NULL), 0);
        query_options = BlastQuerySetUpOptionsFree(query_options);
	BOOST_REQUIRE_EQUAL((ELookupTableType)lookup_wrap_ptr->lut_type, 
                             eMBLookupTable);

	BlastMBLookupTable* lookup = (BlastMBLookupTable*) lookup_wrap_ptr->lut;
	BOOST_REQUIRE_EQUAL(4194304, lookup->hashsize);
	BOOST_REQUIRE_EQUAL(28, (int)lookup->word_length); 
	BOOST_REQUIRE_EQUAL(18, lookup->scan_step);
	BOOST_REQUIRE_EQUAL(37, lookup->longest_chain);
	BOOST_REQUIRE_EQUAL(7, lookup->pv_array_bts);
	BOOST_REQUIRE_EQUAL(5868, lookup->hashtable[36604]);
	BOOST_REQUIRE_EQUAL(14646, lookup->hashtable[1426260]);
	BOOST_REQUIRE_EQUAL(290, lookup->hashtable[4007075]);
        
    int pv_array_size = (lookup->hashsize >> lookup->pv_array_bts);
	int pv_array_hash =
            EndianIndependentBufferHash((char*) lookup->pv_array,
                                        pv_array_size * sizeof(PV_ARRAY_TYPE),
                                        sizeof(PV_ARRAY_TYPE));
	BOOST_REQUIRE_EQUAL(-729205454, pv_array_hash);

	lookup_wrap_ptr = LookupTableWrapFree(lookup_wrap_ptr);
 	lookup_options = LookupTableOptionsFree(lookup_options);
}

BOOST_AUTO_TEST_CASE(testDiscontiguousMBLookupTableCodingWordSize11) {

    SetUpQuery(SMALL_QUERY_GI);
	LookupTableOptions* lookup_options;
	LookupTableOptionsNew(eBlastTypeBlastn, &lookup_options);
	BLAST_FillLookupTableOptions(lookup_options, eBlastTypeBlastn, 
                                     TRUE, 0, 11);
	lookup_options->mb_template_length = 16; 
	lookup_options->mb_template_type = eMBWordCoding;

    QuerySetUpOptions* query_options;
    BlastQuerySetUpOptionsNew(&query_options);
	LookupTableWrap* lookup_wrap_ptr;
 	BOOST_REQUIRE_EQUAL((int)LookupTableWrapInit(query_blk, 
                                          lookup_options, query_options, lookup_segments, 
                                          0, &lookup_wrap_ptr, NULL, NULL), 0);
        query_options = BlastQuerySetUpOptionsFree(query_options);
	BOOST_REQUIRE_EQUAL(eMBLookupTable, (ELookupTableType)lookup_wrap_ptr->lut_type);

	BlastMBLookupTable* lookup = (BlastMBLookupTable*) lookup_wrap_ptr->lut;
	BOOST_REQUIRE_EQUAL(4194304, lookup->hashsize); // 4**11
	BOOST_REQUIRE_EQUAL(11, (int)lookup->word_length); 
	BOOST_REQUIRE_EQUAL(true, (bool)lookup->discontiguous);
	BOOST_REQUIRE_EQUAL(16, (int)lookup->template_length);
	BOOST_REQUIRE_EQUAL(1, (int)lookup->template_type);
	BOOST_REQUIRE_EQUAL(1, lookup->scan_step);
	BOOST_REQUIRE_EQUAL(2, lookup->longest_chain);
	BOOST_REQUIRE_EQUAL(49, lookup->hashtable[2463300]);
	BOOST_REQUIRE_EQUAL(392, lookup->hashtable[1663305]);
	BOOST_REQUIRE_EQUAL(1049, lookup->hashtable[3586129]);
	BOOST_REQUIRE_EQUAL(8, lookup->pv_array_bts);

    int pv_array_size = (lookup->hashsize >> lookup->pv_array_bts);
	int pv_array_hash =
            EndianIndependentBufferHash((char*) lookup->pv_array,
                                        pv_array_size * sizeof(PV_ARRAY_TYPE),
                                        sizeof(PV_ARRAY_TYPE));
	BOOST_REQUIRE_EQUAL(-160576483, pv_array_hash);

	lookup_wrap_ptr = LookupTableWrapFree(lookup_wrap_ptr);
	lookup_options = LookupTableOptionsFree(lookup_options);
}

BOOST_AUTO_TEST_CASE(testDiscontiguousMBLookupTableCodingWordSize12) {

    SetUpQuery(SMALL_QUERY_GI);
	LookupTableOptions* lookup_options;
	LookupTableOptionsNew(eBlastTypeBlastn, &lookup_options);
	BLAST_FillLookupTableOptions(lookup_options, eBlastTypeBlastn, 
                                     TRUE, 0, 12);
	lookup_options->mb_template_length = 16; 
	lookup_options->mb_template_type = eMBWordCoding;

    QuerySetUpOptions* query_options;
    BlastQuerySetUpOptionsNew(&query_options);
	LookupTableWrap* lookup_wrap_ptr;
 	BOOST_REQUIRE_EQUAL((int)LookupTableWrapInit(query_blk, 
                                        lookup_options, query_options, lookup_segments, 
                                        0, &lookup_wrap_ptr, NULL, NULL), 0);
    query_options = BlastQuerySetUpOptionsFree(query_options);
	BOOST_REQUIRE_EQUAL(eMBLookupTable, (ELookupTableType)lookup_wrap_ptr->lut_type);

	BlastMBLookupTable* lookup = (BlastMBLookupTable*) lookup_wrap_ptr->lut;
	BOOST_REQUIRE_EQUAL(16777216, lookup->hashsize); // 4**11
	BOOST_REQUIRE_EQUAL(12, (int)lookup->word_length); 
	BOOST_REQUIRE_EQUAL(true, (bool)lookup->discontiguous);
	BOOST_REQUIRE_EQUAL(16, (int)lookup->template_length);
	BOOST_REQUIRE_EQUAL(3, (int)lookup->template_type);
	BOOST_REQUIRE_EQUAL(1, lookup->scan_step);
	BOOST_REQUIRE_EQUAL(2, lookup->longest_chain);
	BOOST_REQUIRE_EQUAL(3631, lookup->hashtable[133875]);
	BOOST_REQUIRE_EQUAL(2092, lookup->hashtable[351221]);
	BOOST_REQUIRE_EQUAL(4951, lookup->hashtable[1336356]);
	BOOST_REQUIRE_EQUAL(10, lookup->pv_array_bts);

    int pv_array_size = (lookup->hashsize >> lookup->pv_array_bts);
	int pv_array_hash =
            EndianIndependentBufferHash((char*) lookup->pv_array,
                                        pv_array_size * sizeof(PV_ARRAY_TYPE),
                                        sizeof(PV_ARRAY_TYPE));
	BOOST_REQUIRE_EQUAL(-630452942, pv_array_hash);

	lookup_wrap_ptr = LookupTableWrapFree(lookup_wrap_ptr);
	lookup_options = LookupTableOptionsFree(lookup_options);
}

BOOST_AUTO_TEST_CASE(testDiscontiguousMBLookupTableOptimalWordSize11) {

    SetUpQuery(SMALL_QUERY_GI);
	LookupTableOptions* lookup_options;
	LookupTableOptionsNew(eBlastTypeBlastn, &lookup_options);
	BLAST_FillLookupTableOptions(lookup_options, eBlastTypeBlastn, 
                                     TRUE, 0, 11);
	lookup_options->mb_template_length = 16; 
	lookup_options->mb_template_type = eMBWordOptimal;

    QuerySetUpOptions* query_options;
    BlastQuerySetUpOptionsNew(&query_options);
	LookupTableWrap* lookup_wrap_ptr;
 	BOOST_REQUIRE_EQUAL((int)LookupTableWrapInit(query_blk, 
                                           lookup_options, query_options, lookup_segments, 
                                           0, &lookup_wrap_ptr, NULL, NULL), 0);
    query_options = BlastQuerySetUpOptionsFree(query_options);
	BOOST_REQUIRE_EQUAL(eMBLookupTable, (ELookupTableType)lookup_wrap_ptr->lut_type);

	BlastMBLookupTable* lookup = (BlastMBLookupTable*) lookup_wrap_ptr->lut;
	BOOST_REQUIRE_EQUAL(4194304, lookup->hashsize); // 4**11
	BOOST_REQUIRE_EQUAL(11, (int)lookup->word_length); 
	BOOST_REQUIRE_EQUAL(true, (bool)lookup->discontiguous);
	BOOST_REQUIRE_EQUAL(16, (int)lookup->template_length);
	BOOST_REQUIRE_EQUAL(2, (int)lookup->template_type);
	BOOST_REQUIRE_EQUAL(1, lookup->scan_step);
	BOOST_REQUIRE_EQUAL(2, lookup->longest_chain);
	BOOST_REQUIRE_EQUAL(36, lookup->hashtable[1353317]);
	BOOST_REQUIRE_EQUAL(375, lookup->hashtable[1955444]);
	BOOST_REQUIRE_EQUAL(5455, lookup->hashtable[1735012]);
	BOOST_REQUIRE_EQUAL(8, lookup->pv_array_bts);

    int pv_array_size = (lookup->hashsize >> lookup->pv_array_bts);
	int pv_array_hash =
            EndianIndependentBufferHash((char*) lookup->pv_array,
                                        pv_array_size * sizeof(PV_ARRAY_TYPE),
                                        sizeof(PV_ARRAY_TYPE));
	BOOST_REQUIRE_EQUAL(932347030, pv_array_hash);

	lookup_wrap_ptr = LookupTableWrapFree(lookup_wrap_ptr);
	lookup_options = LookupTableOptionsFree(lookup_options);
}

BOOST_AUTO_TEST_CASE(testDiscontiguousMBLookupTableOptimalWordSize12) {

    SetUpQuery(SMALL_QUERY_GI);
	LookupTableOptions* lookup_options;
	LookupTableOptionsNew(eBlastTypeBlastn, &lookup_options);
	BLAST_FillLookupTableOptions(lookup_options, eBlastTypeBlastn, 
                                     TRUE, 0, 12);
	lookup_options->mb_template_length = 16; 
	lookup_options->mb_template_type = eMBWordOptimal;

    QuerySetUpOptions* query_options;
    BlastQuerySetUpOptionsNew(&query_options);
	LookupTableWrap* lookup_wrap_ptr;
 	BOOST_REQUIRE_EQUAL((int)LookupTableWrapInit(query_blk, 
                                            lookup_options, query_options, lookup_segments, 
                                            0, &lookup_wrap_ptr, NULL, NULL), 0);
    query_options = BlastQuerySetUpOptionsFree(query_options);
	BOOST_REQUIRE_EQUAL(eMBLookupTable, (ELookupTableType)lookup_wrap_ptr->lut_type);

	BlastMBLookupTable* lookup = (BlastMBLookupTable*) lookup_wrap_ptr->lut;
	BOOST_REQUIRE_EQUAL(16777216, lookup->hashsize); // 4**11
	BOOST_REQUIRE_EQUAL(12, (int)lookup->word_length); 
	BOOST_REQUIRE_EQUAL(true, (bool)lookup->discontiguous);
	BOOST_REQUIRE_EQUAL(16, (int)lookup->template_length);
	BOOST_REQUIRE_EQUAL(4, (int)lookup->template_type);
	BOOST_REQUIRE_EQUAL(1, lookup->scan_step);
	BOOST_REQUIRE_EQUAL(2, lookup->longest_chain);
	BOOST_REQUIRE_EQUAL(82, lookup->hashtable[9606485]);
	BOOST_REQUIRE_EQUAL(752, lookup->hashtable[15622537]);
	BOOST_REQUIRE_EQUAL(5408, lookup->hashtable[10084009]);
	BOOST_REQUIRE_EQUAL(10, lookup->pv_array_bts);

    int pv_array_size = (lookup->hashsize >> lookup->pv_array_bts);
	int pv_array_hash =
            EndianIndependentBufferHash((char*) lookup->pv_array,
                                        pv_array_size * sizeof(PV_ARRAY_TYPE),
                                        sizeof(PV_ARRAY_TYPE));
	BOOST_REQUIRE_EQUAL(558099690, pv_array_hash);

	lookup_wrap_ptr = LookupTableWrapFree(lookup_wrap_ptr);
	lookup_options = LookupTableOptionsFree(lookup_options);
}

BOOST_AUTO_TEST_CASE(testDiscontiguousMBLookupTableTwoTemplatesWordSize11) {

    SetUpQuery(SMALL_QUERY_GI);
	LookupTableOptions* lookup_options;
	LookupTableOptionsNew(eBlastTypeBlastn, &lookup_options);
	BLAST_FillLookupTableOptions(lookup_options, eBlastTypeBlastn, 
                                     TRUE, 0, 11);
	lookup_options->mb_template_length = 16; 
	lookup_options->mb_template_type = eMBWordTwoTemplates;

    QuerySetUpOptions* query_options = NULL;
    BlastQuerySetUpOptionsNew(&query_options);
	LookupTableWrap* lookup_wrap_ptr;
 	BOOST_REQUIRE_EQUAL((int)LookupTableWrapInit(query_blk, 
                                         lookup_options, query_options, lookup_segments, 
                                         0, &lookup_wrap_ptr, NULL, NULL), 0);
    query_options = BlastQuerySetUpOptionsFree(query_options);
	BOOST_REQUIRE_EQUAL(eMBLookupTable, (ELookupTableType)lookup_wrap_ptr->lut_type);

	BlastMBLookupTable* lookup = (BlastMBLookupTable*) lookup_wrap_ptr->lut;
	BOOST_REQUIRE_EQUAL(4194304, lookup->hashsize); // 4**11
	BOOST_REQUIRE_EQUAL(11, (int)lookup->word_length); 
	BOOST_REQUIRE_EQUAL(true, (bool)lookup->discontiguous);
	BOOST_REQUIRE_EQUAL(16, (int)lookup->template_length);
	BOOST_REQUIRE_EQUAL(1, (int)lookup->template_type);
	BOOST_REQUIRE_EQUAL(1, (int)lookup->two_templates);
	BOOST_REQUIRE_EQUAL(2, (int)lookup->second_template_type);
	BOOST_REQUIRE_EQUAL(1, lookup->scan_step);
	BOOST_REQUIRE_EQUAL(4, lookup->longest_chain);
	BOOST_REQUIRE_EQUAL(128, lookup->hashtable[1450605]);
	BOOST_REQUIRE_EQUAL(342, lookup->hashtable[4025953]);
	BOOST_REQUIRE_EQUAL(663, lookup->hashtable[3139906]);
	BOOST_REQUIRE_EQUAL(72, lookup->hashtable2[2599530]);
	BOOST_REQUIRE_EQUAL(225, lookup->hashtable2[4110966]);
	BOOST_REQUIRE_EQUAL(8, lookup->pv_array_bts);

    int pv_array_size = (lookup->hashsize >> lookup->pv_array_bts);
	int pv_array_hash =
            EndianIndependentBufferHash((char*) lookup->pv_array,
                                        pv_array_size * sizeof(PV_ARRAY_TYPE),
                                        sizeof(PV_ARRAY_TYPE));
	BOOST_REQUIRE_EQUAL(-36132604, pv_array_hash);

	lookup_wrap_ptr = LookupTableWrapFree(lookup_wrap_ptr);
	lookup_options = LookupTableOptionsFree(lookup_options);
}


BOOST_AUTO_TEST_CASE(testStdLookupTableDebruijn) {

	const int alphabet_size=4;	// in alphabet there are A,C,G,T
	const int word_size=8;		// 5 letters for every hash value.

	debruijnInit(word_size, alphabet_size);

	LookupTableOptions* lookup_options;
	LookupTableOptionsNew(eBlastTypeBlastn, &lookup_options);
	BLAST_FillLookupTableOptions(lookup_options, eBlastTypeBlastn, 
                                     FALSE, 0, word_size);
		
    QuerySetUpOptions* query_options = NULL;
    BlastQuerySetUpOptionsNew(&query_options);
	LookupTableWrap* lookup_wrap_ptr;
 	BOOST_REQUIRE_EQUAL((int)LookupTableWrapInit(query_blk, 
                                            lookup_options, query_options, lookup_segments, 
                                            0, &lookup_wrap_ptr, NULL, NULL), 0);
    query_options = BlastQuerySetUpOptionsFree(query_options);
	BOOST_REQUIRE_EQUAL(eNaLookupTable, (ELookupTableType)lookup_wrap_ptr->lut_type);

	BlastNaLookupTable* lookup = (BlastNaLookupTable*) lookup_wrap_ptr->lut;
	BOOST_REQUIRE_EQUAL(65536, lookup->backbone_size); // 4^8
	BOOST_REQUIRE_EQUAL(1, lookup->longest_chain); 
	BOOST_REQUIRE_EQUAL(0, lookup->overflow_size); 
	
	int index;
    for(index=0;index<lookup->backbone_size;index++)
    {
      BOOST_REQUIRE_EQUAL(1, lookup->thick_backbone[index].num_used);
    }

	PV_ARRAY_TYPE *pv_array = lookup->pv;
    int pv_size = lookup->backbone_size >> PV_ARRAY_BTS;
	for (index=0; index<pv_size; index++)
	{
     BOOST_REQUIRE_EQUAL((Uint4) 0xFFFFFFFF, (Uint4) pv_array[index]);
	}

	lookup_wrap_ptr = LookupTableWrapFree(lookup_wrap_ptr);
	lookup_options = LookupTableOptionsFree(lookup_options);
}

BOOST_AUTO_TEST_CASE(testMegablastLookupTableDebruijn) {

	const int alphabet_size=4;	// in alphabet there are A,C,G,T
	const int word_size=12;		// 12 letters for every hash value.

	debruijnInit(word_size, alphabet_size);

	LookupTableOptions* lookup_options;
	LookupTableOptionsNew(eBlastTypeBlastn, &lookup_options);
	BLAST_FillLookupTableOptions(lookup_options, eBlastTypeBlastn, 
                                     TRUE, 0, 0);
		
    QuerySetUpOptions* query_options = NULL;
    BlastQuerySetUpOptionsNew(&query_options);
	LookupTableWrap* lookup_wrap_ptr;
 	BOOST_REQUIRE_EQUAL((int)LookupTableWrapInit(query_blk, 
                                           lookup_options, query_options, lookup_segments, 
                                           0, &lookup_wrap_ptr, NULL, NULL), 0);
    query_options = BlastQuerySetUpOptionsFree(query_options);
	BOOST_REQUIRE_EQUAL(eMBLookupTable, (ELookupTableType)lookup_wrap_ptr->lut_type);

	BlastMBLookupTable* lookup = (BlastMBLookupTable*) lookup_wrap_ptr->lut;
	BOOST_REQUIRE_EQUAL(16777216, lookup->hashsize); // 4**12
	BOOST_REQUIRE_EQUAL(28, (int)lookup->word_length); 
	BOOST_REQUIRE_EQUAL(2, lookup->longest_chain);   // An overestimate, should be 1.
	BOOST_REQUIRE_EQUAL(10, lookup->pv_array_bts);
	
	int index;

	for (index=0; index<query_blk->length+1; index++)
	{
         BOOST_REQUIRE_EQUAL(0, lookup->next_pos[index]);
	}

	PV_ARRAY_TYPE *pv_array = lookup->pv_array;
    int pv_array_size = (lookup->hashsize >> lookup->pv_array_bts);
	for (index=0; index<pv_array_size; index++)
	{
         BOOST_REQUIRE_EQUAL((Uint4) 0xFFFFFFFF, (Uint4) pv_array[index]);
	}

	lookup_wrap_ptr = LookupTableWrapFree(lookup_wrap_ptr);
	lookup_options = LookupTableOptionsFree(lookup_options);
}

// Test that nothing is put into the lookup table if contiguous unmasked
// regions are smaller than user specified word size.
BOOST_AUTO_TEST_CASE(testStdTableSmallUnmaskedRegion) {

    SetUpQuery(SMALL_QUERY_GI);
	LookupTableOptions* lookup_options;
	LookupTableOptionsNew(eBlastTypeBlastn, &lookup_options);
	BLAST_FillLookupTableOptions(lookup_options, eBlastTypeBlastn, 
                                     FALSE, 0, 28);
		
	BlastSeqLoc* segments = NULL;
	BlastSeqLocNew(&segments, 0, 20);
	BlastSeqLocNew(&segments, 3869, 3889);

    QuerySetUpOptions* query_options = NULL;
    BlastQuerySetUpOptionsNew(&query_options);
	LookupTableWrap* lookup_wrap_ptr;
 	BOOST_REQUIRE_EQUAL((int)LookupTableWrapInit(query_blk, 
                                              lookup_options, query_options, segments, 
                                              0, &lookup_wrap_ptr, NULL, NULL), 0);
    query_options = BlastQuerySetUpOptionsFree(query_options);
	BOOST_REQUIRE_EQUAL(eSmallNaLookupTable, 
                             (ELookupTableType)lookup_wrap_ptr->lut_type);

	BlastSmallNaLookupTable* lookup = 
                        (BlastSmallNaLookupTable*) lookup_wrap_ptr->lut;
	BOOST_REQUIRE_EQUAL(65536, lookup->backbone_size); // 4**8
	BOOST_REQUIRE_EQUAL(0, lookup->longest_chain); 
	BOOST_REQUIRE_EQUAL(28, (int)lookup->word_length); 
	BOOST_REQUIRE_EQUAL(2, lookup->overflow_size); 

	for (int index=0; index<lookup->backbone_size; index++)
	{
        // We expect all backbone cells to be empty
        // since there are no words.
        BOOST_REQUIRE_EQUAL((Int2)(-1), lookup->final_backbone[index]);
	}
        
	lookup_wrap_ptr = LookupTableWrapFree(lookup_wrap_ptr);
	lookup_options = LookupTableOptionsFree(lookup_options);
	segments = BlastSeqLocFree(segments);
}

// Test that nothing is put into the lookup table if contiguous unmasked regions are smaller than user specified word size.
BOOST_AUTO_TEST_CASE(testMegablastTableSmallUnmaskedRegion) {

    const Int4 word_size = 28;
    SetUpQuery(LARGE_QUERY_GI);
	LookupTableOptions* lookup_options;
	LookupTableOptionsNew(eBlastTypeBlastn, &lookup_options);
	BLAST_FillLookupTableOptions(lookup_options, eBlastTypeBlastn, 
                                     TRUE, 0, word_size);

        // make a large number of segments, all smaller than
        // the word size. We cannot have just a few segments because
        // then a large lookup table will not be created

	BlastSeqLoc* segments = NULL;
    Int4 offset = 0;
    while (offset < query_blk->length) {
        BlastSeqLocNew(&segments, offset, offset + word_size - 2);
        offset += word_size;
    }

    QuerySetUpOptions* query_options = NULL;
    BlastQuerySetUpOptionsNew(&query_options);
	LookupTableWrap* lookup_wrap_ptr;
 	BOOST_REQUIRE_EQUAL((int)LookupTableWrapInit(query_blk, 
                                            lookup_options, query_options, segments, 0, 
                                            &lookup_wrap_ptr, NULL, NULL), 0);
    query_options = BlastQuerySetUpOptionsFree(query_options);
	BOOST_REQUIRE_EQUAL((ELookupTableType)lookup_wrap_ptr->lut_type, eMBLookupTable);

	BlastMBLookupTable* lookup = (BlastMBLookupTable*) lookup_wrap_ptr->lut;
	BOOST_REQUIRE_EQUAL(4194304, lookup->hashsize); // 4**11
	BOOST_REQUIRE_EQUAL(28, (int)lookup->word_length); 
	BOOST_REQUIRE_EQUAL(18, lookup->scan_step);
	BOOST_REQUIRE_EQUAL(2, lookup->longest_chain);
	BOOST_REQUIRE_EQUAL(7, lookup->pv_array_bts);
        
	int index;
    int pv_array_size = (lookup->hashsize >> lookup->pv_array_bts);
	PV_ARRAY_TYPE *pv_array = lookup->pv_array;
	for (index=0; index<pv_array_size; index++)
	{
        // We expect pv_array to be all zeros as there are no words.
        BOOST_REQUIRE_EQUAL((PV_ARRAY_TYPE) 0, pv_array[index]);
	}
        
	lookup_wrap_ptr = LookupTableWrapFree(lookup_wrap_ptr);
	lookup_options = LookupTableOptionsFree(lookup_options);
	segments = BlastSeqLocFree(segments);
}


BOOST_AUTO_TEST_SUITE_END()

/*
* ===========================================================================
*
* $Log: ntlookup-cppunit.cpp,v $
* Revision 1.47  2008/10/27 17:00:12  camacho
* Fix include paths to deprecated headers
*
* Revision 1.46  2008/01/31 22:07:00  madden
* Change call to LookupTableWrapInit as part of fix for SB-44
*
* Revision 1.45  2007/02/14 20:18:01  papadopo
* remove SetFullByteScan and discontig. megablast with stride 4
*
* Revision 1.44  2006/12/13 19:19:35  papadopo
* full_byte_scan -> scan_step
*
* Revision 1.43  2006/12/01 16:56:40  papadopo
* modify expectations now that there is an extra blastn lookup table type
*
* Revision 1.42  2006/11/21 17:46:27  papadopo
* rearrange headers, change lookup table type, use enums for lookup table constants
*
* Revision 1.41  2006/09/15 13:12:05  madden
* Change to LookupTableWrapInit prototype
*
* Revision 1.40  2006/05/04 15:53:22  camacho
* Removed unused BLAST_SequenceBlk::context field
*
* Revision 1.39  2005/12/22 14:18:11  papadopo
* change signature of BlastFillLookupTableOptions
*
* Revision 1.38  2005/12/19 16:44:15  papadopo
* 1. Do not assume that lookup table types are those specified
*    when lookup table is constructed
* 2. Add use of small query and small wordsize to force use of standard
*    lookup table instead of megablast lookup table
* 3. Do not assume a single width for standard/megablast lookup tables
*
* Revision 1.37  2005/06/09 20:37:06  camacho
* Use new private header blast_objmgr_priv.hpp
*
* Revision 1.36  2005/05/20 18:30:52  camacho
* Update to use new signature to BLAST_FillLookupTableOptions
*
* Revision 1.35  2005/05/10 16:09:04  camacho
* Changed *_ENCODING #defines to EBlastEncoding enumeration
*
* Revision 1.34  2005/03/16 18:37:18  papadopo
* change expected values to account for modifications to megablast lookup table construction
*
* Revision 1.33  2005/03/04 17:20:44  bealer
* - Command line option support.
*
* Revision 1.32  2005/02/10 21:26:30  bealer
* - Use endianness independant techniques for unit test hashing.
*
* Revision 1.31  2005/01/28 18:30:48  camacho
* Fix memory leak
*
* Revision 1.30  2005/01/13 13:06:51  madden
* New tests for fix to exclude regions of query that are not as long as user specified word size from lookup table
*
* Revision 1.29  2005/01/10 14:01:47  madden
* Prototype change for BLAST_FillLookupTableOptions
*
* Revision 1.28  2004/12/28 16:48:26  camacho
* 1. Use typedefs to AutoPtr consistently
* 2. Use SBlastSequence structure instead of std::pair as return value to
*    blast::GetSequence
*
* Revision 1.27  2004/09/13 12:54:14  madden
* BlastSeqLoc changes
*
* Revision 1.26  2004/08/03 16:13:43  madden
* Correction for use of helper_array
*
* Revision 1.25  2004/07/22 14:29:32  madden
* Add two template discontig mb test
*
* Revision 1.24  2004/07/20 15:50:57  madden
* Added Discontig test cases for optimal pattern, removed dead code
*
* Revision 1.23  2004/07/12 16:28:38  papadopo
* Prepend 'Blast' to {MB|PHI|RPS}LookupTable
*
* Revision 1.22  2004/06/22 16:46:19  camacho
* Changed the blast_type_* definitions for the EBlastProgramType enumeration.
*
* Revision 1.21  2004/04/16 14:35:06  papadopo
* remove unneeded RPS argument in FillLookupTableOptions
*
* Revision 1.20  2004/04/05 16:10:26  camacho
* Rename DoubleInt -> SSeqRange
*
* Revision 1.19  2004/03/23 16:10:34  camacho
* Minor changes to CTestObjMgr
*
* Revision 1.18  2004/03/10 17:39:40  papadopo
* add (unused) RPS blast parameters to LookupTableWrapInit and FillLookupTableOptions
*
* Revision 1.17  2004/03/06 00:40:27  camacho
* Use correct enum argument to ncbi::blast::GetSequence
*
* Revision 1.16  2004/03/05 15:12:07  papadopo
* add (unused) RPS blast parameter to FillLookupTable calls
*
* Revision 1.15  2004/02/24 15:19:39  madden
* Check pv_array_bts for megablast tables, use calculated size of pv_array rather than hard-coded numbers, append WordSize11 to testDiscontiguousMegablastLookupTable
*
* Revision 1.14  2004/02/23 19:52:52  madden
* Add testDiscontiguousMegablastLookupTableWordSize12 test
*
* Revision 1.13  2004/02/20 23:20:37  camacho
* Remove undefs.h
*
* Revision 1.12  2004/02/17 20:33:12  dondosha
* Use BOOST_REQUIRE_EQUAL; const int array sizes
*
* Revision 1.11  2004/02/09 22:37:15  dondosha
* Sentinel values are 15, not 0 for nucleotide sequence; corrected one location endpoint
*
* Revision 1.10  2004/01/26 20:25:20  coulouri
* Use offset rather than pointer for LookupBackboneCell
*
* Revision 1.9  2004/01/06 21:32:06  dondosha
* Corrected values in assertions for megablast lookup table
*
* Revision 1.8  2004/01/02 16:12:34  madden
* Add Debruijn sequences for standard and (contiguous) megablast lookup tables
*
* Revision 1.7  2004/01/02 14:38:22  madden
* Changes for new offset conventions both for lookup table creation and recording of hits
*
* Revision 1.6  2003/12/09 21:38:21  madden
* Compensate for recent discontig. mb changes
*
* Revision 1.5  2003/12/09 18:02:26  camacho
* Use BOOST_REQUIRE_EQUALS to see expected/actual values in error report
*
* Revision 1.4  2003/12/08 21:56:58  madden
* Use setUp and tearDown methods
*
* Revision 1.3  2003/12/08 20:39:00  madden
* Discontiguous megablast test, some cleanup
*
* Revision 1.2  2003/12/08 14:20:28  madden
* Add megablast test
*
* Revision 1.1  2003/12/04 22:03:09  madden
* Nucleotide lookup table tests, first cut
*
*
* ===========================================================================
*/
