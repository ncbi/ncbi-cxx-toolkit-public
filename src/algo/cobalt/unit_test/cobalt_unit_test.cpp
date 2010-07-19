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
* Author:  Greg Boratyn
*
* File Description:
*   Unit tests for CMultiAligner
*
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objtools/readers/fasta.hpp>
#include <objtools/readers/reader_exception.hpp>
#include <objtools/data_loaders/blastdb/bdbloader.hpp>
#include <serial/iterator.hpp>

#include <objects/biotree/NodeSet.hpp>

#include <algo/cobalt/cobalt.hpp>
#include "cobalt_unit_test.hpp"

#include <corelib/hash_set.hpp>

// This macro should be defined before inclusion of test_boost.hpp in all
// "*.cpp" files inside executable except one. It is like function main() for
// non-Boost.Test executables is defined only in one *.cpp file - other files
// should not include it. If NCBI_BOOST_NO_AUTO_TEST_MAIN will not be defined
// then test_boost.hpp will define such "main()" function for tests.
//
// Usually if your unit tests contain only one *.cpp file you should not
// care about this macro at all.
//
//#undef NCBI_BOOST_NO_AUTO_TEST_MAIN
#define NCBI_BOOST_NO_AUTO_TEST_MAIN

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  /* This header must go last */

#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;
USING_SCOPE(cobalt);


void ReadFastaQueries(const string& filename, CObjectManager& objmgr,
                      vector< CRef<objects::CSeq_loc> >& seqs,
                      CRef<objects::CScope>& scope)
{
    seqs.clear();
    scope.Reset(new CScope(objmgr));

    CNcbiIfstream instream(filename.c_str());
    BOOST_REQUIRE(instream);

    // read one query at a time, and use a separate seq_entry,
    // scope, and lowercase mask for each query. This lets different
    // query sequences have the same ID. Later code will distinguish
    // between queries by using different elements of retval[]

    CStreamLineReader line_reader(instream);
    CFastaReader fasta_reader(line_reader, 
                              CFastaReader::fAssumeProt |
                              CFastaReader::fForceType |
                              CFastaReader::fNoParseID);

    while (!line_reader.AtEOF()) {

        scope->AddDefaults();
        CRef<CSeq_entry> entry = fasta_reader.ReadOneSeq();

        if (entry == 0) {
            NCBI_THROW(CObjReaderException, eInvalid, 
                        "Could not retrieve seq entry");
        }
        scope->AddTopLevelSeqEntry(*entry);
        CTypeConstIterator<CBioseq> itr(ConstBegin(*entry));
        CRef<CSeq_loc> seqloc(new CSeq_loc());
        seqloc->SetWhole().Assign(*itr->GetId().front());
        seqs.push_back(seqloc);
    }
}


// Queries returned by aligner must be in the same order as
// in which they were assigned
static void s_TestQueriesAsSeq_locs(const vector< CRef<CSeq_loc> >& seqlocs,
                                    CRef<CScope> scope)
{
    CMultiAligner aligner;
    aligner.SetQueries(seqlocs, scope);

    const vector< CRef<CSeq_loc> >& q = aligner.GetQueries();
    BOOST_REQUIRE_EQUAL(seqlocs.size(), q.size());
    for (size_t i=0;i < q.size();i++) {
        BOOST_CHECK(seqlocs[i]->GetId()->Match(*q[i]->GetId()));
    }
}

// Queries returned by aligner must be in the same order as
// in which they were assigned
static void s_TestQueriesAsBioseqs(const vector< CRef<CBioseq> >& bioseqs)
{
    CMultiAligner aligner;
    aligner.SetQueries(bioseqs);

    const vector< CRef<CSeq_loc> >& q = aligner.GetQueries();
    BOOST_REQUIRE_EQUAL(bioseqs.size(), q.size());
    for (size_t i=0;i < q.size();i++) {
        BOOST_CHECK(bioseqs[i]->GetFirstId()->Match(*q[i]->GetId()));
    }   
}


