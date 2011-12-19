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
* Author: George Coulouris
*
* File Description:
*   Protein lookup table unit tests
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

#include <corelib/ncbitime.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
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

#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_encoding.h>
#include <algo/blast/core/blast_aalookup.h>
#include <algo/blast/core/lookup_util.h>

#include "test_objmgr.hpp"

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;

/// A general lookup table for the test
struct AalookupTestFixture {

  BLAST_SequenceBlk  *query_blk;
  BlastSeqLoc        *lookup_segments;
  LookupTableOptions *lookup_options;
  BlastScoreBlk      *sbp;
  LookupTableWrap    *lookup_wrap_ptr;
  BlastAaLookupTable *lookup;

  // constructor
  // this constructor actually does nothing.
  // use GetSeqBlk() and FillLookupTable() to instantiate a 
  // testing lookuptable instead.
  AalookupTestFixture(){
    query_blk=NULL;
    lookup_segments=NULL;
    lookup_options=NULL;
    sbp=NULL;
    lookup_wrap_ptr=NULL;
    lookup=NULL;
  }

  // destructor
  ~AalookupTestFixture() {
    LookupTableWrapFree(lookup_wrap_ptr);
    LookupTableOptionsFree(lookup_options);
    BlastSequenceBlkFree(query_blk);
    BlastSeqLocFree(lookup_segments);
    BlastScoreBlkFree(sbp);
  }

  // to create a sequence with given gid
  void GetSeqBlk(string gid){
    CSeq_id id(gid);
    auto_ptr<SSeqLoc> ssl(CTestObjMgr::Instance().CreateSSeqLoc(id, eNa_strand_unknown));
    SBlastSequence sequence =
              GetSequence(*ssl->seqloc,
			    eBlastEncodingProtein,
			    ssl->scope,
			    eNa_strand_unknown, // strand not applicable
			    eNoSentinels);      // nucl sentinel not applicable
    // Create the sequence block.
    // Note that GetSequence always includes sentinel bytes for
    //     protein sequences.
    BlastSeqBlkNew(&query_blk);
    BlastSeqBlkSetSequence(query_blk, sequence.data.release(), 
                         sequence.length - 2);

    const Uint1 kNullByte = GetSentinelByte(eBlastEncodingProtein);
    BOOST_REQUIRE(query_blk != NULL);
    BOOST_REQUIRE(query_blk->sequence[0] != kNullByte);
    BOOST_REQUIRE(query_blk->sequence[query_blk->length - 1] != kNullByte);
    BOOST_REQUIRE(query_blk->sequence_start[0] == kNullByte);
    BOOST_REQUIRE(query_blk->sequence_start[query_blk->length + 1] == 
                 kNullByte);
    // Indicate which regions of the query to index
    // The interval is [0...length-1] but must ignore the two
    //       sentinel bytes. This makes the interval [0...length-3]
    BlastSeqLocNew(&lookup_segments, 0, sequence.length - 3);
  }
 
  // to create debruijn sequence
  void GetSeqBlk(){
    // generate sequence 
    Int4 k=0,n=0,i=0, len=0;
    Uint1 *sequence=NULL;
    k=BLASTAA_SIZE; //alphabet size
    n=3;  //word size
    len = iexp(k,n) + (n-1);
    // leave room for and pad with sentinels
    sequence = (Uint1*) malloc(len + 2);
    sequence[0] = 0;
    sequence[len+1] = 0;
    debruijn(n,k,sequence+1,NULL);
    for(i=0;i<n-1;i++) sequence[len-n+2+i] = sequence[i];
    // create sequence block
    BlastSetUp_SeqBlkNew(sequence, len, &query_blk, TRUE);
    // indicate region of query to index 
    lookup_segments=NULL;
    BlastSeqLocNew(&lookup_segments, 0, len-1);
  }

