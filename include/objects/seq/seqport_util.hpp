#ifndef OBJECTS_SEQ___SEQPORT_UTIL__HPP
#define OBJECTS_SEQ___SEQPORT_UTIL__HPP

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
 * Author:  Clifford Clausen
 *          (also reviewed/fixed/groomed by Denis Vakatov)
 *
 * File Description:
 *   
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.3  2002/01/10 19:20:45  clausen
 * Added GetIupacaa3, GetCode, and GetIndex
 *
 * Revision 1.2  2001/09/07 14:16:49  ucko
 * Cleaned up external interface.
 *
 * Revision 1.1  2001/08/24 00:34:22  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#include <objects/seq/Seq_data.hpp>
#include <util/random_gen.hpp>
#include <vector>


BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE


// CSeqportUtil is a wrapper for a hidden object of class
// CSeqportUtil_implementation.

class CSeqportUtil
{
public:
    // Alphabet conversion function. Function returns the
    // number of converted codes.
    static unsigned int Convert(const CSeq_data&       in_seq,
                                CSeq_data*             out_seq,
                                CSeq_data::E_Choice    to_code,
                                unsigned int           uBeginIdx = 0,
                                unsigned int           uLength   = 0,
                                bool                   bAmbig    = false,
                                CRandom::TValue        seed      = 17734276);

    // Function to provide maximum in-place packing of na
    // sequences without loss of information. Iupacna
    // can always be packed to ncbi4na without loss. Iupacna
    // can sometimes be packed to ncbi2na. Ncbi4na can
    // sometimes be packed to ncbi2na. Returns number of
    // residues packed. If in_seq cannot be packed, the
    // original in_seq is returned unchanged and the return value
    // from Pack is 0
    static unsigned int Pack(CSeq_data*     in_seq,
                             unsigned int   uBeginidx = 0,
                             unsigned int   uLength   = 0);

    // Performs fast validation of CSeq_data. If all data in the
    // sequence represent valid elements of a biological sequence, then
    // FastValidate returns true. Otherwise it returns false
    static bool FastValidate(const CSeq_data&   in_seq,
                             unsigned int       uBeginIdx = 0,
                             unsigned int       uLength   = 0);

    // Performs validation of CSeq_data. Returns a list of indices
    // corresponding to data that does not represent a valid element
    // of a biological sequence.
    static void Validate(const CSeq_data&       in_seq,
                         vector<unsigned int>*  badIdx,
                         unsigned int           uBeginIdx = 0,
                         unsigned int           uLength   = 0);

    // Get ambiguous bases. out_indices returns
    // the indices relative to in_seq of ambiguous bases.
    // out_seq returns the ambiguous bases. Note, there are
    // only ambiguous bases for iupacna->ncib2na and
    // ncib4na->ncbi2na coversions.
    static unsigned int GetAmbigs
    (const CSeq_data&       in_seq,
     CSeq_data*             out_seq,
     vector<unsigned int>*  out_indices,
     CSeq_data::E_Choice    to_code = CSeq_data::e_Ncbi2na,
     unsigned int           uBeginIdx = 0,
     unsigned int           uLength   = 0);

    // Get a copy of CSeq_data. No conversion is done. uBeginIdx of the
    // biological sequence in in_seq will be in position
    // 0 of out_seq. Usually, uLength bases will be copied
    // from in_seq to out_seq. If uLength goes beyond the end of
    // in_seq, it will be shortened to go to the end of in_seq.
    // For packed sequence formats (ncbi2na and ncbi4na),
    // only uLength bases are valid copies. For example,
    // in an ncbi4na encoded sequence, if uLength is odd, the last
    // sequence returned will be uLength+1 because 2 bases are encoded
    // per byte in ncbi4na. However, in this case, uLength will be returned
    // unchanged (it will remain odd unless it goes beyond the end
    // of in_seq). If uLength=0, then a copy from uBeginIdx to the end
    // of in_seq is returned.
    static unsigned int GetCopy(const CSeq_data&       in_seq,
                                CSeq_data*             out_seq,
                                unsigned int           uBeginIdx = 0,
                                unsigned int           uLength   = 0);

