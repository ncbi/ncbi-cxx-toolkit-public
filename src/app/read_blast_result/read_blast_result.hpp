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
* Author of the template:  Aaron Ucko
*
* File Description:
*   Simple program demonstrating the use of serializable objects (in this
*   case, biological sequences).  Does NOT use the object manager.
*
* Modified: Azat Badretdinov
*   reads seq-submit file, blast file and optional tagmap file to produce list of potential candidates
*
* ===========================================================================
*/
#include <stdio.h>
#include <malloc.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>


#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbifile.hpp>

#include <serial/iterator.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>

#include <objmgr/util/sequence.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/bioseq_handle.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Date.hpp>

#include <objects/submit/Seq_submit.hpp>
#include <objects/submit/Submit_block.hpp>

#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/Imp_feat.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <objects/seqalign/Score.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Dense_seg.hpp>

#include <string>
#include <algorithm>

USING_SCOPE(ncbi);
USING_SCOPE(objects);

#include "tbl.h"

typedef struct
  {
  CRef<CSeq_loc> seqloc;
  string locus_tag;
  int sort_key;
  } TGenfo;
//typedef map<CSeq_id::EAccessionInfo, CRef<CSeq_loc> > TranStr3;
typedef map<CSeq_id::EAccessionInfo, TGenfo > TranStr3;
typedef map<string, TranStr3> TranStrMap3;



typedef struct { int q_left_left, q_left_middle, q_left_right, space, q_right_left, q_right_middle, q_right_right;
                 int s_left_left, s_left_middle,                      s_left_right;
                 int                             s_right_left,                      s_right_middle, s_right_right;
                 string q_id_left, q_id_right;
                 long   s_id;
                 string q_name_left, q_name_right;
                 string s_name;
                 string alignment_left;
                 string alignment_right;
                 ENa_strand left_strand, right_strand;
                 int left_frame, right_frame;
                 int diff_left, diff_right;
                 int diff_edge_left, diff_edge_right;
                 int q_loc_left_from;
                 int q_loc_left_to;
                 int q_loc_right_from;
                 int q_loc_right_to;
               }  distanceReportStr;
enum ECoreDataType   
     {
     eUndefined = 0,
     eSubmit,
     eEntry,
     eTbl
     };

enum EProblem
     {
     eOverlap            = (1<<0),
     eCompleteOverlap    = (1<<1),
     ePartial            = (1<<2),
     eFrameShift         = (1<<3),
     eMayBeNotFrameShift = (1<<4),
     eRnaOverlap         = (1<<5),
     eTRNAMissing        = (1<<6),
     eTRNABadStrand      = (1<<7),
     eTRNAComMismatch    = (1<<8),
     eTRNAMismatch       = (1<<9),
     eTRNAAbsent         = (1<<10),
     eRemoveOverlap      = (1<<11),
     eRelFrameShift = eFrameShift | eMayBeNotFrameShift,
     eTRNAProblems  =  eTRNAMissing
        | eTRNABadStrand
        | eTRNAAbsent   
        | eTRNAComMismatch
        | eTRNAMismatch
        ,
     eAllProblems = eOverlap 
        | eRnaOverlap 
        | eCompleteOverlap 
        | ePartial 
        | eFrameShift
        | eTRNAProblems
        ,
     };

enum EMyFeatureType
  {
  eMyFeatureType_unknown = 0,
  eMyFeatureType_pseudo_tRNA ,
  eMyFeatureType_atypical_tRNA,
  eMyFeatureType_normal_tRNA,
  eMyFeatureType_hypo_CDS,
  eMyFeatureType_normal_CDS,
  };

typedef struct
     {
     EProblem type;
     string message;
     string misc_feat_message;
//  
     string id1;
     string id2;
     int i1, i2;
     ENa_strand strand;
     } problemStr;

typedef struct
     {
     list<problemStr> problems;
     } diagStr; // argument to seq


