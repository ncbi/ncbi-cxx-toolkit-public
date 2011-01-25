#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */
/* ===========================================================================
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
 */

/** @file msa_pssm_input.cpp
 * Implementation of the concrete strategy to obtain PSSM input data for
 * PSI-BLAST from a multiple sequence alignment.
 */

#include <ncbi_pch.hpp>

// BLAST includes
#include <algo/blast/api/msa_pssm_input.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include "../core/blast_psi_priv.h"         // for kQueryIndex

// Objtools includes
#include <objtools/readers/aln_reader.hpp>  // for CAlnReader

// Object includes
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/seqport_util.hpp>

// Serial includes
#include <serial/iterator.hpp>              // for CTypeIterator, Begin

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

/// The representation of a gap in ASCII format
static const char kGapChar('-');

//////////////////////////////////////////////////////////////////////////////

CPsiBlastInputClustalW::CPsiBlastInputClustalW
        (CNcbiIstream& input_file,
         const PSIBlastOptions& opts,
         const char* matrix_name /* = NULL */,
         const PSIDiagnosticsRequest* diags /* = NULL */,
         const unsigned char* query /* = NULL */,
         unsigned int query_length /* = 0 */,
         int gap_existence /* = 0 */,
         int gap_extension /* = 0 */,
         unsigned int msa_master_idx /* = 0 */)
    : m_Query(0), m_GapExistence(gap_existence), m_GapExtension(gap_extension)
{
    if (query) {
        _ASSERT(query_length);
        m_MsaDimensions.query_length = query_length;
        m_Query.reset(new Uint1[query_length]);
        memcpy((void*) m_Query.get(), (void*) query, query_length);
    }

    m_Opts = opts;
    m_Opts.ignore_unaligned_positions = true;

    x_ReadAsciiMsa(input_file);
    if ( !m_Query || msa_master_idx != 0) {
        x_ExtractQueryFromMsa(msa_master_idx);
    }
    x_ValidateQueryInMsa();
    _ASSERT(m_Query);
    _ASSERT(m_MsaDimensions.query_length);
    // query is included in m_AsciiMsa, so decrement it by 1
    m_MsaDimensions.num_seqs = m_AsciiMsa.size() - 1; 

    m_Msa = NULL;

    // Default value provided by base class
    m_MatrixName = string(matrix_name ? matrix_name : "");
    if (diags) {
        m_DiagnosticsRequest = PSIDiagnosticsRequestNew();
        *m_DiagnosticsRequest = *diags;
    } else {
        m_DiagnosticsRequest = NULL;
    }
}

CPsiBlastInputClustalW::~CPsiBlastInputClustalW()
{
    PSIMsaFree(m_Msa);
    PSIDiagnosticsRequestFree(m_DiagnosticsRequest);
}

void
CPsiBlastInputClustalW::x_ReadAsciiMsa(CNcbiIstream& input_file)
{
    _ASSERT(m_AsciiMsa.empty());
    CAlnReader reader(input_file);
    reader.SetClustal(CAlnReader::eAlpha_Protein);
    reader.Read();
    m_AsciiMsa = reader.GetSeqs();
    m_SeqEntry = reader.GetSeqEntry();
    // Test our post-condition
    _ASSERT( !m_AsciiMsa.empty() );
    _ASSERT( !m_SeqEntry.Empty() );
}

/// Auxiliary function to retrieve the sequence data in NCBI-stdaa format from
/// the bioseq.
/// @param bioseq Bioseq to extract the data from [in]
/// @param query_length size of the query [in]
/// @param retval return value of this function [in|out]
/// @sa CPssm::GetQuerySequenceData
static void 
s_GetQuerySequenceData(const CBioseq& bioseq, size_t query_length, CNCBIstdaa& retval)
{
    const CSeq_data& seq_data = bioseq.GetInst().GetSeq_data();
    retval.Set().reserve(query_length);
    if ( !seq_data.IsNcbistdaa() ) {
        CSeq_data ncbistdaa;
        CSeqportUtil::Convert(seq_data, &ncbistdaa, CSeq_data::e_Ncbistdaa);
        copy(ncbistdaa.GetNcbistdaa().Get().begin(),
             ncbistdaa.GetNcbistdaa().Get().end(),
             back_inserter(retval.Set()));
    } else {
        copy(seq_data.GetNcbistdaa().Get().begin(),
             seq_data.GetNcbistdaa().Get().end(),
             back_inserter(retval.Set()));
    }
}

/// Returns true iff sequence is identical to query
static bool
s_AreSequencesEqual(const CNCBIstdaa& sequence, Uint1* query)
{
    bool retval = true;
    for (TSeqPos i = 0; i < sequence.Get().size(); i++) {
        if (sequence.Get()[i] != query[i]) {
            retval = false;
            break;
        }
    }
    return retval;
}

void
CPsiBlastInputClustalW::x_ExtractQueryForPssm()
{
    // Test our pre-conditions
    _ASSERT(m_Query.get() && m_SeqEntry.NotEmpty());
    _ASSERT(m_QueryBioseq.Empty());

    for (CTypeIterator<CBioseq> itr(Begin(*m_SeqEntry)); itr; ++itr) {
        _ASSERT(itr->IsAa());
        if (itr->GetLength() != GetQueryLength()) {
            continue;
        }
        // let's check the sequence data
        CNCBIstdaa sequence;
        s_GetQuerySequenceData(*itr, GetQueryLength(), sequence);
        if (s_AreSequencesEqual(sequence, m_Query.get())) {
            m_QueryBioseq.Reset(&*itr);
            break;
        }
    }
    // note that the title cannot be set because we're getting the query
    // sequence from the multiple sequence alignment file via CAlnReader

    // Test our post-condition
    _ASSERT(m_QueryBioseq.NotEmpty());
}

