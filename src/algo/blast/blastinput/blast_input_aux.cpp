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
 * Author: Christiam Camacho
 *
 */

/** @file blast_input_aux.cpp
 *  Auxiliary functions for BLAST input library
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <algo/blast/blastinput/blast_input_aux.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <serial/iterator.hpp>  // for CTypeConstIterator
/* for CBlastFastaInputSource */
#include <algo/blast/blastinput/blast_fasta_input.hpp>  
#include <algo/blast/blastinput/psiblast_args.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objmgr/util/seq_loc_util.hpp>     // for sequence::GetLength

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

CNcbiOstream*
CAutoOutputFileReset::GetStream()
{
    CFile file_deleter(m_FileName);
    if (file_deleter.Exists()) {
        file_deleter.Remove();
    }
    m_FileStream.reset(new ofstream(m_FileName.c_str()));
    return m_FileStream.get();
}

int
GetQueryBatchSize(EProgram program, bool is_ungapped /* = false */)
{
    int retval = 0;

    /*  It seems no harm to turn on query concatenation for ungapped search
        JIRA SB-764
    if (is_ungapped) {
        retval = 1000000;
        _TRACE("Using query batch size " << retval);
        return retval;
    }
    */

    // used for experimentation purposes
    char* batch_sz_str = getenv("BATCH_SIZE");
    if (batch_sz_str) {
        retval = NStr::StringToInt(batch_sz_str);
        _TRACE("DEBUG: Using query batch size " << retval);
        return retval;
    }

    switch (program) {
    case eBlastn:
        retval = 100000;
        break;
    case eDiscMegablast:
	retval = 1000000;
	break;
    case eMegablast:
        retval = 5000000;
        break;
    case eTblastn:
        retval = 20000;
        break;
    // if the query will be translated, round the chunk size up to the next
    // multiple of 3, that way, when the nucleotide sequence(s) get(s)
    // split, context N%6 in one chunk will have the same frame as context N%6
    // in the next chunk
    case eBlastx:
    case eTblastx:
        // N.B.: the splitting is done on the nucleotide query sequences, then
        // each of these chunks is translated
        retval = 10002;
        break;
    case eBlastp:
    default:
        retval = 10000;
        break;
    }

    _TRACE("Using query batch size " << retval);
    return retval;
}

TSeqRange
ParseSequenceRange(const string& range_str,
                   const char* error_prefix /* = NULL */)
{
    static const char* kDfltErrorPrefix = "Failed to parse sequence range";
    static const string kDelimiters("-");
    string error_msg(error_prefix ? error_prefix : kDfltErrorPrefix);

    vector<string> tokens;
    NStr::Tokenize(range_str, kDelimiters, tokens);
    if (tokens.size() != 2 || tokens.front().empty() || tokens.back().empty()) {
        error_msg += " (Format: start-stop)";
        NCBI_THROW(CBlastException, eInvalidArgument, error_msg);
    }
    int from = NStr::StringToInt(tokens.front());
    int to = NStr::StringToInt(tokens.back());
    if (from <= 0 || to <= 0) {
        error_msg += " (range elements cannot be less than or equal to 0)";
        NCBI_THROW(CBlastException, eInvalidArgument, error_msg);
    }
    if (from == to) {
        error_msg += " (range cannot be empty)";
        NCBI_THROW(CBlastException, eInvalidArgument, error_msg);
    }
    if (from > to) {
        error_msg += " (start cannot be larger than stop)";
        NCBI_THROW(CBlastException, eInvalidArgument, error_msg);
    }
    from--, to--;   // decrement to make range 0-based

    TSeqRange retval;
    retval.SetFrom(from);
    retval.SetTo(to);
    return retval;
}

