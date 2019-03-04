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
#include <objects/seqset/Bioseq_set.hpp>

#include <corelib/ncbienv.hpp>
#include <objtools/readers/reader_exception.hpp>
#include <algo/blast/api/sseqloc.hpp>
#include <algo/blast/core/blast_query_info.h>
#include <algo/blast/blastinput/blast_input.hpp>
#include <algo/blast/blastinput/blast_input_aux.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>
#include <algo/blast/blastinput/blast_asn1_input.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_vector.hpp>

#include <algo/blast/blastinput/blastp_args.hpp>
#include <algo/blast/blastinput/blastn_args.hpp>
#include <algo/blast/blastinput/blastx_args.hpp>
#include <algo/blast/blastinput/tblastn_args.hpp>
#include <algo/blast/blastinput/tblastx_args.hpp>
#include <algo/blast/blastinput/psiblast_args.hpp>
#include <algo/blast/blastinput/rpsblast_args.hpp>
#include "blast_input_unit_test_aux.hpp"

#include <unordered_map>

#undef NCBI_BOOST_NO_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);

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

BOOST_AUTO_TEST_SUITE(blastinput)

BOOST_AUTO_TEST_CASE(ReadAccession_MismatchNuclProt)
{
    CNcbiIfstream infile("data/nucl_acc.txt");
    const bool is_protein(true);
    CBlastInputSourceConfig iconfig(is_protein);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CScope scope(*CObjectManager::GetInstance());

    BOOST_REQUIRE(source->End() == false);
    bool caught_exception(false);
    try { 
        blast::SSeqLoc ssl = source->GetNextSeqLocBatch(scope).front(); 
        // here's a 'misplaced' test for blast::IsLocalId
        BOOST_REQUIRE(blast::IsLocalId(ssl.seqloc->GetId()) == false);
    }
    catch (const CInputException& e) {
        string msg(e.what());
        BOOST_REQUIRE(msg.find("GI/accession/sequence mismatch: protein input required but nucleotide provided")
                    != NPOS);
        BOOST_REQUIRE_EQUAL(CInputException::eSequenceMismatch, e.GetErrCode());
        caught_exception = true;
    }
    BOOST_REQUIRE(caught_exception);
    BOOST_REQUIRE(source->End() == true);
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(ReadAccession_MismatchProtNucl)
{
    CNcbiIfstream infile("data/prot_acc.txt");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CScope scope(*CObjectManager::GetInstance());

    BOOST_REQUIRE(source->End() == false);
    bool caught_exception(false);
    try { 
        blast::SSeqLoc ssl = source->GetNextSeqLocBatch(scope).front();
        // here's a 'misplaced' test for blast::IsLocalId
        BOOST_REQUIRE(blast::IsLocalId(ssl.seqloc->GetId()) == false);
    }
    catch (const CInputException& e) {
        string msg(e.what());
        BOOST_REQUIRE(msg.find("GI/accession/sequence mismatch: nucleotide input required but protein provided")
                    != NPOS);
        BOOST_REQUIRE_EQUAL(CInputException::eSequenceMismatch, e.GetErrCode());
        caught_exception = true;
    }
    BOOST_REQUIRE(caught_exception);
    BOOST_REQUIRE(source->End() == true);
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(ReadGi_MismatchNuclProt)
{
    CNcbiIfstream infile("data/gi.txt");
    const bool is_protein(true);
    CBlastInputSourceConfig iconfig(is_protein);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CScope scope(*CObjectManager::GetInstance());

    BOOST_REQUIRE(source->End() == false);
    bool caught_exception(false);
    try { 
        blast::SSeqLoc ssl = source->GetNextSeqLocBatch(scope).front();
        // here's a 'misplaced' test for blast::IsLocalId
        BOOST_REQUIRE(blast::IsLocalId(ssl.seqloc->GetId()) == false);
    }
    catch (const CInputException& e) {
        string msg(e.what());
        BOOST_REQUIRE(msg.find("GI/accession/sequence mismatch: protein input required but nucleotide provided")
                    != NPOS);
        BOOST_REQUIRE_EQUAL(CInputException::eSequenceMismatch, e.GetErrCode());
        caught_exception = true;
    }
    BOOST_REQUIRE(caught_exception);
    BOOST_REQUIRE(source->End() == true);
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(ReadGi_MismatchProtNucl)
{
    CNcbiIfstream infile("data/prot_gi.txt");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CScope scope(*CObjectManager::GetInstance());

    BOOST_REQUIRE(source->End() == false);
    bool caught_exception(false);
    try { 
        blast::SSeqLoc ssl = source->GetNextSeqLocBatch(scope).front();
        // here's a 'misplaced' test for blast::IsLocalId
        BOOST_REQUIRE(blast::IsLocalId(ssl.seqloc->GetId()) == false);
    }
    catch (const CInputException& e) {
        string msg(e.what());
        BOOST_REQUIRE(msg.find("GI/accession/sequence mismatch: nucleotide input required but protein provided")
                    != NPOS);
        BOOST_REQUIRE_EQUAL(CInputException::eSequenceMismatch, e.GetErrCode());
        caught_exception = true;
    }
    BOOST_REQUIRE(caught_exception);
    BOOST_REQUIRE(source->End() == true);
    scope.GetObjectManager().RevokeAllDataLoaders();        
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
        BOOST_REQUIRE(source->End() == false);
        bool caught_exception(false);
        blast::SSeqLoc ssl;
        try { 
            ssl = source->GetNextSeqLocBatch(scope).front();
            // here's a 'misplaced' test for blast::IsLocalId
            BOOST_REQUIRE(blast::IsLocalId(ssl.seqloc->GetId()) == true);
        }
        catch (const CInputException& e) {
            string msg(e.what());
            BOOST_REQUIRE(msg.find("Gi/accession mismatch: ") != NPOS);
            BOOST_REQUIRE_EQUAL(CInputException::eSequenceMismatch, 
                              e.GetErrCode());
            caught_exception = true;
        }
        BOOST_REQUIRE(caught_exception == false);
        BOOST_REQUIRE(source->End() == true);

        TSeqPos length = sequence::GetLength(*ssl.seqloc, ssl.scope);
        BOOST_REQUIRE_EQUAL(itr->GetLength(), length);
        scope.GetObjectManager().RevokeAllDataLoaders();        
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
        BOOST_REQUIRE(source->End() == false);
        bool caught_exception(false);
        blast::SSeqLoc ssl;
        try { 
            ssl = source->GetNextSeqLocBatch(scope).front();
            // here's a 'misplaced' test for blast::IsLocalId
            BOOST_REQUIRE(blast::IsLocalId(ssl.seqloc->GetId()) == true);
        }
        catch (const CInputException& e) {
            string msg(e.what());
            BOOST_REQUIRE(msg.find("Nucleotide FASTA provided for prot") != NPOS);
            BOOST_REQUIRE_EQUAL(CInputException::eSequenceMismatch, 
                              e.GetErrCode());
            caught_exception = true;
        }
        BOOST_REQUIRE(caught_exception == true);
        BOOST_REQUIRE(source->End() == true);
        scope.GetObjectManager().RevokeAllDataLoaders();        
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
    BOOST_REQUIRE(source->End() == false);
    bool caught_exception(false);
    try { blast::SSeqLoc ssl = source->GetNextSeqLocBatch(scope).front(); }
    catch (const CInputException& e) {
        string msg(e.what());
        BOOST_REQUIRE(msg.find("Protein FASTA provided for nucleotide") != NPOS);
        BOOST_REQUIRE_EQUAL(CInputException::eSequenceMismatch, e.GetErrCode());
        caught_exception = true;
    }
    BOOST_REQUIRE(caught_exception);
    BOOST_REQUIRE(source->End() == true);
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(ReadFastaWithDefline_MismatchNuclProt)
{
    CNcbiIfstream infile("data/nt.555");
    const bool is_protein(true);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetSeqLenThreshold2Guess(25);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    CScope scope(*CObjectManager::GetInstance());
    BOOST_REQUIRE(source->End() == false);
    bool caught_exception(false);
    try { blast::SSeqLoc ssl = source->GetNextSeqLocBatch(scope).front(); }
    catch (const CInputException& e) {
        string msg(e.what());
        BOOST_REQUIRE(msg.find("Nucleotide FASTA provided for protein") != NPOS);
        BOOST_REQUIRE_EQUAL(CInputException::eSequenceMismatch, e.GetErrCode());
        caught_exception = true;
    }
    BOOST_REQUIRE(caught_exception);
    BOOST_REQUIRE(source->End() == true);
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(ReadFastaWithDeflineProtein_Single)
{
    CNcbiIfstream infile("data/aa.129295");
    const bool is_protein(true);
    CBlastInputSourceConfig iconfig(is_protein);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    CScope scope(*CObjectManager::GetInstance());
    BOOST_REQUIRE(source->End() == false);
    blast::SSeqLoc ssl = source->GetNextSeqLocBatch(scope).front();
    BOOST_REQUIRE(source->End() == true);

    BOOST_REQUIRE(ssl.seqloc->IsInt() == true);
    BOOST_REQUIRE(blast::IsLocalId(ssl.seqloc->GetId()) == true);

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetStrand() == true);
    BOOST_REQUIRE_EQUAL((int)eNa_strand_unknown, (int)ssl.seqloc->GetInt().GetStrand());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetFrom() == true);
    BOOST_REQUIRE_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetTo() == true);
    const TSeqPos length(232);
    BOOST_REQUIRE_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetId() == true);
    BOOST_REQUIRE_EQUAL(CSeq_id::e_Local, ssl.seqloc->GetInt().GetId().Which());

    BOOST_REQUIRE(!ssl.mask);
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(RawFastaWithSpaces)
{
    // this is gi 555, length 624
    CNcbiIfstream infile("data/raw_fasta.na");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    CScope scope(*CObjectManager::GetInstance());
    BOOST_REQUIRE(source->End() == false);
    blast::SSeqLoc ssl = source->GetNextSeqLocBatch(scope).front();
    BOOST_REQUIRE(source->End() == true);

    BOOST_REQUIRE(ssl.seqloc->IsInt() == true);
    BOOST_REQUIRE(blast::IsLocalId(ssl.seqloc->GetId()) == true);

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetStrand() == true);
    BOOST_REQUIRE_EQUAL((int)eNa_strand_both, (int)ssl.seqloc->GetInt().GetStrand());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetFrom() == true);
    BOOST_REQUIRE_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetTo() == true);
    const TSeqPos length(624);
    BOOST_REQUIRE_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetId() == true);
    BOOST_REQUIRE_EQUAL(CSeq_id::e_Local, ssl.seqloc->GetInt().GetId().Which());

    BOOST_REQUIRE(!ssl.mask);
    scope.GetObjectManager().RevokeAllDataLoaders();        
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
    BOOST_REQUIRE(source->End() == true);

    BOOST_REQUIRE(ssl.seqloc->IsInt() == true);
    BOOST_REQUIRE(blast::IsLocalId(ssl.seqloc->GetId()) == true);

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetFrom() == true);
    BOOST_REQUIRE_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetTo() == true);
    const TSeqPos length(91); // it's actually 103 with gaps
    BOOST_REQUIRE_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

    const CSeq_id * seqid = ssl.seqloc->GetId();
    CBioseq_Handle bh = scope.GetBioseqHandle(*seqid);
    CSeqVector sv = bh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);

    for (size_t i = 0; i < sv.size(); i++) {
        BOOST_CHECK_NE('-', (char)sv[i]);
    }

    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
    const CBioseq& bioseq = bioseqs->GetSeq_set().front()->GetSeq();
    const CSeq_inst& inst = bioseq.GetInst();
    BOOST_REQUIRE_EQUAL(inst.GetLength(), length);
    BOOST_REQUIRE(inst.IsSetSeq_data());
    const CSeq_data& seq_data = inst.GetSeq_data();
    BOOST_REQUIRE(seq_data.IsNcbieaa());
    const string& seq = seq_data.GetNcbieaa().Get();
    for (size_t i = 0; i < seq.size(); i++) {
        BOOST_CHECK_NE('-', (char)seq[i]);
    }
    scope.GetObjectManager().RevokeAllDataLoaders();        
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
    BOOST_REQUIRE(source->End() == true);

    BOOST_REQUIRE(ssl.seqloc->IsInt() == true);
    BOOST_REQUIRE(blast::IsLocalId(ssl.seqloc->GetId()) == true);

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetStrand() == true);
    BOOST_REQUIRE_EQUAL((int)eNa_strand_both, (int)ssl.seqloc->GetInt().GetStrand());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetFrom() == true);
    BOOST_REQUIRE_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetTo() == true);
    const TSeqPos length(624);
    BOOST_REQUIRE_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetId() == true);
    BOOST_REQUIRE_EQUAL(CSeq_id::e_Local, ssl.seqloc->GetInt().GetId().Which());

    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
    BOOST_REQUIRE(bioseqs.NotEmpty());

    BOOST_REQUIRE(!ssl.mask);
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

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
    BOOST_REQUIRE(source->End() == true);

    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
    BOOST_REQUIRE(bioseqs->CanGetSeq_set());
    BOOST_REQUIRE(bioseqs->GetSeq_set().front()->IsSeq());
    BOOST_REQUIRE(bioseqs->GetSeq_set().front()->GetSeq().CanGetInst());
    BOOST_REQUIRE(bioseqs->GetSeq_set().front()->GetSeq().GetInst().CanGetRepr());
    BOOST_REQUIRE(bioseqs->GetSeq_set().front()->GetSeq().GetInst().GetRepr() 
          == CSeq_inst::eRepr_delta);
    s.GetObjectManager().RevokeAllDataLoaders();        
}


