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
#include <serial/iterator.hpp>

#include <objects/biotree/NodeSet.hpp>
#include <objects/blast/Blast4_request.hpp>
#include <objects/blast/Blast4_request_body.hpp>
#include <objects/blast/Blast4_queue_search_reques.hpp>
#include <objects/blast/Blast4_queries.hpp>
#include <objects/blast/Blas_get_searc_resul_reply.hpp>

#include <objects/scoremat/PssmWithParameters.hpp>
#include <objects/scoremat/Pssm.hpp>

#include <algo/cobalt/cobalt.hpp>
#include <algo/cobalt/options.hpp>
#include "cobalt_test_util.hpp"

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

/// Calculate the size of a static array
#define STATIC_ARRAY_SIZE(array) (sizeof(array)/sizeof(*array))

USING_NCBI_SCOPE;
USING_SCOPE(cobalt);
USING_SCOPE(objects);


/// Representation of a hit for computing constraints
struct SHit {
    /// query id
    string query;

    /// subject ordinal id in the database
    int subject;

    /// alignment score
    int score;

    /// alignment extents
    TRange query_range;
    TRange subject_range;

    SHit(void) : query("lcl|null"), subject(-1), score(-1),
                 query_range(TRange(-1, -1)), subject_range(TRange(-1, -1))
    {}
};


/// Test class for accessing CMultiAligner private attributes and methods
class CMultiAlignerTest
{
public:
    
    /// Set queries in the aligner only as Seq-locs do not retrieve sequences.
    /// Useful for testing functions that only match sequence ids and do not
    /// align sequences
    static void SetQuerySeqlocs(CMultiAligner& aligner,
                                const vector< CRef<CSeq_loc> >& queries)
    {
        aligner.m_tQueries.clear();
        ITERATE (vector< CRef<CSeq_loc> >, it, queries) {
            BOOST_REQUIRE(it->NotEmpty());
            aligner.m_tQueries.push_back(*it);
        }
    }

    static const vector<bool>& GetIsDomainSearched(
                                             const CMultiAligner& aligner)
    {
        return aligner.m_IsDomainSearched;
    }

    static const CHitList& GetDomainHits(const CMultiAligner& aligner)
    {
        return aligner.m_DomainHits;
    }

    /// Quit after doing RPS-BLAST search
    static bool InterruptAfterRpsBlastSearch(CMultiAligner::SProgress* progress)
    {
        return progress->stage == CMultiAligner::eDomainHitsSearch;
    }

    /// Compare domain hits in CMultiAligner with reference alignements
    /// @param expected_hits Reference alignments [in]
    /// @param aligner CMultiAligner object with with domain hits to compare [in]
    /// @param err Error messages [out]
    /// @return True if all hits are the same as reference, false otherwise
    static bool CompareDomainHits(const vector<SHit>& expected_hits,
                                  const CMultiAligner& aligner,
                                  string& err)
    {
        bool retval = true;
        const CHitList& hitlist = aligner.m_DomainHits;

        // compare numbers of hits
        if ((int)expected_hits.size() != hitlist.Size()) {
            err += "Hitlist sizes "
                + NStr::UIntToString((unsigned)expected_hits.size())
                + " and "
                + NStr::IntToString(hitlist.Size())
                + " do not match\n";

            retval = false;
        }
    
        for (size_t i=0;i < min(expected_hits.size(), (size_t)hitlist.Size());
             i++) {

            string header = "Hit " + NStr::UIntToString((unsigned)i) + ": ";
            const CHit* hit = hitlist.GetHit(i);

            // compare query ids
            CSeq_id expected_query_id(expected_hits[i].query);
            if (aligner.GetQueries()[hit->m_SeqIndex1]->GetId()
                ->CompareOrdered(expected_query_id) != 0) {

                err += header + "Query ids " + expected_hits[i].query
                    + " and " + aligner.GetQueries()[hit->m_SeqIndex1]
                    ->GetId()->AsFastaString() + " do not match\n";

                retval = false;
            }

            // compare subject ordinal ids
            if (expected_hits[i].subject != hit->m_SeqIndex2) {
                err += header + "Subject ids "
                    + NStr::IntToString(expected_hits[i].subject)
                    + " and " + NStr::IntToString(hit->m_SeqIndex2)
                    + " do not match\n";

                retval = false;
            }

            // compare query ranges
            if (expected_hits[i].query_range.GetFrom()
                != hit->m_SeqRange1.GetFrom()
                || expected_hits[i].query_range.GetTo()
                != hit->m_SeqRange1.GetTo()) {

                err += header + "Query ranges "
                    + NStr::IntToString(expected_hits[i].query_range.GetFrom())
                    + "-"
                    + NStr::IntToString(expected_hits[i].query_range.GetTo())
                    + " and "
                    + NStr::IntToString(hit->m_SeqRange1.GetFrom()) + "-"
                    + NStr::IntToString(hit->m_SeqRange1.GetTo())
                    + " do not match\n";

                retval = false;
            }
                    
            // compare subjet ranges
            if (expected_hits[i].subject_range.GetFrom()
                != hit->m_SeqRange2.GetFrom()
                || expected_hits[i].subject_range.GetTo()
                != hit->m_SeqRange2.GetTo()) {

                err += header + "Subject ranges "
                    + NStr::IntToString(expected_hits[i].subject_range.GetFrom())
                    + "-"
                    + NStr::IntToString(expected_hits[i].subject_range.GetTo())
                    + " and "
                    + NStr::IntToString(hit->m_SeqRange2.GetFrom()) + "-"
                    + NStr::IntToString(hit->m_SeqRange2.GetTo())
                    + " do not match\n";

                retval = false;
            }

            // compare alignment scores
            if (expected_hits[i].score != hit->m_Score) {
                err += header + "Scores "
                    + NStr::IntToString(expected_hits[i].score)
                    + " and "
                    + NStr::IntToString(hit->m_Score)
                    + " do not match\n";

                retval = false;
            }
        }

        return retval;
    }
};