static void s_MakeBioseqs(const vector< CRef<CSeq_loc> >& seqlocs,
                          CRef<CScope> scope,
                          vector< CRef<CBioseq> >& bioseqs)
{
    bioseqs.clear();
    ITERATE(vector< CRef<CSeq_loc> >, it, seqlocs) {

        CBioseq_Handle handle = scope->GetBioseqHandle(**it);
        CBioseq* bseq = (CBioseq*)handle.GetCompleteBioseq().GetNonNullPointer();
        bioseqs.push_back(CRef<CBioseq>(bseq));
    }

}

BOOST_AUTO_TEST_SUITE(multialigner)


// Make sure assiging query sequences are assigned properly
BOOST_AUTO_TEST_CASE(TestSetQueries)
{
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*objmgr));
    scope->AddDefaults();

    vector< CRef<CSeq_loc> > seqlocs;
    vector< CRef<CBioseq> > bioseqs;

    // Test for fasta input
    ReadFastaQueries("data/small.fa", *objmgr, seqlocs, scope);
    s_MakeBioseqs(seqlocs, scope, bioseqs);
    s_TestQueriesAsSeq_locs(seqlocs, scope);
    s_TestQueriesAsBioseqs(bioseqs);

    // Test for sequences as gis
    // There is a problem with CSeqVector

    // Using gis causes problems with CSeqVector
    CSeq_id id("gi|129295");
    CBioseq_Handle h = scope->GetBioseqHandle(id);
    BOOST_CHECK(h.GetSeqId()->Match(id));

    CSeq_loc s;
    s.SetInt().SetFrom(0);
    s.SetInt().SetTo(sequence::GetLength(id, scope)-1);
    s.SetInt().SetId().Assign(id);
    h = scope->GetBioseqHandle(s);

    //TODO: test for sequences as gis and mix.
    // Currently there is a problem in CSeqVector used in CSequence
    // CSeqMap::GetLength(CScope*) throws exception for some reason
}


BOOST_AUTO_TEST_CASE(TestBadQueries)
{
    CMultiAligner aligner;

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*objmgr));
    scope->AddDefaults();

    vector< CRef<CSeq_loc> > seqlocs;
    vector< CRef<CBioseq> > bioseqs;

    // Empty list of qurey sequences causes exception
    BOOST_CHECK_THROW(aligner.SetQueries(seqlocs, scope),
                      CMultiAlignerException);

    BOOST_CHECK_THROW(aligner.SetQueries(bioseqs), CMultiAlignerException);
    
    // Empty seqloc or bioseq causes exception
    seqlocs.push_back(CRef<CSeq_loc>(new CSeq_loc()));
    seqlocs.push_back(CRef<CSeq_loc>(new CSeq_loc()));
    BOOST_CHECK_THROW(aligner.SetQueries(seqlocs, scope),
                      CMultiAlignerException);

    bioseqs.push_back(CRef<CBioseq>(new CBioseq()));
    bioseqs.push_back(CRef<CBioseq>(new CBioseq()));
    BOOST_CHECK_THROW(aligner.SetQueries(bioseqs), CMultiAlignerException);

    ReadFastaQueries("data/small.fa", *objmgr, seqlocs, scope);

    // Single input sequence causes exception
    seqlocs.resize(1);
    BOOST_REQUIRE_EQUAL((int)seqlocs.size(), 1);
    BOOST_CHECK_THROW(aligner.SetQueries(seqlocs, scope),
                      CMultiAlignerException);

    s_MakeBioseqs(seqlocs, scope, bioseqs);
    BOOST_CHECK_THROW(aligner.SetQueries(bioseqs), CMultiAlignerException);

    // A gap in input sequence causes exception
    ReadFastaQueries("data/queries_with_gaps.fa", *objmgr, seqlocs, scope);
    BOOST_CHECK_THROW(aligner.SetQueries(seqlocs, scope),
                      CMultiAlignerException);

    s_MakeBioseqs(seqlocs, scope, bioseqs);
    BOOST_CHECK_THROW(aligner.SetQueries(bioseqs), CMultiAlignerException);
}


