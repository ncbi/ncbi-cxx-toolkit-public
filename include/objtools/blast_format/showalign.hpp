#ifndef SHOWALIGN_HPP
#define SHOWALIGN_HPP

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

#include <corelib/ncbireg.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objtools/alnmgr/alnvec.hpp>
#include <objects/blastdb/Blast_def_line_set.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objtools/readers/getfeature.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/**
 * Example:
 * @code
 * int option = 0;
 * option += CDisplaySeqalign::eShowGi;
 * option += CDisplaySeqalign::eHtml;
 * .......
 * CDisplaySeqalign ds(aln_set, scope);   
 * ds.SetOption(defline_option);
 * ds.DisplaySeqalign(stdout);
 * @endcode
 */

class NCBI_XALNUTIL_EXPORT CDisplaySeqalign {

  public:
    // Defines
    
    /// frame defines for translated alignment
    enum TranslatedFrames {
        eFrameNotSet = 0,
        ePlusStrand1 = 1,
        ePlusStrand2 = 2,
        ePlusStrand3 = 3,
        eMinusStrand1 = -1,
        eMinusStrand2 = -2,
        eMinusStrand3 = -3
    };

    /// Alignment display type, specific for showing blast-related info
    enum AlignType {
        eNotSet = 0,            // Default
        eNuc = 1,
        eProt = 2
    };

    ///structure for seqloc info
    struct SeqlocInfo {
        CRef < CSeq_loc > seqloc;       // must be seqloc int
        TranslatedFrames frame;         // For translated nucleotide sequence
    };

    ///structure for store feature display info
    struct FeatureInfo {
        CConstRef < CSeq_loc > seqloc;  // must be seqloc int
        char feature_char;               // Character for feature
        string feature_id;               // ID for feature
    };

    ///protein matrix define
    enum {
        ePMatrixSize = 23       // number of amino acid for matrix
    };

    ///option for alignment display
    enum DisplayOption {
        eHtml = (1 << 0),               // Html output. Default text.
        eLinkout = (1 << 1),            // Linkout gifs. 
        eSequenceRetrieval = (1 << 2),  // Get sequence feature
        eMultiAlign = (1 << 3),         // Multiple alignment view. 
                                        // default pairwise
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
        eColorDifferentBases = (1 << 14),       // Coloring mismatches for
                                                // subject seq
        eTranslateNucToNucAlignment = (1 << 15), //nuecleotide to nucleotide
                                                //alignment as translated
        eShowBl2seqLink = (1 << 16),    // Show web link to bl2seq
        eDynamicFeature = (1 << 17)     //show dynamic feature line
    };
    
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

    /// Constructors
    ///@param seqalign: seqalign to display. 
    ///@param mask_seqloc: seqloc to be displayed with different characters
    ///and colors such as masked sequence.  Must be seqloc-int
    ///@param external_feature:  Feature to display such as phiblast pattern.
    ///Must be seqloc-int 
    ///@param matrix: customized matrix for computing
    ///positive protein matchs.  Note the matrix must exactly consist of
    ///"ARNDCQEGHILKMFPSTWYVBZX", default matrix is blosum62 
    ///@param scope: scope to fetch your sequence 
    ///
    CDisplaySeqalign(const CSeq_align_set & seqalign,
                     CScope & scope,
                     list < SeqlocInfo * >* mask_seqloc = NULL,
                     list < FeatureInfo * >* external_feature = NULL,
                     const int matrix[][ePMatrixSize] = NULL);
    
    /// Destructor
    ~CDisplaySeqalign();
    
    ///call this to display seqalign
    ///@param out: stream for display
    ///
    void DisplaySeqalign(CNcbiOstream & out);
    
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
    
    /// static functions
    ///Need to call this if the seqalign is stdseg or dendiag for ungapped
    ///blast alignment display as each stdseg ro dendiag is a distinct
    /// alignment.  Don't call it for other case as it's a waste of time.
    ///@param alnset: input alnset
    ///@return processed alnset
    ///
    static CRef < CSeq_align_set >
    PrepareBlastUngappedSeqalign(CSeq_align_set & alnset);
    
private:
    
    ///internal insert information
    ///aln_start. insert right after this position
    struct SInsertInformation {
        int aln_start;              
        int seq_start;
        int insert_len;
    };
    
    /// store alnvec and score info
    struct SAlnInfo {               
        CRef < CAlnVec > alnvec;
        int score;
        double bits;
        double evalue;
        list<int> use_this_gi;
    };