void
CPsiBlastInputClustalW::Process()
{
    // Create multiple alignment data structure and populate with query
    // sequence
    m_Msa = PSIMsaNew(&m_MsaDimensions);
    if ( !m_Msa ) {
        NCBI_THROW(CBlastSystemException, eOutOfMemory, 
                   "Multiple alignment data structure");
    }

    x_CopyQueryToMsa();
    x_ExtractAlignmentData();
    x_ExtractQueryForPssm();
}

void
CPsiBlastInputClustalW::x_ValidateQueryInMsa()
{
    const size_t kAligmentLength = m_AsciiMsa.front().size();
    const char kMaskingRes = NCBISTDAA_TO_AMINOACID[kProtMask];
    _ASSERT( !m_AsciiMsa.empty() );

    size_t seq_idx = 0;
    for (; seq_idx < m_AsciiMsa.size(); seq_idx++) {
        size_t query_idx = 0;
        for (size_t align_idx = 0;
             align_idx < kAligmentLength && query_idx < GetQueryLength();
             align_idx++) {
            if (m_AsciiMsa[seq_idx][align_idx] == kGapChar) {
                continue;
            }
            char query_res = NCBISTDAA_TO_AMINOACID[m_Query.get()[query_idx]];
            const char kCurrentRes = toupper(m_AsciiMsa[seq_idx][align_idx]);
            /* Selenocysteines are replaced by X's in query; test for this
             * possibility */
            if (query_res == kMaskingRes && kCurrentRes == 'U') {
                query_res = kCurrentRes;
            }
            if (query_res != kCurrentRes) {
                break;  // character mismatch
            } else {
                query_idx++;
            }
        }

        if (query_idx == GetQueryLength()) {
            break;
        }
    }

    if (seq_idx < m_AsciiMsa.size()) { 
        // If the query was found at position seq_idx, swap it with the first
        // element in the m_AsciiMsa vector
        for (size_t align_idx = 0; align_idx < kAligmentLength; align_idx++) {
            swap(m_AsciiMsa[seq_idx][align_idx], m_AsciiMsa.front()[align_idx]);
        }
    } else {
        string msg("No sequence in the multiple sequence alignment provided ");
        msg += "matches the query sequence";
        NCBI_THROW(CBlastException, eInvalidOptions, msg);
    }
}

void
CPsiBlastInputClustalW::x_ExtractQueryFromMsa(unsigned int msa_master_idx/*=0*/)
{
    if (msa_master_idx >= m_AsciiMsa.size()) {
        CNcbiOstrstream oss;
        oss << "Invalid master sequence index, please use a value between 1 "
            << "and " << m_AsciiMsa.size();
        NCBI_THROW(CBlastException, eInvalidOptions,
                   CNcbiOstrstreamToString(oss));
    }
    const string& kQuery = m_AsciiMsa.at(msa_master_idx);
    size_t kNumGaps = 0;
    ITERATE(string, residue, kQuery) {
        if (*residue == kGapChar) {
            kNumGaps++;
        }
    } 
    const unsigned int kQueryLength = kQuery.size() - kNumGaps;

    m_MsaDimensions.query_length = kQueryLength;
    m_Query.reset(new Uint1[kQueryLength]);
    unsigned int query_idx = 0;
    ITERATE(string, residue, kQuery) {
        _ASSERT(isalpha(*residue) || *residue == kGapChar);
        if (*residue == kGapChar) {
            continue;
        }
        m_Query.get()[query_idx] = AMINOACID_TO_NCBISTDAA[toupper(*residue)];
        query_idx++;
    }
    _ASSERT(query_idx == kQueryLength);

    // Test our post-conditions
    _ASSERT(m_Query.get() != NULL);
    _ASSERT(m_MsaDimensions.query_length);
}

void
CPsiBlastInputClustalW::x_CopyQueryToMsa()
{
    _ASSERT(m_Msa);
    const string& ascii_query = m_AsciiMsa.front();

    unsigned int query_idx = 0;
    ITERATE(string, residue, ascii_query) {
        if (*residue == kGapChar) {
            continue;
        }
        m_Msa->data[kQueryIndex][query_idx].letter = m_Query.get()[query_idx];
        m_Msa->data[kQueryIndex][query_idx].is_aligned = 
            (isupper(*residue) ? true : false);
        query_idx++;
    }
    _ASSERT(query_idx == GetQueryLength());
}

void
CPsiBlastInputClustalW::x_ExtractAlignmentData()
{
    const size_t kAlignmentLength = m_AsciiMsa.front().size();
    _ASSERT( !m_AsciiMsa.empty() );

    size_t seq_index = kQueryIndex + 1;
    for (; seq_index < m_AsciiMsa.size(); seq_index++) {
        size_t query_idx = 0;
        for (size_t align_idx = 0; align_idx < kAlignmentLength; align_idx++) {
            if (m_AsciiMsa.front()[align_idx] == kGapChar) {
                continue;
            }
            _ASSERT(toupper(m_AsciiMsa.front()[align_idx]) ==
                    NCBISTDAA_TO_AMINOACID[m_Query.get()[query_idx]]);
            const char kCurrentRes = m_AsciiMsa[seq_index][align_idx];
            _ASSERT(isalpha(kCurrentRes) || kCurrentRes == kGapChar);
            m_Msa->data[seq_index][query_idx].letter = 
                AMINOACID_TO_NCBISTDAA[(int) toupper(kCurrentRes)];
            if (isupper(kCurrentRes) && kCurrentRes != kGapChar) {
                m_Msa->data[seq_index][query_idx].is_aligned = true;
            } else {
                m_Msa->data[seq_index][query_idx].is_aligned = false;
            }
            query_idx++;
        }
    }
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
