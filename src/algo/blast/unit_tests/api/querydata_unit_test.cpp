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
 *   Unit test module for the query data extraction interface
 *
 * ===========================================================================
 */
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

#include <algo/blast/api/query_data.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/api/objmgrfree_query_data.hpp>

#include <algo/blast/api/blast_exception.hpp>

// needed to obtain the blast options handle from the search factory
#include <algo/blast/api/uniform_search.hpp>
#include <algo/blast/api/local_search.hpp>

// needed for objmgr dependent tests of query data interface
#include "test_objmgr.hpp"

#include <objects/seqloc/Seq_interval.hpp>  // for CSeq_int

#include <objtools/simple/simple_om.hpp> // Simple Object manager interface
#include <objmgr/util/seq_loc_util.hpp> // CSeq_loc utilities
#include <objmgr/util/sequence.hpp>     // for GetGiForAccession
#include <corelib/ncbidiag.hpp>         // for NCBI_CURRENT_FUNCTION
#include <serial/iterator.hpp>          // for CTypeConstIterator
// needed to perform translations between encodings
#include <objects/seq/seqport_util.hpp>

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;

// ========================================================================= //
// Helper Classes

class CSequenceDataTester
{
public:
    CSequenceDataTester(CRef<IQueryFactory> query_factory, 
                        int gi);

    CSequenceDataTester(CRef<IQueryFactory> query_factory, 
                        const vector<int>& gis);

    CSequenceDataTester(CRef<IQueryFactory> query_factory,
                        const IRemoteQueryData::TSeqLocs& seqlocs);

    void operator()(void);

private:
    CRef<IQueryFactory> m_QueryFactory;
    vector<int> m_Gis;
    vector<ENa_strand> m_Strands;
    CConstRef<CBlastOptions> m_Options;
    CRef<CScope> m_Scope;

    void x_Init(const IRemoteQueryData::TSeqLocs* seqlocs = 0);
    bool x_IsProtein();
    ENa_strand x_GetStrand(int index);
    void x_TestSingleSequence_Local(int index);
    void x_TestSingleSequence_Remote(int index);

    void x_TestSingleProtein_Local(int index,
                                   const BlastQueryInfo* qinfo,
                                   const BLAST_SequenceBlk* seqblk);
    void x_TestSingleNucleotide_Local(int index,
                                      const BlastQueryInfo* qinfo,
                                      const BLAST_SequenceBlk* seqblk);
    void x_TestSingleTranslatedNucl_Local(int index,
                                          const BlastQueryInfo* qinfo, 
                                          const BLAST_SequenceBlk* seqblk);
    void x_TestSingleProtein_Remote(const CSeq_id& id,
                                    const CSeq_inst& seq_inst);
    void x_TestSingleNucleotide_Remote(const CSeq_id& id,
                                       const CSeq_inst& seq_inst);

    void x_TestLocalStrand(const CSeq_id& id,
                           int ctx_index,
                           ENa_strand strand,
                           const BlastQueryInfo* qinfo,
                           const BLAST_SequenceBlk* seqblk);
    void x_CompareSequenceData(CSeqVector& sv, const Uint1* sequence,
                               const string& strand = "");
};

CSequenceDataTester::CSequenceDataTester(CRef<IQueryFactory> query_factory, 
                                         int gi)
    : m_QueryFactory(query_factory), m_Gis(1, gi)
{
    x_Init();
}

CSequenceDataTester::CSequenceDataTester(CRef<IQueryFactory> query_factory, 
                                         const vector<int>& gis)
    : m_QueryFactory(query_factory), m_Gis(gis)
{
    BOOST_REQUIRE(!gis.empty());
    x_Init();
}

CSequenceDataTester::CSequenceDataTester(CRef<IQueryFactory> query_factory, 
                     const IRemoteQueryData::TSeqLocs& seqlocs)
    : m_QueryFactory(query_factory)
{
    BOOST_REQUIRE(!seqlocs.empty());
    x_Init(&seqlocs);
}

