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
* Authors: Amelia Fong 
* 
*/

/** @file data4xml2format.hpp
 * Implementation of interface class to produce data required for generating
 * BLAST XML2 output
 */

#ifndef APP___DATA4XML2FORMAT__HPP
#define APP___DATA4XML2FORMAT__HPP

#include <objects/seq/seqlocinfo.hpp>

#include <algo/blast/api/sseqloc.hpp>
#include <algo/blast/api/setup_factory.hpp>
#include <algo/blast/api/uniform_search.hpp>

#include <algo/blast/format/blastfmtutil.hpp>
#include <algo/blast/format/blastxml2_format.hpp>

#include <algo/blast/api/blast_seqinfosrc.hpp>

BEGIN_NCBI_SCOPE

/// Strategy class to gather the data for generating BLAST XML output
class CCmdLineBlastXML2ReportData : public IBlastXML2ReportData
{
public:

    /// Constructor db search
    /// @param query Query sequences [in]
    /// @param results results set containing one query per element or one
    /// @param opts Blast options container [in]
	/// @param scope scope containing query and subject seqs [in]
    /// @param dbsInfo vector of SDbInfo containing db names and type [in]
	CCmdLineBlastXML2ReportData(CConstRef<blast::CBlastSearchQuery> query,
								const blast::CSearchResults& results,
	               	   	   	   	CConstRef<blast::CBlastOptions> opts,
	               	   	   	   	CRef<objects::CScope> scope,
	               	   	   	   	const vector<align_format::CAlignFormatUtil::SDbInfo> & dbsInfo);

    /// Constructor bl2seq in db mode
    /// @param subjectsInfo contains bl2seq subjects info [in]
	 CCmdLineBlastXML2ReportData(CConstRef<blast::CBlastSearchQuery> query,
			 	 	 	 	 	 const blast::CSearchResults& results,
	               	   	   	   	 CConstRef<blast::CBlastOptions> opts,
	               	   	   	   	 CRef<objects::CScope> scope,
	               	   	   	   	 CConstRef<blast::IBlastSeqInfoSrc> subjectsInfo);

    /// Constructor iterative db search
    /// @param resultSet containing results from all iteration [in]
    CCmdLineBlastXML2ReportData(CConstRef<blast::CBlastSearchQuery> query,
    							const blast::CSearchResultSet& resultSet,
               	   	   	   	    CConstRef<blast::CBlastOptions> opts,
               					CRef<objects::CScope> scope,
               					const vector<align_format::CAlignFormatUtil::SDbInfo> & dbsInfo);

    /// Constructor iterative bl2seq or bl2seq in orginal mode
    /// @param resultSet containing results from all iteration [in]
    CCmdLineBlastXML2ReportData(CConstRef<blast::CBlastSearchQuery> query,
    							const blast::CSearchResultSet& resultSet,
               	   	   	   	    CConstRef<blast::CBlastOptions> opts,
               	   	   	   	    CRef<objects::CScope>  scope,
               	   	   	   	    CConstRef<blast::IBlastSeqInfoSrc> subjectsInfo);

    /// Destructor
    ~CCmdLineBlastXML2ReportData();

    //------------ callbacks needed by IBlastXMLReportData ---------

    /// @inheritDoc
    string GetBlastProgramName(void) const;

    /// @inheritDoc
    blast::EProgram GetBlastTask(void) const {
        return m_Options->GetProgram();
    }

    /// @inheritDoc
    string GetDatabaseName(void) const { return m_DbName; }

    /// @inheritDoc
    double GetEvalueThreshold(void) const { 
        return m_Options->GetEvalueThreshold();
    }

    /// @inheritDoc
    int GetGapOpeningCost(void) const {
        return m_Options->GetGapOpeningCost();
    }

    /// @inheritDoc
    int GetGapExtensionCost(void) const {
        return m_Options->GetGapExtensionCost();
    }

    /// @inheritDoc
    int GetMatchReward(void) const {
        return m_Options->GetMatchReward();
    }

    /// @inheritDoc
    int GetMismatchPenalty(void) const {
        return m_Options->GetMismatchPenalty();
    }

