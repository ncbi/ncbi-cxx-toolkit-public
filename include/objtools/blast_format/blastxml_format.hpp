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

#ifndef OBJTOOLS_BLASTFORMAT___BLASTXML_FORMAT__HPP
#define OBJTOOLS_BLASTFORMAT___BLASTXML_FORMAT__HPP

#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/scope.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objtools/blast_format/showalign.hpp>
#include <objtools/blast_format/blastfmtutil.hpp>
#include <objects/blastxml/blastxml__.hpp>

#include <algo/blast/api/blast_aux.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup BlastFormatting
 *
 * @{
 */

/// Interface for filling the top layer of the XML report
class IBlastXMLReportData
{
public:
    /// Our virtual destructor
    virtual ~IBlastXMLReportData() {}
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
    /// Returns number of query sequences.
    virtual unsigned int GetNumQueries(void) const = 0;
    /// Returns list of mask locations for a given query.
    virtual const blast::TMaskedQueryRegions*
    GetMaskLocations(int query_index) const = 0;
    /// Returns number of database sequences.
    virtual int GetDbNumSeqs(void) const = 0;
    /// Returns database length
    virtual Int8 GetDbLength(void) const = 0;
    /// Returns length adjustment for a given query.
    virtual int GetLengthAdjustment(int query_index) const = 0;
    /// Returns effective search space for a given query.
    virtual Int8 GetEffectiveSearchSpace(int query_index) const = 0;
    /// Returns Karlin-Altschul Lambda parameter for a given query.
    virtual double GetLambda(int query_index) const = 0;
    /// Returns Karlin-Altschul K parameter for a given query.
    virtual double GetKappa(int query_index) const = 0;
    /// Returns Karlin-Altschul H parameter for a given query.
    virtual double GetEntropy(int query_index) const = 0;
    /// Returns a query Seq-loc for a given query index.
    virtual const objects::CSeq_loc* GetQuery(int query_index) const = 0;
    /// Returns scope for a given query.
    virtual objects::CScope* GetScope(int query_index) const = 0;
    /// Returns set of alignments found for a given query.
    virtual const objects::CSeq_align_set* 
    GetAlignment(int query_index) const = 0;
    /// Returns true if search was gapped, false otherwise.
    virtual bool GetGappedMode(void) const = 0;
    ///master genetic code
    virtual int GetMasterGeneticCode() const = 0;
    ///slave genetic code
    virtual int GetSlaveGeneticCode() const = 0;
    /// Get error messages, one element per query (e.g.: size of return value
    /// == GetNumQueries());
    virtual vector<string> GetMessages() const = 0;

};

/// Fills all fields in the XML BLAST output object.
/// @param bxmlout XML BLAST output object [in] [out]
/// @param data Data structure containing all information necessary to
///             produce a BLAST XML report. 
NCBI_XBLASTFORMAT_EXPORT
void BlastXML_FormatReport(CBlastOutput& bxmlout, 
                           const IBlastXMLReportData* data);

/* @} */

END_NCBI_SCOPE

#endif /* OBJTOOLS_BLASTFORMAT___BLASTXML_FORMAT__HPP */
