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

#include <ncbi_pch.hpp>
#include <objtools/blast_format/showalign.hpp>

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
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature.hpp>

#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Bioseq.hpp>

#include <objects/seqset/Seq_entry.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Dense_diag.hpp>

#include <objtools/alnmgr/alnmix.hpp>
#include <objtools/alnmgr/alnvec.hpp>

#include <objects/blastdb/Blast_def_line.hpp>
#include <objects/blastdb/Blast_def_line_set.hpp>
#include <objects/blastdb/defline_extra.hpp>

#include <stdio.h>
#include <util/tables/raw_scoremat.h>
#include <objtools/readers/getfeature.hpp>
#include <objtools/blast_format/blastfmtutil.hpp>
#include <algo/blast/core/blast_stat.h>
#include <html/htmlhelper.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE (sequence);

static const char k_IdentityChar = '.';
static const int k_NumFrame = 6;
static const string k_FrameConversion[k_NumFrame] = {"+1", "+2", "+3", "-1",
                                                     "-2", "-3"};
static const int k_GetSubseqThreshhold = 10000;

///threshhold to color mismatch. 98 means 98% 
static const int k_ColorMismatchIdentity = 0; 
static const int k_GetDynamicFeatureSeqLength = 200000;
static const string k_DumpGnlUrl = "/blast/dumpgnl.cgi";
static const int k_FeatureIdLen = 16;
static const int k_NumAsciiChar = 128;
const string color[]={"#000000", "#808080", "#FF0000"};
const string k_ColorRed = "#FF0000";
const string k_ColorPink = "#F805F5";

///protein matrix define
enum {
    ePMatrixSize = 23       // number of amino acid for matrix
};

static const char k_PSymbol[ePMatrixSize+1] =
"ARNDCQEGHILKMFPSTWYVBZX";

static const char k_IntronChar = '~';
static const int k_IdStartMargin = 2;
static const int k_SeqStopMargin = 2;
static const int k_StartSequenceMargin = 2;

static const string k_UncheckabeCheckbox = "<input type=\"checkbox\" \
name=\"getSeqMaster\" value=\"\" onClick=\"uncheckable('getSeqAlignment%d',\
 'getSeqMaster')\">";

