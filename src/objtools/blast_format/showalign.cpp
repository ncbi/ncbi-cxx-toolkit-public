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

#include <objtools/alnmgr/util/showalign.hpp>

#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbireg.hpp>

#include <util/range.hpp>
#include <util/md5.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>

#include <serial/iterator.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>
#include <serial/objostrasnb.hpp> 
#include <serial/objistrasnb.hpp> 
#include <connect/ncbi_conn_stream.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/gbloader.hpp>
#include <objmgr/reader_id1.hpp>

#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature.hpp>

#include <objects/seqfeat/SeqFeatData.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Bioseq.hpp>

#include <objects/seqset/Seq_entry.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Score.hpp>

#include <objtools/alnmgr/alnmix.hpp>
#include <objtools/alnmgr/alnvec.hpp>

#include <objects/blastdb/Blast_def_line.hpp>
#include <objects/blastdb/Blast_def_line_set.hpp>
#include <objects/blastdb/defline_extra.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE (objects)
USING_SCOPE (sequence);


#define ENTREZ_URL "<a href=\"http://www.ncbi.nlm.nih.gov/entrez/query.fcgi?cmd=Retrieve&db=%s&list_uids=%ld&dopt=%s\" %s>"
#define TRACE_URL  "<a href=\"http://www.ncbi.nlm.nih.gov/Traces/trace.cgi?val=%s&cmd=retrieve&dopt=fasta\">"

/* url for linkout*/
#define URL_LocusLink "<a href=\"http://www.ncbi.nlm.nih.gov/LocusLink/list.cgi?Q=%d%s\"><img border=0 height=16 width=16 src=\"/blast/images/L.gif\" alt=\"LocusLink info\"></a>"
#define URL_Unigene "<a href=\"http://www.ncbi.nlm.nih.gov/entrez/query.fcgi?db=unigene&cmd=search&term=%d[Nucleotide+UID]\"><img border=0 height=16 width=16 src=\"/blast/images/U.gif\" alt=\"UniGene info\"></a>"

#define URL_Structure "<a href=\"http://www.ncbi.nlm.nih.gov/Structure/cblast/cblast.cgi?blast_RID=%s&blast_rep_gi=%d&hit=%d&blast_CD_RID=%s&blast_view=%s&hsp=0&taxname=%s&client=blast\"><img border=0 height=16 width=16 src=\"http://www.ncbi.nlm.nih.gov/Structure/cblast/str_link.gif\" alt=\"Related structures\"></a>"

#define URL_Structure_Overview "<a href=\"http://www.ncbi.nlm.nih.gov/Structure/cblast/cblast.cgi?blast_RID=%s&blast_rep_gi=%d&hit=%d&blast_CD_RID=%s&blast_view=%s&hsp=0&taxname=%s&client=blast\">Related Structures</a>"

#define URL_Geo "<a href=\"http://www.ncbi.nlm.nih.gov/entrez/query.fcgi?db=geo&term=%d[gi]\"><img border=0 height=16 width=16 src=\"/blast/images/G.gif\" alt=\"Geo\"></a>"

const int CDisplaySeqalign::m_NumColor;
const string CDisplaySeqalign::color[m_NumColor]={"#000000", "#808080", "#5A0052"};
const int CDisplaySeqalign::m_PMatrixSize;
const char CDisplaySeqalign::m_PSymbol[m_PMatrixSize+1] = "ARNDCQEGHILKMFPSTWYVBZX";
const int CDisplaySeqalign::m_Blosum62[m_PMatrixSize][m_PMatrixSize] = {
  { 4,-1,-2,-2, 0,-1,-1, 0,-2,-1,-1,-1,-1,-2,-1, 1, 0,-3,-2, 0,-2,-1, 0 },
   {-1, 5, 0,-2,-3, 1, 0,-2, 0,-3,-2, 2,-1,-3,-2,-1,-1,-3,-2,-3,-1, 0,-1 },
   {-2, 0, 6, 1,-3, 0, 0, 0, 1,-3,-3, 0,-2,-3,-2, 1, 0,-4,-2,-3, 3, 0,-1 },
   {-2,-2, 1, 6,-3, 0, 2,-1,-1,-3,-4,-1,-3,-3,-1, 0,-1,-4,-3,-3, 4, 1,-1 },
   { 0,-3,-3,-3, 9,-3,-4,-3,-3,-1,-1,-3,-1,-2,-3,-1,-1,-2,-2,-1,-3,-3,-2 },
   {-1, 1, 0, 0,-3, 5, 2,-2, 0,-3,-2, 1, 0,-3,-1, 0,-1,-2,-1,-2, 0, 3,-1 },
   {-1, 0, 0, 2,-4, 2, 5,-2, 0,-3,-3, 1,-2,-3,-1, 0,-1,-3,-2,-2, 1, 4,-1 },
   { 0,-2, 0,-1,-3,-2,-2, 6,-2,-4,-4,-2,-3,-3,-2, 0,-2,-2,-3,-3,-1,-2,-1 },
   {-2, 0, 1,-1,-3, 0, 0,-2, 8,-3,-3,-1,-2,-1,-2,-1,-2,-2, 2,-3, 0, 0,-1 },
   {-1,-3,-3,-3,-1,-3,-3,-4,-3, 4, 2,-3, 1, 0,-3,-2,-1,-3,-1, 3,-3,-3,-1 },
   {-1,-2,-3,-4,-1,-2,-3,-4,-3, 2, 4,-2, 2, 0,-3,-2,-1,-2,-1, 1,-4,-3,-1 },
   {-1, 2, 0,-1,-3, 1, 1,-2,-1,-3,-2, 5,-1,-3,-1, 0,-1,-3,-2,-2, 0, 1,-1 },
   {-1,-1,-2,-3,-1, 0,-2,-3,-2, 1, 2,-1, 5, 0,-2,-1,-1,-1,-1, 1,-3,-1,-1 },
   {-2,-3,-3,-3,-2,-3,-3,-3,-1, 0, 0,-3, 0, 6,-4,-2,-2, 1, 3,-1,-3,-3,-1 },
   {-1,-2,-2,-1,-3,-1,-1,-2,-2,-3,-3,-1,-2,-4, 7,-1,-1,-4,-3,-2,-2,-1,-2 },
   { 1,-1, 1, 0,-1, 0, 0, 0,-1,-2,-2, 0,-1,-2,-1, 4, 1,-3,-2,-2, 0, 0, 0 },
   { 0,-1, 0,-1,-1,-1,-1,-2,-2,-1,-1,-1,-1,-2,-1, 1, 5,-2,-2, 0,-1,-1, 0 },
   {-3,-3,-4,-4,-2,-2,-3,-2,-2,-3,-2,-3,-1, 1,-4,-3,-2,11, 2,-3,-4,-3,-2 },
   {-2,-2,-2,-3,-2,-1,-2,-3, 2,-1,-1,-2,-1, 3,-3,-2,-2, 2, 7,-1,-3,-2,-1 },
   { 0,-3,-3,-3,-1,-2,-2,-3,-3, 3, 1,-2, 1,-1,-2,-2, 0,-3,-1, 4,-3,-2,-1 },
   {-2,-1, 3, 4,-3, 0, 1,-1, 0,-3,-4, 0,-3,-3,-2, 0,-1,-4,-3,-3, 4, 1,-1 },
   {-1, 0, 0, 1,-3, 3, 4,-2, 0,-3,-3, 1,-1,-3,-1, 0,-1,-3,-2,-2, 1, 4,-1 },
   { 0,-1,-1,-1,-2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-2, 0, 0,-2,-1,-1,-1,-1,-1 },
  };