void
CSequenceDataTester::x_Init(const IRemoteQueryData::TSeqLocs* seqlocs)
{
    // based on the first sequence, determine if all are nucleotide or protein
    CBioseq_Handle bh = CSimpleOM::GetBioseqHandle(m_Gis.at(0));
    bool is_protein_sequence = false;
    CBioseq_Handle::TBioseqCore bioseq;
    try {
        bioseq = bh.GetBioseqCore();
        is_protein_sequence = bioseq->GetInst().IsAa();
    } catch (...) {
        // deliberately ignore exception, as thi would occur in the case of an
        // invalid gi
    }

    blast::EProgram prog = is_protein_sequence
        ? blast::eBlastp
        : blast::eBlastn;
    // FIXME: this should allow for translated query searches also!
    CRef<CBlastOptionsHandle> oh((CBlastOptionsFactory::Create(prog)));
    m_Options.Reset(&oh->GetOptions());

    m_Scope.Reset(CSimpleOM::NewScope());

    if (seqlocs) {
        // No gis were provided, build gi list
        m_Gis.reserve(seqlocs->size());
        m_Strands.reserve(seqlocs->size());

        size_t index = 0;
        ITERATE(IRemoteQueryData::TSeqLocs, itr, *seqlocs) {
            // Get the gi
            const CSeq_id& id = sequence::GetId(**itr, &*m_Scope);
            m_Gis[index] = sequence::GetGiForAccession(id.AsFastaString(),
                                                       *m_Scope);
            BOOST_REQUIRE(m_Gis[index] != 0);

            // Get the strand
            m_Strands[index] = (*itr)->GetStrand();
            index++;
        }
        BOOST_REQUIRE(index == seqlocs->size());

    } else {
        // No IRemoteQueryData::TSeqLocs were provided, build strand list

        ENa_strand s(eNa_strand_unknown);
        m_Strands.resize(m_Gis.size(), s);

        if (x_IsProtein()) {
            s = eNa_strand_unknown;
        } else if ( (s = m_Options->GetStrandOption()) == eNa_strand_unknown) {
            s = eNa_strand_both;
        }
        fill(m_Strands.begin(), m_Strands.end(), s);
    }
}

bool
CSequenceDataTester::x_IsProtein()
{
    if ( !m_Options ) {
        x_Init();
    }
    return Blast_QueryIsProtein(m_Options->GetProgramType());
}

inline ENa_strand
CSequenceDataTester::x_GetStrand(int index)
{
    return m_Strands[index];
}

void
CSequenceDataTester::operator () (void)
{
    for (size_t i = 0; i < m_Gis.size(); i++) {
        x_TestSingleSequence_Local(i);
        x_TestSingleSequence_Remote(i);
    }
}

void
CSequenceDataTester::x_TestSingleNucleotide_Local(int index,
                                               const BlastQueryInfo* qinfo,
                                               const BLAST_SequenceBlk* seqblk)
{
    const CSeq_id id(CSeq_id::e_Gi, m_Gis[index]);

    int ctx_index = index * 2;     // index into BlastQueryInfo::contexts
    switch (x_GetStrand(index)) {
    case eNa_strand_plus:
        x_TestLocalStrand(id, ctx_index, x_GetStrand(index), qinfo, seqblk);
        break;
    case eNa_strand_minus:
        x_TestLocalStrand(id, ctx_index + 1, x_GetStrand(index), qinfo, seqblk);
        break;
    case eNa_strand_both:
        x_TestLocalStrand(id, ctx_index, eNa_strand_plus, qinfo, seqblk);
        x_TestLocalStrand(id, ctx_index + 1, eNa_strand_minus, qinfo, seqblk);
        break;
    default:
        throw runtime_error("Internal error in " +
                            string(NCBI_CURRENT_FUNCTION));
    }
}

void
CSequenceDataTester::x_TestLocalStrand(const CSeq_id& id,
                                       int ctx_index,
                                       ENa_strand strand,
                                       const BlastQueryInfo* qinfo,
                                       const BLAST_SequenceBlk* seqblk)
{
    BOOST_REQUIRE(strand == eNa_strand_plus || strand == eNa_strand_minus);
    BOOST_REQUIRE(qinfo->contexts[ctx_index].query_length != 0);

    // Test the sequence length
    const Int4 kLength = sequence::GetLength(id, &*m_Scope);
    BOOST_REQUIRE_EQUAL(kLength, qinfo->contexts[ctx_index].query_length);

    // Test the actual sequence data
    CSeqVector sv = CSimpleOM::GetSeqVector(id, strand);
    Uint1* sequence = seqblk->sequence +
        qinfo->contexts[ctx_index].query_offset;
    x_CompareSequenceData(sv, sequence, 
                          strand == eNa_strand_plus ? "plus" : "minus");
}

void
CSequenceDataTester::x_TestSingleTranslatedNucl_Local(int index,
                                               const BlastQueryInfo* qinfo,
                                               const BLAST_SequenceBlk* seqblk)
{
    string msg("CSequenceDataTester::x_TestSingleTranslatedNucl_Local ");
    msg += "not implemented";
    throw runtime_error(msg);
}

