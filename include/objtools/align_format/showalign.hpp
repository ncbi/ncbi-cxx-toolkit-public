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

/** @file showalign.hpp
 *  Sequence alignment display tool
 *
 */

#ifndef OBJTOOLS_ALIGN_FORMAT___SHOWALIGN_HPP
#define OBJTOOLS_ALIGN_FORMAT___SHOWALIGN_HPP

#include <corelib/ncbireg.hpp>
#include <corelib/hash_set.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/blastdb/Blast_def_line_set.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seq/seqlocinfo.hpp>

#include <objtools/alnmgr/alnvec.hpp>
#include <objtools/readers/getfeature.hpp>
#include <objtools/blast/gene_info_reader/gene_info_reader.hpp>
#include <objtools/align_format/align_format_util.hpp>

#include <objmgr/object_manager.hpp>

#include <cgi/cgictx.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(align_format)

/// Auxiliary type to embed a CConstRef<objects::CSeq_id> in STL containers that require
/// operator< to be defined
struct SSeqIdKey {
    /// Constructor
    SSeqIdKey(const objects::CSeq_id& id) : m_Id(&id) {}
    /// Operator< to comply with STL container requirements
    bool operator<(const SSeqIdKey& other) const {
        return *m_Id < *other.m_Id;
    }
    /// Retrieve the object contained in this structure
    operator const objects::CSeq_id& () const { return *m_Id; }
private:
    CConstRef<objects::CSeq_id> m_Id;    ///< The wrapped object
};

/**
 * Example:
 * @code
 * CRef<objects::CSeq_align_set> aln_set = ...
 * CRef<objects::CScope> scope = ...
 * int display_option = 0;
 * display_option += CDisplaySeqalign::eShowGi;
 * display_option += CDisplaySeqalign::eHtml;
 * .......
 * CDisplaySeqalign ds(*aln_set, *scope);   
 * ds.SetAlignOption(display_option);
 * ds.DisplaySeqalign(stdout);
 * @endcode
 */

// Note: copied from algo/blast/core/blast_options.h
#define BLAST_DEFAULT_MATRIX   "BLOSUM62" 

class NCBI_ALIGN_FORMAT_EXPORT CDisplaySeqalign {

  public:
    // Defines
    
    
    /// Alignment display type, specific for showing blast-related info
    enum AlignType {
        eNotSet = 0,            // Default
        eNuc = 1,
        eProt = 2
    };
	
    ///structure for store feature display info
    struct FeatureInfo : public CObject {
        CConstRef < objects::CSeq_loc > seqloc;  // must be seqloc int
        char feature_char;               // Character for feature
        string feature_id;               // ID for feature
    };

    ///structure for showing domains on the master sequence
    struct DomainInfo : public CObject {
        CConstRef < objects::CSeq_loc > seqloc; //seqloc int only. use seq coodindate 
        string domain_name;               //domain string to show
        CConstRef < objects::CSeq_loc > subject_seqloc; //used only when master has
        //gaps at domain junctions
        bool is_subject_start_valid;
        bool is_subject_stop_valid;
    };