//Constructor
CDisplaySeqalign::CDisplaySeqalign(CSeq_align_set& seqalign, list <SeqlocInfo>& maskSeqloc, list <FeatureInfo>& externalFeature, const int matrix[][m_PMatrixSize], CScope& scope) : m_SeqalignSetRef(&seqalign), m_Seqloc(maskSeqloc), m_QueryFeature(externalFeature), m_Scope(scope) {
  m_AlignOption = 0;
  m_SeqLocChar = eX;
  m_SeqLocColor = eBlack;
  m_LineLen = 60;
  m_IdStartMargin = 2;
  m_StartSequenceMargin = 2;
  m_SeqStopMargin = 2;
  m_IsDbNa = true;
  m_IsQueryNa = true;
  m_IsDbGi = false;
  m_DbName = NcbiEmptyString;
  m_NumAlignToShow = 100000;
  m_AlignType = eNotSet;
  m_Rid = "0";
  m_CddRid = "0";
  m_EntrezTerm = NcbiEmptyString;
  m_QueryNumber = 0;
  m_BlastType = NcbiEmptyString;

  const int (*matrixToUse)[m_PMatrixSize];
  if(!matrix){
    matrixToUse = m_Blosum62;
  } else {
    matrixToUse = matrix;
  }
  int** temp = new int*[m_NumAsciiChar];
  for(int i = 0; i<m_NumAsciiChar; ++i) {
    temp[i] = new int[m_NumAsciiChar];
  }
  for (int i=0; i<m_NumAsciiChar; i++){
    for (int j=0; j<m_NumAsciiChar; j++){
      temp[i][j] = -1000;
    }
  }
  for(int i = 0; i < m_PMatrixSize; ++i){
    for(int j = 0; j < m_PMatrixSize; ++j){
      temp[m_PSymbol[i]][m_PSymbol[j]] = matrixToUse[i][j];
     
    }
  }
  for(int i = 0; i < m_PMatrixSize; ++i) {
    temp[m_PSymbol[i]]['*'] = temp['*'][m_PSymbol[i]] = -4;
  }
  temp['*']['*'] = 1;
 
  m_Matrix = temp;
  //set config file
  m_ConfigFile = new CNcbiIfstream(".ncbirc");
  m_Reg = new CNcbiRegistry(*m_ConfigFile);
}

//Destructor
CDisplaySeqalign::~CDisplaySeqalign(){
  for(int i = 0; i<m_NumAsciiChar; ++i) {
    delete [] m_Matrix[i];
  }
  delete [] m_Matrix;
  delete m_ConfigFile;
  delete m_Reg;
}

static void AddSpace(CNcbiOstream& out, int number);

static string GetTaxNames(const CBioseq& cbsp, int taxid);

static string getNameInitials(string& name);
static string GetSeqForm(char* formName, bool dbIsNa);
static const string GetSeqIdStringByFastaOrder(const CSeq_id& id, CScope& sp, bool with_version);
static int GetGiForSeqIdList (const list<CRef<CSeq_id> >& ids);
static void setFeatureInfo(CDisplaySeqalign::FeatureInfo* featInfo, const CSeq_loc& seqloc, int alnFrom, int alnTo, int alnStop, char patternChar, string patternId);
static string MakeURLSafe(char* src);
static void getAlnScores(const CSeq_align& aln, int& score, double& bits, double& evalue);

static void getAlnScores(const CSeq_align& aln, int& score, double& bits, double& evalue){
  for (CSeq_align::TScore::const_iterator iter = aln.GetScore().begin(); iter != aln.GetScore().end(); iter++){
	    
    const CObject_id& id=(*iter)->GetId();
    if (id.IsStr()) {
      if (id.GetStr()=="score"){
	score = (*iter)->GetValue().GetInt();
	
      } else if (id.GetStr()=="bit_score"){
	bits = (*iter)->GetValue().GetReal();
	
      } else if (id.GetStr()=="e_value" || id.GetStr()=="sum_e") {
	evalue = (*iter)->GetValue().GetReal();
      } 
    }          
  }
}

string CDisplaySeqalign::getUrl(const list<CRef<CSeq_id> >& ids, int row) const{
  string urlLink = NcbiEmptyString;
 
  char dopt[32], db[32];
  int gi = GetGiForSeqIdList(ids);
  string toolUrl= m_Reg->Get(m_BlastType, "TOOL_URL");
  if(toolUrl == NcbiEmptyString || (gi > 0 && toolUrl.find("dumpgnl.cgi") != string::npos)){ //use entrez or dbtag specified
    if(m_IsDbNa) {
      strcpy(dopt, "GenBank");
      strcpy(db, "Nucleotide");
    } else {
      strcpy(dopt, "GenPept");
      strcpy(db, "Protein");
    }    
 
    char urlBuf[1024];
    if (gi > 0) {
      sprintf(urlBuf, ENTREZ_URL, db, gi, dopt, (m_AlignOption & eNewTargetWindow) ? "TARGET=\"EntrezView\"" : "");
      urlLink = urlBuf;
    } else {//seqid general, dbtag specified
      const CRef<CSeq_id> wid = FindBestChoice(ids, CSeq_id::WorstRank);
      if(wid->Which() == CSeq_id::e_General){
        const CDbtag& dtg = wid->GetGeneral();
        const string& dbName = dtg.GetDb();
        if(NStr::CompareNocase(dbName, "TI") == 0){
          sprintf(urlBuf, TRACE_URL, wid->GetSeqIdString().c_str());
          urlLink = urlBuf;
        } else { //future use

        }
      }
    }
  } else { //need to use url in configuration file
   
      urlLink = getDumpgnlLink(ids, row);
  }
  return urlLink;
}

static string getNameInitials(string& name){
  vector<string> arr;
  string initials;
  NStr::Tokenize(name, " ", arr);

  for(vector<string>::iterator iter = arr.begin(); iter != arr.end(); iter ++){
    if (*iter != NcbiEmptyString){
      initials += (*iter)[0];
    }
  }
  return initials;
}

static void AddSpace(CNcbiOstream& out, int number){
  for(int i=0; i<number; i++){
    out<<" ";
  }
}


void CDisplaySeqalign::AddLinkout(const CBioseq& cbsp, const CBlast_def_line& bdl, int firstGi, int gi, CNcbiOstream& out) const{

  char molType[8]={""};
  if(cbsp.IsAa()){
    sprintf(molType, "[pgi]");
  }
  else {
    sprintf(molType, "[ngi]");
  }
 
  if (bdl.IsSetLinks()){
    for (list< int >::const_iterator iter = bdl.GetLinks().begin(); iter != bdl.GetLinks().end(); iter ++){
      char buf[1024];
      if((*iter) & eLocuslink){
        sprintf(buf, URL_LocusLink, gi, molType);
        out << buf;
      }
      if ((*iter) & eUnigene) {
	sprintf(buf, URL_Unigene,  gi);
	out << buf;
      }
      if ((*iter) & eStructure){
        sprintf(buf, URL_Structure, m_Rid.c_str(), firstGi, gi, m_CddRid.c_str(), "onepair", (m_EntrezTerm == NcbiEmptyString) ? "none":((char*) m_EntrezTerm.c_str()));
	out << buf;
      }
    }
  }
}

//return the get sequence table for html display
static string GetSeqForm(char* formName, bool dbIsNa){
  char buf[2048] = {""};
  if(formName){
    sprintf(buf, "<table border=\"0\"><tr><td><FORM  method=\"post\" action=\"http://www.ncbi.nlm.nih.gov:80/entrez/query.fcgi?SUBMIT=y\" name=\"%s\"><input type=button value=\"Get selected sequences\" onClick=\"finalSubmit(%d, 'getSeqAlignment', 'getSeqGi', '%s')\"><input type=\"hidden\" name=\"db\" value=\"\"><input type=\"hidden\" name=\"term\" value=\"\"><input type=\"hidden\" name=\"doptcmdl\" value=\"docsum\"><input type=\"hidden\" name=\"cmd\" value=\"search\"></form></td><td><FORM><input type=\"button\" value=\"Select all\" onClick=\"handleCheckAll('select', 'getSeqAlignment', 'getSeqGi')\"></form></td><td><FORM><input type=\"button\" value=\"Deselect all\" onClick=\"handleCheckAll('deselect', 'getSeqAlignment', 'getSeqGi')\"></form></td></tr></table>", formName, dbIsNa?1:0, formName);
   
  }
  return buf;
}

