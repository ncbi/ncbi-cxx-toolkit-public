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

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <serial/iterator.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/readers/reader_exception.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>

#include <algo/blast/blastinput/blast_fasta_input.hpp>

#include <util/regexp.hpp>


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
    virtual void x_CloseGap(TSeqPos len) {
        (void)len;  // remove solaris compiler warning
        return;
    }

    /// Override logic for assigning the molecule type
    /// @note fForceType is ignored if the sequence length is less than the
    /// value configured in the constructor
    virtual void AssignMolType() {
        if (GetCurrentPos(eRawPos) < m_SeqLenThreshold) {
            _ASSERT( (TestFlag(fAssumeNuc) ^ TestFlag(fAssumeProt) ) );
            SetCurrentSeq().SetInst().SetMol(TestFlag(fAssumeNuc) 
                                             ? CSeq_inst::eMol_na 
                                             : CSeq_inst::eMol_aa);
        } else {
            CFastaReader::AssignMolType();
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
    virtual CRef<CSeq_entry> ReadOneSeq(void) {
        
        static const string kRegex_Gi("^[0-9]+$");
        static const string kRegex_Ti("^gnl\\|ti\\|[0-9]+$");
        static const string kRegex_Accession("^[a-z].*[0-9]+$");
        // The following are not matched by CSeq_id::IdentifyAccession
        static const string kRegex_PdbWithDash("^pdb\\|[a-z0-9-]+$");
        static const string kRegex_Prf("^prf\\|{1,2}[a-z0-9]+$");
        static const string kRegex_SpWithVersion("^sp\\|[a-z0-9]+.[0-9]+$");

        const string line = NStr::TruncateSpaces(*++GetLineReader());

        CRegexpUtil regex(line);
        const CRegexp::ECompile flags = static_cast<CRegexp::ECompile>
            (CRegexp::fCompile_ignore_case | CRegexp::fCompile_newline);

        try {
            // Test for GI only
            if (regex.Exists(kRegex_Gi)) {
                static const int conv_flags
                    (NStr::fAllowLeadingSpaces|NStr::fAllowTrailingSpaces);
                int gi = NStr::StringToLong(line, conv_flags);
                return x_PrepareBioseqWithGi(gi);
            // Test for Trace ID
            } else if (regex.Exists(kRegex_Ti, flags)) {
                int ti = NStr::StringToLong(regex.Extract("[0-9]+", flags));
                return x_PrepareBioseqWithTi(ti);
            // Test for accession
            } else if (regex.Exists(kRegex_Accession, flags) ||
                       regex.Exists(kRegex_PdbWithDash, flags) ||
                       regex.Exists(kRegex_Prf, flags) ||
                       regex.Exists(kRegex_SpWithVersion, flags) ||
                       x_MatchKnownAccessions(line)) {
                return x_PrepareBioseqWithAccession(line);
            }
        } catch (const CInputException&) { 
            throw; 
        } catch (const CSeqIdException& e) {
            if (e.GetErrCode() == CSeqIdException::eFormat) {
                NCBI_THROW(CInputException, eSeqIdNotFound,
                           "Sequence ID not found: '" + line + "'");
            }
        } catch (const exception&) {
            throw;
        } catch (...) {
            // in case of other exceptions, just defer to CFastaReader
        }

        // If all fails, fall back to parent's implementation
        GetLineReader().UngetLine();
        return CFastaReader::ReadOneSeq();
    }

private:
    /// Configuration options for the CBlastScopeSource
    const SDataLoaderConfig& m_DLConfig;
    /// Scope used to retrieve the sequence length
    CRef<CScope> m_InputScope;
    /// True if we're supposed to be reading proteins, else false
    bool m_ReadProteins;
    /// True if the sequence data must be fetched
    bool m_RetrieveSeqData;

    /// Performs sanity checks to make sure that the sequence requested is of
    /// the expected type. If the tests fail, an exception is thrown.
    /// @param id Sequence id for this sequence [in]
    void x_ValidateMoleculeType(CConstRef<CSeq_id> id) 
    {
        _ASSERT(m_InputScope.NotEmpty());
        CBioseq_Handle bh = m_InputScope->GetBioseqHandle(*id);
        if (bh.IsNucleotide() && m_ReadProteins) {
            NCBI_THROW(CInputException, eSequenceMismatch,
               "Gi/accession mismatch: requested protein, found nucleotide");
        }
        if (bh.IsProtein() && !m_ReadProteins) {
            NCBI_THROW(CInputException, eSequenceMismatch,
               "Gi/accession mismatch: requested nucleotide, found protein");
        }
    }

    /// Auxiliary function to create a Bioseq given a CSeq_id ready to be added
    /// to a BlastObject, which does NOT contain sequence data
    /// @param id Sequence id for this bioseq [in]
    CRef<CBioseq> x_CreateBioseq(CRef<CSeq_id> id)
    {
        if (m_InputScope.Empty()) {
            m_InputScope = CBlastScopeSource(m_DLConfig).NewScope();
        }

        // N.B.: this call fetches the Bioseq into the scope from its
        // data sources (should be BLAST DB first, then Genbank)
        TSeqPos len = sequence::GetLength(*id, m_InputScope);
        if (len == numeric_limits<TSeqPos>::max()) {
            NCBI_THROW(CInputException, eSeqIdNotFound,
                       "Sequence ID not found: '" + 
                       id->AsFastaString() + "'");
        }

        x_ValidateMoleculeType(id);

        CRef<CBioseq> retval;
        if (m_RetrieveSeqData) {
            CBioseq_Handle bh = m_InputScope->GetBioseqHandle(*id);
            retval.Reset(const_cast<CBioseq*>(&*bh.GetCompleteBioseq()));
        } else {
            retval.Reset(new CBioseq());
            retval->SetId().push_back(id);
            retval->SetInst().SetRepr(CSeq_inst::eRepr_raw);
            retval->SetInst().SetMol(m_ReadProteins 
                                     ? CSeq_inst::eMol_aa
                                     : CSeq_inst::eMol_dna);
            retval->SetInst().SetLength(len);
        }
        return retval;
    }

    /// Create a Bioseq with a gi and save it in a Seq-entry object
    /// @param gi Gi which identifies Bioseq [in]
    CRef<CSeq_entry> x_PrepareBioseqWithGi(int gi) {
        CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Gi, gi));
        CRef<CBioseq> bioseq(x_CreateBioseq(id));
        CRef<CSeq_entry> retval(new CSeq_entry());
        retval->SetSeq(*bioseq);
        return retval;
    }

    /// Create a Bioseq with a Trace ID and save it in a Seq-entry object
    /// @param ti Trace ID which identifies Bioseq [in]
    CRef<CSeq_entry> x_PrepareBioseqWithTi(int ti) {
        CRef<CDbtag> general_id(new CDbtag());
        general_id->SetDb("ti");
        general_id->SetTag().SetId(ti);
        CRef<CSeq_id> id(new CSeq_id());
        id->SetGeneral(*general_id);
        CRef<CBioseq> bioseq(x_CreateBioseq(id));
        CRef<CSeq_entry> retval(new CSeq_entry());
        retval->SetSeq(*bioseq);
        return retval;
    }

    /// Create a Bioseq with an accession and save it in a Seq-entry object
    /// @param line accession which identifies Bioseq [in]
    CRef<CSeq_entry> x_PrepareBioseqWithAccession(const string& line) {
        CRef<CSeq_id> id(new CSeq_id(line));
        CRef<CBioseq> bioseq(x_CreateBioseq(id));
        CRef<CSeq_entry> retval(new CSeq_entry());
        retval->SetSeq(*bioseq);
        return retval;
    }
    
    /// Check whether an input line qualifies as a known accession type.
    /// @param str The input line.
    /// @return True iff the input line is a known accession, as identified by
    /// CSeq_id::IdentifyAccession
    inline bool x_MatchKnownAccessions(const string & str) const
    {
        // try matching untagged accessions first...
        switch (CSeq_id::IdentifyAccession(str)) {
        case CSeq_id::eAcc_pdb:
        case CSeq_id::eAcc_swissprot:
        case CSeq_id::eAcc_prf:
            return true;
        default:
            break;
        }
        return false;
    }
};

