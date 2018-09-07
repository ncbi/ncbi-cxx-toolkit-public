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
 * Produce formatted blast output for command line applications
*/

#ifndef ALGO_BLAST_FORMAT___BLAST_FORMAT__HPP
#define ALGO_BLAST_FORMAT___BLAST_FORMAT__HPP

#include <corelib/ncbi_limits.hpp>
#include <algo/blast/api/sseqloc.hpp>
#include <algo/blast/api/setup_factory.hpp>
#include <algo/blast/api/uniform_search.hpp>
#include <algo/blast/api/blast_results.hpp>
#include <algo/blast/igblast/igblast.hpp>
#include <algo/blast/api/psiblast_iteration.hpp>
#include <algo/blast/core/blast_seqsrc.h>
#include <objtools/align_format/tabular.hpp>
#include <objtools/align_format/showalign.hpp>
#include <objtools/align_format/showdefline.hpp>
#include <objtools/align_format/taxFormat.hpp>
#include <algo/blast/format/blastfmtutil.hpp>
#include <algo/blast/blastinput/blast_input.hpp>
#include <algo/blast/blastinput/blast_args.hpp>
#include <algo/blast/api/local_db_adapter.hpp>
#include <algo/blast/api/blast_seqinfosrc.hpp>
#include <algo/blast/format/sam.hpp>
#include <objects/blast/blast__.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(align_format);
class CCmdLineBlastXML2ReportData;

/// This class formats the BLAST results for command line applications
class NCBI_XBLASTFORMAT_EXPORT CBlastFormat : public CObject
{
public:

    ///igblast clone info
    struct SClone {
        string na;
        string chain_type;
        string aa;
        string v_gene;
        string d_gene;
        string j_gene;
        string seqid;
        double identity;
        string productive;
    };
        
    enum {
        /// The line length of pairwise blast output
        kFormatLineLength = 68,
        kMinTaxFormatLineLength = 100
    };

    ///Display options for blast_report
    enum DisplayOption {
        eDescriptions,
        eAlignments,
        eMetadata
    };
    /// Constructor
    /// @param opts BLAST options used in the search [in]
    /// @param db_adapter Adapter object representing a BLAST database or
    /// subject sequences [in]
    /// @param format_type enumeration that specifies the requested type of output [in]
    /// @param believe_query set to true if sequence IDs of query sequences
    ///                are to be parsed. If multiple queries are provieded,
    ///                their sequence ID's must be distinct. If you don't know
    ///                what this means, set to false [in]
    /// @param outfile Stream that will receive formatted output
    /// @param num_summary The number of 1-line summaries at the top of
    ///                   the blast report (for output types that have
    ///                   1-line summaries). Default value for the BLAST
    ///                   programs is align_format::kDfltArgNumDescriptions [in]
    /// @param num_alignments The number of alignments to display in the BLAST
    ///                 report. Default value for the BLAST programs is
    ///                 align_format::kDfltArgNumAlignments [in] 
    /// @param scope The scope to use for retrieving sequence data
    ///              (must contain query and database sequences) [in]
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
    /// @param use_sum_statistics Were sum statistics used in this search? [in]
    /// @param is_remote_search is this formatting the results of a remote
    /// search [in]
    /// @param dbfilt_algorithm BLAST database filtering algorithm ID (if
    /// applicable), value of -1 means not available [in]
    /// @param is_megablast true if megablast [in]
    /// @param is_indexed true if indexed search [in]
    /// @param custom_output_format custom output format specification for
    /// tabular/comma-separated value output format. An empty string implies to
    /// use the default value when applicable. [in]
    /// @param domain_db_adapter Domain database for Delta-blast [in]
    /// @param cmdline Actual command-line for SAM [in]
    /// @param subjectTag Identifier for set of subjects [in]
    CBlastFormat(const blast::CBlastOptions& opts, 
                 blast::CLocalDbAdapter& db_adapter,
                 blast::CFormattingArgs::EOutputFormat format_type, 
                 bool believe_query, CNcbiOstream& outfile,
                 int num_summary, 
                 int num_alignments,
                 CScope & scope,
                 const char *matrix_name = BLAST_DEFAULT_MATRIX,
                 bool show_gi = false, 
                 bool is_html = false, 
                 int qgencode = BLAST_GENETIC_CODE,
                 int dbgencode = BLAST_GENETIC_CODE,
                 bool use_sum_statistics = false,
                 bool is_remote_search = false,
                 int dbfilt_algorithm = -1,
                 const string& custom_output_format = kEmptyStr,
                 bool is_megablast = false,
                 bool is_indexed = false,
                 const blast::CIgBlastOptions * ig_opts = NULL,
                 const blast::CLocalDbAdapter* domain_db_adapter = NULL,
                 const string & cmdline = kEmptyStr,
		 const string& subjectTag = kEmptyStr);