  // to create a trivial sequence (all '0') of length len
  void GetSeqBlk(Int4 len){
    Uint1 *sequence=NULL;
    // leave room for and pad with sentinels
    sequence = (Uint1*) malloc(len + 2);
    for(Int4 i=0; i<len+2; i++) sequence[i] = 0;
    // create sequence block
    BlastSetUp_SeqBlkNew(sequence, len, &query_blk, TRUE);
    // indicate region of query to index 
    lookup_segments=NULL;
    BlastSeqLocNew(&lookup_segments, 0, len-1);
  }

  // to fill up a lookup table
  // hasNeighbor specfies if neighboring words will be considered.
  void FillLookupTable(bool hasNeighbor=false){
    // create lookup options
    LookupTableOptionsNew(eBlastTypeBlastp, &lookup_options);
    BLAST_FillLookupTableOptions(lookup_options,
				 eBlastTypeBlastp, 
				 FALSE,  // megablast
				 (hasNeighbor)? 11: -1,     // threshold
				 3);     // word size
    // create score blocks
    sbp = BlastScoreBlkNew(BLASTAA_SEQ_CODE, 1);
    if(hasNeighbor){
      // generate score options
      BlastScoringOptions *score_options;
      BlastScoringOptionsNew(eBlastTypeBlastp, &score_options);
      BLAST_FillScoringOptions(score_options,
			     eBlastTypeBlastp,
			     FALSE,
			     0,
			     0,
			     NULL,
			     BLAST_GAP_OPEN_PROT,
			     BLAST_GAP_EXTN_PROT);
      Blast_ScoreBlkMatrixInit(eBlastTypeBlastp, score_options, sbp,
         &BlastFindMatrixPath);
      BlastScoringOptionsFree(score_options);
    }
    // create lookup table
    LookupTableWrapInit(query_blk,
			lookup_options,
                      NULL,
			lookup_segments,
			sbp,
			&lookup_wrap_ptr,
                      NULL /* RPS info */,
                      NULL);
    lookup = (BlastAaLookupTable*) lookup_wrap_ptr->lut;
  }
};

BOOST_FIXTURE_TEST_SUITE(aalookup, AalookupTestFixture)

BOOST_AUTO_TEST_CASE(BackboneIntegrityTest) {
  // get a sequence block of gi|129295
  GetSeqBlk("gi|129295");
  FillLookupTable();
  // In this case, we asked for no neighboring, so there should be no neighboring words
  BOOST_REQUIRE_EQUAL(0, lookup->threshold);
  BOOST_REQUIRE_EQUAL(0, lookup->neighbor_matches);
  // The selected sequence should use smallbone
  BOOST_REQUIRE_EQUAL( lookup->bone_type, eSmallbone );
  // count the total number of words found
  Int4 num_hits_found = 0, i;
  for(i=0;i<lookup->backbone_size;i++) {
     num_hits_found += ((AaLookupSmallboneCell *)(lookup->thick_backbone))[i].num_used;
  }
  BOOST_REQUIRE_EQUAL(230, num_hits_found);
}


BOOST_AUTO_TEST_CASE(DebruijnSequenceTest) {
  // create a debruijn sequence
  GetSeqBlk();
  FillLookupTable();
  // The constructed sequence should use smallbone
  BOOST_REQUIRE_EQUAL( lookup->bone_type, eSmallbone );
  Int4 i;
  // by definition, a de Bruijn sequence contains one occurrence of each word. 
  for(i=0;i<lookup->backbone_size;i++) {
    Int4 num_used = ((AaLookupSmallboneCell *)(lookup->thick_backbone))[i].num_used;
    // some cells should be vacant
    if( 
      ( ( i & 0x1F ) >= BLASTAA_SIZE )
      || ( ( (i & 0x3E0) >> 5 ) >= BLASTAA_SIZE )
      || ( ( ( i & 0x7C00 ) >> 10 ) >= BLASTAA_SIZE )
      ){
         BOOST_REQUIRE_EQUAL( num_used, 0 );
    }else{
    // otherwise, the cell should contain exactly one hit
         BOOST_REQUIRE_EQUAL( num_used, 1 );
    }
  }
}


