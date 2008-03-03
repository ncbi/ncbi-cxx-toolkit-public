#ifndef OBJTOOLS_BLAST_FORMAT___BLASTFMTUTIL_HPP
#define OBJTOOLS_BLAST_FORMAT___BLASTFMTUTIL_HPP

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

/** @addtogroup BlastFormatting
 *
 * @{
 */

/**setting up scope*/
BEGIN_NCBI_SCOPE
USING_SCOPE (objects);

///blast related url

///class info
const static string kClassInfo = "class=\"info\"";

///entrez
// .ncbirc alias: ENTREZ
const string kEntrezUrl = "<a %shref=\"http://www.ncbi.nlm.nih.gov/entre\
z/query.fcgi?cmd=Retrieve&db=%s&list_uids=%d&dopt=%s&RID=%s&log$=%s%s&blast_rank=%d\" %s>";

///trace db
//.ncbirc alias: TRACE
const string kTraceUrl = "<a %shref=\"http://www.ncbi.nlm.nih.gov/Traces\
/trace.cgi?cmd=retrieve&dopt=fasta&val=%s&RID=%s\">";

///genome button
//.ncbirc alias: GENOME_BTN
const string kGenomeButton = "<table border=0 width=600 cellpadding=8>\
<tr valign=\"top\"><td><a href=\
\"http://www.ncbi.nlm.nih.gov/mapview/map_search.cgi?taxid=%d&RID=%s&CLIENT=\
%s&QUERY_NUMBER=%d\"><img border=0 src=\"html/GenomeView.gif\"></a></td>\
<td>Show positions of the BLAST hits in the %s genome \
using the Entrez Genomes MapViewer</td></tr></table><p>";

///unigene
// .ncbirc alias: UNIGEN
const string kUnigeneUrl = "<a href=\"http://www.ncbi.nlm.nih.gov/entrez/query.fcgi?\
db=%s&cmd=Display&dopt=%s_unigene&from_uid=%d&RID=%s&log$=unigene%s&blast_rank=%d\"><img border=0 h\
eight=16 width=16 src=\"images/U.gif\" alt=\"UniGene info\"></a>";

///structure
// .ncbirc alias: STRUCTURE_URL
const string kStructureUrl = "<a href=\"http://www.ncbi.nlm.nih.gov/St\
ructure/cblast/cblast.cgi?blast_RID=%s&blast_rep_gi=%d&hit=%d&blast_CD_RID=%s\
&blast_view=%s&hsp=0&taxname=%s&client=blast&log$=structure%s&blast_rank=%d\"><img border=0 height=16 width=\
16 src=\"http://www.ncbi.nlm.nih.gov/Structure/cblast/str_link.gif\" alt=\"Re\
lated structures\"></a>";

///structure overview
const string kStructure_Overview = "<a href=\"http://www.ncbi.nlm.nih.\
gov/Structure/cblast/cblast.cgi?blast_RID=%s&blast_rep_gi=%d&hit=%d&blast_CD_\
RID=%s&blast_view=%s&hsp=0&taxname=%s&client=blast\">Related Structures</a>";

///Geo
// .ncbirc alias: GEO
const string kGeoUrl =  "<a href=\"http://www.ncbi.nlm.nih.gov/entrez/\
query.fcgi?db=geo&term=%d[gi]&RID=%s&log$=geo%s&blast_rank=%d\"><img border=0 height=16 width=16 src=\
\"images/E.gif\" alt=\"Geo\"></a>";

///Gene
// .ncbirc alias: GENE
const string kGeneUrl = "<a href=\"http://www.ncbi.nlm.nih.gov/entrez/\
query.fcgi?db=gene&cmd=search&term=%d[%s]&RID=%s&log$=gene%s&blast_rank=%d\"><img border=0 height=16 width=16 \
src=\"images/G.gif\" alt=\"Gene info\"></a>";

///Bioassay for proteins
// .ncbirc alias: BIOASSAY_PROT
const string kBioAssayProtURL = "<a href=\"http://www.ncbi.nlm.nih.gov/entrez\
?db=pcassay&term=%d[ProteinTargetGI]&RID=%s&log$=pcassay%s&blast_rank=%d\"><img border=0 height=16 width=16 \
src=\"images/Bioassay.gif\" alt=\"PubChem BioAssay Info\"></a>";

