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
#include <objtools/blast_format/blastfmtutil.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

/** @addtogroup BlastFormatting
 *
 * @{
 */

/// Class containing information needed for tabular formatting of BLAST 
/// results.
class NCBI_XBLASTFORMAT_EXPORT CBlastTabularInfo : public CObject 
{
public:
    /// In what form should the sequence identifiers be shown?
    enum ESeqIdType {
        eFullId = 0, ///< Show full seq-id, with multiple ids concatenated.
        eAccession,  ///< Show only best accession
        eGi          ///< Show only gi
    };

    /// What delimiter to use between fields in each row of the tabular output.
    enum EFieldDelimiter {
        eTab = 0, ///< Tab
        eSpace,   ///< Space
        eComma    ///< Comma
    };

    /// Constructor
    /// @param ostr Stream to write output to [in]
    /// @param format Output format - what fields to include in the output [in]
    /// @param delim Delimiter to use between tabular fields [in]
    CBlastTabularInfo(CNcbiOstream& ostr, const string& format=NcbiEmptyString,
                      EFieldDelimiter delim=eTab);
    /// Destructor
    ~CBlastTabularInfo();
    /// Set query id from a CSeq_id
    /// @param id List of Seq-ids to use [in]
    void SetQueryId(list<CRef<CSeq_id> >& id);
    /// Set query id from a Bioseq handle
    /// @param bh Bioseq handle to get Seq-ids from
    void SetQueryId(const CBioseq_Handle& bh);
    /// Set subject id from a CSeq_id
    /// @param id List of Seq-ids to use [in]
    void SetSubjectId(list<CRef<CSeq_id> >& id);
    /// Set subject id from a Bioseq handle
    /// @param bh Bioseq handle to get Seq-ids from
    void SetSubjectId(const CBioseq_Handle& bh);
    /// Set the HSP scores
    /// @param score Raw score [in]
    /// @param bit_score Bit score [in]
    /// @param evalue Expect value [in]
    void SetScores(int score, double bit_score, double evalue);
    /// Set the HSP endpoints. Note that if alignment is on opposite strands,
    /// the subject offsets must be reversed.
    /// @param q_start Starting offset in query [in]
    /// @param q_end Ending offset in query [in]
    /// @param s_start Starting offset in subject [in]
    /// @param s_end Ending offset in subject [in]
    void SetEndpoints(int q_start, int q_end, int s_start, int s_end);
    /// Set various counts/lengths
    /// @param num_ident Number of identities [in]
    /// @param length Alignment length [in]
    /// @param gaps Total number of gaps [in]
    /// @param gap_opens Number of gap openings [in]
    /// @param positives Number of positives [in]
    void SetCounts(int num_ident, int length, int gaps, int gap_opens, 
                   int positives =0, int query_frame = 1, 
                   int subject_frame = 1);
    /// Set all member fields, given a Seq-align
    /// @param sal Seq-align to get data from [in]
    /// @param scope Scope for Bioseq retrieval [in]
    /// @param matrix Matrix to calculate positives; NULL if not applicable. [in]
    /// @return 0 on success, 1 if query or subject Bioseq is not found.
    virtual int SetFields(const CSeq_align& sal, 
                          CScope& scope, 
                          CBlastFormattingMatrix* matrix=0);

    /// Print one line of tabular output
    virtual void Print(void);
    /// Print the tabular output header
    /// @param program Program name to show in the header [in]
    /// @param bioseq Query Bioseq [in]
    /// @param dbname Search database name [in]
    /// @param rid the search RID (if not applicable, it should be empty
    /// the string) [in]
    /// @param iteration Iteration number (for PSI-BLAST), use default
    /// parameter value when not applicable [in]
    /// @param align_set All alignments for this query [in]
    virtual void PrintHeader(const string& program, 
                             const CBioseq& bioseq, 
                             const string& dbname, 
                             const string& rid = kEmptyStr,
                             unsigned int iteration = 
                                numeric_limits<unsigned int>::max(),
                             const CSeq_align_set* align_set=0); 
    /// Enumeration for all fields that are supported in the tabular output
    enum ETabularField {
        eQuerySeqId = 0,       ///< Query Seq-id(s)
        eQueryGi,              ///< Query gi
        eQueryAccession,       ///< Query accession
        eSubjectSeqId,         ///< Subject Seq-id(s)
        eSubjectAllSeqIds,     ///< If multiple redundant sequences, all sets
                               /// of subject Seq-ids, separated by ';'
        eSubjectGi,            ///< Subject gi
        eSubjectAllGis,        ///< All subject gis
        eSubjectAccession,     ///< Subject accession 
        eSubjectAllAccessions, ///< All subject accessions, separated by ';'
        eQueryStart,           ///< Start of alignment in query
        eQueryEnd,             ///< End of alignment in query
        eSubjectStart,         ///< Start of alignment in subject
        eSubjectEnd,           ///< End of alignment in subject
        eQuerySeq,             ///< Aligned part of query sequence
        eSubjectSeq,           ///< Aligned part of subject sequence
        eEvalue,               ///< Expect value
        eBitScore,             ///< Bit score
        eScore,                ///< Raw score
        eAlignmentLength,      ///< Alignment length
        ePercentIdentical,     ///< Percent of identical matches
        eNumIdentical,         ///< Number of identical matches
        eMismatches,           ///< Number of mismatches
        ePositives,            ///< Number of positive-scoring matches
        eGapOpenings,          ///< Number of gap openings
        eGaps,                 ///< Total number of gaps
        ePercentPositives,     ///<percent positives 
        eFrames,               ///< frames
        eMaxTabularField       ///< Sentinel value
    };

