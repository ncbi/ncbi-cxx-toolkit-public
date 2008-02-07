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
 * Authors:  Jason Papadopoulos
 *
 * File Description:
 *   Unit tests for CBlastInput, CBlastInputSource and derived classes
 *
 */

#include <ncbi_pch.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Packed_seqint.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/PDB_seq_id.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/NCBIeaa.hpp>

#include <corelib/ncbienv.hpp>
#include <objtools/readers/reader_exception.hpp>
#include <algo/blast/api/sseqloc.hpp>
#include <algo/blast/blastinput/blast_input.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_vector.hpp>

#include <algo/blast/blastinput/blastp_args.hpp>
#include <algo/blast/blastinput/blastn_args.hpp>
#include <algo/blast/blastinput/blastx_args.hpp>
#include <algo/blast/blastinput/tblastn_args.hpp>
#include <algo/blast/blastinput/tblastx_args.hpp>
#include <algo/blast/blastinput/psiblast_args.hpp>

// Keep Boost's inclusion of <limits> from breaking under old WorkShop versions.
#if defined(numeric_limits)  &&  defined(NCBI_NUMERIC_LIMITS)
#  undef numeric_limits
#endif

#define BOOST_AUTO_TEST_MAIN
#include <boost/test/auto_unit_test.hpp>
#ifndef BOOST_PARAM_TEST_CASE
#  include <boost/test/parameterized_test.hpp>
#endif
#include <boost/current_function.hpp>
#ifndef BOOST_AUTO_TEST_CASE
#  define BOOST_AUTO_TEST_CASE BOOST_AUTO_UNIT_TEST
#endif

#include <common/test_assert.h>  /* This header must go last */

#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
using boost::unit_test::test_suite;

// Use macros rather than inline functions to get accurate line number reports