// Bad user constraints must be reported
BOOST_AUTO_TEST_CASE(TestBadUserConstraints)
{
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*objmgr));

    vector< CRef<CSeq_loc> > seqlocs;
    ReadFastaQueries("data/small.fa", *objmgr, seqlocs, scope);

    vector< CRef<CBioseq> > bioseqs;
    s_MakeBioseqs(seqlocs, scope, bioseqs);

    // user constraints for non-exiting queries must be reported
    CRef<CMultiAlignerOptions> opts(new CMultiAlignerOptions());
    
    CMultiAlignerOptions::TConstraints& constr = opts->SetUserConstraints();
    constr.resize(1);
    
    int query_index = (int)seqlocs.size();
    constr[0].seq1_index = query_index;
    constr[0].seq1_start = 0;
    constr[0].seq1_stop = 50;

    constr[0].seq2_index = 0;
    constr[0].seq2_start = 0;
    constr[0].seq2_stop = 50;

    // this problem cannot be detected in options validation before queries
    // are set
    BOOST_REQUIRE(opts->Validate());

    CRef<CMultiAligner> aligner(new CMultiAligner(opts));
    BOOST_CHECK_THROW(aligner->SetQueries(seqlocs, scope),
                      CMultiAlignerException);

    aligner.Reset(new CMultiAligner(opts));
    BOOST_CHECK_THROW(aligner->SetQueries(bioseqs),
                      CMultiAlignerException);
    
    // user constraint with position of ouf of sequence range must be reported
    constr.clear();
    constr.resize(1);
    constr[0].seq1_index = 0;
    constr[0].seq1_start = 0;
    constr[0].seq1_stop = 50;

    int length = (int)sequence::GetLength(*seqlocs[1],
                                          scope.GetNonNullPointer());

    constr[0].seq2_index = 1;
    constr[0].seq2_start = 0;
    constr[0].seq2_stop = length + 1;

    BOOST_REQUIRE(opts->Validate());

    aligner.Reset(new CMultiAligner(opts));
    BOOST_CHECK_THROW(aligner->SetQueries(seqlocs, scope),
                      CMultiAlignerException);    

    aligner.Reset(new CMultiAligner(opts));
    BOOST_CHECK_THROW(aligner->SetQueries(bioseqs),
                      CMultiAlignerException);    
}

BOOST_AUTO_TEST_CASE(TestNoResults)
{
    // Getting results without computing them causes exception
    CMultiAligner aligner;
    BOOST_CHECK_THROW(aligner.GetResults(), CMultiAlignerException);
    BOOST_CHECK_THROW(aligner.GetTreeContainer(), CMultiAlignerException);
}


static bool s_Interrupt(CMultiAligner::SProgress* progress)
{
    return true;
}

BOOST_AUTO_TEST_CASE(TestInterrupt)
{
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();

    CMultiAligner aligner;
    vector< CRef<CSeq_loc> > queries;
    CRef<CScope> scope;
    ReadFastaQueries("data/small.fa", *objmgr, queries, scope);
    aligner.SetQueries(queries, scope);
    aligner.SetInterruptCallback(s_Interrupt);
    CMultiAligner::TStatus status = aligner.Run();
    BOOST_CHECK_EQUAL((CMultiAligner::EStatus)status, CMultiAligner::eInterrupt);
}