//Return the seqid in "fasta" orders
static const string GetSeqIdStringByFastaOrder(const CSeq_id& id, CScope& sp, bool with_version){
  string idString = NcbiEmptyString;
  static const int total_seqid_types=19;
  static int fasta_order[total_seqid_types];
  //fasta order.  See seqidwrite() in C library
  fasta_order[CSeq_id::e_not_set]=33;
  fasta_order[CSeq_id::e_Local]=20;
  fasta_order[CSeq_id::e_Gibbsq]=15;
  fasta_order[CSeq_id::e_Gibbmt]=16;
  fasta_order[CSeq_id::e_Giim]=30;
  fasta_order[CSeq_id::e_Genbank]=10;   
  fasta_order[CSeq_id::e_Embl]=10;
  fasta_order[CSeq_id::e_Pir]=10;
  fasta_order[CSeq_id::e_Swissprot]=10;
  fasta_order[CSeq_id::e_Patent]=15;
  fasta_order[CSeq_id::e_Other]=12;
  fasta_order[CSeq_id::e_General]=13;
  fasta_order[CSeq_id::e_Gi]=255;
  fasta_order[CSeq_id::e_Ddbj]=10;
  fasta_order[CSeq_id::e_Prf]=10;
  fasta_order[CSeq_id::e_Pdb]=12;
  fasta_order[CSeq_id::e_Tpg]=10;
  fasta_order[CSeq_id::e_Tpe]=10;
  fasta_order[CSeq_id::e_Tpd]=10;
 
  const CSeq_id* sip;
  const CBioseq* bioseq=&sp.GetBioseqHandle(id).GetBioseq();
  if(bioseq){
    for (CBioseq::TId::const_iterator id_it = bioseq->GetId().begin(); id_it!=bioseq->GetId().end(); ++id_it){
      if(id_it == bioseq->GetId().begin()){
	sip=*id_it;
      }
      else if(fasta_order[(*id_it)->Which()]<fasta_order[sip->Which()]){
	sip=*id_it;
      }
      
    }
    idString = sip->GetSeqIdString(with_version);
  }
  return idString;
}

CRef<CSeq_id> CDisplaySeqalign::GetSeqIdByType(const list<CRef<CSeq_id> >& ids, CSeq_id::E_Choice choice) {
  CRef<CSeq_id> cid;

  for (CBioseq::TId::const_iterator iter = ids.begin(); iter != ids.end(); iter ++){
    if ((*iter)->Which() == choice){
      cid = *iter;
      break;
    }
  }
 
  return cid;
}

static int GetGiForSeqIdList (const list<CRef<CSeq_id> >& ids){
  int gi = 0;
  CRef<CSeq_id> id = CDisplaySeqalign::GetSeqIdByType(ids, CSeq_id::e_Gi);
  if (!(id.Empty())){
    return id->GetGi();
  }
  return gi;
}