/// Fixture class initialized for each multialigner test
class CMultiAlignerFixture
{
public:
    static CRef<CObjectManager> m_Objmgr;   
    static CRef<CScope> m_Scope;
    static vector< CRef<CSeq_loc> > m_Sequences;
    static CRef<CSeq_align> m_Align1;
    static CRef<CSeq_align> m_Align2;
    static CRef<CBlast4_archive> m_RpsArchive;

    CRef<CMultiAlignerOptions> m_Options;
    CRef<CMultiAligner> m_Aligner;
    
    CMultiAlignerFixture(void)
    {
        m_Options.Reset(new CMultiAlignerOptions);

#if defined(WORDS_BIGENDIAN) || defined(IS_BIG_ENDIAN)
        m_Options->SetRpsDb("data/cddtest_be");
#else
        m_Options->SetRpsDb("data/cddtest_le");
#endif

        m_Aligner.Reset(new CMultiAligner(m_Options));
    }

    ~CMultiAlignerFixture()
    {
        m_Options.Reset();
        m_Aligner.Reset();
    }

    /// Initialize scope
    static void x_InitScope(void)
    {
        m_Objmgr = CObjectManager::GetInstance();
        m_Scope.Reset(new CScope(*m_Objmgr));
        m_Scope->AddDefaults();
    }

    /// Read test sequences in FASTA format from file
    static void x_ReadSequences(void)
    {
        bool kParseDeflines = true;
        int status = ReadFastaQueries("data/small.fa", m_Sequences, m_Scope,
                                      kParseDeflines);

        if (status) {
            NCBI_THROW(CException, eInvalid,
                       "Reading FASTA sequences has failed");
        }
    }

    /// Read test MSAs from files
    static void x_ReadAlignments(void)
    {
        bool kParseDeflines = true;
        int status = ReadMsa("data/msa1.fa", m_Align1, m_Scope,
                             kParseDeflines);

        if (status) {
            NCBI_THROW(CException, eInvalid, "Reading alignments failed");
        }

        status = ReadMsa("data/msa2.fa", m_Align2, m_Scope,
                         kParseDeflines);

        if (status) {
            NCBI_THROW(CException, eInvalid, "Reading alignments failed");
        }
    }

    /// Read test RPS-BLAST output in the archive format from file
    static void x_ReadRpsArchive(void)
    {
        m_RpsArchive.Reset(new CBlast4_archive);
        CNcbiIfstream istr("data/rps_archive_seqloclist.asn");
        if (!istr) {
            NCBI_THROW(CException, eInvalid, "RPS-BLAST archive not found");
        }
        istr >> MSerial_AsnText >> *m_RpsArchive;
    }

    /// Initialize static attributes
    static void Initialize(void)
    {
        x_InitScope();
        x_ReadSequences();
        x_ReadAlignments();
        x_ReadRpsArchive();
    }

    /// Release static attributes
    static void Finalize(void)
    {
        m_Sequences.clear();
        m_Scope.Reset();
        m_Objmgr.Reset();
        m_Align1.Reset();
        m_Align2.Reset();
        m_RpsArchive.Reset();
    }
};

CRef<CObjectManager> CMultiAlignerFixture::m_Objmgr;
CRef<CScope> CMultiAlignerFixture::m_Scope;
vector< CRef<CSeq_loc> > CMultiAlignerFixture::m_Sequences;
CRef<CSeq_align> CMultiAlignerFixture::m_Align1;
CRef<CSeq_align> CMultiAlignerFixture::m_Align2;
CRef<CBlast4_archive> CMultiAlignerFixture::m_RpsArchive;


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

NCBITEST_AUTO_INIT()
{
    CMultiAlignerFixture::Initialize();
}

NCBITEST_AUTO_FINI()
{
    CMultiAlignerFixture::Finalize();
}

BOOST_FIXTURE_TEST_SUITE(multialigner, CMultiAlignerFixture)


// Make sure assiging query sequences are assigned properly
BOOST_AUTO_TEST_CASE(TestSetQueries)
{
    vector< CRef<CBioseq> > bioseqs;

    // Test for fasta input
    s_MakeBioseqs(m_Sequences, m_Scope, bioseqs);
    s_TestQueriesAsSeq_locs(m_Sequences, m_Scope);
    s_TestQueriesAsBioseqs(bioseqs);

    // Test for sequences as gis
    // There is a problem with CSeqVector

    // Using gis causes problems with CSeqVector
    CSeq_id id("gi|129295");
    CBioseq_Handle h = m_Scope->GetBioseqHandle(id);
    BOOST_CHECK(h.GetSeqId()->Match(id));

    CSeq_loc s;
    s.SetInt().SetFrom(0);
    s.SetInt().SetTo(sequence::GetLength(id, m_Scope)-1);
    s.SetInt().SetId().Assign(id);
    h = m_Scope->GetBioseqHandle(s);

    //TODO: test for sequences as gis and mix.
    // Currently there is a problem in CSeqVector used in CSequence
    // CSeqMap::GetLength(CScope*) throws exception for some reason
}