typedef map < string , diagStr > diagMap;
typedef struct 
     {
     list < long > sbjGIs;
     int sbjLen;
     string sbjName;
     double bitscore;
     double eval;
     int nident;
     int alilen;
     double pident;
     int npos;
     int ppos;
     string alignment;
     int sbjstart, sbjend, q_start, q_end;
     }
    hitStr;
typedef struct 
     {
     int qLen;
     string qName;
     vector < hitStr > hits;
     } 
    blastStr;

typedef struct
     {
     string s_name;
     } perfectHitStr;

typedef map<long,long> parent_map;

typedef map < string, CRef < CSeq_feat > > LocMap;

typedef struct
  {
  string type3;
  int from;
  int to;
  ENa_strand strand;
  bool present;
  } TExtRNA;

typedef struct
  {
  int from, to;
  ENa_strand strand;
  } TSimplePair;

typedef vector<TSimplePair> TSimplePairs;

typedef struct
  {
  int key;
  string locus_tag;
  string name;
  string description;
  string type;
  TSimplePairs exons;
  CRef<CBioseq> seq;
  }
TSimpleSeq;

typedef list<TSimpleSeq> TSimpleSeqs;

typedef vector<TExtRNA> TExtRNAtable;

class CReadBlastApp : public CNcbiApplication
{
    virtual void Init(void);
    virtual int  Run(void);
public: 
    static string getLocusTag(const CBioseq& seq);

private:

// Main functions
    int ReadBlast(const char *file, map<string, blastStr>& blastMap );
    int ReadTRNA(const string& file);
    int ReadTRNA2(const string& file);
    int ReadRRNA2(const string& file);
    int StoreBlast(map<string, blastStr>& blastMap ); 
    // int ReadParents(CNcbiIstream& in, const string& nacc);
    int ReadParents(CNcbiIstream& in, const list<long>& nacc);
    bool ReadPreviousAcc(const string& file, list<long>& input_acc);
    int ProcessCDD(map<string, blastStr>& blastMap);
    int ReadTagMap(const char *file);

    int SortSeqs(void);
    int CollectSimpleSeqs(TSimpleSeqs& seqs);
    static bool less_seq(CRef<CSeq_entry>first, CRef<CSeq_entry>second);
    static bool less_simple_seq(TSimpleSeq first, TSimpleSeq second);
    int SortSeqs(CBioseq_set::TSeq_set& seqs);
    int AnalyzeSeqs(void);
    int AnalyzeSeqs(CBioseq_set::TSeq_set& seqs);

    int SetParents(CSeq_entry* parent, CBioseq_set::TSeq_set& where);
    int AnalyzeSeqsViaBioseqs(bool in_pool_prot, bool against_prot);
    int AnalyzeSeqsViaBioseqs( CBioseq_set::TSeq_set& in_pool_seqs,  CBioseq_set::TSeq_set& against_seqs,
                              bool in_pool_prot, bool against_prot);
    int AnalyzeSeqsViaBioseqs(CBioseq& left, CBioseq_set::TSeq_set& against_seqs,
                              bool against_prot);
    int AnalyzeSeqsViaBioseqs(CBioseq& left, CBioseq& right);
    int AnalyzeSeqsViaBioseqs1(CBioseq& left); // unary analysis


    int simple_overlaps(void);
// this is for optimization, do not laugh
    void ugly_simple_overlaps_call(int& n_user_neighbors, int& n_ext_neighbors,
      TSimpleSeqs::iterator& ext_rna,
      TSimpleSeqs::iterator& first_user_in_range, TSimpleSeqs::iterator& first_user_non_in_range,
      TSimpleSeqs& seqs, int max_distance,
      TSimpleSeqs::iterator& first_ext_in_range, TSimpleSeqs::iterator& first_ext_non_in_range,
      string& bufferstr);




    int CollectFrameshiftedSeqs(map<string,bool>& problem_names);
    int RemoveProblems(void);
    int RemoveProblems(CBioseq_set::TSeq_set& seqs, const map<string, bool>& problem_seqs);
    int RemoveProblems(CBioseq::TAnnot& annots, const map<string, bool>& problem_seqs);
    int RemoveProblems(CSeq_annot::C_Data::TFtable& table, const map<string, bool>& problem_seqs);