    /// Return all field names supported in the format string.
    list<string> GetAllFieldNames(void);

protected:
    /// Add a field to the list of fields to show, if it is not yet present in
    /// the list of fields.
    /// @param field Which field to add? [in]
    void x_AddFieldToShow(ETabularField field);
    /// Delete a field from the list of fields to show
    /// @param field Which field to delete? [in]
    void x_DeleteFieldToShow(ETabularField field);
    /// Add a default set of fields to show.
    void x_AddDefaultFieldsToShow(void);
    /// Set fields to show, given an output format string
    /// @param format Output format [in]
    void x_SetFieldsToShow(const string& format);
    /// Reset values of all fields.
    void x_ResetFields(void);
    /// Set the tabular fields delimiter.
    /// @param delim Which delimiter to use
    void x_SetFieldDelimiter(EFieldDelimiter delim);
    /// Print the names of all supported fields
    void x_PrintFieldNames(void);
    /// Print the value of a given field
    /// @param field Which field to show? [in]
    void x_PrintField(ETabularField field);
    /// Print query Seq-id
    void x_PrintQuerySeqId(void);
    /// Print query gi
    void x_PrintQueryGi(void);
    /// Print query accession
    void x_PrintQueryAccession(void);
    /// Print subject Seq-id
    void x_PrintSubjectSeqId(void);
    /// Print all Seq-ids associated with this subject, separated by ';'
    void x_PrintSubjectAllSeqIds(void);
    /// Print subject gi
    void x_PrintSubjectGi(void);
    /// Print all gis associated with this subject, separated by ';'
    void x_PrintSubjectAllGis(void);
    /// Print subject accessions
    void x_PrintSubjectAccession(void);
    /// Print all accessions associated with this subject, separated by ';'
    void x_PrintSubjectAllAccessions(void);
    /// Print aligned part of query sequence
    void x_PrintQuerySeq(void);
    /// Print aligned part of subject sequence
    void x_PrintSubjectSeq(void);
    /// Print query start
    void x_PrintQueryStart(void);
    /// Print query end
    void x_PrintQueryEnd(void);
    /// Print subject start
    void x_PrintSubjectStart(void);
    /// Print subject end
    void x_PrintSubjectEnd(void);
    /// Print e-value
    void x_PrintEvalue(void);
    /// Print bit score
    void x_PrintBitScore(void);
    /// Print raw score
    void x_PrintScore(void);
    /// Print alignment length
    void x_PrintAlignmentLength(void);
    /// Print percent of identical matches
    void x_PrintPercentIdentical(void);
    /// Print number of identical matches
    void x_PrintNumIdentical(void);
    /// Print number of mismatches
    void x_PrintMismatches(void);
    /// Print number of positive matches
    void x_PrintNumPositives(void);
    /// Print number of gap openings
    void x_PrintGapOpenings(void);
    /// Print total number of gaps
    void x_PrintGaps(void);
    /// Print percent positives
    void x_PrintPercentPositives();
    /// Print frames
    void x_PrintFrames();

private:

    list<CRef<CSeq_id> > m_QueryId;  ///< List of query ids for this HSP
    /// All subject sequence ids for this HSP
    vector<list<CRef<CSeq_id> > > m_SubjectIds;
    CNcbiOstream& m_Ostream; ///< Stream to write output to
    int m_Score;             ///< Raw score of this HSP
    string m_BitScore;       ///< Bit score of this HSP, in appropriate format
    string m_Evalue;         ///< E-value of this HSP, in appropriate format
    int m_AlignLength;       ///< Alignment length of this HSP
    int m_NumGaps;           ///< Total number of gaps in this HSP
    int m_NumGapOpens;       ///< Number of gap openings in this HSP
    int m_NumIdent;          ///< Number of identities in this HSP
    int m_NumPositives;      ///< Number of positives in this HSP
    int m_QueryStart;        ///< Starting offset in query
    int m_QueryEnd;          ///< Ending offset in query
    int m_SubjectStart;      ///< Starting offset in subject
    int m_SubjectEnd;        ///< Ending offset in subject 
    string m_QuerySeq;       ///< Aligned part of the query sequence
    string m_SubjectSeq;     ///< Aligned part of the subject sequence
    int m_QueryFrame;        ///< query frame
    int m_SubjectFrame;      ///< subject frame
    /// Map of field enum values to field names.
    map<string, ETabularField> m_FieldMap; 
    list<ETabularField> m_FieldsToShow; ///< Which fields to show?
    char m_FieldDelimiter;   ///< Delimiter character for tabular fields.
};

inline void 
CBlastTabularInfo::SetScores(int score, double bit_score, double evalue)
{
    string total_bit_string;
    m_Score = score;
    CBlastFormatUtil::GetScoreString(evalue, bit_score, 0, m_Evalue, 
                                     m_BitScore, total_bit_string);
}

inline void 
CBlastTabularInfo::SetEndpoints(int q_start, int q_end, int s_start, int s_end)
{
    m_QueryStart = q_start;
    m_QueryEnd = q_end;
    m_SubjectStart = s_start;
    m_SubjectEnd = s_end;    
}

inline void 
CBlastTabularInfo::SetCounts(int num_ident, int length, int gaps, int gap_opens,
                             int positives, int query_frame, int subject_frame)
{
    m_AlignLength = length;
    m_NumIdent = num_ident;
    m_NumGaps = gaps;
    m_NumGapOpens = gap_opens;
    m_NumPositives = positives;
    m_QueryFrame = query_frame;
    m_SubjectFrame = subject_frame;
}

inline void
CBlastTabularInfo::SetQueryId(list<CRef<CSeq_id> >& id)
{
    m_QueryId = id;
}

inline void 
CBlastTabularInfo::SetSubjectId(list<CRef<CSeq_id> >& id)
{
    m_SubjectIds.push_back(id);
}

inline list<string> 
CBlastTabularInfo::GetAllFieldNames(void)
{
    list<string> field_names;

    for (map<string,ETabularField>::iterator iter = m_FieldMap.begin();
         iter != m_FieldMap.end(); ++iter) {
        field_names.push_back((*iter).first);
    }
    return field_names;
}

inline void 
CBlastTabularInfo::x_AddFieldToShow(ETabularField field)
{
    if (find(m_FieldsToShow.begin(), m_FieldsToShow.end(), field) == 
        m_FieldsToShow.end())
        m_FieldsToShow.push_back(field);
}

inline void 
CBlastTabularInfo::x_DeleteFieldToShow(ETabularField field)
{
    list<ETabularField>::iterator iter; 

    while ((iter = find(m_FieldsToShow.begin(), m_FieldsToShow.end(), field))
           != m_FieldsToShow.end())
        m_FieldsToShow.erase(iter); 
}

inline void 
CBlastTabularInfo::x_AddDefaultFieldsToShow()
{
    x_AddFieldToShow(eQueryAccession);
    x_AddFieldToShow(eSubjectAllAccessions);
    x_AddFieldToShow(ePercentIdentical);
    x_AddFieldToShow(eAlignmentLength);
    x_AddFieldToShow(eMismatches);
    x_AddFieldToShow(eGapOpenings);
    x_AddFieldToShow(eQueryStart);
    x_AddFieldToShow(eQueryEnd);
    x_AddFieldToShow(eSubjectStart);
    x_AddFieldToShow(eSubjectEnd);
    x_AddFieldToShow(eEvalue);
    x_AddFieldToShow(eBitScore);
}