    /// Constructor
    /// @param opts BLAST options used in the search [in]
    /// @param dbinfo_list Database info list, provided directly [in]
    /// @param format_type Integer indication the type of output [in]
    /// @param believe_query set to true if sequence IDs of query sequences
    ///                are to be parsed. If multiple queries are provieded,
    ///                their sequence ID's must be distinct. If you don't know
    ///                what this means, set to false [in]
    /// @param outfile Stream that will receive formatted output
    /// @param num_summary The number of 1-line summaries at the top of
    ///                   the blast report (for output types that have
    ///                   1-line summaries). Default value for the BLAST
    ///                   programs is align_format::kDfltArgNumDescriptions [in]
    /// @param num_alignments The number of alignments to display in the BLAST
    ///                 report. Default value for the BLAST programs is
    ///                 align_format::kDfltArgNumAlignments [in] 
    /// @param scope The scope to use for retrieving sequence data
    ///              (must contain query and database sequences) [in]
    /// @param show_gi When printing database sequence identifiers, 
    ///                include the GI number if set to true; otherwise
    ///                database sequences only have an accession [in]
    /// @param is_html true if the output is to be in HTML format [in]
    /// @param is_remote_search is this formatting the results of a remote
    /// search [in]
    /// @param custom_output_format custom output format specification for
    /// tabular/comma-separated value output format. An empty string implies to
    /// use the default value when applicable. [in]
    CBlastFormat(const blast::CBlastOptions& opts, 
                 const vector< CBlastFormatUtil::SDbInfo >& dbinfo_list,
                 blast::CFormattingArgs::EOutputFormat format_type, 
                 bool believe_query, CNcbiOstream& outfile,
                 int num_summary, 
                 int num_alignments,
                 CScope & scope,
                 bool show_gi = false, 
                 bool is_html = false, 
                 bool is_remote_search = false,
                 const string& custom_output_format = kEmptyStr,
                 bool is_vdb = false,
                 const string & cmdline = kEmptyStr);

    /// Class destructor
    ~CBlastFormat();

    /// Print the header of the blast report
    void PrintProlog();

    /// Get total length of the database
    Int8 GetDbTotalLength();

    /// Print all alignment information for a single query sequence along with
    /// any errors or warnings (errors are deemed fatal)
    /// @param results Object containing alignments, mask regions, and
    ///                ancillary data to be output [in]
    /// @param queries Query sequences (cached for XML formatting) [in]
    /// @param itr_num iteration number, if applicable, otherwise it should be
    /// numeric_limits<unsigned int>::max() [in]
    /// @param prev_seqids list of previously found Seq-ids, if applicable,
    /// otherwise it should be an empty list [in]
    /// @param is_deltablast_domain_result True if Delta-blast CDD search
    /// results are being formatted [in]
    void PrintOneResultSet(const blast::CSearchResults& results,
                           CConstRef<blast::CBlastQueryVector> queries,
                           unsigned int itr_num =
                           numeric_limits<unsigned int>::max(),
                           blast::CPsiBlastIterationState::TSeqIds prev_seqids =
                           blast::CPsiBlastIterationState::TSeqIds(),
                           bool is_deltablast_domain_result = false);

    /// Print all Ig alignment information for a single query sequence along with
    /// any errors or warnings (errors are deemed fatal)
    /// @param results Object containing alignments, mask regions, and
    ///                ancillary data to be output [in]
    /// @param queries Query sequences (cached for XML formatting) [in]
    /// @param result position index if index >= 0 [in]
    void PrintOneResultSet(blast::CIgBlastResults& results,
                           CConstRef<blast::CBlastQueryVector> queries, 
                           SClone& clone_info,
                           bool fill_clone_info,
                           bool print_airr_format_header,
                           int index = -1);

