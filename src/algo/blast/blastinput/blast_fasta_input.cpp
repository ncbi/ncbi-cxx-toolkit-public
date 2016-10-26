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
 * Author:  Jason Papadopoulos
 *
 */

/** @file blast_fasta_input.cpp
 * Convert FASTA-formatted files into blast sequence input
 */

#include <ncbi_pch.hpp>
#include <serial/iterator.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/readers/reader_exception.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>

#include <algo/blast/core/blast_query_info.h>
#include <algo/blast/blastinput/blast_fasta_input.hpp>
#include <algo/blast/blastinput/blast_input_aux.hpp>

#include <objmgr/seq_vector_ci.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)
USING_SCOPE(objects);


/// CFastaReader-derived class which contains customizations for processing
/// BLAST sequence input.
///
/// 1) In BLAST gaps are not accepted, so we create this class to override
/// CFastaReader's behavior when the flag fParseGaps is present, namely to
/// ignore the gaps.
/// 2) Also, this class allows for overriding the logic to set the molecule type
/// for sequences read by CFastaReader @sa kSeqLenThreshold2Guess
class CCustomizedFastaReader : public CFastaReader
{
public:
    /// Constructor
    /// @param reader line reader argument for parent class [in]
    /// @param seqlen_thresh2guess sequence length threshold for molecule
    /// type guessing [in]
    /// @param flags flags for parent class [in]
    CCustomizedFastaReader(ILineReader& reader, 
                           CFastaReader::TFlags flags,
                           unsigned int seq_len_threshold)
        : CFastaReader(reader, flags), m_SeqLenThreshold(seq_len_threshold) {}

    /// Override this method to force the parent class to ignore gaps
    /// @param len length of the gap? @sa CFastaReader
    protected:
    virtual void x_CloseGap(TSeqPos /*len*/, bool /*atStartOfLine*/,
                            ILineErrorListener * /*pMessageListener*/)
        { }

    /// Override logic for assigning the molecule type
    /// @note fForceType is ignored if the sequence length is less than the
    /// value configured in the constructor
    virtual void AssignMolType(ILineErrorListener * pMessageListener) {
        if (GetCurrentPos(eRawPos) < m_SeqLenThreshold) {
            _ASSERT( (TestFlag(fAssumeNuc) ^ TestFlag(fAssumeProt) ) );
            SetCurrentSeq().SetInst().SetMol(TestFlag(fAssumeNuc) 
                                             ? CSeq_inst::eMol_na 
                                             : CSeq_inst::eMol_aa);
        } else {
            CFastaReader::AssignMolType(pMessageListener);
        }
    }

private:
    /// Sequence length threshold for molecule type guessing
    unsigned int m_SeqLenThreshold;
};

/// Class to read non-FASTA sequence input to BLAST programs using the various
/// data loaders configured in CBlastScopeSource objects
class CBlastInputReader : public CCustomizedFastaReader
{
public:
    /// Constructor
    /// @param dlconfig CBlastScopeSource configuration options, used to
    /// instantiate a CScope object to fetch the length of the IDs read by
    /// this class (otherwise it is ignored) [in]
    /// @param read_proteins are we expecting to read proteins? [in]
    /// @param retrieve_seq_data Should the sequence data be fetched by this
    /// library? [in]
    /// @param reader line reader argument for parent class [in]
    /// @param seqlen_thresh2guess sequence length threshold for molecule
    /// type guessing [in]
    /// @param flags flags for parent class [in]
    CBlastInputReader(const SDataLoaderConfig& dlconfig,
                      bool read_proteins,
                      bool retrieve_seq_data,
                      unsigned int seqlen_thresh2guess,
                      ILineReader& reader, 
                      CFastaReader::TFlags flags)
        : CCustomizedFastaReader(reader, flags, seqlen_thresh2guess), 
          m_DLConfig(dlconfig), m_ReadProteins(read_proteins), 
          m_RetrieveSeqData(retrieve_seq_data) {}

    /// Overloaded method to attempt to read non-FASTA input types
    virtual CRef<CSeq_entry> ReadOneSeq(ILineErrorListener * pMessageListener) {
        
        const string line = NStr::TruncateSpaces_Unsafe(*++GetLineReader());
        if ( !line.empty() && isalnum(line.data()[0]&0xff) ) {
            try {
                CRef<CSeq_id> id(new CSeq_id(line, (CSeq_id::fParse_AnyRaw | 
							CSeq_id::fParse_ValidLocal)));
		if (id->IsLocal()  &&  !NStr::StartsWith(line, "lcl|") ) {
                    // Expected to throw an exception.
                    id.Reset(new CSeq_id(line));
		}
                CRef<CBioseq> bioseq(x_CreateBioseq(id));
                CRef<CSeq_entry> retval(new CSeq_entry());
                retval->SetSeq(*bioseq);
                return retval;
            } catch (const CSeqIdException& e) {
                if (NStr::Find(e.GetMsg(), "Malformatted ID") != NPOS) {
                    // This is probably just plain fasta, so just
                    // defer to CFastaReader
                } else {
                    throw;
                }
            } catch (const exception&) {
                throw;
            } catch (...) {
                // in case of other exceptions, just defer to CFastaReader
            }
        } // end if ( !line.empty() ...

        // If all fails, fall back to parent's implementation
        GetLineReader().UngetLine();
        return CFastaReader::ReadOneSeq(pMessageListener);
    }