    ///option for alignment display
    enum DisplayOption {
        eHtml = (1 << 0),               // Html output. Default text.
        eLinkout = (1 << 1),            // Linkout gifs. 
        eSequenceRetrieval = (1 << 2),  // Get sequence feature
        eMergeAlign = (1 << 3),         // Merge alignment
        eShowMiddleLine = (1 << 4),     // Show line that indicates identity 
                                        // between query and hit. 
        eShowGi = (1 << 6),
        eShowIdentity = (1 << 7),       // Show dot as identity to master
        eShowBlastInfo = (1 << 8),      // Show defline and score info for 
                                        // blast pairwise alignment
        eShowBlastStyleId = (1 << 9),   // Show seqid as "Query" and "Sbjct" 
                                        // respectively for pairwise 
                                        // alignment. Default shows seqid as 
                                        // is
        eNewTargetWindow = (1 << 10),   // Clicking url link will open a new 
                                        // window
        eShowCdsFeature = (1 << 11),    // Show cds encoded protein seq for 
                                        // sequence. Need to fetch from id 
                                        // server, a bit slow. Only available
                                        // for non-anchored alignment 
        eShowGeneFeature = (1 << 12),   // Show gene for sequence. Need to 
                                        // fetch from id server, a bit slow.
                                        // Only available for non-anchored 
                                        // alignment
        eMasterAnchored = (1 << 13),    // Query anchored, for 
                                        // multialignment only, default not 
                                        // anchored
        eColorDifferentBases = (1 << 14),    // Coloring mismatches for
                                             // subject seq for eShowIdentity
                                             // case
        eTranslateNucToNucAlignment = (1 << 15), //nuecleotide to nucleotide
                                                //alignment as translated
        eShowBl2seqLink = (1 << 16),    // Show web link to bl2seq
        eDynamicFeature = (1 << 17),    //show dynamic feature line
        eShowNoDeflineInfo = (1 << 18), //Don't show defline when eShowBlastInfo
                                        //is chosen
        eHyperLinkSlaveSeqid = (1 << 19),    //Hyperlink slave seqids 
        eHyperLinkMasterSeqid = (1 << 20),    //Hyperlink master seqids 
        eDisplayTreeView = (1 << 21),         // Display tree feature
        eShowInfoOnMouseOverSeqid = (1 << 22), //show defline when mouse
                                               //seqid. Note you need 
                                               //seperate style sheet functions
                                               //for this to work
        eShowSortControls = (1 << 23),
        eShowGeneInfo = (1 << 24),
        eShowCheckBox = (1 << 25),

        eShowEndGaps = (1 << 26),      // if set end gaps will be shown as '-'
                                       // otherwise as spaces
        eShowGapOnlyLines = (1 << 27), // Show lines than contain only gaps
        eShowRawScoreOnly = (1 << 28),  // For RMBLASTN.  This disables the
                                       //   display of the bitscore and
                                       //   the evalue fields. -RMH-
        eShowTranslationForLocalSeq = (1 << 29),  //show translated protein for 
                                                  //local master nuc sequence
                                                  //and first local subject seq only.

        eShowAlignStatsForMultiAlignView = (1 << 30),  //percent identity etc. only for multi-align view.

        eShowSequencePropertyLabel = (1 << 31) //Property label (such as chain type for igblast) only for multi-align view
    };
    
    enum TranslatedFrameForLocalSeq {
        eFirst = 0,             
        eSecond,
        eThird
    };
    
    void SetTranslatedFrameForLocalSeq (TranslatedFrameForLocalSeq frame) {
        m_TranslatedFrameForLocalSeq = frame;
    }

    ///Middle line style option
    enum MiddleLineStyle {
        eChar = 0,              // show character as identity between query
                                // and hit. Default
        eBar                    // show bar as identity between query and hit
    };
    
    /// character used to display seqloc, such as masked sequence
    enum SeqLocCharOption {
        eX = 0,                 // use X to replace sequence character.
                                // Default 
        eN,                     // use n to replace sequence character
        eLowerCase              // use lower case of the original sequence
                                // letter
    };

    /// colors for seqloc display
    enum SeqLocColorOption {
        eBlack = 0,             // Default
        eGrey,
        eRed
    };


    enum LinksDisplayParams {        
        eDisplayDefault = 0,
        eDisplayResourcesLinks = (1 << 0),
        eDisplayDownloadLink = (1 << 1)
    };

    /// Constructors
    ///@param seqalign: seqalign to display. 
    ///@param mask_seqloc: seqloc to be displayed with different characters
    ///and colors such as masked sequence.
    ///@param external_feature:  Feature to display such as phiblast pattern.
    ///Must be seqloc-int 
    ///@param matrix_name: scoring matrix name [in]
    ///@param scope: scope to fetch your sequence 
    ///
    CDisplaySeqalign(const objects::CSeq_align_set & seqalign,
                     objects::CScope & scope,
                     list < CRef<CSeqLocInfo> >* mask_seqloc = NULL,
                     list < FeatureInfo * >* external_feature = NULL,
                     const char* matrix_name = BLAST_DEFAULT_MATRIX);
    
    /// Destructor
    ~CDisplaySeqalign();
    
    ///call this to display seqalign
    ///@param out: stream for display
    ///
    void DisplaySeqalign(CNcbiOstream & out);

    /// Set the linkout implementation
    void SetLinkoutDB(ILinkoutDB* l,
                      const string& mv_build_name) 
    {
        m_LinkoutDB = l; 
        m_MapViewerBuildName = mv_build_name;
    }