///Bioassay for nucleotides
// .ncbirc alias: BIOASSAY_NUC
const string kBioAssayNucURL = "<a href=\"http://www.ncbi.nlm.nih.gov/entrez\
?db=pcassay&term=%d&RID=%s&log$=pcassay%s&blast_rank=%d\"><img border=0 height=16 width=16 \
src=\"images/Bioassay.gif\" alt=\"PubChem BioAssay Info\"></a>";

///mapviewer linkout
// .ncbirc alias: MAPVIEWER
/*const string kMapviwerUrl = "<a href=\"%s\"><img border=0 height=16 width=16 \
  src=\"images/M.gif\" alt=\"Genome view with mapviewer\"></a>";*/
const string kMapviwerUrl = "<a href=\"http://www.ncbi.nlm.nih.gov/mapview/map_search.cgi?direct=on&gbgi=%d&THE_BLAST_RID=%s&log$=map%s&blast_rank=%d\"><img border=0 height=16 width=16 \
src=\"images/M.gif\" alt=\"Genome view with mapviewer\"></a>";

///Sub-sequence
// .ncbirc alias: ENTREZ_SUBSEQ
const string kEntrezSubseqUrl = "<a href=\"http://www.ncbi.nlm.nih.\
gov/entrez/viewer.fcgi?val=%d&db=%s&from=%d&to=%d&view=gbwithparts&RID=%s\">";

///Bl2seq 
// .ncbirc alias: BL2SEQ
const string kBl2seqUrl = "<a href=\"http://www.ncbi.nlm.nih.gov/\
blast/bl2seq/wblast2.cgi?PROGRAM=tblastx&WORD=3&RID=%s&ONE=%s&TWO=%s\">Get \
TBLASTX alignments</a>";

// .ncbirc alias: GETSEQ_SUB_FRM_0
// .ncbirc alias: GETSEQ_SUB_FRM_1
const string k_GetSeqSubmitForm[] = {"<FORM  method=\"post\" \
action=\"http://www.ncbi.nlm.nih.gov:80/entrez/query.fcgi?SUBMIT=y\" \
name=\"%s%d\"><input type=button value=\"Get selected sequences\" \
onClick=\"finalSubmit(%d, 'getSeqAlignment%d', 'getSeqGi', '%s%d', %d)\"><input \
type=\"hidden\" name=\"db\" value=\"\"><input type=\"hidden\" name=\"term\" \
value=\"\"><input type=\"hidden\" name=\"doptcmdl\" value=\"docsum\"><input \
type=\"hidden\" name=\"cmd\" value=\"search\"></form>",
                                     
                                     "<FORM  method=\"POST\" \
action=\"http://www.ncbi.nlm.nih.gov/Traces/trace.cgi\" \
name=\"%s%d\"><input type=button value=\"Get selected sequences\" \
onClick=\"finalSubmit(%d, 'getSeqAlignment%d', 'getSeqGi', '%s%d', %d)\"><input \
type=\"hidden\" name=\"val\" value=\"\"><input \
type=\"hidden\" name=\"cmd\" value=\"retrieve\"></form>"
};

// .ncbirc alias: GETSEQ_SEL_FRM
// 'getSeqAlignment%d', 'getSeqGi')\"></form>";
const string k_GetSeqSelectForm = "<FORM><input \
type=\"button\" value=\"Select all\" onClick=\"handleCheckAll('select', \
'getSeqAlignment%d', 'getSeqGi')\"></form></td><td><FORM><input \
type=\"button\" value=\"Deselect all\" onClick=\"handleCheckAll('deselect', \
'getSeqAlignment%d', 'getSeqGi')\"></form>";

// .ncbirc alias: TREEVIEW_FRM
const  string k_GetTreeViewForm =  "<FORM  method=\"post\" \
action=\"http://www.ncbi.nlm.nih.gov/blast/treeview/blast_tree_view.cgi?request=page&rid=%s&queryID=%s&distmode=on\" \
name=\"tree%s%d\" target=\"trv%s\"> \
<input type=button value=\"Distance tree of results\" onClick=\"extractCheckedSeq('getSeqAlignment%d', 'getSeqGi', 'tree%s%d')\"> \
<input type=\"hidden\" name=\"sequenceSet\" value=\"\"><input type=\"hidden\" name=\"screenWidth\" value=\"\"></form>";

