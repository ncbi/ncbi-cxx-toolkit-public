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
 *
 * File Description:
 *   Sequence alignment display
 *
 */

#include <corelib/ncbireg.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objtools/alnmgr/alnvec.hpp>
#include <objects/blastdb/Blast_def_line_set.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objtools/readers/getfeature.hpp>

BEGIN_NCBI_SCOPE BEGIN_SCOPE(objects)

  
class NCBI_XALNUTIL_EXPORT CDisplaySeqalign {

  public:
    // Defines

    // frame defines for translated alignment
    enum TranslatedFrames {
        eFrameNotSet = 0,
        ePlusStrand1 = 1,
        ePlusStrand2 = 2,
        ePlusStrand3 = 3,
        eMinusStrand1 = -1,
        eMinusStrand2 = -2,
        eMinusStrand3 = -3
    };

    // Alignment display type, specific for showing blast-related info
    enum AlignType {
        eNotSet = 0,            // Default
        eNuc = 1,
        eProt = 2
    };

    struct SeqlocInfo {
        CRef < CSeq_loc > seqloc;       // must be seqloc int
        TranslatedFrames frame; // For translated nucleotide sequence
    };

    struct FeatureInfo {
        CConstRef < CSeq_loc > seqloc;  // must be seqloc int
        char featureChar;       // Character for feature
        string featureId;       // ID for feature
    };

    enum {
        kPMatrixSize = 23       // number of amino acid for matrix
    };

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
        eTranslateNucToNucAlignment = (1 << 15), //Show nuecleotide to nucleotide
                                                //alignment as translated
        eShowBl2seqLink = (1 << 16),    // Show web link to bl2seq
        eDynamicFeature = (1 << 17)     //show dynamic feature line
    };

    // Need to set eShowMiddleLine to get this
    enum MiddleLineStyle {
        eChar = 0,              // show character as identity between query
                                // and hit. Default
        eBar                    // show bar as identity between query and hit
    };

    // character used to display seqloc, such as masked sequence
    enum SeqLocCharOption {
        eX = 0,                 // use X to replace sequence character.
                                // Default 
        eN,                     // use n to replace sequence character
        eLowerCase              // use lower case of the original sequence
                                // letter
    };
    // color for seqloc
    enum SeqLocColorOption {
        eBlack = 0,             // Default
        eGrey,
        eRed
    };


    // Constructors
    /* CSeq_align_set: seqalign to display. 
       maskSeqloc: seqloc to be displayed with different characters such as
         masked sequence.  Must be seqloc-int
       externalFeature: Feature to display such as phiblast pattern.
         Must be seqloc-int 
       matrix: customized matrix for computing
         positive protein matchs.  Note the matrix must exactly consist of
        "ARNDCQEGHILKMFPSTWYVBZX", default matrix is blosum62 
      scope: scope to fetch your sequence */
    CDisplaySeqalign(const CSeq_align_set & seqalign,
                     CScope & scope,
                     list < SeqlocInfo * >* maskSeqloc = NULL,
                     list < FeatureInfo * >* externalFeature = NULL,
                     const int matrix[][kPMatrixSize] = NULL);

    // Destructor
    ~CDisplaySeqalign();

    // Set functions
    /* These are for all alignment display */
    // Set according to DsiplayOption
    void SetAlignOption(int option)
    {
        m_AlignOption = option;
    } 
    void SetSeqLocChar(SeqLocCharOption option = eX) {
        m_SeqLocChar = option;
    }

    void SetSeqLocColor(SeqLocColorOption option = eBlack) {
        m_SeqLocColor = option;
    }
      // number of bases or amino acids per line
    void SetLineLen(int len) {
        m_LineLen = len;
    }

      // Display top num seqalign
    void SetNumAlignToShow(int num) {
        m_NumAlignToShow = num;
    }

    void SetMiddleLineStyle(MiddleLineStyle option = eBar) {
        m_MidLineStyle = option;
    }

      /* These are for blast alignment style display only */
      // Needed only if you want to display positives and strand 
    void SetAlignType(AlignType type) {
        m_AlignType = type;
    }

      // blastdb name.  
    void SetDbName(string name) {
        m_DbName = name;
    }

      // for seq fetching from blast db
    void SetDbType(bool isNa) {
        m_IsDbNa = isNa;
    }

      // type for query sequence
    void SetQueryType(bool isNa) {
        m_IsQueryNa = isNa;
    }

      // blast request id
    void SetRid(string rid) {
        m_Rid = rid;
    }

      // CDD rid for constructing linkout
    void SetCddRid(string cddRid) {
        m_CddRid = cddRid;
    }

      // for constructing structure linkout
    void SetEntrezTerm(string term) {
        m_EntrezTerm = term;
    }

      // for linking to mapviewer
    void SetQueryNumber(int number) {
        m_QueryNumber = number;
    }

      // refer to blobj->adm->trace->created_by
    void SetBlastType(string type) {
        m_BlastType = type;
    }

      // static
      /* Need to call this if the seqalign is stdseg or dendiag for ungapped
         blast alignment display as each stdseg ro dendiag is a distinct
         alignment.  Don't call it for other case as it's a waste of time. */
    static CRef < CSeq_align_set >
        PrepareBlastUngappedSeqalign(CSeq_align_set & alnset);
      // display seqalign
    void DisplaySeqalign(CNcbiOstream & out);