// Check if all queries appear in the tree and all distances and labels are set
static void s_TestTree(vector<bool>& queries, const TPhyTreeNode* node)
{
    // each node except for root must have set distance
    if (node->GetParent()) {
        BOOST_CHECK(node->GetValue().IsSetDist());
    }

    if (node->IsLeaf()) {

        // each leaf node must have set label ...
        BOOST_CHECK(!node->GetValue().GetLabel().empty());
        const char* label = node->GetValue().GetLabel().c_str();
        // some labels are of the form N<number>
        if (!isdigit((unsigned char)label[0])) {
            label++;
        }
        // ... which is the same as node id
        BOOST_CHECK_EQUAL(node->GetValue().GetId(),
                          NStr::StringToInt((string)label));

        // each query id appears in the tree exactly once
        int id = node->GetValue().GetId();
        BOOST_REQUIRE(!queries[id]);
        queries[id] = true;
    }
    else {

        // non-leaf nodes must have empty labels
        BOOST_CHECK(node->GetValue().GetLabel().empty());

        TPhyTreeNode::TNodeList_CI it = node->SubNodeBegin();
        for (; it != node->SubNodeEnd();it++) {
            s_TestTree(queries, *it);
        }
    }
}

// Check if all queries appear in the tree and all distances and labels are set
static void s_TestResultTree(int num_queries, const TPhyTreeNode* tree)
{
    // make sure that all query sequences are represented by id and label
    // check both GetTree() and GetTreeContainer()

    vector<bool> used_queries(num_queries, false);
    s_TestTree(used_queries, tree);

    // make sure all queries are in the tree
    ITERATE(vector<bool>, it, used_queries) {
        BOOST_REQUIRE(*it);
    }
}

// Make sure that all queries appear in the tree
static void s_TestResultTreeContainer(int num_queries,
                                      const CBioTreeContainer& btc)
{
    vector<bool> used_queries(num_queries, false);

    // Find feature id for node label
    int label_fid = -1;
    int dist_fid = -1;
    ITERATE(CFeatureDictSet::Tdata, it, btc.GetFdict().Get()) {
        BOOST_CHECK((*it)->CanGetName());
        if ((*it)->GetName() == "label") {
            label_fid = (*it)->GetId();
        }
        if ((*it)->GetName() == "dist") {
            dist_fid = (*it)->GetId();
        }
    }
    // node label feature must be present
    BOOST_REQUIRE(label_fid >= 0);
    // dist feature must be present
    BOOST_REQUIRE(dist_fid >= 0);

    ITERATE(CNodeSet::Tdata, node, btc.GetNodes().Get()) {
        if ((*node)->GetId() == 0) {
            continue;
        }

        // each node except for rooot must have features
        BOOST_REQUIRE((*node)->CanGetFeatures());

        bool is_dist = false;
        ITERATE(CNodeFeatureSet::Tdata, feat, (*node)->GetFeatures().Get()) {
            if ((*feat)->GetFeatureid() == label_fid) {
                string label = (*feat)->GetValue();
                const char* ptr = label.c_str();
                if (!isdigit(ptr[0])) {
                    ptr++;
                }

                // each query id must appear exactly once in the tree
                int id = NStr::StringToInt((string)ptr);                
                BOOST_REQUIRE(!used_queries[id]);
                used_queries[id] = true;
            }

            if ((*feat)->GetFeatureid() == dist_fid) {
                is_dist = true;
            }
        }
        // each node except for root must have dist feature
        BOOST_CHECK(is_dist);
    }

    // make sure all query ids were found
    ITERATE(vector<bool>, it, used_queries) {
        BOOST_REQUIRE(*it);
    }
}