    /// Print all alignment information for aa PHI-BLAST run.
    /// any errors or warnings (errors are deemed fatal)
    /// @param result_set Object containing alignments, mask regions, and
    ///                ancillary data to be output [in]
    /// @param queries Query sequences (cached for XML formatting) [in]
    /// @param itr_num iteration number, if applicable, otherwise it should be
    /// numeric_limits<unsigned int>::max() [in]
    /// @param prev_seqids list of previously found Seq-ids, if applicable,
    /// otherwise it should be an empty list [in]
    void PrintPhiResult(const blast::CSearchResultSet& result_set,
                           CConstRef<blast::CBlastQueryVector> queries,
                           unsigned int itr_num =
                           numeric_limits<unsigned int>::max(),
                           blast::CPsiBlastIterationState::TSeqIds prev_seqids =
                           blast::CPsiBlastIterationState::TSeqIds());

    ///Print Metadata in json format or descriptions in html format or alignments in html format
    ///app.ini file with template info must be present
    /// @param results Object containing alignments, mask regions, and
    ///                ancillary data to be output [in]
    /// @param displayOption indicates what info to display
    void PrintReport(const blast::CSearchResults& results,              
                        CBlastFormat::DisplayOption displayOption);


    /// Writes out the query and results as an "archive" format
    /// @param queries Query factory to provide queries
    /// @param options_handle BLAST options
    /// @param results the search results
    /// @param num_iters number of iterations for psiblast
    void WriteArchive(blast::IQueryFactory& queries,
                           blast::CBlastOptionsHandle& options_handle,
                           const blast::CSearchResultSet& results,
                           unsigned int num_iters = 0,
                           const list<CRef<objects::CBlast4_error> >& msg = list<CRef<objects::CBlast4_error> > () );

    /// Writes out the pssm and results as an "archive" format
    /// @param pssm 	psiblast pssm
    /// @param options_handle BLAST options
    /// @param results the search results
    /// @param num_iter number of iterations for psiblast
    void WriteArchive(objects::CPssmWithParameters & pssm,
                      blast::CBlastOptionsHandle& options_handle,
                      const blast::CSearchResultSet& results,
                      unsigned int num_iters = 0,
                      const list<CRef<objects::CBlast4_error> > & msg = list<CRef<objects::CBlast4_error> >());

    /// Print the footer of the blast report
    /// @param options Options used for performing the blast search [in]
    ///
    void PrintEpilog(const blast::CBlastOptions& options);
    
    /// Resets the scope history for some output formats.
    ///
    /// This method tries to release memory no longer needed after
    /// each query sequence is processed.  For XML formatting, all
    /// data must be kept until the Epilog stage, so for XML this
    /// method has no effect.  Searches with multiple queries will
    /// therefore require more memory to produce XML format output
    /// than when using other output formats.
    void ResetScopeHistory();
    
    /// Set query range
    /// @param query_range query range [in]

    void SetQueryRange(const TSeqRange & query_range) { m_QueryRange = query_range; }

    /// Get query range
    TSeqRange GetQueryRange(void) { return m_QueryRange; }

    /// For use by XML2 only
    void SetBaseFile(string base) {m_BaseFile = base;}

    /// Set Alignment Length
    void SetLineLength(size_t len) {m_LineLength = len;}    
    void SetAlignSeqList(string alignSeqList) {m_AlignSeqList = alignSeqList;}
    
    

