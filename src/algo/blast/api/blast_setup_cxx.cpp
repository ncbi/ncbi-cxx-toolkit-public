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
*   Auxiliary setup functions for Blast objects interface
*
* ===========================================================================
*/

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/gbloader.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/seq/seq__.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>

#include <BlastSetup.hpp>
#include <BlastException.hpp>

// NewBlast includes
#include <blast_util.h>
#include <blastkar.h>

USING_SCOPE(objects);

BEGIN_NCBI_SCOPE

#define LAST2BITS 0x03

// Compresses sequence data on vector to buffer, which should have been
// allocated and have the right size.
static void PackDNA(const CSeqVector& vector, Uint1* buffer, const int buflen)
{
    TSeqPos i;                  // loop index of original sequence
    TSeqPos ci;                 // loop index for compressed sequence
    TSeqPos remainder;          // number of bases packed in ncbi2na format in
                                // last byte of buffer

    _ASSERT(vector.GetCoding() == CSeq_data::e_Ncbi2na);

    for (ci = 0, i = 0; ci < (TSeqPos) buflen-1; ci++, i += COMPRESSION_RATIO) {
        buffer[ci] = ((vector[i+0] & LAST2BITS)<<6) |
                     ((vector[i+1] & LAST2BITS)<<4) |
                     ((vector[i+2] & LAST2BITS)<<2) |
                     ((vector[i+3] & LAST2BITS)<<0);
    }
    if ( (remainder = vector.size()%COMPRESSION_RATIO) == 0) {
        buffer[ci] = ((vector[i+0] & LAST2BITS)<<6) |
                     ((vector[i+1] & LAST2BITS)<<4) |
                     ((vector[i+2] & LAST2BITS)<<2) |
                     ((vector[i+3] & LAST2BITS)<<0);
    } else {
        Uint1 bit_shift = 0;

        buffer[ci] = 0;
        for (; i < vector.size(); i++) {
            switch (i%COMPRESSION_RATIO) {
            case 0: bit_shift = 6; break;
            case 1: bit_shift = 4; break;
            case 2: bit_shift = 2; break;
            default: abort();   // should never happen
            }
            buffer[ci] |= ((vector[i] & LAST2BITS)<<bit_shift);
        }
        buffer[ci] |= remainder;
    }
}

Uint1*
BLASTGetSequence(const CSeq_loc& sl, Uint1 encoding, int& len, CScope* scope,
        ENa_strand strand, bool add_nucl_sentinel)
{
    Uint1 *buf, *buf_var;       // buffers to write sequence
    TSeqPos buflen;             // length of buffer allocated
    TSeqPos i;                  // loop index of original sequence
    Uint1 sentinel;             // sentinel byte

    CBioseq_Handle handle = scope->GetBioseqHandle(sl);
    if (!handle) {
        ERR_POST(Error << "Could not retrieve bioseq_handle");
        return NULL;
    }

    // Retrieves the correct strand (plus or minus), but not both
    CSeqVector sv = handle.GetSeqVector(CBioseq_Handle::eCoding_Ncbi, strand);

    switch (encoding) {
    // Protein sequences (query & subject) always have sentinels around sequence
    case BLASTP_ENCODING:
        sentinel = NULLB;
        sv.SetCoding(CSeq_data::e_Ncbistdaa);
        buflen = sv.size()+2;
        buf = buf_var = (Uint1*) malloc(sizeof(Uint1)*buflen);
        *buf_var++ = sentinel;
        for (i = 0; i < sv.size(); i++)
            *buf_var++ = sv[i];
        *buf_var++ = sentinel;
        break;

    case NCBI4NA_ENCODING:
    case BLASTNA_ENCODING: // Used for nucleotide blastn queries
        sv.SetCoding(CSeq_data::e_Ncbi4na);
        sentinel = 0xF;
        buflen = add_nucl_sentinel ? sv.size() + 2 : sv.size();
        if (strand == eNa_strand_both)
            buflen = add_nucl_sentinel ? buflen * 2 - 1 : buflen * 2;

        buf = buf_var = (Uint1*) malloc(sizeof(Uint1)*buflen);
        if (add_nucl_sentinel)
            *buf_var++ = sentinel;
        for (i = 0; i < sv.size(); i++) {
            if (encoding == BLASTNA_ENCODING)
                *buf_var++ = ncbi4na_to_blastna[sv[i]];
            else
                *buf_var++ = sv[i];
        }
        if (add_nucl_sentinel)
            *buf_var++ = sentinel;

        if (strand == eNa_strand_both) {
            // Get the minus strand if both strands are required
            sv = handle.GetSeqVector(CBioseq_Handle::eCoding_Ncbi,
                    eNa_strand_minus);
            sv.SetCoding(CSeq_data::e_Ncbi4na);
            for (i = 0; i < sv.size(); i++) {
                if (encoding == BLASTNA_ENCODING)
                    *buf_var++ = ncbi4na_to_blastna[sv[i]];
                else
                    *buf_var++ = sv[i];
            }
            if (add_nucl_sentinel)
                *buf_var++ = sentinel;
        }
        break;

    // Used only in Blast2Sequences for the subject sequence. No sentinels are
    // required. As in readdb, remainder (sv.size()%4 != 0) goes in the last 
    // 2 bits of the last byte.
    case NCBI2NA_ENCODING:
        _ASSERT(add_nucl_sentinel == false);
        sv.SetCoding(CSeq_data::e_Ncbi2na);
        buflen = (sv.size()+COMPRESSION_RATIO-1)/COMPRESSION_RATIO;
        if (strand == eNa_strand_both)
            buflen *= 2;

        buf = buf_var = (Uint1*) malloc(sizeof(Uint1)*buflen);
        PackDNA(sv, buf_var, (strand == eNa_strand_both) ? buflen/2 : buflen);

        if (strand == eNa_strand_both) {
            // Get the minus strand if both strands are required
            sv = handle.GetSeqVector(CBioseq_Handle::eCoding_Ncbi,
                    eNa_strand_minus);
            sv.SetCoding(CSeq_data::e_Ncbi2na);
            buf_var += buflen/2;
            PackDNA(sv, buf_var, buflen/2);
        }
        break;

    default:
        ERR_POST(Error << "Invalid encoding " << encoding);
        return NULL;
    }

    len = buflen;

    return buf;
}