    /// Retrieves the CBlastScopeSource object used to fetch the query
    /// sequence(s) if these were provided as Seq-ids so that its data
    /// loader(s) can be added to the CScope that contains it.
    CRef<CBlastScopeSource> GetQueryScopeSource() const {
        return m_QueryScopeSource;
    }

private:
    /// Configuration options for the CBlastScopeSource
    const SDataLoaderConfig& m_DLConfig;
    /// True if we're supposed to be reading proteins, else false
    bool m_ReadProteins;
    /// True if the sequence data must be fetched
    bool m_RetrieveSeqData;
    /// The object that creates Bioseqs given SeqIds
    CRef<CBlastBioseqMaker> m_BioseqMaker;
    /// The source of CScope objects to fetch sequences if given by Seq-id
    CRef<CBlastScopeSource> m_QueryScopeSource;

    /// Performs sanity checks to make sure that the sequence requested is of
    /// the expected type. If the tests fail, an exception is thrown.
    /// @param id Sequence id for this sequence [in]
    void x_ValidateMoleculeType(CConstRef<CSeq_id> id) 
    {
        _ASSERT(m_BioseqMaker.NotEmpty());

        if (id.Empty())
        {
            NCBI_THROW(CInputException, eInvalidInput,
                       "Empty SeqID passed to the molecule type validation");
        }

        bool isProtein = m_BioseqMaker->IsProtein(id);
        if (!isProtein && m_ReadProteins)
        {
            NCBI_THROW(CInputException, eSequenceMismatch,
               "GI/accession/sequence mismatch: protein input required but nucleotide provided");
        }
        if (isProtein && !m_ReadProteins)
        {
            NCBI_THROW(CInputException, eSequenceMismatch,
               "GI/accession/sequence mismatch: nucleotide input required but protein provided");
        }

        if (!isProtein)  // Never seen a virtual protein sequence.
        {
             if (m_BioseqMaker->HasSequence(id) == false)
             {
                  string message = "No sequence available for " + id->AsFastaString();
                  NCBI_THROW(CInputException, eInvalidInput, message);
             }
        }
    }

    /// Auxiliary function to create a Bioseq given a CSeq_id ready to be added
    /// to a BlastObject, which does NOT contain sequence data
    /// @param id Sequence id for this bioseq [in]
    CRef<CBioseq> x_CreateBioseq(CRef<CSeq_id> id)
    {
        if (m_BioseqMaker.Empty()) {
            m_QueryScopeSource.Reset(new CBlastScopeSource(m_DLConfig));
            m_BioseqMaker.Reset
                (new CBlastBioseqMaker(m_QueryScopeSource->NewScope()));
        }

        x_ValidateMoleculeType(id);
        return m_BioseqMaker->CreateBioseqFromId(id, m_RetrieveSeqData);
    }

};

/// Stream line reader that converts gaps to Ns before returning each line
class CStreamLineReaderConverter : public CStreamLineReader
{
public:

    CStreamLineReaderConverter(CNcbiIstream& instream)
        : CStreamLineReader(instream) {}

    CStreamLineReaderConverter& operator++(void){
        CStreamLineReader::operator++();
        CTempString line = CStreamLineReader::operator*();
        if (NStr::StartsWith(line, ">")) {
            m_ConvLine = line;
        }
        else {
            m_ConvLine = NStr::Replace(line, "-", "N");
        }
        return *this;
    }

    CTempString operator*(void) const {
        return CTempString(m_ConvLine);
    }

private:
    string m_ConvLine;
};

CBlastFastaInputSource::CBlastFastaInputSource(CNcbiIstream& infile,
                                       const CBlastInputSourceConfig& iconfig)
    : m_Config(iconfig),
      m_LineReader(iconfig.GetConvertGapsToNs() ?
                   new CStreamLineReaderConverter(infile) :
                   new CStreamLineReader(infile)),
      m_ReadProteins(iconfig.IsProteinInput())
{
    x_InitInputReader();
}

CBlastFastaInputSource::CBlastFastaInputSource(const string& user_input,
                                       const CBlastInputSourceConfig& iconfig)
    : m_Config(iconfig),
      m_ReadProteins(iconfig.IsProteinInput())
{
    if (user_input.empty()) {
        NCBI_THROW(CInputException, eEmptyUserInput, 
                   "No sequence input was provided");
    }
    m_LineReader.Reset(new CMemoryLineReader(user_input.c_str(), 
                                             user_input.size()));
    x_InitInputReader();
}