    static void PrintArchive(CRef<objects::CBlast4_archive> archive,
            					  CNcbiOstream& out);
private:
    /// Format type
    blast::CFormattingArgs::EOutputFormat m_FormatType;
    bool m_IsHTML;              ///< true if HTML output desired
    bool m_DbIsAA;              ///< true if database has protein sequences
    bool m_BelieveQuery;        ///< true if query sequence IDs are parsed
    CNcbiOstream& m_Outfile;    ///< stream to receive output
    int m_NumSummary;        ///< number of 1-line summaries
    int m_NumAlignments;        ///< number of database sequences to present alignments for.
    int m_HitlistSize;		///< number of database sequences to save results for
    string m_Program;           ///< blast program
    string m_DbName;            ///< name of blast database
    int m_QueryGenCode;         ///< query genetic code
    int m_DbGenCode;            ///< database genetic code
    bool m_ShowGi;              ///< add GI number of database sequence IDs
    /// If the output format supports 1-line summaries, the search is ungapped
    /// and the alignments have had HSP linking performed, append the number of
    /// hits in the linked set of the best alignment to each database sequence
    bool m_ShowLinkedSetSize;
    bool m_IsUngappedSearch;    ///< true if the search was ungapped
    const char* m_MatrixName;   ///< name of scoring matrix
    /** Scoring matrix instantiated from m_MatrixName */
    CNcbiMatrix<int> m_ScoringMatrix; 
    /** Scope containing query and subject sequences */
    CRef<CScope> m_Scope;       
    /** True if we are formatting for BLAST2Sequences */
    bool m_IsBl2Seq;            
    /// True if it is m_IsBl2Seq in dbscan mode
    bool m_IsDbScan;
    /// Tag for subject sequences (e.g., name of input file)
    string m_SubjectTag;
    /// True if this object is formatting the results of a remote search
    bool m_IsRemoteSearch;
    /// Used to count number of searches formatted. 
    int m_QueriesFormatted;
    
    bool m_Megablast;           ///< true if megablast was used.
    bool m_IndexedMegablast;    ///< true if indexed megablast was used.
    /// Used to retrieve subject sequence information
    CRef<blast::IBlastSeqInfoSrc> m_SeqInfoSrc;

    /// internal representation of database information
    vector<CBlastFormatUtil::SDbInfo> m_DbInfo;

    /// Queries are required for XML format only
    CRef<blast::CBlastQueryVector> m_AccumulatedQueries;
    /// Accumulated results to display in XML format 
    blast::CSearchResultSet m_AccumulatedResults;
    /// Modify format to accomodate results that do not
    /// contain KA stats such as bitscores and evalues. -RMH-
    bool m_DisableKAStats;
    /// The custom output format specification
    string m_CustomOutputFormatSpec;

    /// Structure for incremental XML BLAST output.
    CRef<SBlastXMLIncremental> m_BlastXMLIncremental;

    /// Used by Igblast formatting.
    CConstRef<blast::CIgBlastOptions> m_IgOptions;

    /// Information about DELTA-BLAST domain database
    vector<CBlastFormatUtil::SDbInfo> m_DomainDbInfo;

    /// BLAST options
    CConstRef<blast::CBlastOptions> m_Options;

    /// Flag indicate vdb search
    bool m_IsVdb;

    TSeqRange m_QueryRange;

    bool m_IsIterative;

    string m_BaseFile;
    int m_XMLFileCount;

    size_t m_LineLength;

    IOS_BASE::iostate m_OrigExceptionMask;

    /// Pointer to the SAM formatting object
    auto_ptr<CBlast_SAM_Formatter> m_SamFormatter;

    string m_Cmdline;

    /// If true, print long sequence ids (database|accession)
    bool m_LongSeqId;
    
    
    CShowBlastDefline::SDeflineTemplates *m_DeflineTemplates;
    CDisplaySeqalign::SAlignTemplates *m_AlignTemplates;
    string m_AlignSeqList;

    /// Output the ancillary data for one query that was searched
    /// @param summary The ancillary data to report [in]
    void x_PrintOneQueryFooter(const blast::CBlastAncillaryData& summary);

    /// Display the BLAST deflines in the traditional BLAST report
    /// @param aln_set alignments to display [in]
    /// @param itr_num iteration number, if applicable, otherwise it should be
    /// numeric_limits<unsigned int>::max() [in]
    /// @param prev_seqids list of previously found Seq-ids, if applicable,
    /// otherwise it should be an empty list [in]
    /// @param additional show additional aligment summary [in]
    /// @param index add result postition index to quick url link on score if index >=0 [in]
    void x_DisplayDeflines(CConstRef<CSeq_align_set> aln_set,
                           unsigned int itr_num,
                           blast::CPsiBlastIterationState::TSeqIds& prev_seqids,
                           int additional = 0,
                           int index = -1,
                           int defline_length = -1);