    int RemoveInterim(void);
    int RemoveInterim(CBioseq_set::TSeq_set& seqs);
    int RemoveInterim(CBioseq::TAnnot& annots);


    void processFeature ( CSeq_annot::C_Data::TFtable::iterator& feat, TranStrMap3& tranStrMap );
    template <typename t> void processAnnot   ( CBioseq::TAnnot::iterator& annot,            t& tranStrMap);
    void addLocation(string& prot_id, CBioseq& seq, CRef<CSeq_loc>& loc, const string& locus_tag);


    int CopyInfoFromGenesToProteins(void);
    void dump_fasta_for_pretty_blast ( diagMap& diag);
    void append_misc_feature(CBioseq_set::TSeq_set& seqs, const string& name, EProblem problem_type);

// tools 
    static char *next_w(char *w);
    static char *skip_space(char *w);
    static bool is_prot_entry(const CBioseq& seq);
    static bool has_blast_hits(const CBioseq& seq);
    static bool skip_to_valid_seq_cand(
     CBioseq_set::TSeq_set::const_iterator& seq, 
     const CBioseq_set::TSeq_set& seqs);
    static bool skip_to_valid_seq_cand(
     CBioseq_set::TSeq_set::iterator& seq, 
     CBioseq_set::TSeq_set& seqs);
    static int skip_toprot(CTypeIterator<CBioseq>& seq);
    static int skip_toprot(CTypeConstIterator<CBioseq>& seq);
    static bool skip_toprot(CBioseq_set::TSeq_set::const_iterator& seq,
                            const CBioseq_set::TSeq_set& seqs);
    static bool skip_toprot(CBioseq_set::TSeq_set::iterator& seq,
                            CBioseq_set::TSeq_set& seqs);
    static const CSeq_loc& getGenomicLocation(const CBioseq& seq);
    static bool                 hasGenomicLocation(const CBioseq& seq);

    static const CSeq_interval& getGenomicInterval(const CBioseq& seq);
    static bool                 hasGenomicInterval(const CBioseq& seq);
    
    static string GetProtName(const CBioseq& seq);

    static string getAnnotName(CBioseq::TAnnot::const_iterator& annot);
    static string getAnnotComment(CBioseq::TAnnot::const_iterator& annot);
    static int getQueryLen(const CBioseq& seq);
    static vector<long> getGIs(CBioseq::TAnnot::const_iterator& annot);
    static int getLenScore( CBioseq::TAnnot::const_iterator& annot);
    static void getBounds
     (
     CBioseq::TAnnot::const_iterator& annot,
     int* qFrom, int* qTo, int* sFrom, int* sTo
     );
    static bool giMatch(const vector<long>& left, const vector<long>& right);
    static int collectPerfectHits(vector<perfectHitStr>& perfect, const CBioseq& seq);
    static void check_alignment
      (
      CBioseq::TAnnot::const_iterator& annot,
      const CBioseq& seq, 
      vector<perfectHitStr>& results
      );
public:
    static void getFromTo(const CSeq_loc& loc, TSeqPos& from, TSeqPos& to, ENa_strand& strand);
    static void getFromTo(const CSeq_loc_mix& mix, TSeqPos& from, TSeqPos& to, ENa_strand& strand);
    static void getFromTo(const CPacked_seqint& mix, TSeqPos& from, TSeqPos& to, ENa_strand& strand);
    static void getFromTo(const CSeq_interval& inter, TSeqPos& from, TSeqPos& to, ENa_strand& strand);
    static int get_neighboring_sequences(
      const TSimpleSeqs::iterator& ext_rna,
      TSimpleSeqs::iterator& first_user_in_range, TSimpleSeqs::iterator& first_user_non_in_range,
      TSimpleSeqs& seqs, const int max_distance);
    static int sequence_proximity(const int target_from, const int target_to,
                           const int from, const int to, const int key);
    static int sequence_proximity(const int target_from, const int target_to,
                           const int from, const int to, const int key, const int max_distance);
    static void addSimpleTab(strstream& buffer, const string tag, const TSimpleSeqs::iterator& ext_rna, 
       const int max_distance);
    static void addLoctoSimpleSeq(TSimpleSeq& seq, const CSeq_loc&  loc);

private:

// polymorphic wrappers around core data
    CBeginInfo      Begin(void);
    CConstBeginInfo ConstBegin(void);   
// input tools