    ///store feature information
    struct SAlnFeatureInfo {
        FeatureInfo *feature;
        string feature_string;
        CRange < TSignedSeqPos > aln_range;
    };
    
    ///store seqloc info
    struct SAlnSeqlocInfo {
        SeqlocInfo *seqloc;
        CRange < TSignedSeqPos > aln_range;
    };

    /// reference to seqalign set
    CConstRef < CSeq_align_set > m_SeqalignSetRef; 
    
    /// display character option for list of seqloc         
    list < SeqlocInfo * >* m_Seqloc; 

    /// external feature such as phiblast
    list < FeatureInfo * >* m_QueryFeature; 
    CScope & m_Scope;
    CAlnVec *m_AV;                  // current aln vector
    int **m_Matrix;                 // matrix used to compute the midline
    int m_AlignOption;              // Display options
    AlignType m_AlignType;          // alignment type, used for displaying
                                    //blast info
    int m_NumAlignToShow;           // number of alignment to display
    SeqLocCharOption m_SeqLocChar;  // character for seqloc display
    SeqLocColorOption m_SeqLocColor; // clolor for seqloc display
    size_t m_LineLen;                  // number of sequence character per line
    bool m_IsDbNa;
    bool m_IsDbGi;
    string m_DbName;
    string m_BlastType;
    string m_Rid;
    string m_CddRid;
    string m_EntrezTerm;
    int m_QueryNumber;
    CNcbiIfstream *m_ConfigFile;
    CNcbiRegistry *m_Reg;
    CGetFeature* m_DynamicFeature;
    map < string, string > m_Segs;
    CRef < CObjectManager > m_FeatObj;  // used for fetching feature
    CRef < CScope > m_featScope;        // used for fetching feature
    MiddleLineStyle m_MidLineStyle;

    ///Display the current alnvec
    ///@param out: stream for display
    ///
    void x_DisplayAlnvec(CNcbiOstream & out);

    ///print defline
    ///@param bsp_handle: bioseq of interest
    ///@param use_this_gi: display this gi instead
    ///@param out: output stream
    ///
    const void x_PrintDefLine(const CBioseq_Handle& bsp_handle, 
                              list<int>& use_this_gi,
                              CNcbiOstream& out) const;

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
    const void x_OutputSeq(string& sequence, const CSeq_id& id, int start, 
                           int len, int frame, int row, bool color_mismatch, 
                           list<SAlnSeqlocInfo*> loc_list, 
                           CNcbiOstream& out) const;
    
    /// Count number of total gaps
    ///@return: number of toal gaps for the current alnvec
    ///
    int x_GetNumGaps();               
    
    ///add linkout url
    ///@param cbsp: bioseq of interest
    ///@param bdl: blast defline structure
    ///@param first_gi: the first gi in redundant sequence
    ///@param gi: the actual gi
    ///@param out: output stream
    ///
    void x_AddLinkout(const CBioseq& cbsp, const CBlast_def_line& bdl,
                      int first_gi, int gi, CNcbiOstream& out) const;

    ///get url to sequence record
    ///@param ids: id list
    ///@param gi: gi or 0 if no gi
    ///@param row: the current row
    ///
    string x_GetUrl(const list < CRef < CSeq_id > >&ids, int gi, int row) const;

    ///get dumpgnl url to sequence record 
    ///@param ids: id list
    ///@param row: the current row
    ///@param alternative_url: user specified url or empty string
    ///
    string x_GetDumpgnlLink(const list < CRef < CSeq_id > >&ids, int row,
                            const string & alternative_url) const;
    
    ///get feature info
    ///@param feature: where feature info to be filled
    ///@param scope: scope to fectch sequence
    ///@param choice: which feature to get
    ///@param row: current row number
    ///@param sequence: the sequence string
    ///
    void x_GetFeatureInfo(list < SAlnFeatureInfo * >&feature, CScope & scope,
                          CSeqFeatData::E_Choice choice, int row,
                          string& sequence) const;
    
    ///get inserts info
    ///@param row: current row
    ///@param aln_range: the alignment range
    ///@param aln_start: start for current row
    ///@param inserts: inserts to be filled
    ///@param insert_pos_string: string to indicate the start of insert
    ///@param insert_list: information containing the insert info
    ///
    void x_FillInserts(int row, CAlnMap::TSignedRange& aln_range, 
                       int aln_start, list < string >& inserts, 
                       string& insert_pos_string,
                       list < SInsertInformation * >& insert_list) const;
    