    //Display pariwise seqalign for the set of seq IDS (for future use)
    void DisplayPairwiseSeqalign(CNcbiOstream& out,hash_set <string> selectedIDs);
    //Data representing templates for defline display 
    struct SAlignTemplates {
        string alignHeaderTmpl; ///< Template for displaying header,deflines and gene info  - BLAST_ALIGN_HEADER
        string sortInfoTmpl;    ///< Template for displaying  Sort by header - SORT_ALIGNS_SEQ
        string alnDefLineTmpl;    ///< Template for displaying one defline ALN_DEFLINE_ROW    
        string alnSeqInfoTmpl;  ///< Template for displaying sequnce link in defline
        string alignInfoTmpl;   ///< Template for displaying singe align params - BLAST_ALIGN_PARAMS_NUC,BLAST_ALIGN_PARAMS_PROT
        string alignFeatureTmpl; ///< Template for displaying  align features -ALN_FEATURES 
        string alignFeatureLinkTmpl; ///< Template for displaying  align features link -ALN_FEATURES_LINK         
        string alignRowTmpl;    ///<Template for displayin actual pairwise alignment - BLAST_ALIGN_ROWS
        string alignRowTmplLast; ///<Template for displayin actual last pairwise alignment - BLAST_ALIGN_ROWS_LST
    };
       
    /// Set functions
    /***The following functions are for all alignment display ****/
    
    /// Set according to DsiplayOption
    ///@param option: display option disired
    ///
    void SetAlignOption(int option)
    {
        m_AlignOption = option;
    } 
    
    ///character style for seqloc display such as masked region
    ///@param option: character style option
    ///
    void SetSeqLocChar(SeqLocCharOption option = eX) {
        m_SeqLocChar = option;
    }

    ///color for seqloc display such as masked region
    ///@param option: color desired
    ///
    void SetSeqLocColor(SeqLocColorOption option = eBlack) {
        m_SeqLocColor = option;
    }
    
    ///number of bases or amino acids per line
    ///@param len: length desired
    ///
    void SetLineLen(size_t len) {
        m_LineLen = len;
    }

    ///Display top num seqalign
    ///Note this only limit the number of seqalign regardless
    ///of the seqids.  This won't work if you want to limit the 
    ///number of hits (or nmuber of database seqeunces) as in blast display.
    ///
    ///@param num: number desired
    ///
    void SetNumAlignToShow(int num) {
        m_NumAlignToShow = num;
    }
    
    ///set middle line style
    ///@param option: style desired
    ///
    void SetMiddleLineStyle(MiddleLineStyle option = eBar) {
        m_MidLineStyle = option;
    }

    ///Set genetic code for master sequence
    ///@param code: the genetic code
    ///
    void SetMasterGeneticCode(int code) {
        m_MasterGeneticCode = code;
    }

    ///Set Genetic cod for slaves
    ///@param code: the genetic code
    ///
    void SetSlaveGeneticCode(int code) {
        m_SlaveGeneticCode = code;
    }

    /***The following functions are for blast alignment style display only***/
    
    ///Needed only if you want to display positives and strand 
    ///@param type: type of seqalign
    ///
    void SetAlignType(AlignType type) {
        m_AlignType = type;
    }

    ///set blast database name
    ///@param name: db name
    ///
    void SetDbName(string name) {
        m_DbName = name;
    }

    ///database type.  used for seq fetching from blast db
    ///@param is_na: is nuc database or not
    ///
    void SetDbType(bool is_na) {
        m_IsDbNa = is_na;
    }
    
    ///set blast request id
    ///@param rid: blast RID
    ///
    void SetRid(string rid) {
        m_Rid = rid;
    }

    /// CDD rid for constructing linkout
    ///@param cdd_rid: cdd RID
    ///
    void SetCddRid(string cdd_rid) {
        m_CddRid = cdd_rid;
    }

    ///for constructing structure linkout
    ///@param term: entrez query term
    ///
    void SetEntrezTerm(string term) {
        m_EntrezTerm = term;
    }

    /// for linking to mapviewer
    ///@param number: blast query number
    ///
    void SetQueryNumber(int number) {
        m_QueryNumber = number;
    }

