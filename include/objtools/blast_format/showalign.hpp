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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class NCBI_XALNUTIL_EXPORT CDisplaySeqalign{

public:
  //Defines

  //frame defines for translated alignment
  enum TranslatedFrames{
    eFrameNotSet =0,
    ePlusStrand1 = 1,
    ePlusStrand2 = 2,
    ePlusStrand3 = 3,
    eMinusStrand1 = -1,
    eMinusStrand2 = -2,
    eMinusStrand3 = -3
  };

  //Alignment display type, specific for showing blast-related info
  enum AlignType {
    eNotSet = 0, //Default
    eNuc = 1,
    eProt
  };

  struct SeqlocInfo {
    CRef<CSeq_loc> seqloc; //must be seqloc int
    TranslatedFrames frame;  //For translated nucleotide sequence
  };

  struct FeatureInfo {
    CConstRef<CSeq_loc> seqloc;  //must be seqloc int
    char featureChar;  //Character for feature
    string featureId;  //ID for feature
  };

  static const int m_PMatrixSize = 23;  //number of amino acid symbol in matrix
  static const int m_NumAsciiChar = 128;  //number of ASCII char
  static const int m_NumColor=3;

  enum DisplayOption {
    eHtml = (1 << 0),    //html output. Default text.
    eLinkout = (1 <<1 ),  //linkout gifs. 
    eSequenceRetrieval = (1 << 2),  //Get sequence feature
    eMultiAlign = (1 << 3 ),   //Multiple alignment view. Default pairwise
    eShowMiddleLine = (1 << 4),  //show line that indicates identity between query and hit. 
    eShowGi = (1 << 6),
    eShowIdentity = (1 << 7),  //show dot as identity to master
    eShowBlastInfo = (1 << 8),  //show defline and score info for blast pairwise alignment
    eShowBlastStyleId = (1 << 9),  //show seqid as "Query" and "Sbjct" respectively for pairwise alignment.  Default shows seqid as is
    eNewTargetWindow = (1 << 10),  //clicking url link will open a new window
    eShowCdsFeature = (1 << 11),  //show cds for sequence.  Need to fetch from id server, a bit slow. 
    eShowGeneFeature = (1 << 12), //show gene for sequence.  Need to fetch from id server, a bit slow.
    eMasterAnchored = (1 << 13),  //Query anchored, for multialignment only, default not anchored
    eColorDifferentBases = (1 << 14)  //coloring mismatches for subject seq
  };

  //Need to set eShowMiddleLine to get this
  enum MiddleLineStyle {
    eChar = 0, //show character as identity between query and hit. Default
    eBar       //show bar as identity between query and hit
  };

  //character used to display seqloc, such as masked sequence
  enum SeqLocCharOption {
    eX = 0,  //use X to replace sequence character. Default 
    eN, //use n to replace sequence character
    eLowerCase  //use lower case of the original sequence letter
  };
  //color for seqloc
  enum SeqLocColorOption{
    eBlack = 0,  //Default
    eGrey,
    ePurple
  };
   

  //Constructors
  /* CSeq_align_set: seqalign to display.
     maskSeqloc: seqloc to be displayed with different characters such as masked sequence.  Must be seqloc-int
     externalFeature: Feature to display such as phiblast pattern.  Must be seqloc-int
     matrix: customized matrix for computing positive protein matchs.  Note the matrix must exactly consist of "ARNDCQEGHILKMFPSTWYVBZX", default matrix is blosum62
     scope: scope to fetch your sequence
  */
  CDisplaySeqalign(CSeq_align_set& seqalign, list <SeqlocInfo*>& maskSeqloc, list <FeatureInfo*>& externalFeature, const int matrix[][m_PMatrixSize], CScope& scope);  

  //Destructor
  ~CDisplaySeqalign();

  //Set functions
  /*These are for all alignment display*/
  void SetAlignOption (int option) {m_AlignOption = option;} //Set according to DsiplayOption
  void SetSeqLocChar (SeqLocCharOption option = eX) {m_SeqLocChar = option;}
  void SetSeqLocColor (SeqLocColorOption option = eBlack) {m_SeqLocColor=option;}
  void SetLineLen (int len) {m_LineLen = len;}  //number of bases or amino acids per line
  void SetNumAlignToShow (int num) {m_NumAlignToShow = num;}  //Display top num seqalign
  void SetMiddleLineStyle (MiddleLineStyle option = eBar) {m_MidLineStyle = option;}

  /*These are for blast alignment style display only*/
  void SetAlignType (AlignType type) {m_AlignType = type;}  //Needed only if you want to display positives and strand  
  void SetDbName (string name) {m_DbName = name;}  //blastdb name.  
  void SetDbType(bool isNa) {m_IsDbNa = isNa;}  //for seq fetching from blast db
  void SetQueryType(bool isNa) {m_IsQueryNa = isNa;} //type for query sequence
  void SetRid(string rid) {m_Rid = rid;} //blast request id
  void SetCddRid(string cddRid) {m_CddRid = cddRid;}  //CDD rid for constructing linkout
  void SetEntrezTerm(string term) {m_EntrezTerm = term;}  //for constructing structure linkout
  void SetQueryNumber(int number){m_QueryNumber = number;}  //for linking to mapviewer
  void SetBlastType(string type) {m_BlastType = type;} //refer to blobj->adm->trace->created_by

  //static
  /*Need to call this if the seqalign is stdseg or dendiag for ungapped blast alignment display as each stdseg ro dendiag is a distinct alignment.  Don't call it for other case as it's a waste of time.*/
  static  CRef<CSeq_align_set> PrepareBlastUngappedSeqalign(CSeq_align_set& alnset);
  //display seqalign
  void DisplaySeqalign(CNcbiOstream& out) ;
  
 
private:
  
  
  struct insertInformation{
    int alnStart;  //aln coords. insert right after this position
    int seqStart;
    int insertLen;
  };

  struct alnInfo { //store alnvec and score info
    CRef<CAlnVec> alnVec;
    int score;
    double bits;
    double eValue;
  };
   
  struct alnFeatureInfo {
    FeatureInfo* feature;
    string featureString;
    CRange<TSignedSeqPos> alnRange;
  };

  struct alnSeqlocInfo {
    SeqlocInfo* seqloc;
    CRange<TSignedSeqPos> alnRange;
  };
  CConstRef<CSeq_align_set> m_SeqalignSetRef;  //reference to seqalign set for displaying
  list <SeqlocInfo*>& m_Seqloc;  //display character option for list of seqloc 
  list <FeatureInfo*>& m_QueryFeature;  //external feature such as phiblast pattern
  CScope& m_Scope;
 
  CAlnVec* m_AV;  //current aln vector
  int** m_Matrix;  //matrix used to compute the midline
  static const char m_PSymbol[m_PMatrixSize+1]; //Amino acid symbol
  static const int m_Blosum62[m_PMatrixSize][m_PMatrixSize];  //BLOSUM62 matrix
  int m_AlignOption;  //Display options
  static const string color[m_NumColor];
  AlignType m_AlignType;  //alignment type, used for displaying blast info
  int m_NumAlignToShow;  //number of alignment to display
  SeqLocCharOption m_SeqLocChar; //character for seqloc display
  SeqLocColorOption m_SeqLocColor;  //clolor for seqloc display
  int m_LineLen;  //number of sequence character per line
  int m_IdStartMargin;  //margin between seqid and start number
  int m_StartSequenceMargin;  //margin between start number and sequence
  int m_SeqStopMargin;  //margin between sequence and stop number
  bool m_IsDbNa;
  bool m_IsQueryNa;
  bool m_IsDbGi;
  string m_DbName;
  string m_BlastType;
  string m_Rid;
  string m_CddRid;
  string m_EntrezTerm;  
  int m_QueryNumber;
  CNcbiIfstream* m_ConfigFile;
  CNcbiRegistry* m_Reg;
  map<string, string> m_Segs;
  CRef<CObjectManager> m_FeatObj; //used for fetching feature
  CRef<CScope> m_featScope; //used for fetching feature
  list<alnSeqlocInfo*> m_Alnloc; //seqloc display info (i.e., mask) for current alnvec
  MiddleLineStyle m_MidLineStyle;
  //helper functions
  void DisplayAlnvec(CNcbiOstream& out);
  const void PrintDefLine(const CBioseq_Handle& bspHandle, CNcbiOstream& out) const;
  const void OutputSeq(string& sequence, const CSeq_id& id, int start, int len, int frame, bool colorMismatch, CNcbiOstream& out) const; //display sequence, start is seqalign coodinate

  int getNumGaps();  //Count number of total gaps
  const CRef<CBlast_def_line_set> GetBlastDefline (const CBioseq& cbsp) const;
  void AddLinkout(const CBioseq& cbsp, const CBlast_def_line& bdl, int firstGi,int gi, CNcbiOstream& out) const;
  string getUrl(const list<CRef<CSeq_id> >& ids, int row) const;
  string getDumpgnlLink(const list<CRef<CSeq_id> >& ids, int row, const string& alternativeUrl) const;
  void getFeatureInfo(list<alnFeatureInfo*>& feature, CScope& scope, CSeqFeatData::E_Choice choice, int row) const;

  void fillInserts(int row, CAlnMap::TSignedRange& alnRange, int alnStart, list<string>& inserts, string& insertPosString, list<insertInformation*>& insertList) const;
  void doFills(int row, CAlnMap::TSignedRange& alnRange, int alnStart, list<insertInformation*>& insertList, list<string>& inserts) const;
  string getSegs(int row) const;
  const void fillIdentityInfo(const string& sequenceStandard, const string& sequence , int& match, int& positive, string& middleLine);
  void setFeatureInfo(alnFeatureInfo* featInfo, const CSeq_loc& seqloc, int alnFrom, int alnTo, int alnStop, char patternChar, string patternId) const;  
  void setDbGi();
  void GetInserts(list<insertInformation*>& insertList, CAlnMap::TSeqPosList& insertAlnStart, CAlnMap::TSeqPosList& insertSeqStart, CAlnMap::TSeqPosList& insertLength,  int lineAlnStop);
  void x_DisplayAlnvecList(CNcbiOstream& out, list<alnInfo*>& avList);
  
};


/***********************Inlines************************/

END_SCOPE(objects)
END_NCBI_SCOPE

/* 
*===========================================
*$Log$
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