static const string k_Checkbox = "<input type=\"checkbox\" \
name=\"getSeqGi\" value=\"%s\" onClick=\"synchronizeCheck(this.value, \
'getSeqAlignment%d', 'getSeqGi', this.checked)\">";
#ifdef USE_ORG_IMPL
static string k_GetSeqSubmitForm[] = {"<FORM  method=\"post\" \
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

static string k_GetSeqSelectForm = "<FORM><input \
type=\"button\" value=\"Select all\" onClick=\"handleCheckAll('select', \
'getSeqAlignment%d', 'getSeqGi')\"></form></td><td><FORM><input \
type=\"button\" value=\"Deselect all\" onClick=\"handleCheckAll('deselect', \
'getSeqAlignment%d', 'getSeqGi')\"></form>";


static string k_GetTreeViewForm =  "<FORM  method=\"post\" \
action=\"http://www.ncbi.nlm.nih.gov/blast/treeview/blast_tree_view.cgi?request=page&rid=%s&queryID=%s&distmode=on\" \
name=\"tree%s%d\" target=\"trv%s\"> \
<input type=button value=\"Distance tree of results\" onClick=\"extractCheckedSeq('getSeqAlignment%d', 'getSeqGi', 'tree%s%d')\"> \
<input type=\"hidden\" name=\"sequenceSet\" value=\"\"><input type=\"hidden\" name=\"screenWidth\" value=\"\"></form>";
#endif





CDisplaySeqalign::CDisplaySeqalign(const CSeq_align_set& seqalign, 
                       CScope& scope,
                       list <CRef<blast::CSeqLocInfo> >* mask_seqloc, 
                       list <FeatureInfo*>* external_feature,
                       const char* matrix_name /* = BLAST_DEFAULT_MATRIX */)
    : m_SeqalignSetRef(&seqalign),
      m_Seqloc(mask_seqloc),
      m_QueryFeature(external_feature),
      m_Scope(scope) 
{
    m_AlignOption = 0;
    m_SeqLocChar = eX;
    m_SeqLocColor = eBlack;
    m_LineLen = 60;
    m_IsDbNa = true;
    m_CanRetrieveSeq = false;
    m_DbName = NcbiEmptyString;
    m_NumAlignToShow = 1000000;
    m_AlignType = eNotSet;
    m_Rid = "0";
    m_CddRid = "0";
    m_EntrezTerm = NcbiEmptyString;
    m_QueryNumber = 0;
    m_BlastType = NcbiEmptyString;
    m_MidLineStyle = eBar;
    m_ConfigFile = NULL;
    m_Reg = NULL;
    m_DynamicFeature = NULL;    
    m_MasterGeneticCode = 1;
    m_SlaveGeneticCode = 1;
    m_Ctx = NULL;

    const SNCBIPackedScoreMatrix* packed_mtx = 
        BlastScoreBlkGetCompiledInMatrix(matrix_name);
    if (packed_mtx == NULL) {
        packed_mtx = &NCBISM_Blosum62;
    }

    SNCBIFullScoreMatrix mtx;
    NCBISM_Unpack(packed_mtx, &mtx);
 
    int** temp = new int*[k_NumAsciiChar];
    for(int i = 0; i<k_NumAsciiChar; ++i) {
        temp[i] = new int[k_NumAsciiChar];
    }
    for (int i=0; i<k_NumAsciiChar; i++){
        for (int j=0; j<k_NumAsciiChar; j++){
            temp[i][j] = -1000;
        }
    }
    for(int i = 0; i < ePMatrixSize; ++i){
        for(int j = 0; j < ePMatrixSize; ++j){
            temp[(size_t)k_PSymbol[i]][(size_t)k_PSymbol[j]] =
                mtx.s[(size_t)k_PSymbol[i]][(size_t)k_PSymbol[j]];
        }
    }
    for(int i = 0; i < ePMatrixSize; ++i) {
        temp[(size_t)k_PSymbol[i]]['*'] = temp['*'][(size_t)k_PSymbol[i]] = -4;
    }
    temp['*']['*'] = 1; 
    m_Matrix = temp;
}


CDisplaySeqalign::~CDisplaySeqalign()
{
    for(int i = 0; i<k_NumAsciiChar; ++i) {
        delete [] m_Matrix[i];
    }
    delete [] m_Matrix;
    if (m_ConfigFile) {
        delete m_ConfigFile;
    } 
    if (m_Reg) {
        delete m_Reg;
    }
    
    if(m_DynamicFeature){
        delete m_DynamicFeature;
    }
}

///show blast identity, positive etc.
///@param out: output stream
///@param aln_stop: stop in aln coords
///@param identity: identity
///@param positive: positives
///@param match: match
///@param gap: gap
///@param master_strand: plus strand = 1 and minus strand = -1
///@param slave_strand:  plus strand = 1 and minus strand = -1
///@param master_frame: frame for master
///@param slave_frame: frame for slave
///@param aln_is_prot: is protein alignment?
///
static void s_DisplayIdentityInfo(CNcbiOstream& out, int aln_stop, 
                                   int identity, int positive, int match,
                                   int gap, int master_strand, 
                                   int slave_strand, int master_frame, 
                                   int slave_frame, bool aln_is_prot)
{
    out<<" Identities = "<<match<<"/"<<(aln_stop+1)<<" ("<<identity<<"%"<<")";
    if(aln_is_prot){
        out<<", Positives = "<<(positive + match)<<"/"<<(aln_stop+1)
           <<" ("<<(((positive + match)*100)/(aln_stop+1))<<"%"<<")";
    }
    out<<", Gaps = "<<gap<<"/"<<(aln_stop+1)
       <<" ("<<((gap*100)/(aln_stop+1))<<"%"<<")"<<endl;
    if (!aln_is_prot){ 
        out<<" Strand="<<(master_strand==1 ? "Plus" : "Minus")
           <<"/"<<(slave_strand==1? "Plus" : "Minus")<<endl;
    }
    if(master_frame != 0 && slave_frame != 0) {
        out <<" Frame = " << ((master_frame > 0) ? "+" : "") 
            << master_frame <<"/"<<((slave_frame > 0) ? "+" : "") 
            << slave_frame<<endl;
    } else if (master_frame != 0){
        out <<" Frame = " << ((master_frame > 0) ? "+" : "") 
            << master_frame << endl;
    }  else if (slave_frame != 0){
        out <<" Frame = " << ((slave_frame > 0) ? "+" : "") 
            << slave_frame <<endl;
    } 
    out<<endl;
    
}

///wrap line
///@param out: output stream
///@param str: string to wrap
///
static void s_WrapOutputLine(CNcbiOstream& out, const string& str)
{
    const int line_len = 60;
    bool do_wrap = false;
    int length = (int) str.size();
    if (length > line_len) {
        for (int i = 0; i < length; i ++){
            if(i > 0 && i % line_len == 0){
                do_wrap = true;
            }   
            out << str[i];
            if(do_wrap && isspace((unsigned char) str[i])){
                out << endl;  
                do_wrap = false;
            }
        }
    } else {
        out << str;
    }
}

///To add color to bases other than identityChar
///@param seq: sequence
///@param identity_char: identity character
///@param out: output stream
///
static void s_ColorDifferentBases(string& seq, char identity_char,
                                  CNcbiOstream& out){
    string base_color = k_ColorRed;
    bool tagOpened = false;
    for(int i = 0; i < (int)seq.size(); i ++){
        if(seq[i] != identity_char){
            if(!tagOpened){
                out << "<font color=\""+base_color+"\"><b>";
                tagOpened =  true;
            }
            
        } else {
            if(tagOpened){
                out << "</b></font>";
                tagOpened = false;
            }
        }
        out << seq[i];
        if(tagOpened && i == (int)seq.size() - 1){
            out << "</b></font>";
            tagOpened = false;
        }
    } 
}

///return the frame for a given strand
///Note that start is zero bases.  It returns frame +/-(1-3). 0 indicates error
///@param start: sequence start position
///@param strand: strand
///@param id: the seqid
///@param scope: the scope
///@return: the frame
///
static int s_GetFrame (int start, ENa_strand strand, const CSeq_id& id, 
                       CScope& sp) 
{
    int frame = 0;
    if (strand == eNa_strand_plus) {
        frame = (start % 3) + 1;
    } else if (strand == eNa_strand_minus) {
        frame = -(((int)sp.GetBioseqHandle(id).GetBioseqLength() - start - 1)
                  % 3 + 1);
        
    }
    return frame;
}

///reture the frame for master seq in stdseg
///@param ss: the input stdseg
///@param scope: the scope
///@return: the frame
///
static int s_GetStdsegMasterFrame(const CStd_seg& ss, CScope& scope)
{
    const CRef<CSeq_loc> slc = ss.GetLoc().front();
    ENa_strand strand = GetStrand(*slc);
    int frame = s_GetFrame(strand ==  eNa_strand_plus ?
                           GetStart(*slc, &scope) : GetStop(*slc, &scope),
                           strand ==  eNa_strand_plus ?
                           eNa_strand_plus : eNa_strand_minus,
                           *(ss.GetIds().front()), scope);
    return frame;
}

///return the get sequence table for html display
///@param form_name: form name
///@parm db_is_na: is the db of nucleotide type?
///@query_number: the query number
///@return: the from string
///
static string s_GetSeqForm(char* form_name, bool db_is_na, int query_number,
                           int db_type, const string& dbName,const char *rid, const char *queryID,bool showTreeButtons)
{
    string temp_buf = NcbiEmptyString;
    char* buf = new char[dbName.size() + 4096];
    if(form_name){             
        string localClientButtons = "";
        if(showTreeButtons) {
	    string l_GetTreeViewForm  = CBlastFormatUtil::GetURLFromRegistry( "TREEVIEW_FRM");
            localClientButtons = "<td>" + l_GetTreeViewForm + "</td>";
        }
	string l_GetSeqSubmitForm  = CBlastFormatUtil::GetURLFromRegistry( "GETSEQ_SUB_FRM", db_type); 
	string l_GetSeqSelectForm = CBlastFormatUtil::GetURLFromRegistry( "GETSEQ_SEL_FRM");
	
        string template_str = "<table border=\"0\"><tr><td>" +
	                    l_GetSeqSubmitForm + //k_GetSeqSubmitForm[db_type] +
                            "</td><td>" +
 	                    l_GetSeqSelectForm + //k_GetSeqSelectForm +
                            "</td>" +
                            localClientButtons +
                            "</tr></table>";

        if(showTreeButtons) {
            sprintf(buf, template_str.c_str(), form_name, query_number,
                db_is_na?1:0, query_number, form_name, query_number, db_type, 
                query_number,query_number,             
                    rid,queryID,form_name,query_number,rid,query_number,form_name,query_number);  	        
        
        }
        else {
             sprintf(buf, template_str.c_str(), form_name, query_number,
                db_is_na?1:0, query_number, form_name, query_number, db_type, 
                query_number,query_number);              
        }

    }
    temp_buf = buf;
    delete [] buf;
    return temp_buf;
}

///Gets Query Seq ID from Seq Align
///@param actual_aln_list: align set for one query
///@return: string query ID
static string s_GetQueryIDFromSeqAlign(const CSeq_align_set& actual_aln_list) 
{
    CRef<CSeq_align> first_aln = actual_aln_list.Get().front();
    const CSeq_id& query_SeqID = first_aln->GetSeq_id(0);
    string queryID;
    query_SeqID.GetLabel(&queryID);    
    return queryID;
}

///return id type specified or null ref
///@param ids: the input ids
///@param choice: id of choice
///@return: the id with specified type
///
static CRef<CSeq_id> s_GetSeqIdByType(const list<CRef<CSeq_id> >& ids, 
                                      CSeq_id::E_Choice choice)
{
    CRef<CSeq_id> cid;
    
    for (CBioseq::TId::const_iterator iter = ids.begin(); iter != ids.end(); 
         iter ++){
        if ((*iter)->Which() == choice){
            cid = *iter;
            break;
        }
    }
    
    return cid;
}

///return gi from id list
///@param ids: the input ids
///@return: the gi if found
///
static int s_GetGiForSeqIdList (const list<CRef<CSeq_id> >& ids)
{
    int gi = 0;
    CRef<CSeq_id> id = s_GetSeqIdByType(ids, CSeq_id::e_Gi);
    if (!(id.Empty())){
        return id->GetGi();
    }
    return gi;
}


///return concatenated exon sequence
///@param feat: the feature containing this cds
///@param feat_strand: the feature strand
///@param range: the range list of seqloc
///@param total_coding_len: the total exon length excluding intron
///@param raw_cdr_product: the raw protein sequence
///@return: the concatenated exon sequences with amino acid aligned to 
///to the second base of a codon
///
static string s_GetConcatenatedExon(CFeat_CI& feat,  
                                    ENa_strand feat_strand, 
                                    list<CRange<TSeqPos> >& range,
                                    TSeqPos total_coding_len,
                                    string& raw_cdr_product, TSeqPos frame_adj)
{

    string concat_exon(total_coding_len, ' ');
    TSeqPos frame = 1;
    const CCdregion& cdr = feat->GetData().GetCdregion();
    if(cdr.IsSetFrame()){
        frame = cdr.GetFrame();
    }
    TSeqPos num_coding_base;
    int num_base;
    TSeqPos coding_start_base;
    if(feat_strand == eNa_strand_minus){
        coding_start_base = total_coding_len - 1 - (frame -1) - frame_adj;
        num_base = total_coding_len - 1;
        num_coding_base = 0;
        
    } else {
        coding_start_base = 0; 
        coding_start_base += frame - 1 + frame_adj;
        num_base = 0;
        num_coding_base = 0;
    }
    
    ITERATE(list<CRange<TSeqPos> >, iter, range){
        //note that feature on minus strand needs to be
        //filled backward.
        if(feat_strand != eNa_strand_minus){
            for(TSeqPos i = 0; i < iter->GetLength(); i ++){
                if((TSeqPos)num_base >= coding_start_base){
                    num_coding_base ++;
                    if(num_coding_base % 3 == 2){
                        //a.a to the 2nd base
                        if(num_coding_base / 3 < raw_cdr_product.size()){
                            //make sure the coding region is no
                            //more than the protein seq as there
                            //could errors in ncbi record
                            concat_exon[num_base] 
                                = raw_cdr_product[num_coding_base / 3];
                        }                           
                    }
                }
                num_base ++;
            }    
        } else {
            
            for(TSeqPos i = 0; i < iter->GetLength() &&
                    num_base >= 0; i ++){
                if((TSeqPos)num_base <= coding_start_base){
                    num_coding_base ++;
                    if(num_coding_base % 3 == 2){
                        //a.a to the 2nd base
                        if(num_coding_base / 3 < 
                           raw_cdr_product.size() &&
                           coding_start_base >= num_coding_base){
                            //make sure the coding region is no
                            //more than the protein seq as there
                            //could errors in ncbi record
                            concat_exon[num_base] 
                                = raw_cdr_product[num_coding_base / 3];
                        }                           
                    }
                }
                num_base --;
            }    
        }
    }
    return concat_exon;
}

///map slave feature info to master seq
///@param master_feat_range: master feature seqloc to be filled
///@param feat: the feature in concern
///@param slave_feat_range: feature info for slave
///@param av: the alignment vector for master-slave seqalign
///@param row: the row
///@param frame_adj: frame adjustment
///

static void s_MapSlaveFeatureToMaster(list<CRange<TSeqPos> >& master_feat_range,
                                      ENa_strand& master_feat_strand, CFeat_CI& feat,
                                      list<CSeq_loc_CI::TRange>& slave_feat_range,
                                      ENa_strand slave_feat_strand,
                                      CAlnVec* av, 
                                      int row, TSeqPos frame_adj)
{
    TSeqPos trans_frame = 1;
    const CCdregion& cdr = feat->GetData().GetCdregion();
    if(cdr.IsSetFrame()){
        trans_frame = cdr.GetFrame();
    }
    trans_frame += frame_adj;

    TSeqPos prev_exon_len = 0;
    bool is_first_in_range = true;

    if ((av->IsPositiveStrand(1) && slave_feat_strand == eNa_strand_plus) || 
        (av->IsNegativeStrand(1) && slave_feat_strand == eNa_strand_minus)) {
        master_feat_strand = eNa_strand_plus;
    } else {
        master_feat_strand = eNa_strand_minus;
    }
    
    list<CSeq_loc_CI::TRange> acutal_slave_feat_range = slave_feat_range;

    ITERATE(list<CSeq_loc_CI::TRange>, iter_temp,
            acutal_slave_feat_range){
        CRange<TSeqPos> actual_feat_seq_range = av->GetSeqRange(row).
            IntersectionWith(*iter_temp);          
        if(!actual_feat_seq_range.Empty()){
            TSeqPos slave_aln_from = 0, slave_aln_to = 0;
            TSeqPos frame_offset = 0;
            int curr_exon_leading_len = 0;
            //adjust frame 
            if (is_first_in_range) {  
                if (slave_feat_strand == eNa_strand_plus) {
                    curr_exon_leading_len 
                        = actual_feat_seq_range.GetFrom() - iter_temp->GetFrom();
                    
                } else {
                    curr_exon_leading_len 
                        = iter_temp->GetTo() - actual_feat_seq_range.GetTo();
                }
                is_first_in_range = false;
                frame_offset = (3 - (prev_exon_len + curr_exon_leading_len)%3
                                + (trans_frame - 1)) % 3;
            }
            
            if (av->IsPositiveStrand(1) && 
                slave_feat_strand == eNa_strand_plus) {
                slave_aln_from 
                    = av->GetAlnPosFromSeqPos(row, actual_feat_seq_range.GetFrom() + 
                                              frame_offset, CAlnMap::eRight );   
                
                slave_aln_to =
                    av->GetAlnPosFromSeqPos(row, actual_feat_seq_range.GetTo(),
                                            CAlnMap::eLeft);
            } else if (av->IsNegativeStrand(1) && 
                       slave_feat_strand == eNa_strand_plus) {
                
                slave_aln_from 
                    = av->GetAlnPosFromSeqPos(row, actual_feat_seq_range.GetTo(),
                                              CAlnMap::eRight);   
                
                slave_aln_to =
                    av->GetAlnPosFromSeqPos(row,
                                            actual_feat_seq_range.GetFrom() +
                                            frame_offset, CAlnMap::eLeft);
            }  else if (av->IsPositiveStrand(1) && 
                        slave_feat_strand == eNa_strand_minus) {
                slave_aln_from 
                    = av->GetAlnPosFromSeqPos(row, actual_feat_seq_range.GetFrom(),
                                              CAlnMap::eRight);   
                
                slave_aln_to =
                    av->GetAlnPosFromSeqPos(row, actual_feat_seq_range.GetTo() -
                                            frame_offset, CAlnMap::eLeft);

            } else if (av->IsNegativeStrand(1) && 
                       slave_feat_strand == eNa_strand_minus){
                slave_aln_from 
                    = av->GetAlnPosFromSeqPos(row, actual_feat_seq_range.GetTo() - 
                                              frame_offset, CAlnMap::eRight );   
                
                slave_aln_to =
                    av->GetAlnPosFromSeqPos(row, actual_feat_seq_range.GetFrom(),
                                            CAlnMap::eLeft);
            }
            
            TSeqPos master_from = 
                av->GetSeqPosFromAlnPos(0, slave_aln_from, CAlnMap::eRight);
            
            TSeqPos master_to = 
                av->GetSeqPosFromAlnPos(0, slave_aln_to, CAlnMap::eLeft);
            
            CRange<TSeqPos> master_range(master_from, master_to);
            master_feat_range.push_back(master_range); 
            
        }
        prev_exon_len += iter_temp->GetLength();
    }
}



///return cds coded sequence and fill the id if found
///@param genetic_code: the genetic code
///@param feat: the feature containing this cds
///@param scope: scope to fetch sequence
///@param range: the range list of seqloc
///@param handle: the bioseq handle
///@param feat_strand: the feature strand
///@param feat_id: the feature id to be filled
///@param frame_adj: frame adjustment
///@param mix_loc: is this seqloc mixed with other seqid?
///@return: the encoded protein sequence
///
static string s_GetCdsSequence(int genetic_code, CFeat_CI& feat, 
                               CScope& scope, list<CRange<TSeqPos> >& range,
                               const CBioseq_Handle& handle, 
                               ENa_strand feat_strand, string& feat_id,
                               TSeqPos frame_adj, bool mix_loc)
{
    string raw_cdr_product = NcbiEmptyString;
    if(feat->IsSetProduct() && feat->GetProduct().IsWhole() && !mix_loc){
        //show actual aa  if there is a cds product
          
        const CSeq_id& productId = 
            feat->GetProduct().GetWhole();
        const CBioseq_Handle& productHandle 
            = scope.GetBioseqHandle(productId );
        feat_id = "CDS:" + 
            GetTitle(productHandle).substr(0, k_FeatureIdLen);
        productHandle.
            GetSeqVector(CBioseq_Handle::eCoding_Iupac).
            GetSeqData(0, productHandle.
            GetBioseqLength(), raw_cdr_product);
    } else { 
        CSeq_loc isolated_loc;
        ITERATE(list<CRange<TSeqPos> >, iter, range){
            TSeqPos from = iter->GetFrom();
            TSeqPos to = iter->GetTo();
            if(feat_strand == eNa_strand_plus){
                isolated_loc.
                    Add(*(handle.GetRangeSeq_loc(from + frame_adj,
                                                 to,         
                                                 feat_strand)));
            } else {
                isolated_loc.
                    Add(*(handle.GetRangeSeq_loc(from,
                                                 to - frame_adj,   
                                                 feat_strand)));
            }
        }
        CGenetic_code gc;
        CRef<CGenetic_code::C_E> ce(new CGenetic_code::C_E);
        ce->Select(CGenetic_code::C_E::e_Id);
        ce->SetId(genetic_code);
        gc.Set().push_back(ce);
        CSeqTranslator::Translate(isolated_loc, handle,
                                  raw_cdr_product, &gc);
      
    }
    return raw_cdr_product;
}

///fill the cds start positions (1 based)
///@param line: the input cds line
///@param concat_exon: exon only string
///@param length_per_line: alignment length per line
///@param feat_aln_start_totalexon: feature aln pos in concat_exon
///@param strand: the alignment strand
///@param start: start list to be filled
///
static void s_FillCdsStartPosition(string& line, string& concat_exon,
                                   size_t length_per_line,
                                   TSeqPos feat_aln_start_totalexon,
                                   ENa_strand seq_strand, 
                                   ENa_strand feat_strand,
                                   list<TSeqPos>& start)
{
    size_t actual_line_len = 0;
    size_t aln_len = line.size();
    TSeqPos previous_num_letter = 0;
    
    //the number of amino acids preceeding this exon start position
    for (size_t i = 0; i <= feat_aln_start_totalexon; i ++){
        if(feat_strand == eNa_strand_minus){
            //remember the amino acid in this case goes backward
            //therefore we count backward too
            
            int pos = concat_exon.size() -1 - i;
            if(pos >= 0 && isalpha((unsigned char) concat_exon[pos])){
                previous_num_letter ++;
            }
            
        } else {
            if(isalpha((unsigned char) concat_exon[i])){
                previous_num_letter ++;
            }
        }
    }
    
    
    TSeqPos prev_num = 0;
    //go through the entire feature line and get the amino acid position 
    //for each line
    for(size_t i = 0; i < aln_len; i += actual_line_len){
        //handle the last row which may be shorter
        if(aln_len - i< length_per_line) {
            actual_line_len = aln_len - i;
        } else {
            actual_line_len = length_per_line;
        }
        //the number of amino acids on this row
        TSeqPos cur_num = 0;
        bool has_intron = false;
        
        //go through each character on a row
        for(size_t j = i; j < actual_line_len + i; j ++){
            //don't count gap
            if(isalpha((unsigned char) line[j])){
                cur_num ++;
            } else if(line[j] == k_IntronChar){
                has_intron = true;
            }
        }
            
        if(cur_num > 0){
            if(seq_strand == eNa_strand_plus){
                if(feat_strand == eNa_strand_minus) {
                    start.push_back(previous_num_letter - prev_num); 
                } else {
                    start.push_back(previous_num_letter + prev_num);  
                }
            } else {
                if(feat_strand == eNa_strand_minus) {
                    start.push_back(previous_num_letter + prev_num);  
                } else {
                    start.push_back(previous_num_letter - prev_num);  
                } 
            }
        } else if (has_intron) {
            start.push_back(0);  //sentinal for no show
        }
        prev_num += cur_num;
    }
}

///make a new copy of master seq with feature info and return the scope
///that contains this sequence
///@param feat_range: the feature seqlocs
///@param feat_seq_strand: the stand info
///@param handle: the seq handle for the original master seq
///@return: the scope containing the new master seq
///
static CRef<CScope> s_MakeNewMasterSeq(list<list<CRange<TSeqPos> > >& feat_range,
                                       list<ENa_strand>& feat_seq_strand,
                                       const CBioseq_Handle& handle) 
{
    CRef<CObjectManager> obj;
    obj = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*obj);       
    CRef<CScope> scope (new CScope(*obj));
    scope->AddDefaults();
    CRef<CBioseq> cbsp(new CBioseq());
    cbsp->Assign(*(handle.GetCompleteBioseq()));

    CBioseq::TAnnot& anot_list = cbsp->SetAnnot();
    CRef<CSeq_annot> anot(new CSeq_annot);
    CRef<CSeq_annot::TData> data(new CSeq_annot::TData);
    data->Select(CSeq_annot::TData::e_Ftable);
    anot->SetData(*data);
    CSeq_annot::TData::TFtable& ftable = anot->SetData().SetFtable();
    int counter = 0;
    ITERATE(list<list<CRange<TSeqPos> > >, iter, feat_range) {
        counter ++;
        CRef<CSeq_feat> seq_feat(new CSeq_feat);
        CRef<CSeqFeatData> feat_data(new CSeqFeatData);
        feat_data->Select(CSeq_feat::TData::e_Cdregion);
        seq_feat->SetData(*feat_data);
        seq_feat->SetComment("Putative " + NStr::IntToString(counter));
        CRef<CSeq_loc> seq_loc (new CSeq_loc);
       
        ITERATE(list<CRange<TSeqPos> >, iter2, *iter) {
            seq_loc->Add(*(handle.GetRangeSeq_loc(iter2->GetFrom(),
                                                  iter2->GetTo(),
                                                  feat_seq_strand.front())));
        }
        seq_feat->SetLocation(*seq_loc);
        ftable.push_back(seq_feat);
        feat_seq_strand.pop_front();
    }
    anot_list.push_back(anot);
    CRef<CSeq_entry> entry(new CSeq_entry());
    entry->SetSeq(*cbsp);
    scope->AddTopLevelSeqEntry(*entry);
  
    return scope;
}

//output feature lines
//@param reference_feat_line: the master feature line to be compared 
//for coloring
//@param feat_line: the slave feature line
//@param color_feat_mismatch: color or not
//@param start: the alignment pos
//@param len: the length per line
//@param out: stream for output
//
static void s_OutputFeature(string& reference_feat_line, 
                            string& feat_line,
                            bool color_feat_mismatch,
                            int start,
                            int len,  
                            CNcbiOstream& out)
{
    if((int)feat_line.size() > start){
        string actual_feat = feat_line.substr(start, len);
        string actual_reference_feat = NcbiEmptyString;
        if(reference_feat_line != NcbiEmptyString){
            actual_reference_feat = reference_feat_line.substr(start, len);
        }
        if(color_feat_mismatch 
           && actual_reference_feat != NcbiEmptyString &&
           !NStr::IsBlank(actual_reference_feat)){
            string base_color = k_ColorPink;
            bool tagOpened = false;
            for(int i = 0; i < (int)actual_feat.size() &&
                    i < (int)actual_reference_feat.size(); i ++){
                if (actual_feat[i] != actual_reference_feat[i]) {
                    if(actual_feat[i] != ' ' &&
                       actual_feat[i] != k_IntronChar &&
                       actual_reference_feat[i] != k_IntronChar) {
                        if(!tagOpened){
                            out << "<font color=\""+base_color+"\"><b>";
                            tagOpened =  true;
                        }
                        
                    }
                } else {
                    if (actual_feat[i] != ' '){ //no close if space to 
                        //minimizing the open and close of tags
                        if(tagOpened){
                            out << "</b></font>";
                            tagOpened = false;
                        }
                    }
                }
                out << actual_feat[i];
                //close tag at the end of line
                if(tagOpened && i == (int)actual_feat.size() - 1){
                    out << "</b></font>";
                    tagOpened = false;
                }
            }
        } else {
            out << actual_feat;
        }
    }
    
}