BOOST_AUTO_TEST_CASE(TestBadQueries)
{
    CMultiAligner aligner;

    vector< CRef<CSeq_loc> > seqlocs;
    vector< CRef<CBioseq> > bioseqs;

    // Empty list of qurey sequences causes exception
    BOOST_CHECK_THROW(aligner.SetQueries(seqlocs, m_Scope),
                      CMultiAlignerException);

    BOOST_CHECK_THROW(aligner.SetQueries(bioseqs), CMultiAlignerException);
    
    // Empty seqloc or bioseq causes exception
    seqlocs.push_back(CRef<CSeq_loc>(new CSeq_loc()));
    seqlocs.push_back(CRef<CSeq_loc>(new CSeq_loc()));
    BOOST_CHECK_THROW(aligner.SetQueries(seqlocs, m_Scope),
                      CMultiAlignerException);

    bioseqs.push_back(CRef<CBioseq>(new CBioseq()));
    bioseqs.push_back(CRef<CBioseq>(new CBioseq()));
    BOOST_CHECK_THROW(aligner.SetQueries(bioseqs), CMultiAlignerException);

    seqlocs.clear();
    seqlocs.push_back(m_Sequences.front());

    // Single input sequence causes exception
    BOOST_REQUIRE_EQUAL((int)seqlocs.size(), 1);
    BOOST_CHECK_THROW(aligner.SetQueries(seqlocs, m_Scope),
                      CMultiAlignerException);

    s_MakeBioseqs(seqlocs, m_Scope, bioseqs);
    BOOST_CHECK_THROW(aligner.SetQueries(bioseqs), CMultiAlignerException);

    // A gap in input sequence causes exception
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*objmgr));
    seqlocs.clear();
    int status = ReadFastaQueries("data/queries_with_gaps.fa", seqlocs, scope);
    BOOST_REQUIRE_EQUAL(status, 0);
    BOOST_CHECK_THROW(aligner.SetQueries(seqlocs, scope),
                      CMultiAlignerException);

    s_MakeBioseqs(seqlocs, scope, bioseqs);
    BOOST_CHECK_THROW(aligner.SetQueries(bioseqs), CMultiAlignerException);
}


// Bad user constraints must be reported
BOOST_AUTO_TEST_CASE(TestBadUserConstraints)
{
    vector< CRef<CBioseq> > bioseqs;
    s_MakeBioseqs(m_Sequences, m_Scope, bioseqs);

    m_Options->SetRpsDb("");
    CMultiAlignerOptions::TConstraints& constr = m_Options->SetUserConstraints();
    constr.resize(1);
    
    int query_index = (int)m_Sequences.size();
    constr[0].seq1_index = query_index;
    constr[0].seq1_start = 0;
    constr[0].seq1_stop = 50;

    constr[0].seq2_index = 0;
    constr[0].seq2_start = 0;
    constr[0].seq2_stop = 50;

    // this problem cannot be detected in options validation before queries
    // are set
    BOOST_REQUIRE(m_Options->Validate());

    m_Aligner.Reset(new CMultiAligner(m_Options));
    BOOST_CHECK_THROW(m_Aligner->SetQueries(m_Sequences, m_Scope),
                      CMultiAlignerException);

    m_Aligner.Reset(new CMultiAligner(m_Options));
    BOOST_CHECK_THROW(m_Aligner->SetQueries(bioseqs),
                      CMultiAlignerException);
    
    // user constraint with position of ouf of sequence range must be reported
    constr.clear();
    constr.resize(1);
    constr[0].seq1_index = 0;
    constr[0].seq1_start = 0;
    constr[0].seq1_stop = 50;

    int length = (int)sequence::GetLength(*m_Sequences[1],
                                          m_Scope.GetNonNullPointer());

    constr[0].seq2_index = 1;
    constr[0].seq2_start = 0;
    constr[0].seq2_stop = length + 1;

    BOOST_REQUIRE(m_Options->Validate());

    m_Aligner.Reset(new CMultiAligner(m_Options));
    BOOST_CHECK_THROW(m_Aligner->SetQueries(m_Sequences, m_Scope),
                      CMultiAlignerException);    

    m_Aligner.Reset(new CMultiAligner(m_Options));
    BOOST_CHECK_THROW(m_Aligner->SetQueries(bioseqs),
                      CMultiAlignerException);    
}

BOOST_AUTO_TEST_CASE(TestNoResults)
{
    // Getting results without computing them causes exception
    BOOST_CHECK_THROW(m_Aligner->GetResults(), CMultiAlignerException);
    BOOST_CHECK_THROW(m_Aligner->GetTreeContainer(), CMultiAlignerException);
}


static bool s_Interrupt(CMultiAligner::SProgress* progress)
{
    return true;
}

BOOST_AUTO_TEST_CASE(TestInterrupt)
{
    m_Aligner->SetQueries(m_Sequences, m_Scope);
    m_Aligner->SetInterruptCallback(s_Interrupt);
    CMultiAligner::TStatus status = m_Aligner->Run();
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
    BOOST_REQUIRE(m_Options->Validate());

    m_Aligner->SetQueries(m_Sequences, m_Scope);

    CMultiAligner::TStatus status = m_Aligner->Run();
    BOOST_REQUIRE_EQUAL(status, (CMultiAligner::TStatus)CMultiAligner::eSuccess);
    BOOST_CHECK_EQUAL((int)m_Aligner->GetMessages().size(), 0);

    s_TestResults(*m_Aligner);
//    s_TestResults(aligner, "data/ref_seqalign.asn");
}


// Max cluster diameter set to zero must cause a warning
BOOST_AUTO_TEST_CASE(TestResultsForZeroClusterDiam)
{
    m_Options->SetMaxInClusterDist(0.0);
    BOOST_REQUIRE(m_Options->Validate());

    m_Aligner.Reset(new CMultiAligner(m_Options));
    m_Aligner->SetQueries(m_Sequences, m_Scope);

    CMultiAligner::TStatus status = m_Aligner->Run();

    //Bad input file - there are two exactly same sequences

    BOOST_REQUIRE_EQUAL(status, (CMultiAligner::TStatus)CMultiAligner::eSuccess);
    BOOST_CHECK((int)m_Aligner->GetMessages().size() == 0);

    s_TestResults(*m_Aligner);
}