BOOST_AUTO_TEST_CASE(ReadGenbankReport)
{
    CDiagRestorer diag_restorer;

    // Redirect the output warnings
    SetDiagPostLevel(eDiag_Warning);
    CNcbiOstrstream error_stream;
    SetDiagStream(&error_stream);

    // this is gi 555, length 624
    CNcbiIfstream infile("data/gbreport.txt");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    CScope scope(*CObjectManager::GetInstance());
    BOOST_REQUIRE(source->End() == false);
    blast::SSeqLoc ssl = source->GetNextSeqLocBatch(scope).front();
    BOOST_REQUIRE(source->End() == true);

    string s = CNcbiOstrstreamToString(error_stream);
    BOOST_REQUIRE(s.find("Ignoring invalid residues at ") != NPOS);

    BOOST_REQUIRE(ssl.seqloc->IsInt() == true);
    BOOST_REQUIRE(blast::IsLocalId(ssl.seqloc->GetId()) == true);

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetStrand() == true);
    BOOST_REQUIRE_EQUAL((int)eNa_strand_both, (int)ssl.seqloc->GetInt().GetStrand());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetFrom() == true);
    BOOST_REQUIRE_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetTo() == true);
    const TSeqPos length(624);
    BOOST_REQUIRE_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetId() == true);
    BOOST_REQUIRE_EQUAL(CSeq_id::e_Local, ssl.seqloc->GetInt().GetId().Which());

    BOOST_REQUIRE(!ssl.mask);
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(ReadInvalidGi)
{
    const char* fname = "data/invalid_gi.txt";
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);

    CNcbiIfstream infile(fname);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    BOOST_REQUIRE(source->End() == false);

    CScope scope(*CObjectManager::GetInstance());
    blast::SSeqLoc ssl;
    bool caught_exception(false);
    try { ssl = source->GetNextSeqLocBatch(scope).front(); }
    catch (const CInputException& e) {
        string msg(e.what());
        BOOST_REQUIRE(msg.find("Sequence ID not found: ") != NPOS);
        BOOST_REQUIRE_EQUAL(CInputException::eSeqIdNotFound, e.GetErrCode());
        caught_exception = true;
    }
    BOOST_REQUIRE(caught_exception);
    BOOST_REQUIRE(source->End() == true);
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(ReadInvalidSeqId)
{
    const char* fname = "data/bad_seqid.txt";
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);

    CNcbiIfstream infile(fname);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    BOOST_REQUIRE(source->End() == false);

    CScope scope(*CObjectManager::GetInstance());
    blast::SSeqLoc ssl;
    bool caught_exception(false);
    try { ssl = source->GetNextSeqLocBatch(scope).front(); }
    catch (const CSeqIdException& e) {
        string msg(e.what());
        BOOST_REQUIRE_EQUAL(CSeqIdException::eFormat, e.GetErrCode());
        caught_exception = true;
    }
    BOOST_REQUIRE(caught_exception);
    BOOST_REQUIRE(source->End() == true);
    scope.GetObjectManager().RevokeAllDataLoaders();        
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
        BOOST_REQUIRE(source->End() == false);

        blast::TSeqLocVector query_vector;
        BOOST_REQUIRE_THROW(query_vector = source->GetAllSeqLocs(scope),
                            CObjReaderParseException);
        BOOST_REQUIRE_EQUAL(kNumQueries, query_vector.size());
        BOOST_REQUIRE_EQUAL(kNumQueries, query_vector.size());

        CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(query_vector);
        BOOST_REQUIRE(bioseqs.Empty());
    }

    {
        CNcbiIfstream infile(fname);
        CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
        BOOST_REQUIRE(source->End() == false);

        CRef<blast::CBlastQueryVector> query_vector;
        BOOST_REQUIRE_THROW(query_vector = source->GetAllSeqs(scope), 
                            CObjReaderParseException);
        BOOST_REQUIRE(query_vector.Empty());
    }
    scope.GetObjectManager().RevokeAllDataLoaders();        
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
    BOOST_REQUIRE(source->End() == false);

    CScope scope(*CObjectManager::GetInstance());

    blast::TSeqLocVector seqs;
    BOOST_REQUIRE_THROW(seqs = source->GetAllSeqLocs(scope),
                        CObjReaderParseException);
    scope.GetObjectManager().RevokeAllDataLoaders();        
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
        BOOST_REQUIRE(source->End() == true);

        blast::TSeqLocVector query_vector = source->GetAllSeqLocs(scope);
        BOOST_REQUIRE(query_vector.empty());

        CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(query_vector);
        BOOST_REQUIRE(bioseqs.Empty());
    }

    {
        CNcbiIfstream infile(fname);
        CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
        BOOST_REQUIRE(source->End() == true);

        CRef<blast::CBlastQueryVector> queries = source->GetAllSeqs(scope);
        BOOST_REQUIRE(queries->Empty());
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
            BOOST_REQUIRE(msg.find("No sequence input was provided") != NPOS);
            BOOST_REQUIRE_EQUAL(CInputException::eEmptyUserInput, e.GetErrCode());
            caught_exception = true;
        }
        BOOST_REQUIRE(caught_exception);
    }
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

// Basic test case to ensure CFastaReader changes don't break basic
// functionality required by BLAST 
BOOST_AUTO_TEST_CASE(ReadSingleFasta_WithTitle)
{
    const string kFileName("data/isprot.fa");
    const string kExpectedTitle("seq");
    const bool is_protein(false);

    CScope scope(*CObjectManager::GetInstance());
    CBlastInputSourceConfig iconfig(is_protein);

    CNcbiIfstream infile(kFileName.c_str());
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    blast::TSeqLocVector query_vector = source->GetAllSeqLocs(scope); 
    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(query_vector);
    BOOST_REQUIRE(!bioseqs.Empty());

    string title;
    ITERATE(CBioseq::TDescr::Tdata, itr, bioseqs->GetSeq_set().front()->GetSeq().GetDescr().Get()) {
        const CSeqdesc& desc = **itr;
        if (desc.IsTitle()) {
            title = desc.GetTitle();
            break;
        }
    }
    BOOST_REQUIRE_EQUAL(kExpectedTitle, title);
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

static
void s_ReadAndTestQueryFromString_CFastaReader(const string& input, 
                                               TSeqPos expected_length)
{
    CFastaReader::TFlags defaultBLASTflags = CFastaReader::fNoParseID |
                                             CFastaReader::fDLOptional;
    defaultBLASTflags += CFastaReader::fAssumeNuc;
    defaultBLASTflags += CFastaReader::fNoSplit;
    defaultBLASTflags += CFastaReader::fHyphensIgnoreAndWarn;
    defaultBLASTflags += CFastaReader::fDisableNoResidues;
    defaultBLASTflags += CFastaReader::fQuickIDCheck;

    CRef<ILineReader> line_reader(new CMemoryLineReader(input.c_str(),
                                                        input.size()));
    CFastaReader fasta_reader(*line_reader, defaultBLASTflags);
    fasta_reader.IgnoreProblem(ILineError::eProblem_ModifierFoundButNoneExpected);
    fasta_reader.IgnoreProblem(ILineError::eProblem_TooLong);

    CRef<CSeqIdGenerator> idgen(new CSeqIdGenerator(1, kEmptyStr));
    fasta_reader.SetIDGenerator(*idgen);

    CRef<CSeq_entry> se(fasta_reader.ReadOneSeq());
    BOOST_REQUIRE_EQUAL(expected_length, se->GetSeq().GetLength());
}

BOOST_AUTO_TEST_CASE(SingleSequenceString_CFastaReaderNoNewLineAfterSeq)
{
    const string kUserInput(">seq_1\nATGC");
    const TSeqPos kExpectedLength(4);
    s_ReadAndTestQueryFromString_CFastaReader(kUserInput, kExpectedLength);
}
BOOST_AUTO_TEST_CASE(SingleSequenceString_CFastaReaderWithNewLines)
{
    const string kUserInput(">seq_1\nATGC\n");
    const TSeqPos kExpectedLength(4);
    s_ReadAndTestQueryFromString_CFastaReader(kUserInput, kExpectedLength);
}
BOOST_AUTO_TEST_CASE(SingleSequenceString_CFastaReaderNoDeflineNoNewLines)
{
    const string kUserInput("ATGC");
    const TSeqPos kExpectedLength(4);
    s_ReadAndTestQueryFromString_CFastaReader(kUserInput, kExpectedLength);
}

static 
void s_ReadAndTestQueryFromString(const string& input, TSeqPos expected_length, 
                                  bool is_protein)
{
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    BOOST_REQUIRE(objmgr);

    SDataLoaderConfig dlconfig(is_protein);
    CBlastInputSourceConfig iconfig(dlconfig);
    CBlastFastaInputSource queryInput(input, iconfig);
    CScope scope(*objmgr);
    CBlastInput qIn(&queryInput);
    blast::TSeqLocVector query = qIn.GetAllSeqLocs(scope);
    BOOST_REQUIRE_EQUAL(expected_length, 
                        sequence::GetLength(*query.front().seqloc, &scope));
    CRef<CSeqVector> sv(new CSeqVector(*query.front().seqloc, scope));
    BOOST_REQUIRE_EQUAL(expected_length, sv->size());
    BOOST_REQUIRE_EQUAL(is_protein, sv->IsProtein());
    sv->SetIupacCoding();
    string::size_type input_pos = input.find_first_of("ACTG");
    BOOST_REQUIRE(input_pos != string::npos);
    for (TSeqPos pos = 0; pos < sv->size(); pos++, input_pos++) {
        CNcbiOstrstream oss;
        oss << "Sequence data differs at position " << pos << ": '"
            << input[input_pos] << "' .vs '" << (*sv)[pos] << "'";
        string msg = CNcbiOstrstreamToString(oss);
        BOOST_REQUIRE_MESSAGE(input[input_pos] == (*sv)[pos],  msg);
    }
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(SingleSequenceString_NoNewLineAfterSeq)
{
    const string kUserInput(">seq_1\nATGC");
    const TSeqPos kExpectedLength(4);
    s_ReadAndTestQueryFromString(kUserInput, kExpectedLength, false);
}

BOOST_AUTO_TEST_CASE(SingleSequenceString_WithNewLines)
{
    const string kUserInput(">seq_1\nATGC\n");
    const TSeqPos kExpectedLength(4);
    s_ReadAndTestQueryFromString(kUserInput, kExpectedLength, false);
}

BOOST_AUTO_TEST_CASE(SingleSequenceString_NoDeflineNoNewLines)
{
    const string kUserInput("ATGC");
    const TSeqPos kExpectedLength(4);
    s_ReadAndTestQueryFromString(kUserInput, kExpectedLength, false);
}

BOOST_AUTO_TEST_CASE(ReadEmptyUserInput_OnlyTitle)
{
    CTmpFile tmpfile;
    const string kUserInput(">mygene\n");
    CNcbiOfstream out(tmpfile.GetFileName().c_str());
    out << kUserInput;
    out.close();
   

    const bool is_protein(false);
    CScope scope(*CObjectManager::GetInstance());
    CBlastInputSourceConfig iconfig(is_protein);
    bool caught_exception(false);
    string warnings;
    {
        CNcbiIfstream infile(tmpfile.GetFileName().c_str());
        CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

        blast::TSeqLocVector query_vector;
        try { CheckForEmptySequences(query_vector, warnings); }
        catch (const CInputException& e) {
            BOOST_REQUIRE_EQUAL(CInputException::eEmptyUserInput, e.GetErrCode());
        }

        query_vector = source->GetAllSeqLocs(scope); 
        try { CheckForEmptySequences(query_vector, warnings); }
        catch (const CInputException& e) {
            string msg(e.what());
            BOOST_REQUIRE(msg.find("Query contains no sequence data") != NPOS);
            BOOST_REQUIRE(warnings.empty());
            BOOST_REQUIRE_EQUAL(CInputException::eEmptyUserInput, e.GetErrCode());
            caught_exception = true;
        }
        BOOST_REQUIRE(caught_exception);
        BOOST_REQUIRE(query_vector.empty() == false);

        CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(query_vector);
        BOOST_REQUIRE(!bioseqs.Empty());
        caught_exception = false;
        try { CheckForEmptySequences(bioseqs, warnings); }
        catch (const CInputException& e) {
            string msg(e.what());
            BOOST_REQUIRE(msg.find("Query contains no sequence data") != NPOS);
            BOOST_REQUIRE(warnings.empty());
            BOOST_REQUIRE_EQUAL(CInputException::eEmptyUserInput, e.GetErrCode());
            caught_exception = true;
        }
        BOOST_REQUIRE(caught_exception);
    }

    {
        CNcbiIfstream infile(tmpfile.GetFileName().c_str());
        CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

        caught_exception = false;
        CRef<blast::CBlastQueryVector> queries = source->GetAllSeqs(scope); 
        try { CheckForEmptySequences(queries, warnings); }
        catch (const CInputException& e) {
            string msg(e.what());
            BOOST_REQUIRE(msg.find("Query contains no sequence data") != NPOS);
            BOOST_REQUIRE(warnings.empty());
            BOOST_REQUIRE_EQUAL(CInputException::eEmptyUserInput, e.GetErrCode());
            caught_exception = true;
        }
        BOOST_REQUIRE(caught_exception);
        BOOST_REQUIRE(!queries.Empty());
    }

    // Read from buffer
    {
        const string empty;
        CRef<CObjectManager> om(CObjectManager::GetInstance());
        CRef<CBlastInput> source(s_DeclareBlastInput(kUserInput, iconfig));
        CRef<blast::CBlastQueryVector> queries;
        try { CheckForEmptySequences(queries, warnings); }
        catch (const CInputException& e) {
            BOOST_REQUIRE_EQUAL(CInputException::eEmptyUserInput, e.GetErrCode());
        }
        
        caught_exception = false;
        queries = source->GetAllSeqs(scope);
        try { CheckForEmptySequences(queries, warnings); }
        catch (const CInputException& e) {
            string msg(e.what());
            BOOST_REQUIRE(msg.find("Query contains no sequence data") != NPOS);
            BOOST_REQUIRE(warnings.empty());
            BOOST_REQUIRE_EQUAL(CInputException::eEmptyUserInput, e.GetErrCode());
            caught_exception = true;
        }
        BOOST_REQUIRE(caught_exception);
    }
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(ReadSingleAccession)
{
    CNcbiIfstream infile("data/accession.txt");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRetrieveSeqData(false);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    CScope scope(*CObjectManager::GetInstance());
    BOOST_REQUIRE(source->End() == false);
    blast::TSeqLocVector seqs = source->GetAllSeqLocs(scope);
    blast::SSeqLoc ssl = seqs.front();
    BOOST_REQUIRE(source->End() == true);

    BOOST_REQUIRE(ssl.seqloc->IsInt() == true);
    BOOST_REQUIRE(blast::IsLocalId(ssl.seqloc->GetId()) == false);

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetStrand() == true);
    BOOST_REQUIRE_EQUAL((int)eNa_strand_both, (int)ssl.seqloc->GetInt().GetStrand());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetFrom() == true);
    BOOST_REQUIRE_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetTo() == true);
    const TSeqPos length(248956422);
    BOOST_REQUIRE_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetId() == true);
    BOOST_REQUIRE_EQUAL(CSeq_id::e_Other, ssl.seqloc->GetInt().GetId().Which());
    const string accession("NC_000001");
    BOOST_REQUIRE_EQUAL(accession,
                ssl.seqloc->GetInt().GetId().GetOther().GetAccession());

    BOOST_REQUIRE(!ssl.mask);

    /// Validate the data that would be retrieved by blast.cgi
    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
    BOOST_REQUIRE_EQUAL((size_t)1, bioseqs->GetSeq_set().size());
    BOOST_REQUIRE(bioseqs->GetSeq_set().front()->IsSeq());
    const CBioseq& b = bioseqs->GetSeq_set().front()->GetSeq();
    BOOST_REQUIRE(b.IsNa());
    BOOST_REQUIRE_EQUAL(CSeq_id::e_Other, b.GetId().front()->Which());
    BOOST_REQUIRE_EQUAL(accession, b.GetId().front()->GetOther().GetAccession());
    BOOST_REQUIRE_EQUAL(CSeq_inst::eRepr_raw, b.GetInst().GetRepr());
    BOOST_REQUIRE(CSeq_inst::IsNa(b.GetInst().GetMol()));
    BOOST_REQUIRE_EQUAL(length, b.GetInst().GetLength());
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(ReadSingleAccession_RetrieveLargeSequence)
{
    CNcbiIfstream infile("data/accession.txt");
    const bool is_protein(false);
    const TIntId kGi = 568815597;
    const TSeqPos kStart = 0;
    const TSeqPos kStop(248956421);
    SDataLoaderConfig dlconfig("chromosome", is_protein);
    dlconfig.OptimizeForWholeLargeSequenceRetrieval(true);

    CBlastInputSourceConfig iconfig(dlconfig);
    iconfig.SetRetrieveSeqData(true);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    CRef<CScope> scope(CBlastScopeSource(dlconfig).NewScope());
    BOOST_REQUIRE(source->End() == false);

    blast::TSeqLocVector seqs = source->GetAllSeqLocs(*scope);
    blast::SSeqLoc ssl = seqs.front();
    BOOST_REQUIRE(source->End() == true);

    BOOST_REQUIRE(ssl.seqloc->IsInt() == true);
    BOOST_REQUIRE(blast::IsLocalId(ssl.seqloc->GetId()) == false);

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetStrand() == true);
    BOOST_REQUIRE_EQUAL((int)eNa_strand_both, (int)ssl.seqloc->GetInt().GetStrand());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetFrom() == true);
    BOOST_REQUIRE_EQUAL(kStart, ssl.seqloc->GetInt().GetFrom());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetTo() == true);
    BOOST_REQUIRE_EQUAL(kStop, ssl.seqloc->GetInt().GetTo());

    const string accession = "NC_000001";
    const int version = 11;
    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetId() == true);
    if ( !CSeq_id::PreferAccessionOverGi() ) {
        BOOST_REQUIRE_EQUAL(CSeq_id::e_Gi, ssl.seqloc->GetInt().GetId().Which());
        BOOST_REQUIRE_EQUAL(GI_CONST(kGi), ssl.seqloc->GetInt().GetId().GetGi());
    }
    else {
        BOOST_REQUIRE_EQUAL(CSeq_id::e_Other, ssl.seqloc->GetInt().GetId().Which());
        BOOST_REQUIRE_EQUAL(accession, ssl.seqloc->GetInt().GetId().GetOther().GetAccession());
        BOOST_REQUIRE_EQUAL(version, ssl.seqloc->GetInt().GetId().GetOther().GetVersion());
    }

    BOOST_REQUIRE(!ssl.mask);

    /// Validate the data that would be retrieved by a BLAST command line
    /// binary
    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
    BOOST_REQUIRE_EQUAL((size_t)1, bioseqs->GetSeq_set().size());
    BOOST_REQUIRE(bioseqs->GetSeq_set().front()->IsSeq());
    const CBioseq& b = bioseqs->GetSeq_set().front()->GetSeq();
    BOOST_REQUIRE(b.IsNa());
    bool found_gi = false, found_accession = false;
    ITERATE(CBioseq::TId, id, b.GetId()) {
        if ((*id)->Which() == CSeq_id::e_Gi) {
            BOOST_REQUIRE_EQUAL(GI_CONST(kGi), (*id)->GetGi());
            found_gi = true;
        } else if ((*id)->Which() == CSeq_id::e_Other) {
            CNcbiOstrstream os;
            (*id)->GetOther().AsFastaString(os);
            const string fasta_acc = CNcbiOstrstreamToString(os);
            BOOST_REQUIRE(NStr::Find(fasta_acc, accession) != NPOS);
            found_accession = true;
        }
    }
    BOOST_REQUIRE(found_gi);
    BOOST_REQUIRE(found_accession);
    // the BLAST database data loader will fetch this as a delta sequence
    BOOST_REQUIRE_EQUAL(CSeq_inst::eRepr_delta, b.GetInst().GetRepr());
    BOOST_REQUIRE(CSeq_inst::IsNa(b.GetInst().GetMol()));
    BOOST_REQUIRE_EQUAL(kStop+1, b.GetInst().GetLength());
    scope->GetObjectManager().RevokeAllDataLoaders();        
}
#ifdef _DEBUG
const int kTimeOutLargeSeq = 60;
#else
const int kTimeOutLargeSeq = 20;
#endif
BOOST_AUTO_TEST_CASE_TIMEOUT(ReadSingleAccession_RetrieveLargeSequence,
                             kTimeOutLargeSeq);