void CDisplaySeqalign::x_PrintFeatures(list<SAlnFeatureInfo*> feature,
                                       int row, 
                                       CAlnMap::TSignedRange alignment_range,
                                       int aln_start,
                                       int line_length, 
                                       int id_length,
                                       int start_length,
                                       int max_feature_num, 
                                       string& master_feat_str,
                                       CNcbiOstream& out)
{
    for (list<SAlnFeatureInfo*>::iterator 
             iter=feature.begin(); 
         iter != feature.end(); iter++){
        //check blank string for cases where CDS is in range 
        //but since it must align with the 2nd codon and is 
        //actually not in range
        if (alignment_range.IntersectingWith((*iter)->aln_range) && 
            !(NStr::IsBlank((*iter)->feature_string.
                            substr(aln_start, line_length)) &&
              m_AlignOption & eShowCdsFeature)){  
            if((m_AlignOption&eHtml)&&(m_AlignOption&eMergeAlign)
               && (m_AlignOption&eSequenceRetrieval && m_CanRetrieveSeq)){
                char checkboxBuf[200];
                sprintf(checkboxBuf,  k_UncheckabeCheckbox.c_str(),
                        m_QueryNumber);
                out << checkboxBuf;
            }
            out<<(*iter)->feature->feature_id;
            if((*iter)->feature_start.empty()){
                CBlastFormatUtil::
                    AddSpace(out, id_length + k_IdStartMargin
                             +start_length + k_StartSequenceMargin
                             -(*iter)->feature->feature_id.size());
            } else {
                int feat_start = (*iter)->feature_start.front();
                if(feat_start > 0){
                    CBlastFormatUtil::
                        AddSpace(out, id_length + k_IdStartMargin
                                 -(*iter)->feature->feature_id.size());
                    out << feat_start;
                    CBlastFormatUtil::
                        AddSpace(out, start_length -
                                 NStr::IntToString(feat_start).size() +
                                 k_StartSequenceMargin);
                } else { //no show start
                    CBlastFormatUtil::
                        AddSpace(out, id_length + k_IdStartMargin
                                 +start_length + k_StartSequenceMargin
                                 -(*iter)->feature->feature_id.size());
                }
                
                (*iter)->feature_start.pop_front();
            }
            bool color_cds_mismatch = false; 
            if(max_feature_num == 1 && (m_AlignOption & eHtml) && 
               (m_AlignOption & eShowCdsFeature) && row > 0){
                //only for slaves, only for cds feature
                //only color mismach if only one cds exists
                color_cds_mismatch = true;
            }
            s_OutputFeature(master_feat_str, 
                            (*iter)->feature_string,
                            color_cds_mismatch, aln_start,
                            line_length, out);
            if(row == 0){//set master feature as reference
                master_feat_str = (*iter)->feature_string;
            }
            out<<endl;
        }
    }
    
}


string CDisplaySeqalign::x_GetUrl(const list<CRef<CSeq_id> >& ids, int gi, 
                                  int row, int taxid, int linkout) const
                                  
{
    string urlLink = NcbiEmptyString;
    char dopt[32], db[32];
    char logstr_moltype[32], logstr_location[32];
    bool hit_not_in_mapviewer = !(linkout & eHitInMapviewer);
    
    gi = (gi == 0) ? s_GetGiForSeqIdList(ids):gi;
    string user_url= m_Reg->Get(m_BlastType, "TOOL_URL");

    if (user_url != NcbiEmptyString && 
        !((user_url.find("dumpgnl.cgi") != string::npos && gi > 0) || 
          (user_url.find("maps.cgi") != string::npos && hit_not_in_mapviewer &&
          m_AlignOption & eShowSortControls))) { 
        //need to use url in configuration file
        string altUrl = NcbiEmptyString;
        urlLink = x_GetDumpgnlLink(ids, row, altUrl, taxid);
    } else {
        
        char urlBuf[2048];
        string temp_class_info = kClassInfo + " ";
        if (gi > 0) {
            //use entrez or dbtag specified
            if(m_IsDbNa) {
                strcpy(dopt, "GenBank");
                strcpy(db, "Nucleotide");
                strcpy(logstr_moltype, "nucl");
            } else {
                strcpy(dopt, "GenPept");
                strcpy(db, "Protein");
                strcpy(logstr_moltype, "prot");
            }   

            strcpy(logstr_location, "align");

            string l_EntrezUrl = CBlastFormatUtil::GetURLFromRegistry("ENTREZ");            
            sprintf(urlBuf, l_EntrezUrl.c_str(), 
                    (m_AlignOption & eShowInfoOnMouseOverSeqid) ? 
                    temp_class_info.c_str() : "", db, gi, dopt, m_Rid.c_str(),
                    logstr_moltype, logstr_location, m_cur_align,
                    (m_AlignOption & eNewTargetWindow) ? 
                    "TARGET=\"EntrezView\"" : "");
            urlLink = urlBuf;
        } else {//seqid general, dbtag specified
            const CRef<CSeq_id> wid = FindBestChoice(ids, CSeq_id::WorstRank);
            if(wid->Which() == CSeq_id::e_General){
                const CDbtag& dtg = wid->GetGeneral();
                const string& dbName = dtg.GetDb();
                if(NStr::CompareNocase(dbName, "TI") == 0){
                    sprintf(urlBuf, kTraceUrl.c_str(),
                            (m_AlignOption & eShowInfoOnMouseOverSeqid) ?
                            temp_class_info.c_str() : "",
                            wid->GetSeqIdString().c_str(), m_Rid.c_str());
                    urlLink = urlBuf;
                } else { //future use
                    
                }
            }
        }
    }
    return urlLink;
}


void CDisplaySeqalign::x_AddLinkout(const CBioseq& cbsp, 
                                    const CBlast_def_line& bdl,
                                    int first_gi, int gi, 
                                    CNcbiOstream& out) const
{
    char molType[8]={""};
    if(cbsp.IsAa()){
        sprintf(molType, "[pgi]");
    }
    else {
        sprintf(molType, "[ngi]");
    }
    
    if (bdl.IsSetLinks()){
      string l_GeoUrl = CBlastFormatUtil::GetURLFromRegistry("GEO");
      string l_UnigeneUrl = CBlastFormatUtil::GetURLFromRegistry("UNIGEN");
      string l_GeneUrl = CBlastFormatUtil::GetURLFromRegistry("GENE");
        for (list< int >::const_iterator iter = bdl.GetLinks().begin(); 
             iter != bdl.GetLinks().end(); iter ++){
            char buf[1024];
            
            if ((*iter) & eUnigene) {

                sprintf(buf, l_UnigeneUrl.c_str(), 
                        !cbsp.IsAa() ? "nucleotide" : "protein", 
                        !cbsp.IsAa() ? "nucleotide" : "protein", gi,
                        m_Rid.c_str(),
                        "align", m_cur_align);
                out << buf;
            }
            if ((*iter) & eStructure){
                sprintf(buf, kStructureUrl.c_str(), m_Rid.c_str(), first_gi,
                        gi, m_CddRid.c_str(), "onepair", 
                        (m_EntrezTerm == NcbiEmptyString) ? 
                        "none":((char*) m_EntrezTerm.c_str()),
                        "align", m_cur_align);
                out << buf;
            }
            if ((*iter) & eGeo){

                sprintf(buf, l_GeoUrl.c_str(), gi, m_Rid.c_str(),
                        "align", m_cur_align);
                out << buf;
            }
            if((*iter) & eGene){
                sprintf(buf, l_GeneUrl.c_str(), gi, cbsp.IsAa() ? 
                        "PUID" : "NUID", m_Rid.c_str(),
                        "align", m_cur_align);
                out << buf;
            }
        }
    }
}