//To display the seqalign represented by internal alnvec
void CDisplaySeqalign::DisplayAlnvec(CNcbiOstream& out){ 
    int maxIdLen=0;
  int maxStartLen=0;
  int startLen=0;
  int actualLineLen=0;
  int aln_stop=m_AV->GetAlnStop();
  const int rowNum=m_AV->GetNumRows();
 
  if(m_AlignOption & eMasterAnchored){
    m_AV->SetAnchor(0);
  }
  m_AV->SetGapChar('-');
  m_AV->SetEndChar(' ');
  vector<string> sequence(rowNum);

  CAlnMap::TSeqPosList*  seqStarts = new CAlnMap::TSeqPosList[rowNum];
  CAlnMap::TSeqPosList*  seqStops = new CAlnMap::TSeqPosList[rowNum];
  string* seqidArray=new string[rowNum];
  string middleLine;
  list<FeatureInfo*>* bioseqFeature= new list<FeatureInfo*>[rowNum];
  CAlnMap::TSignedRange* rowRng = new CAlnMap::TSignedRange[rowNum];

  //conver to aln coordinates for mask seqloc
  for (list<SeqlocInfo>::iterator iter=m_Seqloc.begin();  iter!=m_Seqloc.end(); iter++){
    for (int i=0; i<rowNum; i++){
      if((*iter).seqloc->GetInt().GetId().Match(m_AV->GetSeqId(i))){
        (*iter).alnRange.Set(m_AV->GetAlnPosFromSeqPos(i, (*iter).seqloc->GetInt().GetFrom()), m_AV->GetAlnPosFromSeqPos(i, (*iter).seqloc->GetInt().GetTo()));       
      }
    }
  }
  //Add external query feature info such as phi blast pattern
  for (list<FeatureInfo>::iterator iter=m_QueryFeature.begin();  iter!=m_QueryFeature.end(); iter++){
    for(int i = 0; i < rowNum; i++){
      if(iter->seqloc->GetInt().GetId().Match(m_AV->GetSeqId(i))){
	int alnFrom = m_AV->GetAlnPosFromSeqPos(i, iter->seqloc->GetInt().GetFrom() < m_AV->GetSeqStart(i) ? m_AV->GetSeqStart(i):iter->seqloc->GetInt().GetFrom());
	int alnTo = m_AV->GetAlnPosFromSeqPos(i, iter->seqloc->GetInt().GetTo() > m_AV->GetSeqStop(i) ? m_AV->GetSeqStop(i):iter->seqloc->GetInt().GetTo());
	
	FeatureInfo* featInfo = new FeatureInfo;
	setFeatureInfo(featInfo, *(iter->seqloc), alnFrom, alnTo, aln_stop, iter->featureChar, iter->featureId);    
	bioseqFeature[i].push_back(featInfo);
      }
    }
  }

  //prepare data for each row
  for (int row=0; row<rowNum; row++) {
    rowRng[row] = m_AV->GetSeqAlnRange(row);
    //make feature
    if(m_AlignOption & eShowCdsFeature){
      getFeatureInfo(bioseqFeature[row], *m_featScope, CSeqFeatData::e_Cdregion, row);
    }
    if(m_AlignOption & eShowGeneFeature){
      getFeatureInfo(bioseqFeature[row], *m_featScope, CSeqFeatData::e_Gene, row);
    }
   
    //make sequence
    m_AV->GetWholeAlnSeqString(row, sequence[row],  NULL, NULL, NULL, m_LineLen, &seqStarts[row], &seqStops[row]);

    //make id
    if(m_AlignOption & eShowBlastStyleId) {
       if(row==0){//query
         seqidArray[row]="Query";
       } else {//hits
         if (!(m_AlignOption&eMultiAlign)){
         //hits for pairwise 
           seqidArray[row]="Sbjct";
         } else {
           int gi = GetGiForSeqIdList(m_AV->GetBioseqHandle(row).GetBioseq().GetId());
           if (m_AlignOption&eShowGi && gi > 0){
             seqidArray[row]=NStr::IntToString(gi);
           } else {
             const CBioseq& bsp=m_AV->GetBioseqHandle(row).GetBioseq();
             const CRef<CSeq_id> wid = FindBestChoice(bsp.GetId(), CSeq_id::WorstRank);
             seqidArray[row]=wid->GetSeqIdString();
           }           
         }
       }

    } else {
      int gi = GetGiForSeqIdList(m_AV->GetBioseqHandle(row).GetBioseq().GetId());
      if (m_AlignOption&eShowGi && gi > 0){
        seqidArray[row]=NStr::IntToString(gi);
      } else {
        const CBioseq& bsp=m_AV->GetBioseqHandle(row).GetBioseq();
        const CRef<CSeq_id> wid = FindBestChoice(bsp.GetId(), CSeq_id::WorstRank);
        seqidArray[row]=wid->GetSeqIdString();
      }      
    }

    //max id length
    maxIdLen=max<int>(seqidArray[row].size(), maxIdLen);
    //max start length
    int maxCood=max<int>(m_AV->GetSeqStart(row), m_AV->GetSeqStop(row));
    maxStartLen = max<int>(NStr::IntToString(maxCood).size(), maxStartLen);
  }
  //adjust max id length for feature id 
  for(int i = 0; i < rowNum; i ++){
    for (list<FeatureInfo*>::iterator iter=bioseqFeature[i].begin();  iter != bioseqFeature[i].end(); iter++){
      maxIdLen=max<int>((*iter)->featureId.size(), maxIdLen );
    }
  }  //end of preparing row data
  
  //output identities info 
  if(m_AlignOption&eShowBlastInfo && !(m_AlignOption&eMultiAlign)) {
    int match = 0;
    int positive = 0;
    int gap = 0;
    fillIdentityInfo(sequence[0], sequence[1],  match,  positive, middleLine);
    out<<" Identities = "<<match<<"/"<<(aln_stop+1)<<" ("<<((match*100)/(aln_stop+1))<<"%"<<")";
    if(m_AlignType&eProt) {
      out<<", Positives = "<<(positive + match)<<"/"<<(aln_stop+1)<<" ("<<(((positive + match)*100)/(aln_stop+1))<<"%"<<")";
    }
    gap = getNumGaps();
    out<<", Gaps = "<<gap<<"/"<<(aln_stop+1)<<" ("<<((gap*100)/(aln_stop+1))<<"%"<<")"<<endl;
    if (m_AlignType&eNuc){ 
      out<<" Strand="<<(m_AV->StrandSign(0)==1 ? "Plus" : "Minus")<<"/"<<(m_AV->StrandSign(1)==1? "Plus" : "Minus")<<endl;
    }
    out<<endl;
  }
  //output rows
  for(int j=0; j<aln_stop; j+=m_LineLen){
    //output according to aln coordinates
    if(aln_stop-j+1<m_LineLen) {
      actualLineLen=aln_stop-j+1;
    } else {
      actualLineLen=m_LineLen;
    }
    CAlnMap::TSignedRange curRange(j, j+actualLineLen-1);
    //here is each row
    for (int row=0; row<rowNum; row++) {
      bool hasSequence = true;
      if(m_AlignOption&eMultiAlign){
	hasSequence = curRange.IntersectingWith(rowRng[row]);
      }
      //only output rows that have sequence
      if (hasSequence){
	int start = seqStarts[row].front() + 1;  //+1 for 1 based
	int end = seqStops[row].front() + 1;
	list<string> inserts;
	string insertPosString;  //the one with "\" to indicate insert
	if(m_AlignOption & eMasterAnchored){
	  fillInserts(row, curRange, j, inserts, insertPosString);
	}
        if(row == 0&&(m_AlignOption&eHtml)&&(m_AlignOption&eMultiAlign) && (m_AlignOption&eSequenceRetrieval && m_IsDbGi)){
          char checkboxBuf[200];
          sprintf(checkboxBuf, "<input type=\"checkbox\" name=\"getSeqMaster\" value=\"\" onClick=\"uncheckable('getSeqAlignment', 'getSeqMaster')\">");
          out << checkboxBuf;
        }
        string urlLink;
        //setup url link for seqid
        if(row>0&&(m_AlignOption&eHtml)&&(m_AlignOption&eMultiAlign)){
        
          int gi = GetGiForSeqIdList(m_AV->GetBioseqHandle(row).GetBioseq().GetId());
          if(gi > 0){
            out<<"<a name="<<gi<<"></a>";
          } else {
            out<<"<a name="<<seqidArray[row]<<"></a>";
          }
          //get sequence checkbox
          if(m_AlignOption&eSequenceRetrieval && m_IsDbGi){
            char checkBoxBuf[512];
            sprintf(checkBoxBuf, "<input type=\"checkbox\" name=\"getSeqGi\" value=\"%ld\" onClick=\"synchronizeCheck(this.value, 'getSeqAlignment', 'getSeqGi', this.checked)\">", gi);
            out << checkBoxBuf;        
          }
          urlLink = getUrl(m_AV->GetBioseqHandle(row).GetBioseq().GetId(), row);         
          out << urlLink;
         
        }
        out<<seqidArray[row]; 
        if(row>0&& m_AlignOption&eHtml && m_AlignOption&eMultiAlign && urlLink != NcbiEmptyString){
          out<<"</a>";
         
        }
        //adjust space between id and start
        AddSpace(out, maxIdLen-seqidArray[row].size()+m_IdStartMargin);
	out << start;
        startLen=NStr::IntToString(start).size();
        AddSpace(out, maxStartLen-startLen+m_StartSequenceMargin);
	if (row>0 && m_AlignOption & eShowIdentity){
	  for (int index = j; index < j + actualLineLen && index < sequence[row].size(); index ++){
	    if (sequence[row][index] == sequence[0][index] && isalpha(sequence[row][index])) {
	      sequence[row][index] = '.';           
	    }         
	  }
	}
	OutputSeq(sequence[row], m_AV->GetSeqId(row), j, actualLineLen, out);
        AddSpace(out, m_SeqStopMargin);
	out << end;
        
        out<<endl;
     
	//display inserts for anchored type
	if(m_AlignOption & eMasterAnchored){
	  bool insertAlready = false;
	  for(list<string>::iterator iter = inserts.begin(); iter != inserts.end(); iter ++){
	   
	    if(!insertAlready){
	      if((m_AlignOption&eHtml)&&(m_AlignOption&eMultiAlign) && (m_AlignOption&eSequenceRetrieval && m_IsDbGi)){
		char checkboxBuf[200];
		sprintf(checkboxBuf, "<input type=\"checkbox\" name=\"getSeqMaster\" value=\"\" onClick=\"uncheckable('getSeqAlignment', 'getSeqMaster')\">");
		out << checkboxBuf;
	      }
	      AddSpace(out, maxIdLen+m_IdStartMargin+maxStartLen+m_StartSequenceMargin);
	      out << insertPosString<<endl;
	    }
	    if((m_AlignOption&eHtml)&&(m_AlignOption&eMultiAlign) && (m_AlignOption&eSequenceRetrieval && m_IsDbGi)){
	      char checkboxBuf[200];
	      sprintf(checkboxBuf, "<input type=\"checkbox\" name=\"getSeqMaster\" value=\"\" onClick=\"uncheckable('getSeqAlignment', 'getSeqMaster')\">");
	      out << checkboxBuf;
	    }
	    AddSpace(out, maxIdLen+m_IdStartMargin+maxStartLen+m_StartSequenceMargin);
	    out<<*iter<<endl;
	    
	    insertAlready = true;
	  }
	} 
	//display feature. Feature, if set, will be displayed for query regardless

	for (list<FeatureInfo*>::iterator iter=bioseqFeature[row].begin();  iter != bioseqFeature[row].end(); iter++){
	  if ( curRange.IntersectingWith((*iter)->alnRange)){  
	    if((m_AlignOption&eHtml)&&(m_AlignOption&eMultiAlign) && (m_AlignOption&eSequenceRetrieval && m_IsDbGi)){
	      char checkboxBuf[200];
	      sprintf(checkboxBuf, "<input type=\"checkbox\" name=\"getSeqMaster\" value=\"\" onClick=\"uncheckable('getSeqAlignment', 'getSeqMaster')\">");
	      out << checkboxBuf;
	    }
	    out<<(*iter)->featureId;
	    AddSpace(out, maxIdLen+m_IdStartMargin+maxStartLen+m_StartSequenceMargin-(*iter)->featureId.size());
	    OutputSeq((*iter)->featureString, CSeq_id(), j, actualLineLen, out);
	    out<<endl;
	  }
	}
	
	//display middle line
	if (row == 0 && ((m_AlignOption & eShowBarMiddleLine)||(m_AlignOption & eShowCharMiddleLine)) && !(m_AlignOption&eMultiAlign)) {
	  AddSpace(out, maxIdLen+m_IdStartMargin+maxStartLen+m_StartSequenceMargin);
	  OutputSeq(middleLine, CSeq_id(), j, actualLineLen, out);
	  out<<endl;
	}
      }
      if(!seqStarts[row].empty()){ //shouldn't need this check
	seqStarts[row].pop_front();
      }
      if(!seqStops[row].empty()){
	seqStops[row].pop_front();
      }
    }
    out<<endl;
  }//end of displaying rows
 
  //free allocation
  for(int i = 0; i < rowNum; i ++){
    for (list<FeatureInfo*>::iterator iter=bioseqFeature[i].begin();  iter != bioseqFeature[i].end(); iter++){
      delete (*iter);
    }
  } 
  delete [] bioseqFeature;
  delete [] seqidArray;
  delete [] rowRng;
  delete [] seqStarts;
  delete [] seqStops;
}

