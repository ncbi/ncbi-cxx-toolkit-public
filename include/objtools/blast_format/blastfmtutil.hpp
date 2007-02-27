#ifndef BLASTFMTUTIL_HPP
#define BLASTFMTUTIL_HPP

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

#include <cgi/cgictx.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbireg.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/blastdb/Blast_def_line_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include  <objmgr/bioseq_handle.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objtools/alnmgr/alnvec.hpp>
#include <algo/blast/api/version.hpp>
#include <algo/blast/blastinput/cmdline_flags.hpp>
#include <util/math/matrix.hpp>

/**setting up scope*/
BEGIN_NCBI_SCOPE
USING_SCOPE (objects);

///blast related url

///class info
const static string kClassInfo = "class=\"info\"";

///entrez
const string kEntrezUrl = "<a %shref=\"http://www.ncbi.nlm.nih.gov/entre\
z/query.fcgi?cmd=Retrieve&db=%s&list_uids=%d&dopt=%s\" %s>";

///trace db
const string kTraceUrl = "<a %shref=\"http://www.ncbi.nlm.nih.gov/Traces\
/trace.cgi?cmd=retrieve&dopt=fasta&val=%s\">";

///genome button
const string kGenomeButton = "<table border=0 width=600 cellpadding=8>\
<tr valign=\"top\"><td><a href=\
\"http://www.ncbi.nlm.nih.gov/mapview/map_search.cgi?taxid=%d&RID=%s&CLIENT=\
%s&QUERY_NUMBER=%d\"><img border=0 src=\"html/GenomeView.gif\"></a></td>\
<td>Show positions of the BLAST hits in the %s genome \
using the Entrez Genomes MapViewer</td></tr></table><p>";

///unigene
const string kUnigeneUrl = "<a href=\"http://www.ncbi.nlm.nih.gov/entrez/query.fcgi?db=%s&cmd=Display&dopt=%s_unigene&from_uid=%d\"><img border=0 h\
eight=16 width=16 src=\"/blast/images/U.gif\" alt=\"UniGene info\"></a>";

///structure
const string kStructureUrl = "<a href=\"http://www.ncbi.nlm.nih.gov/St\
ructure/cblast/cblast.cgi?blast_RID=%s&blast_rep_gi=%d&hit=%d&blast_CD_RID=%s\
&blast_view=%s&hsp=0&taxname=%s&client=blast\"><img border=0 height=16 width=\
16 src=\"http://www.ncbi.nlm.nih.gov/Structure/cblast/str_link.gif\" alt=\"Re\
lated structures\"></a>";

///structure overview
const string kStructure_Overview = "<a href=\"http://www.ncbi.nlm.nih.\
gov/Structure/cblast/cblast.cgi?blast_RID=%s&blast_rep_gi=%d&hit=%d&blast_CD_\
RID=%s&blast_view=%s&hsp=0&taxname=%s&client=blast\">Related Structures</a>";

///Geo
const string kGeoUrl =  "<a href=\"http://www.ncbi.nlm.nih.gov/entrez/\
query.fcgi?db=geo&term=%d[gi]\"><img border=0 height=16 width=16 src=\"/blast\
/images/E.gif\" alt=\"Geo\"></a>";

///Gene
const string kGeneUrl = "<a href=\"http://www.ncbi.nlm.nih.gov/entrez/\
query.fcgi?db=gene&cmd=search&term=%d[%s]\"><img border=0 height=16 width=16 \
src=\"/blast/images/G.gif\" alt=\"Gene info\"></a>";

///mapviewer linkout
/*const string kMapviwerUrl = "<a href=\"%s\"><img border=0 height=16 width=16 \
  src=\"/blast/images/M.gif\" alt=\"Genome view with mapviewer\"></a>";*/
const string kMapviwerUrl = "<a href=\"http://www.ncbi.nlm.nih.gov/mapview/map_search.cgi?direct=on&gbgi=%d\"><img border=0 height=16 width=16 \
src=\"/blast/images/M.gif\" alt=\"Genome view with mapviewer\"></a>";

///Sub-sequence
const string kEntrezSubseqUrl = "<a href=\"http://www.ncbi.nlm.nih.\
gov/entrez/viewer.fcgi?val=%d&db=%s&from=%d&to=%d&view=gbwithparts\">";

///Bl2seq 
const string kBl2seqUrl = "<a href=\"http://www.ncbi.nlm.nih.gov/\
blast/bl2seq/wblast2.cgi?PROGRAM=tblastx&WORD=3&RID=%s&ONE=%s&TWO=%s\">Get \
TBLASTX alignments</a>";


