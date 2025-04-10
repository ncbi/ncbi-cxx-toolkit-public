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
 */

#ifndef OBJTOOLS_ALIGN_FORMAT___ALIGN_FORMAT_UTIL_HPP
#define OBJTOOLS_ALIGN_FORMAT___ALIGN_FORMAT_UTIL_HPP

#include <corelib/ncbistre.hpp>
#include <corelib/ncbireg.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/blastdb/Blast_def_line_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objtools/alnmgr/alnvec.hpp>
#include <objtools/align_format/format_flags.hpp>
#include <util/math/matrix.hpp>
#include <algo/blast/core/blast_stat.h>
#include <objtools/align_format/ilinkoutdb.hpp>


#ifdef _MSC_VER
#define strcasecmp _stricmp
#define strdup _strdup
#  if _MSC_VER < 1900
#define snprintf _snprintf
#  endif
#endif

/**setting up scope*/
BEGIN_NCBI_SCOPE

class CCgiContext;

BEGIN_SCOPE(align_format)


///protein matrix define
enum {
    ePMatrixSize = 23       // number of amino acid for matrix
};

/// Number of ASCII characters for populating matrix columns
const int k_NumAsciiChar = 128;

/// Residues
NCBI_ALIGN_FORMAT_EXPORT
extern const char k_PSymbol[];

/** This class contains misc functions for displaying BLAST results. */

class NCBI_ALIGN_FORMAT_EXPORT CAlignFormatUtil 
{
public:
   
    /// The string containing the message that no hits were found
    static const char* kNoHitsFound;
    //Enum for mapping const strings
    enum EMapConstString {
        eMapToURL,
        eMapToHTML,
        eMapToString,
    };
    
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
        /// Filtering algorithm ID used in BLAST search
        string filt_algorithm_name;
        /// Filtering algorithm options used in BLAST search
        string filt_algorithm_options;