BOOST_AUTO_TEST_CASE(NeighboringWordsTest) {
  // create a deruijn sequence for neibhoring words test
  GetSeqBlk();
  FillLookupTable(true);
  // The constructed sequence should use smallbone
  BOOST_REQUIRE_EQUAL( lookup->bone_type, eSmallbone );
  // Now we have neighboring words, for each possible 3-mer, 
  Int4 index;
  for(Int4 u=0;u<BLASTAA_SIZE;u++)
    for(Int4 v=0;v<BLASTAA_SIZE;v++)
	for(Int4 w=0;w<BLASTAA_SIZE;w++)
	  {
	    Int4 score;
	    Int4 count = 0;
	    // find its neighbors by brute force 
	    for(Int4 x=0;x<BLASTAA_SIZE;x++)
	      for(Int4 y=0;y<BLASTAA_SIZE;y++)
		for(Int4 z=0;z<BLASTAA_SIZE;z++)
		  {
		    // compute the score of these two words 
		    score = sbp->matrix->data[u][x] + sbp->matrix->data[v][y] +
              sbp->matrix->data[w][z];
		    // if the score is above the threshold or the words match, record it
		    if ( (score >= 11) || ( (u==x) && (v==y) && (w==z) ) )
		      count++;
		  }
	    // compute the index of the word
	    index = (u << 10) | (v << 5) | (w);
	    // ensure that the number of neighbors matches the lookup table
	    //printf("count=%d, lut=%d\n",count, lookup->thick_backbone[index].num_used); 
            Int4 num_used =  ((AaLookupSmallboneCell *)(lookup->thick_backbone))[index].num_used;
	    BOOST_REQUIRE_EQUAL(count, num_used);
	  }
}

BOOST_AUTO_TEST_CASE(BackboneSequenceTest) {
  // create a trivial sequence
  Int4 len = 65534; // 65535 is the maximum possible unsigned short
  GetSeqBlk(len);
  FillLookupTable();
  BOOST_REQUIRE_EQUAL( lookup->bone_type, eBackbone );
  Int4 num_used = ((AaLookupBackboneCell *)(lookup->thick_backbone))[0].num_used;
  BOOST_REQUIRE_EQUAL(num_used, len-2);
  Int4 offset = ((Int4 *)(lookup->overflow))[num_used-1];
  BOOST_REQUIRE_EQUAL(offset, len-3);
}

BOOST_AUTO_TEST_CASE(SmallboneSequenceTest) {
  // create a trivial sequence
  Int4 len = 65533;
  GetSeqBlk(len);
  FillLookupTable();
  BOOST_REQUIRE_EQUAL( lookup->bone_type, eSmallbone );
  Int4 num_used = ((AaLookupSmallboneCell *)(lookup->thick_backbone))[0].num_used;
  BOOST_REQUIRE_EQUAL(num_used, len-2);
  Int4 offset = ((Uint2 *)(lookup->overflow))[num_used-1];
  BOOST_REQUIRE_EQUAL(offset, len-3);
}


#if 0