    static ECoreDataType getCoreDataType(istream& in);
    bool IsSubmit();
    bool IsEntry ();
    bool IsTbl   ();

// output tools

    static void printReport( distanceReportStr *report, ostream& out=NcbiCout);
    static void printOverlapReport( distanceReportStr *report, ostream& out=NcbiCout);
    static void printPerfectHit ( const perfectHitStr& hit, ostream& out=NcbiCout);

    void printGeneralInfo(ostream& out=NcbiCerr);

    static void dumpAlignment( const string& alignment, const string& file);

// more streamline output tools
    static bool hasProblems(const CBioseq& seq, diagMap& diag, const EProblem type);
    static bool hasProblems(const string& qname, diagMap& diag, const EProblem type);
    void reportProblems(const bool report_and_forget, diagMap& diag, ostream& out, 
      const CBioseq::TAnnot& annots, const EProblem type);
    void reportProblems(const bool report_and_forget, diagMap& diag, ostream& out, 
      const CSeq_annot::C_Data::TFtable& feats, const EProblem type);
    void reportProblems(const string& qname, diagMap& diag, ostream& out, const EProblem type);
    void reportProblems(const bool report_and_forget, diagMap& diag, ostream& out=NcbiCout, const EProblem type=eAllProblems);
    void reportProblemMessage(const string& message, ostream& out=NcbiCout);
    void reportProblemType(const EProblem type, ostream& out=NcbiCout);
    void reportProblemSequenceName(const string& name, ostream& out=NcbiCout);

    void erase_problems(const string& qname, diagMap& diag, const EProblem type);

// verbosity output tools
    static void PushVerbosity(void) { m_saved_verbosity.push( m_current_verbosity); } 
    static void PopVerbosity(void) { m_current_verbosity = m_saved_verbosity.top(); m_saved_verbosity.pop(); } 
public:
    static bool PrintDetails(int current_verbosity = m_current_verbosity)  
       { 
       bool result = current_verbosity < m_verbosity_threshold;
       if(result)
          NcbiCerr << current_verbosity << "(" << m_verbosity_threshold << "): ";
       return result; 
       }
    static void IncreaseVerbosity(void)  { m_current_verbosity++; }
    static void DecreaseVerbosity(void)  { m_current_verbosity--; }
private:


// algorithms
    void GetRNAfeats
      (
      const LocMap& loc_map,
      CSeq_annot::C_Data::TFtable& rna_feats,
      const CSeq_annot::C_Data::TFtable& feats
      );
    void GetLocMap
      (
      LocMap& loc_map, 
      const CSeq_annot::C_Data::TFtable& feats
      );
    bool CheckMissingRibosomalRNA( const CBioseq::TAnnot& annots);
    bool CheckMissingRibosomalRNA( const CSeq_annot::C_Data::TFtable& feats);

    int find_overlap(TSimpleSeqs::iterator& seq, const TSimpleSeqs::iterator& ext_rna,
                     TSimpleSeqs& seqs, int& overlap);
    int find_overlap(TSimpleSeqs::iterator& seq, const TSimpleSeqs::iterator& ext_rna,
                     TSimpleSeqs& seqs, TSimpleSeqs& best_seq);

    bool overlaps_na ( const CBioseq::TAnnot& annots);
    bool overlaps_na ( const CSeq_annot::C_Data::TFtable& feats);
    bool overlaps_na ( const CSeq_feat& f1, const CSeq_feat& f2, int& overlap); 

    bool overlaps_prot_na ( CBioseq& seq, const CBioseq::TAnnot& annots);
    bool overlaps_prot_na ( CBioseq& seq, const CSeq_annot::C_Data::TFtable& feats );
    bool overlaps_prot_na ( const string& n1, const CSeq_interval& i1,  const CSeq_feat& f2, int& overlap );