void CDisplaySeqalign::x_DisplayAlnvec(CNcbiOstream& out)
{ 
    size_t maxIdLen=0, maxStartLen=0, startLen=0, actualLineLen=0;
    size_t aln_stop=m_AV->GetAlnStop();
    const int rowNum=m_AV->GetNumRows();   
    if(m_AlignOption & eMasterAnchored){
        m_AV->SetAnchor(0);
    }
    m_AV->SetGapChar('-');
    m_AV->SetEndChar(' ');
    vector<string> sequence(rowNum);
    CAlnMap::TSeqPosList* seqStarts = new CAlnMap::TSeqPosList[rowNum];
    CAlnMap::TSeqPosList* seqStops = new CAlnMap::TSeqPosList[rowNum];
    CAlnMap::TSeqPosList* insertStart = new CAlnMap::TSeqPosList[rowNum];
    CAlnMap::TSeqPosList* insertAlnStart = new CAlnMap::TSeqPosList[rowNum];
    CAlnMap::TSeqPosList* insertLength = new CAlnMap::TSeqPosList[rowNum];
    string* seqidArray=new string[rowNum];
    string middleLine;
    CAlnMap::TSignedRange* rowRng = new CAlnMap::TSignedRange[rowNum];
    int* frame = new int[rowNum];
    int* taxid = new int[rowNum];
    int max_feature_num = 0;
    string master_feat_str = NcbiEmptyString;
    
    //Add external query feature info such as phi blast pattern
    list<SAlnFeatureInfo*>* bioseqFeature= x_GetQueryFeatureList(rowNum,
                                                                (int)aln_stop);
    //conver to aln coordinates for mask seqloc
    list<SAlnSeqlocInfo*> alnLocList;
    x_FillLocList(alnLocList);   
    
    //prepare data for each row 
    list<list<CRange<TSeqPos> > > feat_seq_range;
    list<ENa_strand> feat_seq_strand;
    for (int row=0; row<rowNum; row++) {
        string type_temp = m_BlastType;
        type_temp = NStr::TruncateSpaces(NStr::ToLower(type_temp));
        if(type_temp == "mapview" || type_temp == "mapview_prev" || 
           type_temp == "gsfasta"){
            taxid[row] = CBlastFormatUtil::GetTaxidForSeqid(m_AV->GetSeqId(row),
                                                            m_Scope);
        } else {
            taxid[row] = 0;
        }
        rowRng[row] = m_AV->GetSeqAlnRange(row);
        frame[row] = (m_AV->GetWidth(row) == 3 ? 
                      s_GetFrame(m_AV->IsPositiveStrand(row) ? 
                                 m_AV->GetSeqStart(row) : 
                                 m_AV->GetSeqStop(row), 
                                 m_AV->IsPositiveStrand(row) ? 
                                 eNa_strand_plus : eNa_strand_minus, 
                                 m_AV->GetSeqId(row), m_Scope) : 0);        
        //make sequence
        m_AV->GetWholeAlnSeqString(row, sequence[row], &insertAlnStart[row],
                                   &insertStart[row], &insertLength[row],
                                   (int)m_LineLen, &seqStarts[row], &seqStops[row]);
        //make feature. Only for pairwise and untranslated for subject nuc seq
        if(!(m_AlignOption & eMasterAnchored) &&
           !(m_AlignOption & eMergeAlign) && m_AV->GetWidth(row) != 3 &&
           !(m_AlignType & eProt)){
            if(m_AlignOption & eShowCdsFeature){
                int master_gi = FindGi(m_AV->GetBioseqHandle(0).
                                       GetBioseqCore()->GetId());
                x_GetFeatureInfo(bioseqFeature[row], *m_featScope, 
                                 CSeqFeatData::e_Cdregion, row, sequence[row],
                                 feat_seq_range, feat_seq_strand,
                                 row == 1 && !(master_gi > 0) ? true : false);
                
                if(!(feat_seq_range.empty()) && row == 1) {
                    //make a new copy of master bioseq and add the feature from
                    //slave to make putative cds feature 
                    CRef<CScope> master_scope_with_feat = 
                        s_MakeNewMasterSeq(feat_seq_range, feat_seq_strand,
                                           m_AV->GetBioseqHandle(0));
                    //make feature string for master bioseq
                    list<list<CRange<TSeqPos> > > temp_holder;
                    x_GetFeatureInfo(bioseqFeature[0], *master_scope_with_feat, 
                                     CSeqFeatData::e_Cdregion, 0, sequence[0],
                                     temp_holder, feat_seq_strand, false);
                }
            }
            if(m_AlignOption & eShowGeneFeature){
                x_GetFeatureInfo(bioseqFeature[row], *m_featScope,
                                 CSeqFeatData::e_Gene, row, sequence[row],
                                 feat_seq_range, feat_seq_strand, false);
            }
        }
        //make id
        x_FillSeqid(seqidArray[row], row);
        maxIdLen=max<size_t>(seqidArray[row].size(), maxIdLen);
        size_t maxCood=max<size_t>(m_AV->GetSeqStart(row), m_AV->GetSeqStop(row));
        maxStartLen = max<size_t>(NStr::IntToString(maxCood).size(), maxStartLen);
    }
    for(int i = 0; i < rowNum; i ++){//adjust max id length for feature id 
        int num_feature = 0;
        for (list<SAlnFeatureInfo*>::iterator iter=bioseqFeature[i].begin();
             iter != bioseqFeature[i].end(); iter++){
            maxIdLen=max<size_t>((*iter)->feature->feature_id.size(), maxIdLen );
            num_feature ++;
            if(num_feature > max_feature_num){
                max_feature_num = num_feature;
            }
        }
    }  //end of preparing row data
    bool colorMismatch = false; //color the mismatches
    //output identities info 
    if(((m_AlignOption & eShowBlastInfo) || (m_AlignOption & eShowMiddleLine))
       && !(m_AlignOption & eMergeAlign)) {
        int match = 0;
        int positive = 0;
        int gap = 0;
        int identity = 0;
        x_FillIdentityInfo(sequence[0], sequence[1], match, positive, middleLine);
        if(m_AlignOption & eShowBlastInfo){
            identity = (match*100)/((int)aln_stop+1);
            if(identity >= k_ColorMismatchIdentity && identity <100 &&
               (m_AlignOption & eColorDifferentBases)){
                colorMismatch = true;
            }
            gap = x_GetNumGaps();
            s_DisplayIdentityInfo(out, (int)aln_stop, identity, positive, match, gap,
                                  m_AV->StrandSign(0), m_AV->StrandSign(1),
                                  frame[0], frame[1], 
                                  ((m_AlignType & eProt) != 0 ? true : false));
        }
    }
    //output rows
    for(int j=0; j<=(int)aln_stop; j+=(int)m_LineLen){
        //output according to aln coordinates
        if(aln_stop-j+1<m_LineLen) {
            actualLineLen=aln_stop-j+1;
        } else {
            actualLineLen=m_LineLen;
        }
        CAlnMap::TSignedRange curRange(j, j+(int)actualLineLen-1);
        //here is each row
        for (int row=0; row<rowNum; row++) {
            bool hasSequence = true;   
            hasSequence = curRange.IntersectingWith(rowRng[row]);
            //only output rows that have sequence
            if (hasSequence){
                int start = seqStarts[row].front() + 1;  //+1 for 1 based
                int end = seqStops[row].front() + 1;
                list<string> inserts;
                string insertPosString;  //the one with "\" to indicate insert
                if(m_AlignOption & eMasterAnchored){
                    list<SInsertInformation*> insertList;
                    x_GetInserts(insertList, insertAlnStart[row], 
                                 insertStart[row], insertLength[row],  
                                 j + (int)m_LineLen);
                    x_FillInserts(row, curRange, j, inserts, insertPosString, 
                                  insertList);
                    ITERATE(list<SInsertInformation*>, iterINsert, insertList){
                        delete *iterINsert;
                    }
                }
                //feature for query
                if(row == 0){          
                    x_PrintFeatures(bioseqFeature[row], row, curRange,
                                    j,(int)actualLineLen, maxIdLen,
                                    maxStartLen, max_feature_num, 
                                    master_feat_str, out); 
                }

                string urlLink = NcbiEmptyString;
                //setup url link for seqid
                if(m_AlignOption & eHtml){
                    int gi = 0;
                    if(m_AV->GetSeqId(row).Which() == CSeq_id::e_Gi){
                        gi = m_AV->GetSeqId(row).GetGi();
                    }
                    if(!(gi > 0)){
                        gi = s_GetGiForSeqIdList(m_AV->GetBioseqHandle(row).
                                                 GetBioseqCore()->GetId());
                    }
                    if((row == 0 && (m_AlignOption & eHyperLinkMasterSeqid)) ||
                       (row > 0 && (m_AlignOption & eHyperLinkSlaveSeqid))){
                        
                        if(gi > 0){
                            out<<"<a name="<<gi<<"></a>";
                        } else {
                            out<<"<a name="<<seqidArray[row]<<"></a>";
                        }
                    }
                    //get sequence checkbox
                    if((m_AlignOption & eMergeAlign) && 
                       (m_AlignOption & eSequenceRetrieval) && m_CanRetrieveSeq){
                        char checkboxBuf[512];
                        if (row == 0) {
                            sprintf(checkboxBuf, k_UncheckabeCheckbox.c_str(),
                                    m_QueryNumber); 
                        } else {
                            sprintf(checkboxBuf, k_Checkbox.c_str(), gi > 0 ?
                                    NStr::IntToString(gi).c_str() :
                                    seqidArray[row].c_str(), m_QueryNumber);
                        }
                        out << checkboxBuf;        
                    }
                    
                    if((row == 0 && (m_AlignOption & eHyperLinkMasterSeqid)) ||
                       (row > 0 && (m_AlignOption & eHyperLinkSlaveSeqid))){
                        urlLink = x_GetUrl(m_AV->GetBioseqHandle(row).
                                           GetBioseqCore()->GetId(), gi, 
                                           row, taxid[row], 
                                           CBlastFormatUtil::
                                           GetLinkout(m_AV->GetBioseqHandle(row),
                                                      m_AV->GetSeqId(row)));     
                        out << urlLink;            
                    }        
                }
                
                bool has_mismatch = false;
                //change the alignment line to identity style
                if (row>0 && m_AlignOption & eShowIdentity){
                    for (int index = j; index < j + (int)actualLineLen && 
                             index < (int)sequence[row].size(); index ++){
                        if (sequence[row][index] == sequence[0][index] &&
                            isalpha((unsigned char) sequence[row][index])) {
                            sequence[row][index] = k_IdentityChar;           
                        } else if (!has_mismatch) {
                            has_mismatch = true;
                        }        
                    }
                }
                
                //highlight the seqid for pairwise-with-identity format
                if(row>0 && m_AlignOption&eHtml && !(m_AlignOption&eMergeAlign)
                   && m_AlignOption&eShowIdentity && has_mismatch && 
                   (m_AlignOption & eColorDifferentBases)){
                    out<< "<font color = \""<<k_ColorRed<<"\"><b>";         
                }
                out<<seqidArray[row]; 
               
                //highlight the seqid for pairwise-with-identity format
                if(row>0 && m_AlignOption&eHtml && !(m_AlignOption&eMergeAlign)
                   && m_AlignOption&eShowIdentity && has_mismatch){
                    out<< "</b></font>";         
                } 
               
                if(urlLink != NcbiEmptyString){
                    //mouse over seqid defline
                    if(m_AlignOption&eHtml &&
                       m_AlignOption&eShowInfoOnMouseOverSeqid) {
                        out << "<span>" <<
                            GetTitle(m_AV->GetBioseqHandle(row)) << "</span>";
                    }
                    out<<"</a>";   
                }
                
                //print out sequence line
                //adjust space between id and start
                CBlastFormatUtil::AddSpace(out, 
                                           maxIdLen-seqidArray[row].size()+
                                           k_IdStartMargin);
                out << start;
                startLen=NStr::IntToString(start).size();
                CBlastFormatUtil::AddSpace(out, maxStartLen-startLen+
                                           k_StartSequenceMargin);
                x_OutputSeq(sequence[row], m_AV->GetSeqId(row), j, 
                            (int)actualLineLen, frame[row], row,
                            (row > 0 && colorMismatch)?true:false,  
                            alnLocList, out);
                CBlastFormatUtil::AddSpace(out, k_SeqStopMargin);
                out << end;
                out<<endl;
                if(m_AlignOption & eMasterAnchored){//inserts for anchored view
                    bool insertAlready = false;
                    for(list<string>::iterator iter = inserts.begin(); 
                        iter != inserts.end(); iter ++){   
                        if(!insertAlready){
                            if((m_AlignOption&eHtml)
                               &&(m_AlignOption&eMergeAlign) 
                               && (m_AlignOption&eSequenceRetrieval 
                                   && m_CanRetrieveSeq)){
                                char checkboxBuf[200];
                                sprintf(checkboxBuf, 
                                        k_UncheckabeCheckbox.c_str(),
                                        m_QueryNumber);
                                out << checkboxBuf;
                            }
                            CBlastFormatUtil::AddSpace(out, 
                                                       maxIdLen
                                                       +k_IdStartMargin
                                                       +maxStartLen
                                                       +k_StartSequenceMargin);
                            out << insertPosString<<endl;
                        }
                        if((m_AlignOption&eHtml)
                           &&(m_AlignOption&eMergeAlign) 
                           && (m_AlignOption&eSequenceRetrieval && m_CanRetrieveSeq)){
                            char checkboxBuf[200];
                            sprintf(checkboxBuf, k_UncheckabeCheckbox.c_str(),
                                    m_QueryNumber);
                            out << checkboxBuf;
                        }
                        CBlastFormatUtil::AddSpace(out, maxIdLen
                                                   +k_IdStartMargin
                                                   +maxStartLen
                                                   +k_StartSequenceMargin);
                        out<<*iter<<endl;
                        insertAlready = true;
                    }
                } 
                //display subject sequence feature.
                if(row > 0){ 
                    x_PrintFeatures(bioseqFeature[row], row, curRange,
                                    j,(int)actualLineLen, maxIdLen,
                                    maxStartLen, max_feature_num, 
                                    master_feat_str, out);
                }
                //display middle line
                if (row == 0 && ((m_AlignOption & eShowMiddleLine)) 
                    && !(m_AlignOption&eMergeAlign)) {
                    CSeq_id no_id;
                    CBlastFormatUtil::
                        AddSpace(out, maxIdLen + k_IdStartMargin
                                 + maxStartLen + k_StartSequenceMargin);
                    x_OutputSeq(middleLine, no_id, j, (int)actualLineLen, 0, row, 
                                false,  alnLocList, out);
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
   
    for(int i = 0; i < rowNum; i ++){ //free allocation
        ITERATE(list<SAlnFeatureInfo*>, iter, bioseqFeature[i]){
            delete (*iter)->feature;
            delete (*iter);
        }
    } 
    ITERATE(list<SAlnSeqlocInfo*>, iter, alnLocList){
        delete (*iter);
    }
    delete [] bioseqFeature;
    delete [] seqidArray;
    delete [] rowRng;
    delete [] seqStarts;
    delete [] seqStops;
    delete [] frame;
    delete [] insertStart;
    delete [] insertAlnStart;
    delete [] insertLength;
    delete [] taxid;
}

CRef<CAlnVec> CDisplaySeqalign::x_GetAlnVecForSeqalign(const CSeq_align& align)
{
    
    //make alnvector
    CRef<CAlnVec> avRef;
    CConstRef<CSeq_align> finalAln;
    if (align.GetSegs().Which() == CSeq_align::C_Segs::e_Std) {
        CRef<CSeq_align> densegAln = align.CreateDensegFromStdseg();
        if (m_AlignOption & eTranslateNucToNucAlignment) { 
            finalAln = densegAln->CreateTranslatedDensegFromNADenseg();
        } else {
            finalAln = densegAln;
        }            
    } else if(align.GetSegs().Which() == 
              CSeq_align::C_Segs::e_Denseg){
        if (m_AlignOption & eTranslateNucToNucAlignment) { 
            finalAln = align.CreateTranslatedDensegFromNADenseg();
        } else {
            finalAln = &align;
        }
    } else if(align.GetSegs().Which() == 
              CSeq_align::C_Segs::e_Dendiag){
        CRef<CSeq_align> densegAln = 
            CBlastFormatUtil::CreateDensegFromDendiag(align);
        if (m_AlignOption & eTranslateNucToNucAlignment) { 
            finalAln = densegAln->CreateTranslatedDensegFromNADenseg();
        } else {
            finalAln = densegAln;
        }
    } else {
        NCBI_THROW(CException, eUnknown, 
                   "Seq-align should be Denseg, Stdseg or Dendiag!");
    }
    CRef<CDense_seg> finalDenseg(new CDense_seg);
    const CTypeConstIterator<CDense_seg> ds = ConstBegin(*finalAln);
    if((ds->IsSetStrands() 
        && ds->GetStrands().front()==eNa_strand_minus) 
       && !(ds->IsSetWidths() && ds->GetWidths()[0] == 3)){
        //show plus strand if master is minus for non-translated case
        finalDenseg->Assign(*ds);
        finalDenseg->Reverse();
        avRef = new CAlnVec(*finalDenseg, m_Scope);   
    } else {
        avRef = new CAlnVec(*ds, m_Scope);
    }    
    
    return avRef;
}

void CDisplaySeqalign::DisplaySeqalign(CNcbiOstream& out)
{   
    CSeq_align_set actual_aln_list;
    CBlastFormatUtil::ExtractSeqalignSetFromDiscSegs(actual_aln_list, 
                                                     *m_SeqalignSetRef);
    if (actual_aln_list.Get().empty()){
        return;
    }
    //scope for feature fetching
    if(!(m_AlignOption & eMasterAnchored) 
       && (m_AlignOption & eShowCdsFeature || m_AlignOption 
           & eShowGeneFeature)){
        m_FeatObj = CObjectManager::GetInstance();
        CGBDataLoader::RegisterInObjectManager(*m_FeatObj);
        m_featScope = new CScope(*m_FeatObj);  //for seq feature fetch
        string name = CGBDataLoader::GetLoaderNameFromArgs();
        m_featScope->AddDataLoader(name);
    }
    //for whether to add get sequence feature
    m_CanRetrieveSeq = x_GetDbType(actual_aln_list) == eDbTypeNotSet ? false : true;
    if(m_AlignOption & eHtml || m_AlignOption & eDynamicFeature){
        //set config file
        m_ConfigFile = new CNcbiIfstream(".ncbirc");
        m_Reg = new CNcbiRegistry(*m_ConfigFile);
        string feat_file = m_Reg->Get("FEATURE_INFO", "FEATURE_FILE");
        string feat_file_index = m_Reg->Get("FEATURE_INFO",
                                            "FEATURE_FILE_INDEX");
        if(feat_file != NcbiEmptyString && feat_file_index != NcbiEmptyString){
            m_DynamicFeature = new CGetFeature(feat_file, feat_file_index);
        }
    }
    if(m_AlignOption & eHtml){  
        out<<"<script src=\"blastResult.js\"></script>";
    }
    //get sequence 
    if(m_AlignOption&eSequenceRetrieval && m_AlignOption&eHtml && m_CanRetrieveSeq){ 
        out<<s_GetSeqForm((char*)"submitterTop", m_IsDbNa, m_QueryNumber, 
                          x_GetDbType(actual_aln_list),m_DbName, m_Rid.c_str(),
                          s_GetQueryIDFromSeqAlign(actual_aln_list).c_str(),m_AlignOption & eDisplayTreeView);                          
        out<<"<form name=\"getSeqAlignment"<<m_QueryNumber<<"\">\n";
    }
    //begin to display
    int num_align = 0;
    m_cur_align = 0;
    string toolUrl = NcbiEmptyString;
    if(m_AlignOption & eHtml){
        toolUrl = m_Reg->Get(m_BlastType, "TOOL_URL");
    }
    auto_ptr<CObjectOStream> out2(CObjectOStream::Open(eSerial_AsnText, out));
    //*out2 << *m_SeqalignSetRef;
    if(!(m_AlignOption&eMergeAlign)){
        /*pairwise alignment. Note we can't just show each alnment as we go
          because we will need seg information form all hsp's with the same id
          for genome url link.  As a result we show hsp's with the same id 
          as a group*/
     
        //get segs first and get hsp number
        if (toolUrl.find("dumpgnl.cgi") != string::npos 
            || (m_AlignOption & eLinkout)
            || (m_AlignOption & eHtml && m_AlignOption & eShowBlastInfo)) {
            /*need to construct segs for dumpgnl and
              get sub-sequence for long sequences*/
            for (CSeq_align_set::Tdata::const_iterator 
                     iter =  actual_aln_list.Get().begin(); 
                 iter != actual_aln_list.Get().end() 
                     && num_align<m_NumAlignToShow; iter++, num_align++) {

                //make alnvector
                CRef<CAlnVec> avRef = x_GetAlnVecForSeqalign(**iter);
                string idString = avRef->GetSeqId(1).GetSeqIdString();
                if (toolUrl.find("dumpgnl.cgi") != string::npos 
                    || (m_AlignOption & eLinkout)) {
                    if(m_Segs.count(idString) > 0){ 
                        //already has seg, concatenate
                        /*Note that currently it's not necessary to 
                          use map to store this information.  
                          But I already implemented this way for 
                          previous version.  Will keep this way as
                          it's more flexible if we change something*/
                        
                        m_Segs[idString] += "," 
                            + NStr::IntToString(avRef->GetSeqStart(1))
                            + "-" + 
                            NStr::IntToString(avRef->GetSeqStop(1));
                    } else {//new segs
                        m_Segs.
                            insert(map<string, string>::
                                   value_type(idString, 
                                              NStr::
                                              IntToString(avRef->GetSeqStart(1))
                                              + "-" + 
                                              NStr::IntToString(avRef->GetSeqStop(1))));
                    }
                } 
                if (m_AlignOption & eHtml && m_AlignOption & eShowBlastInfo) {
                    if(m_HspNumber.count(idString) > 0){
                        m_HspNumber[idString] ++;
                    } else {//new subject
                        m_HspNumber.insert(map<string, int>::value_type(idString, 1));
                    }
                }
            }	    
            
        } 
  
        CConstRef<CSeq_id> previousId, subid;
        for (CSeq_align_set::Tdata::const_iterator 
                 iter =  actual_aln_list.Get().begin(); 
             iter != actual_aln_list.Get().end() 
                 && num_align<m_NumAlignToShow; iter++, num_align++) {

            //make alnvector
            CRef<CAlnVec> avRef = x_GetAlnVecForSeqalign(**iter);
            
            if(!(avRef.Empty())){
                //Note: do not switch the set order per calnvec specs.
                avRef->SetGenCode(m_SlaveGeneticCode);
                avRef->SetGenCode(m_MasterGeneticCode, 0);
                try{
                    const CBioseq_Handle& handle = avRef->GetBioseqHandle(1);
                    if(handle){
                      
                        //save the current alnment regardless
                        SAlnInfo* alnvecInfo = new SAlnInfo;
                        int num_ident;
                        CBlastFormatUtil::GetAlnScores(**iter, 
                                                       alnvecInfo->score, 
                                                       alnvecInfo->bits, 
                                                       alnvecInfo->evalue, 
                                                       alnvecInfo->sum_n, 
                                                       num_ident,
                                                       alnvecInfo->use_this_gi,
                                                       alnvecInfo->comp_adj_method);
                        alnvecInfo->alnvec = avRef;
                       
                        subid=&(avRef->GetSeqId(1));
                        if(!previousId.Empty() && 
                           !subid->Match(*previousId)){
                            m_Scope.RemoveFromHistory(m_Scope.
                                                      GetBioseqHandle(*previousId));
                                                      //release memory 
                        }
                        x_DisplayAlnvecInfo(out, alnvecInfo,
                                            previousId.Empty() || 
                                            !subid->Match(*previousId));
                       
                        delete(alnvecInfo);
                        
                        previousId = subid;
                    }                
                } catch (const CException&){
                    out << "Sequence with id "
                        << (avRef->GetSeqId(1)).GetSeqIdString().c_str() 
                        <<" no longer exists in database...alignment skipped\n";
                    continue;
                }
            }
        } 

    } else if(m_AlignOption&eMergeAlign){ //multiple alignment
        CRef<CAlnMix>* mix = new CRef<CAlnMix>[k_NumFrame]; 
        //each for one frame for translated alignment
        for(int i = 0; i < k_NumFrame; i++){
            mix[i] = new CAlnMix(m_Scope);
        }        
        num_align = 0;
        vector<CRef<CSeq_align_set> > alnVector(k_NumFrame);
        for(int i = 0; i <  k_NumFrame; i ++){
            alnVector[i] = new CSeq_align_set;
        }
        for (CSeq_align_set::Tdata::const_iterator 
                 alnIter = actual_aln_list.Get().begin(); 
             alnIter != actual_aln_list.Get().end() 
                 && num_align<m_NumAlignToShow; alnIter ++, num_align++) {

            const CBioseq_Handle& subj_handle = 
                m_Scope.GetBioseqHandle((*alnIter)->GetSeq_id(1));
            if(subj_handle){
                //need to convert to denseg for stdseg
                if((*alnIter)->GetSegs().Which() == CSeq_align::C_Segs::e_Std) {
                    CTypeConstIterator<CStd_seg> ss = ConstBegin(**alnIter); 
                    CRef<CSeq_align> convertedDs = 
                        (*alnIter)->CreateDensegFromStdseg();
                    if((convertedDs->GetSegs().GetDenseg().IsSetWidths() 
                        && convertedDs->GetSegs().GetDenseg().GetWidths()[0] == 3)
                       || m_AlignOption & eTranslateNucToNucAlignment){
                        //only do this for translated master
                        int frame = s_GetStdsegMasterFrame(*ss, m_Scope);
                        switch(frame){
                        case 1:
                            alnVector[0]->Set().push_back(convertedDs);
                            break;
                        case 2:
                            alnVector[1]->Set().push_back(convertedDs);
                            break;
                        case 3:
                            alnVector[2]->Set().push_back(convertedDs);
                            break;
                        case -1:
                            alnVector[3]->Set().push_back(convertedDs);
                            break;
                        case -2:
                            alnVector[4]->Set().push_back(convertedDs);
                            break;
                        case -3:
                            alnVector[5]->Set().push_back(convertedDs);
                            break;
                        default:
                            break;
                        }
                    }
                    else {
                        alnVector[0]->Set().push_back(convertedDs);
                    }
                } else if((*alnIter)->GetSegs().Which() == CSeq_align::C_Segs::
                          e_Denseg){
                    alnVector[0]->Set().push_back(*alnIter);
                } else if((*alnIter)->GetSegs().Which() == CSeq_align::C_Segs::
                          e_Dendiag){
                    alnVector[0]->Set().\
                        push_back(CBlastFormatUtil::CreateDensegFromDendiag(**alnIter));
                } else {
                    NCBI_THROW(CException, eUnknown, 
                               "Input Seq-align should be Denseg, Stdseg or Dendiag!");
                }
            }
        }
        for(int i = 0; i < (int)alnVector.size(); i ++){
            bool hasAln = false;
            for(CTypeConstIterator<CSeq_align> 
                    alnRef = ConstBegin(*alnVector[i]); alnRef; ++alnRef){
                CTypeConstIterator<CDense_seg> ds = ConstBegin(*alnRef);
                //*out2 << *ds;      
                try{
                    if (m_AlignOption & eTranslateNucToNucAlignment) {	 
                        mix[i]->Add(*ds, CAlnMix::fForceTranslation);
                    } else {
                        mix[i]->Add(*ds);
                    }
                } catch (const CException& e){
                    out << "Warning: " << e.what();
                    continue;
                }
                hasAln = true;
            }
            if(hasAln){
                //    *out2<<*alnVector[i];
                mix[i]->Merge(CAlnMix::fMinGap 
                              | CAlnMix::fQuerySeqMergeOnly 
                              | CAlnMix::fFillUnalignedRegions);  
                //	*out2<<mix[i]->GetDenseg();
            }
        }
        
        int numDistinctFrames = 0;
        for(int i = 0; i < (int)alnVector.size(); i ++){
            if(!alnVector[i]->Get().empty()){
                numDistinctFrames ++;
            }
        }
        out<<endl;
        for(int i = 0; i < k_NumFrame; i ++){
            try{
                CRef<CAlnVec> avRef (new CAlnVec (mix[i]->GetDenseg(), 
                                                  m_Scope));
                avRef->SetGenCode(m_SlaveGeneticCode);
                avRef->SetGenCode(m_MasterGeneticCode, 0);
                m_AV = avRef;
                
                if(numDistinctFrames > 1){
                    out << "For reading frame " << k_FrameConversion[i] 
                        << " of query sequence:" << endl << endl;
                }
                x_DisplayAlnvec(out);
            } catch (CException e){
                continue;
            }
        } 
        delete [] mix;
    }
    if(m_AlignOption&eSequenceRetrieval && m_AlignOption&eHtml && m_CanRetrieveSeq){
        out<<"</form>\n";        
        out<<s_GetSeqForm((char*)"submitterBottom", m_IsDbNa, m_QueryNumber, 
                          x_GetDbType(actual_aln_list),m_DbName, m_Rid.c_str(),
                          s_GetQueryIDFromSeqAlign(actual_aln_list).c_str(),m_AlignOption & eDisplayTreeView);                         
    }
}


void CDisplaySeqalign::x_FillIdentityInfo(const string& sequence_standard,
                                          const string& sequence , 
                                          int& match, int& positive, 
                                          string& middle_line) 
{
    match = 0;
    positive = 0;
    int min_length=min<int>((int)sequence_standard.size(), (int)sequence.size());
    if(m_AlignOption & eShowMiddleLine){
        middle_line = sequence;
    }
    for(int i=0; i<min_length; i++){
        if(sequence_standard[i]==sequence[i]){
            if(m_AlignOption & eShowMiddleLine){
                if(m_MidLineStyle == eBar ) {
                    middle_line[i] = '|';
                } else if (m_MidLineStyle == eChar){
                    middle_line[i] = sequence[i];
                }
            }
            match ++;
        } else {
            if ((m_AlignType&eProt) 
                && m_Matrix[sequence_standard[i]][sequence[i]] > 0){  
                positive ++;
                if(m_AlignOption & eShowMiddleLine){
                    if (m_MidLineStyle == eChar){
                        middle_line[i] = '+';
                    }
                }
            } else {
                if (m_AlignOption & eShowMiddleLine){
                    middle_line[i] = ' ';
                }
            }    
        }
    }  
}


void 
CDisplaySeqalign::x_PrintDefLine(const CBioseq_Handle& bsp_handle,
                                 list<int>& use_this_gi, string& id_label,
                                 CNcbiOstream& out) const
{
    if(bsp_handle){
        const CRef<CSeq_id> wid =
            FindBestChoice(bsp_handle.GetBioseqCore()->GetId(), 
                           CSeq_id::WorstRank);
    
        const CRef<CBlast_def_line_set> bdlRef 
            =  CBlastFormatUtil::GetBlastDefline(bsp_handle);
        const list< CRef< CBlast_def_line > >& bdl = bdlRef->Get();
        bool isFirst = true;
        int firstGi = 0;

        m_cur_align++;
    
        if(bdl.empty()){ //no blast defline struct, should be no such case now
            //actually not so fast...as we now fetch from entrez even when it's not in blast db
            //there is no blast defline in such case.
            int gi = FindGi(bsp_handle.GetBioseqCore()->GetId());
            string urlLink;
            out << ">";
            if ((m_AlignOption&eSequenceRetrieval)
                && (m_AlignOption&eHtml) && m_CanRetrieveSeq && isFirst) {
                char buf[512];
                sprintf(buf, k_Checkbox.c_str(), gi > 0 ?
                        NStr::IntToString(gi).c_str() : wid->GetSeqIdString().c_str(),
                        m_QueryNumber);
                out << buf;
            }
                
            if(m_AlignOption&eHtml){
                
                urlLink = x_GetUrl(bsp_handle.GetBioseqCore()->GetId(), gi, 1, 0, 0);
                out<<urlLink;
            }
                
            if(m_AlignOption&eShowGi && gi > 0){
                out<<"gi|"<<gi<<"|";
                    }     
            if(!(wid->AsFastaString().find("gnl|BL_ORD_ID") 
                 != string::npos)){
                wid->WriteAsFasta(out);
            }
            if(m_AlignOption&eHtml){
                if(urlLink != NcbiEmptyString){
                    out<<"</a>";
                }
                if(gi != 0){
                    out<<"<a name="<<gi<<"></a>";
                    id_label = NStr::IntToString(gi);
                } else {
                    out<<"<a name="<<wid->GetSeqIdString()<<"></a>";
                    id_label = wid->GetSeqIdString();
                }
            }
            out <<" ";
            s_WrapOutputLine(out, (m_AlignOption&eHtml) ? 
                             CHTMLHelper::HTMLEncode(GetTitle(bsp_handle)) :
                             GetTitle(bsp_handle));     
                
            out<<endl;
            
        } else {
            //print each defline 
            for(list< CRef< CBlast_def_line > >::const_iterator 
                    iter = bdl.begin(); iter != bdl.end(); iter++){
                string urlLink;
                int gi =  s_GetGiForSeqIdList((*iter)->GetSeqid());
                int gi_in_use_this_gi = 0;
                int taxid = 0;
                ITERATE(list<int>, iter_gi, use_this_gi){
                    if(gi == *iter_gi){
                        gi_in_use_this_gi = *iter_gi;
                        break;
                    }
                }
                if(use_this_gi.empty() || gi_in_use_this_gi > 0) {
                
                    if(isFirst){
                        out << ">";
                        
                    } else{
                        out << " ";
                    }
                    const CRef<CSeq_id> wid2
                        = FindBestChoice((*iter)->GetSeqid(),
                                         CSeq_id::WorstRank);
                
                    if(isFirst){
                        firstGi = gi;
                    }
                    if ((m_AlignOption&eSequenceRetrieval)
                        && (m_AlignOption&eHtml) && m_CanRetrieveSeq && isFirst) {
                        char buf[512];
                        sprintf(buf, k_Checkbox.c_str(), gi > 0 ?
                                NStr::IntToString(gi).c_str() : wid2->GetSeqIdString().c_str(),
                                m_QueryNumber);
                                out << buf;
                    }
                
                    if(m_AlignOption&eHtml){
                        string type_temp = m_BlastType;
                        type_temp = NStr::TruncateSpaces(NStr::ToLower(type_temp));
                        if((type_temp == "mapview" || 
                            type_temp == "mapview_prev" || 
                            type_temp == "gsfasta") && 
                           (*iter)->IsSetTaxid() && 
                           (*iter)->CanGetTaxid()){
                            taxid = (*iter)->GetTaxid();
                        }
                        urlLink = x_GetUrl((*iter)->GetSeqid(), 
                                           gi_in_use_this_gi, 1, 
                                           taxid, CBlastFormatUtil::GetLinkout(**iter));    
                        out<<urlLink;
                    }
                
                    if(m_AlignOption&eShowGi && gi > 0){
                        out<<"gi|"<<gi<<"|";
                    }     
                    if(!(wid2->AsFastaString().find("gnl|BL_ORD_ID") 
                         != string::npos)){
                        wid2->WriteAsFasta(out);
                    }
                    if(m_AlignOption&eHtml){
                        if(urlLink != NcbiEmptyString){
                            out<<"</a>";
                        }
                        if(gi != 0){
                            out<<"<a name="<<gi<<"></a>";
                            id_label = NStr::IntToString(gi);
                        } else {
                            out<<"<a name="<<wid2->GetSeqIdString()<<"></a>";
                            id_label = wid2->GetSeqIdString();
                        }
                        if(m_AlignOption&eLinkout){
                            
                            int linkout = CBlastFormatUtil::
                                GetLinkout((**iter));
                            string user_url = m_Reg->Get(m_BlastType, 
                                                         "TOOL_URL");
                            list<string> linkout_url =  CBlastFormatUtil::
                                GetLinkoutUrl(linkout, (*iter)->GetSeqid(),
                                              m_Rid, m_DbName, m_QueryNumber,
                                              taxid, m_CddRid, m_EntrezTerm,
                                              bsp_handle.
                                              GetBioseqCore()->IsNa(), 
                                              user_url, m_IsDbNa, firstGi,
                                              false, true, m_cur_align);
                            out <<" ";
                            ITERATE(list<string>, iter_linkout, linkout_url){
                                out << *iter_linkout;
                            }
                           
                            if((int)bsp_handle.GetBioseqLength() 
                               > k_GetSubseqThreshhold){
                                string dumpGnlUrl
                                    = x_GetDumpgnlLink((*iter)->GetSeqid(), 1, 
                                                       k_DumpGnlUrl, 0);
                                out<<dumpGnlUrl
                                   <<"<img border=0 height=16 width=16\
                                   src=\"images/D.gif\" alt=\"Download subject sequence spanning the \
                                   HSP\"></a>";
                            }
                        }
                    }
                
                    out <<" ";
                    if((*iter)->IsSetTitle()){
                        s_WrapOutputLine(out, (m_AlignOption&eHtml) ? 
                                         CHTMLHelper::
                                         HTMLEncode((*iter)->GetTitle()) :
                                         (*iter)->GetTitle());     
                    }
                    out<<endl;
                    isFirst = false;
                }
            }
        }
    }
}


void CDisplaySeqalign::x_OutputSeq(string& sequence, const CSeq_id& id, 
                                   int start, int len, int frame, int row,
                                   bool color_mismatch,
                                   list<SAlnSeqlocInfo*> loc_list, 
                                   CNcbiOstream& out) const 
{
    _ASSERT((int)sequence.size() > start);
    list<CRange<int> > actualSeqloc;
    string actualSeq = sequence.substr(start, len);
    
    if(id.Which() != CSeq_id::e_not_set){ 
        /*only do this for sequence but not for others like middle line,
          features*/
        //go through seqloc containing mask info.  Only for master sequence
        if(row == 0){
            for (list<SAlnSeqlocInfo*>::const_iterator iter = loc_list.begin();  
                 iter != loc_list.end(); iter++){
                int from=(*iter)->aln_range.GetFrom();
                int to=(*iter)->aln_range.GetTo();
                int locFrame = (*iter)->seqloc->GetFrame();
                if(id.Match((*iter)->seqloc->GetInterval().GetId()) 
                   && locFrame == frame){
                    bool isFirstChar = true;
                    CRange<int> eachSeqloc(0, 0);
                    //go through each residule and mask it
                    for (int i=max<int>(from, start); 
                         i<=min<int>(to, start+len); i++){
                        //store seqloc start for font tag below
                        if ((m_AlignOption & eHtml) && isFirstChar){         
                            isFirstChar = false;
                            eachSeqloc.Set(i, eachSeqloc.GetTo());
                        }
                        if (m_SeqLocChar==eX){
                            if(isalpha((unsigned char) actualSeq[i-start])){
                                actualSeq[i-start]='X';
                            }
                        } else if (m_SeqLocChar==eN){
                            actualSeq[i-start]='n';
                        } else if (m_SeqLocChar==eLowerCase){
                            actualSeq[i-start]=tolower((unsigned char) actualSeq[i-start]);
                        }
                        //store seqloc start for font tag below
                        if ((m_AlignOption & eHtml) 
                            && i == min<int>(to, start+len)){ 
                            eachSeqloc.Set(eachSeqloc.GetFrom(), i);
                        }
                    }
                    if(!(eachSeqloc.GetFrom()==0&&eachSeqloc.GetTo()==0)){
                        actualSeqloc.push_back(eachSeqloc);
                    }
                }
            }
        }
    }
    
    if(actualSeqloc.empty()){//no need to add font tag
        if((m_AlignOption & eColorDifferentBases) && (m_AlignOption & eHtml)
           && color_mismatch && (m_AlignOption & eShowIdentity)){
            //color the mismatches. Only for rows without mask. 
            //Otherwise it may confilicts with mask font tag.
            s_ColorDifferentBases(actualSeq, k_IdentityChar, out);
        } else {
            out<<actualSeq;
        }
    } else {//now deal with font tag for mask for html display    
        bool endTag = false;
        bool numFrontTag = 0;
        for (int i = 0; i < (int)actualSeq.size(); i ++){
            for (list<CRange<int> >::iterator iter=actualSeqloc.begin(); 
                 iter!=actualSeqloc.end(); iter++){
                int from = (*iter).GetFrom() - start;
                int to = (*iter).GetTo() - start;
                //start tag
                if(from == i){
                    out<<"<font color=\""+color[m_SeqLocColor]+"\">";
                    numFrontTag = 1;
                }
                //need to close tag at the end of mask or end of sequence
                if(to == i || i == (int)actualSeq.size() - 1 ){
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


int CDisplaySeqalign::x_GetNumGaps() 
{
    int gap = 0;
    for (int row=0; row<m_AV->GetNumRows(); row++) {
        CRef<CAlnMap::CAlnChunkVec> chunk_vec 
            = m_AV->GetAlnChunks(row, m_AV->GetSeqAlnRange(0));
        for (int i=0; i<chunk_vec->size(); i++) {
            CConstRef<CAlnMap::CAlnChunk> chunk = (*chunk_vec)[i];
            if (chunk->IsGap()) {
                gap += (chunk->GetAlnRange().GetTo() 
                        - chunk->GetAlnRange().GetFrom() + 1);
            }
        }
    }
    return gap;
}


void CDisplaySeqalign::x_GetFeatureInfo(list<SAlnFeatureInfo*>& feature,
                                        CScope& scope, 
                                        CSeqFeatData::E_Choice choice,
                                        int row, string& sequence,
                                        list<list<CRange<TSeqPos> > >& feat_range_list,
                                        list<ENa_strand>& feat_seq_strand,
                                        bool fill_feat_range ) const 
{
    //Only fetch features for seq that has a gi unless it's master seq
    const CSeq_id& id = m_AV->GetSeqId(row);
    
    int gi_temp = FindGi(m_AV->GetBioseqHandle(row).GetBioseqCore()->GetId());
    if(gi_temp > 0 || row == 0){
        const CBioseq_Handle& handle = scope.GetBioseqHandle(id);
        if(handle){
            TSeqPos seq_start = m_AV->GetSeqPosFromAlnPos(row, 0);
            TSeqPos seq_stop = m_AV->GetSeqPosFromAlnPos(row, m_AV->GetAlnStop());
            CRef<CSeq_loc> loc_ref =
                handle.
                GetRangeSeq_loc(min(seq_start, seq_stop),
                                max(seq_start, seq_stop));
            for (CFeat_CI feat(scope, *loc_ref, choice); feat; ++feat) {
                const CSeq_loc& loc = feat->GetLocation();
                bool has_id = false;
                list<CSeq_loc_CI::TRange> isolated_range;
                ENa_strand feat_strand = eNa_strand_plus, prev_strand = eNa_strand_plus;
                bool first_loc = true, mixed_strand = false, mix_loc = false;
                CRange<TSeqPos> feat_seq_range;
                TSeqPos other_seqloc_length = 0;
                //isolate the seqloc corresponding to feature
                //as this is easier to manipulate and remove seqloc that is
                //not from the bioseq we are dealing with
                for(CSeq_loc_CI loc_it(loc); loc_it; ++loc_it){
                    const CSeq_id& id_it = loc_it.GetSeq_id();
                    if(IsSameBioseq(id_it, id, &scope)){
                        isolated_range.push_back(loc_it.GetRange());
                        if(first_loc){
                            feat_seq_range = loc_it.GetRange();
                        } else {
                            feat_seq_range += loc_it.GetRange();
                        }
                        has_id = true;
                        if(loc_it.IsSetStrand()){
                            feat_strand = loc_it.GetStrand();
                            if(feat_strand != eNa_strand_plus && 
                               feat_strand != eNa_strand_minus){
                                feat_strand = eNa_strand_plus;
                            }
                        } else {
                            feat_strand = eNa_strand_plus;
                        }
                   
                        if(!first_loc && prev_strand != feat_strand){
                            mixed_strand = true;
                        }
                        first_loc = false;
                        prev_strand = feat_strand;
                    } else {
                        //if seqloc has other seqids then need to remove other 
                        //seqid encoded amino acids in the front later
                        if (first_loc) {
                            other_seqloc_length += loc_it.GetRange().GetLength();
                            mix_loc = true;
                        }
                    }
                }
                //give up if mixed strand or no id
                if(!has_id || mixed_strand){
                    continue;
                }
               
                string featLable = NcbiEmptyString;
                string featId;
                char feat_char = ' ';
                string alternativeFeatStr = NcbiEmptyString;            
                TSeqPos feat_aln_from = 0;
                TSeqPos feat_aln_to = 0;
                TSeqPos actual_feat_seq_start = 0, actual_feat_seq_stop = 0;
                feature::GetLabel(feat->GetOriginalFeature(), &featLable, 
                                  feature::eBoth, &scope);
                featId = featLable.substr(0, k_FeatureIdLen); //default
                TSeqPos aln_stop = m_AV->GetAlnStop();  
                SAlnFeatureInfo* featInfo = NULL;
               
                //find the actual feature sequence start and stop 
                if(m_AV->IsPositiveStrand(row)){
                    actual_feat_seq_start = 
                        max(feat_seq_range.GetFrom(), seq_start);
                    actual_feat_seq_stop = 
                        min(feat_seq_range.GetTo(), seq_stop);
                    
                } else {
                    actual_feat_seq_start = 
                        min(feat_seq_range.GetTo(), seq_start);
                    actual_feat_seq_stop =
                        max(feat_seq_range.GetFrom(), seq_stop);
                }
                //the feature alignment positions
                feat_aln_from = 
                    m_AV->GetAlnPosFromSeqPos(row, actual_feat_seq_start);
                feat_aln_to = 
                    m_AV->GetAlnPosFromSeqPos(row, actual_feat_seq_stop);
                if(choice == CSeqFeatData::e_Gene){
                    featInfo = new SAlnFeatureInfo;                 
                    feat_char = '^';
                    
                } else if(choice == CSeqFeatData::e_Cdregion){
                     
                    string raw_cdr_product = 
                        s_GetCdsSequence(m_SlaveGeneticCode, feat, scope,
                                         isolated_range, handle, feat_strand,
                                         featId, other_seqloc_length%3 == 0 ?
                                         0 : 3 - other_seqloc_length%3,
                                         mix_loc);
                    if(raw_cdr_product == NcbiEmptyString){
                        continue;
                    }
                    featInfo = new SAlnFeatureInfo;
                          
                    //line represents the amino acid line starting covering 
                    //the whole alignment.  The idea is if there is no feature
                    //in some range, then fill it with space and this won't
                    //be shown 
                    
                    string line(aln_stop+1, ' '); 
                    //pre-fill all cds region with intron char
                    for (TSeqPos i = feat_aln_from; i <= feat_aln_to; i ++){
                        line[i] = k_IntronChar;
                    }
                    
                    //get total coding length
                    TSeqPos total_coding_len = 0;
                    ITERATE(list<CSeq_loc_CI::TRange>, iter, isolated_range){
                        total_coding_len += iter->GetLength(); 
                    }
                  
                    //fill concatenated exon (excluding intron)
                    //with product
                    //this is will be later used to
                    //fill the feature line
                    char gap_char = m_AV->GetGapChar(row);
                    string concat_exon = 
                        s_GetConcatenatedExon(feat, feat_strand, 
                                              isolated_range,
                                              total_coding_len,
                                              raw_cdr_product,
                                              other_seqloc_length%3 == 0 ?
                                              0 : 3 - other_seqloc_length%3);
                    
                   
                    //fill slave feature info to make putative feature for
                    //master sequence
                    if (fill_feat_range) {
                        list<CRange<TSeqPos> > master_feat_range;
                        ENa_strand master_strand = eNa_strand_plus;
                        s_MapSlaveFeatureToMaster(master_feat_range, master_strand,
                                                  feat, isolated_range,
                                                  feat_strand, m_AV, row,
                                                  other_seqloc_length%3 == 0 ?
                                                  0 : 
                                                  3 - other_seqloc_length%3);
                        if(!(master_feat_range.empty())) {
                            feat_range_list.push_back(master_feat_range); 
                            feat_seq_strand.push_back(master_strand);
                        } 
                    }
                       
                    
                    TSeqPos feat_aln_start_totalexon = 0;
                    TSeqPos prev_feat_aln_start_totalexon = 0;
                    TSeqPos prev_feat_seq_stop = 0;
                    TSeqPos intron_size = 0;
                    bool is_first = true;
                    bool  is_first_exon_start = true;
               
                    //here things get complicated a bit. The idea is fill the
                    //whole feature line in alignment coordinates with
                    //amino acid on the second base of a condon

                    //go through the feature seqloc and fill the feature line
                    
                    //Need to reverse the seqloc order for minus strand
                    if(feat_strand == eNa_strand_minus){
                        isolated_range.reverse(); 
                    }
                    
                    ITERATE(list<CSeq_loc_CI::TRange>, iter, isolated_range){
                        //intron refers to the distance between two exons
                        //i.e. each seqloc is an exon
                        //intron needs to be skipped
                        if(!is_first){
                            intron_size += iter->GetFrom() 
                                - prev_feat_seq_stop - 1;
                        }
                        CRange<TSeqPos> actual_feat_seq_range =
                            loc_ref->GetTotalRange().
                            IntersectionWith(*iter);          
                        if(!actual_feat_seq_range.Empty()){
                            //the sequence start position in aln coordinates
                            //that has a feature
                            TSeqPos feat_aln_start;
                            TSeqPos feat_aln_stop;
                            if(m_AV->IsPositiveStrand(row)){
                                feat_aln_start = 
                                    m_AV->
                                    GetAlnPosFromSeqPos
                                    (row, actual_feat_seq_range.GetFrom());
                                feat_aln_stop
                                    = m_AV->GetAlnPosFromSeqPos
                                    (row, actual_feat_seq_range.GetTo());
                            } else {
                                feat_aln_start = 
                                    m_AV->
                                    GetAlnPosFromSeqPos
                                    (row, actual_feat_seq_range.GetTo());
                                feat_aln_stop
                                    = m_AV->GetAlnPosFromSeqPos
                                    (row, actual_feat_seq_range.GetFrom());
                            }
                            //put actual amino acid on feature line
                            //in aln coord 
                            for (TSeqPos i = feat_aln_start;
                                 i <= feat_aln_stop;  i ++){  
                                    if(sequence[i] != gap_char){
                                        //the amino acid position in 
                                        //concatanated exon that corresponds
                                        //to the sequence position
                                        //note intron needs to be skipped
                                        //as it does not have cds feature
                                        TSeqPos product_adj_seq_pos
                                            = m_AV->GetSeqPosFromAlnPos(row, i) - 
                                            intron_size - feat_seq_range.GetFrom();
                                        if(product_adj_seq_pos < 
                                           concat_exon.size()){
                                            //fill the cds feature line with
                                            //actual amino acids
                                            line[i] = 
                                                concat_exon[product_adj_seq_pos];
                                            //get the exon start position
                                            //note minus strand needs to be
                                            //counted backward
                                            if(m_AV->IsPositiveStrand(row)){
                                                //don't count gap 
                                                if(is_first_exon_start &&
                                                   isalpha((unsigned char) line[i])){
                                                    if(feat_strand == eNa_strand_minus){ 
                                                        feat_aln_start_totalexon = 
                                                            concat_exon.size()
                                                            - product_adj_seq_pos + 1;
                                                        is_first_exon_start = false;
                                                        
                                                    } else {
                                                        feat_aln_start_totalexon = 
                                                            product_adj_seq_pos;
                                                        is_first_exon_start = false;
                                                    }
                                                }
                                                
                                            } else {
                                                if(feat_strand == eNa_strand_minus){ 
                                                    if(is_first_exon_start && 
                                                       isalpha((unsigned char) line[i])){
                                                        feat_aln_start_totalexon = 
                                                            concat_exon.size()
                                                            - product_adj_seq_pos + 1;
                                                        is_first_exon_start = false;
                                                        prev_feat_aln_start_totalexon =
                                                        feat_aln_start_totalexon;
                                                    }
                                                    if(!is_first_exon_start){
                                                        //need to get the
                                                        //smallest start as
                                                        //seqloc list is
                                                        //reversed
                                                        feat_aln_start_totalexon =
                                                            min(TSeqPos(concat_exon.size()
                                                                        - product_adj_seq_pos + 1), 
                                                                prev_feat_aln_start_totalexon);
                                                        prev_feat_aln_start_totalexon =
                                                        feat_aln_start_totalexon;  
                                                    }
                                                } else {
                                                    feat_aln_start_totalexon = 
                                                        max(prev_feat_aln_start_totalexon,
                                                            product_adj_seq_pos); 
                                                    
                                                    prev_feat_aln_start_totalexon =
                                                        feat_aln_start_totalexon;
                                                }
                                            }
                                        }
                                    } else { //adding gap
                                        line[i] = ' '; 
                                    }                         
                               
                            }                      
                        }
                        
                        prev_feat_seq_stop = iter->GetTo();  
                        is_first = false;
                    }                 
                    alternativeFeatStr = line;
                    s_FillCdsStartPosition(line, concat_exon, m_LineLen,
                                           feat_aln_start_totalexon,
                                           m_AV->IsPositiveStrand(row) ?
                                           eNa_strand_plus : eNa_strand_minus,
                                           feat_strand, featInfo->feature_start); 
                 
                }
                
                if(featInfo){
                    x_SetFeatureInfo(featInfo, *loc_ref,
                                     feat_aln_from, feat_aln_to, aln_stop, 
                                     feat_char, featId, alternativeFeatStr);  
                    feature.push_back(featInfo);
                }
            }
        }   
    }
}


void  CDisplaySeqalign::x_SetFeatureInfo(SAlnFeatureInfo* feat_info, 
                                         const CSeq_loc& seqloc, int aln_from, 
                                         int aln_to, int aln_stop, 
                                         char pattern_char, string pattern_id,
                                         string& alternative_feat_str) const
{
    FeatureInfo* feat = new FeatureInfo;
    feat->seqloc = &seqloc;
    feat->feature_char = pattern_char;
    feat->feature_id = pattern_id;
    
    if(alternative_feat_str != NcbiEmptyString){
        feat_info->feature_string = alternative_feat_str;
    } else {
        //fill feature string
        string line(aln_stop+1, ' ');
        for (int j = aln_from; j <= aln_to; j++){
            line[j] = feat->feature_char;
        }
        feat_info->feature_string = line;
    }
    
    feat_info->aln_range.Set(aln_from, aln_to); 
    feat_info->feature = feat;
}

///add a "|" to the current insert for insert on next rows and return the
///insert end position.
///@param seq: the seq string
///@param insert_aln_pos: the position of insert
///@param aln_start: alnment start position
///@return: the insert end position
///
static int x_AddBar(string& seq, int insert_alnpos, int aln_start){
    int end = (int)seq.size() -1 ;
    int barPos = insert_alnpos - aln_start + 1;
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


///Add new insert seq to the current insert seq and return the end position of
///the latest insert
///@param cur_insert: the current insert string
///@param new_insert: the new insert string
///@param insert_alnpos: insert position
///@param aln_start: alnment start
///@return: the updated insert end position
///
static int s_AdjustInsert(string& cur_insert, string& new_insert, 
                          int insert_alnpos, int aln_start)
{
    int insertEnd = 0;
    int curInsertSize = (int)cur_insert.size();
    int insertLeftSpace = insert_alnpos - aln_start - curInsertSize + 2;  
    //plus2 because insert is put after the position
    if(curInsertSize > 0){
        _ASSERT(insertLeftSpace >= 2);
    }
    int newInsertSize = (int)new_insert.size();  
    if(insertLeftSpace - newInsertSize >= 1){ 
        //can insert with the end position right below the bar
        string spacer(insertLeftSpace - newInsertSize, ' ');
        cur_insert += spacer + new_insert;
        
    } else { //Need to insert beyond the insert postion
        if(curInsertSize > 0){
            cur_insert += " " + new_insert;
        } else {  //can insert right at the firt position
            cur_insert += new_insert;
        }
    }
    insertEnd = aln_start + (int)cur_insert.size() -1 ; //-1 back to string position
    return insertEnd;
}


void CDisplaySeqalign::x_DoFills(int row, CAlnMap::TSignedRange& aln_range, 
                                 int  aln_start, 
                                 list<SInsertInformation*>& insert_list, 
                                 list<string>& inserts) const {
    if(!insert_list.empty()){
        string bar(aln_range.GetLength(), ' ');
        
        string seq;
        list<SInsertInformation*> leftOverInsertList;
        bool isFirstInsert = true;
        int curInsertAlnStart = 0;
        int prvsInsertAlnEnd = 0;
        
        //go through each insert and fills the seq if it can 
        //be filled on the same line.  If not, go to the next line
        for(list<SInsertInformation*>::iterator 
                iter = insert_list.begin(); iter != insert_list.end(); iter ++){
            curInsertAlnStart = (*iter)->aln_start;
            //always fill the first insert.  Also fill if there is enough space
            if(isFirstInsert || curInsertAlnStart - prvsInsertAlnEnd >= 1){
                bar[curInsertAlnStart-aln_start+1] = '|';  
                int seqStart = (*iter)->seq_start;
                int seqEnd = seqStart + (*iter)->insert_len - 1;
                string newInsert;
                newInsert = m_AV->GetSeqString(newInsert, row, seqStart,
                                               seqEnd);
                prvsInsertAlnEnd = s_AdjustInsert(seq, newInsert,
                                                  curInsertAlnStart, aln_start);
                isFirstInsert = false;
            } else { //if no space, save the chunk and go to next line 
                bar[curInsertAlnStart-aln_start+1] = '|'; 
                //indicate insert goes to the next line
                prvsInsertAlnEnd += x_AddBar(seq, curInsertAlnStart, aln_start); 
                //May need to add a bar after the current insert sequence 
                //to indicate insert goes to the next line.
                leftOverInsertList.push_back(*iter);    
            }
        }
        //save current insert.  Note that each insert has a bar and sequence
        //below it
        inserts.push_back(bar);
        inserts.push_back(seq);
        //here recursively fill the chunk that don't have enough space
        x_DoFills(row, aln_range, aln_start, leftOverInsertList, inserts);
    }
    
}


void CDisplaySeqalign::x_FillInserts(int row, CAlnMap::TSignedRange& aln_range,
                                     int aln_start, list<string>& inserts,
                                     string& insert_pos_string, 
                                     list<SInsertInformation*>& insert_list) const
{
    
    string line(aln_range.GetLength(), ' ');
    
    ITERATE(list<SInsertInformation*>, iter, insert_list){
        int from = (*iter)->aln_start;
        line[from - aln_start + 1] = '\\';
    }
    insert_pos_string = line; 
    //this is the line with "\" right after each insert position
    
    //here fills the insert sequence
    x_DoFills(row, aln_range, aln_start, insert_list, inserts);
}


void CDisplaySeqalign::x_GetInserts(list<SInsertInformation*>& insert_list,
                                    CAlnMap::TSeqPosList& insert_aln_start, 
                                    CAlnMap::TSeqPosList& insert_seq_start, 
                                    CAlnMap::TSeqPosList& insert_length, 
                                    int line_aln_stop)
{

    while(!insert_aln_start.empty() 
          && (int)insert_aln_start.front() < line_aln_stop){
        CDisplaySeqalign::SInsertInformation* insert
            = new CDisplaySeqalign::SInsertInformation;
        insert->aln_start = insert_aln_start.front() - 1; 
        //Need to minus one as we are inserting after this position
        insert->seq_start = insert_seq_start.front();
        insert->insert_len = insert_length.front();
        insert_list.push_back(insert);
        insert_aln_start.pop_front();
        insert_seq_start.pop_front();
        insert_length.pop_front();
    }
    
}


string CDisplaySeqalign::x_GetSegs(int row) const 
{
    string segs = NcbiEmptyString;
    if(m_AlignOption & eMergeAlign){ //only show this hsp
        segs = NStr::IntToString(m_AV->GetSeqStart(row))
            + "-" + NStr::IntToString(m_AV->GetSeqStop(row));
    } else { //for all segs
        string idString = m_AV->GetSeqId(1).GetSeqIdString();
        map<string, string>::const_iterator iter = m_Segs.find(idString);
        if ( iter != m_Segs.end() ){
            segs = iter->second;
        }
    }
    return segs;
}


string CDisplaySeqalign::x_GetDumpgnlLink(const list<CRef<CSeq_id> >& ids, 
                                          int row,
                                          const string& alternative_url,
                                          int taxid) const
{
    string link = NcbiEmptyString;  
    string toolUrl= m_Reg->Get(m_BlastType, "TOOL_URL");
    string segs = x_GetSegs(row);
    string url_with_parameters = NcbiEmptyString;

    if(alternative_url != NcbiEmptyString){ 
        toolUrl = alternative_url;
    }
    url_with_parameters = CBlastFormatUtil::BuildUserUrl(ids, taxid, toolUrl,
                                                         m_DbName,
                                                         m_IsDbNa, m_Rid, m_QueryNumber,
                                                         true);
    if (url_with_parameters != NcbiEmptyString) {
        
        link +=  (m_AlignOption & eShowInfoOnMouseOverSeqid) ? 
            ("<a " + kClassInfo + " " + "href=\"") : "<a href=\"";
        
        link += url_with_parameters; 
        
        if (toolUrl.find("dumpgnl.cgi") !=string::npos) {
            link += "&segs=" + segs;
        }
        link += "\">";
    }
    return link;
}


CDisplaySeqalign::DbType CDisplaySeqalign::x_GetDbType(const CSeq_align_set& actual_aln_list) 
{
    //determine if the database has gi by looking at the 1st hit.  
    //Could be wrong but simple for now
    DbType type = eDbTypeNotSet;
    CRef<CSeq_align> first_aln = actual_aln_list.Get().front();
    const CSeq_id& subject_id = first_aln->GetSeq_id(1);
    const CBioseq_Handle& handleTemp  = m_Scope.GetBioseqHandle(subject_id);
    if(handleTemp){
        int giTemp = FindGi(handleTemp.GetBioseqCore()->GetId());
        if (giTemp >0) { 
            type = eDbGi;
        } else if (subject_id.Which() == CSeq_id::e_General){
            const CDbtag& dtg = subject_id.GetGeneral();
            const string& dbName = dtg.GetDb();
            if(NStr::CompareNocase(dbName, "TI") == 0){
                type = eDbGeneral;
            }
        }   
    }
    return type;
}


CRef<CSeq_align_set> 
CDisplaySeqalign::PrepareBlastUngappedSeqalign(const CSeq_align_set& alnset) 
{
    CRef<CSeq_align_set> alnSetRef(new CSeq_align_set);

    ITERATE(CSeq_align_set::Tdata, iter, alnset.Get()){
        const CSeq_align::TSegs& seg = (*iter)->GetSegs();
        if(seg.Which() == CSeq_align::C_Segs::e_Std){
            if(seg.GetStd().size() > 1){ 
                //has more than one stdseg. Need to seperate as each 
                //is a distinct HSP
                ITERATE (CSeq_align::C_Segs::TStd, iterStdseg, seg.GetStd()){
                    CRef<CSeq_align> aln(new CSeq_align);
                    if((*iterStdseg)->IsSetScores()){
                        aln->SetScore() = (*iterStdseg)->GetScores();
                    }
                    aln->SetSegs().SetStd().push_back(*iterStdseg);
                    alnSetRef->Set().push_back(aln);
                }
                
            } else {
                alnSetRef->Set().push_back(*iter);
            }
        } else if(seg.Which() == CSeq_align::C_Segs::e_Dendiag){
            if(seg.GetDendiag().size() > 1){ 
                //has more than one dendiag. Need to seperate as each is
                //a distinct HSP
                ITERATE (CSeq_align::C_Segs::TDendiag, iterDendiag,
                         seg.GetDendiag()){
                    CRef<CSeq_align> aln(new CSeq_align);
                    if((*iterDendiag)->IsSetScores()){
                        aln->SetScore() = (*iterDendiag)->GetScores();
                    }
                    aln->SetSegs().SetDendiag().push_back(*iterDendiag);
                    alnSetRef->Set().push_back(aln);
                }
                
            } else {
                alnSetRef->Set().push_back(*iter);
            }
        } else { //Denseg, doing nothing.
            
            alnSetRef->Set().push_back(*iter);
        }
    }
    
    return alnSetRef;
}


bool CDisplaySeqalign::x_IsGeneInfoAvailable(SAlnInfo* aln_vec_info)
{
    const CBioseq_Handle& bsp_handle =
        aln_vec_info->alnvec->GetBioseqHandle(1);
    if (bsp_handle &&
        (m_AlignOption&eHtml) &&
        (m_AlignOption&eLinkout) &&
        (m_AlignOption&eShowGeneInfo))
    {
        CNcbiEnvironment env;
        if (env.Get(GENE_INFO_PATH_ENV_VARIABLE) == kEmptyStr)
        {
            return false;
        }

        const CRef<CBlast_def_line_set> bdlRef 
            =  CBlastFormatUtil::GetBlastDefline(bsp_handle);
        const list< CRef< CBlast_def_line > >& bdl = bdlRef->Get();

        for(list< CRef< CBlast_def_line > >::const_iterator 
                iter = bdl.begin(); iter != bdl.end(); iter++)
        {
            int linkout = CBlastFormatUtil::
                GetLinkout((**iter));
            if (linkout & eGene)
            {
                return true;
            }
        }
    }
    return false;
}


string CDisplaySeqalign::x_GetGeneLinkUrl(int gene_id)
{
    string strGeneLinkUrl = CBlastFormatUtil::GetURLFromRegistry("GENE_INFO");
    char* buf = new char[strGeneLinkUrl.size() + 1024];
    sprintf(buf, strGeneLinkUrl.c_str(), 
                 gene_id,
                 m_Rid.c_str(),
                 m_IsDbNa ? "nucl" : "prot",
                 m_cur_align);
    strGeneLinkUrl = string(buf);
    delete [] buf;
    return strGeneLinkUrl;
}


void CDisplaySeqalign::x_DisplayAlnvecInfo(CNcbiOstream& out, 
                                           SAlnInfo* aln_vec_info,
                                           bool show_defline) 
{
  
    m_AV = aln_vec_info->alnvec;
    const CBioseq_Handle& bsp_handle=m_AV->GetBioseqHandle(1); 
    if(show_defline && (m_AlignOption&eShowBlastInfo)) {
        string id_label;
        if(!(m_AlignOption & eShowNoDeflineInfo)){
            x_PrintDefLine(bsp_handle, aln_vec_info->use_this_gi, id_label, out);
            out<<"Length="<<bsp_handle.GetBioseqLength()<<endl;

            try
            {
                if (x_IsGeneInfoAvailable(aln_vec_info))
                {
                    if (m_GeneInfoReader.get() == 0)
                    {
                        m_GeneInfoReader.reset(
                            new CGeneInfoFileReader(false));
                    }

                    int giForGeneLookup =
                        FindGi(bsp_handle.GetBioseqCore()->GetId());

                    CGeneInfoFileReader::TGeneInfoList infoList;
                    m_GeneInfoReader->GetGeneInfoForGi(giForGeneLookup,
                                                        infoList);

                    CGeneInfoFileReader::TGeneInfoList::const_iterator
                        itInfo = infoList.begin();
                    if (itInfo != infoList.end())
                        out << endl;
                    for (; itInfo != infoList.end(); itInfo++)
                    {
                        CRef<CGeneInfo> info = *itInfo;
                        string strUrl = x_GetGeneLinkUrl(info->GetGeneId());
                        string strInfo;
                        info->ToString(strInfo, true, strUrl);
                        out << strInfo << endl;
                    }
                }
            }
            catch (CException& e)
            {
                out << "(Gene info extraction error: "
                    << e.GetMsg() << ")" << endl;
            }
            catch (...)
            {
                out << "(Gene info extraction error)" << endl;
            }
        }
        
        if(m_AlignOption&eHtml && m_AlignOption & eShowBlastInfo &&
           m_HspNumber[m_AV->GetSeqId(1).GetSeqIdString()] > 1 &&
           m_AlignOption & eShowSortControls){
            string query_buf; 
            map< string, string> parameters_to_change;
            parameters_to_change.insert(map<string, string>::
                                        value_type("HSP_SORT", ""));
            CBlastFormatUtil::BuildFormatQueryString(*m_Ctx, 
                                                     parameters_to_change,
                                                     query_buf);
            out << endl;
            CBlastFormatUtil::AddSpace(out, 57); 
            out << "Sort alignments for this subject sequence by:\n";
            CBlastFormatUtil::AddSpace(out, 59); 
            
            string hsp_sort_value = m_Ctx->GetRequestValue("HSP_SORT").GetValue();
            int hsp_sort = hsp_sort_value == NcbiEmptyString ? 
                0 : NStr::StringToInt(hsp_sort_value);
           
            if (hsp_sort != CBlastFormatUtil::eEvalue) {
                out << "<a href=\"Blast.cgi?CMD=Get&" << query_buf 
                    << "&HSP_SORT="
                    << CBlastFormatUtil::eEvalue
                    << "#" << id_label << "\">";
            }
            
            out << "E value";
            if (hsp_sort != CBlastFormatUtil::eEvalue) {
                out << "</a>"; 
            }
            
            CBlastFormatUtil::AddSpace(out, 2);

            if (hsp_sort != CBlastFormatUtil::eScore) {
                out << "<a href=\"Blast.cgi?CMD=Get&" << query_buf 
                    << "&HSP_SORT="
                    << CBlastFormatUtil::eScore
                    << "#" << id_label << "\">";
            }
            
            out << "Score";
            if (hsp_sort != CBlastFormatUtil::eScore) {
                out << "</a>"; 
            }
            
            CBlastFormatUtil::AddSpace(out, 2);

          

            if (hsp_sort != CBlastFormatUtil::eHspPercentIdentity) {
                out << "<a href=\"Blast.cgi?CMD=Get&" << query_buf 
                    << "&HSP_SORT="
                    << CBlastFormatUtil::eHspPercentIdentity
                    << "#" << id_label << "\">";
            }
            out  << "Percent identity"; 
            if (hsp_sort != CBlastFormatUtil::eHspPercentIdentity) {
                out << "</a>"; 
            }
            out << endl;
            CBlastFormatUtil::AddSpace(out, 59); 
            if (hsp_sort != CBlastFormatUtil::eQueryStart) {
                out << "<a href=\"Blast.cgi?CMD=Get&" << query_buf 
                    << "&HSP_SORT="
                    << CBlastFormatUtil::eQueryStart
                    << "#" << id_label << "\">";
            } 
            out << "Query start position";
            if (hsp_sort != CBlastFormatUtil::eQueryStart) {
                out << "</a>"; 
            }
            CBlastFormatUtil::AddSpace(out, 2);
           
            if (hsp_sort != CBlastFormatUtil::eSubjectStart) {
                out << "<a href=\"Blast.cgi?CMD=Get&" << query_buf 
                    << "&HSP_SORT="
                    << CBlastFormatUtil::eSubjectStart
                    << "#" << id_label << "\">";
            } 
            out << "Subject start position";
            if (hsp_sort != CBlastFormatUtil::eSubjectStart) {
                out << "</a>"; 
            }
            
            out << endl;
        }
        
        if((m_AlignOption&eHtml) && (m_AlignOption&eShowBlastInfo)
           && (m_AlignOption&eShowBl2seqLink)) {
            const CBioseq_Handle& query_handle=m_AV->GetBioseqHandle(0);
            const CBioseq_Handle& subject_handle=m_AV->GetBioseqHandle(1);
            CSeq_id_Handle query_seqid = GetId(query_handle, eGetId_Best);
            CSeq_id_Handle subject_seqid =
                GetId(subject_handle, eGetId_Best);
            int query_gi = FindGi(query_handle.GetBioseqCore()->GetId());   
            int subject_gi = FindGi(subject_handle.GetBioseqCore()->GetId());
            
            char buffer[512];
            
            sprintf(buffer, kBl2seqUrl.c_str(), m_Rid.c_str(), 
                    query_gi > 0 ? 
                    NStr::IntToString(query_gi).c_str():query_seqid.    \
                    GetSeqId()->AsFastaString().c_str(),
                    subject_gi > 0 ? 
                    NStr::IntToString(subject_gi).c_str():subject_seqid. \
                    GetSeqId()->AsFastaString().c_str()); 
            out << buffer << endl;
        }
        out << endl;
    }
    
    //output dynamic feature lines
    if(m_AlignOption&eShowBlastInfo && !(m_AlignOption&eMergeAlign) 
       && (m_AlignOption&eDynamicFeature) 
       && (int)m_AV->GetBioseqHandle(1).GetBioseqLength() 
       >= k_GetDynamicFeatureSeqLength){ 
        if(m_DynamicFeature){
            x_PrintDynamicFeatures(out);
        } 
    }
    if (m_AlignOption&eShowBlastInfo) {
        string evalue_buf, bit_score_buf, total_bit_buf;
        CBlastFormatUtil::GetScoreString(aln_vec_info->evalue, 
                                         aln_vec_info->bits, 0, evalue_buf, 
                                         bit_score_buf, total_bit_buf);
        //add id anchor for mapviewer link
        string type_temp = m_BlastType;
        type_temp = NStr::TruncateSpaces(NStr::ToLower(type_temp));
        if(m_AlignOption&eHtml && 
           (type_temp.find("genome") != string::npos ||
            type_temp == "mapview" || 
            type_temp == "mapview_prev" || 
            type_temp == "gsfasta")){
            string subj_id_str;
            char buffer[126];
            int master_start = m_AV->GetSeqStart(0) + 1;
            int master_stop = m_AV->GetSeqStop(0) + 1;
            int subject_start = m_AV->GetSeqStart(1) + 1;
            int subject_stop = m_AV->GetSeqStop(1) + 1;
            
            m_AV->GetSeqId(1).GetLabel(&subj_id_str, CSeq_id::eContent);
            
            sprintf(buffer, "<a name = %s_%d_%d_%d_%d_%d></a>",
                    subj_id_str.c_str(), aln_vec_info->score,
                    min(master_start, master_stop),
                    max(master_start, master_stop),
                    min(subject_start, subject_stop),
                    max(subject_start, subject_stop));
            
            out << buffer << endl; 
        }
        out<<" Score = "<<bit_score_buf<<" ";
        out<<"bits ("<<aln_vec_info->score<<"),"<<"  ";
        out<<"Expect";
        if (aln_vec_info->sum_n > 0) {
            out << "(" << aln_vec_info->sum_n << ")";
        }
        
        out << " = " << evalue_buf;
        if (aln_vec_info->comp_adj_method == 1)
            out << ", Method: Composition-based stats.";
        else if (aln_vec_info->comp_adj_method == 2)
            out << ", Method: Compositional matrix adjust.";
        out << endl;
    }
        
    x_DisplayAlnvec(out);
    out<<endl;
}


void CDisplaySeqalign::x_PrintDynamicFeatures(CNcbiOstream& out) 
{
    const CSeq_id& subject_seqid = m_AV->GetSeqId(1);
    const CRange<TSeqPos>& range = m_AV->GetSeqRange(1);
    CRange<TSeqPos> actual_range = range;
    if(range.GetFrom() > range.GetTo()){
        actual_range.Set(range.GetTo(), range.GetFrom());
    }
    string id_str;
    subject_seqid.GetLabel(&id_str, CSeq_id::eBoth);
    const CBioseq_Handle& subject_handle=m_AV->GetBioseqHandle(1);
    int subject_gi = FindGi(subject_handle.GetBioseqCore()->GetId());
    char urlBuf[2048];
   
    SFeatInfo* feat5 = NULL;
    SFeatInfo* feat3 = NULL;
    vector<SFeatInfo*>& feat_list 
        =  m_DynamicFeature->GetFeatInfo(id_str, actual_range, feat5, feat3, 2);
    
    string l_EntrezSubseqUrl = CBlastFormatUtil::GetURLFromRegistry("ENTREZ_SUBSEQ");

    if(feat_list.size() > 0) { //has feature in this range
        out << " Features in this part of subject sequence:" << endl;
        ITERATE(vector<SFeatInfo*>, iter, feat_list){
            out << "   ";
            if(m_AlignOption&eHtml && subject_gi > 0){
                sprintf(urlBuf, l_EntrezSubseqUrl.c_str(), subject_gi,
                        m_IsDbNa ? "Nucleotide" : "Protein",  
                        (*iter)->range.GetFrom() +1 , (*iter)->range.GetTo() + 1,
                        m_Rid.c_str());
                out << urlBuf;
            }  
            out << (*iter)->feat_str;
            if(m_AlignOption&eHtml && subject_gi > 0){
                out << "</a>";
            }  
            out << endl;
        }
    } else {  //show flank features
        if(feat5 || feat3){   
            out << " Features flanking this part of subject sequence:" << endl;
        }
        if(feat5){
            out << "   ";
            if(m_AlignOption&eHtml && subject_gi > 0){
                sprintf(urlBuf, l_EntrezSubseqUrl.c_str(), subject_gi,
                        m_IsDbNa ? "Nucleotide" : "Protein",  
                        feat5->range.GetFrom() + 1 , feat5->range.GetTo() + 1,
                        m_Rid.c_str());
                out << urlBuf;
            }  
            out << actual_range.GetFrom() - feat5->range.GetTo() 
                << " bp at 5' side: " << feat5->feat_str;
            if(m_AlignOption&eHtml && subject_gi > 0){
                out << "</a>";
            }  
            out << endl;
        }
        if(feat3){
            out << "   ";
            if(m_AlignOption&eHtml && subject_gi > 0){
                sprintf(urlBuf, l_EntrezSubseqUrl.c_str(), subject_gi,
                        m_IsDbNa ? "Nucleotide" : "Protein",  
                        feat3->range.GetFrom() + 1 , feat3->range.GetTo() + 1,
                        m_Rid.c_str());
                out << urlBuf;
            }
            out << feat3->range.GetFrom() - actual_range.GetTo() 
                << " bp at 3' side: " << feat3->feat_str;
            if(m_AlignOption&eHtml){
                out << "</a>";
            }  
            out << endl;
        }
    }
    if(feat_list.size() > 0 || feat5 || feat3 ){
        out << endl;
    }
}


void CDisplaySeqalign::x_FillLocList(list<SAlnSeqlocInfo*>& loc_list) const 
{
    if(!m_Seqloc){
        return;
    }
    for (list<CRef<blast::CSeqLocInfo> >::iterator iter=m_Seqloc->begin(); 
         iter!=m_Seqloc->end(); iter++){
        SAlnSeqlocInfo* alnloc = new SAlnSeqlocInfo;    
        for (int i=0; i<m_AV->GetNumRows(); i++){
            if((*iter)->GetInterval().GetId().Match(m_AV->GetSeqId(i))){
                int actualAlnStart = 0, actualAlnStop = 0;
                if(m_AV->IsPositiveStrand(i)){
                    actualAlnStart =
                        m_AV->GetAlnPosFromSeqPos(i, 
                            (*iter)->GetInterval().GetFrom());
                    actualAlnStop =
                        m_AV->GetAlnPosFromSeqPos(i, 
                            (*iter)->GetInterval().GetTo());
                } else {
                    actualAlnStart =
                        m_AV->GetAlnPosFromSeqPos(i, 
                            (*iter)->GetInterval().GetTo());
                    actualAlnStop =
                        m_AV->GetAlnPosFromSeqPos(i, 
                            (*iter)->GetInterval().GetFrom());
                }
                alnloc->aln_range.Set(actualAlnStart, actualAlnStop);      
                break;
            }
        }
        alnloc->seqloc = *iter;   
        loc_list.push_back(alnloc);
    }
}


list<CDisplaySeqalign::SAlnFeatureInfo*>* 
CDisplaySeqalign::x_GetQueryFeatureList(int row_num, int aln_stop) const
{
    list<SAlnFeatureInfo*>* bioseqFeature= new list<SAlnFeatureInfo*>[row_num];
    if(m_QueryFeature){
        for (list<FeatureInfo*>::iterator iter=m_QueryFeature->begin(); 
             iter!=m_QueryFeature->end(); iter++){
            for(int i = 0; i < row_num; i++){
                if((*iter)->seqloc->GetInt().GetId().Match(m_AV->GetSeqId(i))){
                    int actualSeqStart = 0, actualSeqStop = 0;
                    if(m_AV->IsPositiveStrand(i)){
                        if((*iter)->seqloc->GetInt().GetFrom() 
                           < m_AV->GetSeqStart(i)){
                            actualSeqStart = m_AV->GetSeqStart(i);
                        } else {
                            actualSeqStart = (*iter)->seqloc->GetInt().GetFrom();
                        }
                        
                        if((*iter)->seqloc->GetInt().GetTo() >
                           m_AV->GetSeqStop(i)){
                            actualSeqStop = m_AV->GetSeqStop(i);
                        } else {
                            actualSeqStop = (*iter)->seqloc->GetInt().GetTo();
                        }
                    } else {
                        if((*iter)->seqloc->GetInt().GetFrom() 
                           < m_AV->GetSeqStart(i)){
                            actualSeqStart = (*iter)->seqloc->GetInt().GetFrom();
                        } else {
                            actualSeqStart = m_AV->GetSeqStart(i);
                        }
                        
                        if((*iter)->seqloc->GetInt().GetTo() > 
                           m_AV->GetSeqStop(i)){
                            actualSeqStop = (*iter)->seqloc->GetInt().GetTo();
                        } else {
                            actualSeqStop = m_AV->GetSeqStop(i);
                        }
                    }
                    int alnFrom = m_AV->GetAlnPosFromSeqPos(i, actualSeqStart);
                    int alnTo = m_AV->GetAlnPosFromSeqPos(i, actualSeqStop);
                    
                    SAlnFeatureInfo* featInfo = new SAlnFeatureInfo;
                    string tempFeat = NcbiEmptyString;
                    x_SetFeatureInfo(featInfo, *((*iter)->seqloc), alnFrom, 
                                     alnTo,  aln_stop, (*iter)->feature_char,
                                     (*iter)->feature_id, tempFeat);    
                    bioseqFeature[i].push_back(featInfo);
                }
            }
        }
    }
    return bioseqFeature;
}


void CDisplaySeqalign::x_FillSeqid(string& id, int row) const
{
    if(m_AlignOption & eShowBlastStyleId) {
        if(row==0){//query
            id="Query";
        } else {//hits
            if (!(m_AlignOption&eMergeAlign)){
                //hits for pairwise 
                id="Sbjct";
            } else {
                if(m_AlignOption&eShowGi){
                    int gi = 0;
                    if(m_AV->GetSeqId(row).Which() == CSeq_id::e_Gi){
                        gi = m_AV->GetSeqId(row).GetGi();
                    }
                    if(!(gi > 0)){
                        gi = s_GetGiForSeqIdList(m_AV->GetBioseqHandle(row).\
                                                 GetBioseqCore()->GetId());
                    }
                    if(gi > 0){
                        id=NStr::IntToString(gi);
                    } else {
                        const CRef<CSeq_id> wid 
                            = FindBestChoice(m_AV->GetBioseqHandle(row).\
                                             GetBioseqCore()->GetId(), 
                                             CSeq_id::WorstRank);
                        id=wid->GetSeqIdString();
                    }
                } else {
                    const CRef<CSeq_id> wid 
                        = FindBestChoice(m_AV->GetBioseqHandle(row).\
                                         GetBioseqCore()->GetId(), 
                                         CSeq_id::WorstRank);
                    id=wid->GetSeqIdString();
                }           
            }
        }
    } else {
        if(m_AlignOption&eShowGi){
            int gi = 0;
            if(m_AV->GetSeqId(row).Which() == CSeq_id::e_Gi){
                gi = m_AV->GetSeqId(row).GetGi();
            }
            if(!(gi > 0)){
                gi = s_GetGiForSeqIdList(m_AV->GetBioseqHandle(row).\
                                         GetBioseqCore()->GetId());
            }
            if(gi > 0){
                id=NStr::IntToString(gi);
            } else {
                const CRef<CSeq_id> wid 
                    = FindBestChoice(m_AV->GetBioseqHandle(row).\
                                     GetBioseqCore()->GetId(),
                                     CSeq_id::WorstRank);
                id=wid->GetSeqIdString();
            }
        } else {
            const CRef<CSeq_id> wid 
                = FindBestChoice(m_AV->GetBioseqHandle(row).\
                                 GetBioseqCore()->GetId(), 
                                 CSeq_id::WorstRank);
            id=wid->GetSeqIdString();
        }     
    }
}

END_NCBI_SCOPE