// .ncbirc alias: GENE_INFO
const string kGeneInfoUrl =
"http://www.ncbi.nlm.nih.gov/sites/entrez?db=gene&cmd=search&term=%d&RID=%s&log$=geneexplicit%s&blast_rank=%d";

// .ncbirc alias: TREEVIEW_CGI
const string kGetTreeViewCgi="http://www.ncbi.nlm.nih.gov/blast/treeview/blast_tree_view.cgi";
// .ncbirc alias: ENTREZ_QUERY_CGI
const string kEntrezQueryCgi = "http://www.ncbi.nlm.nih.gov/entrez/query.fcgi";
// .ncbirc alias: TRACE_CGI
const string kTraceCgi = "http://www.ncbi.nlm.nih.gov/Traces/trace.cgi";
// .ncbirc alias:  MAP_SEARCH_CGI
const string kMapSearchCgi = "http://www.ncbi.nlm.nih.gov/mapview/map_search.cgi";
// .ncbirc alias: CBLAST_CGI
const string kCBlastCgi = "http://www.ncbi.nlm.nih.gov/Structure/cblast/cblast.cgi";
// .ncbirc alias: ENTREZ_VIEWER_CGI
const string kEntrezViewerCgi = "http://www.ncbi.nlm.nih.gov/entrez/viewer.fcgi";
// .ncbirc alias: BL2SEQ_WBLAST_CGI
const string kBl2SeqWBlastCgi = "http://www.ncbi.nlm.nih.gov/blast/bl2seq/wblast2.cgi";
// .ncbirc alias: ENTREZ_SITES_CGI
const string kEntrezSitesCgi = "http://www.ncbi.nlm.nih.gov/sites/entrez";


/// list all static url patterns 
static 
pair <string,string> 
k_all_url_pairs[]={
  pair <string,string>  ("ENTREZ",  kEntrezUrl ),
  pair <string,string>  ("UNIGEN",  kUnigeneUrl),  
  pair <string,string>  ("GEO",    kGeoUrl),
  pair <string,string>  ("GENE",  kGeneUrl),
  pair <string,string>  ("GENE_INFO",  kGeneInfoUrl),
  pair <string,string>  ("BIOASSAY_PROT",  kBioAssayProtURL),
  pair <string,string>  ("BIOASSAY_NUC",  kBioAssayNucURL),
  pair <string,string>  ("ENTREZ_SUBSEQ",  kEntrezSubseqUrl),
  pair <string,string>  ("TRACE",  kTraceUrl),
  pair <string,string>  ("GENOME_BTN",  kGenomeButton),
  pair <string,string>  ("STRUCTURE_URL",  kStructureUrl),
  pair <string,string>  ("STRUCTURE_OVW",  kStructure_Overview),
  pair <string,string>  ("MAPVIEWER",  kMapviwerUrl),
  pair <string,string>  ("BL2SEQ",  kBl2seqUrl),
  pair <string,string>  ("GETSEQ_SUB_FRM_0",  k_GetSeqSubmitForm[0]),
  pair <string,string>  ("GETSEQ_SUB_FRM_1",  k_GetSeqSubmitForm[1]),
  pair <string,string>  ("GETSEQ_SEL_FRM",  k_GetSeqSelectForm),
  pair <string,string>  ("TREEVIEW_FRM",  k_GetTreeViewForm),
  // only cgi links
  pair <string,string>  ("TREEVIEW_CGI",  kGetTreeViewCgi),
  pair <string,string>  ("ENTREZ_QUERY_CGI",  kEntrezQueryCgi),
  pair <string,string>  ("ENTREZ_VIEWER_CGI",  kEntrezViewerCgi),
  pair <string,string>  ("TRACE_CGI",  kTraceCgi),
  pair <string,string>  ("MAP_SEARCH_CGI",  kMapSearchCgi),
  pair <string,string>  ("CBLAST_CGI",  kCBlastCgi),
  pair <string,string>  ("BL2SEQ_WBLAST_CGI",  kBl2SeqWBlastCgi),
  pair <string,string>  ("ENTREZ_SITES_CGI",  kEntrezSitesCgi)
};

