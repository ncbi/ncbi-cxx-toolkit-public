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
                                    CFastaReader::fAllSeqIds | CFastaReader::fParseRawID:
                                    (CFastaReader::fNoParseID |
                                     CFastaReader::fDLOptional);

    // Allow CFastaReader fSkipCheck flag to be set based
    // on new CBlastInputSourceConfig property - GetSkipSeqCheck() -RMH-
    flags += ( m_Config.GetSkipSeqCheck() ? CFastaReader::fSkipCheck : 0 );

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
                               CShortReadFastaInputSource::EInputFormat format,
                               bool paired)
    : m_SeqBuffLen(550),
      m_LineReader(new CStreamLineReader(infile)),
      m_IsPaired(paired),
      m_Format(format),
      m_Id(1),
      m_ParseSeqIds(false)
{
    // allocate sequence buffer
    m_Sequence.resize(m_SeqBuffLen + 1);

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
    }
}

CShortReadFastaInputSource::CShortReadFastaInputSource(CNcbiIstream& infile1,
                               CNcbiIstream& infile2,
                               CShortReadFastaInputSource::EInputFormat format)
    : m_SeqBuffLen(550),
      m_LineReader(new CStreamLineReader(infile1)),
      m_SecondLineReader(new CStreamLineReader(infile2)),
      m_IsPaired(true),
      m_Format(format),
      m_Id(1),
      m_ParseSeqIds(false)
{
    if (m_Format == eFastc) {
        m_LineReader.Reset();
        m_SecondLineReader.Reset();

        NCBI_THROW(CInputException, eInvalidInput, "FASTC format cannot be "
                   "used with two input files");
    }

    // allocate sequence buffer
    m_Sequence.resize(m_SeqBuffLen + 1);

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

int
CShortReadFastaInputSource::GetNextSequence(CBioseq_set& bioseq_set)
{
    m_BasesAdded = 0;

    // read sequernces
    switch (m_Format) {
    case eFasta:
        if (m_SecondLineReader.NotEmpty()) {
            x_ReadFromTwoFiles(bioseq_set, m_Format);
        }
        else {
            x_ReadFastaOrFastq(bioseq_set);
        }
        break;

    case eFastq:
        if (m_SecondLineReader.NotEmpty()) {
            x_ReadFromTwoFiles(bioseq_set, m_Format);
        }
        else {
            x_ReadFastaOrFastq(bioseq_set);
        }
        break;

    case eFastc:
        x_ReadFastc(bioseq_set);
        break;

    default:
        NCBI_THROW(CInputException, eInvalidInput, "Unexpected input format");

    };

    return m_BasesAdded;
}


void
CShortReadFastaInputSource::x_ReadFastaOrFastq(CBioseq_set& bioseq_set)
{
    // tags to indicate paired sequences
    CRef<CSeqdesc> seqdesc_first(new CSeqdesc);
    seqdesc_first->SetUser().SetType().SetStr("Mapping");
    seqdesc_first->SetUser().AddField("has_pair", eFirstSegment);

    CRef<CSeqdesc> seqdesc_last(new CSeqdesc);
    seqdesc_last->SetUser().SetType().SetStr("Mapping");
    seqdesc_last->SetUser().AddField("has_pair", eLastSegment);

    CRef<CSeq_entry> first;
    CRef<CSeq_entry> second;
    switch (m_Format) {
    case eFasta:
        first = x_ReadFastaOneSeq(m_LineReader); 
        break;

    case eFastq:
        first = x_ReadFastqOneSeq(m_LineReader);
        break;

    default:
        NCBI_THROW(CInputException, eInvalidInput, "Invalid input file "
                   "format x_ReadFastaOrFastq read either FASTA or FASTQ");
    }


    // if paired read the next sequence and mark a pair
    if (m_IsPaired) {
        switch (m_Format) {
        case eFasta:
            second = x_ReadFastaOneSeq(m_LineReader);
            break;

        case eFastq:
            second = x_ReadFastqOneSeq(m_LineReader);
            break;
                
        default:
            NCBI_THROW(CInputException, eInvalidInput, "Invalid input file "
                       "format x_ReadFastaOrFastq read either FASTA or "
                       "FASTQ");
        }

        if (first.NotEmpty()) {
            if (second.NotEmpty()) {
                first->SetSeq().SetDescr().Set().push_back(seqdesc_first);
            }
            bioseq_set.SetSeq_set().push_back(first);
        }

        if (second.NotEmpty()) {
            if (first.NotEmpty()) {
                second->SetSeq().SetDescr().Set().push_back(seqdesc_last);
            }
            bioseq_set.SetSeq_set().push_back(second);
        }
    }
    else {
        // otherwise just add the read sequence
        if (first.NotEmpty()) {
            bioseq_set.SetSeq_set().push_back(first);
        }
    }
}


void
CShortReadFastaInputSource::x_ReadFastc(CBioseq_set& bioseq_set)
{
    string id;
    CTempString line;

    // tags to indicate paired sequences
    CRef<CSeqdesc> seqdesc_first(new CSeqdesc);
    seqdesc_first->SetUser().SetType().SetStr("Mapping");
    seqdesc_first->SetUser().AddField("has_pair", eFirstSegment);

    CRef<CSeqdesc> seqdesc_last(new CSeqdesc);
    seqdesc_last->SetUser().SetType().SetStr("Mapping");
    seqdesc_last->SetUser().AddField("has_pair", eLastSegment);

    if (m_LineReader->AtEOF()) {
        return;
    }

    ++(*m_LineReader);
    line = **m_LineReader;

    // ignore empty lines
    while (!m_LineReader->AtEOF() && line.empty()) {
        ++(*m_LineReader);
        line = **m_LineReader;
    }

    if (m_LineReader->AtEOF()) {
        return;
    }

    if (line[0] != '>') {
        NCBI_THROW(CInputException, eInvalidInput,
                   (string)"Missing defline before line: " +
                   NStr::IntToString(m_LineReader->GetLineNumber()));
    }

    id = x_ParseDefline(line);

    if (m_LineReader->AtEOF()) {
        NCBI_THROW(CInputException, eInvalidInput,
                   (string)"No sequence data for defline: " + id +
                   "\nTruncated file?");
    }

    ++(*m_LineReader);
    line = **m_LineReader;
    while (line.empty() && !m_LineReader->AtEOF()) {
        ++(*m_LineReader);
        line = **m_LineReader;
    }

    if (line[0] == '>' || (line.empty() && m_LineReader->AtEOF())) {
        NCBI_THROW(CInputException, eInvalidInput,
                   (string)"No sequence data for defline: " + line);
    }


    // find '><' that separate reads of a pair
    size_t p = line.find('>');
    if (p == CTempString::npos || line[p + 1] != '<') {

        NCBI_THROW(CInputException, eInvalidInput,
                   (string)"FASTC parse error: Sequence separator '><'"
                   " was not found in line: " +
                   NStr::IntToString(m_LineReader->GetLineNumber()));
    }

    // set up reads, there are two sequences in the same line separated
    char* first = (char*)line.data();
    char* second = (char*)line.data() + p + 2;
    size_t first_len = p;
    size_t second_len = line.length() - p - 2;

    {{
            CRef<CSeq_entry> seq_entry(new CSeq_entry);
            CBioseq& bioseq = seq_entry->SetSeq();
            bioseq.SetId().clear();
            if (m_ParseSeqIds) {
                CRef<CSeq_id> seqid(new CSeq_id(id + ".1",
                                                CSeq_id::fParse_AnyLocal));
                bioseq.SetId().push_back(seqid);
            }
            else {
                CRef<CSeqdesc> title(new CSeqdesc);
                title->SetTitle(id + ".1");
                bioseq.SetDescr().Set().push_back(title);
                bioseq.SetId().push_back(x_GetNextSeqId());
            }
            bioseq.SetInst().SetMol(CSeq_inst::eMol_na);
            bioseq.SetInst().SetRepr(CSeq_inst::eRepr_raw);
            bioseq.SetInst().SetLength(first_len);
            first[first_len] = 0;
            bioseq.SetInst().SetSeq_data().SetIupacna(CIUPACna(first));
            bioseq.SetDescr().Set().push_back(seqdesc_first);

            // add a sequence to the batch
            bioseq_set.SetSeq_set().push_back(seq_entry);
    }}
    
    {{
            CRef<CSeq_entry> seq_entry(new CSeq_entry);
            CBioseq& bioseq = seq_entry->SetSeq();
            bioseq.SetId().clear();
            if (m_ParseSeqIds) {
                CRef<CSeq_id> seqid(new CSeq_id(id + ".2",
                                                CSeq_id::fParse_AnyLocal));
                bioseq.SetId().push_back(seqid);
            }
            else {
                CRef<CSeqdesc> title(new CSeqdesc);
                title->SetTitle(id + ".2");
                bioseq.SetDescr().Set().push_back(title);
                bioseq.SetId().push_back(x_GetNextSeqId());
            }
            bioseq.SetInst().SetMol(CSeq_inst::eMol_na);
            bioseq.SetInst().SetRepr(CSeq_inst::eRepr_raw);
            bioseq.SetInst().SetLength(second_len);
            second[second_len] = 0;
            bioseq.SetInst().SetSeq_data().SetIupacna(CIUPACna(second));
            bioseq.SetDescr().Set().push_back(seqdesc_last);

            // add a sequence to the batch
            bioseq_set.SetSeq_set().push_back(seq_entry);
    }}

    m_BasesAdded += first_len + second_len;
    id.clear();
}

CRef<CSeq_entry>
CShortReadFastaInputSource::x_ReadFastaOneSeq(CRef<ILineReader> line_reader)
{
    int start = 0;
    // parse the last read defline
    CTempString line = **line_reader;
    string defline_id = x_ParseDefline(line);
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
            tmp.resize(m_SeqBuffLen);
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
    if (start > 0) {
        CRef<CSeq_entry> seq_entry(new CSeq_entry);
        CBioseq& bioseq = seq_entry->SetSeq();
        bioseq.SetId().clear();
        if (m_ParseSeqIds) {
            CRef<CSeq_id> seqid(new CSeq_id(defline_id,
                                            CSeq_id::fParse_AnyLocal));
            bioseq.SetId().push_back(seqid);
            bioseq.SetDescr();
        }
        else {
            CRef<CSeqdesc> title(new CSeqdesc);
            title->SetTitle(defline_id);
            bioseq.SetDescr().Set().push_back(title);
            bioseq.SetId().push_back(x_GetNextSeqId());
        }
        bioseq.SetInst().SetMol(CSeq_inst::eMol_na);
        bioseq.SetInst().SetRepr(CSeq_inst::eRepr_raw);
        bioseq.SetInst().SetLength(start);
        m_Sequence[start] = 0;
        bioseq.SetInst().SetSeq_data().SetIupacna(CIUPACna(&m_Sequence[0]));

        m_BasesAdded += start;
        return seq_entry;
    }

    return CRef<CSeq_entry>();
}


CRef<CSeq_entry>
CShortReadFastaInputSource::x_ReadFastqOneSeq(CRef<ILineReader> line_reader)
{
    CTempString line;
    string defline_id;
    CRef<CSeq_entry> retval;
    bool empty_sequence = false;

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

    defline_id = x_ParseDefline(line);

    // read sequence
    ++(*line_reader);
    line = **line_reader;
    // skip empty lines
    while (!line_reader->AtEOF() && line.empty()) {
        ++(*line_reader);
        line = **line_reader;
    }

    // set up sequence
    if (line.length() > 0) {
        CRef<CSeq_entry> seq_entry(new CSeq_entry);
        CBioseq& bioseq = seq_entry->SetSeq();
        bioseq.SetId().clear();
        if (m_ParseSeqIds) {
            CRef<CSeq_id> seqid(new CSeq_id(defline_id,
                                            CSeq_id::fParse_AnyLocal));
            bioseq.SetId().push_back(seqid);
            bioseq.SetDescr();
        }
        else {
            CRef<CSeqdesc> title(new CSeqdesc);
            title->SetTitle(defline_id);
            bioseq.SetDescr().Set().push_back(title);
            bioseq.SetId().push_back(x_GetNextSeqId());
        }
        bioseq.SetInst().SetMol(CSeq_inst::eMol_na);
        bioseq.SetInst().SetRepr(CSeq_inst::eRepr_raw);
        // + read instead of a sequence means that the sequence is empty and
        // we reached the second defline
        if (line[0] == '+') {
            bioseq.SetInst().SetLength(0);
            bioseq.SetInst().SetSeq_data().SetIupacna(CIUPACna(""));
            empty_sequence = true;
        }
        else {
            bioseq.SetInst().SetLength(line.length());
            bioseq.SetInst().SetSeq_data().SetIupacna(CIUPACna(line.data()));
            m_BasesAdded += line.length();
        }

        retval = seq_entry;
    }
    
    if (!empty_sequence) {
        // read and skip second defline
        ++(*line_reader);
        line = **line_reader;
        // skip empty lines
        while (!line_reader->AtEOF() && line.empty()) {
            ++(*line_reader);
            line = **line_reader;
        }
    }

    if (line[0] != '+') {
        NCBI_THROW(CInputException, eInvalidInput, (string)"FASTQ parse error:"
                   " defline expected at line: " +
                   NStr::IntToString(line_reader->GetLineNumber()));
    }

    if (!empty_sequence) {
        // read and skip quality scores
        ++(*line_reader);
        line = **line_reader;
        // skip empty lines
        while (!line_reader->AtEOF() && line.empty()) {
            ++(*line_reader);
            line = **line_reader;
        }
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

    CRef<CSeq_entry> first;
    CRef<CSeq_entry> second;

    if (format == eFasta) {
        first = x_ReadFastaOneSeq(m_LineReader); 
        second = x_ReadFastaOneSeq(m_SecondLineReader);
    }
    else {
        first = x_ReadFastqOneSeq(m_LineReader);
        second = x_ReadFastqOneSeq(m_SecondLineReader);
    }

    if (first.NotEmpty()) {
        if (second.NotEmpty()) {
            first->SetSeq().SetDescr().Set().push_back(seqdesc_first);
        }
        bioseq_set.SetSeq_set().push_back(first);
    }

    if (second.NotEmpty()) {
        if (first.NotEmpty()) {
            second->SetSeq().SetDescr().Set().push_back(seqdesc_last);
        }
        bioseq_set.SetSeq_set().push_back(second);
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


CRef<CSeq_id> CShortReadFastaInputSource::x_GetNextSeqId(void)
{
    CRef<CSeq_id> seqid(new CSeq_id);
    seqid->Set(CSeq_id::e_Local, NStr::IntToString(m_Id));
    m_Id++;

    return seqid;
}

END_SCOPE(blast)
END_NCBI_SCOPE