#define CHECK_NO_THROW(statement)                                       \
    try {                                                               \
        statement;                                                      \
        BOOST_CHECK_MESSAGE(true, "no exceptions were thrown by "#statement); \
    } catch (std::exception& e) {                                       \
        BOOST_ERROR("an exception was thrown by "#statement": " << e.what()); \
    } catch (...) {                                                     \
        BOOST_ERROR("a nonstandard exception was thrown by "#statement); \
    }

#define CHECK(expr)       CHECK_NO_THROW(BOOST_CHECK(expr))
#define CHECK_EQUAL(x, y) CHECK_NO_THROW(BOOST_CHECK_EQUAL(x, y))

class CAutoDiagnosticsRedirector
{
public:
    CAutoDiagnosticsRedirector(EDiagSev severity = eDiag_Warning) {
        SetDiagPostLevel(severity);
        m_DiagStream = GetDiagStream();
    }

    void Redirect(CNcbiOstrstream& redirect_stream) {
        SetDiagStream(&redirect_stream);
    }

    ~CAutoDiagnosticsRedirector() {
        // Restore diagnostics stream
        SetDiagStream(m_DiagStream);
    }

private:
    CNcbiOstream* m_DiagStream;
};

static CRef<CBlastInput>
s_DeclareBlastInput(CNcbiIstream& input_file, 
                    const CBlastInputSourceConfig& iconfig,
                    int batch_size = kMax_Int)
{
    CRef<CBlastFastaInputSource> fasta_src
        (new CBlastFastaInputSource(input_file, iconfig));
    return CRef<CBlastInput>(new CBlastInput(&*fasta_src, batch_size));
}

static CRef<CBlastInput>
s_DeclareBlastInput(const string& user_input, 
                    const CBlastInputSourceConfig& iconfig)
{
    CRef<CBlastFastaInputSource> fasta_src
        (new CBlastFastaInputSource(user_input, iconfig));
    return CRef<CBlastInput>(new CBlastInput(&*fasta_src));
}

BOOST_AUTO_TEST_CASE(ReadAccession_MismatchNuclProt)
{
    CNcbiIfstream infile("data/nucl_acc.txt");
    const bool is_protein(true);
    CBlastInputSourceConfig iconfig(is_protein);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CScope scope(*CObjectManager::GetInstance());

    CHECK(source->End() == false);
    bool caught_exception(false);
    try { blast::SSeqLoc ssl = source->GetNextSeqLocBatch(scope).front(); }
    catch (const CInputException& e) {
        string msg(e.what());
        BOOST_CHECK(msg.find("Gi/accession mismatch: requested protein, found nucleotide")
                    != NPOS);
        BOOST_CHECK_EQUAL(CInputException::eSequenceMismatch, e.GetErrCode());
        caught_exception = true;
    }
    CHECK(caught_exception);
    CHECK(source->End() == true);
}

BOOST_AUTO_TEST_CASE(ReadAccession_MismatchProtNucl)
{
    CNcbiIfstream infile("data/prot_acc.txt");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CScope scope(*CObjectManager::GetInstance());

    CHECK(source->End() == false);
    bool caught_exception(false);
    try { blast::SSeqLoc ssl = source->GetNextSeqLocBatch(scope).front(); }
    catch (const CInputException& e) {
        string msg(e.what());
        BOOST_CHECK(msg.find("Gi/accession mismatch: requested nucleotide, found protein")
                    != NPOS);
        BOOST_CHECK_EQUAL(CInputException::eSequenceMismatch, e.GetErrCode());
        caught_exception = true;
    }
    CHECK(caught_exception);
    CHECK(source->End() == true);
}

BOOST_AUTO_TEST_CASE(ReadGi_MismatchNuclProt)
{
    CNcbiIfstream infile("data/gi.txt");
    const bool is_protein(true);
    CBlastInputSourceConfig iconfig(is_protein);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CScope scope(*CObjectManager::GetInstance());

    CHECK(source->End() == false);
    bool caught_exception(false);
    try { blast::SSeqLoc ssl = source->GetNextSeqLocBatch(scope).front(); }
    catch (const CInputException& e) {
        string msg(e.what());
        BOOST_CHECK(msg.find("Gi/accession mismatch: requested protein, found nucleotide")
                    != NPOS);
        BOOST_CHECK_EQUAL(CInputException::eSequenceMismatch, e.GetErrCode());
        caught_exception = true;
    }
    CHECK(caught_exception);
    CHECK(source->End() == true);
}

BOOST_AUTO_TEST_CASE(ReadGi_MismatchProtNucl)
{
    CNcbiIfstream infile("data/prot_gi.txt");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CScope scope(*CObjectManager::GetInstance());

    CHECK(source->End() == false);
    bool caught_exception(false);
    try { blast::SSeqLoc ssl = source->GetNextSeqLocBatch(scope).front(); }
    catch (const CInputException& e) {
        string msg(e.what());
        BOOST_CHECK(msg.find("Gi/accession mismatch: requested nucleotide, found protein")
                    != NPOS);
        BOOST_CHECK_EQUAL(CInputException::eSequenceMismatch, e.GetErrCode());
        caught_exception = true;
    }
    CHECK(caught_exception);
    CHECK(source->End() == true);
}

struct SDubiousShortSequence
{
    string sequence_data;
    CSeq_inst::EMol mol_type;

    SDubiousShortSequence(const string& seq, 
                          CSeq_inst::EMol mol_type)
        : sequence_data(seq), mol_type(mol_type)
    {
        seqlen = NStr::Replace(sequence_data, " ", kEmptyStr).length();
    }

    bool IsProtein() const { return CSeq_inst::IsAa(mol_type); }
    TSeqPos GetLength() const { return seqlen; }

private:
    TSeqPos seqlen;
};

BOOST_AUTO_TEST_CASE(TestSmallDubiousSequences)
{
    string seq;

    vector<SDubiousShortSequence> test_data;
    test_data.push_back(SDubiousShortSequence("NNWNN", CSeq_inst::eMol_aa));
    // P84064
    seq.assign("ykrggggwgg gggwkggggg gggwkggggg gkgggg");
    test_data.push_back(SDubiousShortSequence(seq, CSeq_inst::eMol_aa));
    // AAB32668
    seq.assign("GGGGGGGGGGGGGGG");
    test_data.push_back(SDubiousShortSequence(seq, CSeq_inst::eMol_aa));

    CRef<CObjectManager> om(CObjectManager::GetInstance());

    // First test the usage of the sequence length threshold
    ITERATE(vector<SDubiousShortSequence>, itr, test_data) {
        CBlastInputSourceConfig iconfig(itr->IsProtein());
        iconfig.SetSeqLenThreshold2Guess(itr->GetLength() + 1);

        CRef<CBlastFastaInputSource> fasta_source
            (new CBlastFastaInputSource(itr->sequence_data, iconfig));
        CRef<CBlastInput> source(new CBlastInput(&*fasta_source));

        CScope scope(*om);
        CHECK(source->End() == false);
        bool caught_exception(false);
        blast::SSeqLoc ssl;
        try { ssl = source->GetNextSeqLocBatch(scope).front(); }
        catch (const CInputException& e) {
            string msg(e.what());
            BOOST_CHECK(msg.find("Gi/accession mismatch: ") != NPOS);
            BOOST_CHECK_EQUAL(CInputException::eSequenceMismatch, 
                              e.GetErrCode());
            caught_exception = true;
        }
        BOOST_CHECK(caught_exception == false);
        BOOST_CHECK(source->End() == true);

        TSeqPos length = sequence::GetLength(*ssl.seqloc, ssl.scope);
        BOOST_CHECK_EQUAL(itr->GetLength(), length);
    }

    // Now check that these sequences will be rejected as being the wrong
    // molecule type (achieved by setting seqlen_thresh2guess argument to
    // CBlastFastaInputSource to a small value
    ITERATE(vector<SDubiousShortSequence>, itr, test_data) {

        CBlastInputSourceConfig iconfig(itr->IsProtein());
        iconfig.SetSeqLenThreshold2Guess(5);

        CRef<CBlastFastaInputSource> fasta_source
            (new CBlastFastaInputSource(itr->sequence_data, iconfig));
        CRef<CBlastInput> source(new CBlastInput(&*fasta_source));

        CScope scope(*om);
        CHECK(source->End() == false);
        bool caught_exception(false);
        blast::SSeqLoc ssl;
        try { ssl = source->GetNextSeqLocBatch(scope).front(); }
        catch (const CInputException& e) {
            string msg(e.what());
            BOOST_CHECK(msg.find("Nucleotide FASTA provided for prot") != NPOS);
            BOOST_CHECK_EQUAL(CInputException::eSequenceMismatch, 
                              e.GetErrCode());
            caught_exception = true;
        }
        BOOST_CHECK(caught_exception == true);
        BOOST_CHECK(source->End() == true);
    }
}

BOOST_AUTO_TEST_CASE(ReadFastaWithDefline_MismatchProtNucl)
{
    CNcbiIfstream infile("data/aa.129295");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetSeqLenThreshold2Guess(25);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    CScope scope(*CObjectManager::GetInstance());
    CHECK(source->End() == false);
    bool caught_exception(false);
    try { blast::SSeqLoc ssl = source->GetNextSeqLocBatch(scope).front(); }
    catch (const CInputException& e) {
        string msg(e.what());
        BOOST_CHECK(msg.find("Protein FASTA provided for nucleotide") != NPOS);
        BOOST_CHECK_EQUAL(CInputException::eSequenceMismatch, e.GetErrCode());
        caught_exception = true;
    }
    CHECK(caught_exception);
    CHECK(source->End() == true);
}

BOOST_AUTO_TEST_CASE(ReadFastaWithDefline_MismatchNuclProt)
{
    CNcbiIfstream infile("data/nt.555");
    const bool is_protein(true);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetSeqLenThreshold2Guess(25);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    CScope scope(*CObjectManager::GetInstance());
    CHECK(source->End() == false);
    bool caught_exception(false);
    try { blast::SSeqLoc ssl = source->GetNextSeqLocBatch(scope).front(); }
    catch (const CInputException& e) {
        string msg(e.what());
        BOOST_CHECK(msg.find("Nucleotide FASTA provided for protein") != NPOS);
        BOOST_CHECK_EQUAL(CInputException::eSequenceMismatch, e.GetErrCode());
        caught_exception = true;
    }
    CHECK(caught_exception);
    CHECK(source->End() == true);
}

BOOST_AUTO_TEST_CASE(ReadFastaWithDeflineProtein_Single)
{
    CNcbiIfstream infile("data/aa.129295");
    const bool is_protein(true);
    CBlastInputSourceConfig iconfig(is_protein);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    CScope scope(*CObjectManager::GetInstance());
    CHECK(source->End() == false);
    blast::SSeqLoc ssl = source->GetNextSeqLocBatch(scope).front();
    CHECK(source->End() == true);

    CHECK(ssl.seqloc->IsInt() == true);

    CHECK(ssl.seqloc->GetInt().IsSetStrand() == true);
    CHECK_EQUAL(eNa_strand_unknown, ssl.seqloc->GetInt().GetStrand());

    CHECK(ssl.seqloc->GetInt().IsSetFrom() == true);
    CHECK_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    CHECK(ssl.seqloc->GetInt().IsSetTo() == true);
    const TSeqPos length(232);
    CHECK_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

    CHECK(ssl.seqloc->GetInt().IsSetId() == true);
    CHECK_EQUAL(CSeq_id::e_Local, ssl.seqloc->GetInt().GetId().Which());

    CHECK(!ssl.mask);
}

BOOST_AUTO_TEST_CASE(RawFastaWithSpaces)
{
    // this is gi 555, length 624
    CNcbiIfstream infile("data/raw_fasta.na");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    CScope scope(*CObjectManager::GetInstance());
    CHECK(source->End() == false);
    blast::SSeqLoc ssl = source->GetNextSeqLocBatch(scope).front();
    CHECK(source->End() == true);

    CHECK(ssl.seqloc->IsInt() == true);

    CHECK(ssl.seqloc->GetInt().IsSetStrand() == true);
    CHECK_EQUAL(eNa_strand_both, ssl.seqloc->GetInt().GetStrand());

    CHECK(ssl.seqloc->GetInt().IsSetFrom() == true);
    CHECK_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    CHECK(ssl.seqloc->GetInt().IsSetTo() == true);
    const TSeqPos length(624);
    CHECK_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

    CHECK(ssl.seqloc->GetInt().IsSetId() == true);
    CHECK_EQUAL(CSeq_id::e_Local, ssl.seqloc->GetInt().GetId().Which());

    CHECK(!ssl.mask);
}

BOOST_AUTO_TEST_CASE(ReadProteinWithGaps)
{
    CNcbiIfstream infile("data/prot_w_gaps.txt");
    const bool is_protein(true);
    CBlastInputSourceConfig iconfig(is_protein);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    CScope scope(*CObjectManager::GetInstance());
    TSeqLocVector seqs = source->GetAllSeqLocs(scope);
    blast::SSeqLoc ssl = seqs.front();
    CHECK(source->End() == true);

    CHECK(ssl.seqloc->IsInt() == true);

    CHECK(ssl.seqloc->GetInt().IsSetFrom() == true);
    CHECK_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    CHECK(ssl.seqloc->GetInt().IsSetTo() == true);
    const TSeqPos length(91); // it's actually 103 with gaps
    CHECK_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

    CBioseq_Handle bh = scope.GetBioseqHandle(*ssl.seqloc);
    CSeqVector sv = bh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);

    for (size_t i = 0; i < sv.size(); i++) {
        BOOST_CHECK((char)sv[i] != '-');
    }

    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
    const CBioseq& bioseq = bioseqs->GetSeq_set().front()->GetSeq();
    const CSeq_inst& inst = bioseq.GetInst();
    BOOST_CHECK_EQUAL(inst.GetLength(), length);
    BOOST_CHECK(inst.IsSetSeq_data());
    const CSeq_data& seq_data = inst.GetSeq_data();
    BOOST_CHECK(seq_data.IsNcbieaa());
    const string& seq = seq_data.GetNcbieaa().Get();
    for (size_t i = 0; i < seq.size(); i++) {
        BOOST_CHECK((char)seq[i] != '-');
    }
}

BOOST_AUTO_TEST_CASE(RawFastaNoSpaces)
{
    // this is gi 555, length 624
    CNcbiIfstream infile("data/raw_fasta2.na");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    CScope scope(*CObjectManager::GetInstance());
    TSeqLocVector seqs = source->GetAllSeqLocs(scope);
    blast::SSeqLoc ssl = seqs[0];
    CHECK(source->End() == true);

    CHECK(ssl.seqloc->IsInt() == true);

    CHECK(ssl.seqloc->GetInt().IsSetStrand() == true);
    CHECK_EQUAL(eNa_strand_both, ssl.seqloc->GetInt().GetStrand());

    CHECK(ssl.seqloc->GetInt().IsSetFrom() == true);
    CHECK_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    CHECK(ssl.seqloc->GetInt().IsSetTo() == true);
    const TSeqPos length(624);
    CHECK_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

    CHECK(ssl.seqloc->GetInt().IsSetId() == true);
    CHECK_EQUAL(CSeq_id::e_Local, ssl.seqloc->GetInt().GetId().Which());

    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
    CHECK(bioseqs.NotEmpty());

    CHECK(!ssl.mask);
}

BOOST_AUTO_TEST_CASE(RawFastaNoSpaces_UpperCaseWithN)
{
    // this has length 682 and contains an 'N' which without the
    // CFastaReader::fNoSplit flag, produces a delta sequence
    CNcbiIfstream infile("data/nucl_w_n.fsa");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    CScope scope(*CObjectManager::GetInstance());
    CHECK(source->End() == false);
    TSeqLocVector seqs = source->GetAllSeqLocs(scope);
    blast::SSeqLoc ssl = seqs.front();
    CHECK(source->End() == true);

    CHECK(ssl.seqloc->IsInt() == true);

    CHECK(ssl.seqloc->GetInt().IsSetStrand() == true);
    CHECK_EQUAL(eNa_strand_both, ssl.seqloc->GetInt().GetStrand());

    CHECK(ssl.seqloc->GetInt().IsSetFrom() == true);
    CHECK_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    CHECK(ssl.seqloc->GetInt().IsSetTo() == true);
    const TSeqPos length(682);
    CHECK_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

    CHECK(ssl.seqloc->GetInt().IsSetId() == true);
    CHECK_EQUAL(CSeq_id::e_Local, ssl.seqloc->GetInt().GetId().Which());
    CHECK(!ssl.mask);

    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
    CHECK(bioseqs->CanGetSeq_set());
    CHECK(bioseqs->GetSeq_set().front()->IsSeq());
    CHECK(bioseqs->GetSeq_set().front()->GetSeq().CanGetInst());
    CHECK(bioseqs->GetSeq_set().front()->GetSeq().GetInst().CanGetRepr());
    CHECK(bioseqs->GetSeq_set().front()->GetSeq().GetInst().GetRepr() 
          == CSeq_inst::eRepr_raw);

}

class CAutoEnvironmentVariable
{
public:
    CAutoEnvironmentVariable(const char* var_name) 
        : m_VariableName(var_name)
    {
        _ASSERT(var_name);
        string var(m_VariableName);
        string value("1");
        CNcbiEnvironment env(0);
        env.Set(var, value);
    }

    ~CAutoEnvironmentVariable() {
        string var(m_VariableName);
        CNcbiEnvironment env(0);
        env.Set(var, kEmptyStr);
    }
private:
    const char* m_VariableName;
};

BOOST_AUTO_TEST_CASE(RawFastaNoSpaces_UpperCaseWithN_ReadDeltaSeq)
{
    // Note the setting of the environment variable
    CAutoEnvironmentVariable env("BLASTINPUT_GEN_DELTA_SEQ");
    CNcbiIfstream infile("data/nucl_w_n.fsa");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    CScope s(*CObjectManager::GetInstance());
    blast::TSeqLocVector seqs = source->GetAllSeqLocs(s);
    blast::SSeqLoc ssl = seqs.front();
    (void)ssl;
    CHECK(source->End() == true);

    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
    CHECK(bioseqs->CanGetSeq_set());
    CHECK(bioseqs->GetSeq_set().front()->IsSeq());
    CHECK(bioseqs->GetSeq_set().front()->GetSeq().CanGetInst());
    CHECK(bioseqs->GetSeq_set().front()->GetSeq().GetInst().CanGetRepr());
    CHECK(bioseqs->GetSeq_set().front()->GetSeq().GetInst().GetRepr() 
          == CSeq_inst::eRepr_delta);
}


BOOST_AUTO_TEST_CASE(ReadGenbankReport)
{
    CAutoDiagnosticsRedirector dr;

    // Redirect the output warnings
    CNcbiOstrstream error_stream;
    dr.Redirect(error_stream);

    // this is gi 555, length 624
    CNcbiIfstream infile("data/gbreport.txt");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    CScope scope(*CObjectManager::GetInstance());
    CHECK(source->End() == false);
    blast::SSeqLoc ssl = source->GetNextSeqLocBatch(scope).front();
    CHECK(source->End() == true);

    string s(error_stream.str());
    CHECK(s.find("Ignoring invalid residue 1 at position ") != NPOS);

    CHECK(ssl.seqloc->IsInt() == true);

    CHECK(ssl.seqloc->GetInt().IsSetStrand() == true);
    CHECK_EQUAL(eNa_strand_both, ssl.seqloc->GetInt().GetStrand());

    CHECK(ssl.seqloc->GetInt().IsSetFrom() == true);
    CHECK_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    CHECK(ssl.seqloc->GetInt().IsSetTo() == true);
    const TSeqPos length(624);
    CHECK_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

    CHECK(ssl.seqloc->GetInt().IsSetId() == true);
    CHECK_EQUAL(CSeq_id::e_Local, ssl.seqloc->GetInt().GetId().Which());

    CHECK(!ssl.mask);
}

BOOST_AUTO_TEST_CASE(ReadInvalidGi)
{
    const char* fname = "data/invalid_gi.txt";
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);

    CNcbiIfstream infile(fname);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CHECK(source->End() == false);

    CScope scope(*CObjectManager::GetInstance());
    blast::SSeqLoc ssl;
    bool caught_exception(false);
    try { ssl = source->GetNextSeqLocBatch(scope).front(); }
    catch (const CInputException& e) {
        string msg(e.what());
        BOOST_CHECK(msg.find("Sequence ID not found: ") != NPOS);
        BOOST_CHECK_EQUAL(CInputException::eSeqIdNotFound, e.GetErrCode());
        caught_exception = true;
    }
    CHECK(caught_exception);
    CHECK(source->End() == true);
}

BOOST_AUTO_TEST_CASE(ReadInvalidSeqId)
{
    const char* fname = "data/bad_seqid.txt";
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);

    CNcbiIfstream infile(fname);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CHECK(source->End() == false);

    CScope scope(*CObjectManager::GetInstance());
    blast::SSeqLoc ssl;
    bool caught_exception(false);
    try { ssl = source->GetNextSeqLocBatch(scope).front(); }
    catch (const CInputException& e) {
        string msg(e.what());
        BOOST_CHECK(msg.find("Sequence ID not found: ") != NPOS);
        BOOST_CHECK_EQUAL(CInputException::eSeqIdNotFound, e.GetErrCode());
        caught_exception = true;
    }
    CHECK(caught_exception);
    CHECK(source->End() == true);
}