// Check whether all queries appear in clusters and each prototype belongs
// to the cluster
static void s_TestResultClusters(int num_queries,
                        const CClusterer::TClusters& clusters,
                        const CMultiAlignerOptions::TConstraints& constraints)
{
    // Exit if there are no clusters
    if (clusters.empty()) {
        return;
    }

    // Check if all queries appear in clusters
    vector<bool> used_queries(num_queries, false);
    int num_elems = 0;
    ITERATE(CClusterer::TClusters, cluster, clusters) {

        // cluster must not be empty
        BOOST_REQUIRE(cluster->size() > 0);
        num_elems += (int)cluster->size();

        int prototype = cluster->GetPrototype();
        bool is_prototype = false;
        ITERATE(CClusterer::TSingleCluster, elem, *cluster) {

            // each query appears exactly onces in all clusters
            BOOST_REQUIRE(!used_queries[*elem]);
            used_queries[*elem] = true;

            if (prototype == *elem) {
                is_prototype = true;
            }
        }

        // prototype must belong to the cluster
        BOOST_CHECK(is_prototype);
    }

    // make sure all queries appear once in all clusters
    BOOST_REQUIRE_EQUAL(num_elems, num_queries);

    // make sure all queries were found
    ITERATE(vector<bool>, it, used_queries) {
        BOOST_CHECK(*it);
    }


    // Check if queries involved in user constraints for one-element clusters
    if (constraints.empty()) {
        return;
    }

    hash_set<int> constr_queries;
    ITERATE(CMultiAlignerOptions::TConstraints, it, constraints) {
        constr_queries.insert(it->seq1_index);
        constr_queries.insert(it->seq2_index);
    }
    size_t remain = constr_queries.size();

    ITERATE(CClusterer::TClusters, cluster, clusters) {
        ITERATE(CClusterer::TSingleCluster, elem, *cluster) {
            hash_set<int>::const_iterator it = constr_queries.find(*elem);
            if (it != constr_queries.end()) {

                // query that appears in a user constraint must be in
                // a one-element cluster
                BOOST_CHECK_EQUAL(cluster->size(), 1u);
                remain--;
            }
        }

        // exit loop if all constraint queries are checked
        if (remain == 0) {
            break;
        }
    }
}


static void s_TestResultAlignment(const vector< CRef<CSeq_loc> >& queries,
                                  const CRef<CSeq_align>& seqalign,
                                  const vector<CSequence>& seqs,
                                  CRef<CScope> scope,
                                  const string& aln_ref = "")
{
    // alignment must be of type global
    BOOST_CHECK_EQUAL(seqalign->GetType(), CSeq_align::eType_global);

    int num_queries = (int)queries.size();
    // dim must be equal to number of queries
    BOOST_REQUIRE_EQUAL(seqalign->GetDim(), num_queries);

    // sequences in the seqalign must be in the same order as in queries
    for (int i=0;i < num_queries;i++) {
        BOOST_REQUIRE(queries[i]->GetId()->Match(seqalign->GetSeq_id(i)));

        //seqalign->GetSeq_id(i).Match(ref_align.GetSeq_id(i)));
    }

    // all sequneces in CSequence format must have equal length
    BOOST_REQUIRE_EQUAL(seqs.size(), queries.size());
    int len = seqs[0].GetLength();
    ITERATE(vector<CSequence>, it, seqs) {
        BOOST_CHECK_EQUAL(it->GetLength(), len);
    }

    // make sure that all residues appear in the resulting alignment
    for (size_t i=0;i < queries.size();i++) {
        int query_len = (int)sequence::GetLength(*queries[i],
                                                 scope.GetPointer());

        int num_residues = 0;
        const unsigned char* sequence = seqs[i].GetSequence();
        for (int k=0;k < seqs[i].GetLength();k++) {
            if (sequence[k] != CSequence::kGapChar) {
                num_residues++;
            }
        }
        BOOST_CHECK_EQUAL(query_len, num_residues);
    }
    

    // if reference file name provided, compare seq-align with the reference
    if (!aln_ref.empty()) {
        CSeq_align ref_align;
        CNcbiIfstream istr(aln_ref.c_str());
        istr >> MSerial_AsnText >> ref_align;

        // check order of seq-ids
        BOOST_REQUIRE_EQUAL(seqalign->GetDim(), ref_align.GetDim());

        // compare resulting alignment to the reference
        const CDense_seg& denseg = seqalign->GetSegs().GetDenseg();
        const CDense_seg::TStarts& starts = denseg.GetStarts();
        const CDense_seg::TLens& lens = denseg.GetLens();

        const CDense_seg& ref_denseg = ref_align.GetSegs().GetDenseg();
        const CDense_seg::TStarts& ref_starts = ref_denseg.GetStarts();
        const CDense_seg::TLens& ref_lens = ref_denseg.GetLens();


        BOOST_REQUIRE_EQUAL(starts.size(), ref_starts.size());
        BOOST_REQUIRE_EQUAL(lens.size(), ref_lens.size());
        for (size_t i=0;i < starts.size();i++) {
            BOOST_CHECK_EQUAL(starts[i], ref_starts[i]);
        }
        for (size_t i=0;i < lens.size();i++) {
            BOOST_CHECK_EQUAL(lens[i], ref_lens[i]);
        }
    }
}


