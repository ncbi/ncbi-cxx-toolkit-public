#ifndef UTIL___NW_ALIGNER_MRNA2DNA__HPP
#define UTIL___NW_ALIGNER_MRNA2DNA__HPP

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
* Author:  Yuri Kapustin
*
* File Description:
*   CNWAlignerMrna2Dna class definition
*
*   This class implements a special case of the global alignment where 
*   one of the sequences is mRna and the other is Dna.
*   The algorithm accounts for introns and splice sites on the Dna
*   while filling out its dynamic programming table.
*   
*/

#include "nw_aligner.hpp"


/** @addtogroup AlgoGlobalmRna
 *
 * @{
 */


// There are three true splice types. The fourth
// is a long gap which is internally treated as
// a separate splice type.
const unsigned char splice_type_count = 4;

BEGIN_NCBI_SCOPE

class NCBI_XALGOALIGN_EXPORT CNWAlignerMrna2Dna: public CNWAligner
{
public:

    CNWAlignerMrna2Dna(const char* seq1, size_t len1,
                       const char* seq2, size_t len2)
        throw(CAlgoAlignException);

    // Setters
    void SetWi  (unsigned char splice_type, TScore value);

    void SetIntronMinSize  (size_t s)  {
        m_IntronMinSize  = s;
    }

    size_t MakeGuides(const size_t guide_size = 30);

    // Getters
    static TScore GetDefaultWi  (unsigned char splice_type)
        throw(CAlgoAlignException);
    static size_t GetDefaultIntronMinSize () {
        return 50;
    }

    // Formatters
    virtual void FormatAsText(string* output, EFormat type,
                              size_t line_width = 100) const
                              throw(CAlgoAlignException);

protected:

    TScore   m_Wi [splice_type_count];  // intron weights
    size_t   m_IntronMinSize;           // intron min size

    virtual TScore x_Align (const char* seg1, size_t len1,
                            const char* seg2, size_t len2,
                            vector<ETranscriptSymbol>* transcript);

    virtual TScore x_ScoreByTranscript() const
        throw(CAlgoAlignException);

    // guiding hits
    unsigned char x_CalcFingerPrint64( const char* beg, const char* end,
                                       size_t& err_index);
    const char*   x_FindFingerPrint64( const char* beg, const char* end,
                                       unsigned char fingerprint,
                                       size_t size, size_t& err_index);
private:

    void x_DoBackTrace(const Uint2* backtrace_matrix,
                       size_t N1, size_t N2,
                       vector<ETranscriptSymbol>* transcript);
};


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.12  2003/08/04 15:43:19  dicuccio
 * Modified export specifiers to be more flexible
 *
 * Revision 1.11  2003/06/17 17:20:28  kapustin
 * CNWAlignerException -> CAlgoAlignException
 *
 * Revision 1.10  2003/05/23 18:23:22  kapustin
 * Introduce a generic splice type. Make transcript symbol to be more specific about type of the intron. Backtrace procedure now takes double-byte matrix.
 *
 * Revision 1.9  2003/04/14 18:59:31  kapustin
 * Add guide creation facility.  x_Run() -> x_Align()
 *
 * Revision 1.8  2003/04/10 19:14:04  kapustin
 * Introduce guiding hits approach
 *
 * Revision 1.7  2003/04/10 19:04:31  siyan
 * Added doxygen support
 *
 * Revision 1.6  2003/04/02 20:52:24  kapustin
 * Make FormatAsText virtual. Pass output string as a parameter.
 *
 * Revision 1.5  2003/03/31 15:31:47  kapustin
 * Calculate score independently from transcript
 *
 * Revision 1.4  2003/03/25 22:06:25  kapustin
 * Support non-canonical splice signals
 *
 * Revision 1.3  2003/02/21 16:41:11  dicuccio
 * Added Win32 export specifier
 *
 * Revision 1.2  2002/12/17 21:49:40  kapustin
 * Remove unnecesary seq type parameter from the constructor
 *
 * Revision 1.1  2002/12/12 17:54:16  kapustin
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* UTIL___NW_ALIGNER_MRNA2DNA__HPP */