inline void 
CBlastTabularInfo::x_PrintField(ETabularField field)
{
    switch (field) {
    case eQuerySeqId:
        x_PrintQuerySeqId(); break;
    case eQueryGi:
        x_PrintQueryGi(); break;
    case eQueryAccession:
        x_PrintQueryAccession(); break;
    case eSubjectSeqId:
        x_PrintSubjectSeqId(); break;
    case eSubjectAllSeqIds:
        x_PrintSubjectAllSeqIds(); break;
    case eSubjectGi:
        x_PrintSubjectGi(); break;
    case eSubjectAllGis:
        x_PrintSubjectAllGis(); break;
    case eSubjectAccession:
        x_PrintSubjectAccession(); break;
    case eSubjectAllAccessions:
        x_PrintSubjectAllAccessions(); break;
    case eQueryStart:
        x_PrintQueryStart(); break;
    case eQueryEnd:
        x_PrintQueryEnd(); break;
    case eSubjectStart:
        x_PrintSubjectStart(); break;
    case eSubjectEnd:
        x_PrintSubjectEnd(); break;
    case eQuerySeq:
        x_PrintQuerySeq(); break;
    case eSubjectSeq:
        x_PrintSubjectSeq(); break;
    case eEvalue:
        x_PrintEvalue(); break;
    case eBitScore:
        x_PrintBitScore(); break;
    case eScore:
        x_PrintScore(); break;
    case eAlignmentLength:
        x_PrintAlignmentLength(); break;
    case ePercentIdentical:
        x_PrintPercentIdentical(); break;
    case eNumIdentical:
        x_PrintNumIdentical(); break;
    case eMismatches:
        x_PrintMismatches(); break;
    case ePositives:
        x_PrintNumPositives(); break;
    case eGapOpenings:
        x_PrintGapOpenings(); break;
    case eGaps:
        x_PrintGaps(); break;
    case ePercentPositives:
        x_PrintPercentPositives(); break;
    case eFrames:
        x_PrintFrames(); break;
    default:
        break;
    }
}

inline void CBlastTabularInfo::x_PrintQuerySeq(void)
{
    m_Ostream << m_QuerySeq;
}

inline void CBlastTabularInfo::x_PrintSubjectSeq(void)
{
    m_Ostream << m_SubjectSeq;
}

inline void CBlastTabularInfo::x_PrintQueryStart(void)
{
    m_Ostream << m_QueryStart;
}

inline void CBlastTabularInfo::x_PrintQueryEnd(void)
{
    m_Ostream << m_QueryEnd;
}

inline void CBlastTabularInfo::x_PrintSubjectStart(void)
{
    m_Ostream << m_SubjectStart;
}

inline void CBlastTabularInfo::x_PrintSubjectEnd(void)
{
    m_Ostream << m_SubjectEnd;
}

inline void CBlastTabularInfo::x_PrintEvalue(void)
{
    m_Ostream << m_Evalue;
}

inline void CBlastTabularInfo::x_PrintBitScore(void)
{
    m_Ostream << m_BitScore;
}

inline void CBlastTabularInfo::x_PrintScore(void)
{
    m_Ostream << m_Score;
}

inline void CBlastTabularInfo::x_PrintAlignmentLength(void)
{
    m_Ostream << m_AlignLength;
}

inline void CBlastTabularInfo::x_PrintPercentIdentical(void)
{
    double perc_ident = 
        (m_AlignLength > 0 ? ((double)m_NumIdent)/m_AlignLength * 100 : 0);
    m_Ostream << NStr::DoubleToString(perc_ident, 2);
}

inline void CBlastTabularInfo::x_PrintPercentPositives(void)
{
    double perc_positives = 
        (m_AlignLength > 0 ? ((double)m_NumPositives)/m_AlignLength * 100 : 0);
    m_Ostream << NStr::DoubleToString(perc_positives, 2);
}

inline void CBlastTabularInfo::x_PrintFrames(void)
{
    m_Ostream << m_QueryFrame << "/" << m_SubjectFrame;
}

inline void CBlastTabularInfo::x_PrintNumIdentical(void)
{
    m_Ostream << m_NumIdent;
}

inline void CBlastTabularInfo::x_PrintMismatches(void)
{
    int num_mismatches = m_AlignLength - m_NumIdent - m_NumGaps;
    m_Ostream << num_mismatches;
}

inline void CBlastTabularInfo::x_PrintNumPositives(void)
{
    m_Ostream << m_NumPositives;
}

inline void CBlastTabularInfo::x_PrintGapOpenings(void)
{
    m_Ostream << m_NumGapOpens;
}

inline void CBlastTabularInfo::x_PrintGaps(void)
{
    m_Ostream << m_NumGaps;
}


/* @} */

END_NCBI_SCOPE

#endif /* OBJTOOLS_BLASTFORMAT___TABULAR__HPP */
