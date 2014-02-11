#ifndef ALGO_ALIGN_SPLIGN_FORMATTER__HPP
#define ALGO_ALIGN_SPLIGN_FORMATTER__HPP

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
*   Splign formatter
*/

#include <algo/align/splign/splign.hpp>


BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
    class CSeq_id;
    class CDense_seg;
    class CSeq_align;
    class CSeq_align_set;
    class CScope;
END_SCOPE(objects)


class NCBI_XALGOALIGN_EXPORT CSplignFormatter: public CObject
{
public:

    CSplignFormatter(const CSplign::TResults& results);
    CSplignFormatter(const CSplign& splign);

    // setters
    void SetSeqIds(CConstRef<objects::CSeq_id> id1, 
                   CConstRef<objects::CSeq_id> id2);

    enum ETextFlags {
        eTF_None                 = 0,
        eTF_NoExonScores         = 0x0001,
        eTF_UseFastaStyleIds     = 0x0002
    };

    enum EAsnFlags {
        eAF_Disc                 = 0x0000,
        eAF_SplicedSegNoParts    = 0x0001,
        eAF_SplicedSegWithParts  = 0x0003,
        eAF_NoVersion            = 0x0004
    };

    // formatters
    string AsExonTable(const CSplign::TResults* results = 0,
                       int flags = eTF_None) const;

    /// Format alignment as plain text.
    ///
    /// @param scope
    ///   Source for sequence data.
    /// @param results
    ///   Splign results for formatting. If not specified, the results
    ///   will be read from the object used to construct the formatter.
    /// @param line_width
    ///   The maximum number of alignment chars per line.
    /// @param segnum
    ///   The segment to print in each compartment, or -1 to print all segments.
    /// @return
    ///   Formatted alignment.
    string AsAlignmentText(CRef<objects::CScope> scope,
                           const CSplign::TResults* results = 0,
                           size_t line_width = 80,
                           int segnum = -1)
        const;

    /// Format alignment as a seq-align-set
    ///
    /// @param results
    ///   Splign results for formatting. If not specified, the results
    ///   will be read from the object used to construct the formatter.
    /// @param flags
    ///   A bitwise combination of EAsnFlags.
    /// @return
    ///   Formatted alignment as a seq-align-set reference.
    CRef<objects::CSeq_align_set> AsSeqAlignSet(
        const CSplign::TResults* results = 0,
        int flags  = eAF_SplicedSegWithParts)
        const;

private:

    const CSplign::TResults            m_splign_results;

    CConstRef<objects::CSeq_id>        m_QueryId, m_SubjId;

    CRef<objects::CSeq_align> x_Compartment2SeqAlign(
                               const vector<size_t>& boxes,
                               const vector<string>& transcripts,
                               const vector<float>&  scores) const;

    void x_Exon2DS(const size_t* box, const string& trans,
                   objects::CDense_seg* pds) const;

    void x_Init(void);
};


END_NCBI_SCOPE

#endif