// Needs to be fixed to actually use a PSSM
BOOST_AUTO_TEST_CASE(testDebruijnPSSM)
{
  Int4 i=0,j=0;        // loop indices
  
  Int4 k=BLASTAA_SIZE; // alphabet size
  Int4 n=3;  // word size
  
  Int4 len = iexp(k,n) + (n-1);
  
  // leave room for and pad with sentinels
  Uint1 *sequence = (Uint1*) malloc(len + 2);
  sequence[0] = 0;
  sequence[len+1] = 0;
  // generate debruijn sequence
  debruijn(n,k,sequence+1,NULL);
  // unwrap it
  for(i=0;i<n-1;i++)
    sequence[len-n+2+i] = sequence[i];
  
  // create sequence block
  BLAST_SequenceBlk *query_blk_debruijn=NULL;
  BlastSetUp_SeqBlkNew(sequence, len, &query_blk_debruijn, TRUE);
  
  // indicate region of query to index
  BlastSeqLoc       *lookup_segments_debruijn=NULL;
  BlastSeqLocNew(&lookup_segments_debruijn, 0, len-1);

  // create the score block
  BlastScoringOptions *score_options = NULL;
  BlastScoringOptionsNew(eBlastTypeBlastp, &score_options);
  BLAST_FillScoringOptions(score_options,
			     eBlastTypeBlastp,
			     FALSE,
			     0,
			     0,
			     NULL,
			     BLAST_GAP_OPEN_PROT,
			     BLAST_GAP_EXTN_PROT);

  BlastScoreBlk *sbp = BlastScoreBlkNew(BLASTAA_SEQ_CODE, 1);
  Blast_ScoreBlkMatrixInit(eBlastTypeBlastp, score_options, sbp, &BlastFindMatrixPath);

  // create the PSSM
  BlastScoreBlk *pssm_sbp = BlastScoreBlkNew(BLASTAA_SEQ_CODE, 1);

  pssm_sbp->posMatrix = (Int4 **) calloc(len, sizeof(Int4 *));

  for(i=0;i<len;i++)
    {
	pssm_sbp->posMatrix[i] = (Int4 *) calloc(BLASTAA_SIZE, sizeof(Int4));

	for(j=0;j<BLASTAA_SIZE;j++)
	  pssm_sbp->posMatrix[i][j] = sbp->matrix[j][sequence[i+1]];
    } 

  // set PSSM lookup table options
  LookupTableOptions* pssm_lut_options=NULL;
  LookupTableOptionsNew(eBlastTypeBlastp, &pssm_lut_options);
  BLAST_FillLookupTableOptions(pssm_lut_options,
				 eBlastTypeBlastp, 
				 FALSE,  // megablast
				 11,     // threshold
				 3);     // word size

  // create the PSSM lookup table
  LookupTableWrap* pssm_lut_wrap_ptr=NULL;

  LookupTableWrapInit(NULL, //query_blk_debruijn,
			pssm_lut_options,
                      NULL,
			lookup_segments_debruijn,
			pssm_sbp,
			&pssm_lut_wrap_ptr,
                      NULL /* RPS Info */,
                      NULL);

  BlastAaLookupTable* pssm_lut = (BlastAaLookupTable*) pssm_lut_wrap_ptr->lut;

{
  /* for each possible 3-mer, */

  Int4 index;
  
  for(i=0;i<BLASTAA_SIZE;i++)
    for(j=0;j<BLASTAA_SIZE;j++)
	for(k=0;k<BLASTAA_SIZE;k++)
	  {
	    Int4 score;
	    Int4 count = 0;
	    
	    /* find its neighbors by brute force */

	    for(Int4 x=0;x<BLASTAA_SIZE;x++)
	      for(Int4 y=0;y<BLASTAA_SIZE;y++)
		for(Int4 z=0;z<BLASTAA_SIZE;z++)
		  {
		    /* compute the score of these two words */
		    score = sbp->matrix[i][x] + sbp->matrix[j][y] + sbp->matrix[k][z];
		    
		    /* if the score is above the threshold, record it */
		    if (score >= 11)
		      count++;
		  }

	    /* compute the index of the word */

	    index = (i << 10) | (j << 5) | (k);

	    /* ensure that the number of neighbors matches the lookup table */
	    //printf("count=%d, lut=%d\n",count, pssm_lut->thick_backbone[index].num_used); 
	    BOOST_REQUIRE_EQUAL(count, pssm_lut->thick_backbone[index].num_used);
	  }
}

  LookupTableWrapFree(pssm_lut_wrap_ptr);
  LookupTableOptionsFree(pssm_lut_options);
  BlastScoreBlkFree(pssm_sbp);

  BlastSequenceBlkFree(query_blk_debruijn);
  BlastSeqLocFree(lookup_segments_debruijn);

  BlastScoreBlkFree(sbp);
  BlastScoringOptionsFree(score_options);
}
// testDebruijnPSSM
#endif

// TestBlastAaLookupTable

// Register this test suite with the default test factory registry

BOOST_AUTO_TEST_SUITE_END()