void
CBlastFastaInputSource::x_InitInputReader()
{
    CFastaReader::TFlags flags = m_Config.GetBelieveDeflines() ? 
                                    CFastaReader::fAllSeqIds :
                                    (CFastaReader::fNoParseID |
                                     CFastaReader::fDLOptional);
    flags += (m_ReadProteins
              ? CFastaReader::fAssumeProt 
              : CFastaReader::fAssumeNuc);
    const char* env_var = getenv("BLASTINPUT_GEN_DELTA_SEQ");
    if (env_var == NULL || (env_var && string(env_var) == kEmptyStr)) {
        flags += CFastaReader::fNoSplit;
    }
    // This is necessary to enable the ignoring of gaps in classes derived from
    // CFastaReader

   	flags+= CFastaReader::fHyphensIgnoreAndWarn;

    flags+= CFastaReader::fDisableNoResidues;
    // Do not check more than few characters in local ID for illegal characters.
    // Illegal characters can be things like = and we want to let those through.
    flags+= CFastaReader::fQuickIDCheck;

    if (m_Config.GetDataLoaderConfig().UseDataLoaders()) {
        m_InputReader.reset
            (new CBlastInputReader(m_Config.GetDataLoaderConfig(), 
                                   m_ReadProteins, 
                                   m_Config.RetrieveSeqData(),
                                   m_Config.GetSeqLenThreshold2Guess(),
                                   *m_LineReader, 
                                   flags));
    } else {
        m_InputReader.reset(new CCustomizedFastaReader(*m_LineReader, flags,
                                       m_Config.GetSeqLenThreshold2Guess()));
    }

    m_InputReader->IgnoreProblem(ILineError::eProblem_ModifierFoundButNoneExpected);
    m_InputReader->IgnoreProblem(ILineError::eProblem_TooLong);
    m_InputReader->IgnoreProblem(ILineError::eProblem_TooManyAmbiguousResidues);
    //m_InputReader->IgnoreProblem(ILineError::eProblem_InvalidResidue);
    //m_InputReader->IgnoreProblem(ILineError::eProblem_IgnoredResidue);

    CRef<CSeqIdGenerator> idgen
        (new CSeqIdGenerator(m_Config.GetLocalIdCounterInitValue(),
                             m_Config.GetLocalIdPrefix()));
    m_InputReader->SetIDGenerator(*idgen);
}

bool
CBlastFastaInputSource::End()
{
    return m_LineReader->AtEOF();
}

CRef<CSeq_loc>
CBlastFastaInputSource::x_FastaToSeqLoc(CRef<objects::CSeq_loc>& lcase_mask,
                                        CScope& scope)
{
    static const TSeqRange kEmptyRange(TSeqRange::GetEmpty());
    CRef<CBlastScopeSource> query_scope_source;

    if (m_Config.GetLowercaseMask())
        lcase_mask = m_InputReader->SaveMask();

    CRef<CSeq_entry> seq_entry(m_InputReader->ReadOneSeq());
    if (lcase_mask) {
        if (lcase_mask->Which() != CSeq_loc::e_not_set) {
            lcase_mask->SetStrand(eNa_strand_plus);
        }
        _ASSERT(lcase_mask->GetStrand() == eNa_strand_plus ||
                lcase_mask->GetStrand() == eNa_strand_unknown);
    }
    _ASSERT(seq_entry.NotEmpty());
    scope.AddTopLevelSeqEntry(*seq_entry);

    CTypeConstIterator<CBioseq> itr(ConstBegin(*seq_entry));

    CRef<CSeq_loc> retval(new CSeq_loc());

    if ( !blast::HasRawSequenceData(*itr) ) {
        CBlastInputReader* blast_reader = 
            dynamic_cast<CBlastInputReader*>(m_InputReader.get());
        _ASSERT(blast_reader);
        CRef<CBlastScopeSource> query_scope_source =
            blast_reader->GetQueryScopeSource();
        query_scope_source->AddDataLoaders(CRef<CScope>(&scope));
    }

    if (m_ReadProteins && itr->IsNa()) {
        NCBI_THROW(CInputException, eSequenceMismatch,
                   "Nucleotide FASTA provided for protein sequence");
    } else if ( !m_ReadProteins && itr->IsAa() ) {
        NCBI_THROW(CInputException, eSequenceMismatch,
                   "Protein FASTA provided for nucleotide sequence");
    }

    // set strand
    if (m_Config.GetStrand() == eNa_strand_other ||
        m_Config.GetStrand() == eNa_strand_unknown) {
        if (m_ReadProteins)
            retval->SetInt().SetStrand(eNa_strand_unknown);
        else
            retval->SetInt().SetStrand(eNa_strand_both);
    } else {
        if (m_ReadProteins) {
            NCBI_THROW(CInputException, eInvalidStrand,
                       "Cannot assign nucleotide strand to protein sequence");
        }
        retval->SetInt().SetStrand(m_Config.GetStrand());
    }

    // sanity checks for the range
    const TSeqPos from = m_Config.GetRange().GetFrom() == kEmptyRange.GetFrom()
        ? 0 : m_Config.GetRange().GetFrom();
    const TSeqPos to = m_Config.GetRange().GetTo() == kEmptyRange.GetTo()
        ? 0 : m_Config.GetRange().GetTo();

    // Get the sequence length
    const TSeqPos seqlen = seq_entry->GetSeq().GetInst().GetLength();
    //if (seqlen == 0) {
    //    NCBI_THROW(CInputException, eEmptyUserInput,
    //               "Query contains no sequence data");
    //}
    _ASSERT(seqlen != numeric_limits<TSeqPos>::max());
    if (to > 0 && to < from) {
        NCBI_THROW(CInputException, eInvalidRange, 
                   "Invalid sequence range");
    }
    if (from > seqlen) {
        NCBI_THROW(CInputException, eInvalidRange, 
                   "Invalid from coordinate (greater than sequence length)");
    }
    // N.B.: if the to coordinate is greater than or equal to the sequence
    // length, we fix that silently


    // set sequence range
    retval->SetInt().SetFrom(from);
    retval->SetInt().SetTo((to > 0 && to < seqlen) ? to : (seqlen-1));

    // set ID
    retval->SetInt().SetId().Assign(*FindBestChoice(itr->GetId(), CSeq_id::BestRank));

    return retval;
}