    string GetPHIPattern(void) const {
        const char *tmp = m_Options->GetPHIPattern();
        return tmp == NULL ? string() : string(tmp);
    }

    string GetFilterString(void) const {
        blast::TAutoCharPtr tmp = 
            m_Options->GetFilterString(); /* NCBI_FAKE_WARNING */
        return tmp.get() == NULL ? NcbiEmptyString : string(tmp.get());
    }

    string GetMatrixName(void) const {
        const char *tmp = m_Options->GetMatrixName();
        return tmp == NULL ? string() : string(tmp);
    }

    CBlastFormattingMatrix* GetMatrix(void) const;

    CConstRef<objects::CSeq_loc> GetQuerySeqLoc(void) const {
        return m_Query->GetQuerySeqLoc();
    }

    const TMaskedQueryRegions &
        GetMaskLocations(void) const {
        return m_QueryMasks;
    }

    Int8 GetDbNumSeqs(void) const {
        return m_NumSequences;
    }

    Int8 GetDbLength(void) const {
        return m_NumBases;
    }

    int GetLengthAdjustment(int num) const;

    Int8 GetEffectiveSearchSpace(int num) const;

    double GetLambda(int num) const;

    double GetKappa(int num) const;

    double GetEntropy(int num) const;

    CRef<objects::CScope> GetScope() const {
        return m_Scope;
    }

    CConstRef<CSeq_align_set> GetAlignmentSet(int num) const;

    int GetQueryGeneticCode() const;

    int GetDbGeneticCode() const;

    string GetMessages(int num) const { return m_Errors[num]; }

    bool CanGetTaxInfo(void) const { return m_TaxDBFound; }

    bool IsBl2seq(void) const {return m_isBl2seq;}

    int GetNumOfSearchResults(void) const {return (int) m_Alignments.size();}

    list<string> GetSubjectIds(void) const {return m_SubjectIds;}

    bool IsGappedSearch(void) const {return m_Options->GetGappedMode();}

    int GetCompositionBasedStats(void) const {return m_Options->GetCompositionBasedStats();}

    string GetBl2seqMode(void) const { return kEmptyStr; };

    bool IsIterativeSearch(void) const {return m_isIterative;}
    string GetEntrezQuery(void) const {return kEmptyStr;}

private:

    CConstRef<blast::CBlastSearchQuery> m_Query;
    /// BLAST algorithm options
    CConstRef<blast::CBlastOptions> m_Options;
    CRef<objects::CScope> m_Scope;
    string m_DbName;            ///< name of blast database
    /// Number of sequences in all BLAST databases involved in this search
    Int8 m_NumSequences;
    /// Number of bases in all BLAST databases involved in this search
    Int8 m_NumBases;
    bool m_TaxDBFound;
    bool m_isBl2seq;
    bool m_isIterative;

    /* Folowwing vectors are one for each iteration */
    /// ancillary results data
    vector<CRef<blast::CBlastAncillaryData> >  m_AncillaryData;
    /// the alignments
    vector<CConstRef<CSeq_align_set>  > m_Alignments;
    /// Error messages
    vector<string> m_Errors;

    /// Number of columns used in score matrices
    static const unsigned int kMatrixCols = 28;

    /// Score matrix used to determine neighboring protein residues
    CBlastFormattingMatrix * m_Matrix;

    list<string> m_SubjectIds;

    TMaskedQueryRegions m_QueryMasks;

    /// Initialize the score matrix to be used for formatting
    /// (if applicable)
    /// @param matrix_name Name of score matrix. NULL defaults to
    ///                    BLOSUM62 [in]
    ///
    void x_FillScoreMatrix(const char *matrix_name = BLAST_DEFAULT_MATRIX);

    void x_InitResults(const blast::CSearchResults & results);
    void x_InitCommon(const blast::CSearchResults & results, CConstRef<blast::CBlastOptions> opts);
    void x_InitDB(const vector<align_format::CAlignFormatUtil::SDbInfo> & dbsInfo);
    void x_InitSubjects(CConstRef<blast::IBlastSeqInfoSrc> subjectsInfo);
};

END_NCBI_SCOPE

#endif /* !APP___DATA4XML2FORMAT__HPP */