#ifndef DIM
#define DIM(static_array) (sizeof(static_array)/sizeof(*static_array))
#endif

/// Type definition for a map of tags - URLs
typedef map<string, string> TUrlMap;

/// create map source of all static URL's using previously defined pairs
static 
TUrlMap
k_UrlMap((const TUrlMap::value_type*)&k_all_url_pairs[0], 
         (const TUrlMap::value_type*)k_all_url_pairs + DIM(k_all_url_pairs));


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

        /// Default constructor
        SDbInfo() {
            is_protein = true;
            name = definition = date = "Unknown";
            total_length = 0;
            number_seqs = 0;
            subset = false;
        }
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
    ///@param is_psiblast: is this reference for psiblast? [in]
    static void BlastPrintReference(bool html, size_t line_len, 
                                    CNcbiOstream& out, 
                                    blast::CReference::EPublication publication =
                                    blast::CReference::eGappedBlast,
                                    bool is_psiblast = false);

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
    static void PrintDbReport(const vector<SDbInfo>& dbinfo_list, 
                              size_t line_length, 
                              CNcbiOstream& out, 
                              bool top=false);
    
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
    /// @param html in html format or not [in]
    /// @param tabular Is this done for tabular formatting? [in]
    /// @param rid the RID to acknowledge (if not empty) [in]
    ///
    static void AcknowledgeBlastQuery(const CBioseq& cbs, size_t line_len,
                                      CNcbiOstream& out, bool believe_query,
                                      bool html, bool tabular=false,
                                      const string& rid = kEmptyStr);
    
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
    static void PruneSeqalign(const CSeq_align_set& source_aln, 
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
    ///@param for_alignment: is the URL generated for an alignment or a top defline?
    ///
    static string BuildUserUrl(const CBioseq::TId& ids, int taxid, string user_url,
                               string database, bool db_is_na, string rid,
                               int query_number, bool for_alignment);
    
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

    ///function for Filtering seqalign by expect value
    ///@param source_aln
    /// CSeq_align_set original seqalign
    ///@param evalueLow 
    /// double min expect value
    ///@param evalueHigh 
    /// double max expect value
    ///@return
    /// CRef<CSeq_align_set> - filtered seq align
    static CRef<CSeq_align_set> FilterSeqalignByEval(CSeq_align_set& source_aln,                                      
                                     double evalueLow,
                                     double evalueHigh);

    ///function for Limitting seqalign by hsps number
    ///(by default results are not cut off within the query)
    ///@param source_aln
    /// CSeq_align_set original seqalign
    ///@param maxAligns 
    /// double max number of alignments (per query)
    ///@param maxHsps 
    /// double max number of Hsps (for all qeuries)    
    ///@return
    /// CRef<CSeq_align_set> - filtered seq align
    static CRef<CSeq_align_set> LimitSeqalignByHsps(CSeq_align_set& source_aln,
                                                    int maxAligns,
                                                    int maxHsps);                                                    
    
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
                                      bool structure_linkout_as_group,
                                      bool for_alignment, int cur_align);

    static int GetMasterCoverage(const CSeq_align_set& alnset);
    ///retrieve URL from .ncbirc file combining host/port and format strings values.
    ///consult blastfmtutil.hpp
    ///@param url_name:  url name to retrieve
    ///@param index:   name index ( default: -1: )
    ///@return: URL format string from .ncbirc file or default as kNAME
    ///
    static string GetURLFromRegistry( const string url_name, int index = -1);
    ////get default value if there is problem with .ncbirc file or
    ////settings are not complete. return corresponding static value
    ///@param url_name: constant name to return .
    ///@param index:   name index ( default: -1: )
    ///@return: URL format string defined in blastfmtutil.hpp 
  static string GetURLDefault( const string url_name, int index = -1);
    ///release memory allocated for the registry object by GetURLFromRegistry
    ///
    static void ReleaseURLRegistry(void);
    static CNcbiRegistry *m_Reg;
    static bool   m_geturl_debug_flag;
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

/* @} */

#endif /* OBJTOOLS_BLAST_FORMAT___BLASTFMTUTIL_HPP */