    ///internal blast type
    ///@param type: refer to blobj->adm->trace->created_by
    ///
    void SetBlastType(string type) {
        m_BlastType = type;
    }

    /// Sets the masks and the masking algorithm used for the subject sequences
    /// @param masks subject masks [in]
    void SetSubjectMasks(const TSeqLocInfoVector& masks);
    
    void SetCgiContext (CCgiContext& ctx) {
        m_Ctx = &ctx;
    }

    ///Sets CDD precomputed results ID
    /// @param string containing seq id used in contsructing URL to CDART
    void SetPreComputedResID(string preComputedResID) {
        m_PreComputedResID = preComputedResID;
    }
    /// static functions
    ///Need to call this if the seqalign is stdseg or dendiag for ungapped
    ///blast alignment display as each stdseg ro dendiag is a distinct
    /// alignment.  Don't call it for other case as it's a waste of time.
    ///@param alnset: input alnset
    ///@return processed alnset
    ///
    static CRef < objects::CSeq_align_set >
    PrepareBlastUngappedSeqalign(const objects::CSeq_align_set & alnset);

    void SetAlignTemplates(SAlignTemplates *alignTemplates) {m_AlignTemplates = alignTemplates;}    
    
    
    void SetMasterDomain(list <CRef<DomainInfo> >* domain) {
        
        m_DomainInfo = domain;
    }

    void SetSequencePropertyLabel(const vector<string>* SequencePropertyLabel) {
        m_SeqPropertyLabel = SequencePropertyLabel;
    }

private:

    /// Prohibit copy constructor
    CDisplaySeqalign(const CDisplaySeqalign& other);
    /// Prohibit assignment operator
    CDisplaySeqalign& operator=(const CDisplaySeqalign& rhs);
 
    ///internal insert information
    ///aln_start. insert right after this position
    struct SInsertInformation : public CObject {
        int aln_start;              
        int seq_start;
        int insert_len;
    };
    typedef list< CRef<SInsertInformation> > TSInsertInformationList;
    
    ///store feature information
    struct SAlnFeatureInfo : public CObject {
        CRef<FeatureInfo> feature;
        string feature_string;
        list<TSeqPos> feature_start;
        CRange < TSignedSeqPos > aln_range;
    };
    typedef list< CRef<SAlnFeatureInfo> > TSAlnFeatureInfoList;
    
    ///store seqloc info
    struct SAlnSeqlocInfo : public CObject {
        CRef<CSeqLocInfo> seqloc;
        CRange < TSignedSeqPos > aln_range;
    };

    /// List of SAlnSeqlocInfo structures
    typedef list< CRef<SAlnSeqlocInfo> > TSAlnSeqlocInfoList;

    struct SAlnRowInfo : public CObject {
        vector<string> sequence;
        vector<objects::CAlnMap::TSeqPosList> seqStarts;
        vector<objects::CAlnMap::TSeqPosList> seqStops;
        vector<objects::CAlnMap::TSeqPosList> insertStart;
        vector<objects::CAlnMap::TSeqPosList> insertAlnStart;
        vector<objects::CAlnMap::TSeqPosList> insertLength;
        vector<string> seqidArray;
        string middleLine;
        vector<objects::CAlnMap::TSignedRange> rowRng;
        vector<int> frame;
        vector<int> taxid;
        vector<TSAlnFeatureInfoList> bioseqFeature;
        vector<TSAlnSeqlocInfoList> masked_regions;        
        size_t maxIdLen;
        size_t  maxStartLen;
        int max_feature_num;
        bool colorMismatch;
        int         rowNum;
        vector<int> match;
        vector<int> align_length;
        vector<double> percent_ident;
        vector<string> align_stats;
        int max_align_stats_len;
        vector<string> seq_property_label;
        int max_seq_property_label;
    };

    
	//Info used to display defline information
    struct SAlnDispParams: public CObject {
        int gi;                         ///< gi used in defline
        CRef<objects::CSeq_id> seqID;    ///< seqID used in defline
        string label;                   ///< sequence label
        string    id_url;                 ///< entrz, mapview etc id url
        string linkoutStr;              ///< string containing all linkout urls
        string dumpGnlUrl;              ///< download sequnce url
        string title;                   ///< sequnce title
    };