TSeqRange
ParseSequenceRangeOpenEnd(const string& range_str,
                   	   	  const char* error_prefix /* = NULL */)
{
    static const char* kDfltErrorPrefix = "Failed to parse sequence range";
    static const string kDelimiters("-");
    string error_msg(error_prefix ? error_prefix : kDfltErrorPrefix);

    vector<string> tokens;
    NStr::Tokenize(range_str, kDelimiters, tokens);
    if (tokens.front().empty()) {
        error_msg += " (start cannot be empty)";
        NCBI_THROW(CBlastException, eInvalidArgument, error_msg);
    }

    TSeqRange retval;
    int from = NStr::StringToInt(tokens.front());
    int to = 0;

    if(!tokens.back().empty()) {
    	to = NStr::StringToInt(tokens.back());

    	if (from <= 0 || to <= 0) {
    		error_msg += " (range elements cannot be less than or equal to 0)";
    		NCBI_THROW(CBlastException, eInvalidArgument, error_msg);
    	}
    	if (from > to) {
    	    error_msg += " (start cannot be larger than stop)";
    	    NCBI_THROW(CBlastException, eInvalidArgument, error_msg);
    	}
    	to--;
    	retval.SetTo(to);
    }
    //Note: TSeqRange is defaulted to max value.

    from--;
    retval.SetFrom(from);
    return retval;
}

CRef<CScope>
ReadSequencesToBlast(CNcbiIstream& in, 
                     bool read_proteins, 
                     const TSeqRange& range, 
                     bool parse_deflines,
                     bool use_lcase_masking,
                     CRef<CBlastQueryVector>& sequences)
{
    SDataLoaderConfig dlconfig(read_proteins);
    dlconfig.OptimizeForWholeLargeSequenceRetrieval();

    CBlastInputSourceConfig iconfig(dlconfig);
    iconfig.SetRange(range);
    iconfig.SetBelieveDeflines(parse_deflines);
    iconfig.SetLowercaseMask(use_lcase_masking);
    iconfig.SetSubjectLocalIdMode();

    CRef<CBlastFastaInputSource> fasta(new CBlastFastaInputSource(in, iconfig));
    CRef<CBlastInput> input(new CBlastInput(fasta));
    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
    sequences = input->GetAllSeqs(*scope);
    return scope;
}

string
CalculateFormattingParams(TSeqPos max_target_seqs, 
                          TSeqPos* num_descriptions, 
                          TSeqPos* num_alignments, 
                          TSeqPos* num_overview /* = NULL */)
{
    string warnings;
    const TSeqPos kResetSeqNumMax = 1000; 
    const TSeqPos kResetSeqNum250 = 250;  
    _ASSERT(max_target_seqs > 0);
    if (num_descriptions) {
        *num_descriptions = max_target_seqs;
        warnings += "Number of descriptions overridden to ";
        warnings += NStr::IntToString(*num_descriptions);
    }
    if (num_overview) {
        *num_overview = min(max_target_seqs, kDfltArgMaxTargetSequences);
        warnings += (warnings.empty() ? "Number " : ", number ");
        warnings += "of overview alignments overridden to ";
        warnings += NStr::IntToString(*num_overview);
    }
    if (num_alignments) {
        bool overridden = false;
        TSeqPos halfHits = max_target_seqs/2;    
        if(max_target_seqs <= kDfltArgMaxTargetSequences) { 
            *num_alignments = max_target_seqs;
            overridden = true;
        }
        else if(halfHits < kResetSeqNum250) { 
            *num_alignments = kDfltArgMaxTargetSequences;
            overridden = true;
        }    
        else if(halfHits <= kResetSeqNumMax) { 
            *num_alignments = halfHits;
            overridden = true;
        }
        else {
            *num_alignments = kResetSeqNumMax;
            overridden = true;
        }
        if (overridden) {
            warnings += (warnings.empty() ? "Number " : ", number ");
            warnings += "of alignments overridden to ";
            warnings += NStr::IntToString(*num_alignments);
        }
    }
    if ( !warnings.empty() ) {
        warnings += ".";
    }
    return warnings;
}