void
CSequenceDataTester::x_CompareSequenceData(CSeqVector& sv,
                                           const Uint1* sequence,
                                           const string& strand)
{
    if (x_IsProtein()) {
        BOOST_REQUIRE(sv.IsProtein());
        sv.SetCoding(CSeq_data::e_Ncbistdaa);
    } else {
        BOOST_REQUIRE(sv.IsNucleotide());
        sv.SetCoding(CSeq_data::e_Ncbi4na);
    }

    string msg("Different ");
    msg += x_IsProtein() ? "residues" : "bases";
    msg += " at position ";

    for (TSeqPos i = 0; i < sv.size(); i++) {

        const Uint1 kBase = x_IsProtein() ? sv[i] : NCBI4NA_TO_BLASTNA[sv[i]];
        msg += NStr::IntToString(i);
        if ( !x_IsProtein() && !strand.empty() ) {
            msg += " (" + strand + " strand)";
        }

        BOOST_REQUIRE_MESSAGE(static_cast<int>(sequence[i]) == static_cast<int>(kBase),
                              msg);
    }
}

void
CSequenceDataTester::x_TestSingleProtein_Local(int index,
                                               const BlastQueryInfo* qinfo,
                                               const BLAST_SequenceBlk* seqblk)
{
    BOOST_REQUIRE(index >= qinfo->first_context);
    BOOST_REQUIRE(index <= qinfo->last_context);

    const CSeq_id id(CSeq_id::e_Gi, m_Gis[index]);

    // Test the sequence length
    const Int4 kLength = sequence::GetLength(id, &*m_Scope);
    BOOST_REQUIRE_EQUAL(kLength, qinfo->contexts[index].query_length);

    // Test the actual sequence data
    Uint1* sequence = seqblk->sequence + qinfo->contexts[index].query_offset;
    CSeqVector sv = CSimpleOM::GetSeqVector(id);
    x_CompareSequenceData(sv, sequence);
}

void
CSequenceDataTester::x_TestSingleSequence_Local(int index)
{
    CRef<ILocalQueryData>
        queries(m_QueryFactory->MakeLocalQueryData(m_Options));
    BOOST_REQUIRE(queries.NotEmpty());

    const BlastQueryInfo* qinfo = queries->GetQueryInfo();
    const BLAST_SequenceBlk* seq_blk = queries->GetSequenceBlk();
    BOOST_REQUIRE(qinfo != NULL);
    BOOST_REQUIRE(seq_blk != NULL);

    TQueryMessages msgs;
    queries->GetQueryMessages(index, msgs);
    if ( !msgs.empty() ) {
        string message;
        ITERATE(TQueryMessages, m, msgs) {
            message += (*m)->GetMessage();
        }
        NCBI_THROW(CBlastException, eCoreBlastError, message);
    }

    BOOST_REQUIRE_EQUAL(m_Gis.size(),
                         static_cast<size_t>(qinfo->num_queries));

    if (x_IsProtein()) {
        x_TestSingleProtein_Local(index, qinfo, seq_blk);
    } else {
        x_TestSingleNucleotide_Local(index, qinfo, seq_blk);
    }

    // Make sure we get the same pointer to local queries from the factory
    BOOST_REQUIRE_EQUAL(queries.GetNonNullPointer(),
             m_QueryFactory->MakeLocalQueryData(m_Options).GetNonNullPointer());
}