    ///Info used to contstruct seq url obtained from processing the whole seq align
    ///for particular subject sequnce
    struct SAlnLinksParams {
        string segs;                    ///< seq align segments in the format seg1start-seg1end,seg2start-seg2end,
        int  hspNumber;                 ///< hsp number  
        CRange<TSeqPos> *subjRange;     ///< subject sequnce range
        bool flip;                      ///< opposite starnds indicator

        /// Constructor        
        SAlnLinksParams(){hspNumber = 1;subjRange = NULL;flip = false;}
    };

    /// store alnvec and score info
    struct SAlnInfo : public CObject {               
        CRef < objects::CAlnVec > alnvec;
        int score;
        double bits;
        double evalue;
        list<int> use_this_gi;
        int comp_adj_method;
        int sum_n;
        string id_label;

        SAlnRowInfo *alnRowInfo;

        //Features calc params
        vector<objects::SFeatInfo*> feat_list;
        CRange<TSeqPos> actual_range;
        int subject_gi;        
        objects::SFeatInfo* feat5;
        objects::SFeatInfo* feat3;        


        //Identity calc params
        int match;
        int positive;
        int gap;
        int identity;
    };

	
     /// Definition of std::map of objects::CSeq_ids to masks
    typedef map< SSeqIdKey, TMaskedQueryRegions > TSubjectMaskMap;

    /// Map of subject masks
    TSubjectMaskMap m_SubjectMasks;

    /// reference to seqalign set
    CConstRef < objects::CSeq_align_set > m_SeqalignSetRef; 
    
    /// display character option for list of seqloc         
    list < CRef<CSeqLocInfo>  >* m_Seqloc; 

    /// external feature such as phiblast
    list < FeatureInfo * >* m_QueryFeature; 
    list <CRef<DomainInfo> >* m_DomainInfo; 
    const vector<string>* m_SeqPropertyLabel;

    objects::CScope & m_Scope;
    objects::CAlnVec *m_AV;                  // current aln vector
    int **m_Matrix;                 // matrix used to compute the midline
    int m_AlignOption;              // Display options
    AlignType m_AlignType;          // alignment type, used for displaying
                                    //blast info
    int m_NumAlignToShow;           // number of alignment to display
    SeqLocCharOption m_SeqLocChar;  // character for seqloc display
    SeqLocColorOption m_SeqLocColor; // color for seqloc display
    size_t m_LineLen;                  // number of sequence character per line
    bool m_IsDbNa;
    bool m_CanRetrieveSeq;
    string m_DbName;
    string m_BlastType;
    string m_LinkoutOrder;
    string m_Rid;
    string m_CddRid;
    string m_EntrezTerm;
    int m_QueryNumber;
    CNcbiIfstream *m_ConfigFile;
    CNcbiRegistry *m_Reg;
    objects::CGetFeature* m_DynamicFeature;
    
    map < string, struct SAlnLinksParams > m_AlnLinksParams;
    list <string> m_CustomLinksList;
    list <string> m_HSPLinksList;

    CRef < objects::CObjectManager > m_FeatObj;  // used for fetching feature
    CRef < objects::CScope > m_featScope;        // used for fetching feature
    MiddleLineStyle m_MidLineStyle;
    int m_MasterGeneticCode;
    int m_SlaveGeneticCode;
    CCgiContext* m_Ctx;
    SAlignTemplates *m_AlignTemplates;    
    /// Gene info reader object, reads Gene info entries from files.
    auto_ptr<CGeneInfoFileReader> m_GeneInfoReader;

    /// Current alignment index (added to the linkout and entrez URL's)
    mutable int m_cur_align;

    int		m_NumBlastDefLines;///< Number of subject sequence deflines 

    int     m_currAlignHsp;///< Current HSP number for single alignmnet

    string  m_PreComputedResID;///<CDD precomputed results ID

    /// Reference to LinkoutDB implementation. Not owned by this class
    ILinkoutDB* m_LinkoutDB;
    /// mapviewer build name associated with the sequences in the BLAST
    /// database out of which the results are being formatted by this class.
    string m_MapViewerBuildName;

    TranslatedFrameForLocalSeq m_TranslatedFrameForLocalSeq;

    string x_PrintDynamicFeatures(void); 
    ///Display the current alnvec
    ///@param out: stream for display
    ///
    void x_DisplayAlnvec(CNcbiOstream & out);
    