/** This class contains misc functions for displaying BLAST results. */

class NCBI_XBLASTFORMAT_EXPORT CBlastFormatUtil 
{
   
public:
   
    ///Error info structure
    struct SBlastError {
        EDiagSev level;   
        string message;  
    };

    ///Blast database info
    struct SDbInfo {
        bool   is_protein;
        string name;
        string definition;
        string date;
        Int8   total_length;
        int    number_seqs;
        bool   subset;	
    };

    enum DbSortOrder {
        eNonGenomicFirst = 1,
        eGenomicFirst
    };

    enum HitOrder {
        eEvalue = 0,
        eHighestScore,
        eTotalScore,
        ePercentIdentity,
        eQueryCoverage
    };

    enum HspOrder {
        eHspEvalue = 0,
        eScore,
        eQueryStart,
        eHspPercentIdentity,
        eSubjectStart
    };

    ///Output blast errors
    ///@param error_return: list of errors to report
    ///@param error_post: post to stderr or not
    ///@param out: stream to ouput
    ///
    static void BlastPrintError(list<SBlastError>& error_return, 
                                bool error_post, CNcbiOstream& out);
    
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
    ///
    static void BlastPrintReference(bool html, size_t line_len, 
                                    CNcbiOstream& out, 
                                    blast::CReference::EPublication publication =
                                    blast::CReference::eGappedBlast);

    ///Print out misc information separated by "~"
    ///@param str:  input information
    ///@param line_len: length of each line desired
    ///@param out: stream to ouput
    ///
    static void PrintTildeSepLines(string str, size_t line_len, 
                                   CNcbiOstream& out);

    ///Print out blast database information
    ///@param dbinfo_list: database info list
    ///@param line_length: length of each line desired
    ///@param out: stream to ouput
    ///@param top Is this top or bottom part of the BLAST report?
    static void PrintDbReport(list<SDbInfo>& dbinfo_list, size_t line_length, 
                              CNcbiOstream& out, bool top=false);
    
    ///Print out kappa, lamda blast parameters
    ///@param lambda
    ///@param k
    ///@param h
    ///@param line_len: length of each line desired
    ///@param out: stream to ouput
    ///@param gapped: gapped alignment?
    ///@param c
    ///
    static void PrintKAParameters(float lambda, float k, float h,
                                  size_t line_len, CNcbiOstream& out, 
                                  bool gapped, float c = 0.0);

    /// Returns a full '|'-delimited Seq-id string for a Bioseq.
    /// @param cbs Bioseq object [in]
    /// @param believe_local_id Should local ids be parsed? [in]
    static string 
    GetSeqIdString(const CBioseq& cbs, bool believe_local_id=true);
    
    /// Returns a full description for a Bioseq, concatenating all available 
    /// titles.
    /// @param cbs Bioseq object [in]
    static string GetSeqDescrString(const CBioseq& cbs);

    ///Print out blast query info
    /// @param cbs bioseq of interest
    /// @param line_len length of each line desired
    /// @param out stream to ouput
    /// @param believe_query use user id or not
    /// @param html in html format or not
    /// @param tabular Is this done for tabular formatting? 
    ///
    static void AcknowledgeBlastQuery(const CBioseq& cbs, size_t line_len,
                                      CNcbiOstream& out, bool believe_query,
                                      bool html, bool tabular=false);
    
    ///Get blast defline
    ///@param handle: bioseq handle to extract blast defline from
    ///
    static CRef<CBlast_def_line_set> 
    GetBlastDefline (const CBioseq_Handle& handle);

    ///Get linkout membership
    ///@param bdl: blast defline to get linkout membership from
    ///@return the value representing the membership bits set
    ///
    static int GetLinkout(const CBlast_def_line& bdl);
    
    ///Get linkout membership for this bioseq, this id only
    ///@param handle: bioseq handle
    ///@param id: the id to be matched
    ///@return the value representing the membership bits set
    ///
    static int GetLinkout(const CBioseq_Handle& handle, const CSeq_id& id);	
    
    ///Extract score info from blast alingment
    ///@param aln: alignment to extract score info from
    ///@param score: place to extract the raw score to
    ///@param bits: place to extract the bit score to
    ///@param evalue: place to extract the e value to
    ///@param sum_n: place to extract the sum_n to
    ///@param num_ident: place to extract the num_ident to
    ///@param use_this_gi: place to extract use_this_gi to
    ///
    static void GetAlnScores(const CSeq_align& aln,
                             int& score, 
                             double& bits, 
                             double& evalue,
                             int& sum_n,
                             int& num_ident,
                             list<int>& use_this_gi);
    