// Max cluster diameter set to 1 (maxiumu value) must cause a warning
BOOST_AUTO_TEST_CASE(TestResultsForMaxClusterDiam)
{
    m_Options->SetMaxInClusterDist(1.0);
    BOOST_REQUIRE(m_Options->Validate());

    m_Aligner.Reset(new CMultiAligner(m_Options));
    m_Aligner->SetQueries(m_Sequences, m_Scope);

    CMultiAligner::TStatus status = m_Aligner->Run();

    //Bad input file - there are two exactly same sequences

    BOOST_REQUIRE_EQUAL(status, (CMultiAligner::TStatus)CMultiAligner::eWarnings);
    BOOST_CHECK((int)m_Aligner->GetMessages().size() > 0);

    s_TestResults(*m_Aligner);
}


BOOST_AUTO_TEST_CASE(TestResultsForNoClusters)
{
    m_Options.Reset(new CMultiAlignerOptions(
                                CMultiAlignerOptions::fNoQueryClusters
                                | CMultiAlignerOptions::fNoRpsBlast));

    BOOST_REQUIRE(m_Options->Validate());

    m_Aligner.Reset(new CMultiAligner(m_Options));
    m_Aligner->SetQueries(m_Sequences, m_Scope);

    CMultiAligner::TStatus status = m_Aligner->Run();
    BOOST_REQUIRE_EQUAL(status, (CMultiAligner::TStatus)CMultiAligner::eSuccess);
    BOOST_CHECK_EQUAL((int)m_Aligner->GetMessages().size(), 0);

    s_TestResults(*m_Aligner);
}


// Make sure that queries that appear in user constraints form one-element
// clusters
BOOST_AUTO_TEST_CASE(TestResultsForClustersAndUserConstraints)
{
    vector<CMultiAlignerOptions::SConstraint>& constr
        = m_Options->SetUserConstraints();

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

    BOOST_REQUIRE(m_Options->Validate());

    m_Aligner.Reset(new CMultiAligner(m_Options));
    m_Aligner->SetQueries(m_Sequences, m_Scope);

    CMultiAligner::TStatus status = m_Aligner->Run();
    BOOST_REQUIRE_EQUAL(status, (CMultiAligner::TStatus)CMultiAligner::eSuccess);
    BOOST_CHECK_EQUAL((int)m_Aligner->GetMessages().size(), 0);

    s_TestResults(*m_Aligner);
}


// Two sequences is a special case for query clustering and computing
// guide tree as cluster dendrogram
BOOST_AUTO_TEST_CASE(TestTwoSequences)
{
    // case with one cluster
    m_Options->SetMaxInClusterDist(1.0);
    m_Aligner.Reset(new CMultiAligner(m_Options));

    vector< CRef<CSeq_loc> > queries;
    queries.push_back(m_Sequences[0]);
    queries.push_back(m_Sequences[1]);
    m_Aligner->SetQueries(queries, m_Scope);
    
    CMultiAligner::TStatus status = m_Aligner->Run();
    BOOST_CHECK(status == CMultiAligner::eSuccess
                || status == CMultiAligner::eWarnings);
    s_TestResults(*m_Aligner);

    // case with two clusters
    m_Options->SetMaxInClusterDist(0.01);
    m_Aligner.Reset(new CMultiAligner(m_Options));
    m_Aligner->SetQueries(queries, m_Scope);
    status = m_Aligner->Run();
    BOOST_CHECK(status == CMultiAligner::eSuccess
                || status == CMultiAligner::eWarnings);
    s_TestResults(*m_Aligner);
}



void s_TestAlignmentFromMSAs(CRef<CSeq_align> result, CRef<CSeq_align> in_first,
                              CRef<CSeq_align> in_second)
{
    // alignment must be of type global
    BOOST_REQUIRE_EQUAL(result->GetType(), CSeq_align::eType_global);

    BOOST_REQUIRE(result->GetSegs().IsDenseg());
    BOOST_REQUIRE(in_first->GetSegs().IsDenseg());
    BOOST_REQUIRE(in_second->GetSegs().IsDenseg());

    const CDense_seg& first_denseg = in_first->GetSegs().GetDenseg();
    const CDense_seg& second_denseg = in_second->GetSegs().GetDenseg();

    int num_input_sequences = (int)first_denseg.GetDim()
        + second_denseg.GetDim();

    // dim must be equal to number of input sequences
    BOOST_REQUIRE_EQUAL(result->GetDim(), num_input_sequences);

    vector<int> first_rows, second_rows;

    // all sequence ids in input alignments must be present in the result
    for (size_t i=0;i < first_denseg.GetIds().size();i++) {
        const CSeq_id& id = *first_denseg.GetIds()[i];
        int j = 0;
        for (;j < result->GetDim();j++) {
            if (id.Match(*result->GetSegs().GetDenseg().GetIds()[j])) {
                first_rows.push_back(j);
                break;
            }
        }
        BOOST_REQUIRE(j < result->GetDim());
    }

    for (size_t i=0;i < second_denseg.GetIds().size();i++) {
        const CSeq_id& id = *second_denseg.GetIds()[i];
        int j = 0;
        for (;j < result->GetDim();j++) {
            if (id.Match(*result->GetSegs().GetDenseg().GetIds()[j])) {
                second_rows.push_back(j);
                break;
            }
        }
        BOOST_REQUIRE(j < result->GetDim());
    }

    // if one extracts input alignments from the result, they should be the
    // same as the input alignments

    // compare the first input alignment
    CRef<CDense_seg> f = result->GetSegs().GetDenseg().ExtractRows(first_rows);
    f->RemovePureGapSegs();
    f->Compact();
    BOOST_REQUIRE_EQUAL(first_denseg.GetStarts().size(), f->GetStarts().size());
    BOOST_REQUIRE_EQUAL(first_denseg.GetLens().size(), f->GetLens().size());
    for (size_t i=0;i < first_denseg.GetStarts().size();i++) {
        BOOST_REQUIRE_EQUAL(first_denseg.GetStarts()[i], f->GetStarts()[i]);
    }
    for (size_t i=0;i < first_denseg.GetLens().size();i++) {
        BOOST_REQUIRE_EQUAL(first_denseg.GetLens()[i], f->GetLens()[i]);
    }

    CRef<CDense_seg> s = result->GetSegs().GetDenseg().ExtractRows(second_rows);

    // compare the second input alignment
    s->RemovePureGapSegs();
    s->Compact();
    BOOST_REQUIRE_EQUAL(second_denseg.GetStarts().size(), s->GetStarts().size());
    BOOST_REQUIRE_EQUAL(second_denseg.GetLens().size(), s->GetLens().size());
    for (size_t i=0;i < second_denseg.GetStarts().size();i++) {
        BOOST_REQUIRE_EQUAL(second_denseg.GetStarts()[i], s->GetStarts()[i]);
    }
    for (size_t i=0;i < second_denseg.GetLens().size();i++) {
        BOOST_REQUIRE_EQUAL(second_denseg.GetLens()[i], s->GetLens()[i]);
    }
}