SSeqLoc
CBlastFastaInputSource::GetNextSSeqLoc(CScope& scope)
{
    CRef<CSeq_loc> lcase_mask;
    CRef<CSeq_loc> seqloc = x_FastaToSeqLoc(lcase_mask, scope);
    
    SSeqLoc retval(seqloc, &scope);
    if (m_Config.GetLowercaseMask()) {
        retval.mask = lcase_mask;
    }

    return retval;
}

CRef<CBlastSearchQuery>
CBlastFastaInputSource::GetNextSequence(CScope& scope)
{
    CRef<CSeq_loc> lcase_mask;
    CRef<CSeq_loc> seqloc = x_FastaToSeqLoc(lcase_mask, scope);

    TMaskedQueryRegions masks_in_query;
    if (m_Config.GetLowercaseMask()) {
        const EBlastProgramType program = m_ReadProteins ? 
                                eBlastTypeBlastp : eBlastTypeBlastn;
        // masks are independent from the strand specification for the
        // query/subj to search
        const bool apply_mask_to_both_strands = true;
        masks_in_query = 
            PackedSeqLocToMaskedQueryRegions(
                                static_cast<CConstRef<CSeq_loc> >(lcase_mask),
                                program, apply_mask_to_both_strands);
    }
    return CRef<CBlastSearchQuery>
        (new CBlastSearchQuery(*seqloc, scope, masks_in_query));
}


CShortReadFastaInputSource::CShortReadFastaInputSource(CNcbiIstream& infile,
                               TSeqPos num_seqs,
                               CShortReadFastaInputSource::EInputFormat format,
                               bool paired,
                               bool validate)
    : m_NumSeqsInBatch(num_seqs),
      m_SeqBuffLen(550),
      m_LineReader(new CStreamLineReader(infile)),
      m_IsPaired(paired),
      m_Validate(validate),
      m_NumRejected(0),
      m_Format(format)
{
    // allocate sequence buffer
    m_Sequence.reserve(m_SeqBuffLen + 1);

    // read the first line for FASTA input
    if (m_Format == eFasta) {
        do {
            ++(*m_LineReader);
            m_Line = **m_LineReader;
        } while (m_Line.empty() && !m_LineReader->AtEOF());

        if (m_Line[0] != '>') {
            NCBI_THROW(CInputException, eInvalidInput, "FASTA parse error: "
                       "defline expected");
        }
    }
}

CShortReadFastaInputSource::CShortReadFastaInputSource(CNcbiIstream& infile1,
                               CNcbiIstream& infile2,
                               TSeqPos num_seqs,
                               CShortReadFastaInputSource::EInputFormat format,
                               bool validate)
    : m_NumSeqsInBatch(num_seqs),
      m_SeqBuffLen(550),
      m_LineReader(new CStreamLineReader(infile1)),
      m_SecondLineReader(new CStreamLineReader(infile2)),
      m_IsPaired(true),
      m_Validate(validate),
      m_NumRejected(0),
      m_Format(format)
{
    if (m_Format == eFastc) {
        m_LineReader.Reset();
        m_SecondLineReader.Reset();

        NCBI_THROW(CInputException, eInvalidInput, "FASTC format cannot be "
                   "used with two input files");
    }

    // allocate sequence buffer
    m_Sequence.reserve(m_SeqBuffLen + 1);

    // read the first line for FASTA input
    if (m_Format == eFasta) {
        CTempString line;
        do {
            ++(*m_LineReader);
            line = **m_LineReader;
        } while (line.empty() && !m_LineReader->AtEOF());

        if (line[0] != '>') {
            NCBI_THROW(CInputException, eInvalidInput, "FASTA parse error: "
                       "defline expected");
        }
    
        do {
            ++(*m_SecondLineReader);
            line = **m_SecondLineReader;
        } while (line.empty() && !m_SecondLineReader->AtEOF());

        if (line[0] != '>') {
            NCBI_THROW(CInputException, eInvalidInput, "FASTA parse error: "
                       "defline expected");
        }
    }
}