BOOST_AUTO_TEST_CASE(ReadSingleAccession_RetrieveLargeSequenceWithRange)
{
    CNcbiIfstream infile("data/accession.txt");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    const TSeqPos kStart = 1;
    const TSeqPos kStop = 1000;
    iconfig.SetRange().SetFrom(kStart);
    iconfig.SetRange().SetTo(kStop);
    // comment the line below to fetch the sequence data
    iconfig.SetRetrieveSeqData(false);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    SDataLoaderConfig dlconfig(is_protein);
    CRef<CScope> scope(CBlastScopeSource(dlconfig).NewScope());
    BOOST_REQUIRE(source->End() == false);
    blast::TSeqLocVector seqs = source->GetAllSeqLocs(*scope);
    blast::SSeqLoc ssl = seqs.front();
    BOOST_REQUIRE(source->End() == true);

    BOOST_REQUIRE(ssl.seqloc->IsInt() == true);
    BOOST_REQUIRE(blast::IsLocalId(ssl.seqloc->GetId()) == false);

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetStrand() == true);
    BOOST_REQUIRE_EQUAL((int)eNa_strand_both, (int)ssl.seqloc->GetInt().GetStrand());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetFrom() == true);
    BOOST_REQUIRE_EQUAL(kStart, ssl.seqloc->GetInt().GetFrom());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetTo() == true);
    BOOST_REQUIRE_EQUAL(kStop, ssl.seqloc->GetInt().GetTo());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetId() == true);
    BOOST_REQUIRE_EQUAL(CSeq_id::e_Other, ssl.seqloc->GetInt().GetId().Which());
    const string accession("NC_000001");
    BOOST_REQUIRE_EQUAL(accession,
                ssl.seqloc->GetInt().GetId().GetOther().GetAccession());
    BOOST_REQUIRE(!ssl.mask);

    /// Validate the data that would be retrieved by blast.cgi
    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
    BOOST_REQUIRE_EQUAL((size_t)1, bioseqs->GetSeq_set().size());
    BOOST_REQUIRE(bioseqs->GetSeq_set().front()->IsSeq());
    const CBioseq& b = bioseqs->GetSeq_set().front()->GetSeq();
    BOOST_REQUIRE(b.IsNa());
    BOOST_REQUIRE_EQUAL(CSeq_id::e_Other, b.GetId().front()->Which());
    BOOST_REQUIRE_EQUAL(accession, b.GetId().front()->GetOther().GetAccession());
    BOOST_REQUIRE_EQUAL(CSeq_inst::eRepr_raw, b.GetInst().GetRepr());
    BOOST_REQUIRE(CSeq_inst::IsNa(b.GetInst().GetMol()));
    const TSeqPos length(248956422);
    BOOST_REQUIRE_EQUAL(length, b.GetInst().GetLength());
    scope->GetObjectManager().RevokeAllDataLoaders();        
}
#ifdef _DEBUG
const int kTimeOutLargeSeqWithRange = 60;
#else 
const int kTimeOutLargeSeqWithRange = 15;
#endif
BOOST_AUTO_TEST_CASE_TIMEOUT(ReadSingleAccession_RetrieveLargeSequenceWithRange,
                             kTimeOutLargeSeqWithRange);

BOOST_AUTO_TEST_CASE(ReadMultipleAccessions)
{
    CNcbiIfstream infile("data/accessions.txt");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRetrieveSeqData(false);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    vector< pair<string, long> > accession_lengths;
    accession_lengths.push_back(make_pair(string("NC_000001"), 248956422L));
    accession_lengths.push_back(make_pair(string("NC_000010.9"), 135374737L));
    accession_lengths.push_back(make_pair(string("NC_000011.8"), 134452384L));
    accession_lengths.push_back(make_pair(string("NC_000012.10"), 132349534L));

    const size_t kNumQueries(accession_lengths.size());
    CScope scope(*CObjectManager::GetInstance());
    blast::TSeqLocVector query_vector = source->GetAllSeqLocs(scope);
    BOOST_REQUIRE_EQUAL(kNumQueries, query_vector.size());
    BOOST_REQUIRE(source->End() == true);

    {{
        blast::TSeqLocVector cached_queries = source->GetAllSeqLocs(scope);
        BOOST_REQUIRE_EQUAL((size_t)0, (size_t)cached_queries.size());
        BOOST_REQUIRE(source->End() == true);
    }}

    for (size_t i = 0; i < kNumQueries; i++) {

        blast::SSeqLoc& ssl = query_vector[i];
        BOOST_REQUIRE_EQUAL((int)eNa_strand_both, (int)ssl.seqloc->GetStrand());
        BOOST_REQUIRE_EQUAL((TSeqPos)accession_lengths[i].second - 1, 
                    ssl.seqloc->GetInt().GetTo());

        BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetId() == true);
        BOOST_REQUIRE(blast::IsLocalId(ssl.seqloc->GetId()) == false);
        BOOST_REQUIRE_EQUAL(CSeq_id::e_Other, ssl.seqloc->GetInt().GetId().Which());
        string accession;
        int version;
        switch (i) {
        case 0: accession.assign("NC_000001"); version = 0; break;
        case 1: accession.assign("NC_000010"); version = 9; break;
        case 2: accession.assign("NC_000011"); version = 8; break;
        case 3: accession.assign("NC_000012"); version = 10; break;
        default: abort();
        }

        BOOST_REQUIRE_EQUAL(accession,
                    ssl.seqloc->GetInt().GetId().GetOther().GetAccession());
        if (version != 0) {
            BOOST_REQUIRE_EQUAL(version, 
                        ssl.seqloc->GetInt().GetId().GetOther().GetVersion());
        }
        BOOST_REQUIRE(!ssl.mask);

    }

    /// Validate the data that would be retrieved by blast.cgi
    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(query_vector);
    BOOST_REQUIRE_EQUAL(kNumQueries, bioseqs->GetSeq_set().size());
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