BOOST_AUTO_TEST_CASE(ReadBadUserInput)
{
    const char* fname = "data/bad_input.txt";
    const bool is_protein(false);
    const size_t kNumQueries(0);
    CBlastInputSourceConfig iconfig(is_protein);
    CScope scope(*CObjectManager::GetInstance());

    {
        CNcbiIfstream infile(fname);
        CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
        CHECK(source->End() == false);

        blast::TSeqLocVector query_vector;
        BOOST_REQUIRE_THROW(query_vector = source->GetAllSeqLocs(scope),
                            CObjReaderParseException);
        CHECK_EQUAL(kNumQueries, query_vector.size());
        CHECK_EQUAL(kNumQueries, query_vector.size());

        CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(query_vector);
        CHECK(bioseqs.Empty());
    }

    {
        CNcbiIfstream infile(fname);
        CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
        CHECK(source->End() == false);

        CRef<blast::CBlastQueryVector> query_vector;
        BOOST_REQUIRE_THROW(query_vector = source->GetAllSeqs(scope), 
                            CObjReaderParseException);
        CHECK(query_vector.Empty());
    }

}

/// This unit test proves that if one input is bad, all of them are rejected.
BOOST_AUTO_TEST_CASE(ReadMultipleGis_WithBadInput)
{
    const char* fname = "data/gis_bad_input.txt";
    CNcbiIfstream infile(fname);
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRetrieveSeqData(false);

    vector< pair<long, long> > gi_length;
    gi_length.push_back(make_pair(89161185L, 247249719L));
    // this is never read...
    //gi_length.push_back(make_pair(0L, 0L));   // bad sequence
    //gi_length.push_back(make_pair(557L, 489L));

    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CHECK(source->End() == false);

    CScope scope(*CObjectManager::GetInstance());

    blast::TSeqLocVector seqs;
    BOOST_REQUIRE_THROW(seqs = source->GetAllSeqLocs(scope),
                        CObjReaderParseException);
}

BOOST_AUTO_TEST_CASE(ReadEmptyUserInput)
{
    const char* fname("/dev/null");
    const bool is_protein(true);
    CScope scope(*CObjectManager::GetInstance());
    CBlastInputSourceConfig iconfig(is_protein);
    {
        CNcbiIfstream infile(fname);
        CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
        CHECK(source->End() == true);

        blast::TSeqLocVector query_vector = source->GetAllSeqLocs(scope);
        CHECK(query_vector.empty());

        CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(query_vector);
        BOOST_CHECK(bioseqs.Empty());
    }

    {
        CNcbiIfstream infile(fname);
        CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
        CHECK(source->End() == true);

        CRef<blast::CBlastQueryVector> queries = source->GetAllSeqs(scope);
        CHECK(queries->Empty());
    }

    // Read from buffer
    {
        const string empty;
        CRef<CObjectManager> om(CObjectManager::GetInstance());
        CRef<CBlastFastaInputSource> source;
        
        bool caught_exception(false);
        try { source.Reset(new CBlastFastaInputSource(empty, iconfig)); }
        catch (const CInputException& e) {
            string msg(e.what());
            BOOST_CHECK(msg.find("No sequence input was provided") != NPOS);
            BOOST_CHECK_EQUAL(CInputException::eEmptyUserInput, e.GetErrCode());
            caught_exception = true;
        }
        CHECK(caught_exception);
    }
}

BOOST_AUTO_TEST_CASE(ReadSingleAccession)
{
    CNcbiIfstream infile("data/accession.txt");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRetrieveSeqData(false);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    CScope scope(*CObjectManager::GetInstance());
    CHECK(source->End() == false);
    blast::TSeqLocVector seqs = source->GetAllSeqLocs(scope);
    blast::SSeqLoc ssl = seqs.front();
    CHECK(source->End() == true);

    CHECK(ssl.seqloc->IsInt() == true);

    CHECK(ssl.seqloc->GetInt().IsSetStrand() == true);
    CHECK_EQUAL(eNa_strand_both, ssl.seqloc->GetInt().GetStrand());

    CHECK(ssl.seqloc->GetInt().IsSetFrom() == true);
    CHECK_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    CHECK(ssl.seqloc->GetInt().IsSetTo() == true);
    const TSeqPos length(247249719);
    CHECK_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

    CHECK(ssl.seqloc->GetInt().IsSetId() == true);
    CHECK_EQUAL(CSeq_id::e_Other, ssl.seqloc->GetInt().GetId().Which());
    const string accession("NC_000001");
    CHECK_EQUAL(accession,
                ssl.seqloc->GetInt().GetId().GetOther().GetAccession());
    const int version(9);
    CHECK_EQUAL(version, ssl.seqloc->GetInt().GetId().GetOther().GetVersion());

    CHECK(!ssl.mask);

    /// Validate the data that would be retrieved by blast.cgi
    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
    CHECK_EQUAL((size_t)1, bioseqs->GetSeq_set().size());
    CHECK(bioseqs->GetSeq_set().front()->IsSeq());
    const CBioseq& b = bioseqs->GetSeq_set().front()->GetSeq();
    CHECK(b.IsNa());
    CHECK_EQUAL(CSeq_id::e_Other, b.GetId().front()->Which());
    CHECK_EQUAL(accession, b.GetId().front()->GetOther().GetAccession());
    CHECK_EQUAL(CSeq_inst::eRepr_raw, b.GetInst().GetRepr());
    BOOST_CHECK(CSeq_inst::IsNa(b.GetInst().GetMol()));
    CHECK_EQUAL(length, b.GetInst().GetLength());
}

BOOST_AUTO_TEST_CASE(ReadMultipleAccessions)
{
    CNcbiIfstream infile("data/accessions.txt");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRetrieveSeqData(false);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    vector< pair<string, long> > accession_lengths;
    accession_lengths.push_back(make_pair(string("NC_000001.9"), 247249719L));
    accession_lengths.push_back(make_pair(string("NC_000010.9"), 135374737L));
    accession_lengths.push_back(make_pair(string("NC_000011.8"), 134452384L));
    accession_lengths.push_back(make_pair(string("NC_000012.10"), 132349534L));

    const size_t kNumQueries(accession_lengths.size());
    CScope scope(*CObjectManager::GetInstance());
    blast::TSeqLocVector query_vector = source->GetAllSeqLocs(scope);
    CHECK_EQUAL(kNumQueries, query_vector.size());
    CHECK(source->End() == true);

    {{
        blast::TSeqLocVector cached_queries = source->GetAllSeqLocs(scope);
        CHECK_EQUAL((size_t)0, (size_t)cached_queries.size());
        CHECK(source->End() == true);
    }}

    for (size_t i = 0; i < kNumQueries; i++) {

        blast::SSeqLoc& ssl = query_vector[i];
        CHECK_EQUAL(eNa_strand_both, ssl.seqloc->GetStrand());
        CHECK_EQUAL((TSeqPos)accession_lengths[i].second - 1, 
                    ssl.seqloc->GetInt().GetTo());

        CHECK(ssl.seqloc->GetInt().IsSetId() == true);
        CHECK_EQUAL(CSeq_id::e_Other, ssl.seqloc->GetInt().GetId().Which());
        string accession;
        int version;
        switch (i) {
        case 0: accession.assign("NC_000001"); version = 9; break;
        case 1: accession.assign("NC_000010"); version = 9; break;
        case 2: accession.assign("NC_000011"); version = 8; break;
        case 3: accession.assign("NC_000012"); version = 10; break;
        default: abort();
        }

        CHECK_EQUAL(accession,
                    ssl.seqloc->GetInt().GetId().GetOther().GetAccession());
        CHECK_EQUAL(version, 
                    ssl.seqloc->GetInt().GetId().GetOther().GetVersion());
        CHECK(!ssl.mask);

    }

    /// Validate the data that would be retrieved by blast.cgi
    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(query_vector);
    CHECK_EQUAL(kNumQueries, bioseqs->GetSeq_set().size());
}