void
CShortReadFastaInputSource::GetNextNumSequences(CBioseq_set& bioseq_set,
                                                TSeqPos /* num_seqs */)
{
    // preallocate and initialize objects for sequences to be read
    m_SeqIds.clear();
    m_Entries.clear();

    // +1 in case we need to read one more sequence so that a pair is not
    // broken
    m_SeqIds.resize(m_NumSeqsInBatch + 1);
    m_Entries.resize(m_NumSeqsInBatch + 1);

    // allocate seq_id and seq_entry objects, they will be reused to minimize
    // time spent on memory management
    for (TSeqPos i=0;i < m_NumSeqsInBatch + 1;i++) {
        m_SeqIds[i].Reset(new CSeq_id);
        m_Entries[i].Reset(new CSeq_entry);
        m_Entries[i]->SetSeq().SetInst().SetMol(CSeq_inst::eMol_na);
        m_Entries[i]->SetSeq().SetInst().SetRepr(CSeq_inst::eRepr_raw);
    }

    // read sequernces
    switch (m_Format) {
    case eFasta:
        if (m_SecondLineReader.NotEmpty()) {
            x_ReadFromTwoFiles(bioseq_set, m_Format);
        }
        else {
            x_ReadFasta(bioseq_set);
        }
        break;

    case eFastq:
        if (m_SecondLineReader.NotEmpty()) {
            x_ReadFromTwoFiles(bioseq_set, m_Format);
        }
        else {
            x_ReadFastq(bioseq_set);
        }
        break;

    case eFastc:
        x_ReadFastc(bioseq_set);
        break;

    default:
        NCBI_THROW(CInputException, eInvalidInput, "Unexpected input format");

    };

    // detach CRefs from in m_Entries and m_SeqIds from objects in bioseq_set
    // for thread safety
    m_Entries.clear();
    m_SeqIds.clear();
}

void
CShortReadFastaInputSource::x_ReadFasta(CBioseq_set& bioseq_set)
{
    TSeqPos index = 0;
    int start = 0;
    // parse the last read defline
    CTempString id = x_ParseDefline(m_Line);
    m_SeqIds[0]->Set(CSeq_id::e_Local, id);
    int current_read = 0;
    bool first_added = false;

    // tags to indicate paired sequences
    CRef<CSeqdesc> seqdesc_first(new CSeqdesc);
    seqdesc_first->SetUser().SetType().SetStr("Mapping");
    seqdesc_first->SetUser().AddField("has_pair", eFirstSegment);

    CRef<CSeqdesc> seqdesc_last(new CSeqdesc);
    seqdesc_last->SetUser().SetType().SetStr("Mapping");
    seqdesc_last->SetUser().AddField("has_pair", eLastSegment);

    CRef<CSeqdesc> seqdesc_first_partial(new CSeqdesc);
    seqdesc_first_partial->SetUser().SetType().SetStr("Mapping");
    seqdesc_first_partial->SetUser().AddField("has_pair",
                                              fFirstSegmentFlag | fPartialFlag);

    CRef<CSeqdesc> seqdesc_last_partial(new CSeqdesc);
    seqdesc_last_partial->SetUser().SetType().SetStr("Mapping");
    seqdesc_last_partial->SetUser().AddField("has_pair",
                                             fLastSegmentFlag | fPartialFlag);

    while (index < m_NumSeqsInBatch && !m_LineReader->AtEOF()) {
        ++(*m_LineReader);
        m_Line = **m_LineReader;

        // ignore empty lines
        if (m_Line.empty()) {
            continue;
        }

        // if defline
        if (m_Line[0] == '>') {

            // set up sequence
            if (!m_Validate || x_ValidateSequence(m_Sequence.data(), start)) {
                CBioseq& bioseq = m_Entries[index]->SetSeq();
                bioseq.SetId().clear();
                bioseq.SetId().push_back(m_SeqIds[index]);
                bioseq.SetInst().SetLength(start);
                m_Sequence[start] = 0;
                bioseq.SetInst().SetSeq_data().SetIupacna(CIUPACna(
                                                            &m_Sequence[0]));

                if (m_IsPaired && (current_read & 1) == 0) {
                    first_added = true;
                }

                if (m_IsPaired && (current_read & 1) == 1) {
                    if (first_added) {
                        // set paired tags: both sequences of the pair passed
                        // validation
                        bioseq_set.SetSeq_set().back()->SetSeq().SetDescr().
                            Set().push_back(seqdesc_first);

                        m_Entries[index]->SetSeq().SetDescr().Set().push_back(
                                                               seqdesc_last);
                    }
                    else {
                        // only the second sequence of the pair passed
                        // validation
                        m_Entries[index]->SetSeq().SetDescr().Set().push_back(
                                                       seqdesc_last_partial);
                    }
                    first_added = false;
                }

                // add a sequence to the batch
                bioseq_set.SetSeq_set().push_back(m_Entries[index]);

                index++;
            }
            else {
                if (first_added) {
                    // only the first sequence of the pair passed validation
                    bioseq_set.SetSeq_set().back()->SetSeq().SetDescr().
                        Set().push_back(seqdesc_first_partial);
                    
                }

                m_NumRejected++;
                first_added = false;
            }
            current_read++;

            start = 0;

            // set sequence id for the new sequence
            if (index < m_NumSeqsInBatch) {
                id = x_ParseDefline(m_Line);
                m_SeqIds[index]->Set(CSeq_id::e_Local, id);
            }
        }
        else {
            // otherwise copy the sequence
            // increase the sequence buffer if necessary
            if (start + m_Line.length() + 1 > m_SeqBuffLen) {
                string tmp;
                m_SeqBuffLen = 2 * (start + m_Line.length() + 1);
                tmp.reserve(m_SeqBuffLen);
                memcpy(&tmp[0], &m_Sequence[0], start);
                m_Sequence.swap(tmp);
            }
            memcpy(&m_Sequence[start], m_Line.data(), m_Line.length());
            start += m_Line.length();
        }
    }

    if (!m_LineReader->AtEOF()) {
        return;
    }

    if (m_Validate && (!x_ValidateSequence(m_Sequence.data(), start))) {
        m_NumRejected++;
        if (m_IsPaired) { 
            if (first_added && (current_read & 1)  == 1) {
                bioseq_set.SetSeq_set().back()->SetSeq().SetDescr().Set().
                    push_back(seqdesc_first_partial);
            }
        }

        return;
    }

    if (m_IsPaired && (current_read & 1) == 1 && first_added) {
        // add the tag for paired reads
        bioseq_set.SetSeq_set().back()->SetSeq().SetDescr().Set().
            push_back(seqdesc_first);
    }

    // set up the last sequence read
    CBioseq& bioseq = m_Entries[index]->SetSeq();
    bioseq.SetId().clear();
    bioseq.SetId().push_back(m_SeqIds[index]);
    bioseq.SetInst().SetLength(start);
    m_Sequence[start] = 0;
    bioseq.SetInst().SetSeq_data().SetIupacna(CIUPACna(&m_Sequence[0]));
    bioseq_set.SetSeq_set().push_back(m_Entries[index]);    

    if (m_IsPaired && (current_read & 1) == 1) {
        bioseq_set.SetSeq_set().back()->SetSeq().SetDescr().Set().
            push_back(first_added ? seqdesc_last : seqdesc_last_partial);
    }
}


