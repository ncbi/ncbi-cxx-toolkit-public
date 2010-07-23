/* $Id$
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
 *
 */

/**
 * @brief Determines the scanner's offsets taking the database masking
 * restrictions into account (if any). This function should be called from the
 * WordFinder routines only.
 *
 * @param subject The subject sequence [in]
 * @param word_length the real word length [in]
 * @param lut_word_length the lookup table word length [in]
 * @param range the structure to record seq mask index, start and
 *              end of scanning pos, and start and end of current mask [in][out]
 *
 * @return TRUE if the scanning should proceed, FALSE otherwise
 */
static NCBI_INLINE Boolean
s_DetermineScanningOffsets(const BLAST_SequenceBlk* subject,
                           Int4  word_length,
                           Int4  lut_word_length,
                           Int4* range)
{
    while (range[1] > range[2]) {
        range[0]++;
        if (range[0] >= subject->num_seq_ranges) {
            return FALSE;
        }
        range[1] = subject->seq_ranges[range[0]].left + word_length - lut_word_length;
        range[2] = subject->seq_ranges[range[0]].right - lut_word_length;
    } 
    return TRUE;
}