// This test was created to test issues in jira/browse/CXX-82
BOOST_AUTO_TEST_CASE(ReadMultipleAccessionsFromMemory)
{
    typedef vector< pair<string, int> > TStringIntVector;
    TStringIntVector accession_lengths;
    accession_lengths.push_back(make_pair(string("P01012.2"), 386));
    accession_lengths.push_back(make_pair(string("1OVA-A"), 386));
    // Fails in entrez, we implemented regex for this in CBlastInputReader
    accession_lengths.push_back(make_pair(string("pdb|1OVA-A"), 386));
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
    BOOST_REQUIRE_EQUAL(kNumQueries, query_vector.size());
    BOOST_REQUIRE(source->End() == true);

    {{
        blast::TSeqLocVector cached_queries = source->GetAllSeqLocs(scope);
        BOOST_REQUIRE_EQUAL((size_t)0, (size_t)cached_queries.size());
        BOOST_REQUIRE(source->End() == true);
    }}

    for (size_t i = 0; i < kNumQueries; i++) {

        const string& accession = accession_lengths[i].first;
        CNcbiOstrstream oss;
        blast::SSeqLoc& ssl = query_vector[i];
        oss << "Accession " << accession << " difference in lengths: " 
            << ((TSeqPos)accession_lengths[i].second - 1) << " vs. "
            << ssl.seqloc->GetInt().GetTo();
        string msg = CNcbiOstrstreamToString(oss);
        BOOST_REQUIRE_MESSAGE(((TSeqPos)accession_lengths[i].second - 1) == 
                    ssl.seqloc->GetInt().GetTo(), msg);
        BOOST_REQUIRE_EQUAL((int)eNa_strand_unknown, (int)ssl.seqloc->GetStrand());
        BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetId() == true);
        BOOST_REQUIRE(blast::IsLocalId(ssl.seqloc->GetId()) == false);
    }

    /// Validate the data that would be retrieved by blast.cgi
    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(query_vector);
    BOOST_REQUIRE_EQUAL(kNumQueries, bioseqs->GetSeq_set().size());
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(ReadSingleGi)
{
    CNcbiIfstream infile("data/gi.txt");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRetrieveSeqData(false);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    CScope scope(*CObjectManager::GetInstance());
    BOOST_REQUIRE(source->End() == false);
    blast::TSeqLocVector seqs = source->GetAllSeqLocs(scope);
    blast::SSeqLoc ssl = seqs.front();
    BOOST_REQUIRE(source->End() == true);

    BOOST_REQUIRE(ssl.seqloc->IsInt() == true);
    BOOST_REQUIRE(blast::IsLocalId(ssl.seqloc->GetId()) == false);

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetStrand() == true);
    BOOST_REQUIRE_EQUAL((int)eNa_strand_both, (int)ssl.seqloc->GetInt().GetStrand());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetFrom() == true);
    BOOST_REQUIRE_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetTo() == true);
    const TSeqPos length = 247249719;
    BOOST_REQUIRE_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetId() == true);
    BOOST_REQUIRE_EQUAL(CSeq_id::e_Gi, ssl.seqloc->GetInt().GetId().Which());
    const TGi gi = GI_CONST(89161185);
    BOOST_REQUIRE_EQUAL(gi, ssl.seqloc->GetInt().GetId().GetGi());

    BOOST_REQUIRE(!ssl.mask);

    /// Validate the data that would be retrieved by blast.cgi
    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
    BOOST_REQUIRE_EQUAL((size_t)1, bioseqs->GetSeq_set().size());
    BOOST_REQUIRE(bioseqs->GetSeq_set().front()->IsSeq());
    const CBioseq& b = bioseqs->GetSeq_set().front()->GetSeq();
    BOOST_REQUIRE(b.IsNa());
    BOOST_REQUIRE_EQUAL(CSeq_id::e_Gi, b.GetId().front()->Which());
    BOOST_REQUIRE_EQUAL(gi, b.GetId().front()->GetGi());
    BOOST_REQUIRE_EQUAL(CSeq_inst::eRepr_raw, b.GetInst().GetRepr());
    BOOST_REQUIRE(CSeq_inst::IsNa(b.GetInst().GetMol()));
    BOOST_REQUIRE_EQUAL(length, b.GetInst().GetLength());
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(ReadMultipleGis)
{
    CNcbiIfstream infile("data/gis.txt");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRetrieveSeqData(false);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    vector< pair<TIntId, long> > gi_length;
    gi_length.push_back(make_pair(89161185, 247249719L));
    gi_length.push_back(make_pair(555, 624L));
    gi_length.push_back(make_pair(557, 489L));

    const size_t kNumQueries(gi_length.size());
    CScope scope(*CObjectManager::GetInstance());
    BOOST_REQUIRE(source->End() == false);
    blast::TSeqLocVector seqs = source->GetAllSeqLocs(scope);
    BOOST_REQUIRE(source->End() == true);

    for (size_t i = 0; i < kNumQueries; i++) {
        blast::SSeqLoc ssl = seqs[i];
        BOOST_REQUIRE(ssl.seqloc->IsInt() == true);

        BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetStrand() == true);
        BOOST_REQUIRE_EQUAL((int)eNa_strand_both, (int)ssl.seqloc->GetInt().GetStrand());

        BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetFrom() == true);
        BOOST_REQUIRE_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

        BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetTo() == true);
        const TSeqPos length = gi_length[i].second;
        BOOST_REQUIRE_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

        BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetId() == true);
        BOOST_REQUIRE(blast::IsLocalId(ssl.seqloc->GetId()) == false);
        BOOST_REQUIRE_EQUAL(CSeq_id::e_Gi, ssl.seqloc->GetInt().GetId().Which());
        const TIntId gi = gi_length[i].first;
        BOOST_REQUIRE_EQUAL(GI_FROM(TIntId, gi), ssl.seqloc->GetInt().GetId().GetGi());

        BOOST_REQUIRE(!ssl.mask);
    }

    /// Validate the data that would be retrieved by blast.cgi
    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
    BOOST_REQUIRE_EQUAL(kNumQueries, bioseqs->GetSeq_set().size());

    CBioseq_set::TSeq_set::const_iterator itr = bioseqs->GetSeq_set().begin();
    CBioseq_set::TSeq_set::const_iterator end = bioseqs->GetSeq_set().end();
    for (size_t i = 0; i < kNumQueries; i++, ++itr) {
        BOOST_REQUIRE(itr != end);
        BOOST_REQUIRE((*itr)->IsSeq());
        const CBioseq& b = (*itr)->GetSeq();
        BOOST_REQUIRE(b.IsNa());
        BOOST_REQUIRE_EQUAL(CSeq_id::e_Gi, b.GetId().front()->Which());
        BOOST_REQUIRE_EQUAL(GI_FROM(TIntId, gi_length[i].first), b.GetId().front()->GetGi());
        BOOST_REQUIRE_EQUAL(CSeq_inst::eRepr_raw, b.GetInst().GetRepr());
        BOOST_REQUIRE(CSeq_inst::IsNa(b.GetInst().GetMol()));
        BOOST_REQUIRE_EQUAL((long)gi_length[i].second, (long)b.GetInst().GetLength());
    }
    scope.GetObjectManager().RevokeAllDataLoaders();        
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

    BOOST_REQUIRE(source->End() == false);

    CScope scope(*CObjectManager::GetInstance());
    blast::TSeqLocVector query_vector = source->GetAllSeqLocs(scope);
    BOOST_REQUIRE_EQUAL(kNumQueries, query_vector.size());
    BOOST_REQUIRE(blast::IsLocalId(query_vector.front().seqloc->GetId()));
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(ReadMultipleSequencesFromSequencerParseLocalIds)
{
    CNcbiIfstream infile("data/DF-1.txt");
    const bool kIsProtein(false);
    const bool kParseID(true);
    SDataLoaderConfig dlconfig(kIsProtein);
    CBlastInputSourceConfig iconfig(dlconfig, objects::eNa_strand_other, false, kParseID);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    const size_t kNumQueries(96);

    BOOST_REQUIRE(source->End() == false);

    CScope scope(*CObjectManager::GetInstance());
    blast::TSeqLocVector query_vector = source->GetAllSeqLocs(scope);
    BOOST_REQUIRE_EQUAL(kNumQueries, query_vector.size());
    BOOST_REQUIRE(blast::IsLocalId(query_vector.front().seqloc->GetId()));
    // Check that the first three IDs went through.
    BOOST_REQUIRE_EQUAL(query_vector[0].seqloc->GetId()->AsFastaString(), string("lcl|seq#474_A03_564_c_T3+40.ab1"));
    BOOST_REQUIRE_EQUAL(query_vector[1].seqloc->GetId()->AsFastaString(), string("lcl|seq#474_A01_564_a_T3+40.ab1"));
    BOOST_REQUIRE_EQUAL(query_vector[2].seqloc->GetId()->AsFastaString(), string("lcl|seq#474_A02_564_b_T3+40.ab1"));
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(ReadSequenceWithlclID)
{
    CNcbiIfstream infile("data/localid.txt");
    const bool kIsProtein(false);
    const bool kParseID(true);
    SDataLoaderConfig dlconfig(kIsProtein);
    CBlastInputSourceConfig iconfig(dlconfig, objects::eNa_strand_other, false, kParseID);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    CScope scope(*CObjectManager::GetInstance());
    blast::TSeqLocVector query_vector = source->GetAllSeqLocs(scope);
    BOOST_REQUIRE(blast::IsLocalId(query_vector.front().seqloc->GetId()));
    // Check that the local ID went through.
    BOOST_REQUIRE_EQUAL(query_vector[0].seqloc->GetId()->AsFastaString(), string("lcl|mylocalID555"));
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

// This input file contains several sequences in FASTA format, but one of them
// is empty, this should proceed with no problems
BOOST_AUTO_TEST_CASE(ReadMultipleSequences_OneEmpty)
{
    CNcbiIfstream infile("data/nt.multiple_queries.one.empty");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    const size_t kNumQueries(6);

    BOOST_REQUIRE(source->End() == false);

    CScope scope(*CObjectManager::GetInstance());
    blast::TSeqLocVector query_vector = source->GetAllSeqLocs(scope);
    BOOST_REQUIRE_EQUAL(kNumQueries, query_vector.size());
    BOOST_REQUIRE(source->End() == true);
    TSeqPos query_lengths[] = { 1920, 1, 130, 0, 2, 1552 };
    int i = 0;
    ITERATE(blast::TSeqLocVector, q, query_vector) {
        BOOST_REQUIRE(blast::IsLocalId(query_vector[i].seqloc->GetId()));
        BOOST_REQUIRE_EQUAL(query_lengths[i], 
                            sequence::GetLength(*query_vector[i].seqloc, 
                                                query_vector[i].scope));
        i++;
    }

    string warnings;
    CheckForEmptySequences(query_vector, warnings);
    BOOST_REQUIRE(warnings.find("following sequences had no sequence data:")
                  != NPOS);

    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(query_vector);
    warnings.clear();
    CheckForEmptySequences(bioseqs, warnings);
    BOOST_REQUIRE(warnings.find("following sequences had no sequence data:")
                  != NPOS);
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(ReadMultipleTis)
{
    CNcbiIfstream infile("data/tis.txt");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRetrieveSeqData(false);
    iconfig.SetDataLoaderConfig().m_BlastDbName = "Trace/Mus_musculus_WGS" ;
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CScope scope(*CObjectManager::GetInstance());

    BOOST_REQUIRE(source->End() == false);

    vector< pair<int, long> > ti_lengths;
    ti_lengths.push_back(make_pair(12345, 657L));
    ti_lengths.push_back(make_pair(12347, 839L));
    ti_lengths.push_back(make_pair(12348, 658L));
    ti_lengths.push_back(make_pair(10000, 670L));

    const size_t kNumQueries(ti_lengths.size());
    blast::TSeqLocVector query_vector = source->GetAllSeqLocs(scope);
    BOOST_REQUIRE_EQUAL(kNumQueries, query_vector.size());
    BOOST_REQUIRE(source->End() == true);

    {{
        blast::TSeqLocVector cached_queries = source->GetAllSeqLocs(scope);
        BOOST_REQUIRE_EQUAL((size_t)0, (size_t)cached_queries.size());
        BOOST_REQUIRE(source->End() == true);
    }}

    const string db("ti");
    for (size_t i = 0; i < kNumQueries; i++) {

        const blast::SSeqLoc& ssl = query_vector[i];
        BOOST_REQUIRE(ssl.seqloc->IsInt());
        const CSeq_interval& seqint = ssl.seqloc->GetInt();
        BOOST_REQUIRE_EQUAL((int)eNa_strand_both, (int)ssl.seqloc->GetStrand());
        BOOST_REQUIRE_EQUAL((TSeqPos)ti_lengths[i].second - 1, seqint.GetTo());

        BOOST_REQUIRE(seqint.IsSetId() == true);
        BOOST_REQUIRE( !blast::IsLocalId(query_vector.front().seqloc->GetId()));
        BOOST_REQUIRE_EQUAL(CSeq_id::e_General, seqint.GetId().Which());
        BOOST_REQUIRE_EQUAL(db, seqint.GetId().GetGeneral().GetDb());
        BOOST_REQUIRE_EQUAL(ti_lengths[i].first,
                    seqint.GetId().GetGeneral().GetTag().GetId());
        BOOST_REQUIRE(!ssl.mask);
    }

    /// Validate the data that would be retrieved by blast.cgi
    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(query_vector);
    BOOST_REQUIRE_EQUAL(kNumQueries, bioseqs->GetSeq_set().size());
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(ReadSingleTi)
{
    CNcbiIfstream infile("data/ti.txt");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRetrieveSeqData(true);
    iconfig.SetDataLoaderConfig().m_BlastDbName = "Trace/Mus_musculus_WGS" ;
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    CScope scope(*CObjectManager::GetInstance());
    BOOST_REQUIRE(source->End() == false);
    blast::TSeqLocVector seqs = source->GetAllSeqLocs(scope);
    blast::SSeqLoc ssl = seqs.front();
    BOOST_REQUIRE(source->End() == true);

    BOOST_REQUIRE(ssl.seqloc->IsInt() == true);
    BOOST_REQUIRE( !blast::IsLocalId(ssl.seqloc->GetId()) );

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetStrand() == true);
    BOOST_REQUIRE_EQUAL((int)eNa_strand_both, (int)ssl.seqloc->GetInt().GetStrand());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetFrom() == true);
    BOOST_REQUIRE_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetTo() == true);
    const TSeqPos length(657);
    BOOST_REQUIRE_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetId() == true);
    BOOST_REQUIRE_EQUAL(CSeq_id::e_General, ssl.seqloc->GetInt().GetId().Which());
    const string db("ti");
    BOOST_REQUIRE_EQUAL(db, ssl.seqloc->GetInt().GetId().GetGeneral().GetDb());
    BOOST_REQUIRE(ssl.seqloc->GetInt().GetId().GetGeneral().GetTag().IsId());
    const int ti(12345);
    BOOST_REQUIRE_EQUAL(ti, ssl.seqloc->GetInt().GetId().GetGeneral().GetTag().GetId());

    BOOST_REQUIRE(!ssl.mask);

    /// Validate the data that would be retrieved by blast.cgi
    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
    BOOST_REQUIRE_EQUAL((size_t)1, bioseqs->GetSeq_set().size());
    BOOST_REQUIRE(bioseqs->GetSeq_set().front()->IsSeq());
    const CBioseq& b = bioseqs->GetSeq_set().front()->GetSeq();
    BOOST_REQUIRE(b.IsNa());
    BOOST_REQUIRE_EQUAL(CSeq_id::e_General, b.GetId().front()->Which());
    BOOST_REQUIRE_EQUAL(db, b.GetId().front()->GetGeneral().GetDb());
    BOOST_REQUIRE_EQUAL(ti, b.GetId().front()->GetGeneral().GetTag().GetId());
    BOOST_REQUIRE_EQUAL(CSeq_inst::eRepr_raw, b.GetInst().GetRepr());
    BOOST_REQUIRE(CSeq_inst::IsNa(b.GetInst().GetMol()));
    BOOST_REQUIRE_EQUAL(length, b.GetInst().GetLength());
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(ReadAccessionsAndGisWithNewLines)
{
    CNcbiIfstream infile("data/accgis_nl.txt");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRetrieveSeqData(false);
    iconfig.SetDataLoaderConfig().m_BlastDbName = "Trace/Mus_musculus_WGS" ;
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    vector< pair<string, long> > gi_accessions;
    gi_accessions.push_back(make_pair(string("89161215"), 111583154L));
    gi_accessions.push_back(make_pair(string("89161217"), 155407050L));
    gi_accessions.push_back(make_pair(string("89161219"), 11133097L));
    gi_accessions.push_back(make_pair(string("NC_000001"), 248956422L));
    gi_accessions.push_back(make_pair(string("NC_000010.9"), 135374737L));
    gi_accessions.push_back(make_pair(string("gnl|ti|12345"), 657L));
    gi_accessions.push_back(make_pair(string("NC_000011.8"), 134452384L));
    gi_accessions.push_back(make_pair(string("NC_000012.10"), 132349534L));

    const size_t kNumQueries(gi_accessions.size());
    CScope scope(*CObjectManager::GetInstance());
    blast::TSeqLocVector query_vector = source->GetAllSeqLocs(scope);
    BOOST_REQUIRE_EQUAL(kNumQueries, query_vector.size());
    BOOST_REQUIRE(source->End() == true);

    {{
        blast::TSeqLocVector cached_queries = source->GetAllSeqLocs(scope);
        BOOST_REQUIRE_EQUAL((size_t)0, (size_t)cached_queries.size());
        BOOST_REQUIRE(source->End() == true);
    }}

    for (size_t i = 0; i < kNumQueries; i++) {

        blast::SSeqLoc& ssl = query_vector[i];
        BOOST_REQUIRE_EQUAL((int)eNa_strand_both, (int)ssl.seqloc->GetStrand());
        BOOST_REQUIRE_EQUAL((TSeqPos)gi_accessions[i].second - 1, 
                    ssl.seqloc->GetInt().GetTo());

        const string& id = gi_accessions[i].first;

        BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetId() == true);
        BOOST_REQUIRE( !blast::IsLocalId(ssl.seqloc->GetId()) );
        TGi gi = ZERO_GI;
        if ( (gi = NStr::StringToNumeric<TGi>(id, NStr::fConvErr_NoThrow)) != ZERO_GI) {
            BOOST_REQUIRE_EQUAL(CSeq_id::e_Gi, ssl.seqloc->GetInt().GetId().Which());
            BOOST_REQUIRE_EQUAL(gi, ssl.seqloc->GetInt().GetId().GetGi());
        } else if (i == 5) {
            BOOST_REQUIRE_EQUAL(CSeq_id::e_General, 
                        ssl.seqloc->GetInt().GetId().Which());
            const string db("ti");
            BOOST_REQUIRE_EQUAL(db, ssl.seqloc->GetInt().GetId().GetGeneral().GetDb());
            BOOST_REQUIRE(ssl.seqloc->GetInt().GetId().GetGeneral().GetTag().IsId());
            const int ti(12345);
            BOOST_REQUIRE_EQUAL(ti, 
                        ssl.seqloc->GetInt().GetId().
                        GetGeneral().GetTag().GetId());
        } else {
            BOOST_REQUIRE_EQUAL(CSeq_id::e_Other, ssl.seqloc->GetInt().GetId().Which());
            string accession;
            int version;

            switch (i) {
            case 3: accession.assign("NC_000001"); version = 0; break;
            case 4: accession.assign("NC_000010"); version = 9; break;
            case 6: accession.assign("NC_000011"); version = 8; break;
            case 7: accession.assign("NC_000012"); version = 10; break;
            default: abort();
            }

            BOOST_REQUIRE_EQUAL(accession,
                        ssl.seqloc->GetInt().GetId().GetOther().GetAccession());
            if (version != 0) {
                BOOST_REQUIRE_EQUAL(version, 
                        ssl.seqloc->GetInt().GetId().GetOther().GetVersion());
            }
        }
        BOOST_REQUIRE(!ssl.mask);

    }

    /// Validate the data that would be retrieved by blast.cgi
    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(query_vector);
    BOOST_REQUIRE_EQUAL(kNumQueries, bioseqs->GetSeq_set().size());
    scope.GetObjectManager().RevokeAllDataLoaders();        
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
    BOOST_REQUIRE(source->End() == false);
    blast::TSeqLocVector seqs = source->GetAllSeqLocs(scope);
    blast::SSeqLoc ssl = seqs.front();


    BOOST_REQUIRE(source->End() == true);

    BOOST_REQUIRE(ssl.seqloc->IsInt() == true);

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetStrand() == true);
    BOOST_REQUIRE_EQUAL((int)eNa_strand_both, (int)ssl.seqloc->GetInt().GetStrand());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetFrom() == true);
    BOOST_REQUIRE_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetTo() == true);
    const TSeqPos length(248956422);
    BOOST_REQUIRE_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetId() == true);
    BOOST_REQUIRE( !blast::IsLocalId(ssl.seqloc->GetId()) );
    BOOST_REQUIRE_EQUAL(CSeq_id::e_Other, ssl.seqloc->GetInt().GetId().Which());
    const string accession("NC_000001");
    BOOST_REQUIRE_EQUAL(accession,
                ssl.seqloc->GetInt().GetId().GetOther().GetAccession());

    BOOST_REQUIRE(!ssl.mask);

    /// Validate the data that would be retrieved by blast.cgi
    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
    BOOST_REQUIRE_EQUAL((size_t)1, bioseqs->GetSeq_set().size());
    BOOST_REQUIRE(bioseqs->GetSeq_set().front()->IsSeq());
    const CBioseq& b = bioseqs->GetSeq_set().front()->GetSeq();
    BOOST_REQUIRE(b.IsNa());
    BOOST_REQUIRE_EQUAL(CSeq_id::e_Other, b.GetId().front()->Which());
    BOOST_REQUIRE_EQUAL(accession, b.GetId().front()->GetOther().GetAccession());
    BOOST_REQUIRE_EQUAL(CSeq_inst::eRepr_raw, b.GetInst().GetRepr());
    BOOST_REQUIRE(CSeq_inst::IsNa(b.GetInst().GetMol()));
    BOOST_REQUIRE_EQUAL(length, b.GetInst().GetLength());
    scope.GetObjectManager().RevokeAllDataLoaders();        

}