void
CShortReadFastaInputSource::x_ReadFastc(CBioseq_set& bioseq_set)
{
    TSeqPos index = 0;
    string id;

    // tags to indicate paired sequences
    CRef<CSeqdesc> seqdesc_first(new CSeqdesc);
    seqdesc_first->SetUser().SetType().SetStr("Mapping");
    seqdesc_first->SetUser().AddField("has_pair", eFirstSegment);

    CRef<CSeqdesc> seqdesc_last(new CSeqdesc);
    seqdesc_last->SetUser().SetType().SetStr("Mapping");
    seqdesc_last->SetUser().AddField("has_pair", eLastSegment);

    while (index < m_NumSeqsInBatch && !m_LineReader->AtEOF()) {
        ++(*m_LineReader);
        m_Line = **m_LineReader;

        // ignore empty lines
        if (m_Line.empty()) {
            continue;
        }

        // if defline
        if (m_Line[0] == '>') {
            id = x_ParseDefline(m_Line);
        }
        else {
            // otherwise sequence

            // make sure that a defline was read first
            if (id.empty()) {
                NCBI_THROW(CInputException, eInvalidInput,
                           (string)"Missing defline before line: " +
                           NStr::IntToString(m_LineReader->GetLineNumber()));
            }

            // find '><' that separate reads of a pair
            size_t p = m_Line.find('>');
            if (p == CTempString::npos || m_Line[p + 1] != '<') {

                NCBI_THROW(CInputException, eInvalidInput,
                           (string)"FASTC parse error: Sequence separator '><'"
                           " was not found in line: " +
                           NStr::IntToString(m_LineReader->GetLineNumber()));
            }

            // set up reads, there are two sequences in the same line separated
            char* first = (char*)m_Line.data();
            char* second = (char*)m_Line.data() + p + 2;
            size_t first_len = p;
            size_t second_len = m_Line.length() - p - 2;

            // FIXME: Both reads are rejected if only one fails validation
            if (x_ValidateSequence(first, first_len) &&
                x_ValidateSequence(second, second_len)) {

                {{
                    CBioseq& bioseq = m_Entries[index]->SetSeq();
                    bioseq.SetId().clear();
                    m_SeqIds[index]->Set(CSeq_id::e_Local, id + ".1");
                    bioseq.SetId().push_back(m_SeqIds[index]);
                    bioseq.SetInst().SetLength(first_len);
                    first[first_len] = 0;
                    bioseq.SetInst().SetSeq_data().SetIupacna(CIUPACna(first));

                    bioseq.SetDescr().Set().push_back(seqdesc_first);
                }}
                // add a sequence to the batch
                bioseq_set.SetSeq_set().push_back(m_Entries[index]);
                index++;

                {{
                    CBioseq& bioseq = m_Entries[index]->SetSeq();
                    bioseq.SetId().clear();
                    m_SeqIds[index]->Set(CSeq_id::e_Local, id + ".2");
                    bioseq.SetId().push_back(m_SeqIds[index]);
                    bioseq.SetInst().SetLength(second_len);
                    second[second_len] = 0;
                    bioseq.SetInst().SetSeq_data().SetIupacna(CIUPACna(second));

                    bioseq.SetDescr().Set().push_back(seqdesc_last);
                }}
                // add a sequence to the batch
                bioseq_set.SetSeq_set().push_back(m_Entries[index]);
                index++;
            }
            else {
                m_NumRejected++;
            }
            id.clear();
        }
    }
}