//To display teh seqalign
void CDisplaySeqalign::DisplaySeqalign(CNcbiOstream& out){

  string previous_id = NcbiEmptyString, subid = NcbiEmptyString;
  //scope for feature fetching
  if(m_AlignOption & eShowCdsFeature || m_AlignOption & eShowGeneFeature){
    m_FeatObj = new CObjectManager();
    m_FeatObj->RegisterDataLoader(*new CGBDataLoader("ID", NULL, 2), CObjectManager::eDefault); 
    m_featScope = new CScope(*m_FeatObj);  //for seq feature fetch
    m_featScope->AddDefaults();	     
  }	
  
  //determine if the database has gi by looking at the 1st hit.  Could be wrong but simple for now
  CTypeConstIterator<CSeq_align> saTemp = ConstBegin(*m_SeqalignSetRef); 
  CTypeConstIterator<CDense_seg> dsTemp = ConstBegin(*saTemp);
  const vector< CRef< CSeq_id > >& idsTemp = dsTemp->GetIds();
  vector< CRef< CSeq_id > >::const_iterator iterTemp = idsTemp.begin();
  iterTemp++;
  const CBioseq_Handle& handleTemp = m_Scope.GetBioseqHandle(**iterTemp);
  if(handleTemp){
    int giTemp = GetGiForSeqIdList(handleTemp.GetBioseq().GetId());
    if(giTemp >0 ) { 
      m_IsDbGi = true;
    }
  }
  if(m_AlignOption & eHtml){
    out<<"<script src=\"blastResult.js\"></script>";
  }
   //get sequence 
  if(m_AlignOption&eSequenceRetrieval && m_AlignOption&eHtml && m_IsDbGi){ 
        out<<GetSeqForm((char*)"submitterTop", m_IsDbNa);
        out<<"<form name=\"getSeqAlignment\">\n";
      }

  //begin to display
  int num_align = 0;
  string toolUrl= m_Reg->Get(m_BlastType, "TOOL_URL");
  if(!(m_AlignOption&eMultiAlign)){//pairwise alignment

    list<alnInfo*> avList;  
    if(!(m_AlignOption&eMultiAlign)){ //construct aln vector and store alignment start and stop for dumpgnl
      for (CTypeConstIterator<CSeq_align> sa_it2 = ConstBegin(*m_SeqalignSetRef); sa_it2&&num_align<m_NumAlignToShow; ++sa_it2, num_align++) {

	CTypeConstIterator<CDense_seg> ds = ConstBegin(*sa_it2);
	CRef<CAlnVec> avRef( new CAlnVec(*ds, m_Scope));
	
	try{
	  const CBioseq_Handle& handle = avRef->GetBioseqHandle(1);	
	  if(handle){
	    alnInfo* alnvecInfo = new alnInfo;
	    getAlnScores(*sa_it2, alnvecInfo->score, alnvecInfo->bits, alnvecInfo->eValue);
	    alnvecInfo->alnVec = avRef;
	    avList.push_back(alnvecInfo);
	    int gi = GetGiForSeqIdList(handle.GetBioseq().GetId());
	    if(!(toolUrl == NcbiEmptyString || (gi > 0 && toolUrl.find("dumpgnl.cgi") != string::npos))){ //need to construct segs for dumpgnl
	      string idString = avRef->GetSeqId(1).GetSeqIdString();
	      if(m_Segs.count(idString) > 0){ //already has seg, concatenate
		m_Segs[idString] += "," + NStr::IntToString(avRef->GetSeqStart(1)) + "-" + NStr::IntToString(avRef->GetSeqStop(1));
	      } else {//new segs
		m_Segs.insert(map<string, string>::value_type(idString, NStr::IntToString(avRef->GetSeqStart(1)) + "-" + NStr::IntToString(avRef->GetSeqStop(1))));
	      }
	    }
	  }
	} catch(CException& e){
	  continue;
	}
      }
    } 
    //display
  
    for(list<alnInfo*>::iterator iterAv = avList.begin(); iterAv != avList.end(); iterAv ++){
      m_AV = (*iterAv)->alnVec;
      const CBioseq_Handle& bsp_handle=m_AV->GetBioseqHandle(1); 
      if(bsp_handle){
	subid=m_AV->GetSeqId(1).GetSeqIdString();
	if((previous_id == NcbiEmptyString || subid != previous_id)&&(m_AlignOption&eShowBlastInfo)) {
	  PrintDefLine(bsp_handle,  out);
	  out<<"          Length="<<bsp_handle.GetBioseq().GetInst().GetLength()<<endl<<endl;
	  
	}
	previous_id = subid;
	if (m_AlignOption&eShowBlastInfo) {
	  
	  out<<" Score = "<<(*iterAv)->bits<<" ";
	  out<<"bits ("<<(*iterAv)->score<<"),"<<"  ";
	  out<<"Expect = "<<(*iterAv)->eValue<<endl;
	}
	DisplayAlnvec(out);
	out<<endl;
	
      }
    }
    for(list<alnInfo*>::iterator iterAv = avList.begin(); iterAv != avList.end(); iterAv ++){
      delete(*iterAv);
    }
  } else if(m_AlignOption&eMultiAlign){ //multiple alignment
       
    auto_ptr<CObjectOStream> out2(CObjectOStream::Open(eSerial_AsnText, out));
    CAlnMix mix(m_Scope);
    num_align = 0;
    for (CTypeConstIterator<CSeq_align> sa_it = ConstBegin(*m_SeqalignSetRef); sa_it&&num_align<m_NumAlignToShow; ++sa_it, num_align++) {
      CTypeConstIterator<CDense_seg> ds = ConstBegin(*sa_it);
      if(m_Scope.GetBioseqHandle(*((*ds).GetIds())[1])){
	mix.Add(*ds);
      }
    
      //   *out2<<*sa_it;
    }  
    
    mix.Merge(CAlnMix::fGen2EST| CAlnMix::fMinGap | CAlnMix::fQuerySeqMergeOnly | CAlnMix::fFillUnalignedRegions);  
  
    // *out2<<mix.GetDenseg();
    out<<endl;
    CRef<CAlnVec> avRef (new CAlnVec (mix.GetDenseg(), m_Scope));
    m_AV = avRef;
    DisplayAlnvec(out); 

  }
  if(m_AlignOption&eSequenceRetrieval && m_AlignOption&eHtml && m_IsDbGi){
    out<<"</form>\n";
    out<<GetSeqForm((char*)"submitterBottom", m_IsDbNa);
  }
}

//compute number of identical and positive residues; set middle line accordingly
const void CDisplaySeqalign::fillIdentityInfo(const string& sequenceStandard, const string& sequence , int& match, int& positive, string& middleLine) {
  match = 0;
  positive = 0;
  int min_length=min<int>(sequenceStandard.size(), sequence.size());
  if(m_AlignOption & eShowBarMiddleLine || m_AlignOption & eShowCharMiddleLine){
    middleLine = sequence;
  }
  for(int i=0; i<min_length; i++){
    if(sequenceStandard[i]==sequence[i]){
      if(m_AlignOption & eShowBarMiddleLine ) {
	middleLine[i] = '|';
      } else if (m_AlignOption & eShowCharMiddleLine){
	middleLine[i] = sequence[i];
      }
      match ++;
    } else {
      if ((m_AlignType&eProt) && m_Matrix[sequenceStandard[i]][sequence[i]] > 0){  
	positive ++;
	if (m_AlignOption & eShowCharMiddleLine){
	  middleLine[i] = '+';
	}
      } else {
	middleLine[i] = ' ';
      }    
    }
  }  
}