BOOST_AUTO_TEST_CASE(TestAlignMSAs)
{
    set<int> repr;

    // align input MSAs
    m_Aligner->SetInputMSAs(*m_Align1, *m_Align2, repr, repr, m_Scope);
    m_Aligner->Run();

    // test result
    s_TestAlignmentFromMSAs(m_Aligner->GetResults(), m_Align1, m_Align2);
}


BOOST_AUTO_TEST_CASE(TestAlignMSAWithSequence)
{
    set<int> repr;

    // create a one-sequence Seq_align from the input sequence
    CRef<CSeq_align> align(new CSeq_align());
    align->SetType(CSeq_align::eType_global);
    align->SetDim(1);
    CDense_seg& denseg = align->SetSegs().SetDenseg();
    denseg.SetDim(1);
    denseg.SetNumseg(1);
    denseg.SetIds().push_back(CRef<CSeq_id>(
                         const_cast<CSeq_id*>(m_Sequences.front()->GetId())));
    denseg.SetStarts().push_back(0);
    CSeqVector v(*m_Sequences.front(), *m_Scope);
    denseg.SetLens().push_back(v.size());
    
    // align MSA and sequence
    m_Aligner->SetInputMSAs(*m_Align1, *align, repr, repr, m_Scope);
    m_Aligner->Run();

    // test result
    s_TestAlignmentFromMSAs(m_Aligner->GetResults(), m_Align1, align);
}

BOOST_AUTO_TEST_CASE(TestAlignMSAsWithRepresentatives)
{
    // set representatives
    BOOST_REQUIRE(m_Align1->CheckNumRows() > 3);
    BOOST_REQUIRE(m_Align2->CheckNumRows() > 3);
    set<int> repr;
    repr.insert(2);
    repr.insert(3);

    // align input MSAs
    m_Aligner->SetInputMSAs(*m_Align1, *m_Align2, repr, repr, m_Scope);
    m_Aligner->Run();

    // test result
    s_TestAlignmentFromMSAs(m_Aligner->GetResults(), m_Align1, m_Align2);
}

BOOST_AUTO_TEST_CASE(TestAlignMSAsWithWrongRepresentatives)
{
    // set representatives
    set<int> repr;

    // indeces of representatives must be non-negative
    repr.insert(-1);
    BOOST_CHECK_THROW(m_Aligner->SetInputMSAs(*m_Align1, *m_Align2, repr, repr,
                                              m_Scope),
                      CMultiAlignerException);


    // indeces of representatives must be smaller than number of sequences
    // in MSA
    repr.clear();
    repr.insert(m_Align1->CheckNumRows());
    BOOST_CHECK_THROW(m_Aligner->SetInputMSAs(*m_Align1, *m_Align2, repr, repr,
                                              m_Scope),
                      CMultiAlignerException);
}