    ///Extract score info from blast alingment
    /// Second version that fetches compositional adjustment integer
    ///@param aln: alignment to extract score info from
    ///@param score: place to extract the raw score to
    ///@param bits: place to extract the bit score to
    ///@param evalue: place to extract the e value to
    ///@param sum_n: place to extract the sum_n to
    ///@param num_ident: place to extract the num_ident to
    ///@param use_this_gi: place to extract use_this_gi to
    ///@param comp_adj_method: composition based statistics method [out]
    ///
    static void GetAlnScores(const CSeq_align& aln,
                             int& score, 
                             double& bits, 
                             double& evalue,
                             int& sum_n,
                             int& num_ident,
                             list<int>& use_this_gi,
                             int& comp_adj_method);

    ///Add the specified white space
    ///@param out: ostream to add white space
    ///@param number: the number of white spaces desired
    ///
    static void AddSpace(CNcbiOstream& out, int number);
    
    ///format evalue and bit_score 
    ///@param evalue: e value
    ///@param bit_score: bit score
    ///@param evalue_str: variable to store the formatted evalue
    ///@param bit_score_str: variable to store the formatted bit score
    ///
    static void GetScoreString(double evalue, 
                               double bit_score, 
                               double total_bit_score, 
                               string& evalue_str, 
                               string& bit_score_str,
                               string& total_bit_score_str);
    
    ///Fill new alignset containing the specified number of alignments with
    ///unique slave seqids.  Note no new seqaligns were created. It just 
    ///references the original seqalign
    ///@param source_aln: the original alnset
    ///@param new_aln: the new alnset
    ///@param num: the specified number
    ///
    static void PruneSeqalign(CSeq_align_set& source_aln, 
                              CSeq_align_set& new_aln,
                              unsigned int num = blast::kDfltArgNumAlignments);

    /// Count alignment length, number of gap openings and total number of gaps
    /// in a single alignment.
    /// @param salv Object representing one alignment (HSP) [in]
    /// @param align_length Total length of this alignment [out]
    /// @param num_gaps Total number of insertions and deletions in this 
    ///                 alignment [out]
    /// @param num_gap_opens Number of gap segments in the alignment [out]
    static void GetAlignLengths(CAlnVec& salv, int& align_length, 
                                int& num_gaps, int& num_gap_opens);

    /// If a Seq-align-set contains Seq-aligns with discontinuous type segments, 
    /// extract the underlying Seq-aligns and put them all in a flat 
    /// Seq-align-set.
    /// @param source Original Seq-align-set
    /// @param target Resulting Seq-align-set
    static void ExtractSeqalignSetFromDiscSegs(CSeq_align_set& target,
                                               const CSeq_align_set& source);

    ///Create denseseg representation for densediag seqalign
    ///@param aln: the input densediag seqalign
    ///@return: the new denseseg seqalign
    static CRef<CSeq_align> CreateDensegFromDendiag(const CSeq_align& aln);

    ///return the tax id for a seqid
    ///@param id: seq id
    ///@param scope: scope to fetch this sequence
    ///
    static int GetTaxidForSeqid(const CSeq_id& id, CScope& scope);
    
    ///return the frame for a given strand
    ///Note that start is zero bases.  It returns frame +/-(1-3).
    ///0 indicates error
    ///@param start: sequence start position
    ///@param strand: strand
    ///@param id: the seqid
    ///@return: the frame
    ///
    static int GetFrame (int start, ENa_strand strand, const CBioseq_Handle& handle); 

    ///return the comparison result: 1st >= 2nd => true, false otherwise
    ///@param info1
    ///@param info2
    ///@return: the result
    ///
    static bool SortHitByTotalScoreDescending(CRef<CSeq_align_set> const& info1,
                                    CRef<CSeq_align_set> const& info2);

    static bool 
    SortHitByMasterCoverageDescending(CRef<CSeq_align_set> const& info1,
                                     CRef<CSeq_align_set> const& info2);
    

    ///group hsp's with the same id togeter
    ///@param target: the result list
    ///@param source: the source list
    ///
    static void HspListToHitList(list< CRef<CSeq_align_set> >& target,
                                 const CSeq_align_set& source); 

    ///extract all nested hsp's into a list
    ///@param source: the source list
    ///@return the list of hsp's
    ///
    static CRef<CSeq_align_set> HitListToHspList(list< CRef<CSeq_align_set> >& source);

