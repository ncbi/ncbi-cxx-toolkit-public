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

/// @file: blastxml_format.hpp
/// Formatting of pairwise sequence alignments in XML form. 

#ifndef ALGO_BLAST_FORMAT___BLASTXML2_FORMAT__HPP
#define ALGO_BLAST_FORMAT___BLASTXML2_FORMAT__HPP

#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/scope.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

#include <objtools/align_format/showalign.hpp>
#include <algo/blast/format/blastfmtutil.hpp>

#include <objects/seq/seqlocinfo.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup BlastFormatting
 *
 * @{
 */

/// Interface for filling the top layer of the XML report
class NCBI_XBLASTFORMAT_EXPORT IBlastXML2ReportData
{
public:
    /// Our virtual destructor
    virtual ~IBlastXML2ReportData() {}
    /// Returns BLAST program name as string.
    virtual string GetBlastProgramName(void) const = 0;
    /// Returns BLAST task as an enumerated value.
    virtual blast::EProgram GetBlastTask(void) const = 0;
    /// Returns database name.
    virtual string GetDatabaseName(void) const = 0;
    /// Returns e-value theshold used in search.
    virtual double GetEvalueThreshold(void) const = 0;
    /// Returns gap opening cost used in search.
    virtual int GetGapOpeningCost(void) const = 0;
    /// Returns gap extension cost used in search.
    virtual int GetGapExtensionCost(void) const = 0;
    /// Returns match reward, for blastn search only.
    virtual int GetMatchReward(void) const = 0;
    /// Returns mismatch penalty, for blastn search only.
    virtual int GetMismatchPenalty(void) const = 0; 
    /// Returns pattern string, for PHI BLAST search only.
    virtual string GetPHIPattern(void) const = 0;
    /// Returns filtering option string.
    virtual string GetFilterString(void) const = 0;
    /// Returns matrix name.
    virtual string GetMatrixName(void) const = 0;
    /// Returns a 256x256 ASCII-alphabet matrix, needed for formatting.
    virtual CBlastFormattingMatrix* GetMatrix(void) const = 0;
    virtual CConstRef<objects::CSeq_loc> GetQuerySeqLoc(void) const  = 0;
    /// Returns list of mask locations for a given query.
    virtual const TMaskedQueryRegions & GetMaskLocations() const = 0;
    /// Returns number of database sequences.
    virtual Int8 GetDbNumSeqs(void) const = 0;
    /// Returns database length
    virtual Int8 GetDbLength(void) const = 0;
    /// Returns length adjustment for a given query.
    virtual int GetLengthAdjustment(int num) const = 0;
    /// Returns effective search space for a given query.
    virtual Int8 GetEffectiveSearchSpace(int num) const = 0;
    /// Returns Karlin-Altschul Lambda parameter for a given query.
    virtual double GetLambda(int num) const = 0;
    /// Returns Karlin-Altschul K parameter for a given query.
    virtual double GetKappa(int num) const = 0;
    /// Returns Karlin-Altschul H parameter for a given query.
    virtual double GetEntropy(int num) const = 0;
    /// Returns scope .
    virtual CRef<objects::CScope> GetScope(void) const = 0;
    /// Returns  a vector continaing set of alignments found for a
    /// given query
    virtual CConstRef<objects::CSeq_align_set> GetAlignmentSet(int num) const = 0;
    ///master genetic code
    virtual int GetQueryGeneticCode(void) const = 0;
    ///slave genetic code
    virtual int GetDbGeneticCode(void) const = 0;
    /// Get error messages
    virtual string GetMessages(int num) const = 0;

    virtual list<string> GetSubjectIds(void) const = 0;
    virtual bool IsBl2seq(void) const = 0;
    virtual int GetNumOfSearchResults(void) const = 0;
    virtual bool CanGetTaxInfo(void) const = 0;
    virtual bool IsGappedSearch(void) const = 0;
    virtual int GetCompositionBasedStats(void) const = 0;
    virtual string GetBl2seqMode(void) const = 0;
    virtual bool IsIterativeSearch(void) const = 0;
    virtual string GetEntrezQuery(void) const = 0; 

};


/// Fills all fields in the XML BLAST v2 output object.
/// @param bxmlout XML BLAST v2 output object [in] [out]
/// @param data Data structure containing all information necessary to
///             produce a BLAST XML report.[in]
/// @param out_stream  output stream [out]
NCBI_XBLASTFORMAT_EXPORT
void BlastXML2_FormatReport(const IBlastXML2ReportData* data,
                            CNcbiOstream *out_stream);

NCBI_XBLASTFORMAT_EXPORT
void BlastXML2_FormatReport(const IBlastXML2ReportData* data, string file_name);

NCBI_XBLASTFORMAT_EXPORT
void BlastXML2_FormatError( int exit_code,
						   string err_msg,
                          CNcbiOstream *out_stream);

NCBI_XBLASTFORMAT_EXPORT
void
BlastJSON_FormatReport(const IBlastXML2ReportData* data, string file_name);

NCBI_XBLASTFORMAT_EXPORT
void
BlastJSON_FormatReport(const IBlastXML2ReportData* data, CNcbiOstream *out_stream );

/* @} */

END_NCBI_SCOPE

#endif /* ALGO_BLAST_FORMAT___BLASTXML2_FORMAT__HPP */