void
CSequenceDataTester::x_TestSingleSequence_Remote(int index)
{
    CRef<IRemoteQueryData> queries(m_QueryFactory->MakeRemoteQueryData());
    BOOST_REQUIRE(queries.NotEmpty());

    // Test the seqlocs
    IRemoteQueryData::TSeqLocs seqlocs = queries->GetSeqLocs();
    IRemoteQueryData::TSeqLocs::const_iterator itr = seqlocs.begin();
    BOOST_REQUIRE_EQUAL(m_Gis.size(), seqlocs.size());
    BOOST_REQUIRE(index >= 0);
    BOOST_REQUIRE(index < static_cast<int>(seqlocs.size()));
    for (int i = 0; itr != seqlocs.end(); ++itr, ++i) {
        if (i == index) break;
    }
    BOOST_REQUIRE(itr != seqlocs.end());

#if 0
    // Currently disabled because the seqid string doesn't necessarily contain
    // the gi...
    // Test the gi being present in the Seq-id string
    const CSeq_id* seqid = (*itr)->GetId();
    BOOST_REQUIRE(seqid != NULL);
    string gi_string(NStr::IntToString(m_Gis[index]));
    string seqid_string(seqid->AsFastaString());
    cout << seqid_string << endl;
    BOOST_REQUIRE_MESSAGE(seqid_string.find(gi_string) != ncbi::NPOS,
                          "Cannot find gi in Seq-id string for remote query "
                          "data");
#endif

    // Test the Bioseq_set
    CRef<CBioseq_set> bioseq_set = queries->GetBioseqSet();
    CTypeConstIterator<CBioseq> bioseq(ConstBegin(*bioseq_set));
    TSeqPos seq_index = 0;
    for (; bioseq; ++bioseq) {
        if (seq_index != static_cast<TSeqPos>(index)) {
            seq_index++;
        } else {
            break;
        }
    }
    BOOST_REQUIRE(seq_index < m_Gis.size());

    const CSeq_id id(CSeq_id::e_Gi, m_Gis[index]);
    const CBioseq::TInst& seq_inst = bioseq->GetInst();
    BOOST_REQUIRE_EQUAL(CSeq_inst::eRepr_raw, seq_inst.GetRepr());
    BOOST_REQUIRE_EQUAL(x_IsProtein(), seq_inst.IsAa());
    BOOST_REQUIRE_EQUAL(sequence::GetLength(id, &*m_Scope),
                                 seq_inst.GetLength());

    if (x_IsProtein()) {
        x_TestSingleProtein_Remote(id, seq_inst);
    } else {
        x_TestSingleNucleotide_Remote(id, seq_inst);
    }

    // Make sure we get the same pointer to local queries from the factory
    BOOST_REQUIRE_EQUAL(queries.GetNonNullPointer(),
             m_QueryFactory->MakeRemoteQueryData().GetNonNullPointer());
}

void
CSequenceDataTester::x_TestSingleProtein_Remote(const CSeq_id& id,
                                                const CSeq_inst& seq_inst)
{
    CSeq_inst::TSeq_data seq_data;
    TSeqPos nconv = CSeqportUtil::Convert(seq_inst.GetSeq_data(),
                                          &seq_data, CSeq_data::e_Ncbistdaa);
    BOOST_REQUIRE(seq_data.IsNcbistdaa());

    CSeqVector sv = CSimpleOM::GetSeqVector(id);
    BOOST_REQUIRE_EQUAL(sv.size(), nconv);
    BOOST_REQUIRE(sv.IsProtein() == seq_inst.IsAa());
    sv.SetCoding(CSeq_data::e_Ncbistdaa);

    for (TSeqPos i = 0; i < sv.size(); i++) {
        const char kResidue = sv[i];
        BOOST_REQUIRE_MESSAGE(kResidue == seq_data.GetNcbistdaa().Get()[i],
                              "Different residues at position " + NStr::IntToString(i));
    }
}

void
CSequenceDataTester::x_TestSingleNucleotide_Remote(const CSeq_id& id,
                                                   const CSeq_inst& seq_inst)
{
    CSeq_inst::TSeq_data seq_data;

    // N.B.: data returned in seq_data is compressed 2 bases per byte
    TSeqPos nconv = CSeqportUtil::Convert(seq_inst.GetSeq_data(),
                                          &seq_data, CSeq_data::e_Ncbi4na);
    BOOST_REQUIRE(seq_data.IsNcbi4na());

    CSeqVector sv = CSimpleOM::GetSeqVector(id);
    BOOST_REQUIRE_EQUAL(sv.size(), nconv);
    BOOST_REQUIRE(sv.IsProtein() == seq_inst.IsAa());
    sv.SetCoding(CSeq_data::e_Ncbi4na);

    for (TSeqPos i = 0; i < sv.size(); i++) {
        const char kBase = sv[i];
        const char kCompressedBase = seq_data.GetNcbi4na().Get()[(int)i/2];
        char BaseTest;

        if ((i%2) == 0) {
            // get the high 4 bits
            BaseTest = (kCompressedBase & 0xF0) >> 4;
        } else {
            // get the low 4 bits
            BaseTest = kCompressedBase & 0x0F;
        }

        BOOST_REQUIRE_MESSAGE(static_cast<int>(kBase) == static_cast<int>(BaseTest),
                              "Different bases at position " + NStr::IntToString(i));
    }
}

// ========================================================================= //
// Unit Tests

