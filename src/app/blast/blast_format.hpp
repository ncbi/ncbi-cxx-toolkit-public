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
* ===========================================================================*/

/*****************************************************************************

Author: Jason Papadopoulos

******************************************************************************/

/** @file blast_format.hpp
 * Produce formatted blast output
*/

#ifndef APP___BLAST_FORMAT__HPP
#define APP___BLAST_FORMAT__HPP

#include <corelib/ncbi_limits.hpp>
#include <algo/blast/api/sseqloc.hpp>
#include <algo/blast/api/setup_factory.hpp>
#include <algo/blast/api/uniform_search.hpp>
#include <algo/blast/api/blast_results.hpp>
#include <algo/blast/core/blast_seqsrc.h>
#include <objtools/blast_format/tabular.hpp>
#include <objtools/blast_format/showalign.hpp>
#include <objtools/blast_format/showdefline.hpp>
#include <objtools/blast_format/blastfmtutil.hpp>
#include <algo/blast/blastinput/cmdline_flags.hpp>
#include <algo/blast/blastinput/blast_input.hpp>

BEGIN_NCBI_SCOPE

// Wrapper class for formatting BLAST results
class CBlastFormat
{
public:
    /// The line length of pairwise blast output
    static const int kFormatLineLength = 65;
    static const string kNoHitsFound;

    /// Constructor
    /// @param program Blast program name ("blastn", "blastp", etc) [in]
    /// @param dbname Name of database to search ("" if none) [in]
    /// @param format_type Integer indication the type of output [in]
    /// @param db_is_aa true if database contains protein sequences [in]
    /// @param believe_query true if sequence ID's of query sequences
    ///                are to be parsed. If multiple queries are provieded,
    ///                their sequence ID's must be distinct [in]
    /// @param outfile Stream that will receive formatted output
    /// @param blast_input object containing query sequences read. Its lifetime
    ///                 must exceed that of this object, as this object doesn't
    ///                 own this pointer [in]
    /// @param asn_outfile Name of file to receive ASN.1 output
    /// @param num_summary The number of 1-line summaries at the top of
    ///                   the blast report (for output types that have
    ///                   1-line summaries) [in]
    /// @param matrix_name Name of protein score matrix (BLOSUM62 if
    ///                    empty, ignored for nucleotide formatting) [in]
    /// @param show_gi When printing database sequence identifiers, 
    ///                include the GI number if set to true; otherwise
    ///                database sequences only have an accession [in]
    /// @param is_html true if the output is to be in HTML format [in]
    /// @param qgencode Genetic code used to translate query sequences
    ///                 (if applicable) [in]
    /// @param dbgencode Genetic code used to translate database sequences
    ///                 (if applicable) [in]
    /// @param show_linked If the output format supports 1-line summaries,
    ///                    and the alignments have had HSP linking performed,
    ///                    append the number of hits in the linked set of
    ///                    the best alignment to each database sequence [in]
    ///
    CBlastFormat(const blast::CBlastOptions& opts, const string& dbname, 
                 int format_type, bool db_is_aa,
                 bool believe_query, CNcbiOstream& outfile,
                 blast::CBlastInput* blast_input,
                 int num_summary, 
                 int num_alignments, 
                 const char *matrix_name = BLAST_DEFAULT_MATRIX,
                 bool show_gi = false, 
                 bool is_html = false, 
                 int qgencode = BLAST_GENETIC_CODE,
                 int dbgencode = BLAST_GENETIC_CODE,
                 bool show_linked = false);

    /// Destructor
    ~CBlastFormat();

    /// Print the header of the blast report
    void PrintProlog();

    /// Print all alignment information for a single query sequence along with
    /// any errors or warnings (errors are deemed fatal)
    /// @param results Object containing alignments, mask regions, and
    ///                ancillary data to be output [in]
    /// @param scope The scope to use for retrieving sequence data
    ///              (must contain query and database sequences) [in]
    void PrintOneResultSet(const blast::CSearchResults& results,
                           objects::CScope& scope,
                           unsigned int itr_num =
                           numeric_limits<unsigned int>::max());

    /// Print the footer of the blast report
    /// @param options Options used for performing the blast search [in]
    ///
    void PrintEpilog(const blast::CBlastOptions& options);

private:
    int m_FormatType;           ///< Format type
    bool m_IsHTML;              ///< true if HTML output desired
    bool m_DbIsAA;              ///< true if database has protein sequences
    bool m_BelieveQuery;        ///< true if query sequence IDs are parsed
    CNcbiOstream& m_Outfile;    ///< stream to receive output
    int m_NumSummary;           ///< number of 1-line summaries
    int m_NumAlignments;        ///< number of database sequences to present alignments for.
    string m_Program;           ///< blast program
    string m_DbName;            ///< name of blast database
    int m_QueryGenCode;         ///< query genetic code
    int m_DbGenCode;            ///< database genetic code
    bool m_ShowGi;              ///< add GI number of database sequence IDs
    bool m_ShowLinkedSetSize;   ///< show size of linked set in 1-line summary
    bool m_IsDbAvailable;       ///< true if a database is available
    bool m_IsUngappedSearch;    ///< true if the search was ungapped

    /// True if a user-specified score matrix is required
    /// for formatting
    bool m_MatrixSet;

    /// Score matrix used by blast formatter to determine
    /// neighboring protein residues
    int *m_Matrix[CDisplaySeqalign::ePMatrixSize];

    /// internal representation of database information
    list<CBlastFormatUtil::SDbInfo> m_DbInfo;

    /// Queries are required for XML format, not owned by this class
    blast::CBlastInput* m_Queries;
    /// Accumulated results to display in XML format 
    blast::CSearchResultSet m_AccumulatedResults;
    

    /// Output the ancillary data for one query that was searched
    /// @param summary The ancillary data to report [in]
    /// @param options Options used for blast search [in]
    ///
    void x_PrintOneQueryFooter(const blast::CBlastAncillaryData& summary);

    /// Initialize the score matrix to be used for formatting
    /// (if applicable)
    /// @param matrix_name Name of score matrix. NULL defaults to
    ///                    BLOSUM62 [in]
    ///
    void x_FillScoreMatrix(const char *matrix_name = BLAST_DEFAULT_MATRIX);

    /// Initialize database statistics
    void x_FillDbInfo();
};

END_NCBI_SCOPE

#endif /* !APP___BLAST_FORMAT__HPP */