const void CDisplaySeqalign::PrintDefLine(const CBioseq_Handle& bspHandle, CNcbiOstream& out) const
{
  if(bspHandle){
    const CRef<CSeq_id> wid = FindBestChoice(bspHandle.GetBioseq().GetId(), CSeq_id::WorstRank);
 
    const CRef<CBlast_def_line_set> bdlRef = GetBlastDefline(bspHandle.GetBioseq());
    const list< CRef< CBlast_def_line > >& bdl = bdlRef->Get();
    bool isFirst = true;
    int firstGi = 0;
  
    if(bdl.empty()){ //no blast defline struct, should be no such case now
      wid->WriteAsFasta(out);
      out << GetTitle(bspHandle);
      out << endl;
    } else {
      //print each defline 
      for(list< CRef< CBlast_def_line > >::const_iterator iter = bdl.begin(); iter != bdl.end(); iter++){
	string urlLink;
	if(isFirst){
	  out << ">";
   
	} else{
	  out << " ";
	}

	int gi =  GetGiForSeqIdList((*iter)->GetSeqid());
	if(isFirst){
	  firstGi = gi;
	}
	if ((m_AlignOption&eSequenceRetrieval) && (m_AlignOption&eHtml) && m_IsDbGi && isFirst) {
	  char buf[512];
	  sprintf(buf, "<input type=\"checkbox\" name=\"getSeqGi\" value=\"%ld\" onClick=\"synchronizeCheck(this.value, 'getSeqAlignment', 'getSeqGi', this.checked)\">", gi);
	  out << buf;
	}
 
	if(m_AlignOption&eHtml){
     
	  urlLink = getUrl((*iter)->GetSeqid(), 1);    
	  out<<urlLink;
	}
    
	if(m_AlignOption&eShowGi && gi > 0){
	  out<<"gi|"<<gi<<"|";
	}       
   
	wid->WriteAsFasta(out);
	if(m_AlignOption&eHtml){
	  if(urlLink != NcbiEmptyString){
	    out<<"</a>";
	  }
	  if(gi != 0){
	    out<<"<a name="<<gi<<"></a>";
	  } else {
	    out<<"<a name="<<wid->GetSeqIdString()<<"></a>";
	  }
	  if(m_AlignOption&eLinkout){
	    out <<" ";
	    AddLinkout(bspHandle.GetBioseq(), (**iter), firstGi, gi, out);
	  }
	}
 
	out <<" ";
	if((*iter)->IsSetTitle()){
	  out << (*iter)->GetTitle();     
	}
	out<<endl;
	isFirst = false;
      }
    }
  }
}

//Output sequence and mask sequences if any
const void CDisplaySeqalign::OutputSeq(string& sequence, const CSeq_id& id, int start, int len, CNcbiOstream& out) const {
  int actualSize = sequence.size();
  assert(actualSize > start);
  list<CRange<int> > actualSeqloc;
  string actualSeq = sequence.substr(start, len);
  //go through seqloc containing mask info
  for (list<SeqlocInfo>::iterator iter=m_Seqloc.begin();  iter!=m_Seqloc.end(); iter++){
    int from=(*iter).alnRange.GetFrom();
    int to=(*iter).alnRange.GetTo();
    if(id.Match((*iter).seqloc->GetInt().GetId())){
      bool isFirstChar = true;
      CRange<int> eachSeqloc(0, 0);
      //go through each residule and mask it
      for (int i=max<int>(from, start); i<=min<int>(to, start+len); i++){
	//store seqloc start for font tag below
        if ((m_AlignOption & eHtml) && isFirstChar){         
          isFirstChar = false;
          eachSeqloc.Set(i, eachSeqloc.GetTo());
        }
        if (m_SeqLocChar==eX){
          actualSeq[i-start]='X';
        } else if (m_SeqLocChar==eN){
          actualSeq[i-start]='n';
        } else if (m_SeqLocChar==eLowerCase){
          actualSeq[i-start]=tolower(actualSeq[i-start]);
        }
	//store seqloc start for font tag below
        if ((m_AlignOption & eHtml) && i == min<int>(to, start+len)){ 
          eachSeqloc.Set(eachSeqloc.GetFrom(), i);
        }
      }
      if(!(eachSeqloc.GetFrom()==0&&eachSeqloc.GetTo()==0)){
        actualSeqloc.push_back(eachSeqloc);
      }
    }
  }

  if(actualSeqloc.empty()){//no need to add font tag
    out<<actualSeq;
  } else {//now deal with font tag for mask for html display    
    bool endTag = false;
    bool numFrontTag = 0;
    for (int i=0; i<actualSeq.size(); i++){
      for (list<CRange<int> >::iterator iter=actualSeqloc.begin();  iter!=actualSeqloc.end(); iter++){
        int from = (*iter).GetFrom() - start;
        int to = (*iter).GetTo() - start;
	//start tag
        if(from == i){
          out<<"<font color=\""+color[m_SeqLocColor]+"\">";
          numFrontTag = 1;
        }
	//need to close tag at the end of mask or end of sequence
        if(to == i || i == actualSeq.size() - 1 ){
          endTag = true;
        }
      }
      out<<actualSeq[i];
      if(endTag && numFrontTag == 1){
        out<<"</font>";
        endTag = false;
        numFrontTag = 0;
      }
    }
  }
}

int CDisplaySeqalign::getNumGaps() {
  int gap = 0;
  for (int row=0; row<m_AV->GetNumRows(); row++) {
    CRef<CAlnMap::CAlnChunkVec> chunk_vec = m_AV->GetAlnChunks(row, m_AV->GetSeqAlnRange(0));
    for (int i=0; i<chunk_vec->size(); i++) {
      CConstRef<CAlnMap::CAlnChunk> chunk = (*chunk_vec)[i];
      if (chunk->IsGap()) {
        gap += (chunk->GetAlnRange().GetTo() - chunk->GetAlnRange().GetFrom() + 1);
      }
    }
  }
  return gap;
}


const CRef<CBlast_def_line_set>  CDisplaySeqalign::GetBlastDefline (const CBioseq& cbsp) const {
  CRef<CBlast_def_line_set> bdls(new CBlast_def_line_set());
  if(cbsp.IsSetDescr()){
    const CSeq_descr& desc = cbsp.GetDescr();
    const list< CRef< CSeqdesc > >& descList = desc.Get();
    for (list<CRef< CSeqdesc > >::const_iterator iter = descList.begin(); iter != descList.end(); iter++){
      
      if((*iter)->IsUser()){
        const CUser_object& uobj = (*iter)->GetUser();
        const CObject_id& uobjid = uobj.GetType();
        if(uobjid.IsStr()){
   
          const string& label = uobjid.GetStr();
          if (label == kAsnDeflineObjLabel){
           const vector< CRef< CUser_field > >& usf = uobj.GetData();
           string buf;
          
	   if(usf.front()->GetData().IsOss()){ //only one user field
	   
	     const list< vector< char >* >& oss =  usf.front()->GetData().GetOss();
             int size = 0;
             //determine the octet string length
             for (list< vector< char >* >::const_iterator iter3 = oss.begin(); iter3 != oss.end(); iter3++){           
	       size += (**iter3).size();
             }
             
             int i =0;
             char* temp = new char[size];
             //retrive the string
             for (list< vector< char >* >::const_iterator iter3 = oss.begin(); iter3 != oss.end(); iter3++){
      
               for(vector< char >::iterator iter4 = (**iter3).begin(); iter4 !=(**iter3).end(); iter4++){
                 temp[i] = *iter4;
                 i++;
               }
             }            
	    
	     CConn_MemoryStream stream;
             stream.write(temp, i);
             auto_ptr<CObjectIStream> ois(CObjectIStream::Open(eSerial_AsnBinary, stream));
             *ois >> *bdls;
	     delete [] temp;
           }         
          }
        }
      }
    }
  }
  return bdls;
}