// This test was created to test issues in jira/browse/CXX-82
BOOST_AUTO_TEST_CASE(ReadMultipleAccessionsFromMemory)
{
    typedef vector< pair<string, int> > TStringIntVector;
    TStringIntVector accession_lengths;
    accession_lengths.push_back(make_pair(string("P01012.2"), 386));
    accession_lengths.push_back(make_pair(string("1OVA-A"), 385));
    // Fails in entrez, we implemented regex for this in CBlastInputReader
    accession_lengths.push_back(make_pair(string("pdb|1OVA-A"), 385));
    // Note the double bar..
    accession_lengths.push_back(make_pair(string("prf||0705172A"), 385));
    // Fails in entrez, we implemented regex for this in CBlastInputReader
    accession_lengths.push_back(make_pair(string("sp|P01012.2"), 386));

    // This we're not even going to try to fix...
    //accession_lengths.push_back(make_pair(string("0705172A"), 385));

    string user_input;
    ITERATE(TStringIntVector, itr, accession_lengths) {
        user_input += itr->first + "\n";
    }
    istringstream instream(user_input);

    const bool is_protein(true);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRetrieveSeqData(false);
    CRef<CBlastInput> source(s_DeclareBlastInput(instream, iconfig));

    const size_t kNumQueries(accession_lengths.size());
    CScope scope(*CObjectManager::GetInstance());
    blast::TSeqLocVector query_vector = source->GetAllSeqLocs(scope);
    CHECK_EQUAL(kNumQueries, query_vector.size());
    CHECK(source->End() == true);

    {{
        blast::TSeqLocVector cached_queries = source->GetAllSeqLocs(scope);
        CHECK_EQUAL((size_t)0, (size_t)cached_queries.size());
        CHECK(source->End() == true);
    }}

    for (size_t i = 0; i < kNumQueries; i++) {

        //cout << "Accession: '" << accession_lengths[i].first << "'" << endl;
        blast::SSeqLoc& ssl = query_vector[i];
        CHECK_EQUAL((TSeqPos)accession_lengths[i].second - 1, 
                    ssl.seqloc->GetInt().GetTo());
        CHECK_EQUAL(eNa_strand_unknown, ssl.seqloc->GetStrand());

        CHECK(ssl.seqloc->GetInt().IsSetId() == true);
    }

    /// Validate the data that would be retrieved by blast.cgi
    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(query_vector);
    CHECK_EQUAL(kNumQueries, bioseqs->GetSeq_set().size());
}

BOOST_AUTO_TEST_CASE(ReadSingleGi)
{
    CNcbiIfstream infile("data/gi.txt");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRetrieveSeqData(false);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    CScope scope(*CObjectManager::GetInstance());
    CHECK(source->End() == false);
    blast::TSeqLocVector seqs = source->GetAllSeqLocs(scope);
    blast::SSeqLoc ssl = seqs.front();
    CHECK(source->End() == true);

    CHECK(ssl.seqloc->IsInt() == true);

    CHECK(ssl.seqloc->GetInt().IsSetStrand() == true);
    CHECK_EQUAL(eNa_strand_both, ssl.seqloc->GetInt().GetStrand());

    CHECK(ssl.seqloc->GetInt().IsSetFrom() == true);
    CHECK_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    CHECK(ssl.seqloc->GetInt().IsSetTo() == true);
    const TSeqPos length = 247249719;
    CHECK_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

    CHECK(ssl.seqloc->GetInt().IsSetId() == true);
    CHECK_EQUAL(CSeq_id::e_Gi, ssl.seqloc->GetInt().GetId().Which());
    const int gi = 89161185;
    CHECK_EQUAL(gi, ssl.seqloc->GetInt().GetId().GetGi());

    CHECK(!ssl.mask);

    /// Validate the data that would be retrieved by blast.cgi
    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
    CHECK_EQUAL((size_t)1, bioseqs->GetSeq_set().size());
    CHECK(bioseqs->GetSeq_set().front()->IsSeq());
    const CBioseq& b = bioseqs->GetSeq_set().front()->GetSeq();
    CHECK(b.IsNa());
    CHECK_EQUAL(CSeq_id::e_Gi, b.GetId().front()->Which());
    CHECK_EQUAL(gi, b.GetId().front()->GetGi());
    CHECK_EQUAL(CSeq_inst::eRepr_raw, b.GetInst().GetRepr());
    BOOST_CHECK(CSeq_inst::IsNa(b.GetInst().GetMol()));
    CHECK_EQUAL(length, b.GetInst().GetLength());
}

BOOST_AUTO_TEST_CASE(ReadMultipleGis)
{
    CNcbiIfstream infile("data/gis.txt");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRetrieveSeqData(false);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    vector< pair<long, long> > gi_length;
    gi_length.push_back(make_pair(89161185L, 247249719L));
    gi_length.push_back(make_pair(555L, 624L));
    gi_length.push_back(make_pair(557L, 489L));

    const size_t kNumQueries(gi_length.size());
    CScope scope(*CObjectManager::GetInstance());
    CHECK(source->End() == false);
    blast::TSeqLocVector seqs = source->GetAllSeqLocs(scope);
    CHECK(source->End() == true);

    for (size_t i = 0; i < kNumQueries; i++) {
        blast::SSeqLoc ssl = seqs[i];
        CHECK(ssl.seqloc->IsInt() == true);

        CHECK(ssl.seqloc->GetInt().IsSetStrand() == true);
        CHECK_EQUAL(eNa_strand_both, ssl.seqloc->GetInt().GetStrand());

        CHECK(ssl.seqloc->GetInt().IsSetFrom() == true);
        CHECK_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

        CHECK(ssl.seqloc->GetInt().IsSetTo() == true);
        const TSeqPos length = gi_length[i].second;
        CHECK_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

        CHECK(ssl.seqloc->GetInt().IsSetId() == true);
        CHECK_EQUAL(CSeq_id::e_Gi, ssl.seqloc->GetInt().GetId().Which());
        const int gi = gi_length[i].first;
        CHECK_EQUAL(gi, ssl.seqloc->GetInt().GetId().GetGi());

        CHECK(!ssl.mask);
    }

    /// Validate the data that would be retrieved by blast.cgi
    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
    CHECK_EQUAL(kNumQueries, bioseqs->GetSeq_set().size());

    CBioseq_set::TSeq_set::const_iterator itr = bioseqs->GetSeq_set().begin();
    CBioseq_set::TSeq_set::const_iterator end = bioseqs->GetSeq_set().end();
    for (size_t i = 0; i < kNumQueries; i++, ++itr) {
        CHECK(itr != end);
        CHECK((*itr)->IsSeq());
        const CBioseq& b = (*itr)->GetSeq();
        CHECK(b.IsNa());
        CHECK_EQUAL(CSeq_id::e_Gi, b.GetId().front()->Which());
        CHECK_EQUAL(gi_length[i].first, b.GetId().front()->GetGi());
        CHECK_EQUAL(CSeq_inst::eRepr_raw, b.GetInst().GetRepr());
        BOOST_CHECK(CSeq_inst::IsNa(b.GetInst().GetMol()));
        CHECK_EQUAL((long)gi_length[i].second, (long)b.GetInst().GetLength());
    }
}

// This input file contains very short sequences (1-3 bases) which were product
// of a sequencing machine
BOOST_AUTO_TEST_CASE(ReadMultipleSequencesFromSequencer)
{
    CNcbiIfstream infile("data/DF-1.txt");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    const size_t kNumQueries(96);

    CHECK(source->End() == false);

    CScope scope(*CObjectManager::GetInstance());
    blast::TSeqLocVector query_vector = source->GetAllSeqLocs(scope);
    CHECK_EQUAL(kNumQueries, query_vector.size());
}

BOOST_AUTO_TEST_CASE(ReadMultipleTis)
{
    CNcbiIfstream infile("data/tis.txt");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRetrieveSeqData(false);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CScope scope(*CObjectManager::GetInstance());

    CHECK(source->End() == false);

    vector< pair<int, long> > ti_lengths;
    ti_lengths.push_back(make_pair(12345, 657L));
    ti_lengths.push_back(make_pair(12347, 839L));
    ti_lengths.push_back(make_pair(12348, 658L));
    ti_lengths.push_back(make_pair(10000, 670L));

    const size_t kNumQueries(ti_lengths.size());
    blast::TSeqLocVector query_vector = source->GetAllSeqLocs(scope);
    CHECK_EQUAL(kNumQueries, query_vector.size());
    CHECK(source->End() == true);

    {{
        blast::TSeqLocVector cached_queries = source->GetAllSeqLocs(scope);
        CHECK_EQUAL((size_t)0, (size_t)cached_queries.size());
        CHECK(source->End() == true);
    }}

    const string db("ti");
    for (size_t i = 0; i < kNumQueries; i++) {

        const blast::SSeqLoc& ssl = query_vector[i];
        CHECK(ssl.seqloc->IsInt());
        const CSeq_interval& seqint = ssl.seqloc->GetInt();
        CHECK_EQUAL(eNa_strand_both, ssl.seqloc->GetStrand());
        CHECK_EQUAL((TSeqPos)ti_lengths[i].second - 1, seqint.GetTo());

        CHECK(seqint.IsSetId() == true);
        CHECK_EQUAL(CSeq_id::e_General, seqint.GetId().Which());
        CHECK_EQUAL(db, seqint.GetId().GetGeneral().GetDb());
        CHECK_EQUAL(ti_lengths[i].first,
                    seqint.GetId().GetGeneral().GetTag().GetId());
        CHECK(!ssl.mask);
    }

    /// Validate the data that would be retrieved by blast.cgi
    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(query_vector);
    CHECK_EQUAL(kNumQueries, bioseqs->GetSeq_set().size());
}