private:
    
    struct insertInformation {
        int alnStart;               // aln coords. insert right after this
                                    // position
        int seqStart;
        int insertLen;
    };

    struct alnInfo {                // store alnvec and score info
        CRef < CAlnVec > alnVec;
        int score;
        double bits;
        double eValue;
        list<int> use_this_gi;
    };

    struct alnFeatureInfo {
        FeatureInfo *feature;
        string featureString;
        CRange < TSignedSeqPos > alnRange;
    };

    struct alnSeqlocInfo {
        SeqlocInfo *seqloc;
        CRange < TSignedSeqPos > alnRange;
    };
    CConstRef < CSeq_align_set > m_SeqalignSetRef;  // reference to seqalign set
                                                    // for displaying
    list < SeqlocInfo * >* m_Seqloc; // display character option for list of
                                    // seqloc 
    list < FeatureInfo * >* m_QueryFeature;  // external feature such as phiblast
                                            // pattern
    CScope & m_Scope;
    CAlnVec *m_AV;                  // current aln vector
    int **m_Matrix;                 // matrix used to compute the midline
    int m_AlignOption;              // Display options
    AlignType m_AlignType;          // alignment type, used for displaying
                                    //blast info
    int m_NumAlignToShow;           // number of alignment to display
    SeqLocCharOption m_SeqLocChar;  // character for seqloc display
    SeqLocColorOption m_SeqLocColor;        // clolor for seqloc display
    int m_LineLen;                  // number of sequence character per line
    bool m_IsDbNa;
    bool m_IsQueryNa;
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
    CRef < CObjectManager > m_FeatObj;      // used for fetching feature
    CRef < CScope > m_featScope;    // used for fetching feature
    
    MiddleLineStyle m_MidLineStyle;
      // helper functions
    void DisplayAlnvec(CNcbiOstream & out);
    const void PrintDefLine(const CBioseq_Handle & bspHandle, 
                            list<int>& use_this_gi,
                            CNcbiOstream & out) const;
    // display sequence, start is seqalign coodinate
    const void OutputSeq(string & sequence, const CSeq_id & id, int start, 
                         int len, int frame, int row, bool colorMismatch, 
                         list<alnSeqlocInfo*> loc_list, 
                         CNcbiOstream & out) const;

    int getNumGaps();               // Count number of total gaps
    const CRef < CBlast_def_line_set >
        GetBlastDefline(const CBioseq_Handle& handle) const;
    void AddLinkout(const CBioseq & cbsp, const CBlast_def_line & bdl,
                    int firstGi, int gi, CNcbiOstream & out) const;
    string getUrl(const list < CRef < CSeq_id > >&ids, int gi, int row) const;
    string getDumpgnlLink(const list < CRef < CSeq_id > >&ids, int row,
                          const string & alternativeUrl) const;
    void getFeatureInfo(list < alnFeatureInfo * >&feature, CScope & scope,
                        CSeqFeatData::E_Choice choice, int row,
                        string & sequence) const;

    void fillInserts(int row, CAlnMap::TSignedRange & alnRange, int alnStart,
                     list < string > &inserts, string & insertPosString,
                     list < insertInformation * >&insertList) const;
    void doFills(int row, CAlnMap::TSignedRange & alnRange, int alnStart,
                 list < insertInformation * >&insertList,
                 list < string > &inserts) const;
    string getSegs(int row) const;
    const void fillIdentityInfo(const string & sequenceStandard,
                                const string & sequence, int &match,
                                int &positive, string & middleLine);
    void setFeatureInfo(alnFeatureInfo * featInfo, const CSeq_loc & seqloc,
                        int alnFrom, int alnTo, int alnStop, char patternChar,
                        string patternId, string & alternativeFeatStr) const;
    void setDbGi(const CSeq_align_set& actual_aln_list);
    void GetInserts(list < insertInformation * >&insertList,
                    CAlnMap::TSeqPosList & insertAlnStart,
                    CAlnMap::TSeqPosList & insertSeqStart,
                    CAlnMap::TSeqPosList & insertLength, int lineAlnStop);
    void x_DisplayAlnvecList(CNcbiOstream & out, list < alnInfo * >&avList);
    void x_PrintDynamicFeatures(CNcbiOstream& out);
    void x_FillLocList(list<alnSeqlocInfo*>& loc_list) const;
    list<alnFeatureInfo*>* x_GetQueryFeatureList(int row_num, int aln_stop) const;
    void x_FillSeqid(string& id, int row) const;

};


/***********************Inlines************************/

END_SCOPE(objects)
END_NCBI_SCOPE

/* 
*===========================================
*$Log$
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
