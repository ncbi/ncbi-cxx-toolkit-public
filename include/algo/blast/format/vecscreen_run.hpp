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

Author: Tom Madden

******************************************************************************/

/** @file vecscreen_run.hpp
 * 
*/

#ifndef VECSCREEN_RUN__HPP
#define VECSCREEN_RUN__HPP

#include <corelib/ncbi_limits.hpp>
#include <util/range.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <algo/blast/api/sseqloc.hpp>
#include <algo/blast/api/blast_results.hpp>
#include <objtools/align_format/align_format_util.hpp>
#include <objtools/align_format/vectorscreen.hpp>
#include <algo/blast/api/blast_options_handle.hpp>

BEGIN_NCBI_SCOPE
using namespace ncbi::align_format;
using namespace ncbi::objects;

class NCBI_XBLASTFORMAT_EXPORT CVecScreenVersion : public CVersionInfo {
    static const int kVecScreenMajorVersion = 2;
    static const int kVecScreenMinorVersion = 0;
    static const int kVecScreenPatchVersion = 0;
public:
    CVecScreenVersion()
        : CVersionInfo(kVecScreenMajorVersion,
                       kVecScreenMinorVersion,
                       kVecScreenPatchVersion) {}
};

#define kDefaultVectorDb "UniVec"

/// This class runs vecscreen
class NCBI_XBLASTFORMAT_EXPORT CVecscreenRun
{
public:

     /// Summary of hits.
     struct SVecscreenSummary {
        /// Seq-id of query.
        const CSeq_id* seqid; 
        /// range of match.
        CRange<TSeqPos> range;
        /// Categorizes strength of match.
        string match_type;
     };

     /// Constructor
     ///@param seq_loc sequence locations to screen.
     ///@param scope CScope used to fetch sequence on seq_loc
     ///@param db Database to screen with (UniVec is default).
     CVecscreenRun(CRef<CSeq_loc> seq_loc, CRef<CScope> scope, 
                   const string & db = string(kDefaultVectorDb));

     /// Destructor 
     ~CVecscreenRun() {delete m_Vecscreen;}

     /// Fetches summary list
     list<SVecscreenSummary> GetList() const;

     /// Fetches seqalign-set already processed by vecscreen.
     CRef<objects::CSeq_align_set> GetSeqalignSet() const;
     CRef<blast::CSearchResultSet> GetSearchResultSet() const;
     
     /// The Vecscreen formatter
     class NCBI_XBLASTFORMAT_EXPORT CFormatter {
     public:

         /// Controls the output formats supported by command line VecScreen
         enum EOutputFormat {
             eShowAlignments = 0,       ///< Show the alignments
             eShowIntervalsOnly = 1,    ///< Only show the contaminated intervals
             eBlastTab = 2,             ///< switch to a blast-tab-like fmt
             eJson = 3,                 ///< blast-tab values, but json formatted
             eAsnText = 4,              ///< entire seq-aligns in asn text
             eEndValue                  ///< Sentinel value, not an actual output format
         };
         typedef int TOutputFormat;

         CFormatter(CVecscreenRun& vs, 
                    CScope& scope,
                    TOutputFormat fmt = eShowAlignments, 
                    bool html_output = true)
             : m_Screener(vs), m_Scope(scope),
               m_Outfmt(fmt), m_HtmlOutput(html_output) {}

         /// Format the VecScreen results
         /// @param out stream to write the results to
         /// @param vs_opts VecScreen options
         void FormatResults(CNcbiOstream& out, 
                            CRef<blast::CBlastOptionsHandle> vs_opts);

     private:
         CVecscreenRun& m_Screener; ///< the vecscreen run instance
         CScope& m_Scope;           ///< from which we get the sequence data
         TOutputFormat m_Outfmt;    ///< the requested output format
         bool m_HtmlOutput;         ///< Whether HTML output is requested or not

         /// Prohibit copy constructor
         CFormatter(const CFormatter&);
         /// Prohibit assignment operator
         CFormatter & operator=(const CFormatter&);
     
        void x_GetIdsAndTitlesForSeqAlign(const objects::CSeq_align& align, 
                                        string& qid, string& qtitle, 
                                        string& sid, string& stitle);
     };

private:
     friend class CVecscreenRun::CFormatter;

     /// Runs the actual BLAST search
     /// @pre m_Queries is not empty
     /// @post m_RawBlastResults and m_Seqalign_set are not NULL
     void x_RunBlast();

     /// Seq-loc to screen
     CRef<CSeq_loc> m_SeqLoc;
     /// Scope used to fetch query.
     CRef<CScope> m_Scope;
     /// Database to use (UniVec is default).
     string m_DB;
     /// vecscreen instance for search.
     CVecscreen* m_Vecscreen;
     /// The queries to run VecScreen on
     CRef<blast::CBlastQueryVector> m_Queries;
     /// Processed Seq-align
     CRef<objects::CSeq_align_set> m_Seqalign_set;
     /// The raw  BLAST results
     CRef<blast::CSearchResultSet> m_RawBlastResults;


     /// Prohibit copy constructor
     CVecscreenRun(const CVecscreenRun&);
     /// Prohibit assignment operator
     CVecscreenRun & operator=(const CVecscreenRun&);
};

END_NCBI_SCOPE

#endif /* VECSCREEN_RUN__HPP */