bool
HasRawSequenceData(const objects::CBioseq& bioseq)
{
     if (CBlastBioseqMaker::IsEmptyBioseq(bioseq))
         return false;
     // CFastaReader returns empty Bioseqs with the following traits, assume it
     // has sequence data so it can be processed by the BLAST engine.
     else if (bioseq.GetInst().GetRepr() == CSeq_inst::eRepr_virtual && bioseq.GetInst().CanGetLength() &&
              bioseq.GetLength() == 0)
         return true;
     else if (bioseq.GetInst().CanGetSeq_data() == true)
         return true;
     else if (bioseq.GetInst().IsSetExt())
     {
         if (bioseq.GetInst().GetRepr() == CSeq_inst::eRepr_delta)
         {
              bool is_raw = true;
              ITERATE (CSeq_inst::TExt::TDelta::Tdata, iter,
                      bioseq.GetInst().GetExt().GetDelta().Get()) {
                 if ((*iter)->Which() == CDelta_seq::e_Loc) {
                     is_raw = false;
                     break;
                 }
              }
              return is_raw;
         }
     }
     return false;
}

void
CheckForEmptySequences(CRef<CBlastQueryVector> sequences, string& warnings)
{
    warnings.clear();

    if (sequences.Empty() || sequences->Empty()) {
        NCBI_THROW(CInputException, eEmptyUserInput, "No sequences provided");
    }

    vector<string> empty_sequence_ids;
    bool all_empty = true;

    ITERATE(CBlastQueryVector, query, *sequences) {
        if ((*query)->GetLength() == 0) {
            empty_sequence_ids.
                push_back((*query)->GetQuerySeqLoc()->GetId()->AsFastaString());
        } else {
            all_empty = false;
        }
    }

    if (all_empty) {
        NCBI_THROW(CInputException, eEmptyUserInput, 
                   "Query contains no sequence data");
    }

    if (!empty_sequence_ids.empty())
    {
        warnings.assign("The following sequences had no sequence data:");
        warnings += empty_sequence_ids.front();
        for (TSeqPos i = 1; i < empty_sequence_ids.size(); i++) {
            warnings += ", " + empty_sequence_ids[i];
        }
    }
}

void
CheckForEmptySequences(const TSeqLocVector& sequences, string& warnings)
{
    warnings.clear();

    if (sequences.empty()) {
        NCBI_THROW(CInputException, eEmptyUserInput, "No sequences provided");
    }

    vector<string> empty_sequence_ids;
    bool all_empty = true;

    ITERATE(TSeqLocVector, query, sequences) {
        if (sequence::GetLength(*query->seqloc, query->scope) == 0) {
            empty_sequence_ids.
                push_back(query->seqloc->GetId()->AsFastaString());
        } else {
            all_empty = false;
        }
    }

    if (all_empty) {
        NCBI_THROW(CInputException, eEmptyUserInput, 
                   "Query contains no sequence data");
    }

    if (!empty_sequence_ids.empty())
    {
        warnings.assign("The following sequences had no sequence data:");
        warnings += empty_sequence_ids.front();
        for (TSeqPos i = 1; i < empty_sequence_ids.size(); i++) {
            warnings += ", " + empty_sequence_ids[i];
        }
    }
}

void
CheckForEmptySequences(CRef<CBioseq_set> sequences, string& warnings)
{
    warnings.clear();

    if (sequences.Empty()) {
        NCBI_THROW(CInputException, eEmptyUserInput, "No sequences provided");
    }

    vector<string> empty_sequence_ids;
    bool all_empty = true;

    CTypeConstIterator<CBioseq> itr(ConstBegin(*sequences, eDetectLoops));
    for (; itr; ++itr) {
        if (!itr->IsSetLength() || itr->GetLength() == 0) {
            empty_sequence_ids.
                push_back(itr->GetFirstId()->AsFastaString());
        } else {
            all_empty = false;
        }
    }

    if (all_empty) {
        NCBI_THROW(CInputException, eEmptyUserInput, 
                   "Query contains no sequence data");
    }

    if (!empty_sequence_ids.empty())
    {
        warnings.assign("The following sequences had no sequence data:");
        warnings += empty_sequence_ids.front();
        for (TSeqPos i = 1; i < empty_sequence_ids.size(); i++) {
            warnings += ", " + empty_sequence_ids[i];
        }
    }
}

END_SCOPE(blast)
END_NCBI_SCOPE