static string GetTaxNames(const CBioseq& cbsp, int taxid){
  string name;
  if(cbsp.IsSetDescr()){  
    const CSeq_descr& desc = cbsp.GetDescr();
    const list< CRef< CSeqdesc > >& descList = desc.Get();   
    for (list<CRef< CSeqdesc > >::const_iterator iter = descList.begin(); iter != descList.end(); iter++){
      if((*iter)->IsUser()){
        const CUser_object& uobj = (*iter)->GetUser();
        const CObject_id& uobjid = uobj.GetType();
        if(uobjid.IsStr()){   
          const string& label = uobjid.GetStr();
          if (label == kTaxDataObjLabel){
            const vector< CRef< CUser_field > >& usf = uobj.GetData();        
            for (vector< CRef< CUser_field > >::const_iterator iter2 = usf.begin(); iter2 != usf.end(); iter2 ++){
              const CObject_id& oid = (*iter2)->GetLabel();
              if (oid.GetId() == taxid){
                (**iter2).GetData().Which();
                const list< string >& strs = (**iter2).GetData().GetStrs();
                name = strs.front();
                break;
              }
            }
          }
        }
      }
    }
  }
  return name;
}

void CDisplaySeqalign::getFeatureInfo(list<FeatureInfo*>& feature, CScope& scope, CSeqFeatData::E_Choice choice, int row) const {
  //Only fetch features for seq that has a gi
  CRef<CSeq_id> id = GetSeqIdByType(m_AV->GetBioseqHandle(row).GetBioseq().GetId(), CSeq_id::e_Gi);
  if(!(id.Empty())){
    //cds feature
    for  (CFeat_CI feat (scope.GetBioseqHandle(*id), m_AV->GetSeqStart(row), m_AV->GetSeqStop(row), choice); feat;  ++feat) {
      const CSeq_loc& loc = feat->GetLocation();
      string featLable = NcbiEmptyString;

      GetLabel(feat->GetOriginalFeature(), &featLable, feature::eBoth, &(m_AV->GetScope()));
      int alnStop = m_AV->GetAlnStop();

      if(loc.IsInt()){
	FeatureInfo* featInfo = new FeatureInfo;
     
	int alnFrom = m_AV->GetAlnPosFromSeqPos(row, loc.GetInt().GetFrom() < m_AV->GetSeqStart(row) ? m_AV->GetSeqStart(row):loc.GetInt().GetFrom());
	int alnTo = m_AV->GetAlnPosFromSeqPos(row, loc.GetInt().GetTo() > m_AV->GetSeqStop(row) ? m_AV->GetSeqStop(row):loc.GetInt().GetTo());
	char featChar;
	if(choice == CSeqFeatData::e_Gene){
	  featChar = '^';
	} else if (choice == CSeqFeatData::e_Cdregion){
	  featChar = '~';
	} else {
	  featChar = '*';
	}
	//only show first 15 letters
	setFeatureInfo(featInfo, loc, alnFrom, alnTo, alnStop, featChar, featLable.substr(0, 15));     
	feature.push_back(featInfo);
      }
    }
  }
}

void setFeatureInfo(CDisplaySeqalign::FeatureInfo* featInfo, const CSeq_loc& seqloc, int alnFrom, int alnTo, int alnStop, char patternChar, string patternId){
  featInfo->seqloc = &seqloc;
  featInfo->alnRange.Set(alnFrom, alnTo); 
  featInfo->featureChar = patternChar;
    featInfo->featureId = patternId;
    //fill feature string
    string line(alnStop+1, ' ');  
    for (int j = alnFrom; j <= alnTo; j++){
      line[j] = featInfo->featureChar;
    }
    featInfo->featureString = line;
}

//May need to add a "|" to the current insert for insert on next rows
int addBar(string& seq, int insertAlnPos, int alnStart){
  int end = seq.size() -1 ;
  int barPos = insertAlnPos - alnStart + 1;
  string addOn;
  if(barPos - end > 1){
    string spacer(barPos - end - 1, ' ');
    addOn += spacer + "|";
  } else if (barPos - end == 1){
    addOn += "|";
  }
  seq += addOn;
  return max<int>((barPos - end), 0);
}

//Add new insert seq to the current insert seq and return the end position of the latest insert
int adjustInsert(string& curInsert, string& newInsert, int insertAlnPos, int alnStart){
  int insertEnd = 0;
  int curInsertSize = curInsert.size();
  int insertLeftSpace = insertAlnPos - alnStart - curInsertSize + 2;  //plus2 because insert is put after the position
  if(curInsertSize > 0){
    assert(insertLeftSpace >= 2);
  }
  int newInsertSize = newInsert.size();  
  if(insertLeftSpace - newInsertSize >= 1){ //can insert with the end position right below the bar
    string spacer(insertLeftSpace - newInsertSize, ' ');
    curInsert += spacer + newInsert;
    
  } else { //Need to insert beyond the insert postion
    if(curInsertSize > 0){
      curInsert += " " + newInsert;
    } else {  //can insert right at the firt position
      curInsert += newInsert;
    }
  }
  insertEnd = alnStart + curInsert.size() -1 ; //-1 back to string position
  return insertEnd;
}

//recusively fill the insert
void CDisplaySeqalign::doFills(int row, CAlnMap::TSignedRange& alnRange, int alnStart, list<CConstRef<CAlnMap::CAlnChunk> >& chunks, list<string>& inserts) const{
  if(!chunks.empty()){
    string bar(alnRange.GetLength(), ' ');
   
    string seq;
    list<CConstRef<CAlnMap::CAlnChunk> > leftoverChunk;
    bool isFirstChunk = true;
    int curInsertAlnStart = 0;
    int prvsInsertAlnEnd = 0;

    //go through each chunk at fills the seq if it can  be filled on the same line.  If not, go to the next line
    for(list<CConstRef<CAlnMap::CAlnChunk> >::iterator iter = chunks.begin(); iter != chunks.end(); iter ++){
      curInsertAlnStart = (*iter)->GetAlnRange().GetFrom();
      //always fill the first insert.  Also fill if there is enough space
      if(isFirstChunk || curInsertAlnStart - prvsInsertAlnEnd >= 1){
	bar[curInsertAlnStart-alnStart+1] = '|';  
	int seqStart = (*iter)->GetRange().GetFrom();
	int seqEnd = (*iter)->GetRange().GetTo();
	string newInsert(abs(seqStart - seqEnd) + 1, ' ');
	newInsert = m_AV->GetSeqString(newInsert, row, seqStart, seqEnd);
	prvsInsertAlnEnd = adjustInsert(seq, newInsert, curInsertAlnStart, alnStart);
	isFirstChunk = false;
      } else { //if no space, save the chunk and go to next line 
	bar[curInsertAlnStart-alnStart+1] = '|';  //indicate insert goes to the next line
	prvsInsertAlnEnd += addBar(seq, curInsertAlnStart, alnStart);   //May need to add a bar after the current insert sequence to indicate insert goes to the next line.
	leftoverChunk.push_back(*iter);    
      }
    }
    //save current insert.  Note that each insert has a bar and sequence below it
    inserts.push_back(bar);
    inserts.push_back(seq);
    //here recursively fill the chunk that don't have enough space
    doFills(row, alnRange, alnStart, leftoverChunk, inserts);
  }
}

