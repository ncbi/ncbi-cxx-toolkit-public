#ifndef OBJECTS_SEQ___GENCODE_HPP
#define OBJECTS_SEQ___GENCODE_HPP

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
 * Revision 1.3  2001/11/13 12:12:52  clausen
 * Added Codon2Idx and Idx2Codon. Modified type of code_breaks
 *
 * Revision 1.2  2001/09/07 14:16:49  ucko
 * Cleaned up external interface.
 *
 * Revision 1.1  2001/08/24 00:32:44  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#include <objects/seqloc/Na_strand.hpp>
#include <vector>
#include <utility>


BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE


class CSeq_data;
class CGenetic_code;


// Public front-end for hidden singleton object of class
// CGencode_implementation.
class CGencode
{
public:
    typedef vector<pair<unsigned int, char> > TCodeBreaks;
    
    // Function to translate na to aa. in_seq must
    // be in Iupacna and out_seq will be in Ncbieaa
    // The key to the code_breaks map is the index
    // in out_seq (sequence of amino acids) where
    // an exception to the genetic code occurs. The
    // char values will replace the genetic coded
    // implied aa at that index.
    
    static void Translate
    (const CSeq_data&      in_seq,
     CSeq_data*            out_seq,
     const CGenetic_code&  genetic_code,
     const TCodeBreaks &   code_breaks,
     unsigned int          uBeginIdx          = 0,
     unsigned int          uLength            = 0,
     bool                  bCheck_first       = true,
     bool                  bPartial_start     = false,
     ENa_strand            eStrand            = eNa_strand_plus,
     bool                  bStop              = true,
     bool                  bRemove_trailing_x = false);
     
     // Converts a codon triplet to an index = 
     // 16*codon[0] + 4*codon[1] + codon[2] where 'T'=0, 'C'=1, 'A'=2, 'G'=3     
     static unsigned int Codon2Idx(const string& codon);
     
     // Converts an index to a codon triplet
     static const string& Idx2Codon(unsigned int idx);
};


END_objects_SCOPE
END_NCBI_SCOPE

#endif  /* OBJECTS_SEQ___GENCODE_HPP */