    bool match_na ( const CSeq_feat& f1, const string& type1);
    int  match_na ( const CSeq_feat& f1, const TExtRNA& ext_rna, 
                    int& left, int& right, bool& strand_match, int& abs_left );
    int overlaps(const TSimpleSeqs::iterator& seq1, const TSimpleSeqs::iterator& seq2, int& overlap);

    template <typename t1, typename t2> bool overlaps ( const t1& l1, const t2& l2, int& overlap);
    // bool overlaps ( const CSeq_loc& l1, const CSeq_loc& l2, int& overlap);
    bool overlaps ( const CSeq_loc& l1, int from, int to, int& overlap);
    bool complete_overlap ( const CSeq_loc& l1, const CSeq_loc& l2);
    // template <typename t1, typename t2> bool overlaps ( const t1& l1, const t2& l2, int& overlap);
    bool overlaps
      (
      const CBioseq& left,
      const CBioseq& right
      );
    bool fit_blast
      (
      const CBioseq& left, 
      const CBioseq& right,
      string& common_subject
      );
    static bool fit_blast
      (
      const CBioseq& left, 
      const CBioseq& right,
      CBioseq::TAnnot::const_iterator& left_annot,
      CBioseq::TAnnot::const_iterator& right_annot,
      int left_qLen, 
      int right_qLen,
      int space, 
      distanceReportStr* report
      );

// member vars
    // Member variable to help illustrate our naming conventions
    CSeq_submit m_Submit;
    CSeq_entry  m_Entry;
    Ctbl m_tbl;
    ECoreDataType   m_coreDataType;
    map < string, string > m_tagmap;
    bool m_usemap;
    string m_align_dir;
    diagMap m_diag;
    parent_map m_parent;
    // string m_previous_genome;
    list<long> m_previous_genome;

    TExtRNAtable m_extRNAtable;
    TSimpleSeqs m_extRNAtable2; // external rna sequences
    TSimpleSeqs m_simple_seqs;  // internal rna sequences

// alogithm control
    static double m_small_tails_threshold;
    static int    m_n_best_hit;
    static double m_eThreshold; 
    static double m_entireThreshold; 
    static double m_partThreshold; 
    static int    m_rna_overlapThreshold;
    static int    m_cds_overlapThreshold;
    static double m_trnascan_scoreThreshold;

// verbosity
    static int    m_verbosity_threshold;
    static int    m_current_verbosity;
    static stack < int > m_saved_verbosity;

   
};

// global functions
ESerialDataFormat s_GetFormat(const string& name);


CBioseq_set::TSeq_set* get_parent_seqset(const CBioseq& seq);

template <typename interval_type> string GetLocationString ( const interval_type& loc);
string GetLocationString ( const CSeq_feat& f); // will return just location
// string GetLocationString ( const CSeq_interval& inter );
// string GetLocationString ( const CSeq_loc& loc);

string GetLocusTag(const CSeq_feat& f, const LocMap& loc_map);

int addProblems(list<problemStr>& dest, const list<problemStr>& src);

string GetStringDescr(const CBioseq& bioseq);

string Get3type(const CRNA_ref& rna);
string GetRRNAtype(const CRNA_ref& rna);

string printed_range(const TSimpleSeqs::iterator& ext_rna);
string printed_range(const TSimpleSeqs::iterator& ext_rna, const TSimpleSeqs::iterator& end);
string printed_range(const TSimpleSeqs::iterator& ext_rna, TSimpleSeqs& seqs);




string let1_2_let3(char let1);
string diagName(const string& type, const string& value);

EMyFeatureType get_my_seq_type(const CBioseq& seq);
EMyFeatureType get_my_feat_type(const CSeq_feat& feat, const LocMap& loc_map);
string get_trna_string(const CSeq_feat& feat);
string GetRNAname(const CSeq_feat& feat);