    /// Split the full alignment into two sets of alignments: one for those
    /// seen in the previous iteration and used to build the PSSM and the other
    /// for newly found sequences. Only applicable to PSI-BLAST.
    /// @param full_alignment results of the PSI-BLAST search [in]
    /// @param repeated_seqs object where the repeated alignments will be
    /// copied to [in|out]
    /// @param new_seqs object where the newly found sequence alignments will
    /// be copied to [in|out]
    /// @param prev_seqids list of previosly found seqids [in]
    void x_SplitSeqAlign(CConstRef<CSeq_align_set> full_alignment,
                 CSeq_align_set& repeated_seqs,
                 CSeq_align_set& new_seqs,
                 blast::CPsiBlastIterationState::TSeqIds& prev_seqids);

    /// Configure the CShowBlastDefline instance passed to it
    /// @param showdef CShowBlastDefline object to configure [in|out]
    /// @param skip_from skipping display from [in]
    /// @param skip_to skipping display to [in]
    /// @param index to add index (if >=0)to quick url link to score [in]
    /// @param int num_descriptions_to_show the number of descriptions to show,
    /// this controls whether the header in the descriptions section is shown
    /// or not: if its value is non-zero it's shown, otherwise it isn't [in]
    void x_ConfigCShowBlastDefline(align_format::CShowBlastDefline& showdef,
                                   int skip_from = -1,
                                   int skip_to = -1,
                                   int index = -1,
                                   int num_descriptions_to_show = -1);

    /// Prints XML and both species of ASN.1
    /// @param results Results for one query or Phi-blast iteration [in]
    /// @param queries Bioseqs to be formatted (for XML) [in]
    void x_PrintStructuredReport(const blast::CSearchResults& results,
                                 CConstRef<blast::CBlastQueryVector> queries);

   /// Prints Tabular report for one query
   /// @param results Results for one query or Phi-blast iteration [in]
   /// @param itr_num Iteration number for PSI-BLAST [in]
   void x_PrintTabularReport(const blast::CSearchResults& results,
                             unsigned int itr_num);

   /// Prints IgTabular report for one query
   /// @param results Results for one query or Phi-blast iteration [in]
   void x_PrintIgTabularReport(const blast::CIgBlastResults& results,
                               SClone& clone_info,
                               bool fill_clone_info);

    /// Prints AirrRearrangement format
   /// @param results Results for one query or Phi-blast iteration [in]
   void x_PrintAirrRearrangement(const blast::CIgBlastResults& results,
                                 SClone& clone_info,
                                 bool fill_clone_info,
                                 bool print_airr_format_header);
    

   /// Replace the query with its reversed-compliement
   /// @param results Ig Blast results [in, out]
   void x_ReverseQuery(blast::CIgBlastResults& results);

   /// Creates a bioseq to be able to print the acknowledgement for the
   /// subject bioseq when formatting bl2seq results
   /// @note this method contains a static variable that assumes the order of
   /// the calls to it to retrieve the proper subject sequence
   CConstRef<objects::CBioseq> x_CreateSubjectBioseq();

   /// Wrap the Seq-align-set to be printed in a Seq-annot (as the C toolkit
   /// binaries)
   /// @param alnset Alignment to wrap in a Seq-annot, must be non-NULL [in]
   CRef<objects::CSeq_annot> 
   x_WrapAlignmentInSeqAnnot(CConstRef<objects::CSeq_align_set> alnset,
                             const string& db_title) const;

   // Return true if the search is againist vdb databases
   bool x_IsVdbSearch()const;

   void x_PrintXML2Report(const blast::CSearchResults& results,
            			  CConstRef<blast::CBlastQueryVector> queries);

   void x_GenerateXML2MasterFile(void);
   void x_GenerateJSONMasterFile(void);
   void x_WriteXML2(CCmdLineBlastXML2ReportData & report_data);

   void x_InitSAMFormatter();
   void x_PrintTaxReport(const blast::CSearchResults& results);
   void x_InitDeflineTemplates(void);
   void x_InitAlignTemplates(void);
   void x_DisplayDeflinesWithTemplates(CConstRef<CSeq_align_set> aln_set);
   void x_SetAlignParameters(CDisplaySeqalign& cds);
   void x_DisplayAlignsWithTemplates(CConstRef<CSeq_align_set> aln_set,const blast::CSearchResults& results);
};

END_NCBI_SCOPE

#endif /* ALGO_BLAST_FORMAT___BLAST_FORMAT__HPP */