    ///print defline
    ///@param bsp_handle: bioseq of interest
    ///@param use_this_gi: display this gi instead    
    ///@return: string containig defline(s)
    ///
    string x_PrintDefLine(const objects::CBioseq_Handle& bsp_handle, SAlnInfo* aln_vec_info);
                        
    /// display sequence for one row
    ///@param sequence: the sequence for that row
    ///@param id: seqid
    ///@param start: seqalign coodinate
    ///@param len: length desired
    ///@param frame: for tranlated alignment
    ///@param row: the current row
    ///@param color_mismatch: colorize the mismatch or not
    ///@param loc_list: seqlocs to be shown as specified in constructor
    ///@param out: output stream
    ///
    void x_OutputSeq(string& sequence, const objects::CSeq_id& id, int start, 
                     int len, int frame, int row, bool color_mismatch, 
                     const TSAlnSeqlocInfoList& loc_list, 
                     CNcbiOstream& out) const;
    
    /// Count number of total gaps
    ///@return: number of toal gaps for the current alnvec
    ///
    int x_GetNumGaps();               
    
    ///get url to sequence record
    ///@param ids: id list    
    ///@param seqUrlInfo: struct containging params for URL    
    ///    


    CAlignFormatUtil::SSeqURLInfo *x_InitSeqUrl(int giToUse,string accession,int linkout,
        int taxid,const list<CRef<objects::CSeq_id> >& ids);

    string x_GetUrl(int giToUse,string accession,int linkout,int taxid,const list<CRef<objects::CSeq_id> >& ids);
    string x_GetUrl(const objects::CBioseq_Handle& bsp_handle,int giToUse,string accession,int linkout,int taxid,const list<CRef<objects::CSeq_id> >& ids,int lnkDispPrarms = 0);

    ///get dumpgnl url to sequence record
    ///@param ids: id list
    ///@param row: the current row
    ///@param alternative_url: user specified url or empty string
    ///@param taxid: taxid
    ///
    string x_GetDumpgnlLink(const list < CRef < objects::CSeq_id > >&ids) const;
    
    ///get feature info
    ///@param feature: where feature info to be filled
    ///@param scope: scope to fectch sequence
    ///@param choice: which feature to get
    ///@param row: current row number
    ///@param sequence: the sequence string
    ///@param feat_seq_range: to be filled with the feature seqlocs of this row
    ///@param feat_seq_strand: strand to be filled corresponding to feat_seq_range
    ///@param fill_feat_range: to fill feat_seq_range?
    ///
    void x_GetFeatureInfo(TSAlnFeatureInfoList& feature, objects::CScope & scope,
                          objects::CSeqFeatData::E_Choice choice, int row,
                          string& sequence,
                          list<list<CRange<TSeqPos> > >& feat_seq_range,
                          list<objects::ENa_strand>& feat_seq_strand,
                          bool fill_feat_range) const;
    
    ///get inserts info
    ///@param row: current row
    ///@param aln_range: the alignment range
    ///@param aln_start: start for current row
    ///@param inserts: inserts to be filled
    ///@param insert_pos_string: string to indicate the start of insert
    ///@param insert_list: information containing the insert info
    ///
    void x_FillInserts(int row, objects::CAlnMap::TSignedRange& aln_range, 
                       int aln_start, list < string >& inserts, 
                       string& insert_pos_string,
                       TSInsertInformationList& insert_list) const;
    
    ///recusively fill the insert for anchored view
    ///@param row: the row number
    ///@param aln_range: the alignment range
    ///@param aln_start: start for current row
    ///@param insert_list: information containing the insert info
    ///@param inserts: inserts strings to be inserted
    ///
    void x_DoFills(int row, objects::CAlnMap::TSignedRange& aln_range, int aln_start,
                   TSInsertInformationList& insert_list,
                   list < string > &inserts) const;
    
    ///segments starts and stops used for map viewer, etc
    ///@param row: row number
    ///@return: the seg string
    ///
    string x_GetSegs(int row) const;

    ///compute number of identical and positive residues
    ///and set middle line accordingly
    ///@param sequence_standard: the master sequence
    ///@param sequence: the slave sequence
    ///@param match: the number of identical match
    ///@param positive: number of positive match
    ///@param middle_line: the middle line to be filled
    ///
    void x_FillIdentityInfo(const string& sequence_standard,
                            const string& sequence, int& match,
                            int& positive, string& middle_line);