BOOST_AUTO_TEST_CASE(ReadGiNuclWithFlankingSpacesIntoBuffer_Single)
{
    // N.B.: the extra newline causes the CFastaReader to throw an EOF exception
    auto_ptr<string> user_input(new string("    1945386  \n "));

    CRef<CObjectManager> om(CObjectManager::GetInstance());
    CBlastInputSourceConfig iconfig(false);
    CRef<CBlastInput> source(s_DeclareBlastInput(*user_input, iconfig));

    CScope scope(*om);
    BOOST_REQUIRE(source->End() == false);
    blast::TSeqLocVector seqs = source->GetAllSeqLocs(scope);
    BOOST_REQUIRE(source->End() == true);
    blast::SSeqLoc ssl = seqs.front();

    BOOST_REQUIRE(source->End() == true);

    BOOST_REQUIRE(ssl.seqloc->IsInt() == true);

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetStrand() == true);
    BOOST_REQUIRE_EQUAL((int)eNa_strand_both, (int)ssl.seqloc->GetInt().GetStrand());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetFrom() == true);
    BOOST_REQUIRE_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetTo() == true);
    const TSeqPos length(2772);
    BOOST_REQUIRE_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetId() == true);
    BOOST_REQUIRE( !blast::IsLocalId(ssl.seqloc->GetId()) );
    const TGi gi = GI_CONST(1945386);
    const string gb_name = "HSU93236";
    const string gb_accession = "U93236";
    const int gb_version = 1;
    if ( !CSeq_id::PreferAccessionOverGi() ) {
        BOOST_REQUIRE_EQUAL(CSeq_id::e_Gi, ssl.seqloc->GetInt().GetId().Which());
        BOOST_REQUIRE_EQUAL(gi, ssl.seqloc->GetInt().GetId().GetGi());
    }
    else {
        BOOST_REQUIRE_EQUAL(CSeq_id::e_Genbank, ssl.seqloc->GetInt().GetId().Which());
        BOOST_REQUIRE_EQUAL(gb_name, ssl.seqloc->GetInt().GetId().GetGenbank().GetName());
        BOOST_REQUIRE_EQUAL(gb_accession, ssl.seqloc->GetInt().GetId().GetGenbank().GetAccession());
        BOOST_REQUIRE_EQUAL(gb_version, ssl.seqloc->GetInt().GetId().GetGenbank().GetVersion());
    }

    BOOST_REQUIRE(!ssl.mask);

    /// Validate the data that would be retrieved by blast.cgi
    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
    BOOST_REQUIRE_EQUAL((size_t)1, bioseqs->GetSeq_set().size());
    BOOST_REQUIRE(bioseqs->GetSeq_set().front()->IsSeq());
    const CBioseq& b = bioseqs->GetSeq_set().front()->GetSeq();
    BOOST_REQUIRE(b.IsNa());

    CRef<CSeq_id> id = FindBestChoice(b.GetId(), CSeq_id::BestRank);
    BOOST_REQUIRE(id.NotNull());
    if ( !CSeq_id::PreferAccessionOverGi() ) {
        BOOST_REQUIRE_EQUAL(CSeq_id::e_Gi, id->Which());
        BOOST_REQUIRE_EQUAL(gi, id->GetGi());
    }
    else {
        BOOST_REQUIRE_EQUAL(CSeq_id::e_Genbank, id->Which());
        BOOST_REQUIRE_EQUAL(gb_name, id->GetGenbank().GetName());
        BOOST_REQUIRE_EQUAL(gb_accession, id->GetGenbank().GetAccession());
        BOOST_REQUIRE_EQUAL(gb_version, id->GetGenbank().GetVersion());
    }
    BOOST_REQUIRE_EQUAL(CSeq_inst::eRepr_raw, b.GetInst().GetRepr());
    BOOST_REQUIRE(CSeq_inst::IsNa(b.GetInst().GetMol()));
    BOOST_REQUIRE_EQUAL(length, b.GetInst().GetLength());
    scope.GetObjectManager().RevokeAllDataLoaders();        

}

