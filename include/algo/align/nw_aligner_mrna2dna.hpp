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
*   filling out its dynamic programming table.
*   
*/

#include "nw_aligner.hpp"


BEGIN_NCBI_SCOPE

class CNWAlignerMrna2Dna: public CNWAligner
{
public:

    CNWAlignerMrna2Dna(const char* seq1, size_t len1,
                       const char* seq2, size_t len2)
        throw(CNWAlignerException);

    // Run the algorithm, return the alignment's score
    virtual TScore Run();

    // Setters
    void SetWi  (TScore value)  { m_Wi  = value; }
    void SetIntronMinSize  (size_t s)  { m_IntronMinSize  = s; }

    // Getters
    static TScore GetDefaultWi  () { return -10; }
    static size_t GetDefaultIntronMinSize () { return 50; }

protected:

    TScore   m_Wi;            // intron weight
    size_t   m_IntronMinSize; // intron min size

    virtual void   x_DoBackTrace(const unsigned char* backtrace_matrix);
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2002/12/17 21:49:40  kapustin
 * Remove unnecesary seq type parameter from the constructor
 *
 * Revision 1.1  2002/12/12 17:54:16  kapustin
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* UTIL___NW_ALIGNER_MRNA2DNA__HPP */
