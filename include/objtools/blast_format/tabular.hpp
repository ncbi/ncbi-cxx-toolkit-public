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
    /// In what form should the sequence identifiers be shown?
    enum ESeqIdType {
        eFullId = 0, ///< Show full seq-id, with multiple ids concatenated.
        eAccession,  ///< Show only best accession
        eGi          ///< Show only gi
    };

    /// Constructor
    CBlastTabularInfo(CNcbiOstream& ostr, const string& format=NcbiEmptyString);
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
    void SetEndpoints(int q_start, int q_end, int s_start, int s_end);
    /// Set various counts/lengths
    void SetCounts(int num_ident, int length, int gaps, int gap_opens, 
                   int positives=0);
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
    virtual void PrintHeader(const string& program, 
                             const CBioseq& bioseq, 
                             const string& dbname, int iteration,
                             const CSeq_align_set* align_set=0); 
    /// Enumeration for all fields that are supported in the tabular output
    enum ETabularField {
        eQuerySeqId = 0,
        eQueryGi,
        eQueryAccession,
        eSubjectSeqId,
        eSubjectAllSeqIds,
        eSubjectGi,
        eSubjectAllGis,
        eSubjectAccession,
        eSubjectAllAccessions,
        eQueryStart,
        eQueryEnd,
        eSubjectStart,
        eSubjectEnd,
        eQuerySeq,
        eSubjectSeq,
        eEvalue,
        eBitScore,
        eScore,
        eAlignmentLength,
        ePercentIdentical,
        eNumIdentical,
        eMismatches,
        ePositives,
        eGapOpenings,
        eGaps,
        eMaxTabularField
    };

    /// Return all field names supported in the format string.
    list<string> GetAllFieldNames(void);

protected:
    void x_AddFieldToShow(ETabularField field);
    void x_DeleteFieldToShow(ETabularField field);
    void x_AddDefaultFieldsToShow(void);
    void x_SetFieldsToShow(const string& format);
    void x_ResetFields(void);

    void x_PrintFieldNames(void);
    void x_PrintField(ETabularField field);
    void x_PrintQuerySeqId(void);
    void x_PrintQueryGi(void);
    void x_PrintQueryAccession(void);
    void x_PrintSubjectSeqId(void);
    void x_PrintSubjectAllSeqIds(void);
    void x_PrintSubjectGi(void);
    void x_PrintSubjectAllGis(void);
    void x_PrintSubjectAccession(void);
    void x_PrintSubjectAllAccessions(void);
    void x_PrintQuerySeq(void);
    void x_PrintSubjectSeq(void);
    void x_PrintQueryStart(void);
    void x_PrintQueryEnd(void);
    void x_PrintSubjectStart(void);
    void x_PrintSubjectEnd(void);
    void x_PrintEvalue(void);
    void x_PrintBitScore(void);
    void x_PrintScore(void);
    void x_PrintAlignmentLength(void);
    void x_PrintPercentIdentical(void);
    void x_PrintNumIdentical(void);
    void x_PrintMismatches(void);
    void x_PrintNumPositives(void);
    void x_PrintGapOpenings(void);
    void x_PrintGaps(void);

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

    map<string, ETabularField> m_FieldMap;
    list<ETabularField> m_FieldsToShow; ///< Which fields to show?
};

/// Set the HSP scores
/// @param score Raw score [in]
/// @param bit_score Bit score [in]
/// @param evalue E-value [in]
inline void 
CBlastTabularInfo::SetScores(int score, double bit_score, double evalue)
{
    m_Score = score;
    CBlastFormatUtil::GetScoreString(evalue, bit_score, m_Evalue, 
                                     m_BitScore);
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
/// @param positives Number of positive scoring residue matches [in]
inline void 
CBlastTabularInfo::SetCounts(int num_ident, int length, int gaps, int gap_opens,
                             int positives)
{
    m_AlignLength = length;
    m_NumIdent = num_ident;
    m_NumGaps = gaps;
    m_NumGapOpens = gap_opens;
    m_NumPositives = positives;
}

inline void
CBlastTabularInfo::SetQueryId(list<CRef<CSeq_id> >& id)
{
    m_QueryId = id;
}

/// Set subject id
inline void 
CBlastTabularInfo::SetSubjectId(list<CRef<CSeq_id> >& id)
{
    m_SubjectIds.push_back(id);
}

/// Return all field names supported in the format string.
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

/// Add a field to show, if it is not yet present in the list of fields.
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

/*
* ===========================================================================
*
* $Log$
* Revision 1.6  2005/08/01 14:57:51  dondosha
* Added API for choosing an arbitrary list of fields to show
*
* Revision 1.5  2005/05/25 13:00:40  camacho
* Do not pull objects namespace
*
* Revision 1.4  2005/05/11 16:21:51  dondosha
* Small doxygen fix
*
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