BOOST_AUTO_TEST_CASE(ReadAccessionNuclWithFlankingSpacesIntoBuffer_Single)
{
    auto_ptr<string> user_input(new string("  X65215.1   "));

    CRef<CObjectManager> om(CObjectManager::GetInstance());
    CBlastInputSourceConfig iconfig(false);
    CBlastFastaInputSource fasta_source(*user_input, iconfig);
    CBlastInput source(&fasta_source);

    CScope scope(*om);
    BOOST_REQUIRE(source.End() == false);
    blast::TSeqLocVector seqs = source.GetAllSeqLocs(scope);
    blast::SSeqLoc ssl = seqs.front();

    BOOST_REQUIRE(source.End() == true);

    BOOST_REQUIRE(ssl.seqloc->IsInt() == true);

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetStrand() == true);
    BOOST_REQUIRE_EQUAL((int)eNa_strand_both, (int)ssl.seqloc->GetInt().GetStrand());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetFrom() == true);
    BOOST_REQUIRE_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetTo() == true);
    const TSeqPos length(624);
    BOOST_REQUIRE_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetId() == true);
    BOOST_REQUIRE( !blast::IsLocalId(ssl.seqloc->GetId()) );

    const TGi gi = GI_CONST(555);
    const string accession = "X65215";
    const int version = 1;
    if ( !CSeq_id::PreferAccessionOverGi() ) {
        BOOST_REQUIRE_EQUAL(CSeq_id::e_Gi, ssl.seqloc->GetInt().GetId().Which());
        BOOST_REQUIRE_EQUAL(gi, ssl.seqloc->GetInt().GetId().GetGi());
    }
    else {
        BOOST_REQUIRE_EQUAL(CSeq_id::e_Embl, ssl.seqloc->GetInt().GetId().Which());
        BOOST_REQUIRE_EQUAL(accession, ssl.seqloc->GetInt().GetId().GetEmbl().GetAccession());
        BOOST_REQUIRE_EQUAL(version, ssl.seqloc->GetInt().GetId().GetEmbl().GetVersion());
    }

    BOOST_REQUIRE(!ssl.mask);

    /// Validate the data that would be retrieved by blast.cgi
    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
    BOOST_REQUIRE_EQUAL((size_t)1, bioseqs->GetSeq_set().size());
    BOOST_REQUIRE(bioseqs->GetSeq_set().front()->IsSeq());
    const CBioseq& b = bioseqs->GetSeq_set().front()->GetSeq();
    BOOST_REQUIRE(b.IsNa());
    bool found_gi = false, found_accession = false;
    ITERATE(CBioseq::TId, id, b.GetId()) {
        if ((*id)->Which() == CSeq_id::e_Gi) {
            BOOST_REQUIRE_EQUAL(GI_CONST(555), (*id)->GetGi());
            found_gi = true;
        } else if ((*id)->Which() == CSeq_id::e_Embl) {
            CNcbiOstrstream os;
            (*id)->GetEmbl().AsFastaString(os);
            const string fasta_acc = CNcbiOstrstreamToString(os);
            BOOST_REQUIRE(NStr::Find(fasta_acc, accession) != NPOS);
            found_accession = true;
        }
    }
    BOOST_REQUIRE(found_gi);
    BOOST_REQUIRE(found_accession);
    BOOST_REQUIRE_EQUAL(CSeq_inst::eRepr_raw, b.GetInst().GetRepr());
    BOOST_REQUIRE(CSeq_inst::IsNa(b.GetInst().GetMol()));
    BOOST_REQUIRE_EQUAL(length, b.GetInst().GetLength());
    scope.GetObjectManager().RevokeAllDataLoaders();        
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
    BOOST_REQUIRE(source.End() == false);
    blast::TSeqLocVector seqs = source.GetAllSeqLocs(scope);
    blast::SSeqLoc ssl = seqs.front();

    BOOST_REQUIRE(source.End() == true);

    BOOST_REQUIRE(ssl.seqloc->IsInt() == true);

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetStrand() == true);
    BOOST_REQUIRE_EQUAL((int)eNa_strand_unknown, (int)ssl.seqloc->GetInt().GetStrand());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetFrom() == true);
    BOOST_REQUIRE_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetTo() == true);
    const TSeqPos length = 232;
    BOOST_REQUIRE_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetId() == true);
    BOOST_REQUIRE_EQUAL(CSeq_id::e_Local, ssl.seqloc->GetInt().GetId().Which());
    BOOST_REQUIRE(blast::IsLocalId(ssl.seqloc->GetId()));

    BOOST_REQUIRE(!ssl.mask);

    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
    BOOST_REQUIRE_EQUAL((size_t)1, bioseqs->GetSeq_set().size());
    BOOST_REQUIRE(bioseqs->GetSeq_set().front()->IsSeq());
    const CBioseq& b = bioseqs->GetSeq_set().front()->GetSeq();
    BOOST_REQUIRE(b.IsAa());
    BOOST_REQUIRE_EQUAL(CSeq_inst::eRepr_raw, b.GetInst().GetRepr());
    BOOST_REQUIRE_EQUAL(CSeq_inst::eMol_aa, b.GetInst().GetMol());
    BOOST_REQUIRE_EQUAL(length, b.GetInst().GetLength());
    scope.GetObjectManager().RevokeAllDataLoaders();        

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

    BOOST_REQUIRE_EQUAL(start, ssl.seqloc->GetInt().GetFrom());
    BOOST_REQUIRE_EQUAL(stop, ssl.seqloc->GetInt().GetTo());
    BOOST_REQUIRE_EQUAL(start, ssl.seqloc->GetStart(eExtreme_Positional));
    BOOST_REQUIRE_EQUAL(stop, ssl.seqloc->GetStop(eExtreme_Positional));
    scope.GetObjectManager().RevokeAllDataLoaders();        
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

    BOOST_REQUIRE_EQUAL(start, ssl.seqloc->GetInt().GetFrom());
    BOOST_REQUIRE_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());
    BOOST_REQUIRE_EQUAL(start, ssl.seqloc->GetStart(eExtreme_Positional));
    BOOST_REQUIRE_EQUAL(length-1, ssl.seqloc->GetStop(eExtreme_Positional));
    scope.GetObjectManager().RevokeAllDataLoaders();        
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
        BOOST_REQUIRE(msg.find("Invalid sequence range") != NPOS);
        BOOST_REQUIRE_EQUAL(CInputException::eInvalidRange, e.GetErrCode());
        return;
    }
    BOOST_REQUIRE(false); // should never get here
    scope.GetObjectManager().RevokeAllDataLoaders();        
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
        BOOST_REQUIRE(msg.find("Invalid from coordinate") != NPOS);
        BOOST_REQUIRE_EQUAL(CInputException::eInvalidRange, e.GetErrCode());
        return;
    }
    BOOST_REQUIRE(false); // should never get here
    scope.GetObjectManager().RevokeAllDataLoaders();        
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

    BOOST_REQUIRE(ssl.seqloc->IsInt() == true);

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetStrand() == true);
    BOOST_REQUIRE_EQUAL((int)eNa_strand_unknown, (int)ssl.seqloc->GetInt().GetStrand());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetFrom() == true);
    BOOST_REQUIRE_EQUAL((TSeqPos)10, ssl.seqloc->GetInt().GetFrom());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetTo() == true);
    BOOST_REQUIRE_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());
    scope.GetObjectManager().RevokeAllDataLoaders();        
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

    BOOST_REQUIRE(ssl.seqloc->IsInt() == true);

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetStrand() == true);
    BOOST_REQUIRE_EQUAL((int)eNa_strand_unknown, (int)ssl.seqloc->GetInt().GetStrand());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetFrom() == true);
    BOOST_REQUIRE_EQUAL((TSeqPos)10, ssl.seqloc->GetInt().GetFrom());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetTo() == true);
    BOOST_REQUIRE_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(ParseDefline)
{
    CNcbiIfstream infile("data/aa.129295");
    const bool is_protein(true);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetBelieveDeflines(true);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CScope scope(*CObjectManager::GetInstance());

    const TGi gi = GI_CONST(129295);
    const string name = "OVAX_CHICK";
    const string accession = "P01013";
    const string release = "reviewed";
    blast::SSeqLoc ssl = source->GetNextSeqLocBatch(scope).front();
    BOOST_REQUIRE( !blast::IsLocalId(ssl.seqloc->GetId()) );

    if ( !CSeq_id::PreferAccessionOverGi() ) {
        BOOST_REQUIRE_EQUAL(CSeq_id::e_Gi, ssl.seqloc->GetId()->Which());
        BOOST_REQUIRE_EQUAL(gi, ssl.seqloc->GetId()->GetGi());
        BOOST_REQUIRE_EQUAL(CSeq_id::e_Gi, ssl.seqloc->GetInt().GetId().Which());
        BOOST_REQUIRE_EQUAL(gi, ssl.seqloc->GetInt().GetId().GetGi());
    }
    else {
        BOOST_REQUIRE_EQUAL(CSeq_id::e_Swissprot, ssl.seqloc->GetId()->Which());
        BOOST_REQUIRE_EQUAL(name, ssl.seqloc->GetId()->GetSwissprot().GetName());
        BOOST_REQUIRE_EQUAL(accession, ssl.seqloc->GetId()->GetSwissprot().GetAccession());
        BOOST_REQUIRE_EQUAL(release, ssl.seqloc->GetId()->GetSwissprot().GetRelease());
        BOOST_REQUIRE_EQUAL(CSeq_id::e_Swissprot, ssl.seqloc->GetInt().GetId().Which());
        BOOST_REQUIRE_EQUAL(name, ssl.seqloc->GetInt().GetId().GetSwissprot().GetName());
        BOOST_REQUIRE_EQUAL(accession, ssl.seqloc->GetInt().GetId().GetSwissprot().GetAccession());
        BOOST_REQUIRE_EQUAL(release, ssl.seqloc->GetInt().GetId().GetSwissprot().GetRelease());
    }
    scope.GetObjectManager().RevokeAllDataLoaders();        
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
        BOOST_REQUIRE(msg.find("Cannot assign nucleotide strand to protein") 
                    != NPOS);
        BOOST_REQUIRE_EQUAL(CInputException::eInvalidStrand, e.GetErrCode());
        caught_exception = true;
    }
    BOOST_REQUIRE(caught_exception);
    BOOST_REQUIRE(source->End() == true);
    scope.GetObjectManager().RevokeAllDataLoaders();        
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
    BOOST_REQUIRE_EQUAL(kNumQueries, query_vector.size());
    BOOST_REQUIRE(source->End() == true);

    {{
        blast::TSeqLocVector cached_queries = source->GetAllSeqLocs(scope);
        BOOST_REQUIRE_EQUAL((size_t)0, (size_t)cached_queries.size());
        BOOST_REQUIRE(source->End() == true);
    }}

    blast::SSeqLoc ssl = query_vector.front();
    TSeqPos length = 646;

    BOOST_REQUIRE_EQUAL((int)eNa_strand_both, (int)ssl.seqloc->GetStrand());
    BOOST_REQUIRE_EQUAL(length-1, ssl.seqloc->GetStop(eExtreme_Positional));
    BOOST_REQUIRE_EQUAL((int)eNa_strand_both, (int)ssl.seqloc->GetInt().GetStrand());
    BOOST_REQUIRE_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());
    BOOST_REQUIRE(blast::IsLocalId(ssl.seqloc->GetId()));

    ssl = query_vector.back();

    length = 360;
    BOOST_REQUIRE_EQUAL((int)eNa_strand_both, (int)ssl.seqloc->GetStrand());
    BOOST_REQUIRE_EQUAL(length-1, ssl.seqloc->GetStop(eExtreme_Positional));
    BOOST_REQUIRE_EQUAL((int)eNa_strand_both, (int)ssl.seqloc->GetInt().GetStrand());
    BOOST_REQUIRE_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());
    BOOST_REQUIRE(blast::IsLocalId(ssl.seqloc->GetId()));
    BOOST_REQUIRE(!ssl.mask);
    scope.GetObjectManager().RevokeAllDataLoaders();        
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
            BOOST_REQUIRE_EQUAL((int)strand, (int)ssl.seqloc->GetStrand());
            BOOST_REQUIRE_EQUAL((int)strand, (int)ssl.seqloc->GetInt().GetStrand());
            BOOST_REQUIRE(blast::IsLocalId(ssl.seqloc->GetId()));
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
            BOOST_REQUIRE_EQUAL((int)strand, (int)ssl.seqloc->GetStrand());
            BOOST_REQUIRE_EQUAL((int)strand, (int)ssl.seqloc->GetInt().GetStrand());
            BOOST_REQUIRE(blast::IsLocalId(ssl.seqloc->GetId()));
        }
    }
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(NuclLcaseMask_TSeqLocVector)
{
    CNcbiIfstream infile("data/nt.cat");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    BOOST_REQUIRE(iconfig.GetBelieveDeflines() == false);
    BOOST_REQUIRE(iconfig.GetLowercaseMask() == false);
    BOOST_REQUIRE_EQUAL((int)eNa_strand_both, (int)iconfig.GetStrand());
    iconfig.SetLowercaseMask(true);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CScope scope(*CObjectManager::GetInstance());

    blast::TSeqLocVector seqs = source->GetNextSeqLocBatch(scope);
    blast::TSeqLocVector::iterator itr = seqs.begin();
    blast::SSeqLoc ssl = *itr;
    BOOST_REQUIRE(ssl.mask);
    BOOST_REQUIRE(ssl.mask->IsPacked_int());

    CPacked_seqint::Tdata masklocs = ssl.mask->GetPacked_int();
    BOOST_REQUIRE_EQUAL((size_t)2, masklocs.size());
    BOOST_REQUIRE_EQUAL((TSeqPos)126, masklocs.front()->GetFrom());
    BOOST_REQUIRE_EQUAL((TSeqPos)167, masklocs.front()->GetTo());
    // any masks read from the file are expected to be in the plus strand
    BOOST_REQUIRE(masklocs.front()->CanGetStrand());
    BOOST_REQUIRE_EQUAL((int)eNa_strand_plus, (int)masklocs.front()->GetStrand());

    BOOST_REQUIRE_EQUAL((TSeqPos)330, masklocs.back()->GetFrom());
    BOOST_REQUIRE_EQUAL((TSeqPos)356, masklocs.back()->GetTo());
    // any masks read from the file are expected to be in the plus strand
    BOOST_REQUIRE(masklocs.back()->CanGetStrand());
    BOOST_REQUIRE_EQUAL((int)eNa_strand_plus, (int)masklocs.back()->GetStrand());

    ssl = *++itr;
    BOOST_REQUIRE(ssl.mask);
    BOOST_REQUIRE(ssl.mask->IsNull());
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(NuclLcaseMask_BlastQueryVector)
{
    CNcbiIfstream infile("data/nt.cat");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetLowercaseMask(true);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CScope scope(*CObjectManager::GetInstance());

    CRef<blast::CBlastQueryVector> seqs = source->GetNextSeqBatch(scope);
    BOOST_REQUIRE( !seqs->Empty() );
    BOOST_REQUIRE_EQUAL((int)2, (int)seqs->size());
    CRef<blast::CBlastSearchQuery> query = (*seqs)[0];
    BOOST_REQUIRE( !query->GetMaskedRegions().empty());

    CRef<CPacked_seqint> masks =
        query->GetMaskedRegions().ConvertToCPacked_seqint();
    CPacked_seqint::Tdata masklocs = masks->Get();
    CPacked_seqint::Tdata::const_iterator itr = masks->Get().begin();
    BOOST_REQUIRE_EQUAL((size_t)4, masklocs.size());

    // Note that for this case, the masks even though are also read from the
    // file (as the unit test above), these are returned for both strands.
    BOOST_REQUIRE_EQUAL((TSeqPos)126, (*itr)->GetFrom());
    BOOST_REQUIRE_EQUAL((TSeqPos)167, (*itr)->GetTo());
    BOOST_REQUIRE((*itr)->CanGetStrand());
    BOOST_REQUIRE_EQUAL((int)eNa_strand_plus, (int)(*itr)->GetStrand());
    ++itr;
    BOOST_REQUIRE_EQUAL((TSeqPos)126, (*itr)->GetFrom());
    BOOST_REQUIRE_EQUAL((TSeqPos)167, (*itr)->GetTo());
    BOOST_REQUIRE((*itr)->CanGetStrand());
    BOOST_REQUIRE_EQUAL((int)eNa_strand_minus, (int)(*itr)->GetStrand());
    ++itr;

    BOOST_REQUIRE_EQUAL((TSeqPos)330, (*itr)->GetFrom());
    BOOST_REQUIRE_EQUAL((TSeqPos)356, (*itr)->GetTo());
    BOOST_REQUIRE((*itr)->CanGetStrand());
    BOOST_REQUIRE_EQUAL((int)eNa_strand_plus, (int)(*itr)->GetStrand());
    ++itr;
    BOOST_REQUIRE_EQUAL((TSeqPos)330, (*itr)->GetFrom());
    BOOST_REQUIRE_EQUAL((TSeqPos)356, (*itr)->GetTo());
    BOOST_REQUIRE((*itr)->CanGetStrand());
    BOOST_REQUIRE_EQUAL((int)eNa_strand_minus, (int)(*itr)->GetStrand());
    ++itr;

    BOOST_REQUIRE(itr == masks->Get().end());

    query = (*seqs)[1];
    BOOST_REQUIRE(query->GetMaskedRegions().empty());
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(MultiSeq)
{
    CNcbiIfstream infile("data/aa.cat");
    const bool is_protein(true);
    CBlastInputSourceConfig iconfig(is_protein);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CScope scope(*CObjectManager::GetInstance());

    blast::TSeqLocVector v = source->GetAllSeqLocs(scope);
    BOOST_REQUIRE(source->End());
    BOOST_REQUIRE_EQUAL((size_t)19, v.size());
    scope.GetObjectManager().RevokeAllDataLoaders();        
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
        BOOST_REQUIRE_EQUAL(start, itr->seqloc->GetStart(eExtreme_Positional));
        BOOST_REQUIRE_EQUAL(stop, itr->seqloc->GetStop(eExtreme_Positional));
        BOOST_REQUIRE_EQUAL(start, itr->seqloc->GetInt().GetFrom());
        BOOST_REQUIRE_EQUAL(stop, itr->seqloc->GetInt().GetTo());
    }
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(MultiBatch)
{
    CNcbiIfstream infile("data/aa.cat");
    const bool is_protein(true);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetBelieveDeflines(true);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig, 5000));
    CScope scope(*CObjectManager::GetInstance());

    TGi gi;
    blast::TSeqLocVector v;

    v = source->GetNextSeqLocBatch(scope);
    BOOST_REQUIRE_EQUAL((size_t)7, v.size());
    BOOST_REQUIRE_EQUAL((TSeqPos)530, v[0].seqloc->GetInt().GetTo());
    gi = GI_CONST(1346057);
    string name = "G11A_ORYSA";
    string accession = "P47997";
    string release = "reviewed";
    BOOST_REQUIRE( !blast::IsLocalId(v[0].seqloc->GetId()) );
    if ( !CSeq_id::PreferAccessionOverGi() ) {
        BOOST_REQUIRE_EQUAL(CSeq_id::e_Gi, v[0].seqloc->GetInt().GetId().Which());
        BOOST_REQUIRE_EQUAL(gi, v[0].seqloc->GetInt().GetId().GetGi());
        BOOST_REQUIRE_EQUAL(CSeq_id::e_Gi, v[0].seqloc->GetId()->Which());
        BOOST_REQUIRE_EQUAL(gi, v[0].seqloc->GetId()->GetGi());
    }
    else {
        BOOST_REQUIRE_EQUAL(CSeq_id::e_Swissprot, v[0].seqloc->GetInt().GetId().Which());
        BOOST_REQUIRE_EQUAL(name, v[0].seqloc->GetInt().GetId().GetSwissprot().GetName());
        BOOST_REQUIRE_EQUAL(accession, v[0].seqloc->GetInt().GetId().GetSwissprot().GetAccession());
        BOOST_REQUIRE_EQUAL(release, v[0].seqloc->GetInt().GetId().GetSwissprot().GetRelease());
        BOOST_REQUIRE_EQUAL(CSeq_id::e_Swissprot, v[0].seqloc->GetId()->Which());
        BOOST_REQUIRE_EQUAL(name, v[0].seqloc->GetId()->GetSwissprot().GetName());
        BOOST_REQUIRE_EQUAL(accession, v[0].seqloc->GetId()->GetSwissprot().GetAccession());
        BOOST_REQUIRE_EQUAL(release, v[0].seqloc->GetId()->GetSwissprot().GetRelease());
    }

    v = source->GetNextSeqLocBatch(scope);
    BOOST_REQUIRE_EQUAL((size_t)8, v.size());
    BOOST_REQUIRE_EQUAL((TSeqPos)445, v[0].seqloc->GetInt().GetTo());
    gi = GI_CONST(1170625);
    name = "KCC1_YEAST";
    accession = "P27466";
    release = "reviewed";
    BOOST_REQUIRE( !blast::IsLocalId(v[0].seqloc->GetId()) );
    if ( !CSeq_id::PreferAccessionOverGi() ) {
        BOOST_REQUIRE_EQUAL(CSeq_id::e_Gi, v[0].seqloc->GetInt().GetId().Which());
        BOOST_REQUIRE_EQUAL(gi, v[0].seqloc->GetInt().GetId().GetGi());
        BOOST_REQUIRE_EQUAL(CSeq_id::e_Gi, v[0].seqloc->GetId()->Which());
        BOOST_REQUIRE_EQUAL(gi, v[0].seqloc->GetId()->GetGi());
    }
    else {
        BOOST_REQUIRE_EQUAL(CSeq_id::e_Swissprot, v[0].seqloc->GetInt().GetId().Which());
        BOOST_REQUIRE_EQUAL(name, v[0].seqloc->GetInt().GetId().GetSwissprot().GetName());
        BOOST_REQUIRE_EQUAL(accession, v[0].seqloc->GetInt().GetId().GetSwissprot().GetAccession());
        BOOST_REQUIRE_EQUAL(release, v[0].seqloc->GetInt().GetId().GetSwissprot().GetRelease());
        BOOST_REQUIRE_EQUAL(CSeq_id::e_Swissprot, v[0].seqloc->GetId()->Which());
        BOOST_REQUIRE_EQUAL(name, v[0].seqloc->GetId()->GetSwissprot().GetName());
        BOOST_REQUIRE_EQUAL(accession, v[0].seqloc->GetId()->GetSwissprot().GetAccession());
        BOOST_REQUIRE_EQUAL(release, v[0].seqloc->GetId()->GetSwissprot().GetRelease());
    }

    v = source->GetNextSeqLocBatch(scope);
    BOOST_REQUIRE_EQUAL((size_t)4, v.size());
    BOOST_REQUIRE_EQUAL((TSeqPos)688, v[0].seqloc->GetInt().GetTo());
    gi = GI_CONST(114152);
    name = "ARK1_HUMAN";
    accession = "P25098";
    release = "reviewed";
    BOOST_REQUIRE( !blast::IsLocalId(v[0].seqloc->GetId()) );
    if ( !CSeq_id::PreferAccessionOverGi() ) {
        BOOST_REQUIRE_EQUAL(CSeq_id::e_Gi, v[0].seqloc->GetInt().GetId().Which());
        BOOST_REQUIRE_EQUAL(gi, v[0].seqloc->GetInt().GetId().GetGi());
        BOOST_REQUIRE_EQUAL(CSeq_id::e_Gi, v[0].seqloc->GetId()->Which());
        BOOST_REQUIRE_EQUAL(gi, v[0].seqloc->GetId()->GetGi());
    }
    else {
        BOOST_REQUIRE_EQUAL(CSeq_id::e_Swissprot, v[0].seqloc->GetInt().GetId().Which());
        BOOST_REQUIRE_EQUAL(name, v[0].seqloc->GetInt().GetId().GetSwissprot().GetName());
        BOOST_REQUIRE_EQUAL(accession, v[0].seqloc->GetInt().GetId().GetSwissprot().GetAccession());
        BOOST_REQUIRE_EQUAL(release, v[0].seqloc->GetInt().GetId().GetSwissprot().GetRelease());
        BOOST_REQUIRE_EQUAL(CSeq_id::e_Swissprot, v[0].seqloc->GetId()->Which());
        BOOST_REQUIRE_EQUAL(name, v[0].seqloc->GetId()->GetSwissprot().GetName());
        BOOST_REQUIRE_EQUAL(accession, v[0].seqloc->GetId()->GetSwissprot().GetAccession());
        BOOST_REQUIRE_EQUAL(release, v[0].seqloc->GetId()->GetSwissprot().GetRelease());
    }

    BOOST_REQUIRE(source->End());
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(NoDeflineExpected)
{
    CNcbiIfstream infile("data/tiny.fa");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CScope scope(*CObjectManager::GetInstance());

    blast::TSeqLocVector v = source->GetAllSeqLocs(scope);
    BOOST_REQUIRE(source->End());
    BOOST_REQUIRE_EQUAL((size_t)1, v.size());
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(NoDeflineUnexpected)
{
    CNcbiIfstream infile("data/tiny.fa");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetBelieveDeflines(true);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CScope scope(*CObjectManager::GetInstance());

    BOOST_REQUIRE_THROW(source->GetAllSeqLocs(scope), CException);
    scope.GetObjectManager().RevokeAllDataLoaders();        
}
BOOST_AUTO_TEST_CASE(wb325_1) {
    string input("gb|ABZI01000088\ngb|ABZN01000067");
    istringstream instream(input);
    
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRetrieveSeqData(false);
    CRef<CBlastInput> source(s_DeclareBlastInput(instream, iconfig));
    CScope scope(*CObjectManager::GetInstance());
    
    BOOST_REQUIRE(source->End() == false);
    blast::TSeqLocVector seqs = source->GetAllSeqLocs(scope);
    BOOST_REQUIRE(source->End() == true);
    BOOST_REQUIRE_EQUAL(2u, seqs.size());
    //blast::SSeqLoc ssl = seqs.front();
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(wb325_2)
{
    string input("gb|ABZN01000067\ngb|ABZI01000088");
    istringstream instream(input);
    
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRetrieveSeqData(false);
    CRef<CBlastInput> source(s_DeclareBlastInput(instream, iconfig));
    CScope scope(*CObjectManager::GetInstance());
    
    BOOST_REQUIRE(source->End() == false);
    blast::TSeqLocVector seqs = source->GetAllSeqLocs(scope);
    BOOST_REQUIRE(source->End() == true);
    BOOST_REQUIRE_EQUAL(2u, seqs.size());
    //blast::SSeqLoc ssl = seqs.front();
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(wb325_single1)
{
    string input("gb|ABZN01000067");
    //string input("218001205");
    istringstream instream(input);
    
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRetrieveSeqData(false);
    CRef<CBlastInput> source(s_DeclareBlastInput(instream, iconfig));
    CScope scope(*CObjectManager::GetInstance());
    
    BOOST_REQUIRE(source->End() == false);
    blast::TSeqLocVector seqs = source->GetAllSeqLocs(scope);
    BOOST_REQUIRE(source->End() == true);
    BOOST_REQUIRE_EQUAL(1u, seqs.size());
    //blast::SSeqLoc ssl = seqs.front();
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(wb325_single2)
{
    string input("gb|ABZI01000088");
    //string input("217999527");
    istringstream instream(input);
    
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRetrieveSeqData(false);
    CRef<CBlastInput> source(s_DeclareBlastInput(instream, iconfig));
    CScope scope(*CObjectManager::GetInstance());
    
    BOOST_REQUIRE(source->End() == false);
    blast::TSeqLocVector seqs = source->GetAllSeqLocs(scope);
    BOOST_REQUIRE(source->End() == true);
    BOOST_REQUIRE_EQUAL(1u, seqs.size());
    //blast::SSeqLoc ssl = seqs.front();
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(ReadSinglePdb)
{
    string pdb_mol("1QCF");
    string pdb_chain("A");
    string pdb(pdb_mol + '_' + pdb_chain);
    istringstream instream(pdb);
    
    const bool is_protein(true);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRetrieveSeqData(false);
    CRef<CBlastInput> source(s_DeclareBlastInput(instream, iconfig));
    CScope scope(*CObjectManager::GetInstance());
    
    BOOST_REQUIRE(source->End() == false);
    blast::TSeqLocVector seqs = source->GetAllSeqLocs(scope);
    blast::SSeqLoc ssl = seqs.front();

    BOOST_REQUIRE(source->End() == true);
    
    BOOST_REQUIRE(ssl.seqloc->IsInt() == true);
    
    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetStrand() == true);
    BOOST_REQUIRE_EQUAL((int)eNa_strand_unknown, (int)ssl.seqloc->GetInt().GetStrand());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetFrom() == true);
    BOOST_REQUIRE_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetTo() == true);
    const TSeqPos length(454);
    BOOST_REQUIRE_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetId() == true);
    BOOST_REQUIRE_EQUAL(CSeq_id::e_Pdb, ssl.seqloc->GetInt().GetId().Which());
    
    BOOST_REQUIRE_EQUAL(pdb_mol, ssl.seqloc->GetInt().GetId().GetPdb().GetMol().Get());
    
    BOOST_REQUIRE(!ssl.mask);
    
    /// Validate the data that would be retrieved by blast.cgi
    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
    BOOST_REQUIRE_EQUAL((size_t)1, bioseqs->GetSeq_set().size());
    BOOST_REQUIRE(bioseqs->GetSeq_set().front()->IsSeq());
    const CBioseq& b = bioseqs->GetSeq_set().front()->GetSeq();
    BOOST_REQUIRE(! b.IsNa());
    BOOST_REQUIRE_EQUAL(CSeq_id::e_Pdb, b.GetId().front()->Which());
    BOOST_REQUIRE_EQUAL(pdb_mol, b.GetId().front()->GetPdb().GetMol().Get());
    BOOST_REQUIRE_EQUAL(CSeq_inst::eRepr_raw, b.GetInst().GetRepr());
    BOOST_REQUIRE(! CSeq_inst::IsNa(b.GetInst().GetMol()));
    BOOST_REQUIRE_EQUAL(length, b.GetInst().GetLength());
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(ThrowOnEmptySequence)
{
    string wgs_master("NZ_ABFD00000000.2"); // Contains no sequence
    istringstream instream(wgs_master);

    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    iconfig.SetRetrieveSeqData(false);
    CRef<CBlastInput> source(s_DeclareBlastInput(instream, iconfig));
    CScope scope(*CObjectManager::GetInstance());
    BOOST_REQUIRE_THROW(source->GetAllSeqLocs(scope), CInputException);
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(FetchSraID)
{
    CNcbiIfstream infile("data/sra_seqid.txt");
    const bool is_protein(false);
    SDataLoaderConfig dlconfig(is_protein,
                               SDataLoaderConfig::eUseGenbankDataLoader);
    CBlastInputSourceConfig iconfig(dlconfig);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));
    CScope scope(*CObjectManager::GetInstance());

    TSeqLocVector seqs = source->GetAllSeqLocs(scope);
    blast::SSeqLoc ssl = seqs.front();
    BOOST_CHECK(source->End() == true);

    // Obtained by running
    // fastq-dump SRR066117 -N 18823 -X 18823 --fasta 80 --split-spot --skip-technical  --minReadLen 6 --clip 
    const string kSeqData =
        "AGCACCACGACTGCTAACCGTAACGCCAGGTGTATAACCTAATGCTTCTTTACAGACTGAAATTGATGCATCTGCATCTC"
        "TTCATTTGTCACAACCGAAATA";

    BOOST_CHECK(ssl.seqloc->IsInt());
    BOOST_REQUIRE(ssl.seqloc->GetId()->IsGeneral());
    BOOST_REQUIRE_EQUAL(CDbtag::eDbtagType_SRA,
                        ssl.seqloc->GetId()->GetGeneral().GetType());

    BOOST_CHECK(ssl.seqloc->GetInt().IsSetFrom() == true);
    BOOST_CHECK_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    BOOST_CHECK(ssl.seqloc->GetInt().IsSetTo() == true);
    BOOST_CHECK_EQUAL(kSeqData.size()-1, ssl.seqloc->GetInt().GetTo());

    const CSeq_id * seqid = ssl.seqloc->GetId();
    CBioseq_Handle bh = scope.GetBioseqHandle(*seqid);
    CSeqVector sv = bh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);

    BOOST_CHECK_EQUAL(kSeqData.size(), sv.size());
    for (size_t i = 0; i < std::min((TSeqPos)kSeqData.size(), sv.size()); i++) {
        CNcbiOstrstream oss;
        oss << "Base number " << i+1 << " differs: got '" 
            << (char)sv[i] << "', expected '" << kSeqData[i]
            << "'";
        string msg = CNcbiOstrstreamToString(oss);
        BOOST_CHECK_MESSAGE((char)sv[i] == kSeqData[i], msg);
        BOOST_CHECK_NE('-', (char)sv[i]);
    }

    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
    const CBioseq& bioseq = bioseqs->GetSeq_set().front()->GetSeq();
    const CSeq_inst& inst = bioseq.GetInst();
    BOOST_CHECK_EQUAL(inst.GetLength(), kSeqData.size());
    BOOST_REQUIRE(inst.IsSetSeq_data());
    const CSeq_data& seq_data = inst.GetSeq_data();
    BOOST_REQUIRE(seq_data.IsIupacna());
    const string& seq = seq_data.GetIupacna().Get();
    for (size_t i = 0; i < seq.size(); i++) {
        CNcbiOstrstream oss;
        oss << "Base number " << i+1 << " differs: got '" 
            << (char)sv[i] << "', expected '" << kSeqData[i]
            << "'";
        string msg = CNcbiOstrstreamToString(oss);
        BOOST_CHECK_MESSAGE((char)sv[i] == kSeqData[i], msg);
        BOOST_CHECK_NE('-', (char)seq[i]);
    }
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

BOOST_AUTO_TEST_CASE(ReadSinglePdb_InDifferentFormats)
{
    string pdb_mol("1IQR");
    string pdb_chain("A");

    for (int i = 0; i < 2; i++) {

        string pdb;
        if (i == 0) {
            pdb.assign(pdb_mol + '|' + pdb_chain);
        } else {
            pdb.assign(pdb_mol + "_" + pdb_chain);
        }
        istringstream instream(pdb);
        
        const bool is_protein(true);
        CBlastInputSourceConfig iconfig(is_protein);
        iconfig.SetRetrieveSeqData(false);
        CRef<CBlastInput> source(s_DeclareBlastInput(instream, iconfig));
        CScope scope(*CObjectManager::GetInstance());
        
        BOOST_REQUIRE(source->End() == false);
        blast::TSeqLocVector seqs = source->GetAllSeqLocs(scope);
        blast::SSeqLoc ssl = seqs.front();
        BOOST_REQUIRE(source->End() == true);
        
        BOOST_REQUIRE(ssl.seqloc->IsInt() == true);
        
        BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetStrand() == true);
        BOOST_REQUIRE_EQUAL((int)eNa_strand_unknown, (int)ssl.seqloc->GetInt().GetStrand());

        BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetFrom() == true);
        BOOST_REQUIRE_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

        BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetTo() == true);
        const TSeqPos length(420);
        BOOST_REQUIRE_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

        BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetId() == true);
        BOOST_REQUIRE_EQUAL(CSeq_id::e_Pdb, ssl.seqloc->GetInt().GetId().Which());
        
        BOOST_REQUIRE_EQUAL(pdb_mol, 
                    ssl.seqloc->GetInt().GetId().GetPdb().GetMol().Get());
        
        BOOST_REQUIRE(!ssl.mask);
        
        /// Validate the data that would be retrieved by blast.cgi
        CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
        BOOST_REQUIRE_EQUAL((size_t)1, bioseqs->GetSeq_set().size());
        BOOST_REQUIRE(bioseqs->GetSeq_set().front()->IsSeq());
        const CBioseq& b = bioseqs->GetSeq_set().front()->GetSeq();
        BOOST_REQUIRE(! b.IsNa());
        BOOST_REQUIRE_EQUAL(CSeq_id::e_Pdb, b.GetId().front()->Which());
        BOOST_REQUIRE_EQUAL(pdb_mol, b.GetId().front()->GetPdb().GetMol().Get());
        BOOST_REQUIRE_EQUAL(CSeq_inst::eRepr_raw, b.GetInst().GetRepr());
        BOOST_REQUIRE(! CSeq_inst::IsNa(b.GetInst().GetMol()));
        BOOST_REQUIRE_EQUAL(length, b.GetInst().GetLength());
        scope.GetObjectManager().RevokeAllDataLoaders();        
    }
    
}