/*
* ===========================================================================
*
* $Log: read_blast_result.h,v $
* Revision 1.13  2007/11/13 20:21:04  badrazat
* fixing broken individiual alignment links for CDD html output
*
* Revision 1.12  2007/11/08 15:49:04  badrazat
* 1. added locus tags
* 2. fixed bugs in detecting RNA problems (simple seq related)
*
* Revision 1.11  2007/10/04 16:35:43  badrazat
* splits
*
* Revision 1.10  2007/10/04 15:25:08  badrazat
* and more reduction
*
* Revision 1.9  2007/10/04 14:19:12  badrazat
* simplification worked!
*
* Revision 1.8  2007/10/04 13:05:47  badrazat
* added accession other
*
* Revision 1.7  2007/10/03 16:28:52  badrazat
* update
*
* Revision 1.6  2007/09/20 17:15:42  badrazat
* more editing
*
* Revision 1.5  2007/09/20 14:40:44  badrazat
* read routines to their own files
*
* Revision 1.4  2007/07/25 12:40:41  badrazat
* read_blast_result_lib.cpp: renaming some routines
* read_blast_result_lib.cpp: attempt at smarting up RemoveInterim: nothing has been done
* read_blast_result.cpp: second dump of -out asn does not happen, first (debug) dump if'd out
*
* Revision 1.3  2007/07/24 16:15:55  badrazat
* added general tag str to the methods of copying location to seqannots
*
* Revision 1.2  2007/07/19 20:59:26  badrazat
* SortSeqs is done for all seqsets
*
* Revision 1.1  2007/06/21 16:21:31  badrazat
* split
*
* Revision 1.20  2007/06/20 18:28:41  badrazat
* regular checkin
*
* Revision 1.19  2007/05/16 18:57:49  badrazat
* fixed feature elimination
*
* Revision 1.18  2007/05/04 19:42:56  badrazat
* *** empty log message ***
*
* Revision 1.17  2006/11/03 15:22:29  badrazat
* flatfiles genomica location starts with 1, not 0 as in case of ASN.1 file
* changed corresponding flatfile locations in the TRNA output
*
* Revision 1.16  2006/11/03 14:47:50  badrazat
* changes
*
* Revision 1.15  2006/11/02 16:44:44  badrazat
* various changes
*
* Revision 1.14  2006/10/25 12:06:42  badrazat
* added a run over annotations for the seqset in case of submit input
*
* Revision 1.13  2006/10/17 18:14:38  badrazat
* modified output for frameshifts according to chart
*
* Revision 1.12  2006/10/17 16:47:02  badrazat
* added modifications to the output ASN file:
* addition of frameshifts
* removal of frameshifted CDSs
*
* removed product names from misc_feature record and
* added common subject info instead
*
* Revision 1.11  2006/10/02 12:50:15  badrazat
* checkin
*
* Revision 1.9  2006/09/08 19:24:23  badrazat
* made a change for Linux
*
* Revision 1.8  2006/09/07 14:21:20  badrazat
* added support of external tRNA annotation input
*
* Revision 1.7  2006/09/01 13:17:23  badrazat
* init
*
* Revision 1.6  2006/08/21 17:32:12  badrazat
* added CheckMissingRibosomalRNA
*
* Revision 1.5  2006/08/11 19:36:09  badrazat
* update
*
* Revision 1.4  2006/05/09 15:08:51  badrazat
* new file cut_blast_output_qnd and some changes to read_blast_result.cpp
*
* Revision 1.3  2006/03/29 19:44:21  badrazat
* same borders are included now in complete overlap calculations
*
* Revision 1.2  2006/03/29 17:17:32  badrazat
* added id extraction from whole product record in cdregion annotations
*
* Revision 1.1  2006/03/22 13:32:59  badrazat
* init
*
* Revision 1000.1  2004/06/01 18:31:56  gouriano
* PRODUCTION: UPGRADED [GCC34_MSVC7] Dev-tree R1.3
*
* Revision 1.3  2004/05/21 21:41:41  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.2  2003/03/10 18:48:48  kuznets
* iterate->ITERATE
*
* Revision 1.1  2002/04/18 16:05:13  ucko
* Add centralized tree for sample apps.
*
*
* ===========================================================================
*/