static void s_TestResults(CMultiAligner& aligner,
                          const string& ref_aln = "")
{
    const int kNumQueries = aligner.GetQueries().size();

    s_TestResultClusters(kNumQueries, aligner.GetQueryClusters(),
                         aligner.GetOptions()->GetUserConstraints());
    s_TestResultTree(kNumQueries, aligner.GetTree());
    s_TestResultTreeContainer(kNumQueries, *aligner.GetTreeContainer());
    s_TestResultAlignment(aligner.GetQueries(), aligner.GetResults(),
                          aligner.GetSeqResults(), aligner.GetScope(),
                          ref_aln);
}

// The blow tests ckeck for proper results for differnt options

// The default options should result in success status
BOOST_AUTO_TEST_CASE(TestResultsForDefaultOpts)
{
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CRef<CMultiAlignerOptions> opts(new CMultiAlignerOptions());
    BOOST_REQUIRE(opts->Validate());

    CMultiAligner aligner(opts);
    vector< CRef<CSeq_loc> > queries;
    CRef<CScope> scope;
    ReadFastaQueries("data/small.fa", *objmgr, queries, scope);
    aligner.SetQueries(queries, scope);

    CMultiAligner::TStatus status = aligner.Run();
    BOOST_REQUIRE_EQUAL(status, (CMultiAligner::TStatus)CMultiAligner::eSuccess);
    BOOST_CHECK_EQUAL((int)aligner.GetMessages().size(), 0);

    s_TestResults(aligner);
//    s_TestResults(aligner, "data/ref_seqalign.asn");
}


// Max cluster diameter set to zero must cause a warning
BOOST_AUTO_TEST_CASE(TestResultsForZeroClusterDiam)
{
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CRef<CMultiAlignerOptions> opts(new CMultiAlignerOptions());
    opts->SetMaxInClusterDist(0.0);

    BOOST_REQUIRE(opts->Validate());

    CMultiAligner aligner(opts);
    vector< CRef<CSeq_loc> > queries;
    CRef<CScope> scope;
    ReadFastaQueries("data/small.fa", *objmgr, queries, scope);
    aligner.SetQueries(queries, scope);

    CMultiAligner::TStatus status = aligner.Run();

    //Bad input file - there are two exactly same sequences

    BOOST_REQUIRE_EQUAL(status, (CMultiAligner::TStatus)CMultiAligner::eSuccess);
    BOOST_CHECK((int)aligner.GetMessages().size() == 0);

    s_TestResults(aligner);
}


// Max cluster diameter set to 1 (maxiumu value) must cause a warning
BOOST_AUTO_TEST_CASE(TestResultsForMaxClusterDiam)
{
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CRef<CMultiAlignerOptions> opts(new CMultiAlignerOptions());
    opts->SetMaxInClusterDist(1.0);

    BOOST_REQUIRE(opts->Validate());

    CMultiAligner aligner(opts);
    vector< CRef<CSeq_loc> > queries;
    CRef<CScope> scope;
    ReadFastaQueries("data/small.fa", *objmgr, queries, scope);
    aligner.SetQueries(queries, scope);

    CMultiAligner::TStatus status = aligner.Run();

    //Bad input file - there are two exactly same sequences

    BOOST_REQUIRE_EQUAL(status, (CMultiAligner::TStatus)CMultiAligner::eWarnings);
    BOOST_CHECK((int)aligner.GetMessages().size() > 0);

    s_TestResults(aligner);
}