BOOST_AUTO_TEST_CASE(RawFastaNoSpaces_UpperCaseWithN)
{
    CNcbiEnvironment().Set("BLASTINPUT_GEN_DELTA_SEQ", kEmptyStr);
    // this has length 682 and contains an 'N' which without the
    // CFastaReader::fNoSplit flag, produces a delta sequence
    CNcbiIfstream infile("data/nucl_w_n.fsa");
    const bool is_protein(false);
    CBlastInputSourceConfig iconfig(is_protein);
    CRef<CBlastInput> source(s_DeclareBlastInput(infile, iconfig));

    CScope scope(*CObjectManager::GetInstance());
    BOOST_REQUIRE(source->End() == false);
    TSeqLocVector seqs = source->GetAllSeqLocs(scope);
    blast::SSeqLoc ssl = seqs.front();
    BOOST_REQUIRE(source->End() == true);

    BOOST_REQUIRE(ssl.seqloc->IsInt() == true);
    BOOST_REQUIRE(blast::IsLocalId(ssl.seqloc->GetId()) == true);

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetStrand() == true);
    BOOST_REQUIRE_EQUAL((int)eNa_strand_both, (int)ssl.seqloc->GetInt().GetStrand());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetFrom() == true);
    BOOST_REQUIRE_EQUAL((TSeqPos)0, ssl.seqloc->GetInt().GetFrom());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetTo() == true);
    const TSeqPos length(682);
    BOOST_REQUIRE_EQUAL(length-1, ssl.seqloc->GetInt().GetTo());

    BOOST_REQUIRE(ssl.seqloc->GetInt().IsSetId() == true);
    BOOST_REQUIRE_EQUAL(CSeq_id::e_Local, ssl.seqloc->GetInt().GetId().Which());
    BOOST_REQUIRE(!ssl.mask);

    CRef<CBioseq_set> bioseqs = TSeqLocVector2Bioseqs(seqs);
    BOOST_REQUIRE(bioseqs->CanGetSeq_set());
    BOOST_REQUIRE(bioseqs->GetSeq_set().front()->IsSeq());
    BOOST_REQUIRE(bioseqs->GetSeq_set().front()->GetSeq().CanGetInst());
    BOOST_REQUIRE(bioseqs->GetSeq_set().front()->GetSeq().GetInst().CanGetRepr());
    BOOST_REQUIRE_EQUAL(CSeq_inst::eRepr_raw,
                bioseqs->GetSeq_set().front()->GetSeq().GetInst().GetRepr());
    scope.GetObjectManager().RevokeAllDataLoaders();        
}

template <typename T>
inline void s_Ignore(const T&) {}

BOOST_AUTO_TEST_CASE(ParseSequenceRange_EmptyRange) {
    TSeqRange r;
    BOOST_REQUIRE_THROW(r = ParseSequenceRange("4-4"),
                        CBlastException);
    s_Ignore(r); /* to pacify compiler warnings */
}

BOOST_AUTO_TEST_CASE(ParseSequenceRange_0BasedRange) {
    TSeqRange r;
    BOOST_REQUIRE_THROW(r = ParseSequenceRange("0-4"),
                        CBlastException);
    s_Ignore(r); /* to pacify compiler warnings */
}

BOOST_AUTO_TEST_CASE(ParseSequenceRange_InvalidDelimiter) {
    TSeqRange r;
    BOOST_REQUIRE_THROW(r = ParseSequenceRange("3,4"),
                        CBlastException);
    s_Ignore(r); /* to pacify compiler warnings */
}

BOOST_AUTO_TEST_CASE(ParseSequenceRange_IncompleteRange) {
    TSeqRange r;
    BOOST_REQUIRE_THROW(r = ParseSequenceRange("3"),
                        CBlastException);
    BOOST_REQUIRE_THROW(r = ParseSequenceRange("3-"),
                        CBlastException);
    BOOST_REQUIRE_THROW(r = ParseSequenceRange("-3"),
                        CBlastException);
    s_Ignore(r); /* to pacify compiler warnings */
}

BOOST_AUTO_TEST_CASE(ParseSequenceRange_InvalidRange) {
    TSeqRange r;
    BOOST_REQUIRE_THROW(r = ParseSequenceRange("9-4"),
                        CBlastException);
    BOOST_REQUIRE_THROW(r = ParseSequenceRange("-4-2"),
                        CBlastException);
    BOOST_REQUIRE_THROW(r = ParseSequenceRange("-4-9"),
                        CBlastException);
}

BOOST_AUTO_TEST_CASE(ParseSequenceRange_1BasedRange) {
    TSeqRange r = ParseSequenceRange("1-10");
    BOOST_REQUIRE_EQUAL(0U, r.GetFrom());
    BOOST_REQUIRE_EQUAL(9U, r.GetTo());
    BOOST_REQUIRE_EQUAL(10U, r.GetToOpen());    
}

BOOST_AUTO_TEST_CASE(CheckQueryBatchSize) {
    BOOST_REQUIRE_EQUAL(100000, GetQueryBatchSize(eBlastn));
    BOOST_REQUIRE_EQUAL(10000, GetQueryBatchSize(eBlastn, false, true));
}

// Test case for WB-1304: save GI (i.e.: best ranked Seq-id) if available
BOOST_AUTO_TEST_CASE(FetchGiFromAccessionInput) 
{
    const CSeq_id id(CSeq_id::PreferAccessionOverGi() ?
        "ref|NT_026437.13|" : "gi|568802206");
    const string input("NT_026437.13");
    typedef vector<pair<SDataLoaderConfig::EConfigOpts, string> > TVecOpts;
    TVecOpts opts;
    opts.push_back(TVecOpts::value_type(SDataLoaderConfig::eUseGenbankDataLoader, "genbank"));
    opts.push_back(TVecOpts::value_type(SDataLoaderConfig::eUseBlastDbDataLoader, "BLASTDB"));
    ITERATE(TVecOpts, config, opts) {
        CAutoNcbiConfigFile acf(config->first);
        blast::SDataLoaderConfig dlconfig(false);
        if(config->second == "BLASTDB") {
            dlconfig.m_BlastDbName = "refseq_genomic";
        }
        dlconfig.OptimizeForWholeLargeSequenceRetrieval();
        blast::CBlastInputSourceConfig input_config(dlconfig);
        // this needs to be omitted for this test to work
        //input_config.SetRetrieveSeqData(false);   
        CBlastFastaInputSource fasta_input(input, input_config);
        CBlastInput blast_input(&fasta_input);
        //CBlastScopeSourceWrapper scope_source(dlconfig);
        CRef<CScope> scope = CBlastScopeSource(dlconfig).NewScope();
        TSeqLocVector query_loc = blast_input.GetAllSeqLocs(*scope);
        BOOST_REQUIRE_EQUAL(1U, query_loc.size());
        string fasta_id = id.AsFastaString();
        string fasta_query = query_loc[0].seqloc->GetId()->AsFastaString();
        if (fasta_id != fasta_query) {
            BOOST_CHECK_EQUAL(fasta_id, fasta_query);
            BOOST_CHECK_MESSAGE(fasta_id == fasta_query,
                "Failed using " + config->second + " data loader");
        }
        scope->GetObjectManager().RevokeAllDataLoaders();        
    }
    
}


BOOST_AUTO_TEST_SUITE_END() // end of blastinput test suite


BOOST_AUTO_TEST_SUITE(short_reads)

static int s_GetSegmentFlags(const CBioseq& bioseq)
{
    int retval = 0;

    BOOST_REQUIRE(bioseq.IsSetDescr());
    for (auto desc : bioseq.GetDescr().Get()) {
        if (desc->Which() == CSeqdesc::e_User) {

            if (!desc->GetUser().IsSetType() ||
                !desc->GetUser().GetType().IsStr() ||
                desc->GetUser().GetType().GetStr() != "Mapping") {
                continue;
            }

            BOOST_REQUIRE(desc->GetUser().HasField("has_pair"));
            const CUser_field& field = desc->GetUser().GetField("has_pair");
            BOOST_REQUIRE(field.GetData().IsInt());

            retval = field.GetData().GetInt();
        }
    }

    return retval;
}

static string s_GetSequenceId(const CBioseq& bioseq)
{
    string retval;
    if (bioseq.IsSetDescr()) {
        for (auto it: bioseq.GetDescr().Get()) {
            if (it->IsTitle()) {
                vector<string> tokens;
                NStr::Split(it->GetTitle(), " ", tokens);
                retval = (string)"lcl|" + tokens[0];
            }
        }
    }

    if (retval.empty()) {
        retval = bioseq.GetFirstId()->AsFastaString();
    }

    return retval;
}