BOOST_AUTO_TEST_CASE(ReadSingleTi)
{
    CNcbiIfstream infile("data/ti.txt");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRetrieveSeqData(true);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    CScope scope(*CObjectManager::GetInstance());
    CHECK(source->End() == false);
    blast::TSeqLocVector seqs = source->GetAllSeqLocs(scope);
    blast::SSeqLoc ssl = seqs.front();
    CHECK(source->End() == true);

    CHECK(ssl.seqloc->IsInt() == true);

    CHECK(ssl.seqloc->GetInt().IsSetStrand() == true);
    CHECK_EQUAL(eNa_strand_both, ssl.seqloc->GetInt().GetStrand());

    CHECK(ssl.seqloc->GetInt().IsSetFrom() == true);
    CHECK_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    CHECK(ssl.seqloc->GetInt().IsSetTo() == true);
    const TSeqPos length(657);
    CHECK_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

    CHECK(ssl.seqloc->GetInt().IsSetId() == true);
    CHECK_EQUAL(CSeq_id::e_General, ssl.seqloc->GetInt().GetId().Which());
    const string db("TRACE");
    CHECK_EQUAL(db, ssl.seqloc->GetInt().GetId().GetGeneral().GetDb());
    CHECK(ssl.seqloc->GetInt().GetId().GetGeneral().GetTag().IsId());
    const int ti(12345);
    CHECK_EQUAL(ti, ssl.seqloc->GetInt().GetId().GetGeneral().GetTag().GetId());

    CHECK(!ssl.mask);

    /// Validate the data that would be retrieved by blast.cgi
    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
    CHECK_EQUAL((size_t)1, bioseqs->GetSeq_set().size());
    CHECK(bioseqs->GetSeq_set().front()->IsSeq());
    const CBioseq& b = bioseqs->GetSeq_set().front()->GetSeq();
    CHECK(b.IsNa());
    CHECK_EQUAL(CSeq_id::e_General, b.GetId().front()->Which());
    CHECK_EQUAL(db, b.GetId().front()->GetGeneral().GetDb());
    CHECK_EQUAL(ti, b.GetId().front()->GetGeneral().GetTag().GetId());
    CHECK_EQUAL(CSeq_inst::eRepr_raw, b.GetInst().GetRepr());
    BOOST_CHECK(CSeq_inst::IsNa(b.GetInst().GetMol()));
    CHECK_EQUAL(length, b.GetInst().GetLength());
}

BOOST_AUTO_TEST_CASE(ReadAccessionsAndGisWithNewLines)
{
    CNcbiIfstream infile("data/accgis_nl.txt");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRetrieveSeqData(false);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    vector< pair<string, long> > gi_accessions;
    gi_accessions.push_back(make_pair(string("89161215"), 111583154L));
    gi_accessions.push_back(make_pair(string("89161217"), 155407050L));
    gi_accessions.push_back(make_pair(string("89161219"), 11133097L));
    gi_accessions.push_back(make_pair(string("NC_000001.9"), 247249719L));
    gi_accessions.push_back(make_pair(string("NC_000010.9"), 135374737L));
    gi_accessions.push_back(make_pair(string("gnl|ti|12345"), 657L));
    gi_accessions.push_back(make_pair(string("NC_000011.8"), 134452384L));
    gi_accessions.push_back(make_pair(string("NC_000012.10"), 132349534L));

    const size_t kNumQueries(gi_accessions.size());
    CScope scope(*CObjectManager::GetInstance());
    blast::TSeqLocVector query_vector = source->GetAllSeqLocs(scope);
    CHECK_EQUAL(kNumQueries, query_vector.size());
    CHECK(source->End() == true);

    {{
        blast::TSeqLocVector cached_queries = source->GetAllSeqLocs(scope);
        CHECK_EQUAL((size_t)0, (size_t)cached_queries.size());
        CHECK(source->End() == true);
    }}

    for (size_t i = 0; i < kNumQueries; i++) {

        blast::SSeqLoc& ssl = query_vector[i];
        CHECK_EQUAL(eNa_strand_both, ssl.seqloc->GetStrand());
        CHECK_EQUAL((TSeqPos)gi_accessions[i].second - 1, 
                    ssl.seqloc->GetInt().GetTo());

        const string& id = gi_accessions[i].first;

        CHECK(ssl.seqloc->GetInt().IsSetId() == true);
        int gi(0);
        if ( (gi = NStr::StringToLong(id, NStr::fConvErr_NoThrow)) != 0) {
            CHECK_EQUAL(CSeq_id::e_Gi, ssl.seqloc->GetInt().GetId().Which());
            CHECK_EQUAL(gi, ssl.seqloc->GetInt().GetId().GetGi());
        } else if (i == 5) {
            CHECK_EQUAL(CSeq_id::e_General, 
                        ssl.seqloc->GetInt().GetId().Which());
            const string db("ti");
            CHECK_EQUAL(db, ssl.seqloc->GetInt().GetId().GetGeneral().GetDb());
            CHECK(ssl.seqloc->GetInt().GetId().GetGeneral().GetTag().IsId());
            const int ti(12345);
            CHECK_EQUAL(ti, 
                        ssl.seqloc->GetInt().GetId().
                        GetGeneral().GetTag().GetId());
        } else {
            CHECK_EQUAL(CSeq_id::e_Other, ssl.seqloc->GetInt().GetId().Which());
            string accession;
            int version;

            switch (i) {
            case 3: accession.assign("NC_000001"); version = 9; break;
            case 4: accession.assign("NC_000010"); version = 9; break;
            case 6: accession.assign("NC_000011"); version = 8; break;
            case 7: accession.assign("NC_000012"); version = 10; break;
            default: abort();
            }

            CHECK_EQUAL(accession,
                        ssl.seqloc->GetInt().GetId().GetOther().GetAccession());
            CHECK_EQUAL(version, 
                        ssl.seqloc->GetInt().GetId().GetOther().GetVersion());
        }
        CHECK(!ssl.mask);

    }

    /// Validate the data that would be retrieved by blast.cgi
    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(query_vector);
    CHECK_EQUAL(kNumQueries, bioseqs->GetSeq_set().size());
}

static string*
s_FileContents2String(const char* file_name)
{
    CNcbiIfstream file(file_name);
    char buffer[2048] = { '\0' };
    auto_ptr<string> retval(new string);

    while (file.getline(buffer, sizeof(buffer))) {
        (*retval) += string(buffer) + "\n";
    }

    return retval.release();
}

BOOST_AUTO_TEST_CASE(ReadAccessionNucleotideIntoBuffer_Single)
{
    const char* fname("data/accession.txt");
    auto_ptr<string> user_input(s_FileContents2String(fname));

    CRef<CObjectManager> om(CObjectManager::GetInstance());
    CBlastInputSourceConfig iconfig(false);
    iconfig.SetRetrieveSeqData(false);
    CRef<CBlastInput> source(s_DeclareBlastInput(*user_input, iconfig));

    CScope scope(*om);
    CHECK(source->End() == false);
    blast::TSeqLocVector seqs = source->GetAllSeqLocs(scope);
    blast::SSeqLoc ssl = seqs.front();


    CHECK(source->End() == true);

    CHECK(ssl.seqloc->IsInt() == true);

    CHECK(ssl.seqloc->GetInt().IsSetStrand() == true);
    CHECK_EQUAL(eNa_strand_both, ssl.seqloc->GetInt().GetStrand());

    CHECK(ssl.seqloc->GetInt().IsSetFrom() == true);
    CHECK_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    CHECK(ssl.seqloc->GetInt().IsSetTo() == true);
    const TSeqPos length(247249719);
    CHECK_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

    CHECK(ssl.seqloc->GetInt().IsSetId() == true);
    CHECK_EQUAL(CSeq_id::e_Other, ssl.seqloc->GetInt().GetId().Which());
    const string accession("NC_000001");
    CHECK_EQUAL(accession,
                ssl.seqloc->GetInt().GetId().GetOther().GetAccession());
    const int version(9);
    CHECK_EQUAL(version, ssl.seqloc->GetInt().GetId().GetOther().GetVersion());

    CHECK(!ssl.mask);

    /// Validate the data that would be retrieved by blast.cgi
    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
    CHECK_EQUAL((size_t)1, bioseqs->GetSeq_set().size());
    CHECK(bioseqs->GetSeq_set().front()->IsSeq());
    const CBioseq& b = bioseqs->GetSeq_set().front()->GetSeq();
    CHECK(b.IsNa());
    CHECK_EQUAL(CSeq_id::e_Other, b.GetId().front()->Which());
    CHECK_EQUAL(accession, b.GetId().front()->GetOther().GetAccession());
    CHECK_EQUAL(CSeq_inst::eRepr_raw, b.GetInst().GetRepr());
    BOOST_CHECK(CSeq_inst::IsNa(b.GetInst().GetMol()));
    CHECK_EQUAL(length, b.GetInst().GetLength());

}

BOOST_AUTO_TEST_CASE(ReadGiNuclWithFlankingSpacesIntoBuffer_Single)
{
    auto_ptr<string> user_input(new string("    1945386   "));

    CRef<CObjectManager> om(CObjectManager::GetInstance());
    CBlastInputSourceConfig iconfig(false);
    CRef<CBlastInput> source(s_DeclareBlastInput(*user_input, iconfig));

    CScope scope(*om);
    CHECK(source->End() == false);
    blast::TSeqLocVector seqs = source->GetAllSeqLocs(scope);
    blast::SSeqLoc ssl = seqs.front();

    CHECK(source->End() == true);

    CHECK(ssl.seqloc->IsInt() == true);

    CHECK(ssl.seqloc->GetInt().IsSetStrand() == true);
    CHECK_EQUAL(eNa_strand_both, ssl.seqloc->GetInt().GetStrand());

    CHECK(ssl.seqloc->GetInt().IsSetFrom() == true);
    CHECK_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    CHECK(ssl.seqloc->GetInt().IsSetTo() == true);
    const TSeqPos length(2772);
    CHECK_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

    CHECK(ssl.seqloc->GetInt().IsSetId() == true);
    CHECK_EQUAL(CSeq_id::e_Gi, ssl.seqloc->GetInt().GetId().Which());
    const int gi(1945386);
    CHECK_EQUAL(gi, ssl.seqloc->GetInt().GetId().GetGi());

    CHECK(!ssl.mask);

    /// Validate the data that would be retrieved by blast.cgi
    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
    CHECK_EQUAL((size_t)1, bioseqs->GetSeq_set().size());
    CHECK(bioseqs->GetSeq_set().front()->IsSeq());
    const CBioseq& b = bioseqs->GetSeq_set().front()->GetSeq();
    CHECK(b.IsNa());
    CHECK_EQUAL(CSeq_id::e_Gi, b.GetId().front()->Which());
    CHECK_EQUAL(gi, b.GetId().front()->GetGi());
    CHECK_EQUAL(CSeq_inst::eRepr_raw, b.GetInst().GetRepr());
    BOOST_CHECK(CSeq_inst::IsNa(b.GetInst().GetMol()));
    CHECK_EQUAL(length, b.GetInst().GetLength());

}