BOOST_AUTO_TEST_CASE(TestSetPrecomputedDomainHitsWithQueriesAsBioseqset)
{
    // make a copy of sequence ids
    vector< CRef<CSeq_id> > expected_queries;
    ITERATE(vector< CRef<CSeq_loc> >, it, m_Sequences) {
        string id = (*it)->GetId()->AsFastaString();
        expected_queries.push_back(CRef<CSeq_id>(new CSeq_id(id)));
    }

    // set options
    m_Options->SetUseQueryClusters(false);
    m_Options->SetRpsEvalue(0.1);

    m_Aligner.Reset(new CMultiAligner(m_Options));
    m_Aligner->SetQueries(m_Sequences, m_Scope);

    // read RPS-BLAST archive
    CBlast4_archive archive;
    CNcbiIfstream istr("data/rps_archive_bioseqset.asn");
    istr >> MSerial_AsnText >> archive;

    // check pre conditions
    BOOST_REQUIRE_EQUAL(m_Options->GetUseQueryClusters(), false);
    BOOST_REQUIRE(fabs(m_Options->GetRpsEvalue() - 0.1) < 0.01);
                         
    // set pre-computed domain hits
    m_Aligner->SetDomainHits(archive);

    // Tests

    // verify sure that queries did not change
    BOOST_REQUIRE_EQUAL(expected_queries.size(),
                        m_Aligner->GetQueries().size());

    for (size_t i=0;i < expected_queries.size();i++) {
        BOOST_REQUIRE(expected_queries[i]->CompareOrdered(
                          *m_Aligner->GetQueries()[i]->GetId()) == 0);
    }

    // expected values of CMultiAligner::m_IsSearchedDomain
    vector<bool> expected_is_domain_searched(m_Sequences.size(), false);
    expected_is_domain_searched[0] = true;
    expected_is_domain_searched[2] = true;

    BOOST_REQUIRE_EQUAL(expected_is_domain_searched.size(),
                CMultiAlignerTest::GetIsDomainSearched(*m_Aligner).size());

    for (size_t i=0;i < expected_is_domain_searched.size();i++) {
        BOOST_REQUIRE_EQUAL(expected_is_domain_searched[i],
                    CMultiAlignerTest::GetIsDomainSearched(*m_Aligner)[i]);
    }


    // Compare domain hits

    const size_t kNumExpectedPreHits = 7;
    vector<SHit> expected_hits(kNumExpectedPreHits);

    // Domain Hit #0
    expected_hits[0].query = "lcl|1buc_A";
    expected_hits[0].subject = 1;
    expected_hits[0].query_range = TRange(6, 382);
    expected_hits[0].subject_range = TRange(0, 372);
    expected_hits[0].score = 1414;

    // Domain Hit #1
    expected_hits[1].query = "lcl|1buc_A";
    expected_hits[1].subject = 0;
    expected_hits[1].query_range = TRange(95, 377);
    expected_hits[1].subject_range = TRange(42, 325);
    expected_hits[1].score = 885;

    // Domain Hit #2
    expected_hits[2].query = "lcl|1buc_A";
    expected_hits[2].subject = 2;
    expected_hits[2].query_range = TRange(1, 382);
    expected_hits[2].subject_range = TRange(19, 405);
    expected_hits[2].score = 718;

    // Domain Hit #3
    expected_hits[3].query = "lcl|Q8jzn5";
    expected_hits[3].subject = 2;
    expected_hits[3].query_range = TRange(41, 448);
    expected_hits[3].subject_range = TRange(0, 408);
    expected_hits[3].score = 1779;

    // Domain Hit #4
    expected_hits[4].query = "lcl|Q8jzn5";
    expected_hits[4].subject = 1;
    expected_hits[4].query_range = TRange(88, 440);
    expected_hits[4].subject_range = TRange(22, 367);
    expected_hits[4].score = 981;

    // Domain Hit #5
    expected_hits[5].query = "lcl|Q8jzn5";
    expected_hits[5].subject = 0;
    expected_hits[5].query_range = TRange(151, 440);
    expected_hits[5].subject_range = TRange(42, 325);
    expected_hits[5].score = 872;

    // Domain Hit #6
    expected_hits[6].query = "lcl|Q8jzn5";
    expected_hits[6].subject = 0;
    expected_hits[6].query_range = TRange(511, 581);
    expected_hits[6].subject_range = TRange(208, 280);
    expected_hits[6].score = 75;

    string errors;
    bool hits_match = CMultiAlignerTest::CompareDomainHits(expected_hits,
                                                           *m_Aligner,
                                                           errors);

    BOOST_REQUIRE_MESSAGE(hits_match, errors);


    // Do RPS-BLAST search inside aligner and verify that the search was done
    // only for queries without pre-computed domain hits

    // set interrupt so that COBALT quits right after RPS-BLAST search
    m_Aligner->SetInterruptCallback(
                          CMultiAlignerTest::InterruptAfterRpsBlastSearch);
    m_Aligner->Run();

    const size_t kNumExpectedHits = 10;
    BOOST_REQUIRE(kNumExpectedHits > kNumExpectedPreHits);
    expected_hits.resize(kNumExpectedHits);


    // Domain Hit #7
    expected_hits[7].query = "lcl|Q10535";
    expected_hits[7].subject = 2;
    expected_hits[7].query_range = TRange(27, 432);
    expected_hits[7].subject_range = TRange(0, 400);
    expected_hits[7].score = 768;
    
    // Domain Hit #8
    expected_hits[8].query = "lcl|Q10535";
    expected_hits[8].subject = 0;
    expected_hits[8].query_range = TRange(138, 433);
    expected_hits[8].subject_range = TRange(42, 326);
    expected_hits[8].score = 738;

    // Domain Hit #9
    expected_hits[9].query = "lcl|Q10535";
    expected_hits[9].subject = 1;
    expected_hits[9].query_range = TRange(75, 434);
    expected_hits[9].subject_range = TRange(24, 369);
    expected_hits[9].score = 704;


    hits_match = CMultiAlignerTest::CompareDomainHits(expected_hits, *m_Aligner,
                                                      errors);

    BOOST_REQUIRE_MESSAGE(hits_match, errors);
}