    ///set feature info
    ///@param feat_info: feature to fill in
    ///@param seqloc: feature for this seqloc
    ///@param aln_from: from coodinate
    ///@param aln_to: to coordinate
    ///@param aln_stop: the stop position for whole alignment
    ///@param pattern_char: the pattern character to show
    ///@param pattern_id: the pattern id to show
    ///@param alternative_feat_str: use this as feature string instead
    ///
    void x_SetFeatureInfo(CRef<SAlnFeatureInfo> feat_info, const objects::CSeq_loc& seqloc,
                          int aln_from, int aln_to, int aln_stop,
                          char pattern_char,  string pattern_id,
                          string& alternative_feat_str) const;

    ///get insert information
    ///@param insert_list: list to be filled
    ///@param insert_aln_start: alnment start coordinate info
    ///@param insert_seq_start: alnment sequence start info
    ///@param insert_length: insert length info
    ///@param line_aln_stop: alignment stop for this row
    ///
    void x_GetInserts(TSInsertInformationList& insert_list,
                      objects::CAlnMap::TSeqPosList& insert_aln_start,
                      objects::CAlnMap::TSeqPosList& insert_seq_start,
                      objects::CAlnMap::TSeqPosList& insert_length, 
                      int line_aln_stop);

    ///check if Gene info is enabled and a Gene link is present for a hit
    ///@param aln_vec_info: alnvec list
    ///
    bool x_IsGeneInfoAvailable(SAlnInfo* aln_vec_info);

    ///get the URL of the Gene info link.
    ///@param gene_id: gene id to link to.
    ///@return: fully formatted URL of the Gene info link.
    ///
    string x_GetGeneLinkUrl(int gene_id);

    ///display alnvec info
    ///@param out: output stream
    ///@param aln_vec_info: alnvec list
    ///
    void x_DisplayAlnvecInfo(CNcbiOstream& out, SAlnInfo* aln_vec_info,
                             bool show_defline,
                             bool showSortControls = false);

    ///output dynamic feature url
    ///@param out: output stream
    ///
    void x_PrintDynamicFeatures(CNcbiOstream& out,SAlnInfo* aln_vec_info);

    ///convert the passed seqloc list info using alnment coordinates
    ///@param loc_list: fill the list with seqloc info using aln coordinates
    ///@param masks: the masked regions 
    void x_FillLocList(TSAlnSeqlocInfoList& loc_list, 
                       const list< CRef<CSeqLocInfo> >* masks) const;

    ///get external query feature info such as phi blast pattern
    ///@param row_num: row number
    ///@param aln_stop: the stop position for the whole alignment
    ///@param features the return value for this method
    void x_GetQueryFeatureList(int row_num, int aln_stop, 
                               vector<TSAlnFeatureInfoList>& features) const;
    ///make the appropriate seqid
    ///@param id: the id to be filled
    ///@param row: row number
    ///
    void x_FillSeqid(string& id, int row) const;

    ///print out features and fill master_feat_str if applicable
    ///@param feature: the feature info
    ///@param row: row num
    ///@param alignment_range: alignment range
    ///@param aln_start: alignment start in align coordinates
    ///@param line_length: length per line
    ///@param id_length: the max seq id length
    ///@param start_length: the max seq start postion string length
    ///@param max_feature_num: max numbe of features contained
    ///@param master_feat_str: the feature for master seq
    ///@param out: the out stream
    ///
    void x_PrintFeatures(TSAlnFeatureInfoList& feature,
                         int row, 
                         objects::CAlnMap::TSignedRange alignment_range,
                         int aln_start,
                         int line_length, 
                         int id_length,
                         int start_length,
                         int max_feature_num, 
                         string& master_feat_str,
                         CNcbiOstream& out);

    CRef<objects::CAlnVec> x_GetAlnVecForSeqalign(const objects::CSeq_align& align); 
    ///Display Gene Info
    ///
    string x_DisplayGeneInfo(const objects::CBioseq_Handle& bsp_handle,SAlnInfo* aln_vec_info);
    
    ///Dipslay Bl2seq TBLASTX link
    ///
    void x_DisplayBl2SeqLink(CNcbiOstream& out);