BOOST_AUTO_TEST_CASE(TestResultsForNoClusters)
{
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CRef<CMultiAlignerOptions> opts(new CMultiAlignerOptions(
                 CMultiAlignerOptions::fNoQueryClusters
                 | CMultiAlignerOptions::fNoRpsBlast));

    BOOST_REQUIRE(opts->Validate());

    CMultiAligner aligner(opts);
    vector< CRef<CSeq_loc> > queries;
    CRef<CScope> scope;
    ReadFastaQueries("data/small.fa", *objmgr, queries, scope);
    aligner.SetQueries(queries, scope);

    CMultiAligner::TStatus status = aligner.Run();
    BOOST_REQUIRE_EQUAL(status, (CMultiAligner::TStatus)CMultiAligner::eSuccess);
    BOOST_CHECK_EQUAL((int)aligner.GetMessages().size(), 0);

    s_TestResults(aligner);
}


// Make sure that queries that appear in user constraints form one-element
// clusters
BOOST_AUTO_TEST_CASE(TestResultsForClustersAndUserConstraints)
{
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CRef<CMultiAlignerOptions> opts(new CMultiAlignerOptions());

    vector<CMultiAlignerOptions::SConstraint>& constr
        = opts->SetUserConstraints();

    constr.resize(2);
    constr[0].seq1_index = 0;
    constr[0].seq1_start = 0;
    constr[0].seq1_stop = 50;

    constr[0].seq2_index = 1;
    constr[0].seq2_start = 0;
    constr[0].seq2_stop = 50;

    constr[1].seq1_index = 1;
    constr[1].seq1_start = 0;
    constr[1].seq1_stop = 50;

    constr[1].seq2_index = 5;
    constr[1].seq2_start = 0;
    constr[1].seq2_stop = 50;

    BOOST_REQUIRE(opts->Validate());

    CMultiAligner aligner(opts);
    vector< CRef<CSeq_loc> > queries;
    CRef<CScope> scope;
    ReadFastaQueries("data/small.fa", *objmgr, queries, scope);
    aligner.SetQueries(queries, scope);

    CMultiAligner::TStatus status = aligner.Run();
    BOOST_REQUIRE_EQUAL(status, (CMultiAligner::TStatus)CMultiAligner::eSuccess);
    BOOST_CHECK_EQUAL((int)aligner.GetMessages().size(), 0);

    s_TestResults(aligner);
}


// Two sequences is a special case for query clustering and computing
// guide tree as cluster dendrogram
BOOST_AUTO_TEST_CASE(TestTwoSequences)
{
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CRef<CMultiAlignerOptions> opts(new CMultiAlignerOptions());

    // case with one cluster
    opts->SetMaxInClusterDist(1.0);
    CMultiAligner aligner(opts);

    vector< CRef<CSeq_loc> > queries;
    CRef<CScope> scope;
    ReadFastaQueries("data/small.fa", *objmgr, queries, scope);
    queries.resize(2);
    aligner.SetQueries(queries, scope);
    
    CMultiAligner::TStatus status = aligner.Run();
    BOOST_CHECK(status == CMultiAligner::eSuccess
                || status == CMultiAligner::eWarnings);
    s_TestResults(aligner);

    // case with two clusters
    opts->SetMaxInClusterDist(0.01);
    aligner.SetQueries(queries, scope);
    status = aligner.Run();
    BOOST_CHECK(status == CMultiAligner::eSuccess
                || status == CMultiAligner::eWarnings);
    s_TestResults(aligner);
    
}

BOOST_AUTO_TEST_SUITE_END()

#endif /* SKIP_DOXYGEN_PROCESSING */
