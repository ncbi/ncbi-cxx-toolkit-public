#ifndef ALGO___NW_FORMAT__HPP
#define ALGO___NW_FORMAT__HPP

/* $Id$
* ===========================================================================
*
*                            public DOMAIN NOTICE                          
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
*   Library's formatting functionality.
*/

/** @addtogroup AlgoAlignFormat
 *
 * @{
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE

class CNWAligner;

BEGIN_SCOPE(objects)
    class CSeq_align;
END_SCOPE(objects)


class NCBI_XALGOALIGN_EXPORT CNWFormatter: public CObject
{
public:

    CNWFormatter(const CNWAligner& aligner);

    // setters
    void SetSeqIds(const string& id1, const string& id2) {
        m_Seq1Id = id1;
        m_Seq2Id = id2;
    }

    // supported text formats
    enum ETextFormatType {
        eFormatType1,
        eFormatType2,
        eFormatAsn,
        eFormatFastA,
        eFormatExonTable,  // spliced alignments
        eFormatExonTableEx //
    };

    void AsText(string* output, ETextFormatType type,
                size_t line_width = 100) const;

    void AsSeqAlign(objects::CSeq_align* output) const;

private:
    const CNWAligner* m_aligner;
    string            m_Seq1Id, m_Seq2Id;

    size_t x_ApplyTranscript(vector<char>* seq1_transformed,
                             vector<char>* seq2_transformed) const;
    
};


END_NCBI_SCOPE

/* @} */

/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2004/05/17 14:50:46  kapustin
 * Add/remove/rearrange some includes and object declarations
 *
 * Revision 1.4  2003/10/27 20:46:41  kapustin
 * Derive from CObject.
 *
 * Revision 1.3  2003/09/26 14:43:01  kapustin
 * Remove exception specifications
 *
 * Revision 1.2  2003/09/10 20:12:47  kapustin
 * Update Doxygen tags
 *
 * Revision 1.1  2003/09/02 22:26:34  kapustin
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* ALGO___NW_FORMATTER__HPP */