BOOST_AUTO_TEST_CASE(TestPairedReadsFromFasta) {

    CNcbiIfstream istr("data/paired_reads.fa");
    BOOST_REQUIRE(istr);
    unordered_map<string, int> ref_flags = {
        {"lcl|pair1", eFirstSegment},
        {"lcl|pair2", eLastSegment},
        {"lcl|incomplete1.1", eFirstSegment},
        {"lcl|incomplete1.2", eLastSegment},
        {"lcl|incomplete2.1", eFirstSegment},
        {"lcl|incomplete2.2", eLastSegment},
    };
    
    
    CShortReadFastaInputSource input_source(istr,
                                           CShortReadFastaInputSource::eFasta,
                                           true);

    CBlastInputOMF input(&input_source, 1000);
    CRef<CBioseq_set> queries(new CBioseq_set);
    input.GetNextSeqBatch(*queries);
    BOOST_REQUIRE_EQUAL(queries->GetSeq_set().size(), 6u);

    size_t count = 0;
    for (auto it : queries->GetSeq_set()) {
        string id = s_GetSequenceId(it->GetSeq());
        int flags = s_GetSegmentFlags(it->GetSeq());
        int expected = ref_flags.at(id);

        BOOST_REQUIRE_MESSAGE(flags == expected, (string)"Segment flag for " +
                id + " is different from expected " +
                NStr::IntToString(flags) + " != " +
                NStr::IntToString(expected));
        count++;
    }

    BOOST_REQUIRE_EQUAL(ref_flags.size(), count);
}

BOOST_AUTO_TEST_CASE(TestPairedReadsFromTwoFastaFiles) {

    CNcbiIfstream istr1("data/paired_reads_1.fa");
    CNcbiIfstream istr2("data/paired_reads_2.fa");
    BOOST_REQUIRE(istr1);
    BOOST_REQUIRE(istr2);
    unordered_map<string, int> ref_flags = {
        {"lcl|pair1", eFirstSegment},
        {"lcl|pair2", eLastSegment},
        {"lcl|incomplete1.1", eFirstSegment},
        {"lcl|incomplete1.2", eLastSegment},
        {"lcl|incomplete2.1", eFirstSegment},
        {"lcl|incomplete2.2", eLastSegment},
    };
    

    CShortReadFastaInputSource input_source(istr1, istr2,
                                     CShortReadFastaInputSource::eFasta);

    CBlastInputOMF input(&input_source, 1000);
    CRef<CBioseq_set> queries(new CBioseq_set);
    input.GetNextSeqBatch(*queries);
    BOOST_REQUIRE_EQUAL(queries->GetSeq_set().size(), 6u);

    size_t count = 0;
    for (auto it : queries->GetSeq_set()) {
        string id = s_GetSequenceId(it->GetSeq());
        int flags = s_GetSegmentFlags(it->GetSeq());
        int expected = ref_flags.at(id);

        BOOST_REQUIRE_MESSAGE(flags == expected, (string)"Segment flag for " +
                id + " is different from expected " +
                NStr::IntToString(flags) + " != " +
                NStr::IntToString(expected));
        count++;
    }

    BOOST_REQUIRE_EQUAL(ref_flags.size(), count);
}

BOOST_AUTO_TEST_CASE(TestSingleReadsFromFasta) {

    CNcbiIfstream istr("data/paired_reads.fa");
    CShortReadFastaInputSource input_source(istr,
                                     CShortReadFastaInputSource::eFasta,
                                     false);

    CBlastInputOMF input(&input_source, 1000);
    CRef<CBioseq_set> queries(new CBioseq_set);
    input.GetNextSeqBatch(*queries);
    BOOST_REQUIRE_EQUAL(queries->GetSeq_set().size(), 6u);

    size_t count = 0;
    for (auto it : queries->GetSeq_set()) {
        if (it->GetSeq().IsSetDescr()) {

            string id = s_GetSequenceId(it->GetSeq());
            int flags = s_GetSegmentFlags(it->GetSeq());
            int expected = 0;

            BOOST_REQUIRE_MESSAGE(flags == expected,
                                  (string)"Segment flag for " +
                                  id + " is different from expected " +
                                  NStr::IntToString(flags) + " != " +
                                  NStr::IntToString(expected));
        }
        count++;
    }

    BOOST_REQUIRE_EQUAL(6u, count);
}

BOOST_AUTO_TEST_CASE(TestPairedReadsFromFastQ) {

    CNcbiIfstream istr("data/paired_reads.fastq");
    BOOST_REQUIRE(istr);
    unordered_map<string, int> ref_flags = {
        {"lcl|pair1", eFirstSegment},
        {"lcl|pair2", eLastSegment},
        {"lcl|incomplete1.1", eFirstSegment},
        {"lcl|incomplete1.2", eLastSegment},
        {"lcl|incomplete2.1", eFirstSegment},
        {"lcl|incomplete2.2", eLastSegment},
    };
    
    CShortReadFastaInputSource input_source(istr,
                                     CShortReadFastaInputSource::eFastq,
                                     true);

    CBlastInputOMF input(&input_source, 1000);
    CRef<CBioseq_set> queries(new CBioseq_set);
    input.GetNextSeqBatch(*queries);
    BOOST_REQUIRE_EQUAL(queries->GetSeq_set().size(), 6u);

    size_t count = 0;
    for (auto it : queries->GetSeq_set()) {
        string id = s_GetSequenceId(it->GetSeq());
        int flags = s_GetSegmentFlags(it->GetSeq());
        int expected = ref_flags.at(id);

        BOOST_REQUIRE_MESSAGE(flags == expected, (string)"Segment flag for " +
                id + " is different from expected " +
                NStr::IntToString(flags) + " != " +
                NStr::IntToString(expected));
        count++;
    }

    BOOST_REQUIRE_EQUAL(ref_flags.size(), count);
}

BOOST_AUTO_TEST_CASE(TestPairedReadsFromTwoFastQFiles) {

    CNcbiIfstream istr1("data/paired_reads_1.fastq");
    CNcbiIfstream istr2("data/paired_reads_2.fastq");
    BOOST_REQUIRE(istr1);
    BOOST_REQUIRE(istr2);
    unordered_map<string, int> ref_flags = {
        {"lcl|pair1", eFirstSegment},
        {"lcl|pair2", eLastSegment},
        {"lcl|incomplete1.1", eFirstSegment},
        {"lcl|incomplete1.2", eLastSegment},
        {"lcl|incomplete2.1", eFirstSegment},
        {"lcl|incomplete2.2", eLastSegment},
    };
    
    CShortReadFastaInputSource input_source(istr1, istr2,
                                      CShortReadFastaInputSource::eFastq);

    CBlastInputOMF input(&input_source, 1000);
    CRef<CBioseq_set> queries(new CBioseq_set);
    input.GetNextSeqBatch(*queries);
    BOOST_REQUIRE_EQUAL(queries->GetSeq_set().size(), 6u);

    size_t count = 0;
    for (auto it : queries->GetSeq_set()) {
        string id = s_GetSequenceId(it->GetSeq());
        int flags = s_GetSegmentFlags(it->GetSeq());
        int expected = ref_flags.at(id);

        BOOST_REQUIRE_MESSAGE(flags == expected, (string)"Segment flag for " +
                id + " is different from expected " +
                NStr::IntToString(flags) + " != " +
                NStr::IntToString(expected));
        count++;
    }

    BOOST_REQUIRE_EQUAL(ref_flags.size(), count);
}


BOOST_AUTO_TEST_CASE(TestPairedReadsFromASN1) {

    CNcbiIfstream istr("data/paired_reads.asn");
    BOOST_REQUIRE(istr);
    unordered_map<string, int> ref_flags = {
        {"lcl|pair1", eFirstSegment},
        {"lcl|pair2", eLastSegment},
        {"lcl|incomplete1.1", eFirstSegment},
        {"lcl|incomplete1.2", eLastSegment},
        {"lcl|incomplete2.1", eFirstSegment},
        {"lcl|incomplete2.2", eLastSegment},
    };
    
    CASN1InputSourceOMF input_source(istr, false, true);
    CBlastInputOMF input(&input_source, 1000);
    CRef<CBioseq_set> queries(new CBioseq_set);
    input.GetNextSeqBatch(*queries);
    BOOST_REQUIRE_EQUAL(queries->GetSeq_set().size(), 6u);

    size_t count = 0;
    for (auto it : queries->GetSeq_set()) {
        string id = it->GetSeq().GetFirstId()->AsFastaString();
        int flags = s_GetSegmentFlags(it->GetSeq());
        int expected = ref_flags.at(id);

        BOOST_REQUIRE_MESSAGE(flags == expected, (string)"Segment flag for " +
                id + " is different from expected " +
                NStr::IntToString(flags) + " != " +
                NStr::IntToString(expected));
        count++;
    }

    BOOST_REQUIRE_EQUAL(ref_flags.size(), count);
}

BOOST_AUTO_TEST_CASE(TestPairedReadsFromTwoASN1Files) {

    CNcbiIfstream istr1("data/paired_reads_1.asn");
    CNcbiIfstream istr2("data/paired_reads_2.asn");
    BOOST_REQUIRE(istr1);
    BOOST_REQUIRE(istr2);
    unordered_map<string, int> ref_flags = {
        {"lcl|pair1", eFirstSegment},
        {"lcl|pair2", eLastSegment},
        {"lcl|incomplete1.1", eFirstSegment},
        {"lcl|incomplete1.2", eLastSegment},
        {"lcl|incomplete2.1", eFirstSegment},
        {"lcl|incomplete2.2", eLastSegment},
    };
    
    CASN1InputSourceOMF input_source(istr1, istr2, false);
    CBlastInputOMF input(&input_source, 1000);
    CRef<CBioseq_set> queries(new CBioseq_set);
    input.GetNextSeqBatch(*queries);
    // input file contains six sequences, but two should have been rejected
    // in screening
    BOOST_REQUIRE_EQUAL(queries->GetSeq_set().size(), 6u);

    size_t count = 0;
    for (auto it : queries->GetSeq_set()) {
        string id = it->GetSeq().GetFirstId()->AsFastaString();
        int flags = s_GetSegmentFlags(it->GetSeq());
        int expected = ref_flags.at(id);

        BOOST_REQUIRE_MESSAGE(flags == expected, (string)"Segment flag for " +
                id + " is different from expected " +
                NStr::IntToString(flags) + " != " +
                NStr::IntToString(expected));
        count++;
    }

    BOOST_REQUIRE_EQUAL(ref_flags.size(), count);
}


BOOST_AUTO_TEST_CASE(TestPairedReadsFromFastC) {

    CNcbiIfstream istr("data/paired_reads.fastc");
    BOOST_REQUIRE(istr);
    unordered_map<string, int> ref_flags = {
        {"lcl|read1.1", eFirstSegment},
        {"lcl|read1.2", eLastSegment},
        {"lcl|read2.1", eFirstSegment},
        {"lcl|read2.2", eLastSegment},
        {"lcl|read3.1", eFirstSegment},
        {"lcl|read3.2", eLastSegment},
    };
    
    CShortReadFastaInputSource input_source(istr,
                                     CShortReadFastaInputSource::eFastc, true);
    CBlastInputOMF input(&input_source, 1000);
    CRef<CBioseq_set> queries(new CBioseq_set);
    input.GetNextSeqBatch(*queries);
    BOOST_REQUIRE_EQUAL(queries->GetSeq_set().size(), 6u);

    size_t count = 0;
    for (auto it : queries->GetSeq_set()) {
        string id = s_GetSequenceId(it->GetSeq());
        int flags = s_GetSegmentFlags(it->GetSeq());
        int expected = ref_flags.at(id);

        BOOST_REQUIRE_MESSAGE(flags == expected, (string)"Segment flag for " +
                id + " is different from expected " +
                NStr::IntToString(flags) + " != " +
                NStr::IntToString(expected));
        count++;
    }

    BOOST_REQUIRE_EQUAL(ref_flags.size(), count);
}


BOOST_AUTO_TEST_SUITE_END() // end of short_reads test suite


BOOST_AUTO_TEST_SUITE(blastargs)

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
        NStr::Split(cmd_line_args, " ", retval);
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

    BOOST_REQUIRE_EQUAL(opts->GetOptions().GetMatrixName(), string("BLOSUM80"));
}

BOOST_AUTO_TEST_CASE(RpsBlastCBS)
{
	CRPSBlastAppArgs rpsblast_args;
    	CString2Args s2a("-db ecoli ");
    	auto_ptr<CArgs> args(s2a.CreateCArgs(rpsblast_args));
	CRef<CBlastOptionsHandle> opts = rpsblast_args.SetOptions(*args);
    	BOOST_REQUIRE_EQUAL(opts->GetOptions().GetCompositionBasedStats(), 1);
    	BOOST_REQUIRE(opts->GetOptions().GetSegFiltering() == false);
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
        BOOST_REQUIRE_THROW(args.reset(s2a.CreateCArgs(**itr)), 
                          CArgException);
    }
}

BOOST_AUTO_TEST_CASE(CheckDiscoMegablast) {
    auto_ptr<CArgs> args;
    CBlastnAppArgs blastn_args;

    // missing required template_length argument
    CString2Args s2a("-db ecoli -template_type coding ");
    BOOST_REQUIRE_THROW(args.reset(s2a.CreateCArgs(blastn_args)), 
                      CArgException);
    // missing required template_type argument
    s2a.Reset("-db ecoli -template_length 21 ");
    BOOST_REQUIRE_THROW(args.reset(s2a.CreateCArgs(blastn_args)), 
                      CArgException);

    // valid combination
    s2a.Reset("-db ecoli -template_type coding -template_length 16");
    BOOST_REQUIRE_NO_THROW(args.reset(s2a.CreateCArgs(blastn_args)));

    // test the setting of an invalid word size for disco. megablast
    s2a.Reset("-db ecoli -word_size 32 -template_type optimal -template_length 16");
    BOOST_REQUIRE_NO_THROW(args.reset(s2a.CreateCArgs(blastn_args)));
    CRef<CBlastOptionsHandle> opts;
    BOOST_REQUIRE_THROW(blastn_args.SetOptions(*args), CInputException);
}

BOOST_AUTO_TEST_CASE(CheckPercentIdentity) {
    auto_ptr<CArgs> args;
    CBlastnAppArgs blast_args;

    // invalid value
    CString2Args s2a("-db ecoli -perc_identity 104.3");
    BOOST_REQUIRE_THROW(args.reset(s2a.CreateCArgs(blast_args)), 
                      CArgException);

    // valid combination
    s2a.Reset("-db ecoli -perc_identity 75.0 ");
    BOOST_REQUIRE_NO_THROW(args.reset(s2a.CreateCArgs(blast_args)));
}

BOOST_AUTO_TEST_CASE(CheckNoGreedyExtension) {
    auto_ptr<CArgs> args;
    CBlastnAppArgs blast_args;

    CString2Args s2a("-db ecoli -no_greedy");
    BOOST_REQUIRE_NO_THROW(args.reset(s2a.CreateCArgs(blast_args)));
    CRef<CBlastOptionsHandle> opts;
    // this throws because non-affine gapping costs must be provided for
    // non-greedy extension
    BOOST_REQUIRE_THROW(blast_args.SetOptions(*args), CInputException);
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
        BOOST_REQUIRE_THROW(args.reset(s2a.CreateCArgs(**itr)), 
                          CArgException);

        // valid combination
        s2a.Reset("-db ecoli -culling_limit 0");
        BOOST_REQUIRE_NO_THROW(args.reset(s2a.CreateCArgs(**itr)));
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

BOOST_AUTO_TEST_CASE(CheckQueryCoveragePercent) {
    auto_ptr<CArgs> args;
    CBlastxAppArgs blast_args;

    // invalid value
    CString2Args s2a("-db ecoli -qcov_hsp_perc 100.3");
    BOOST_REQUIRE_THROW(args.reset(s2a.CreateCArgs(blast_args)),
                      CArgException);

    // valid combination
    s2a.Reset("-db ecoli -qcov_hsp_perc 15");
    BOOST_REQUIRE_NO_THROW(args.reset(s2a.CreateCArgs(blast_args)));
}

BOOST_AUTO_TEST_CASE(CheckMaxHspsPerSubject) {
    auto_ptr<CArgs> args;
    CBlastxAppArgs blast_args;

    // invalid value
    CString2Args s2a("-db ecoli -max_hsps 0");
    BOOST_REQUIRE_THROW(args.reset(s2a.CreateCArgs(blast_args)),
                      CArgException);

    // valid combination
    s2a.Reset("-db ecoli -max_hsps 5");
    BOOST_REQUIRE_NO_THROW(args.reset(s2a.CreateCArgs(blast_args)));
}

BOOST_AUTO_TEST_SUITE_END() // end of blastargs test suite

#endif /* SKIP_DOXYGEN_PROCESSING */