BOOST_AUTO_TEST_CASE(ReadAccessionNuclWithFlankingSpacesIntoBuffer_Single)
{
    auto_ptr<string> user_input(new string("    u93236 "));

    CRef<CObjectManager> om(CObjectManager::GetInstance());
    CBlastInputSourceConfig iconfig(false);
    CBlastFastaInputSource fasta_source(*user_input, iconfig);
    CBlastInput source(&fasta_source);

    CScope scope(*om);
    CHECK(source.End() == false);
    blast::TSeqLocVector seqs = source.GetAllSeqLocs(scope);
    blast::SSeqLoc ssl = seqs.front();

    CHECK(source.End() == true);

    CHECK(ssl.seqloc->IsInt() == true);

    CHECK(ssl.seqloc->GetInt().IsSetStrand() == true);
    CHECK_EQUAL(eNa_strand_both, ssl.seqloc->GetInt().GetStrand());

    CHECK(ssl.seqloc->GetInt().IsSetFrom() == true);
    CHECK_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    CHECK(ssl.seqloc->GetInt().IsSetTo() == true);
    const TSeqPos length(2772);
    CHECK_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

    CHECK(ssl.seqloc->GetInt().IsSetId() == true);
    CHECK_EQUAL(CSeq_id::e_Genbank, ssl.seqloc->GetInt().GetId().Which());
    const string accession("u93236");
    CHECK_EQUAL(NStr::CompareNocase(accession,
                ssl.seqloc->GetInt().GetId().GetGenbank().GetAccession()), 0);

    CHECK(!ssl.mask);

    /// Validate the data that would be retrieved by blast.cgi
    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
    CHECK_EQUAL((size_t)1, bioseqs->GetSeq_set().size());
    CHECK(bioseqs->GetSeq_set().front()->IsSeq());
    const CBioseq& b = bioseqs->GetSeq_set().front()->GetSeq();
    CHECK(b.IsNa());
    CHECK_EQUAL(CSeq_id::e_Genbank, b.GetId().front()->Which());
    CHECK_EQUAL(NStr::CompareNocase(accession,
                b.GetId().front()->GetGenbank().GetAccession()), 0);
    CHECK_EQUAL(CSeq_inst::eRepr_raw, b.GetInst().GetRepr());
    BOOST_CHECK(CSeq_inst::IsNa(b.GetInst().GetMol()));
    CHECK_EQUAL(length, b.GetInst().GetLength());

}

BOOST_AUTO_TEST_CASE(ReadFastaWithDeflineProteinIntoBuffer_Single)
{
    const char* fname("data/aa.129295");
    auto_ptr<string> user_input(s_FileContents2String(fname));

    CRef<CObjectManager> om(CObjectManager::GetInstance());
    CBlastInputSourceConfig iconfig(true);
    CBlastFastaInputSource fasta_source(*user_input, iconfig);
    CBlastInput source(&fasta_source);

    CScope scope(*om);
    CHECK(source.End() == false);
    blast::TSeqLocVector seqs = source.GetAllSeqLocs(scope);
    blast::SSeqLoc ssl = seqs.front();

    CHECK(source.End() == true);

    CHECK(ssl.seqloc->IsInt() == true);

    CHECK(ssl.seqloc->GetInt().IsSetStrand() == true);
    CHECK_EQUAL(eNa_strand_unknown, ssl.seqloc->GetInt().GetStrand());

    CHECK(ssl.seqloc->GetInt().IsSetFrom() == true);
    CHECK_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    CHECK(ssl.seqloc->GetInt().IsSetTo() == true);
    const TSeqPos length = 232;
    CHECK_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

    CHECK(ssl.seqloc->GetInt().IsSetId() == true);
    CHECK_EQUAL(CSeq_id::e_Local, ssl.seqloc->GetInt().GetId().Which());

    CHECK(!ssl.mask);

    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
    CHECK_EQUAL((size_t)1, bioseqs->GetSeq_set().size());
    CHECK(bioseqs->GetSeq_set().front()->IsSeq());
    const CBioseq& b = bioseqs->GetSeq_set().front()->GetSeq();
    CHECK(b.IsAa());
    CHECK_EQUAL(CSeq_inst::eRepr_raw, b.GetInst().GetRepr());
    CHECK_EQUAL(CSeq_inst::eMol_aa, b.GetInst().GetMol());
    CHECK_EQUAL(length, b.GetInst().GetLength());

}

BOOST_AUTO_TEST_CASE(RangeBoth)
{
    CNcbiIfstream infile("data/aa.129295");
    const bool is_protein(true);
    const TSeqPos start(50);
    const TSeqPos stop(100);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRange().SetFrom(start);
    iconfig.SetRange().SetTo(stop);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    CScope scope(*CObjectManager::GetInstance());
    blast::SSeqLoc ssl = source->GetNextSeqLocBatch(scope).front();

    CHECK_EQUAL(start, ssl.seqloc->GetInt().GetFrom());
    CHECK_EQUAL(stop, ssl.seqloc->GetInt().GetTo());
    CHECK_EQUAL(start, ssl.seqloc->GetStart(eExtreme_Positional));
    CHECK_EQUAL(stop, ssl.seqloc->GetStop(eExtreme_Positional));
}

BOOST_AUTO_TEST_CASE(RangeStartOnly)
{
    CNcbiIfstream infile("data/aa.129295");
    const bool is_protein(true);
    const TSeqPos start(50);
    const TSeqPos length(232);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRange().SetFrom(start);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    CScope scope(*CObjectManager::GetInstance());
    blast::SSeqLoc ssl = source->GetNextSeqLocBatch(scope).front();

    CHECK_EQUAL(start, ssl.seqloc->GetInt().GetFrom());
    CHECK_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());
    CHECK_EQUAL(start, ssl.seqloc->GetStart(eExtreme_Positional));
    CHECK_EQUAL(length-1, ssl.seqloc->GetStop(eExtreme_Positional));
}

BOOST_AUTO_TEST_CASE(RangeInvalid_FromGreaterThanTo)
{
    CNcbiIfstream infile("data/aa.129295");
    const bool is_protein(true);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRange().SetFrom(100);
    iconfig.SetRange().SetTo(50);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CScope scope(*CObjectManager::GetInstance());

    try { source->GetNextSeqLocBatch(scope).front(); }
    catch (const CInputException& e) {
        string msg(e.what());
        BOOST_CHECK(msg.find("Invalid sequence range") != NPOS);
        BOOST_CHECK_EQUAL(CInputException::eInvalidRange, e.GetErrCode());
        return;
    }
    BOOST_CHECK(false); // should never get here
}

BOOST_AUTO_TEST_CASE(RangeInvalid_FromGreaterThanSequenceLength)
{
    CNcbiIfstream infile("data/aa.129295");
    const bool is_protein(true);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRange().SetFrom(1000);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CScope scope(*CObjectManager::GetInstance());

    try { source->GetNextSeqLocBatch(scope).front(); }
    catch (const CInputException& e) {
        string msg(e.what());
        BOOST_CHECK(msg.find("Invalid from coordinate") != NPOS);
        BOOST_CHECK_EQUAL(CInputException::eInvalidRange, e.GetErrCode());
        return;
    }
    BOOST_CHECK(false); // should never get here
}

BOOST_AUTO_TEST_CASE(RangeInvalid_ToEqualThanSequenceLength)
{
    CNcbiIfstream infile("data/aa.129295");
    const bool is_protein(true);
    const TSeqPos length(232);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRange().SetFrom(10);
    iconfig.SetRange().SetTo(length);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CScope scope(*CObjectManager::GetInstance());

    blast::SSeqLoc ssl = source->GetNextSeqLocBatch(scope).front();

    CHECK(ssl.seqloc->IsInt() == true);

    CHECK(ssl.seqloc->GetInt().IsSetStrand() == true);
    CHECK_EQUAL(eNa_strand_unknown, ssl.seqloc->GetInt().GetStrand());

    CHECK(ssl.seqloc->GetInt().IsSetFrom() == true);
    CHECK_EQUAL((TSeqPos)10, ssl.seqloc->GetInt().GetFrom());

    CHECK(ssl.seqloc->GetInt().IsSetTo() == true);
    CHECK_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());
}

BOOST_AUTO_TEST_CASE(RangeInvalid_ToGreaterThanSequenceLength)
{
    CNcbiIfstream infile("data/aa.129295");
    const bool is_protein(true);
    const TSeqPos length(232);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRange().SetFrom(10);
    iconfig.SetRange().SetTo(length*2);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CScope scope(*CObjectManager::GetInstance());

    blast::SSeqLoc ssl = source->GetNextSeqLocBatch(scope).front();

    CHECK(ssl.seqloc->IsInt() == true);

    CHECK(ssl.seqloc->GetInt().IsSetStrand() == true);
    CHECK_EQUAL(eNa_strand_unknown, ssl.seqloc->GetInt().GetStrand());

    CHECK(ssl.seqloc->GetInt().IsSetFrom() == true);
    CHECK_EQUAL((TSeqPos)10, ssl.seqloc->GetInt().GetFrom());

    CHECK(ssl.seqloc->GetInt().IsSetTo() == true);
    CHECK_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());
}

BOOST_AUTO_TEST_CASE(ParseDefline)
{
    CNcbiIfstream infile("data/aa.129295");
    const bool is_protein(true);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetBelieveDeflines(true);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CScope scope(*CObjectManager::GetInstance());

    const int gi(129295);
    blast::SSeqLoc ssl = source->GetNextSeqLocBatch(scope).front();
    CHECK_EQUAL(CSeq_id::e_Gi, ssl.seqloc->GetId()->Which());
    CHECK_EQUAL(gi, ssl.seqloc->GetId()->GetGi());
    CHECK_EQUAL(CSeq_id::e_Gi, ssl.seqloc->GetInt().GetId().Which());
    CHECK_EQUAL(gi, ssl.seqloc->GetInt().GetId().GetGi());
}

BOOST_AUTO_TEST_CASE(BadProtStrand)
{
    CNcbiIfstream infile("data/aa.129295");
    const bool is_protein(true);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetStrand(eNa_strand_both);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CScope scope(*CObjectManager::GetInstance());

    bool caught_exception(false);
    try { blast::SSeqLoc ssl = source->GetNextSeqLocBatch(scope).front(); }
    catch (const CInputException& e) {
        string msg(e.what());
        BOOST_CHECK(msg.find("Cannot assign nucleotide strand to protein") 
                    != NPOS);
        BOOST_CHECK_EQUAL(CInputException::eInvalidStrand, e.GetErrCode());
        caught_exception = true;
    }
    CHECK(caught_exception);
    CHECK(source->End() == true);
}