        /// Default constructor
        SDbInfo() {
            is_protein = true;
            name = definition = date = "Unknown";
            total_length = 0;
            number_seqs = 0;
            subset = false;
        }
    };

    ///Structure that holds information needed for creation seqID URL in descriptions
    /// and alignments
    struct SSeqURLInfo { 
        string user_url;        ///< user url TOOL_URL from .ncbirc
        string blastType;       ///< blast type refer to blobj->adm->trace->created_by
        bool isDbNa;            ///< bool indicating if the database is nucleotide or not
        string database;        ///< name of the database
        string rid;             ///< blast RID
        int queryNumber;        ///< the query number
        TGi gi;                 ///< gi to use
        string accession;       ///< accession
        int linkout;            ///< linkout flag
        int blast_rank;         ///< index of the current alignment
        bool isAlignLink;       ///< bool indicating if link is in alignment section
        bool new_win;           ///< bool indicating if click of the url will open a new window
        CRange<TSeqPos> seqRange;///< sequence range
        bool flip;              ///< flip sequence in case of opposite strands
        TTaxId taxid;           ///< taxid
        bool addCssInfo;        ///< bool indicating that css info should be added
        string segs;            ///< string containing align segments in the the following format seg1Start-seg1End,seg2Start-seg2End
        string resourcesUrl;    ///< URL(s) to other resources from .ncbirc
        bool useTemplates;      ///< bool indicating that templates should be used when contsructing links
        bool advancedView;      ///< bool indicating that advanced view design option should be used when contsructing links
        string seqUrl;          ///< sequence URL created
        string defline;         ///< sequence defline
        bool   hasTextSeqID;
        
        /// Constructor        
        SSeqURLInfo(string usurl,string bt, bool isnuc,string db, string rid_in,int qn, 
                    TGi gi_in,  string acc, int lnk, int blrk,bool alnLink, bool nw, CRange<TSeqPos> range = CRange<TSeqPos>(0,0),bool flp = false,
                    TTaxId txid = INVALID_TAX_ID,bool addCssInf = false,string seqSegs = "",string resUrl = "",bool useTmpl = false, bool advView = false) 
                    : user_url(usurl),blastType(bt), isDbNa(isnuc), database(db),rid(rid_in), 
                    queryNumber(qn), gi(gi_in), accession(acc), linkout(lnk),blast_rank(blrk),isAlignLink(alnLink),
                    new_win(nw),seqRange(range),flip(flp),taxid (txid),addCssInfo(addCssInf),segs(seqSegs),
                    resourcesUrl(resUrl),useTemplates(useTmpl),advancedView(advView){}

    };
    
    struct SLinkoutInfo {        
        string rid;
        string cdd_rid;
        string entrez_term;
        bool is_na;        
        string database;
        int query_number;        
        string user_url;
        string preComputedResID;

        bool structure_linkout_as_group;
        bool for_alignment;

        TTaxId taxid;        
        int cur_align;        
        string taxName;
        string gnl;
        CRange<TSeqPos> subjRange;
                
        string linkoutOrder;
        ILinkoutDB* linkoutdb;
        string mv_build_name;

        string giList;
        string labelList;
        TGi first_gi;
                
        string  queryID;        
        
        
        void Init(string rid_in, string cdd_rid_in, string entrez_term_in, bool is_na_in, 
                  string database_in, int query_number_in, string user_url_in, string preComputedResID_in, 
                  string linkoutOrder_in, 
                  bool structure_linkout_as_group_in = false, bool for_alignment_in = true) {
            rid = rid_in;  
            cdd_rid = cdd_rid_in;
            entrez_term = entrez_term_in; 
            is_na = is_na_in;
            database = database_in;            
            query_number = query_number_in;
            user_url = user_url_in;  
            preComputedResID = preComputedResID_in; 

            linkoutOrder = linkoutOrder_in;            
            //linkoutdb = linkoutdb_in; 
            //mv_build_name = mv_build_name_in;


            structure_linkout_as_group = structure_linkout_as_group_in;
            for_alignment = for_alignment_in;
        }

        void Init(string rid_in, string cdd_rid_in, string entrez_term_in, bool is_na_in, 
                  string database_in, int query_number_in, string user_url_in, string preComputedResID_in, 
                  string linkoutOrder_in, ILinkoutDB* linkoutdb_in, string mv_build_name_in,                  
                  bool structure_linkout_as_group_in = false, bool for_alignment_in = true) {

            Init(rid_in,cdd_rid_in,entrez_term_in,is_na_in, 
                  database_in,query_number_in, user_url_in,preComputedResID_in, 
                  linkoutOrder_in, 
                  structure_linkout_as_group_in, for_alignment_in);

            linkoutdb = linkoutdb_in; 
            mv_build_name = mv_build_name_in;
        }
    };

    ///Structure that holds information for all hits of one subject in Seq Align Set    
    struct SSeqAlignSetCalcParams {        
        //values used in descriptions display
        double evalue;                  //lowest evalue in Seq Align Set , displayed on the results page as 'Evalue', 
        double bit_score;               //Highest bit_score in Seq Align Set, displayed on the results page as 'Max Score'
        double total_bit_score;         //total bit_score for Seq Align Set, displayed on the results page as 'Total Score'
        int percent_coverage;           //percent coverage for Seq Align Set, displayed on the results page as 'Query coverage'
                                        //calulated as 100*master_covered_length/queryLength
        double percent_identity;           //highest percent identity in Seq Align Set, displayed on the results page as 'Max ident'
                                        //calulated as 100*match/align_length

        int hspNum;                     //hsp number, number of hits
        Int8 totalLen;                  //total alignment length

        int raw_score;                  //raw score, read from the 'score' in first align in Seq Aln Set, not used        
        list<TGi> use_this_gi;          //Limit formatting by these GI's, read from the first align in Seq Aln Set        
        list<string> use_this_seq;      //Limit formatting by these seqids, read from the first align in Seq Aln Set        
        int sum_n;                      //sum_n in score block , read from the first align in Seq Aln Set        

        int master_covered_length;      //total query length covered by alignment - calculated, used calculate percent_coverage

        int match;                      //number of matches in the alignment with highest percent identity,used to calulate percent_identity
        int align_length;               //length of the alignment with highest percent identity,used to calulate percent_identity
        
        CConstRef<objects::CSeq_id> id; //subject seq id               
        CRange<TSeqPos> subjRange;      //total subject sequence range- calculated
        bool flip;					    //indicates opposite strands in the first seq align	   
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

    enum CustomLinkType {
        eLinkTypeDefault = 0,
        eLinkTypeMapViewer = (1 << 0),
        eLinkTypeSeqViewer = (1 << 1),
        eDownLoadSeq = (1 << 2),
        eLinkTypeGenLinks = (1 << 3),
        eLinkTypeTraceLinks = (1 << 4),
        eLinkTypeSRALinks = (1 << 5),
        eLinkTypeSNPLinks = (1 << 6),
        eLinkTypeGSFastaLinks = (1 << 7)
    };

    ///db type
    enum DbType {
        eDbGi = 0,
        eDbGeneral,
        eDbTypeNotSet
    };

    //Formatting flag for adding spaces
    enum SpacesFormatingFlag {        
        eSpacePosToCenter = (1 << 0),       ///place the param in the center of the string
        eSpacePosAtLineStart = (1 << 1),    ///add spaces at the begining of the string
        eSpacePosAtLineEnd = (1 << 2),      ///add spaces at the end of the string  
        eAddEOLAtLineStart = (1 << 3),      ///add EOL at the beginning of the string  
        eAddEOLAtLineEnd =  (1 << 4)        ///add EOL at the end of the string  
    };


    ///Output blast errors
    ///@param error_return: list of errors to report
    ///@param error_post: post to stderr or not
    ///@param out: stream to ouput
    ///
    static void BlastPrintError(list<SBlastError>& error_return, 
                                bool error_post, CNcbiOstream& out);

    ///Print out misc information separated by "~"
    ///@param str:  input information
    ///@param line_len: length of each line desired
    ///@param out: stream to ouput
    ///
    static void PrintTildeSepLines(string str, size_t line_len, 
                                   CNcbiOstream& out);

    /// Fills one BLAST dbinfo structure.
    /// Intended for use in bl2seq mode with multiple subject sequences.
    /// database title set to "User specified sequence set"
    /// @param retval return vector [in/out]
    /// @param is_protein are these databases protein? [in]
    /// @param numSeqs number of sequecnes in set [in]
    /// @param numLetters size of the set [in]
    /// @param tag Hint to user about subject sequences [in]
    static void FillScanModeBlastDbInfo(vector<SDbInfo>& retval,
                           bool is_protein, int numSeqs, Int8 numLetters, string& tag);

    /// Retrieve BLAST database information for presentation in BLAST report
    /// @param dbname space-separated list of BLAST database names [in]
    /// @param is_protein are these databases protein? [in]
    /// @param dbfilt_algorithm BLAST database filtering algorithm ID (if
    /// applicable), use -1 if not applicable [in]
    /// @param is_remote is this a remote BLAST search? [in]
    static void GetBlastDbInfo(vector<SDbInfo>& retval,
                               const string& blastdb_names, bool is_protein,
                               int dbfilt_algorithm,
                               bool is_remote = false);

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
    ///@param line_len length of each line desired
    ///@param out stream to ouput
    ///@param gapped gapped alignment?
    ///@param gbp Gumbel parameters
    static void PrintKAParameters(double lambda, double k, double h,
                                  size_t line_len, CNcbiOstream& out, 
                                  bool gapped, const Blast_GumbelBlk *gbp=NULL);

    /// Returns a full '|'-delimited Seq-id string for a Bioseq.
    /// @param cbs Bioseq object [in]
    /// @param believe_local_id Should local ids be parsed? [in]
    static string 
    GetSeqIdString(const objects::CBioseq& cbs, bool believe_local_id=true);
    
    /// Returns a full '|'-delimited Seq-id string for a  a list of seq-ids.
    /// @param ids lsit of seq-ids [in]
    /// @param believe_local_id Should local ids be parsed? [in]
    static string
    GetSeqIdString(const list<CRef<objects::CSeq_id> > & ids, bool believe_local_id);

    /// Returns a full description for a Bioseq, concatenating all available 
    /// titles.
    /// @param cbs Bioseq object [in]
    static string GetSeqDescrString(const objects::CBioseq& cbs);

    ///Print out blast query info
    /// @param cbs bioseq of interest
    /// @param line_len length of each line desired
    /// @param out stream to ouput
    /// @param believe_query use user id or not
    /// @param html in html format or not [in]
    /// @param tabular Is this done for tabular formatting? [in]
    /// @param rid the RID to acknowledge (if not empty) [in]
    ///
    static void AcknowledgeBlastQuery(const objects::CBioseq& cbs, size_t line_len,
                                      CNcbiOstream& out, bool believe_query,
                                      bool html, bool tabular=false,
                                      const string& rid = kEmptyStr);

    /// Print out blast subject info
    /// @param cbs bioseq of interest
    /// @param line_len length of each line desired
    /// @param out stream to ouput
    /// @param believe_query use user id or not
    /// @param html in html format or not [in]
    /// @param tabular Is this done for tabular formatting? [in]
    ///
    static void AcknowledgeBlastSubject(const objects::CBioseq& cbs, size_t line_len,
                                        CNcbiOstream& out, bool believe_query,
                                        bool html, bool tabular=false);

    /// Retrieve a scoring matrix for the provided matrix name
    /// @return the requested matrix (indexed using ASCII characters) or an empty
    /// matrix if matrix_name is invalid or can't be found.
    static void GetAsciiProteinMatrix(const char* matrix_name,
                                 CNcbiMatrix<int>& retval);
private:
    static void x_AcknowledgeBlastSequence(const objects::CBioseq& cbs, 
                                           size_t line_len,
                                           CNcbiOstream& out,
                                           bool believe_query,
                                           bool html, 
                                           const string& label,
                                           bool tabular /* = false */,
                                           const string& rid /* = kEmptyStr*/);
public:

    /// Prints out PHI-BLAST info for header (or footer)
    /// @param num_patterns number of times pattern appears in query [in]
    /// @param pattern the pattern used [in]
    /// @param prob probability of pattern [in]
    /// @param offsets vector of pattern offsets in query [in]
    /// @param out stream to ouput [in]
    static void PrintPhiInfo(int num_patterns, const string& pattern,
                                    double prob,
                                    vector<int>& offsets,
                                    CNcbiOstream& out);

    ///Extract score info from blast alingment
    ///@param aln: alignment to extract score info from
    ///@param score: place to extract the raw score to
    ///@param bits: place to extract the bit score to
    ///@param evalue: place to extract the e value to
    ///@param sum_n: place to extract the sum_n to
    ///@param num_ident: place to extract the num_ident to
    ///@param use_this_gi: place to extract use_this_gi to
    ///
    static void GetAlnScores(const objects::CSeq_align& aln,
                             int& score, 
                             double& bits, 
                             double& evalue,
                             int& sum_n,
                             int& num_ident,
                             list<TGi>& use_this_gi);

    ///Extract score info from blast alingment
    ///@param aln: alignment to extract score info from
    ///@param score: place to extract the raw score to
    ///@param bits: place to extract the bit score to
    ///@param evalue: place to extract the e value to
    ///@param sum_n: place to extract the sum_n to
    ///@param num_ident: place to extract the num_ident to
    ///@param use_this_seqid: place to extract use_this_seqid to
    ///
    static void GetAlnScores(const objects::CSeq_align& aln,
                                    int& score, 
                                    double& bits, 
                                    double& evalue,
                                    int& sum_n,
                                    int& num_ident,
                                    list<string>& use_this_seq);
    
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
    static void GetAlnScores(const objects::CSeq_align& aln,
                             int& score, 
                             double& bits, 
                             double& evalue,
                             int& sum_n,
                             int& num_ident,
                             list<TGi>& use_this_gi,
                             int& comp_adj_method);

    ///Extract score info from blast alingment
    /// Second version that fetches compositional adjustment integer
    ///@param aln: alignment to extract score info from
    ///@param score: place to extract the raw score to
    ///@param bits: place to extract the bit score to
    ///@param evalue: place to extract the e value to
    ///@param sum_n: place to extract the sum_n to
    ///@param num_ident: place to extract the num_ident to
    ///@param use_this_seq: place to extract use_this_seq to
    ///@param comp_adj_method: composition based statistics method [out]
    ///
    static void GetAlnScores(const objects::CSeq_align& aln,
                                    int& score, 
                                    double& bits, 
                                    double& evalue,
                                    int& sum_n,
                                    int& num_ident,
                                    list<string>& use_this_seq,
                                    int& comp_adj_method);

    ///Extract use_this_gi info from blast alingment
    ///@param aln: alignment to extract score info from
    ///@param use_this_gi: place to extract use_this_gi to
    static void GetUseThisSequence(const objects::CSeq_align& aln,list<TGi>& use_this_gi);

    ///Extract use_this_seq info from blast alingment
    ///@param aln: alignment to extract score info from
    ///@param use_this_seq: place to extract use_this_seq to
    static void GetUseThisSequence(const objects::CSeq_align& aln,list<string>& use_this_seq);

    ///Matches text seqID or gi with the list of seqIds or gis
    ///@param cur_gi: gi to match
    ///@param seqID: CSeq_id to extract label info to match
    ///@param use_this_seq: list<string> containg gi:sssssss or seqid:sssssssss
    ///@param isGiList: bool= true if use_this_seq conatins gi list
    ///@ret: bool=true if the match is found
    static bool MatchSeqInSeqList(TGi cur_gi, CRef<objects::CSeq_id> &seqID, list<string> &use_this_seq,bool *isGiList = NULL);

    ///Matches string of seqIDs (gis or text seqID)    
    ///@param alnSeqID: CSeq_id to extract label info to match
    ///@param use_this_seq: list<string> containg gi:sssssss or seqid:sssssssss
    ///@param seqList: string contaning comma separated seqIds
    ///@ret: bool=true if the match is found
    static bool MatchSeqInSeqList(CConstRef<objects::CSeq_id> &alnSeqID, list<string> &use_this_seq,vector <string> &seqList);
    
    static bool MatchSeqInUseThisSeqList(list<string> &use_this_seq, string textSeqIDToMatch);

    ///Check if use_this_seq conatins gi list
    ///@param use_this_seq: list<string> containg gi:sssssss or seqid:sssssssss
    ///@ret: bool= true if use_this_seq conatins gi list
    static bool IsGiList(list<string> &use_this_seq);

    ///Convert if string gi list to TGi list
    ///@param use_this_seq: list<string> containg gi:sssssss
    ///@ret: list<TGi> containin sssssss
    static list<TGi> StringGiToNumGiList(list<string> &use_this_seq);

    ///Add the specified white space
    ///@param out: ostream to add white space
    ///@param number: the number of white spaces desired
    ///
    static void AddSpace(CNcbiOstream& out, size_t number);

    ///Return ID for GNL label
    ///@param dtg: dbtag to build label from
    static string GetGnlID(const objects::CDbtag& dtg);

    ///Return a label for an ID
    /// Tries to recreate behavior of GetLabel before a change that 
    /// prepends "ti|" to trace IDs
    ///@param id CSeqId: to build label from
    ///@param with_version bool: include version to the label
    static string GetLabel(CConstRef<objects::CSeq_id> id, bool with_version = false);
    
    ///format evalue and bit_score 
    ///@param evalue: e value
    ///@param bit_score: bit score
    ///@param total_bit_score: total bit score(??)
    ///@param raw_score: raw score (e.g., BLOSUM score)
    ///@param evalue_str: variable to store the formatted evalue
    ///@param bit_score_str: variable to store the formatted bit score
    ///@param raw_score_str: variable to store the formatted raw score
    ///
    static void GetScoreString(double evalue, 
                               double bit_score, 
                               double total_bit_score, 
                               int raw_score,
                               string& evalue_str, 
                               string& bit_score_str,
                               string& total_bit_score_str,
                               string& raw_score_str);
    
    ///Fill new alignset containing the specified number of alignments with
    ///unique slave seqids.  Note no new seqaligns were created. It just 
    ///references the original seqalign
    ///@param source_aln: the original alnset
    ///@param new_aln: the new alnset
    ///@param num: the specified number
    ///@return actual number of subject sequences
    static void PruneSeqalign(const objects::CSeq_align_set& source_aln, 
                              objects::CSeq_align_set& new_aln,
                              unsigned int num = static_cast<unsigned int>(kDfltArgNumAlignments));

    ///Calculate number of subject sequnces in alignment limitted by num
    ///@param source_aln: the original alnset    
    ///@param num: the specified number of subj  sequences to consider
    ///@return actual number of subject seqs in alignment
    static unsigned int GetSubjectsNumber(const objects::CSeq_align_set& source_aln,                                      
                                      unsigned int num);

    ///Fill new alignset containing the specified number of alignments 
    ///plus the rest of alignments for the last subget seq
    ///unique slave seqids.  Note no new seqaligns were created. It just 
    ///references the original seqalign
    ///
    ///@param source_aln: the original alnset
    ///@param new_aln: the new alnset
    ///@param num: the specified number
    ///
    static void PruneSeqalignAll(const objects::CSeq_align_set& source_aln, 
                                     objects::CSeq_align_set& new_aln,
                                     unsigned int number);

    /// Count alignment length, number of gap openings and total number of gaps
    /// in a single alignment.
    /// @param salv Object representing one alignment (HSP) [in]
    /// @param align_length Total length of this alignment [out]
    /// @param num_gaps Total number of insertions and deletions in this 
    ///                 alignment [out]
    /// @param num_gap_opens Number of gap segments in the alignment [out]
    static void GetAlignLengths(objects::CAlnVec& salv, int& align_length, 
                                int& num_gaps, int& num_gap_opens);

    /// If a Seq-align-set contains Seq-aligns with discontinuous type segments, 
    /// extract the underlying Seq-aligns and put them all in a flat 
    /// Seq-align-set.
    /// @param source Original Seq-align-set
    /// @param target Resulting Seq-align-set
    static void ExtractSeqalignSetFromDiscSegs(objects::CSeq_align_set& target,
                                               const objects::CSeq_align_set& source);

    ///Create denseseg representation for densediag seqalign
    ///@param aln: the input densediag seqalign
    ///@return: the new denseseg seqalign
    static CRef<objects::CSeq_align> CreateDensegFromDendiag(const objects::CSeq_align& aln);

    ///return the tax id for a seqid
    ///@param id: seq id
    ///@param scope: scope to fetch this sequence
    ///
    static TTaxId GetTaxidForSeqid(const objects::CSeq_id& id, objects::CScope& scope);
    
    ///return the frame for a given strand
    ///Note that start is zero bases.  It returns frame +/-(1-3).
    ///0 indicates error
    ///@param start: sequence start position
    ///@param strand: strand
    ///@param id: the seqid
    ///@return: the frame
    ///
    static int GetFrame (int start, objects::ENa_strand strand, const objects::CBioseq_Handle& handle); 

    ///return the comparison result: 1st >= 2nd => true, false otherwise
    ///@param info1
    ///@param info2
    ///@return: the result
    ///
    static bool SortHitByTotalScoreDescending(CRef<objects::CSeq_align_set> const& info1,
                                    CRef<objects::CSeq_align_set> const& info2);

    static bool 
    SortHitByMasterCoverageDescending(CRef<objects::CSeq_align_set> const& info1,
                                     CRef<objects::CSeq_align_set> const& info2);
    

    ///group hsp's with the same id togeter
    ///@param target: the result list
    ///@param source: the source list
    ///
    static void HspListToHitList(list< CRef<objects::CSeq_align_set> >& target,
                                 const objects::CSeq_align_set& source); 

    ///extract all nested hsp's into a list
    ///@param source: the source list
    ///@return the list of hsp's
    ///
    static CRef<objects::CSeq_align_set> HitListToHspList(list< CRef<objects::CSeq_align_set> >& source);

    ///extract seq_align_set coreesponding to seqid list
    ///@param all_aln_set: CSeq_align_set source/target list
    ///@param alignSeqList: string of seqIds separated by comma
    ///
    static void ExtractSeqAlignForSeqList(CRef<objects::CSeq_align_set> &all_aln_set, string alignSeqList);

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
    static string BuildUserUrl(const objects::CBioseq::TId& ids, TTaxId taxid, string user_url,
                               string database, bool db_is_na, string rid,
                               int query_number, bool for_alignment);

    ///return the SRA (Short Read Archive) URL
    ///@param ids: the id list
    ///@param user_url: the URL of SRA cgi
    ///@return newly constructed SRA URL pointing to the identified spot
    ///
    static string BuildSRAUrl(const objects::CBioseq::TId& ids, string user_url);
    
 
    ///calculate the percent identity for a seqalign
    ///@param aln" the seqalign
    ///@param scope: scope to fetch sequences
    ///@do_translation: is this a translated nuc to nuc alignment?
    ///@return: the identity 
    ///
    static double GetPercentIdentity(const objects::CSeq_align& aln, objects::CScope& scope,
                                     bool do_translation);

    ///get the alignment length
    ///@param aln" the seqalign
    ///@do_translation: is this a translated nuc to nuc alignment?
    ///@return: the alignment length
    ///
    static int GetAlignmentLength(const objects::CSeq_align& aln, bool do_translation);

    ///sort a list of seqalign set by alignment identity
    ///@param seqalign_hit_list: list to be sorted.
    ///@param do_translation: is this a translated nuc to nuc alignment?
    ///
    static void SortHitByPercentIdentityDescending(list< CRef<objects::CSeq_align_set> >&
                                                   seqalign_hit_list,
                                                   bool do_translation);

    ///sorting function for sorting a list of seqalign set by descending identity
    ///@param info1: the first element 
    ///@param info2: the second element
    ///@return: info1 >= info2?
    ///
    static bool SortHitByPercentIdentityDescendingEx
        (const CRef<objects::CSeq_align_set>& info1,
         const CRef<objects::CSeq_align_set>& info2);
    
    ///sorting function for sorting a list of seqalign by descending identity
    ///@param info1: the first element 
    ///@param info2: the second element
    ///@return: info1 >= info2?
    ///
    static bool SortHspByPercentIdentityDescending 
    (const CRef<objects::CSeq_align>& info1,
     const CRef<objects::CSeq_align>& info2);
    
    ///sorting function for sorting a list of seqalign by ascending mater 
    ///start position
    ///@param info1: the first element 
    ///@param info2: the second element
    ///@return: info1 >= info2?
    ///
    static bool SortHspByMasterStartAscending(const CRef<objects::CSeq_align>& info1,
                                              const CRef<objects::CSeq_align>& info2);

    static bool SortHspBySubjectStartAscending(const CRef<objects::CSeq_align>& info1,
                                               const CRef<objects::CSeq_align>& info2);

    static bool SortHitByScoreDescending
    (const CRef<objects::CSeq_align_set>& info1,
     const CRef<objects::CSeq_align_set>& info2);
    

    static bool SortHspByScoreDescending(const CRef<objects::CSeq_align>& info1,
                                         const CRef<objects::CSeq_align>& info2);

    ///sorting function for sorting a list of seqalign set by ascending mater 
    ///start position
    ///@param info1: the first element 
    ///@param info2: the second element
    ///@return: info1 >= info2?
    ///
    static bool SortHitByMasterStartAscending(CRef<objects::CSeq_align_set>& info1,
                                              CRef<objects::CSeq_align_set>& info2);

    ///sort a list of seqalign set by molecular type
    ///@param seqalign_hit_list: list to be sorted.
    ///@param scope: scope to fetch sequence
    ///
    static void 
    SortHitByMolecularType(list< CRef<objects::CSeq_align_set> >& seqalign_hit_list,
                           objects::CScope& scope, ILinkoutDB* linkoutdb,
                           const string& mv_build_name);
    
    ///actual sorting function for SortHitByMolecularType
    ///@param info1: the first element 
    ///@param info2: the second element
    ///@return: info1 >= info2?
    ///
    //static bool SortHitByMolecularTypeEx (const CRef<objects::CSeq_align_set>& info1,
    //                                      const CRef<objects::CSeq_align_set>& info2);

    static void 
    SortHit(list< CRef<objects::CSeq_align_set> >& seqalign_hit_list,
            bool do_translation, objects::CScope& scope, int sort_method,
            ILinkoutDB* linkoutdb, const string& mv_build_name);
    
    static void SplitSeqalignByMolecularType(vector< CRef<objects::CSeq_align_set> >& 
                                             target,
                                             int sort_method,
                                             const objects::CSeq_align_set& source,
                                             objects::CScope& scope,
                                             ILinkoutDB* linkoutdb,
                                             const string& mv_build_name);
    static CRef<objects::CSeq_align_set> 
    SortSeqalignForSortableFormat(CCgiContext& ctx,
                               objects::CScope& scope,
                               objects::CSeq_align_set& aln_set,
                               bool nuc_to_nuc_translation,
                               int db_order,
                               int hit_order,
                               int hsp_order,
                               ILinkoutDB* linkoutdb,
                               const string& mv_build_name);

    static CRef<objects::CSeq_align_set> 
        SortSeqalignForSortableFormat(objects::CSeq_align_set& aln_set,
                               bool nuc_to_nuc_translation,
                               int hit_order,
                               int hsp_order);

    static list< CRef<objects::CSeq_align_set> >
        SortOneSeqalignForSortableFormat(const objects::CSeq_align_set& source,                                                                                                 
                                      bool nuc_to_nuc_translation,                                             
                                      int hit_sort,
                                      int hsp_sort);

	/// function for calculating  percent match for an alignment.	
	///@param numerator
	/// int numerator in percent identity calculation.
	///@param denominator
	/// int denominator in percent identity calculation.
	static int GetPercentMatch(int numerator, int denominator);
    static double GetPercentIdentity(int numerator, int denominator);
                                            

    ///function for Filtering seqalign by expect value
    ///@param source_aln
    /// CSeq_align_set original seqalign
    ///@param evalueLow 
    /// double min expect value
    ///@param evalueHigh 
    /// double max expect value
    ///@return
    /// CRef<CSeq_align_set> - filtered seq align
    static CRef<objects::CSeq_align_set> FilterSeqalignByEval(objects::CSeq_align_set& source_aln,                                      
                                     double evalueLow,
                                     double evalueHigh);

	///function for Filtering seqalign by percent identity
    ///@param source_aln
    /// CSeq_align_set original seqalign
    ///@param percentIdentLow
    /// double min percent identity
    ///@param percentIdentHigh 
    /// double max percent identity
    ///@return
    /// CRef<CSeq_align_set> - filtered seq align
    static CRef<objects::CSeq_align_set> FilterSeqalignByPercentIdent(objects::CSeq_align_set& source_aln,
                                                                      double percentIdentLow,
                                                                      double percentIdentHigh);

    ///function for Filtering seqalign by expect value and percent identity
    ///@param source_aln
    /// CSeq_align_set original seqalign
    ///@param evalueLow 
    /// double min expect value
    ///@param evalueHigh 
    /// double max expect value
    ///@param percentIdentLow
    /// double min percent identity
    ///@param percentIdentHigh 
    /// double max percent identity
    ///@return
    /// CRef<CSeq_align_set> - filtered seq align
	static CRef<objects::CSeq_align_set> FilterSeqalignByScoreParams(objects::CSeq_align_set& source_aln,
	                                                                 double evalueLow,
	                                                                 double evalueHigh,
	                                                                 double percentIdentLow,
	                                                                 double percentIdentHigh);


    static CRef<objects::CSeq_align_set> FilterSeqalignByScoreParams(objects::CSeq_align_set& source_aln,
                                                                    double evalueLow,
                                                                    double evalueHigh,
                                                                    double percentIdentLow,
                                                                    double percentIdentHigh,
                                                                    int queryCoverageLow,
                                                                    int queryCoverageHigh);
    ///function for Filtering seqalign by specific subjects
    ///@param source_aln
    /// CSeq_align_set original seqalign
    ///@param seqList 
    /// vector of strings with seqIDs    
    ///@return
    /// CRef<CSeq_align_set> - filtered seq align
    static CRef<objects::CSeq_align_set> FilterSeqalignBySeqList(objects::CSeq_align_set& source_aln,
                                                               vector <string> &seqList);

    ///function to remove sequences of accesionType from use_this_seq list    
    ///@param use_this_seq
    /// list <string> of seqIDs
    ///@param accesionType
    /// CSeq_id::EAccessionInfo accession type to check    
    ///@return
    /// bool true if list changed
    static bool RemoveSeqsOfAccessionTypeFromSeqInUse(list<string> &use_this_seq, objects::CSeq_id::EAccessionInfo accesionType);

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
    static CRef<objects::CSeq_align_set> LimitSeqalignByHsps(objects::CSeq_align_set& source_aln,
                                                    int maxAligns,
                                                    int maxHsps); 

    ///function for extracting seqalign for the query
    ///@param source_aln
    /// CSeq_align_set original seqalign
    ///@param queryNumber 
    /// int query number ,starts from 1, 0 means return all queries    
    ///@return
    /// CRef<CSeq_align_set> - seq align set for queryNumber, if invalid queryNumber return empty  CSeq_align_set
    static CRef<objects::CSeq_align_set> ExtractQuerySeqAlign(CRef<objects::CSeq_align_set>& source_aln,
                                                     int queryNumber);
    
    static void BuildFormatQueryString (CCgiContext& ctx, 
                                       string& cgi_query);

    static void BuildFormatQueryString (CCgiContext& ctx, 
                                        map< string, string>& parameters_to_change,
                                        string& cgi_query);

    static bool IsMixedDatabase(const objects::CSeq_align_set& alnset, 
                                objects::CScope& scope, ILinkoutDB* linkoutdb,
                                const string& mv_build_name); 
    static bool IsMixedDatabase(CCgiContext& ctx);

    
    ///Get the list of urls for linkouts
    ///@param linkout: the membership value
    ///@param ids: CBioseq::TId object    
    ///@param rid: RID
    ///@param cdd_rid: CDD RID
    ///@param entrez_term: entrez query term
    ///@param is_na: is this sequence nucleotide or not
    ///@param first_gi: first gi in the list (used to contsruct structure url)
    ///@param structure_linkout_as_group: bool used to contsruct structure url
    ///@param for_alignment: bool indicating if link is located in alignment section
    ///@param int cur_align: int current alignment/description number
    ///@param bool textLink: bool indicating that if true link will be presented as text, otherwise as image
    ///@return list of string containing all linkout urls for one seq 
    static list<string> GetLinkoutUrl(int linkout, 
                                      const objects::CBioseq::TId& ids, 
                                      const string& rid, 
                                      const string& cdd_rid, 
                                      const string& entrez_term,
                                      bool is_na, 
                                      TGi first_gi,
                                      bool structure_linkout_as_group,
                                      bool for_alignment, 
                                      int cur_align,
                                      string preComputedResID);
                                      
    
    ///Create map that holds all linkouts for the list of blast deflines and corresponding seqIDs
    ///@param bdl: list of CRef<CBlast_def_line>
    ///@param linkout_map: map that holds linkouts and corresponding CBioseq::TId for the whole list  of blast deflines  
    ///
    static void GetBdlLinkoutInfo(const list< CRef< objects::CBlast_def_line > > &bdl,
                                  map<int, vector < objects::CBioseq::TId > > &linkout_map,
                                  ILinkoutDB* linkoutdb,
                                  const string& mv_build_name);

    ///Create map that holds all linkouts for one seqID
    ///@param cur_id: CBioseq::TId
    ///@param linkout_map: map that holds linkouts and corresponding CBioseq::TId for the whole list  of blast deflines  
    ///
    static void GetBdlLinkoutInfo(objects::CBioseq::TId& cur_id,
                                  map<int, vector <objects::CBioseq::TId > > &linkout_map,
                                  ILinkoutDB* linkoutdb, 
                                  const string& mv_build_name);

    ///Get linkout membership for for the list of blast deflines
    ///@param bdl: list of CRef<CBlast_def_line>    
    ///@param rid: blast rid
    ///@param cdd_rid: blast cdd_rid
    ///@param entrez_term: entrez_term for building url
    ///@param is_na: bool indication if query is nucleotide
    ///@param first_gi: first gi in the list (used to contsruct structure url)
    ///@param structure_linkout_as_group: bool used to contsruct structure url
    ///@param for_alignment: bool indicating tif link is locted in alignment section
    ///@param int cur_align: int current alignment/description number
    ///@param linkoutOrder: string of letters separated by comma specifing linkout order like "G,U,M,E,S,B"
    ///@param taxid: int taxid
    ///@param database: database name
    ///@param query_number: query_number
    ///@param user_url: url defined as TOOL_URL for blast_type in .ncbirc
    ///@return list of string containing all linkout urls for all of the seqs in the list of blast deflines
    ///
    static list<string> GetFullLinkoutUrl(const list< CRef< objects::CBlast_def_line > > &bdl,                                             
                                                 const string& rid,
                                                 const string& cdd_rid, 
                                                 const string& entrez_term,
                                                 bool is_na,                                                            
                                                 bool structure_linkout_as_group,
                                                 bool for_alignment, 
                                                 int cur_align,
                                                 string& linkoutOrder,
                                                 TTaxId taxid,
                                                 string &database,
                                                 int query_number,                                                 
                                                 string &user_url,
                                                 string &preComputedResID,
                                                 ILinkoutDB* linkoutdb,
                                                 const string& mv_build_name);

    ///Get linkout membership for one seqID
    ///@param cur_id: CBioseq::TId seqID
    ///@param rid: blast rid
    ///@param cdd_rid: blast cdd_rid
    ///@param entrez_term: entrez_term for building url
    ///@param is_na: bool indication if query is nucleotide
    ///@param first_gi: first gi in the list (used to contsruct structure url)
    ///@param structure_linkout_as_group: bool used to contsruct structure url
    ///@param for_alignment: bool indicating tif link is locted in alignment section
    ///@param int cur_align: int current alignment/description number
    ///@param linkoutOrder: string of letters separated by comma specifing linkout order like "G,U,M,E,S,B"
    ///@param taxid: int taxid
    ///@param database: database name
    ///@param query_number: query_number
    ///@param user_url: url defined as TOOL_URL for blast_type in .ncbirc
    ///@return list of string containing all linkout urls for all of the seqs in the list of blast deflines
    ///
    static list<string> GetFullLinkoutUrl(objects::CBioseq::TId& cur_id,                                             
                                                 const string& rid,
                                                 const string& cdd_rid, 
                                                 const string& entrez_term,
                                                 bool is_na,                                                                                                   
                                                 bool structure_linkout_as_group,
                                                 bool for_alignment, 
                                                 int cur_align,
                                                 string& linkoutOrder,
                                                 TTaxId taxid,
                                                 string &database,
                                                 int query_number,                                                 
                                                 string &user_url,
                                                 string &preComputedResID,
                                                 ILinkoutDB* linkoutdb,
                                                 const string& mv_build_name,
                                                 bool getIdentProteins);
    static list<string> GetFullLinkoutUrl(const list< CRef< objects::CBlast_def_line > > &bdl, SLinkoutInfo &linkoutInfo);
    static list<string> GetFullLinkoutUrl(objects::CBioseq::TId& cur_id,SLinkoutInfo &linkoutInfo,bool getIdentProteins);
    static int GetSeqLinkoutInfo(objects::CBioseq::TId& cur_id,                                    
                                    ILinkoutDB** linkoutdb, 
                                    const string& mv_build_name,                                    
                                    TGi gi = INVALID_GI);
    static int GetMasterCoverage(const objects::CSeq_align_set& alnset);
	static CRange<TSeqPos> GetSeqAlignCoverageParams(const objects::CSeq_align_set& alnset,int *masterCoverage,bool *flip);
												

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

    ///Replace template tags by real data
    ///@param inpString: string containing template data
    ///@param tmplParamName:string with template tag name
    ///@param templParamVal: int value that replaces template
    ///@return:string containing template data replaced by real data
    ///
    ///<@tmplParamName@> is replaced by templParamVal
    static string MapTemplate(string inpString,string tmplParamName,Int8 templParamVal);
    
    ///Replace template tags by real data
    ///@param inpString: string containing template data
    ///@param tmplParamName:string with template tag name
    ///@param templParamVal: string value that replaces template
    ///@return:string containing template data replaced by real data
    ///
    ///<@tmplParamName@> is replaced by templParamVal
    static string MapTemplate(string inpString,string tmplParamName,string templParamVal);
        
    ///Replace template tags by real data and calculate and add spaces dependent on maxParamLength and spacesFormatFlag
    ///@param inpString: string containing template data
    ///@param tmplParamName:string with template tag name
    ///@param templParamVal: string value that replaces template
    ///@param maxParamLength: unsigned int maxParamLength
    ///@param spacesFormatFlag: int formatting flag
    ///@return:string containing template data replaced by real data
    ///
    ///<@tmplParamName@> is replaced by templParamVal
    static string MapSpaceTemplate(string inpString,string tmplParamName,string templParamVal, unsigned int maxParamLength, int spacesFormatFlag = eSpacePosAtLineEnd);
    
    //Maps tag_name to const string containing html
    static string  MapTagToHTML( const string tag_name);    
    //Maps tag_name to const string containing url adrres, html code or string
    static string  MapTagToConstString( const string tag_name, EMapConstString flag = eMapToURL);
    
    ///Calculate the number of spaces and add them to paramVal
    ///@param string: input parameter value
    ///@param size_t: max length for the string that holds parameter
    ///@param int: additional fomatting after adding spaces
    ///@param  string: the position of spaces and additional formatting
    ///@return:string containing paramVal and spaces place appropriately
    static string AddSpaces(string paramVal, size_t maxParamLength, int spacesFormatFlag = eSpacePosToCenter);


    static string GetProtocol(void);

    static void InitConfig();

    static string MapProtocol(string url_link);

    ///Create URL for seqid
    ///@param seqUrlInfo: struct SSeqURLInfo containing data for URL construction
    ///@param id: seqid CSeq_id
    ///@param scopeRef:scope to fetch sequence    
    static string GetIDUrl(SSeqURLInfo *seqUrlInfo,
                           const objects::CSeq_id& id,
                           objects::CScope &scope);
                           

    ///Create URL for seqid 
    ///@param seqUrlInfo: struct SSeqURLInfo containing data for URL construction
    ///@param ids: CBioseq::TId object        
    static string GetIDUrl(SSeqURLInfo *seqUrlInfo,
                            const objects::CBioseq::TId* ids);                            

    static string GetFullIDLink(SSeqURLInfo *seqUrlInfo,
                            const objects::CBioseq::TId* ids);                            

    ///Create URL for seqid that goes to entrez or trace
    ///@param seqUrlInfo: struct SSeqURLInfo containing data for URL construction
    ///@param id: seqid CSeq_id
    ///@param scopeRef:scope to fetch sequence    
    static string GetIDUrlGen(SSeqURLInfo *seqUrlInfo,
                              const objects::CSeq_id& id,
                              objects::CScope &scope);
                              

    ///Create URL for seqid that goes to entrez or trace
    ///@param seqUrlInfo: struct SSeqURLInfo containing data for URL construction
    ///@param ids: CBioseq::TId object        
    static string GetIDUrlGen(SSeqURLInfo *seqUrlInfo,const objects::CBioseq::TId* ids);

    ///Create info indicating what kind of links to display
    ///@param seqUrlInfo: struct SSeqURLInfo containing data for URL construction    
    ///@param customLinkTypesInp: original types of links to be included in the list    
    ///@return: int containing customLinkTypes with the bits set to indicate what kind of links to display for the sequence
    ///
    ///examples:(Mapviewer,Download,GenBank,FASTA,Seqviewer, Trace, SRA, SNP, GSFASTA)
    static int SetCustomLinksTypes(SSeqURLInfo *seqUrlInfo, int customLinkTypesInp);

    ///Create the list of string links for seqid that go 
    /// - to GenBank,FASTA and Seqviewer for gi > 0 
    /// - customized links determined by seqUrlInfo->blastType for gi = 0
    /// - customized links determined by customLinkTypes    
    ///@param seqUrlInfo: struct SSeqURLInfo containing data for URL construction
    ///@param id: CSeq_id object    
    ///@param scope: scope to fetch this sequence
    ///@param customLinkTypes: types of links to be included in the list(mapviewer,seqviewer or download etc)    
    ///@param customLinksList: list of strings containing links
    static list <string>  GetCustomLinksList(SSeqURLInfo *seqUrlInfo,
                                   const objects::CSeq_id& id,
                                   objects::CScope &scope,                                             
                                   int customLinkTypes = eLinkTypeDefault);

    static list<string>  GetGiLinksList(SSeqURLInfo *seqUrlInfo,bool hspRange = false);
    static string  GetGraphiscLink(SSeqURLInfo *seqUrlInfo,bool hspRange = false);
    static list<string>  GetSeqLinksList(SSeqURLInfo *seqUrlInfo,bool hspRange = false);
    
    


    ///Create URL showing aligned regions info
    ///@param seqUrlInfo: struct SSeqURLInfo containing data for URL construction
    ///@param id: CSeq_id object    
    ///@param scope: scope to fetch this sequence
    ///@return: string containing URL
    ///
    static string  GetFASTALinkURL(SSeqURLInfo *seqUrlInfo,
                                   const objects::CSeq_id& id,
                                   objects::CScope &scope);

    ///Create URL to FASTA info
    ///@param seqUrlInfo: struct SSeqURLInfo containing data for URL construction
    ///@param id: CSeq_id object    
    ///@param scope: scope to fetch this sequence
    ///@return: string containing URL
    ///
    static string GetAlignedRegionsURL(SSeqURLInfo *seqUrlInfo,
                                const objects::CSeq_id& id,
                                objects::CScope &scope);

    ///Set the database as gi type
    ///@param actual_aln_list: the alignment
    ///@param scope: scope to fetch sequences
    ///
    static CAlignFormatUtil::DbType GetDbType(const objects::CSeq_align_set& actual_aln_list, 
                                              objects::CScope & scope);
                                          
    static CAlignFormatUtil::SSeqAlignSetCalcParams* GetSeqAlignCalcParams(const objects::CSeq_align& aln);

    static CAlignFormatUtil::SSeqAlignSetCalcParams* GetSeqAlignSetCalcParams(const objects::CSeq_align_set& aln,int queryLength,bool do_translation);

    static CAlignFormatUtil::SSeqAlignSetCalcParams* GetSeqAlignSetCalcParamsFromASN(const objects::CSeq_align_set& alnSet);
    static double GetSeqAlignSetCalcPercentIdent(const objects::CSeq_align_set& aln, bool do_translation);

    static map < string, CRef<objects::CSeq_align_set>  >  HspListToHitMap(vector <string> seqIdList, const objects::CSeq_align_set& source);

    ///Scan the the list of blast deflines and find seqID to be use in display    
    ///@param handle: CBioseq_Handle [in]
    ///@param aln_id: CSeq_id object for alignment seq [in]
    ///@param use_this_gi: list<int> list of gis to use [in]
    ///@param gi: gi to be used for display if exists or 0    
    ///@param taxid: taxid to be used for display if exists or 0    
    ///@return: CSeq_id object to be used for display
    static CRef<objects::CSeq_id> GetDisplayIds(const objects::CBioseq_Handle& handle,
                                                const objects::CSeq_id& aln_id,
                                                list<TGi>& use_this_gi,
                                                TGi& gi,
                                                TTaxId& taxid);
    ///Scan the the list of blast deflines and find seqID to be use in display    
    ///@param handle: CBioseq_Handle [in]
    ///@param aln_id: CSeq_id object for alignment seq [in]
    ///@param use_this_gi: list<int> list of gis to use [in]
    ///@param gi: gi to be used for display if exists or 0    
    ///@return: CSeq_id object to be used for display
    static CRef<objects::CSeq_id> GetDisplayIds(const objects::CBioseq_Handle& handle,
                                                const objects::CSeq_id& aln_id,
                                                list<TGi>& use_this_gi,
                                                TGi& gi);

    static TGi GetDisplayIds(const list< CRef< objects::CBlast_def_line > > &bdl,
                                              const objects::CSeq_id& aln_id,
                                              list<TGi>& use_this_gi);
    static bool GetTextSeqID(const list<CRef<objects::CSeq_id> > & ids,string *textSeqID = NULL);
    static bool GetTextSeqID(CConstRef<objects::CSeq_id> seqID,string *textSeqID = NULL);
    

    ///Scan the the list of blast deflines and find seqID to be use in display    
    ///@param handle: CBioseq_Handle [in]
    ///@param aln_id: CSeq_id object for alignment seq [in]
    ///@param use_this_seq: list<string> list of seqids to use (gi:ssssss or seqid:sssss) [in]
    ///@param gi: pointer to gi to be used for display if exists
    ///@param taxid: pointer to taxid to be used for display if exists
    ///@param textSeqID: pointer to textSeqID to be used for display if exists
    ///@return: CSeq_id object to be used for display
    static CRef<objects::CSeq_id> GetDisplayIds(const objects::CBioseq_Handle& handle,
                                const objects::CSeq_id& aln_id,
                                list<string>& use_this_seq,
                                TGi *gi = NULL,                                
                                TTaxId *taxid  = NULL,
                                string *textSeqID = NULL);



    ///Check if accession is WGS
    ///@param accession: string accession [in]
    ///@param wgsProj: string  wgsProj [out]      
    ///@return: bool indicating if accession is WGS
    static bool IsWGSAccession(string &accession, string &wgsProj);

    ///Check if accession is WGS


    ///@param accession: string accession [in]    
    ///@return: bool indicating if accession is WGS
    static bool IsWGSPattern(string &wgsAccession);

    static unique_ptr<CNcbiRegistry> m_Reg;
    static string m_Protocol;
    static bool   m_geturl_debug_flag;
    

    /// Calculate the uniq subject query coverage range (blastn only)
    static int GetUniqSeqCoverage(objects::CSeq_align_set & alnset);
    static TGi GetGiForSeqIdList (const list<CRef<objects::CSeq_id> >& ids);

    static string GetTitle(const objects::CBioseq_Handle & bh);

    /// Get sequence id with no database source (bare accession)
    static string GetBareId(const objects::CSeq_id& id);
    
    
    
protected:

    ///Wrap a string to specified length.  If break happens to be in
    /// a word, it will extend the line length until the end of the word
    ///@param str: input string
    ///@param line_len: length of each line desired
    ///@param out: stream to ouput
    ///@param html Is this HTML output? [in]
    static void x_WrapOutputLine(string str, size_t line_len, 
                                 CNcbiOstream& out,
                                 bool html = false);
};

END_SCOPE(align_format)
END_NCBI_SCOPE

#endif /* OBJTOOLS_ALIGN_FORMAT___ALIGN_FORMAT_UTIL_HPP */