struct CQueryDataTestFixture
{
    CQueryDataTestFixture() {}
    ~CQueryDataTestFixture() {}

    static void
        s_ObjMgrFree_QueryFactory_LocalDataFromBioseq(int kGi)
    {
        CBioseq_Handle bh = CSimpleOM::GetBioseqHandle(kGi);
        CConstRef<CBioseq> bs(bh.GetBioseqCore());
        CRef<IQueryFactory> query_factory(new CObjMgrFree_QueryFactory(bs));/* NCBI_FAKE_WARNING */
        CSequenceDataTester(query_factory, kGi)();
    }

    static void
        s_ObjMgrFree_QueryFactory_LocalDataFromBioseq_set(const vector<int>& gis)
    {
        CRef<CBioseq_set> bs(new CBioseq_set);
        ITERATE(vector<int>, itr, gis) {
            CBioseq_Handle bh = CSimpleOM::GetBioseqHandle(*itr);
            CRef<CSeq_entry> seq_entry(new CSeq_entry);
            seq_entry->SetSeq(const_cast<CBioseq&>(*bh.GetBioseqCore()));
            bs->SetSeq_set().push_back(seq_entry);
        }
        CRef<IQueryFactory> query_factory(new CObjMgrFree_QueryFactory(bs));/* NCBI_FAKE_WARNING */

        CSequenceDataTester(query_factory, gis)();
    }
    
    static void
        s_ObjMgr_QueryFactory_LocalDataFromTSeqLocVector(const vector<int>& gis)
    {
        TSeqLocVector queries;
        ITERATE(vector<int>, itr, gis) {
            CSeq_id qid(CSeq_id::e_Gi, *itr);
            auto_ptr<SSeqLoc> sl(CTestObjMgr::Instance().CreateSSeqLoc(qid));
            queries.push_back(*sl);
        }

        CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(queries));

        CSequenceDataTester(query_factory, gis)();
    }

    static void
        s_ObjMgr_QueryFactory_LocalDataFromBlastQueryVector(const vector<int>& gis)
    {
        CRef<CBlastQueryVector> queries(new CBlastQueryVector);
        
        ITERATE(vector<int>, itr, gis) {
            CSeq_id qid(CSeq_id::e_Gi, *itr);
            
            CRef<CBlastSearchQuery>
                sq(CTestObjMgr::Instance().CreateBlastSearchQuery(qid));
            
            queries->AddQuery(sq);
        }
        
        CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(*queries));

        CSequenceDataTester(query_factory, gis)();
    }

    static void create_EmptyTSeqLocVector()
    {
        TSeqLocVector queries;
        CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(queries));
    }

    static void create_EmptyBlastQueryVector()
    {
        CRef<CBlastQueryVector> queries(new CBlastQueryVector);
        CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(*queries));
    }
   
};

BOOST_FIXTURE_TEST_SUITE(QueryData, CQueryDataTestFixture)

//
// Object-manager dependant test cases
//

BOOST_AUTO_TEST_CASE(ObjMgr_QueryFactory_LocalData_GetSumOfSequenceLengths) {
    vector<int> gis;
    gis.push_back(26);
    gis.push_back(555);
    gis.push_back(556);
    CRef<CBioseq_set> bs(new CBioseq_set);
    ITERATE(vector<int>, itr, gis) {
        CBioseq_Handle bh = CSimpleOM::GetBioseqHandle(*itr);
        CRef<CSeq_entry> seq_entry(new CSeq_entry);
        seq_entry->SetSeq(const_cast<CBioseq&>(*bh.GetBioseqCore()));
        bs->SetSeq_set().push_back(seq_entry);
    }
    CRef<IQueryFactory> query_factory(new CObjMgrFree_QueryFactory(bs)); /* NCBI_FAKE_WARNING */
    CRef<CBlastOptionsHandle> oh((CBlastOptionsFactory::Create(eBlastn)));

    size_t kExpectedSize = 416+624+310;
    CRef<ILocalQueryData> query_data = 
        query_factory->MakeLocalQueryData(&oh->GetOptions());
    BOOST_REQUIRE_EQUAL(kExpectedSize,
                            query_data->GetSumOfSequenceLengths());

    // try a chromosome :)
    CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Gi, 89161185));
    CRef<CBlastQueryVector> q(new CBlastQueryVector);
    q->AddQuery(CTestObjMgr::Instance().CreateBlastSearchQuery(*id));
    query_factory.Reset(new CObjMgr_QueryFactory(*q));
    kExpectedSize = 247249719;
    query_data.Reset(query_factory->MakeLocalQueryData(&oh->GetOptions()));
    BOOST_REQUIRE_EQUAL(kExpectedSize,
                            query_data->GetSumOfSequenceLengths());
}