void
CShortReadFastaInputSource::x_ReadFastq(CBioseq_set& bioseq_set)
{
    int current_read = 0;
    bool first_added = false;

    // tags to indicate paired sequences
    CRef<CSeqdesc> seqdesc_first(new CSeqdesc);
    seqdesc_first->SetUser().SetType().SetStr("Mapping");
    seqdesc_first->SetUser().AddField("has_pair", eFirstSegment);

    CRef<CSeqdesc> seqdesc_last(new CSeqdesc);
    seqdesc_last->SetUser().SetType().SetStr("Mapping");
    seqdesc_last->SetUser().AddField("has_pair", eLastSegment);

    CRef<CSeqdesc> seqdesc_first_partial(new CSeqdesc);
    seqdesc_first_partial->SetUser().SetType().SetStr("Mapping");
    seqdesc_first_partial->SetUser().AddField("has_pair",
                                              fFirstSegmentFlag | fPartialFlag);

    CRef<CSeqdesc> seqdesc_last_partial(new CSeqdesc);
    seqdesc_last_partial->SetUser().SetType().SetStr("Mapping");
    seqdesc_last_partial->SetUser().AddField("has_pair",
                                             fLastSegmentFlag | fPartialFlag);

    m_Index = 0;
    while (m_Index < (int)m_NumSeqsInBatch && !m_LineReader->AtEOF()) {
        int index = x_ReadFastqOneSeq(m_LineReader);

        if (index >= 0) {

            if (m_IsPaired && (current_read & 1) == 0) {
                first_added = true;
            }

            if (m_IsPaired && (current_read & 1) == 1) {
                if (first_added) {
                    // this field indicates that this sequence has a pair
                    bioseq_set.SetSeq_set().back()->SetSeq().SetDescr().Set().
                        push_back(seqdesc_first);

                    m_Entries[index]->SetSeq().SetDescr().Set().push_back(
                                                                seqdesc_last);
                }
                else {
                    m_Entries[index]->SetSeq().SetDescr().Set().push_back(
                                                        seqdesc_last_partial);
                }
                first_added = false;
            }

            bioseq_set.SetSeq_set().push_back(m_Entries[index]);
        }
        else {
            if (first_added) {
                bioseq_set.SetSeq_set().back()->SetSeq().SetDescr().Set().
                    push_back(seqdesc_first_partial);
            }

            m_NumRejected++;
            first_added = false;
        }
        current_read++;
    }

}


int
CShortReadFastaInputSource::x_ReadFastaOneSeq(CRef<ILineReader> line_reader)
{
    int start = 0;
    // parse the last read defline
    CTempString line = **line_reader;
    CTempString id = x_ParseDefline(line);
    m_SeqIds[m_Index]->Set(CSeq_id::e_Local, id);
    ++(*line_reader);
    line = **line_reader;
    while (line[0] != '>') {

        // ignore empty lines
        if (line.empty() && !line_reader->AtEOF()) {
            ++(*line_reader);
            line = **line_reader;
            continue;
        }

        // copy the sequence
        // increase the sequence buffer if necessary
        if (start + line.length() + 1 > m_SeqBuffLen) {
            string tmp;
            m_SeqBuffLen = 2 * (start + line.length() + 1);
            tmp.reserve(m_SeqBuffLen);
            memcpy(&tmp[0], &m_Sequence[0], start);
            m_Sequence.swap(tmp);
        }
        memcpy(&m_Sequence[start], line.data(), line.length());
        start += line.length();

        if (line_reader->AtEOF()) {
            break;
        }

        // read next line
        ++(*line_reader);
        line = **line_reader;
    }

    // set up sequence
    if (!m_Validate || x_ValidateSequence(m_Sequence.data(), start)) {

        CBioseq& bioseq = m_Entries[m_Index]->SetSeq();
        bioseq.SetId().clear();
        bioseq.SetId().push_back(m_SeqIds[m_Index]);
        bioseq.SetInst().SetLength(start);
        m_Sequence[start] = 0;
        bioseq.SetInst().SetSeq_data().SetIupacna(CIUPACna(&m_Sequence[0]));

        m_Index++;
        return m_Index - 1;
    }

    return -1;
}