    // Method to keep only a contiguous piece of a sequence beginning
    // at uBeginIdx and uLength residues long. Does bit shifting as
    // needed to put uBeginIdx of original sequence at position zero on output.
    // Similar to GetCopy(), but done in place.  Returns length of
    // kept sequence.
    static unsigned int Keep(CSeq_data*      in_seq,
                             unsigned int     uBeginIdx = 0,
                             unsigned int     uLength   = 0);

    // Append in_seq2 to to end of in_seq1. Both in seqs must be
    // in the same alphabet or this method will throw a runtime_error.
    // The result of the append will be put into out_seq.
    // For packed sequences ncbi2na and ncbi4na, Append will shift and
    // append so as to remove any jaggedness at the append point.
    static unsigned int Append(CSeq_data*             out_seq,
                               const CSeq_data&       in_seq1,
                               unsigned int           uBeginIdx1,
                               unsigned int           uLength1,
                               const CSeq_data&       in_seq2,
                               unsigned int           uBeginIdx2,
                               unsigned int           uLength2);

    // Create a biological complement of an na sequence.
    // Attempts to complement an aa sequence will throw
    // a runtime_error. Returns length of complemented sequence.

    // Complement the input sequence in place
    static unsigned int Complement(CSeq_data*       in_seq,
                                   unsigned int     uBeginIdx = 0,
                                   unsigned int     uLength   = 0);

    // Complement the input sequence and put the result in
    // the output sequence
    static unsigned int Complement(const CSeq_data&       in_seq,
                                   CSeq_data*             out_seq,
                                   unsigned int           uBeginIdx = 0,
                                   unsigned int           uLength   = 0);

    // Create a biological sequence that is the reversse
    // of an input sequence. Attempts to reverse an aa
    // sequence will throw a runtime_error. Returns length of
    // reversed sequence.

    // Reverse a sequence in place
    static unsigned int Reverse(CSeq_data*       in_seq,
                                unsigned int     uBeginIdx = 0,
                                unsigned int     uLength   = 0);

    // Reverse an input sequence and put result in output sequence.
    // Reverses packed bytes as necessary.
    static unsigned int Reverse(const CSeq_data&  in_seq,
                                CSeq_data*        out_seq,
                                unsigned int      uBeginIdx = 0,
                                unsigned int      uLength   = 0);


    // Create the reverse complement of an input sequence. Attempts
    // to reverse-complement an aa sequence will throw a
    // runtime_error.

    // Reverse complement a sequence in place
    static unsigned int ReverseComplement(CSeq_data*       in_seq,
                                          unsigned int     uBeginIdx = 0,
                                          unsigned int     uLength   = 0);

    // Reverse complmenet a sequence and put result in output sequence
    static unsigned int ReverseComplement(const CSeq_data&   in_seq,
                                          CSeq_data*         out_seq,
                                          unsigned int       uBeginIdx = 0,
                                          unsigned int       uLength   = 0);
                                          
    // Given an Ncbistdaa input code index, returns the 3 letter Iupacaa3 code
    static const string& GetIupacaa3(unsigned int ncbistdaa);
    
    // Given a code index for any of Iupacna, Iupacaa, Ncbistdaa, and Ncbieaa,
    // returns the code corresponding to the index                        
    static const string& GetCode(CSeq_data::E_Choice code_type, 
                                 unsigned int        idx); 
                        
    // Given a code for any of Iupacna, Iupacaa, Ncbistdaa, and Ncbieaa,
    // returns the index corresponding to the code. If no code
    // exists, returns -1
    static int GetIndex(CSeq_data::E_Choice code_type, const string& code);
};


END_objects_SCOPE
END_NCBI_SCOPE

#endif  /* OBJECTS_SEQ___SEQPORT_UTIL__HPP */