BOOST_AUTO_TEST_CASE(ObjMgr_QueryFactory_LocalDataFromTSeqLocVector_Protein) {
    vector<int> gis;
    gis.push_back(38092615);
    gis.push_back(4506509);
    s_ObjMgr_QueryFactory_LocalDataFromTSeqLocVector(gis);
}

BOOST_AUTO_TEST_CASE(ObjMgr_QueryFactory_LocalDataFromTSeqLocVector_Nucleotide) {
    vector<int> gis;
    gis.push_back(555);
    gis.push_back(556);
    gis.push_back(26);
    s_ObjMgr_QueryFactory_LocalDataFromTSeqLocVector(gis);
}

BOOST_AUTO_TEST_CASE(ObjMgr_QueryFactory_LocalDataFromBlastQueryVector_Protein) {
    vector<int> gis;
    gis.push_back(38092615);
    gis.push_back(4506509);
    s_ObjMgr_QueryFactory_LocalDataFromBlastQueryVector(gis);
}

BOOST_AUTO_TEST_CASE(ObjMgr_QueryFactory_LocalDataFromBlastQueryVector_Nucleotide) {
    vector<int> gis;
    gis.push_back(555);
    gis.push_back(556);
    gis.push_back(26);
    s_ObjMgr_QueryFactory_LocalDataFromBlastQueryVector(gis);
}

BOOST_AUTO_TEST_CASE(ObjMgr_QueryFactory_RemoteData_SingleBioseqFromTSeqLocVector) {
    const int kGi = 129295;
    TSeqLocVector queries;
    CSeq_id qid(CSeq_id::e_Gi, kGi);
    auto_ptr<SSeqLoc> sl(CTestObjMgr::Instance().CreateSSeqLoc(qid));
    queries.push_back(*sl);
    CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(queries));
    CSequenceDataTester(query_factory, kGi)();
}

BOOST_AUTO_TEST_CASE(ObjMgr_QueryFactory_RemoteData_SingleBioseqFromBlastQueryVector) {
    const int kGi = 129295;

    CRef<CBlastQueryVector> queries(new CBlastQueryVector);
    CSeq_id qid(CSeq_id::e_Gi, kGi);
    
    CRef<CBlastSearchQuery>
        sq(CTestObjMgr::Instance().CreateBlastSearchQuery(qid));
    
    queries->AddQuery(sq);
    
    CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(*queries));
    CSequenceDataTester(query_factory, kGi)();
}

BOOST_AUTO_TEST_CASE(ObjMgr_QueryFactory_EmptyTSeqLocVector) {
    BOOST_REQUIRE_THROW(create_EmptyTSeqLocVector(), CBlastException);
}

BOOST_AUTO_TEST_CASE(ObjMgr_QueryFactory_EmptyBlastQueryVector) {
    BOOST_REQUIRE_THROW(create_EmptyBlastQueryVector(), CBlastException);
}

//
// Object-manager independant test cases
//

BOOST_AUTO_TEST_CASE(ObjMgrFree_QueryFactory_LocalDataFromBioseq_Protein) {
    const int kGi = 129295;
    s_ObjMgrFree_QueryFactory_LocalDataFromBioseq(kGi);
}

BOOST_AUTO_TEST_CASE(ObjMgrFree_QueryFactory_LocalDataFromBioseq_Nucleotide) {
    const int kGi = 555;
    s_ObjMgrFree_QueryFactory_LocalDataFromBioseq(kGi);
}

BOOST_AUTO_TEST_CASE(ObjMgrFree_QueryFactory_LocalDataFromBioseq_set_Protein) {
    vector<int> gis;
    gis.push_back(129295);
    gis.push_back(87);
    gis.push_back(1900);
    s_ObjMgrFree_QueryFactory_LocalDataFromBioseq_set(gis);
}

BOOST_AUTO_TEST_CASE(ObjMgrFree_QueryFactory_LocalDataFromBioseq_set_Nucleotide) {
    vector<int> gis;
    gis.push_back(26);
    gis.push_back(555);
    gis.push_back(556);
    s_ObjMgrFree_QueryFactory_LocalDataFromBioseq_set(gis);
}

BOOST_AUTO_TEST_SUITE_END()