/*fill a list of inserts for a particular row*/
void CDisplaySeqalign::fillInserts(int row, CAlnMap::TSignedRange& alnRange, int alnStart, list<string>& inserts, string& insertPosString) const {
  CAlnMap::TSignedRange actualRange (alnRange.GetFrom() - 1, alnRange.GetTo()); //-1 to get the insert at the end of the previous line
  /*get insert for this row*/
  CRef<CAlnMap::CAlnChunkVec> chunk_vec = m_AV->GetAlnChunks(row, actualRange, CAlnMap::fInsertsOnly);
  if(chunk_vec->size() > 0){
    string line(alnRange.GetLength(), ' ');
    list<CConstRef<CAlnMap::CAlnChunk> > chunks;

    for (int i=0; i<chunk_vec->size(); i++) {
      CConstRef<CAlnMap::CAlnChunk> chunk = (*chunk_vec)[i];
      chunks.push_back(chunk);
      line[chunk->GetAlnRange().GetFrom()-alnStart+1] = '\\';
    }
    insertPosString = line; //this is the line with "\" right after each insert position
    //here fills the insert sequence
    doFills(row, alnRange, alnStart, chunks, inserts);
  }
}

//segments starts and stops used for map viewer 
string CDisplaySeqalign::getSegs(int row) const {
  string segs = NcbiEmptyString;
  if(m_AlignOption & eMultiAlign){ //only show this hsp
    segs = NStr::IntToString(m_AV->GetSeqStart(row)) + "-" + NStr::IntToString(m_AV->GetSeqStop(row));
  } else { //for all segs
    string idString = m_AV->GetSeqId(1).GetSeqIdString();
    map<string, string>::const_iterator iter = m_Segs.find(idString);
    if ( iter != m_Segs.end() ){
      segs = iter->second;
    }
  }
  return segs;
}



/* transforms a string so that it becomes safe to be used as part of URL
 * the function converts characters with special meaning (such as
 * semicolon -- protocol separator) to escaped hexadecimal (%xx)
 */
static string MakeURLSafe(char* src){
  static char HEXDIGS[] = "0123456789ABCDEF";
  char* buf;
  size_t len;
  char* p;
  char c;
  string url = NcbiEmptyString;

  if (src){
    /* first pass to calculate required buffer size */
    for (p = src, len = 0; (c = *(p++)) != '\0'; ) {
      switch (c) {
      default:
	if (c < '0' || (c > '9' && c < 'A') ||
	    (c > 'Z' && c < 'a') || c > 'z') {
	  len += 3;
	  break;
	}
      case '-': case '_': case '.': case '!': case '~':
      case '*': case '\'': case '(': case ')':
	++len;
      }
    }
    buf = new char[len + 1];
    /* second pass -- conversion */
    for (p = buf; (c = *(src++)) != '\0'; ) {
      switch (c) {
      default:
	if (c < '0' || (c > '9' && c < 'A') ||
	    (c > 'Z' && c < 'a') || c > 'z') {
	  *(p++) = '%';
	  *(p++) = HEXDIGS[(c >> 4) & 0xf];
	  *(p++) = HEXDIGS[c & 0xf];
	  break;
	}
      case '-': case '_': case '.': case '!': case '~':
      case '*': case '\'': case '(': case ')':
	*(p++) = c;
      }
    }
    *p = '\0';
    url = buf;
    delete [] buf;
  }
  return url;
}

//make url for dumpgnl.cgi
string CDisplaySeqalign::getDumpgnlLink(const list<CRef<CSeq_id> >& ids, int row)const {
  string link = NcbiEmptyString;  
  string toolUrl= m_Reg->Get(m_BlastType, "TOOL_URL");
  string passwd = m_Reg->Get(m_BlastType, "PASSWD");
  bool nodb_path =  false;
  CRef<CSeq_id> idGeneral = GetSeqIdByType(ids, CSeq_id::e_General);
  CRef<CSeq_id> idOther = GetSeqIdByType(ids, CSeq_id::e_Other);
  const CRef<CSeq_id> idAccession = FindBestChoice(ids, CSeq_id::WorstRank);
  string segs = getSegs(row);
  int gi = GetGiForSeqIdList(ids);
  if(!idGeneral.Empty() && idGeneral->AsFastaString().find("gnl|BL_ORD_ID")){ /* We do need to make security protected link to BLAST gnl */
    return NcbiEmptyString;
}
  
  /* If we are using 'dumpgnl.cgi' (the default) do not strip off the path. */
  if (toolUrl.find("dumpgnl.cgi") ==string::npos){
    nodb_path = true;
  }  
  int length = m_DbName.size();
  string str;
  char  *chptr, *dbtmp;
  Char tmpbuff[256];
  char* dbname = new char[sizeof(char)*length + 2];
  strcpy(dbname, m_DbName.c_str());
  if(nodb_path) {
    int i, j;
    dbtmp = new char[sizeof(char)*length + 2]; /* aditional space and NULL */
    memset(dbtmp, '\0', sizeof(char)*length + 2);
    for(i = 0; i < length; i++) {            
       if(isspace(dbname[i]) || dbname[i] == ',') {/* Rolling spaces */
	 continue;
       }
       j = 0;
       while (!isspace(dbname[i]) && j < 256  && i < length) { 
	 tmpbuff[j] = dbname[i];
	 j++; i++;
	 if(dbname[i] == ',') { /* Comma is valid delimiter */
	   break;
	 }
       }
       tmpbuff[j] = '\0';
       if((chptr = strrchr(tmpbuff, '/')) != NULL) { 
	 strcat(dbtmp, (char*)(chptr+1));
       } else {
	 strcat(dbtmp, tmpbuff);
       }
       strcat(dbtmp, " ");            
     }
   } else {
     dbtmp = dbname;
   }
  
  const CSeq_id* bestid;
  if (idGeneral.Empty()){
    bestid = idOther;
    if (idOther.Empty()){
      bestid = idAccession;
    }
  }
  /*
   * Need to protect start and stop positions
   * to avoid web users sending us hand-made URLs
   * to retrive full sequences
   */
  char gnl[256];
  unsigned char buf[32];
  CMD5 urlHash;
  if (bestid && bestid->Which() !=  CSeq_id::e_Gi){
    length = passwd.size();
    urlHash.Update(passwd.c_str(), length);
    strcpy(gnl, bestid->AsFastaString().c_str());
    urlHash.Update(gnl, strlen(gnl));
    urlHash.Update(segs.c_str(), segs.size());
    urlHash.Update(passwd.c_str(), length);
    urlHash.Finalize(buf);
    
  } else {
    gnl[0] = '\0';
  }
  
  str = MakeURLSafe(dbtmp == NULL ? (char*) "nr" : dbtmp);
  link += "<a href=\"";
  if (toolUrl.find("?") == string::npos){
    link += toolUrl + "?" + "db=" + str + "&na=" + (m_IsDbNa? "1" : "0") + "&";
  } else {
    link += toolUrl + "&db=" + str + "&na=" + (m_IsDbNa? "1" : "0") + "&";
  }
  
  if (gnl[0] != '\0'){
    str = MakeURLSafe(gnl);
    link += "gnl=";
    link += str;
    link += "&";
  }
  if (gi > 0){
    link += "gi=" + NStr::IntToString(gi) + "&";
  }
  if (m_Rid != NcbiEmptyString){
    link += "RID=" + m_Rid +"&";
  }
  
  if ( m_QueryNumber > 0){
    link += "QUERY_NUMBER=" + NStr::IntToString(m_QueryNumber) + "&";
  }
  link += "segs=" + segs + "&";
  
  char tempBuf[128];
  
  sprintf(tempBuf,
	  "seal=%02X%02X%02X%02X"
	  "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
	  buf[0], buf[1], buf[2], buf[3],
	  buf[4], buf[5], buf[6], buf[7],
	  buf[8], buf[9], buf[10], buf[11],
	  buf[12], buf[13], buf[14], buf[15]);
  
  link += tempBuf;
  link += "\">";
  if(nodb_path){
    delete [] dbtmp;
  }
  delete [] dbname;
  return link;
}

END_SCOPE(objects)
END_NCBI_SCOPE