    ///return the custom url (such as mapview)
    ///@param ids: the id list
    ///@param taxid
    ///@param user_url: the custom url
    ///@param database
    ///@param db_is_na:  is db nucleotide?
    ///@param rid: blast rid
    ///@param query_number: the blast query number.
    ///
    static string BuildUserUrl(const CBioseq::TId& ids, int taxid, string user_url,
                               string database, bool db_is_na, string rid,
                               int query_number);
    
    ///transforms a string so that it becomes safe to be used as part of URL
    ///the function converts characters with special meaning (such as
    ///semicolon -- protocol separator) to escaped hexadecimal (%xx)
    ///@param src: the input url
    ///@return: the safe url
    ///
    static string MakeURLSafe(char* src);

    ///calculate the percent identity for a seqalign
    ///@param aln" the seqalign
    ///@param scope: scope to fetch sequences
    ///@do_translation: is this a translated nuc to nuc alignment?
    ///@return: the identity 
    ///
    static double GetPercentIdentity(const CSeq_align& aln, CScope& scope,
                                     bool do_translation);

    ///get the alignment length
    ///@param aln" the seqalign
    ///@do_translation: is this a translated nuc to nuc alignment?
    ///@return: the alignment length
    ///
    static int GetAlignmentLength(const CSeq_align& aln, bool do_translation);

    ///sort a list of seqalign set by alignment identity
    ///@param seqalign_hit_list: list to be sorted.
    ///@param do_translation: is this a translated nuc to nuc alignment?
    ///
    static void SortHitByPercentIdentityDescending(list< CRef<CSeq_align_set> >&
                                                   seqalign_hit_list,
                                                   bool do_translation);

    ///sorting function for sorting a list of seqalign set by descending identity
    ///@param info1: the first element 
    ///@param info2: the second element
    ///@return: info1 >= info2?
    ///
    static bool SortHitByPercentIdentityDescendingEx
        (const CRef<CSeq_align_set>& info1,
         const CRef<CSeq_align_set>& info2);
    
    ///sorting function for sorting a list of seqalign by descending identity
    ///@param info1: the first element 
    ///@param info2: the second element
    ///@return: info1 >= info2?
    ///
    static bool SortHspByPercentIdentityDescending 
    (const CRef<CSeq_align>& info1,
     const CRef<CSeq_align>& info2);
    
    ///sorting function for sorting a list of seqalign by ascending mater 
    ///start position
    ///@param info1: the first element 
    ///@param info2: the second element
    ///@return: info1 >= info2?
    ///
    static bool SortHspByMasterStartAscending(const CRef<CSeq_align>& info1,
                                              const CRef<CSeq_align>& info2);

    static bool SortHspBySubjectStartAscending(const CRef<CSeq_align>& info1,
                                               const CRef<CSeq_align>& info2);

    static bool SortHitByScoreDescending
    (const CRef<CSeq_align_set>& info1,
     const CRef<CSeq_align_set>& info2);
    

    static bool SortHspByScoreDescending(const CRef<CSeq_align>& info1,
                                         const CRef<CSeq_align>& info2);

    ///sorting function for sorting a list of seqalign set by ascending mater 
    ///start position
    ///@param info1: the first element 
    ///@param info2: the second element
    ///@return: info1 >= info2?
    ///
    static bool SortHitByMasterStartAscending(CRef<CSeq_align_set>& info1,
                                              CRef<CSeq_align_set>& info2);

    ///sort a list of seqalign set by molecular type
    ///@param seqalign_hit_list: list to be sorted.
    ///@param scope: scope to fetch sequence
    ///
    static void 
    SortHitByMolecularType(list< CRef<CSeq_align_set> >& seqalign_hit_list,
                           CScope& scope);
    
    ///actual sorting function for SortHitByMolecularType
    ///@param info1: the first element 
    ///@param info2: the second element
    ///@return: info1 >= info2?
    ///
    static bool SortHitByMolecularTypeEx (const CRef<CSeq_align_set>& info1,
                                          const CRef<CSeq_align_set>& info2);

    static void 
    SortHit(list< CRef<CSeq_align_set> >& seqalign_hit_list,
            bool do_translation, CScope& scope, int sort_method);
    
    static void SplitSeqalignByMolecularType(vector< CRef<CSeq_align_set> >& 
                                             target,
                                             int sort_method,
                                             const CSeq_align_set& source,
                                             CScope& scope);
    static CRef<CSeq_align_set> 
    SortSeqalignForSortableFormat(CCgiContext& ctx,
                               CScope& scope,
                               CSeq_align_set& aln_set,
                               bool nuc_to_nuc_translation,
                               int db_order,
                               int hit_order,
                               int hsp_order);
    