#if 0
// Not used right now, need to complete implementation
void
BLASTGetTranslation(const Uint1* seq, const Uint1* seq_rev,
        const int nucl_length, const short frame, Uint1* translation)
{
    TSeqPos ni = 0;     // index into nucleotide sequence
    TSeqPos pi = 0;     // index into protein sequence

    const Uint1* nucl_seq = frame >= 0 ? seq : seq_rev;
    translation[0] = NULLB;
    for (ni = ABS(frame)-1; ni < (TSeqPos) nucl_length-2; ni += CODON_LENGTH) {
        Uint1 residue = CGen_code_table::CodonToIndex(nucl_seq[ni+0], 
                                                      nucl_seq[ni+1],
                                                      nucl_seq[ni+2]);
        if (IS_residue(residue))
            translation[pi++] = residue;
    }
    translation[pi++] = NULLB;

    return;
}
#endif

unsigned char*
BLASTFindGeneticCode(int genetic_code)
{
    unsigned char* retval = NULL;
    CSeq_data gc_ncbieaa(CGen_code_table::GetNcbieaa(genetic_code),
            CSeq_data::e_Ncbieaa);
    CSeq_data gc_ncbistdaa;

    TSeqPos nconv = CSeqportUtil::Convert(gc_ncbieaa, &gc_ncbistdaa,
            CSeq_data::e_Ncbistdaa);

    _ASSERT(gc_ncbistdaa.IsNcbistdaa());
    _ASSERT(nconv == gc_ncbistdaa.GetNcbistdaa().Get().size());

    if ( !(retval = new(nothrow) unsigned char[nconv]));
        return NULL;

    for (unsigned int i = 0; i < nconv; i++)
        retval[i] = gc_ncbistdaa.GetNcbistdaa().Get()[i];

    return retval;
}

END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.5  2003/07/30 15:00:01  camacho
* Do not use Malloc/MemNew/MemFree
*
* Revision 1.4  2003/07/25 13:55:58  camacho
* Removed unnecessary #includes
*
* Revision 1.3  2003/07/24 18:22:50  camacho
* #include blastkar.h
*
* Revision 1.2  2003/07/23 21:29:06  camacho
* Update BLASTFindGeneticCode to get genetic code string with C++ toolkit
*
* Revision 1.1  2003/07/10 18:34:19  camacho
* Initial revision
*
*
* ===========================================================================
*/