BOOST_AUTO_TEST_CASE(TestSetPrecomputedDomainHitsWithQueriesAsSeqLocs)
{
    // make a copy of sequence ids
    vector< CRef<CSeq_id> > expected_queries;
    ITERATE(vector< CRef<CSeq_loc> >, it, m_Sequences) {
        string id = (*it)->GetId()->AsFastaString();
        expected_queries.push_back(CRef<CSeq_id>(new CSeq_id(id)));
    }

    // set options
    m_Options->SetUseQueryClusters(false);
    m_Options->SetRpsEvalue(0.1);
    m_Aligner.Reset(new CMultiAligner(m_Options));

    m_Aligner->SetQueries(m_Sequences, m_Scope);

    // check pre conditions
    BOOST_REQUIRE_EQUAL(m_Options->GetUseQueryClusters(), false);
    BOOST_REQUIRE(fabs(m_Options->GetRpsEvalue() - 0.1) < 0.01);
                         
    // set pre-computed domain hits
    m_Aligner->SetDomainHits(*m_RpsArchive);

    // Tests

    // verify sure that queries did not change
    BOOST_REQUIRE_EQUAL(expected_queries.size(),
                        m_Aligner->GetQueries().size());

    for (size_t i=0;i < expected_queries.size();i++) {
        BOOST_REQUIRE(expected_queries[i]->CompareOrdered(
                          *m_Aligner->GetQueries()[i]->GetId()) == 0);
    }

    // expected values of CMultiAligner::m_IsSearchedDomain
    vector<bool> expected_is_domain_searched(m_Sequences.size(), false);
    expected_is_domain_searched[0] = true;
    expected_is_domain_searched[2] = true;

    BOOST_REQUIRE_EQUAL(expected_is_domain_searched.size(),
                CMultiAlignerTest::GetIsDomainSearched(*m_Aligner).size());

    for (size_t i=0;i < expected_is_domain_searched.size();i++) {
        BOOST_REQUIRE_EQUAL(expected_is_domain_searched[i],
                    CMultiAlignerTest::GetIsDomainSearched(*m_Aligner)[i]);
    }


    // Compare domain hits

    const size_t kNumExpectedPreHits = 7;
    vector<SHit> expected_hits(kNumExpectedPreHits);

    // Domain Hit #0
    expected_hits[0].query = "lcl|1buc_A";
    expected_hits[0].subject = 1;
    expected_hits[0].query_range = TRange(6, 382);
    expected_hits[0].subject_range = TRange(0, 372);
    expected_hits[0].score = 1414;

    // Domain Hit #1
    expected_hits[1].query = "lcl|1buc_A";
    expected_hits[1].subject = 0;
    expected_hits[1].query_range = TRange(95, 377);
    expected_hits[1].subject_range = TRange(42, 325);
    expected_hits[1].score = 885;

    // Domain Hit #2
    expected_hits[2].query = "lcl|1buc_A";
    expected_hits[2].subject = 2;
    expected_hits[2].query_range = TRange(1, 382);
    expected_hits[2].subject_range = TRange(19, 405);
    expected_hits[2].score = 718;

    // Domain Hit #3
    expected_hits[3].query = "lcl|Q8jzn5";
    expected_hits[3].subject = 2;
    expected_hits[3].query_range = TRange(41, 448);
    expected_hits[3].subject_range = TRange(0, 408);
    expected_hits[3].score = 1779;

    // Domain Hit #4
    expected_hits[4].query = "lcl|Q8jzn5";
    expected_hits[4].subject = 1;
    expected_hits[4].query_range = TRange(88, 440);
    expected_hits[4].subject_range = TRange(22, 367);
    expected_hits[4].score = 981;

    // Domain Hit #5
    expected_hits[5].query = "lcl|Q8jzn5";
    expected_hits[5].subject = 0;
    expected_hits[5].query_range = TRange(151, 440);
    expected_hits[5].subject_range = TRange(42, 325);
    expected_hits[5].score = 872;

    // Domain Hit #6
    expected_hits[6].query = "lcl|Q8jzn5";
    expected_hits[6].subject = 0;
    expected_hits[6].query_range = TRange(511, 581);
    expected_hits[6].subject_range = TRange(208, 280);
    expected_hits[6].score = 75;

    string errors;
    bool hits_match = CMultiAlignerTest::CompareDomainHits(expected_hits,
                                                           *m_Aligner,
                                                           errors);

    BOOST_REQUIRE_MESSAGE(hits_match, errors);


    // Do RPS-BLAST search inside aligner and verify that the search was done
    // only for queries without pre-computed domain hits

    // set interrupt so that COBALT quits right after RPS-BLAST search
    m_Aligner->SetInterruptCallback(
                          CMultiAlignerTest::InterruptAfterRpsBlastSearch);
    m_Aligner->Run();

    const size_t kNumExpectedHits = 10;
    BOOST_REQUIRE(kNumExpectedHits > kNumExpectedPreHits);
    expected_hits.resize(kNumExpectedHits);


    // Domain Hit #7
    expected_hits[7].query = "lcl|Q10535";
    expected_hits[7].subject = 2;
    expected_hits[7].query_range = TRange(27, 432);
    expected_hits[7].subject_range = TRange(0, 400);
    expected_hits[7].score = 768;
    
    // Domain Hit #8
    expected_hits[8].query = "lcl|Q10535";
    expected_hits[8].subject = 0;
    expected_hits[8].query_range = TRange(138, 433);
    expected_hits[8].subject_range = TRange(42, 326);
    expected_hits[8].score = 738;

    // Domain Hit #9
    expected_hits[9].query = "lcl|Q10535";
    expected_hits[9].subject = 1;
    expected_hits[9].query_range = TRange(75, 434);
    expected_hits[9].subject_range = TRange(24, 369);
    expected_hits[9].score = 704;


    hits_match = CMultiAlignerTest::CompareDomainHits(expected_hits, *m_Aligner,
                                                      errors);

    BOOST_REQUIRE_MESSAGE(hits_match, errors);
}


BOOST_AUTO_TEST_CASE(TestSetPrecomputedDomainHitsWithNoMatchingQueries)
{
    // create cobalt queries with fake Seq-ids
    vector< CRef<CSeq_loc> > queries;
    CRef<CSeq_loc> seq(new CSeq_loc);
    seq->SetWhole().Set("lcl|fake");
    queries.push_back(seq);
    queries.push_back(seq);
    
    // set cobalt queries without retrieving sequences
    CMultiAlignerTest::SetQuerySeqlocs(*m_Aligner, queries);
    
    // set pre-computed domain hits
    m_Aligner->SetDomainHits(*m_RpsArchive);

    // verify that none of the pre-computed hits made it to the domain hit list
    BOOST_REQUIRE_EQUAL(CMultiAlignerTest::GetDomainHits(*m_Aligner).Size(), 0);

    BOOST_REQUIRE(CMultiAlignerTest::GetIsDomainSearched(*m_Aligner).empty());
}