    ///recusively fill the insert for anchored view
    ///@param row: the row number
    ///@param aln_range: the alignment range
    ///@param aln_start: start for current row
    ///@param insert_list: information containing the insert info
    ///@param inserts: inserts strings to be inserted
    ///
    void x_DoFills(int row, CAlnMap::TSignedRange& aln_range, int aln_start,
                   list < SInsertInformation * >&insert_list,
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
    const void x_FillIdentityInfo(const string& sequence_standard,
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
    void x_SetFeatureInfo(SAlnFeatureInfo* feat_info, const CSeq_loc& seqloc,
                          int aln_from, int aln_to, int aln_stop,
                          char pattern_char,  string pattern_id,
                          string& alternative_feat_str) const;

    ///Set the database as gi type
    ///@param actual_aln_list: the alignment
    ///
    void x_SetDbGi(const CSeq_align_set& actual_aln_list);

    ///get insert information
    ///@param insert_list: list to be filled
    ///@param insert_aln_start: alnment start coordinate info
    ///@param insert_seq_start: alnment sequence start info
    ///@param insert_length: insert length info
    ///@param line_aln_stop: alignment stop for this row
    ///
    void x_GetInserts(list < SInsertInformation * >&insert_list,
                      CAlnMap::TSeqPosList& insert_aln_start,
                      CAlnMap::TSeqPosList& insert_seq_start,
                      CAlnMap::TSeqPosList& insert_length, 
                      int line_aln_stop);

    ///display alnvec list
    ///@param out: output stream
    ///@param av_list: alnvec list
    ///
    void x_DisplayAlnvecList(CNcbiOstream& out, list < SAlnInfo * >& av_list);

    ///output dynamic feature url
    ///@param out: output stream
    ///
    void x_PrintDynamicFeatures(CNcbiOstream& out);

    ///convert the internal seqloc list info using alnment coordinates
    ///@param loc_list: fill the list with seqloc info using aln coordinates
    ///
    void x_FillLocList(list<SAlnSeqlocInfo*>& loc_list) const;

    ///get external query feature info such as phi blast pattern
    ///@param row_num: row number
    ///@param aln_stop: the stop position for the whole alignment
    ///
    list<SAlnFeatureInfo*>* x_GetQueryFeatureList(int row_num, 
                                                 int aln_stop) const;
    ///make the appropriate seqid
    ///@param id: the id to be filled
    ///@param row: row number
    ///
    void x_FillSeqid(string& id, int row) const;

};


/***********************Inlines************************/

END_SCOPE(objects)
END_NCBI_SCOPE

/* 
*===========================================
*$Log$
*Revision 1.27  2005/03/14 15:50:12  jianye
*minor naming change
*
*Revision 1.26  2005/03/02 18:21:17  jianye
*some style fix
*
*Revision 1.25  2005/02/22 15:59:16  jianye
*some style change
*
*Revision 1.24  2005/02/14 19:04:28  jianye
*changed constructor
*
*Revision 1.23  2004/09/27 14:33:28  jianye
*modify use_this_gi logic
*
*Revision 1.22  2004/09/20 18:12:01  jianye
*Handles Disc alignment and some code clean up
*
*Revision 1.21  2004/08/05 16:58:54  jianye
*Added dynamic features
*
*Revision 1.20  2004/07/22 15:43:20  jianye
*Added eShowBl2seqLink
*
*Revision 1.19  2004/07/12 15:22:12  jianye
*Handles use_this_gi in alignment score structure
*
*Revision 1.18  2004/04/14 16:29:46  jianye
*change GetBlastDefline parameter
*
*Revision 1.17  2004/02/10 22:00:05  jianye
*Clean up some defs
*
*Revision 1.16  2003/12/29 18:37:22  jianye
*Added nuc to nuc translation
*
*Revision 1.15  2003/12/22 21:05:55  camacho
*Add const qualifier for Seq-align-set, indent
*
*Revision 1.14  2003/12/11 22:28:16  jianye
*get rid of m_Blosum62
*
*Revision 1.13  2003/12/01 23:15:56  jianye
*Added showing CDR product
*
*Revision 1.12  2003/10/28 22:41:57  jianye
*Added downloading sub seq capability for long seq
*
*Revision 1.11  2003/10/27 20:59:37  jianye
*Added color mismatches capability
*
*Revision 1.10  2003/10/09 15:06:11  jianye
*Change int to enum defs in SeqlocInfo
*
*Revision 1.9  2003/10/08 17:45:13  jianye
*Added translated frame defs
* 

*===========================================
*/
#endif
