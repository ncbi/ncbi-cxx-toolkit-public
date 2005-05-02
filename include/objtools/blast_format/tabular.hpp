/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* Author:  Ilya Dondoshansky
*
* ===========================================================================
*/

/// @file: tabular.hpp
/// Formatting of pairwise sequence alignments in tabular form. 

#ifndef OBJTOOLS_BLASTFORMAT___TABULAR__HPP
#define OBJTOOLS_BLASTFORMAT___TABULAR__HPP

#include <corelib/ncbistre.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/scope.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

/** @addtogroup BlastFormat
 *
 * @{
 */

/// Class containing information needed for tabular formatting of BLAST 
/// results.
class CBlastTabularInfo : public CObject 
{
public:
    /// Constructor
    CBlastTabularInfo(CNcbiOstream& ostr);
    /// Destructor
    ~CBlastTabularInfo();
    /// Set query id from a CSeq_id
    void SetQueryId(list<CRef<CSeq_id> >& id);
    /// Set query id from a Bioseq handle
    void SetQueryId(const CBioseq_Handle& bh);
    /// Set subject id from a CSeq_id
    void SetSubjectId(list<CRef<CSeq_id> >& id);
    /// Set subject id from a Bioseq handle
    void SetSubjectId(const CBioseq_Handle& bh);
    /// Set the HSP scores
    void SetScores(int score, double bit_score, double evalue);
    /// Set the HSP endpoints
    void SetEndpoints(int q_start, int q_end, 
                      int s_start, int s_end);
    /// Set various counts/lengths
    void SetCounts(int num_ident, int length, 
                   int gaps, int gap_opens);
    /// Set all member fields, given a Seq-align
    /// @param sal Seq-align to get data from [in]
    /// @param scope Scope for Bioseq retrieval [in]
    /// @return 0 on success, 1 if query or subject Bioseq is not found.
    virtual int SetFields(const CSeq_align& sal, CScope& scope);

    /// Tabular formatting options enumeration. Any non-default values request to
    /// add extra fields to the output on every line.
    enum ETabularOption {
        eDefault = 0,             ///< Default set of fields
        eAddRawScore = (1 << 0),  ///< Add a raw score 
        eAddGapCount = (1 << 1),  ///< Add a total gap count 
        eAddPositives = (1 << 2), ///< Add number of positives 
                                  /// @todo Needs implementation
        eAddSeqs = (1 << 3),      ///< Add aligned parts of sequences 
                                  /// @todo Needs implementation 
        eAddSegments = (1 << 4)   ///< Add all segments information
                                  /// @todo Needs implementation
    };


    /// Print one line of tabular output
    virtual void Print(ETabularOption opt = eDefault);
    /// Print the tabular output header
    virtual void PrintHeader(const string& program, const CBioseq& bioseq, 
                             const string& dbname, int iteration, 
                             ETabularOption opt = eDefault); 

private:

    string m_QueryId;        ///< Query sequence id for this HSP
    string m_SubjectId;      ///< Subject sequence id for this HSP
    CNcbiOstream& m_Ostream; ///< Stream to write output to
    int m_Score;             ///< Raw score of this HSP
    double m_BitScore;       ///< Bit score of this HSP
    double m_Evalue;         ///< E-value of this HSP
    int m_AlignLength;       ///< Alignment length of this HSP
    int m_NumGaps;           ///< Total number of gaps in this HSP
    int m_NumGapOpens;       ///< Number of gap openings in this HSP
    int m_NumIdent;          ///< Number of identities in this HSP
    int m_QueryStart;        ///< Starting offset in query
    int m_QueryEnd;          ///< Ending offset in query
    int m_SubjectStart;      ///< Starting offset in subject
    int m_SubjectEnd;        ///< Ending offset in subject 
    string m_QuerySeq;       ///< Aligned part of the query sequence
    string m_SubjectSeq;     ///< Aligned part of the subject sequence
};

/// Set the HSP scores
/// @param score Raw score [in]
/// @param bit_score Bit score [in]
/// @param evalue E-value [in]
inline void 
CBlastTabularInfo::SetScores(int score, double bit_score, double evalue)
{
    m_Score = score;
    m_BitScore = bit_score;
    m_Evalue = evalue;    
}

/// Set HSP endpoints. Note that if alignment is on opposite strands, the 
/// subject offsets must be reversed.
/// @param q_start Query starting offset [in]
/// @param q_end Query ending offset [in]
/// @param s_start Subject starting offset [in]
/// @param s_end Subject ending offset [in]
inline void 
CBlastTabularInfo::SetEndpoints(int q_start, int q_end, int s_start, int s_end)
{
    m_QueryStart = q_start;
    m_QueryEnd = q_end;
    m_SubjectStart = s_start;
    m_SubjectEnd = s_end;    
}

/// Set various counts/lengths
/// @param num_ident Number of identities [in]
/// @param length Alignment length [in]
/// @param gaps Total number of gaps [in]
/// @param gap_opens Number of gap openings [in]
inline void 
CBlastTabularInfo::SetCounts(int num_ident, int length, int gaps, int gap_opens)
{
    m_AlignLength = length;
    m_NumIdent = num_ident;
    m_NumGaps = gaps;
    m_NumGapOpens = gap_opens;
}

END_NCBI_SCOPE

/* @} */

/*
* ===========================================================================
*
* $Log$
* Revision 1.3  2005/05/02 17:32:58  dondosha
* Changed return value of SetFields to int, to allow error return
*
* Revision 1.2  2005/04/28 19:28:17  dondosha
* Changed CBioseq_Handle argument in PrintHeader to CBioseq, needed for web formatting
*
* Revision 1.1  2005/03/14 20:12:06  dondosha
* Tabular formatting of BLAST results
*
*
* ===========================================================================
*/

#endif /* OBJTOOLS_BLASTFORMAT___TABULAR__HPP */