BOOST_AUTO_TEST_CASE(TestSetPrecomputedDomainHitsAboveEThresh)
{
    // create cobalt queries that match at least one query in the RPS-BLAST
    // archive, and are added to m_Scope
    vector< CRef<CSeq_loc> > queries;
    CRef<CSeq_loc> seq(new CSeq_loc);
    seq->SetWhole().Set("gi|129295");
    queries.push_back(seq);
    queries.push_back(seq);
    
    // set options
    m_Options->SetRpsEvalue(10);

    // First make sure that there is a hit

    m_Aligner.Reset(new CMultiAligner(m_Options));

    // set cobalt queries
    m_Aligner->SetQueries(queries, m_Scope);

    // set pre-computed domain hits
    m_Aligner->SetDomainHits(*m_RpsArchive);

    // verify that there is at least one matching RPS-BLAST query with results
    BOOST_REQUIRE(CMultiAlignerTest::GetDomainHits(*m_Aligner).Size() > 0);


    // Test for low RPS-BLAST E-value

    m_Options->SetRpsEvalue(0.00001);
    m_Aligner.Reset(new CMultiAligner(m_Options));
    
    // set cobalt queries
    m_Aligner->SetQueries(queries, m_Scope);

    // set pre-computed domain hits
    m_Aligner->SetDomainHits(*m_RpsArchive);

    // verify that none of the pre-computed hits made it to the hit list
    BOOST_REQUIRE_EQUAL(CMultiAlignerTest::GetDomainHits(*m_Aligner).Size(), 0);
    BOOST_REQUIRE_EQUAL(
                     CMultiAlignerTest::GetIsDomainSearched(*m_Aligner).size(),
                     0u);
}


// verify that alignment runs without errors
BOOST_AUTO_TEST_CASE(TestAlignSequencesWithPrecomputedDomainHits)
{
    // test no query clusters
    m_Options->SetUseQueryClusters(false);
    m_Aligner.Reset(new CMultiAligner(m_Options));
    m_Aligner->SetQueries(m_Sequences, m_Scope);
    m_Aligner->SetDomainHits(*m_RpsArchive);
    m_Aligner->Run();

    s_TestResults(*m_Aligner);


    // test with query clusters
    m_Options->SetUseQueryClusters(true);
    m_Aligner.Reset(new CMultiAligner(m_Options));
    m_Aligner->SetQueries(m_Sequences, m_Scope);
    m_Aligner->SetDomainHits(*m_RpsArchive);
    m_Aligner->Run();

    s_TestResults(*m_Aligner);
}

BOOST_AUTO_TEST_CASE(TestAlignMSAsWithPrecomputedDomainHits)
{
    // set a larger RPS-BLAST e-value threshold, due to hits in m_RpsArchive
    m_Options->SetRpsEvalue(10);
    m_Aligner.Reset(new CMultiAligner(m_Options));
    set<int> repr;

    // Test without representative sequences

    m_Aligner->SetInputMSAs(*m_Align1, *m_Align2, repr, repr, m_Scope);
    m_Aligner->SetDomainHits(*m_RpsArchive);
    m_Aligner->Run();

    // test result
    s_TestAlignmentFromMSAs(m_Aligner->GetResults(), m_Align1, m_Align2);


    // Test with representative sequences
    repr.insert(0);

    m_Aligner.Reset(new CMultiAligner(m_Options));

    m_Aligner->SetInputMSAs(*m_Align1, *m_Align2, repr, repr, m_Scope);
    m_Aligner->SetDomainHits(*m_RpsArchive);
    m_Aligner->Run();

    // test result
    s_TestAlignmentFromMSAs(m_Aligner->GetResults(), m_Align1, m_Align2);
}


BOOST_AUTO_TEST_CASE(TestPrecomputedDomainSubjectNotInDatabase)
{
    m_Aligner->SetQueries(m_Sequences, m_Scope);

    // read RPS-BLAST archive
    CBlast4_archive archive;
    CNcbiIfstream istr("data/rps_archive_subjectnotindb.asn");
    BOOST_REQUIRE(istr);
    istr >> MSerial_AsnText >> archive;

    // get domain hits
    const CSeq_align& align =
        *archive.GetResults().GetAlignments().Get().front();

    // veryfy that expected query and subject pair is in the domain hits
    BOOST_REQUIRE_EQUAL(align.GetSeq_id(0).AsFastaString(),
                        (string)"lcl|1buc_A");
    BOOST_REQUIRE_EQUAL(align.GetSeq_id(1).AsFastaString(),
                        (string)"gnl|CDD|273847");

    // verify that all subjects in the domain hits must exist in the domain
    // database used by cobalt
    BOOST_REQUIRE_THROW(m_Aligner->SetDomainHits(archive),
                        CMultiAlignerException);
}

BOOST_AUTO_TEST_CASE(TestSetPrecomputedDomainHitsBeforeQueries)
{
    // verify that domain hits must be set after either queries or input MSAs
    // are set
    BOOST_REQUIRE_THROW(m_Aligner->SetDomainHits(*m_RpsArchive),
                        CMultiAlignerException);
}

BOOST_AUTO_TEST_CASE(TestSetDomainHitsWithNoCDD)
{
    // create a new aligner with options without specifying domain database
    m_Options.Reset(new CMultiAlignerOptions);
    m_Aligner.Reset(new CMultiAligner(m_Options));

    m_Aligner->SetQueries(m_Sequences, m_Scope);

    // domain database must be specified if pre-computed hits are used
    BOOST_REQUIRE_THROW(m_Aligner->SetDomainHits(*m_RpsArchive),
                        CMultiAlignerException);
}

BOOST_AUTO_TEST_CASE(TestSetDomainHitsWithUnsupportedQueries)
{
    m_Aligner->SetQueries(m_Sequences, m_Scope);

    // change queries to PSSM
    CPssm& pssm = m_RpsArchive->SetRequest().SetBody().SetQueue_search()
        .SetQueries().SetPssm().SetPssm();

    pssm.SetNumRows(28);
    pssm.SetNumColumns(200);

    // PSSM as query is not supported
    BOOST_REQUIRE_THROW(m_Aligner->SetDomainHits(*m_RpsArchive),
                        CMultiAlignerException);

    // re-read RPS-BLAST archive so that it is not changed for future tests
    CMultiAlignerFixture::x_ReadRpsArchive();
}


BOOST_AUTO_TEST_SUITE_END()

#endif /* SKIP_DOXYGEN_PROCESSING */