int
CShortReadFastaInputSource::x_ReadFastqOneSeq(CRef<ILineReader> line_reader)
{
    CTempString line;
    CTempString id;
    int retval = -1;

    // first read defline
    ++(*line_reader);
    line = **line_reader;

    // skip empty lines
    while (!line_reader->AtEOF() && line.empty()) {
        ++(*line_reader);
        line = **line_reader;
    }

    if (line[0] != '@') {
        NCBI_THROW(CInputException, eInvalidInput, (string)"FASTQ parse error:"
                   " defline expected at line: " +
                   NStr::IntToString(line_reader->GetLineNumber()));
    }

    id = x_ParseDefline(line);
    m_SeqIds[m_Index]->Set(CSeq_id::e_Local, id);

    // read sequence
    ++(*line_reader);
    line = **line_reader;
    // skip empty lines
    while (!line_reader->AtEOF() && line.empty()) {
        ++(*line_reader);
        line = **line_reader;
    }

    // set up sequence
    if (!m_Validate || x_ValidateSequence(line.data(), line.length())) {

        CBioseq& bioseq = m_Entries[m_Index]->SetSeq();
        bioseq.SetId().clear();
        bioseq.SetId().push_back(m_SeqIds[m_Index]);
        bioseq.SetInst().SetLength(line.length());
        bioseq.SetInst().SetSeq_data().SetIupacna(CIUPACna(line.data()));

        m_Index++;
        retval =  m_Index - 1;
    }
    
    // read and skip second defline
    ++(*line_reader);
    line = **line_reader;
    // skip empty lines
    while (!line_reader->AtEOF() && line.empty()) {
        ++(*line_reader);
        line = **line_reader;
    }

    if (line[0] != '+') {
        NCBI_THROW(CInputException, eInvalidInput, (string)"FASTQ parse error:"
                   " defline expected at line: " +
                   NStr::IntToString(line_reader->GetLineNumber()));
    }

    // read and skip quality scores
    ++(*line_reader);
    line = **line_reader;
    // skip empty lines
    while (!line_reader->AtEOF() && line.empty()) {
        ++(*line_reader);
        line = **line_reader;
    }

    return retval;
}


bool
CShortReadFastaInputSource::x_ReadFromTwoFiles(CBioseq_set& bioseq_set,
                            CShortReadFastaInputSource::EInputFormat format)
{
    if (format == eFastc) {
        NCBI_THROW(CInputException, eInvalidInput, "FASTC format cannot be "
                   "used with two files");
    }

    // tags to indicate paired sequences
    CRef<CSeqdesc> seqdesc_first(new CSeqdesc);
    seqdesc_first->SetUser().SetType().SetStr("Mapping");
    seqdesc_first->SetUser().AddField("has_pair", eFirstSegment);

    CRef<CSeqdesc> seqdesc_last(new CSeqdesc);
    seqdesc_last->SetUser().SetType().SetStr("Mapping");
    seqdesc_last->SetUser().AddField("has_pair", eLastSegment);

    CRef<CSeqdesc> seqdesc_first_partial(new CSeqdesc);
    seqdesc_first_partial->SetUser().SetType().SetStr("Mapping");
    seqdesc_first_partial->SetUser().AddField("has_pair",
                                              fFirstSegmentFlag | fPartialFlag);

    CRef<CSeqdesc> seqdesc_last_partial(new CSeqdesc);
    seqdesc_last_partial->SetUser().SetType().SetStr("Mapping");
    seqdesc_last_partial->SetUser().AddField("has_pair",
                                             fLastSegmentFlag | fPartialFlag);

    int index1;
    int index2;
    m_Index = 0;
    while (m_Index < (int)m_NumSeqsInBatch && !m_LineReader->AtEOF() &&
           !m_SecondLineReader->AtEOF()) {

        if (format == eFasta) {
            index1 = x_ReadFastaOneSeq(m_LineReader); 
            index2 = x_ReadFastaOneSeq(m_SecondLineReader);
        }
        else {
            index1 = x_ReadFastqOneSeq(m_LineReader);
            index2 = x_ReadFastqOneSeq(m_SecondLineReader);
        }

        if (index1 >= 0) {
            if (index2 >= 0) {
                m_Entries[index1]->SetSeq().SetDescr().Set().push_back(
                                                              seqdesc_first);
            }
            else {
                m_Entries[index1]->SetSeq().SetDescr().Set().push_back(
                                                      seqdesc_first_partial);
            }
            bioseq_set.SetSeq_set().push_back(m_Entries[index1]);
        }

        if (index2 >= 0) {
            if (index1 >= 0) {
                m_Entries[index2]->SetSeq().SetDescr().Set().push_back(
                                                               seqdesc_last);
            }
            else {
                m_Entries[index2]->SetSeq().SetDescr().Set().push_back(
                                                       seqdesc_last_partial);
            }
            bioseq_set.SetSeq_set().push_back(m_Entries[index2]);
        }
    }

    return true;
}


CTempString CShortReadFastaInputSource::x_ParseDefline(CTempString& line)
{
    // set local sequence id for the new sequence as the string between '>'
    // and the first space
    size_t begin = 1;
    size_t end = line.find(' ', 1);
    CTempString id = line.substr(begin, end - begin);
    return id;
}


bool CShortReadFastaInputSource::x_ValidateSequence(const char* sequence,
                                                    int length)
{
    const char* s = sequence;
    const int kNBase = (int)'N';
    const double kMaxFractionAmbiguousBases = 0.5;
    int num = 0;
    for (int i=0;i < length;i++) {
        num += (toupper((int)s[i]) == kNBase);
    }

    if ((double)num / length > kMaxFractionAmbiguousBases) {
        return false;
    }

    int entropy = FindDimerEntropy(sequence, length);
    return entropy > 16;
}

END_SCOPE(blast)
END_NCBI_SCOPE