BOOST_AUTO_TEST_CASE(ReadFastaWithDeflineNucl_Multiple)
{
    CNcbiIfstream infile("data/nt.cat");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetStrand(eNa_strand_both);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    const size_t kNumQueries(2);
    CScope scope(*CObjectManager::GetInstance());
    blast::TSeqLocVector query_vector = source->GetAllSeqLocs(scope);
    CHECK_EQUAL(kNumQueries, query_vector.size());
    CHECK(source->End() == true);

    {{
        blast::TSeqLocVector cached_queries = source->GetAllSeqLocs(scope);
        CHECK_EQUAL((size_t)0, (size_t)cached_queries.size());
        CHECK(source->End() == true);
    }}

    blast::SSeqLoc ssl = query_vector.front();
    TSeqPos length = 646;

    CHECK_EQUAL(eNa_strand_both, ssl.seqloc->GetStrand());
    CHECK_EQUAL(length-1, ssl.seqloc->GetStop(eExtreme_Positional));
    CHECK_EQUAL(eNa_strand_both, ssl.seqloc->GetInt().GetStrand());
    CHECK_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

    ssl = query_vector.back();

    length = 360;
    CHECK_EQUAL(eNa_strand_both, ssl.seqloc->GetStrand());
    CHECK_EQUAL(length-1, ssl.seqloc->GetStop(eExtreme_Positional));
    CHECK_EQUAL(eNa_strand_both, ssl.seqloc->GetInt().GetStrand());
    CHECK_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());
    CHECK(!ssl.mask);
}

BOOST_AUTO_TEST_CASE(NuclStrand)
{
    const char* fname("data/nt.cat");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    CScope scope(*CObjectManager::GetInstance());

    // Test plus strand
    {
        CNcbiIfstream infile(fname);
        const ENa_strand strand(eNa_strand_plus);
        iconfig.SetStrand(strand);
        CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
        TSeqLocVector seqs = source->GetAllSeqLocs(scope);

        ITERATE(TSeqLocVector, itr, seqs) {
            const blast::SSeqLoc& ssl = *itr;
            CHECK_EQUAL(strand, ssl.seqloc->GetStrand());
            CHECK_EQUAL(strand, ssl.seqloc->GetInt().GetStrand());
        }
    }

    // Test minus strand
    {
        CNcbiIfstream infile(fname);
        const ENa_strand strand(eNa_strand_minus);
        iconfig.SetStrand(strand);
        CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
        TSeqLocVector seqs = source->GetAllSeqLocs(scope);

        ITERATE(TSeqLocVector, itr, seqs) {
            const blast::SSeqLoc& ssl = *itr;
            CHECK_EQUAL(strand, ssl.seqloc->GetStrand());
            CHECK_EQUAL(strand, ssl.seqloc->GetInt().GetStrand());
        }
    }
}

BOOST_AUTO_TEST_CASE(NuclLcaseMask)
{
    CNcbiIfstream infile("data/nt.cat");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetLowercaseMask(true);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CScope scope(*CObjectManager::GetInstance());

    blast::TSeqLocVector seqs = source->GetNextSeqLocBatch(scope);
    blast::TSeqLocVector::iterator itr = seqs.begin();
    blast::SSeqLoc ssl = *itr;
    CHECK(ssl.mask)
    CHECK(ssl.mask->IsPacked_int());

    CPacked_seqint::Tdata masklocs = ssl.mask->GetPacked_int();
    CHECK_EQUAL((size_t)2, masklocs.size());
    CHECK_EQUAL((TSeqPos)126, masklocs.front()->GetFrom());
    CHECK_EQUAL((TSeqPos)167, masklocs.front()->GetTo());
    CHECK_EQUAL((TSeqPos)330, masklocs.back()->GetFrom());
    CHECK_EQUAL((TSeqPos)356, masklocs.back()->GetTo());

    ssl = *++itr;
    CHECK(ssl.mask);
    CHECK(ssl.mask->IsNull());
}

BOOST_AUTO_TEST_CASE(MultiSeq)
{
    CNcbiIfstream infile("data/aa.cat");
    const bool is_protein(true);
    CBlastInputSourceConfig iconfig(is_protein);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CScope scope(*CObjectManager::GetInstance());

    blast::TSeqLocVector v = source->GetAllSeqLocs(scope);
    CHECK(source->End());
    CHECK_EQUAL((size_t)19, v.size());
}

BOOST_AUTO_TEST_CASE(MultiRange)
{
    CNcbiIfstream infile("data/aa.cat");
    const bool is_protein(true);
    const TSeqPos start(50);
    const TSeqPos stop(100);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRange().SetFrom(start).SetTo(stop);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CScope scope(*CObjectManager::GetInstance());

    blast::TSeqLocVector v = source->GetAllSeqLocs(scope);
    NON_CONST_ITERATE(blast::TSeqLocVector, itr, v) {
        CHECK_EQUAL(start, itr->seqloc->GetStart(eExtreme_Positional));
        CHECK_EQUAL(stop, itr->seqloc->GetStop(eExtreme_Positional));
        CHECK_EQUAL(start, itr->seqloc->GetInt().GetFrom());
        CHECK_EQUAL(stop, itr->seqloc->GetInt().GetTo());
    }
}

BOOST_AUTO_TEST_CASE(MultiBatch)
{
    CNcbiIfstream infile("data/aa.cat");
    const bool is_protein(true);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetBelieveDeflines(true);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig, 5000));
    CScope scope(*CObjectManager::GetInstance());

    int gi;
    blast::TSeqLocVector v;

    v = source->GetNextSeqLocBatch(scope);
    CHECK_EQUAL((size_t)7, v.size());
    CHECK_EQUAL((TSeqPos)530, v[0].seqloc->GetInt().GetTo());
    gi = 1346057;
    CHECK_EQUAL(gi, v[0].seqloc->GetInt().GetId().GetGi());
    CHECK_EQUAL(gi, v[0].seqloc->GetId()->GetGi());

    v = source->GetNextSeqLocBatch(scope);
    CHECK_EQUAL((size_t)8, v.size());
    CHECK_EQUAL((TSeqPos)445, v[0].seqloc->GetInt().GetTo());
    gi = 1170625;
    CHECK_EQUAL(gi, v[0].seqloc->GetInt().GetId().GetGi());
    CHECK_EQUAL(gi, v[0].seqloc->GetId()->GetGi());

    v = source->GetNextSeqLocBatch(scope);
    CHECK_EQUAL((size_t)4, v.size());
    CHECK_EQUAL((TSeqPos)688, v[0].seqloc->GetInt().GetTo());
    gi = 114152;
    CHECK_EQUAL(gi, v[0].seqloc->GetInt().GetId().GetGi());
    CHECK_EQUAL(gi, v[0].seqloc->GetId()->GetGi());

    CHECK(source->End());
}

BOOST_AUTO_TEST_CASE(NoDeflineExpected)
{
    CNcbiIfstream infile("data/tiny.fa");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CScope scope(*CObjectManager::GetInstance());

    blast::TSeqLocVector v = source->GetAllSeqLocs(scope);
    CHECK(source->End());
    CHECK_EQUAL((size_t)1, v.size());
}

BOOST_AUTO_TEST_CASE(NoDeflineUnexpected)
{
    CNcbiIfstream infile("data/tiny.fa");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetBelieveDeflines(true);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CScope scope(*CObjectManager::GetInstance());

    BOOST_CHECK_THROW(source->GetAllSeqLocs(scope), CException);
}

/// Auxiliary class to convert a string into an argument count and vector
class CString2Args
{
public:
    CString2Args(const string& cmd_line_args) {
        x_Init(cmd_line_args);
    }

    ~CString2Args() {
        x_CleanUp();
    }

    void Reset(const string& cmd_line_args) {
        x_CleanUp();
        x_Init(cmd_line_args);
    }

    CArgs* CreateCArgs(CBlastAppArgs& args) const {
        auto_ptr<CArgDescriptions> arg_desc(args.SetCommandLine());
        CNcbiArguments ncbi_args(m_Argc, m_Argv);
        return arg_desc->CreateArgs(ncbi_args);
    }

private:

    /// Functor to help remove empty strings from a container
    struct empty_string_remover : public unary_function<bool, string> {
        bool operator() (const string& str) {
            return str.empty();
        }
    };

    /// Extract the arguments from a command line
    vector<string> x_TokenizeCmdLine(const string& cmd_line_args) {
        vector<string> retval;
        NStr::Tokenize(cmd_line_args, " ", retval);
        vector<string>::iterator new_end = remove_if(retval.begin(), 
                                                     retval.end(), 
                                                     empty_string_remover());
        retval.erase(new_end, retval.end());
        return retval;
    }

    /// Convert a C++ string into a C-style string
    char* x_ToCString(const string& str) {
        char* retval = new char[str.size()+1];
        strncpy(retval, str.c_str(), str.size());
        retval[str.size()] = '\0';
        return retval;
    }

    void x_CleanUp() {
        for (size_t i = 0; i < m_Argc; i++) {
            delete [] m_Argv[i];
        }
        delete [] m_Argv;
    }

    void x_Init(const string& cmd_line_args) {
        const string program_name("./blastinput_unit_test");
        vector<string> args = x_TokenizeCmdLine(cmd_line_args);
        m_Argc = args.size() + 1;   // one extra for dummy program name
        m_Argv = new char*[m_Argc];
        m_Argv[0] = x_ToCString(program_name);
        for (size_t i = 0; i < args.size(); i++) {
            m_Argv[i+1] = x_ToCString(args[i]);
        }
    }

    char** m_Argv;
    size_t m_Argc;
};


/* Test for the PSI-BLAST command line application arguments */

BOOST_AUTO_TEST_CASE(PsiBlastAppTestMatrix)
{
    CPsiBlastAppArgs psiblast_args;
    CString2Args s2a("-matrix BLOSUM80 -db ecoli ");
    auto_ptr<CArgs> args(s2a.CreateCArgs(psiblast_args));

    CRef<CBlastOptionsHandle> opts = psiblast_args.SetOptions(*args);

    CHECK_EQUAL(opts->GetOptions().GetMatrixName(), string("BLOSUM80"));
}