    static void BuildFormatQueryString (CCgiContext& ctx, 
                                       string& cgi_query);

    static void BuildFormatQueryString (CCgiContext& ctx, 
                                        map< string, string>& parameters_to_change,
                                        string& cgi_query);

    static bool IsMixedDatabase(const CSeq_align_set& alnset, 
                                CScope& scope); 

    static list<string> GetLinkoutUrl(int linkout, const CBioseq::TId& ids, 
                                      const string& rid, const string& db_name, 
                                      const int query_number, const int taxid,
                                      const string& cdd_rid, const string& entrez_term,
                                      bool is_na, string& user_url,
                                      const bool db_is_na, int first_gi,
                                      bool structure_linkout_as_group);

    static int GetMasterCoverage(const CSeq_align_set& alnset);
};

/// 256x256 matrix used for calculating positives etc. during formatting.
/// @todo FIXME Should this be used for non-XML formatting? Currently the 
/// CDisplaySeqalign code uses a direct 2 dimensional array of integers, which 
/// is allocated, populated and freed manually inside different CDisplaySeqalign
/// methods.
class NCBI_XBLASTFORMAT_EXPORT
CBlastFormattingMatrix : public CNcbiMatrix<int> {
public:
    /// Constructor - allocates the matrix with appropriate size and populates
    /// with the values retrieved from a scoring matrix, passed in as a 
    /// 2-dimensional integer array.
    CBlastFormattingMatrix(int** data, unsigned int nrows, unsigned int ncols); 
};

END_NCBI_SCOPE


/*===========================================
$Log$
Revision 1.27  2006/12/13 17:08:11  camacho
+ kBlastFmtDflt_NumDescriptions, kBlastFmtDflt_NumAlignments

Revision 1.26  2006/11/28 15:40:37  jianye
adding sorting seqalign functions

Revision 1.25  2006/07/26 22:05:48  ucko
SortHitByPercentIdentityDescendingEx, SortHspByPercentIdentityDescending:
take references to const CRefs to fix compilation under WorkShop.

Revision 1.24  2006/07/26 18:06:06  jianye
fix unigene linkout and added seqalign sort funstions

Revision 1.23  2006/07/17 14:50:42  jianye
adding sorting seqalign functions

Revision 1.22  2006/05/15 16:19:39  zaretska
Moved s_GetLinkout() function from shodefline.cpp to blastfmtutil.cpp

Revision 1.21  2006/04/05 17:41:02  jianye
change url for mouseover info

Revision 1.20  2006/03/01 18:38:28  jianye
added positives and frame info

Revision 1.19  2005/12/21 15:18:13  jcherry
Added export specifiers

Revision 1.18  2005/10/07 17:30:08  camacho
Doxygen fixes

Revision 1.17  2005/09/27 17:08:29  madden
Add second GetAlnScores method that also fetches comp_adj_method

Revision 1.16  2005/08/11 15:27:42  jianye
add gettaxidforseqid

Revision 1.15  2005/07/20 18:16:56  dondosha
Additions in API, needed for XML formatting

Revision 1.14  2005/05/16 18:00:08  dondosha
Changed total_length type to Int8

Revision 1.13  2005/05/11 16:22:47  dondosha
Added boolean argument to PrintDbReport, so it can be used for both top and bottom versions of database report

Revision 1.12  2005/05/02 17:32:30  dondosha
Added static method CreateDensegFromDendiag, previously static function in showalign.cpp

Revision 1.11  2005/03/14 20:09:41  dondosha
Added 2 static methods, needed for tabular output; added boolean argument to AcknowledgeBlastQuery for tabular version

Revision 1.10  2005/03/02 18:21:17  jianye
some style fix

Revision 1.9  2005/02/23 16:28:55  jianye
num_ident addition to getalnscore

Revision 1.8  2005/02/22 15:52:49  jianye
add pruneseqalign

Revision 1.7  2005/02/14 17:42:36  jianye
Added more url

Revision 1.6  2005/02/09 17:35:30  jianye
add common url

Revision 1.5  2005/02/02 16:31:57  jianye
int to size_t to get rid of compiler warning

Revision 1.4  2005/02/01 21:28:42  camacho
Doxygen fixes

Revision 1.3  2005/01/31 17:43:02  jianye
change unsigned int to size_t

Revision 1.2  2005/01/25 17:34:13  jianye
add NCBI_XBLASTFORMAT_EXPORT label

Revision 1.1  2005/01/24 16:41:43  jianye
Initial check in



*/
#endif