CBlastFastaInputSource::CBlastFastaInputSource(CNcbiIstream& infile,
                                       const CBlastInputSourceConfig& iconfig)
    : m_Config(iconfig),
      m_LineReader(new CStreamLineReader(infile)),
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
    if (getenv("BLASTINPUT_GEN_DELTA_SEQ") == NULL) {
        flags += CFastaReader::fNoSplit;
    }
    // This is necessary to enable the ignoring of gaps in classes derived from
    // CFastaReader
    flags += CFastaReader::fParseGaps;

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

    CRef<CSeqIdGenerator> idgen
        (new CSeqIdGenerator(m_Config.GetLocalIdCounterInitValue()));
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

    if (m_Config.GetLowercaseMask())
        lcase_mask = m_InputReader->SaveMask();

    CRef<CSeq_entry> seq_entry(m_InputReader->ReadOneSeq());
    _ASSERT(seq_entry.NotEmpty());
    scope.AddTopLevelSeqEntry(*seq_entry);

    CTypeConstIterator<CBioseq> itr(ConstBegin(*seq_entry));
    CRef<CSeq_loc> retval(new CSeq_loc());

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
    retval->SetInt().SetId().Assign(*itr->GetId().front());

    return retval;
}


SSeqLoc
CBlastFastaInputSource::GetNextSSeqLoc(CScope& scope)
{
    CRef<CSeq_loc> lcase_mask;
    CRef<CSeq_loc> seqloc = x_FastaToSeqLoc(lcase_mask, scope);
    
    SSeqLoc retval(seqloc, &scope);
    if (m_Config.GetLowercaseMask())
        retval.mask = lcase_mask;

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
        const bool apply_mask_to_both_strands = true;
        masks_in_query = 
            PackedSeqLocToMaskedQueryRegions(
                                static_cast<CConstRef<CSeq_loc> >(lcase_mask),
                                program, apply_mask_to_both_strands);
    }
    return CRef<CBlastSearchQuery>
        (new CBlastSearchQuery(*seqloc, scope, masks_in_query));
}

END_SCOPE(blast)
END_NCBI_SCOPE