BOOST_AUTO_TEST_CASE(CheckMutuallyExclusiveOptions)
{
    CString2Args s2a("-remote -num_threads 2");

    typedef vector< CRef<CBlastAppArgs> > TArgClasses;
    vector< CRef<CBlastAppArgs> > arg_classes;
    arg_classes.push_back(CRef<CBlastAppArgs>(new CPsiBlastAppArgs));
    arg_classes.push_back(CRef<CBlastAppArgs>(new CBlastpAppArgs));
    arg_classes.push_back(CRef<CBlastAppArgs>(new CBlastnAppArgs));
    arg_classes.push_back(CRef<CBlastAppArgs>(new CBlastxAppArgs));
    arg_classes.push_back(CRef<CBlastAppArgs>(new CTblastnAppArgs));
    arg_classes.push_back(CRef<CBlastAppArgs>(new CTblastxAppArgs));

    NON_CONST_ITERATE(TArgClasses, itr, arg_classes) {
        auto_ptr<CArgs> args;
        BOOST_CHECK_THROW(args.reset(s2a.CreateCArgs(**itr)), 
                          CArgException);
    }
}

BOOST_AUTO_TEST_CASE(CheckDiscoMegablast) {
    auto_ptr<CArgs> args;
    CBlastnAppArgs blastn_args;

    // missing required template_length argument
    CString2Args s2a("-db ecoli -template_type coding ");
    BOOST_CHECK_THROW(args.reset(s2a.CreateCArgs(blastn_args)), 
                      CArgException);
    // missing required template_type argument
    s2a.Reset("-db ecoli -template_length 21 ");
    BOOST_CHECK_THROW(args.reset(s2a.CreateCArgs(blastn_args)), 
                      CArgException);

    // valid combination
    s2a.Reset("-db ecoli -template_type coding -template_length 16");
    BOOST_CHECK_NO_THROW(args.reset(s2a.CreateCArgs(blastn_args)));

    // test the setting of an invalid word size for disco. megablast
    s2a.Reset("-db ecoli -word_size 32 -template_type optimal -template_length 16");
    BOOST_CHECK_NO_THROW(args.reset(s2a.CreateCArgs(blastn_args)));
    CRef<CBlastOptionsHandle> opts = blastn_args.SetOptions(*args);
    BOOST_CHECK_THROW(opts->Validate(), CBlastException);
}

BOOST_AUTO_TEST_CASE(CheckPercentIdentity) {
    auto_ptr<CArgs> args;
    CBlastxAppArgs blast_args;

    // invalid value
    CString2Args s2a("-db ecoli -perc_identity 104.3");
    BOOST_CHECK_THROW(args.reset(s2a.CreateCArgs(blast_args)), 
                      CArgException);

    // valid combination
    s2a.Reset("-db ecoli -perc_identity 75.0 ");
    BOOST_CHECK_NO_THROW(args.reset(s2a.CreateCArgs(blast_args)));
}

BOOST_AUTO_TEST_CASE(CheckNoGreedyExtension) {
    auto_ptr<CArgs> args;
    CBlastnAppArgs blast_args;

    CString2Args s2a("-db ecoli -no_greedy");
    BOOST_CHECK_NO_THROW(args.reset(s2a.CreateCArgs(blast_args)));
    CRef<CBlastOptionsHandle> opts = blast_args.SetOptions(*args);

    BOOST_CHECK(opts->GetOptions().GetGapExtnAlgorithm() != eGreedyScoreOnly);
    BOOST_CHECK(opts->GetOptions().GetGapTracebackAlgorithm() != 
                eGreedyTbck);
    // this throws because non-affine gapping costs must be provided for
    // non-greedy extension
    BOOST_CHECK_THROW(opts->Validate(), CBlastException);
}

BOOST_AUTO_TEST_CASE(CheckCulling) {
    typedef vector< CRef<CBlastAppArgs> > TArgClasses;
    vector< CRef<CBlastAppArgs> > arg_classes;
    arg_classes.push_back(CRef<CBlastAppArgs>(new CPsiBlastAppArgs));
    arg_classes.push_back(CRef<CBlastAppArgs>(new CBlastpAppArgs));
    arg_classes.push_back(CRef<CBlastAppArgs>(new CBlastnAppArgs));
    arg_classes.push_back(CRef<CBlastAppArgs>(new CBlastxAppArgs));
    arg_classes.push_back(CRef<CBlastAppArgs>(new CTblastnAppArgs));
    arg_classes.push_back(CRef<CBlastAppArgs>(new CTblastxAppArgs));

    NON_CONST_ITERATE(TArgClasses, itr, arg_classes) {
        auto_ptr<CArgs> args;
        // invalid value
        CString2Args s2a("-db ecoli -culling_limit -4");
        BOOST_CHECK_THROW(args.reset(s2a.CreateCArgs(**itr)), 
                          CArgException);

        // valid combination
        s2a.Reset("-db ecoli -culling_limit 0");
        BOOST_CHECK_NO_THROW(args.reset(s2a.CreateCArgs(**itr)));
    }

}

BOOST_AUTO_TEST_CASE(CheckTaskArgs) {
    set<string> tasks
        (CBlastOptionsFactory::GetTasks(CBlastOptionsFactory::eNuclNucl));
    CRef<IBlastCmdLineArgs> arg;
    arg.Reset(new CTaskCmdLineArgs(tasks, "megablast")),
    arg.Reset(new CTaskCmdLineArgs(tasks, "dc-megablast")),
    arg.Reset(new CTaskCmdLineArgs(tasks, "blastn")),
    arg.Reset(new CTaskCmdLineArgs(tasks, "blastn-short")),

    tasks = CBlastOptionsFactory::GetTasks(CBlastOptionsFactory::eProtProt);
    arg.Reset(new CTaskCmdLineArgs(tasks, "blastp"));
    arg.Reset(new CTaskCmdLineArgs(tasks, "blastp-short"));
}

BOOST_AUTO_TEST_CASE(ReadSinglePdb)
{
    string pdb_mol("1QCF");
    string pdb_chain("A");
    string pdb(pdb_mol + pdb_chain);
    istringstream instream(pdb);
    
    const bool is_protein(true);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRetrieveSeqData(false);
    CRef<CBlastInput> source(s_DeclareBlastInput(instream, iconfig));
    CScope scope(*CObjectManager::GetInstance());
    
    CHECK(source->End() == false);
    blast::TSeqLocVector seqs = source->GetAllSeqLocs(scope);
    blast::SSeqLoc ssl = seqs.front();

    CHECK(source->End() == true);
    
    CHECK(ssl.seqloc->IsInt() == true);
    
    CHECK(ssl.seqloc->GetInt().IsSetStrand() == true);
    CHECK_EQUAL(eNa_strand_unknown, ssl.seqloc->GetInt().GetStrand());

    CHECK(ssl.seqloc->GetInt().IsSetFrom() == true);
    CHECK_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    CHECK(ssl.seqloc->GetInt().IsSetTo() == true);
    const TSeqPos length(454);
    CHECK_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

    CHECK(ssl.seqloc->GetInt().IsSetId() == true);
    CHECK_EQUAL(CSeq_id::e_Pdb, ssl.seqloc->GetInt().GetId().Which());
    
    CHECK_EQUAL(pdb_mol, ssl.seqloc->GetInt().GetId().GetPdb().GetMol().Get());
    
    CHECK(!ssl.mask);
    
    /// Validate the data that would be retrieved by blast.cgi
    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
    CHECK_EQUAL((size_t)1, bioseqs->GetSeq_set().size());
    CHECK(bioseqs->GetSeq_set().front()->IsSeq());
    const CBioseq& b = bioseqs->GetSeq_set().front()->GetSeq();
    CHECK(! b.IsNa());
    CHECK_EQUAL(CSeq_id::e_Pdb, b.GetId().front()->Which());
    CHECK_EQUAL(pdb_mol, b.GetId().front()->GetPdb().GetMol().Get());
    CHECK_EQUAL(CSeq_inst::eRepr_raw, b.GetInst().GetRepr());
    BOOST_CHECK(! CSeq_inst::IsNa(b.GetInst().GetMol()));
    CHECK_EQUAL(length, b.GetInst().GetLength());
}

BOOST_AUTO_TEST_CASE(ReadSinglePdb_InDifferentFormats)
{
    string pdb_mol("1IQR");
    string pdb_chain("A");

    for (int i = 0; i < 2; i++) {

        string pdb;
        if (i == 0) {
            pdb.assign(pdb_mol + pdb_chain);
        } else {
            pdb.assign(pdb_mol + "_" + pdb_chain);
        }
        istringstream instream(pdb);
        
        const bool is_protein(true);
        CBlastInputSourceConfig iconfig(is_protein);
        iconfig.SetRetrieveSeqData(false);
        CRef<CBlastInput> source(s_DeclareBlastInput(instream, iconfig));
        CScope scope(*CObjectManager::GetInstance());
        
        CHECK(source->End() == false);
        blast::TSeqLocVector seqs = source->GetAllSeqLocs(scope);
        blast::SSeqLoc ssl = seqs.front();
        CHECK(source->End() == true);
        
        CHECK(ssl.seqloc->IsInt() == true);
        
        CHECK(ssl.seqloc->GetInt().IsSetStrand() == true);
        CHECK_EQUAL(eNa_strand_unknown, ssl.seqloc->GetInt().GetStrand());

        CHECK(ssl.seqloc->GetInt().IsSetFrom() == true);
        CHECK_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

        CHECK(ssl.seqloc->GetInt().IsSetTo() == true);
        const TSeqPos length(420);
        CHECK_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

        CHECK(ssl.seqloc->GetInt().IsSetId() == true);
        CHECK_EQUAL(CSeq_id::e_Pdb, ssl.seqloc->GetInt().GetId().Which());
        
        CHECK_EQUAL(pdb_mol, 
                    ssl.seqloc->GetInt().GetId().GetPdb().GetMol().Get());
        
        CHECK(!ssl.mask);
        
        /// Validate the data that would be retrieved by blast.cgi
        CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
        CHECK_EQUAL((size_t)1, bioseqs->GetSeq_set().size());
        CHECK(bioseqs->GetSeq_set().front()->IsSeq());
        const CBioseq& b = bioseqs->GetSeq_set().front()->GetSeq();
        CHECK(! b.IsNa());
        CHECK_EQUAL(CSeq_id::e_Pdb, b.GetId().front()->Which());
        CHECK_EQUAL(pdb_mol, b.GetId().front()->GetPdb().GetMol().Get());
        CHECK_EQUAL(CSeq_inst::eRepr_raw, b.GetInst().GetRepr());
        BOOST_CHECK(! CSeq_inst::IsNa(b.GetInst().GetMol()));
        CHECK_EQUAL(length, b.GetInst().GetLength());
    }
}

#endif /* SKIP_DOXYGEN_PROCESSING */
