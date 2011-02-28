/*  $Id$
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
 * Author:  Jian Ye
 */

/** @file blastfmtutil.hpp
 * BLAST formatter utilities.
 */

#ifndef ALGO_BLAST_FORMAT___BLASTFMTUTIL_HPP
#define ALGO_BLAST_FORMAT___BLASTFMTUTIL_HPP

#include <corelib/ncbistre.hpp>
#include <algo/blast/api/version.hpp>
#include <algo/blast/api/blast_results.hpp> // for CBlastAncillaryData
#include <objtools/align_format/align_format_util.hpp>

/** @addtogroup BlastFormatting
 *
 * @{
 */

/**setting up scope*/
BEGIN_NCBI_SCOPE

/** This class contains misc functions for displaying BLAST results. */

class NCBI_XBLASTFORMAT_EXPORT CBlastFormatUtil : public align_format::CAlignFormatUtil
{
public:
    /// Returns the version and release date, e.g. BLASTN 2.2.10 [Oct-19-2004]
    /// @param program Type of BLAST program [in]
    static string BlastGetVersion(const string program);

    ///Print out blast engine version
    ///@param program: name of blast program such as blastp, blastn
    ///@param html: in html format or not
    ///@param out: stream to ouput
    ///
    static void BlastPrintVersionInfo(const string program, bool html, 
                                      CNcbiOstream& out);

    ///Print out blast reference
    ///@param html: in html format or not
    ///@param line_len: length of each line desired
    ///@param out: stream to ouput
    ///@param publication Which publication to show reference for? [in]
    ///@param is_psiblast: is this reference for psiblast? [in]
    static void BlastPrintReference(bool html, size_t line_len, 
                                    CNcbiOstream& out, 
                                    blast::CReference::EPublication publication =
                                    blast::CReference::eGappedBlast,
                                    bool is_psiblast = false);


    static void PrintDbInformation(size_t line_len,
                                   string definition_line, 
                                   int nNumSeqs, 
                                   Uint8 nTotalLength,
                                   bool html,   
                                   bool with_links,
                                   CNcbiOstream& out);

    /** 
     * @brief Prints the PSSM in ASCII format (as in blastpgp's -Q option)
     * 
     * @param pssm pssm to print [in]
     * @param ancillary_data ancillary BLAST data to print [in]
     * @param out output stream to write output to [in]
     */
    static void PrintAsciiPssm(const objects::CPssmWithParameters& pssm,
                       CConstRef<blast::CBlastAncillaryData> ancillary_data,
                       CNcbiOstream& out);

    /*
     * @brief Create a CSeq_annot object from a CSeq_align_set.
     *
     * @input parm: alnset -- seq align set to be embedded in the
     * 						  new seq annot object
     * 				program -- blast program enum
     * 				db_name -- database name
     * @output parm : seq annot object
     */
    static CRef<objects::CSeq_annot> CreateSeqAnnotFromSeqAlignSet(
    				   const objects::CSeq_align_set & alnset,
    				   blast::EProgram program,
    				   const string & db_name);

};


/// 256x256 matrix used for calculating positives etc. during formatting.
/// @todo FIXME Should this be used for non-XML formatting? Currently the 
/// CDisplaySeqalign code uses a direct 2 dimensional array of integers, which 
/// is allocated, populated and freed manually inside different CDisplaySeqalign
/// methods.
class CBlastFormattingMatrix : public CNcbiMatrix<int> {
public:
    /// Constructor - allocates the matrix with appropriate size and populates
    /// with the values retrieved from a scoring matrix, passed in as a 
    /// 2-dimensional integer array.
    CBlastFormattingMatrix(int** data, unsigned int nrows, unsigned int ncols); 
};


END_NCBI_SCOPE

/* @} */

#endif /* ALGO_BLAST_FORMAT___BLASTFMTUTIL_HPP */
