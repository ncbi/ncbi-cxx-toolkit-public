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
  struct SeqlocInfo {
    CRef<CSeq_loc> seqloc;
    CRange<TSignedSeqPos> alnRange;
    int frame;
  };
  struct FeatureInfo{
    CConstRef<CSeq_loc> seqloc;
    CRange<TSignedSeqPos> alnRange;
    char featureChar;
    string featureId;
    string featureString;
  };
  struct alnInfo { //store alnvec and score info
      CRef<CAlnVec> alnVec;
      int score;
      double bits;
      double eValue;
    };
    
  static const int m_PMatrixSize = 23;  //number of amino acid symbol in matrix
  static const int m_NumAsciiChar = 128;  //number of ASCII char
  static const int m_NumColor=3;

  enum DisplayOption {
    eHtml = (1 << 0),    //html output. Default text.
    eLinkout = (1 <<1 ),  //linkout gifs. 
    eSequenceRetrieval = (1 << 2),  //Get sequence feature
    eMultiAlign = (1 << 3 ),   //Flat query anchored. Default pairwise
    eShowBarMiddleLine = (1 << 4),  //show "|" as identity between query and hit. 
    eShowCharMiddleLine = (1 << 5),  //show character or "+" as identity between query and hit.
    eShowGi = (1 << 6),
    eShowIdentity = (1 << 7),  //show dot as identity to master
    eShowBlastInfo = (1 << 8),  //show blast type defline and score info for pairwise alignment
    eShowBlastStyleId = (1 << 9),  //show seqid as "Query" and "Sbjct" respectively for pairwise alignment.  Default shows seqid as is
    eNewTargetWindow = (1 << 10),  //clicking url link will open a new window
    eShowCdsFeature = (1 << 11),  //show cds for sequence 
    eShowGeneFeature = (1 << 12), //show gene for sequence
    eMasterAnchored = (1 << 13)  //Query anchored, for multialignment only
  };

  //character used to display seqloc
  enum SeqLocCharOption {
    eX = 0,  //use X to replace sequence character. Default 
    eN, //use n to replace sequence character
    eLowerCase  //lower case 
  };
  //color for seqloc
  enum SeqLocColorOption{
    eBlack = 0,  //Default
    eGrey,
    ePurple
  };
   
  //Alignment display type, specific for showing blast-related info
  enum AlignType {
    eNotSet = 0, //Default
    eNuc = 1,
    eProt
  };
  //Constructors
  /* CSeq_align_set: seqalign to display
     maskSeqloc: seqloc to be displayed with different color
     externalFeature: Feature to display such as phiblast pattern.  Must be seqloc-int
     matrix: customized matrix for computing positive protein matchs.  Note the matrix must exactly consist of "ARNDCQEGHILKMFPSTWYVBZX", default matrix is blosum62
     scope: scope to fetch your sequence
  */
  CDisplaySeqalign(CSeq_align_set& seqalign, list <SeqlocInfo>& maskSeqloc, list <FeatureInfo>& externalFeature, const int matrix[][m_PMatrixSize], CScope& scope);  

  //Destructor
  ~CDisplaySeqalign();

  //Set functions
  void SetAlignOption (int option) {m_AlignOption = option;} //Set according to DsiplayOption
  void SetSeqLocChar (SeqLocCharOption option = eX) {m_SeqLocChar = option;}
  void SetSeqLocColor (SeqLocColorOption option = eBlack) {m_SeqLocColor=option;}
  void SetAlignType (AlignType type) {m_AlignType = type;}  //Needed only if you want to display blast-related information
  void SetLineLen (int len) {m_LineLen = len;}  //number of bases or amino acids per line
  void SetNumAlignToShow (int num) {m_NumAlignToShow = num;}  //Display top num seqalign
  void SetDbName (string name) {m_DbName = name;}  //blastdb name.  
  void SetDbType(bool isNa) {m_IsDbNa = isNa;}  //for seq fetching from blast db
  void SetQueryType(bool isNa) {m_IsQueryNa = isNa;} //type for query sequence
  void SetRid(string rid) {m_Rid = rid;} //blast request id
  void SetCddRid(string cddRid) {m_CddRid = cddRid;}  //CDD rid for constructing linkout
  void SetEntrezTerm(string term) {m_EntrezTerm = term;}  //for constructing structure linkout
  void SetQueryNumber(int number){m_QueryNumber = number;}  //for linking to mapviewer
  void SetBlastType(string type) {m_BlastType = type;} //refer to blobj->adm->trace->created_by

  //display seqalign
  void DisplaySeqalign(CNcbiOstream& out) ;
  
  static CRef<CSeq_id> GetSeqIdByType(const list<CRef<CSeq_id> >& ids, CSeq_id::E_Choice choice);

protected:

  list <SeqlocInfo>& m_Seqloc;  //display character option for list of seqloc 
  list <FeatureInfo>& m_QueryFeature;  //external feature such as phiblast pattern
  CScope& m_Scope;
  CConstRef<CSeq_align_set> m_SeqalignSetRef;  //reference to seqalign set for displaying
 
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

  //helper functions
  void DisplayAlnvec(CNcbiOstream& out);
  const void PrintDefLine(const CBioseq_Handle& bspHandle, CNcbiOstream& out) const;
  const void OutputSeq(string& sequence, const CSeq_id& id, int start, int len, CNcbiOstream& out) const; //display sequence, start is seqalign coodinate

  int getNumGaps();  //Count number of total gaps
  const CRef<CBlast_def_line_set> GetBlastDefline (const CBioseq& cbsp) const;
  void AddLinkout(const CBioseq& cbsp, const CBlast_def_line& bdl, int firstGi,int gi, CNcbiOstream& out) const;
  string getUrl(const list<CRef<CSeq_id> >& ids, int row) const;
  string getDumpgnlLink(const list<CRef<CSeq_id> >& ids, int row) const;
  void getFeatureInfo(list<FeatureInfo*>& feature, CScope& scope, CSeqFeatData::E_Choice choice, int row) const;

  void fillInserts(int row, CAlnMap::TSignedRange& alnRange, int alnStart, list<string>& inserts, string& insertPosString) const;
  void doFills(int row, CAlnMap::TSignedRange& alnRange, int alnStart, list<CConstRef<CAlnMap::CAlnChunk> >& chunks, list<string>& inserts) const;
  string getSegs(int row) const;
  const void fillIdentityInfo(const string& sequenceStandard, const string& sequence , int& match, int& positive, string& middleLine);

};


/***********************Inlines************************/

END_SCOPE(objects)
END_NCBI_SCOPE
#endif