    ///Display anchor for links from mapview
    ///
    void x_DisplayMpvAnchor(CNcbiOstream& out,SAlnInfo* aln_vec_info);

    ///Display Sorting controls
    ///
    void x_DisplayAlignSortInfo(CNcbiOstream& out,string id_label);

    ///Display score,bits,expect,method
    ///
    void x_DisplayAlignInfo(CNcbiOstream& out, SAlnInfo* aln_vec_info);

    ///Display pairwise alignment
    ///
    string x_DisplayRowData(SAlnRowInfo *alnRoInfo);

    ///Display identities,positives,frames etc
    ///
    void x_DisplayIdentityInfo(SAlnRowInfo *alnRoInfo, CNcbiOstream& out);

    ///Display Sorting controls,score,bits,expect,method,features identities,positives,frames etc
    ///
    void x_DisplaySingleAlignParams(CNcbiOstream& out, 
                                  SAlnInfo* aln_vec_info,bool showSortControls);

    void x_PrepareIdentityInfo(SAlnInfo* aln_vec_info);

    ///Calculate data for feature display
    ///
    void x_PrepareDynamicFeatureInfo(SAlnInfo* aln_vec_info);

    ///Calculate data for pairwise alignment display
    ///
    SAlnRowInfo *x_PrepareRowData(void);

    string x_FormatSingleAlign(SAlnInfo* aln_vec_info);  
    string x_FormatAlignSortInfo(string id_label);
    string x_FormatAlnBlastInfo(SAlnInfo* aln_vec_info);
    string x_FormatIdentityInfo(string alignInfo, SAlnInfo* aln_vec_info);
    string x_FormatDynamicFeaturesInfo(string alignInfo, SAlnInfo* aln_vec_info);
    string x_FormatOneDynamicFeature(string viewerURL,
                                     int subjectGi,                                                    
                                     int fromRange, 
                                     int toRange,
                                     string featText);
    ///Sets m_Segs,m_HspNumber
    void x_PreProcessSeqAlign(objects::CSeq_align_set &actual_aln_list);
    void x_CalcUrlLinksParams(const objects::CSeq_align& align, string idString,string toolUrl);
    void x_DisplayAlnvecInfoHead(CNcbiOstream& out, 
                                 SAlnInfo* aln_vec_info);

    ///Inits align parameters for displaySetup scope for feature fetching and m_DynamicFeature
    ///inits m_FeatObj,m_featScope,m_CanRetrieveSeq,m_ConfigFile,m_Reg,m_LinkoutOrder,m_DynamicFeature
    void x_InitAlignParams(objects::CSeq_align_set &actual_aln_list);
    
	    
    void x_PreProcessSingleAlign(objects::CSeq_align_set::Tdata::const_iterator currSeqAlignIter,
                                 objects::CSeq_align_set &actual_aln_list,
                                  bool multipleSeqs);
    string x_FormatAlnHSPLinks(string &alignInfo);
	
    SAlnDispParams *x_FillAlnDispParams(const CRef< objects::CBlast_def_line > &iter,
                                        const objects::CBioseq_Handle& bsp_handle,
								        list<int>& use_this_gi,
								        int firstGi);
								   
    
	SAlnDispParams *x_FillAlnDispParams(const objects::CBioseq_Handle& bsp_handle);	
	string x_FormatDefLinesHeader(const objects::CBioseq_Handle& bsp_handle,SAlnInfo* aln_vec_info);
	string	x_MapDefLine(SAlnDispParams *alnDispParams,bool isFisrt, bool linkout,bool hideDefline);
	void x_ShowAlnvecInfoTemplate(CNcbiOstream& out, SAlnInfo* aln_vec_info,bool show_defline,bool showSortControls);
	void x_ShowAlnvecInfo(CNcbiOstream& out, SAlnInfo* aln_vec_info,bool show_defline);    

    void x_GetDomainInfo(int row_num, int aln_stop,
                         vector<TSAlnFeatureInfoList>& retval)  const;

    void x_AddTranslationForLocalSeq(vector<TSAlnFeatureInfoList>& retval,
                                     vector<string>& sequence) const; 
};


END_SCOPE(align_format)
END_NCBI_SCOPE

#endif /* OBJTOOLS_ALIGN_FORMAT___SHOWALIGN_HPP */
