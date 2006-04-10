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
 * 12/2004
 * File Description:
 *   blast formatter utilities
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbireg.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbiutil.hpp>

#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>
#include <serial/objostrasnb.hpp> 
#include <serial/objistrasnb.hpp> 
#include <connect/ncbi_conn_stream.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp> 
#include <objects/blastdb/defline_extra.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>

#include <objtools/blast_format/blastfmtutil.hpp>

#include <algo/blast/api/version.hpp>

#include <objects/seq/seqport_util.hpp>
#include <objects/blastdb/Blast_def_line.hpp>
#include <objects/blastdb/Blast_def_line_set.hpp>

#include <stdio.h>

BEGIN_NCBI_SCOPE
USING_SCOPE (ncbi);
USING_SCOPE(objects);

///Get blast score information
///@param scoreList: score container to extract score info from
///@param score: place to extract the raw score to
///@param bits: place to extract the bit score to
///@param evalue: place to extract the e value to
///@param sum_n: place to extract the sum_n to
///@param num_ident: place to extract the num_ident to
///@param use_this_gi: place to extract use_this_gi to
///@return true if found score, false otherwise
///
template<class container> bool
s_GetBlastScore(const container&  scoreList, 
                int& score, 
                double& bits, 
                double& evalue, 
                int& sum_n, 
                int& num_ident,
                list<int>& use_this_gi,
                int& comp_adj_method)
{
    bool hasScore = false;
    ITERATE (typename container, iter, scoreList) {
        const CObject_id& id=(*iter)->GetId();
        if (id.IsStr()) {
            hasScore = true;
            if (id.GetStr()=="score"){
                score = (*iter)->GetValue().GetInt();
                
            } else if (id.GetStr()=="bit_score"){
                bits = (*iter)->GetValue().GetReal();
                
            } else if (id.GetStr()=="e_value" || id.GetStr()=="sum_e") {
                evalue = (*iter)->GetValue().GetReal();
            } else if (id.GetStr()=="use_this_gi"){
                use_this_gi.push_back((*iter)->GetValue().GetInt());
            } else if (id.GetStr()=="sum_n"){
                sum_n = (*iter)->GetValue().GetInt();          
            } else if (id.GetStr()=="num_ident"){
                num_ident = (*iter)->GetValue().GetInt();
            } else if (id.GetStr()=="comp_adjustment_method") {
                comp_adj_method = (*iter)->GetValue().GetInt();
            }
        }
    }
    return hasScore;
}


///Wrap a string to specified length.  If break happens to be in
/// a word, it will extend the line length until the end of the word
///@param str: input string
///@param line_len: length of each line desired
///@param out: stream to ouput
///
void static WrapOutputLine(string str, size_t line_len, 
                                      CNcbiOstream& out) 
{
    bool do_wrap = false;
    size_t length = str.size();
    if (length > line_len) {
        for (size_t i = 0; i < length; i ++) {
            if (i > 0 && i % line_len == 0) {
                do_wrap = true;
            }   
            out << str[i];
            if (do_wrap && isspace((unsigned char) str[i])) {
                out << endl;  
                do_wrap = false;
            }
        }
    } else {
        out << str;
    }

}
    
void CBlastFormatUtil::BlastPrintError(list<SBlastError>& 
                                       error_return, 
                                       bool error_post, CNcbiOstream& out)
{
   
    string errsevmsg[] = { "UNKNOWN","INFO","WARNING","ERROR",
                            "FATAL"};
    
    NON_CONST_ITERATE(list<SBlastError>, iter, error_return) {
        
        if(iter->level > 5){
            iter->level = eDiag_Info;
        }
        
        if(iter->level == 4){
            iter->level = eDiag_Fatal;
        } else{
            iter->level = iter->level;
        }

        if (error_post){
            ERR_POST_EX(iter->level, 0, iter->message);
        }
        out << errsevmsg[iter->level] << ": " << iter->message << endl;
       
    }

}

string CBlastFormatUtil::BlastGetVersion(const string program)
{
    string program_uc = program;
    return NStr::ToUpper(program_uc) + " " + blast::Version.Print() + " [" + 
        blast::Version.GetReleaseDate() + "]";
}

void CBlastFormatUtil::BlastPrintVersionInfo(const string program, bool html, 
                                             CNcbiOstream& out)
{
    if (html)
        out << "<b>" << BlastGetVersion(program) << "</b>" << endl;
    else
        out << BlastGetVersion(program) << endl;
}

void 
CBlastFormatUtil::BlastPrintReference(bool html, size_t line_len, 
                                      CNcbiOstream& out, 
                                      blast::CReference::EPublication pub) 
{
    if(html)
        out << "<b><a href=\""
            << blast::CReference::GetPubmedUrl(pub)
            << "\">Reference</a>:</b>"
            << endl;
    else
        out << "Reference: ";

    WrapOutputLine(blast::CReference::GetString(pub), line_len, out);
    out << endl;
}

void  CBlastFormatUtil::PrintTildeSepLines(string str, size_t line_len,
                                           CNcbiOstream& out) {
    
    list<string> split_line;
    NStr::Split(str, "~", split_line);
    ITERATE(list<string>, iter, split_line) {
        WrapOutputLine(*iter,  line_len, out);
        out << endl;
    }
}

void CBlastFormatUtil::PrintDbReport(list<SDbInfo>& 
                                     dbinfo_list,
                                     size_t line_length,  CNcbiOstream& out,
                                     bool top) 
{

    CBlastFormatUtil::SDbInfo* dbinfo = &(dbinfo_list.front());

    if (top) {
        out << "Database: ";
        WrapOutputLine(dbinfo->definition, line_length, out);
        out << endl;
        CBlastFormatUtil::AddSpace(out, 11);
        out << NStr::IntToString(dbinfo->number_seqs, NStr::fWithCommas) << 
            " sequences; " <<
            NStr::Int8ToString(dbinfo->total_length, NStr::fWithCommas) << 
            " total letters" << endl << endl;
    } else if (dbinfo->subset == false){
        out << "  Database: ";
        WrapOutputLine(dbinfo->definition, line_length, out);
        out << endl;

        out << "    Posted date:  ";
        out << dbinfo->date << endl;
	       
        out << "  Number of letters in database: "; 
        out << NStr::Int8ToString(dbinfo->total_length, NStr::fWithCommas) << endl;
        out << "  Number of sequences in database:  ";
        out << NStr::IntToString(dbinfo->number_seqs, NStr::fWithCommas) << endl;
        
    } else {
        out << "  Subset of the database(s) listed below" << endl;
        out << "  Number of letters searched: "; 
        out << NStr::Int8ToString(dbinfo->total_length, NStr::fWithCommas) << endl;
        out << "  Number of sequences searched:  ";
        out << NStr::IntToString(dbinfo->number_seqs, NStr::fWithCommas) << endl;
    }

}

void CBlastFormatUtil::PrintKAParameters(float lambda, float k, float h, 
                                         size_t line_len, 
                                         CNcbiOstream& out, bool gapped, 
                                         float c) 
{

    char buffer[256];
    if (gapped) { 
        out << "Gapped" << endl;
    }
    if (c == 0.0) {
        out << "Lambda     K      H";
    } else {
        out << "Lambda     K      H      C";
    }
    out << endl;
    sprintf(buffer, "%#8.3g ", lambda);
    out << buffer;
    sprintf(buffer, "%#8.3g ", k);
    out << buffer;
    sprintf(buffer, "%#8.3g ", h);
    out << buffer;
    if (c != 0.0) {
        sprintf(buffer, "%#8.3g ", c);
        WrapOutputLine(buffer, line_len, out);
    }
    out << endl;
}

string
CBlastFormatUtil::GetSeqIdString(const CBioseq& cbs, bool believe_local_id)
{
    const CBioseq::TId& ids = cbs.GetId();
    string all_id_str = NcbiEmptyString;
    CRef<CSeq_id> wid = FindBestChoice(ids, CSeq_id::WorstRank);
    if (wid && (wid->Which()!= CSeq_id::e_Local || believe_local_id)){
        int gi = FindGi(ids);
        if (strncmp(wid->AsFastaString().c_str(), "lcl|", 4) == 0) {
            if(gi == 0){
                all_id_str =  wid->AsFastaString().substr(4);
            } else {
                all_id_str = "gi|" + NStr::IntToString(gi) + 
                    "|" + wid->AsFastaString().substr(4);
            }
        } else {
            if(gi == 0){
                all_id_str = wid->AsFastaString();
            } else {
                all_id_str = "gi|" + NStr::IntToString(gi) + "|" +
                    wid->AsFastaString();
            }
        }
    }

    return all_id_str;
}

string
CBlastFormatUtil::GetSeqDescrString(const CBioseq& cbs)
{
    string all_descr_str = NcbiEmptyString;

    if (cbs.IsSetDescr()) {
        const CBioseq::TDescr& descr = cbs.GetDescr();
        const CBioseq::TDescr::Tdata& data = descr.Get();
        ITERATE(CBioseq::TDescr::Tdata, iter, data) {
            if((*iter)->IsTitle()) {
                all_descr_str += (*iter)->GetTitle();
            }
        }
    }
    return all_descr_str;
}

void CBlastFormatUtil::AcknowledgeBlastQuery(const CBioseq& cbs, 
                                             size_t line_len,
                                             CNcbiOstream& out,
                                             bool believe_query,
                                             bool html, bool tabular) 
{

    if (html) {
        out << "<b>Query=</b> ";
    } else if (tabular) {
        out << "# Query: ";
    } else {
        out << "Query= ";
    }
    
    string all_id_str = GetSeqIdString(cbs, believe_query);
    all_id_str += " ";
    all_id_str += GetSeqDescrString(cbs);

    // For tabular output, there is no limit on the line length.
    // There is also no extra line with the sequence length.
    if (tabular) {
        out << all_id_str;
    } else {
        WrapOutputLine(all_id_str, line_len, out);
        out << endl;
        if(cbs.IsSetInst() && cbs.GetInst().CanGetLength()){
            //out << "          (";
            out << "Length=";
            out << cbs.GetInst().GetLength() <<endl;
            // out << " letters)" << endl;
        }
    }
}

CRef<CBlast_def_line_set> 
CBlastFormatUtil::GetBlastDefline (const CBioseq_Handle& handle) 
{
    CRef<CBlast_def_line_set> bdls(new CBlast_def_line_set());
    if(handle.IsSetDescr()){
        const CSeq_descr& desc = handle.GetDescr();
        const list< CRef< CSeqdesc > >& descList = desc.Get();
        for (list<CRef< CSeqdesc > >::const_iterator iter = descList.begin();
             iter != descList.end(); iter++){
            
            if((*iter)->IsUser()){
                const CUser_object& uobj = (*iter)->GetUser();
                const CObject_id& uobjid = uobj.GetType();
                if(uobjid.IsStr()){
                    
                    const string& label = uobjid.GetStr();
                    if (label == kAsnDeflineObjLabel){
                        const vector< CRef< CUser_field > >& usf = 
                            uobj.GetData();
                        string buf;
                        
                        if(usf.front()->GetData().IsOss()){ 
                            //only one user field
                            typedef const CUser_field::TData::TOss TOss;
                            const TOss& oss = usf.front()->GetData().GetOss();
                            size_t size = 0;
                            //determine the octet string length
                            ITERATE (TOss, iter3, oss) {
                                size += (**iter3).size();
                            }
                            
                            int i =0;
                            char* temp = new char[size];
                            //retrive the string
                            ITERATE (TOss, iter3, oss) {
                                
                                for(vector< char >::iterator 
                                        iter4 = (**iter3).begin(); 
                                    iter4 !=(**iter3).end(); iter4++){
                                    temp[i] = *iter4;
                                    i++;
                                }
                            }            
                            
                            CConn_MemoryStream stream;
                            stream.write(temp, i);
                            auto_ptr<CObjectIStream> 
                                ois(CObjectIStream::
                                    Open(eSerial_AsnBinary, stream));
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


void CBlastFormatUtil::GetAlnScores(const CSeq_align& aln,
                                    int& score, 
                                    double& bits, 
                                    double& evalue,
                                    int& sum_n,
                                    int& num_ident,
                                    list<int>& use_this_gi)
{
    int comp_adj_method = 0; // dummy variable

    CBlastFormatUtil::GetAlnScores(aln, score, bits, evalue, sum_n, 
                                 num_ident, use_this_gi, comp_adj_method);
}


void CBlastFormatUtil::GetAlnScores(const CSeq_align& aln,
                                    int& score, 
                                    double& bits, 
                                    double& evalue,
                                    int& sum_n,
                                    int& num_ident,
                                    list<int>& use_this_gi,
                                    int& comp_adj_method)
{
    bool hasScore = false;
    score = -1;
    bits = -1;
    evalue = -1;
    sum_n = -1;
    num_ident = -1;
    comp_adj_method = 0;
    
    //look for scores at seqalign level first
    hasScore = s_GetBlastScore(aln.GetScore(), score, bits, evalue, 
                               sum_n, num_ident, use_this_gi, comp_adj_method);
    
    //look at the seg level
    if(!hasScore){
        const CSeq_align::TSegs& seg = aln.GetSegs();
        if(seg.Which() == CSeq_align::C_Segs::e_Std){
            s_GetBlastScore(seg.GetStd().front()->GetScores(),  
                            score, bits, evalue, sum_n, num_ident, use_this_gi, comp_adj_method);
        } else if (seg.Which() == CSeq_align::C_Segs::e_Dendiag){
            s_GetBlastScore(seg.GetDendiag().front()->GetScores(), 
                            score, bits, evalue, sum_n, num_ident, use_this_gi, comp_adj_method);
        }  else if (seg.Which() == CSeq_align::C_Segs::e_Denseg){
            s_GetBlastScore(seg.GetDenseg().GetScores(),  
                            score, bits, evalue, sum_n, num_ident, use_this_gi, comp_adj_method);
        }
    }	
}

void CBlastFormatUtil::AddSpace(CNcbiOstream& out, size_t number)

{
    for(size_t i=0; i<number; i++){
        out<<" ";
    }

}

void CBlastFormatUtil::GetScoreString(double evalue, double bit_score, 
                                      string& evalue_str, 
                                      string& bit_score_str)
{
    char evalue_buf[10], bit_score_buf[10];
    
    if (evalue < 1.0e-180) {
        sprintf(evalue_buf, "0.0");
    } else if (evalue < 1.0e-99) {
        sprintf(evalue_buf, "%2.0le", evalue);        
    } else if (evalue < 0.0009) {
        sprintf(evalue_buf, "%3.0le", evalue);
    } else if (evalue < 0.1) {
        sprintf(evalue_buf, "%4.3lf", evalue);
    } else if (evalue < 1.0) { 
        sprintf(evalue_buf, "%3.2lf", evalue);
    } else if (evalue < 10.0) {
        sprintf(evalue_buf, "%2.1lf", evalue);
    } else { 
        sprintf(evalue_buf, "%5.0lf", evalue);
    }
    
    if (bit_score > 9999){
        sprintf(bit_score_buf, "%4.3le", bit_score);
    } else if (bit_score > 99.9){
        sprintf(bit_score_buf, "%4.0ld", (long)bit_score);
    } else {
        sprintf(bit_score_buf, "%4.1lf", bit_score);
    }
    evalue_str = evalue_buf;
    bit_score_str = bit_score_buf;
}


void CBlastFormatUtil::PruneSeqalign(CSeq_align_set& source_aln, 
                                     CSeq_align_set& new_aln,
                                     unsigned int number)
{
    CConstRef<CSeq_id> previous_id, subid; 
    bool is_first_aln = true;
    unsigned int num_align = 0;
    ITERATE(CSeq_align_set::Tdata, iter, source_aln.Get()){ 
        if(num_align >= number) {
            break;
        }
        if ((*iter)->GetSegs().IsDisc()) {
            ++num_align;
        } else {
            subid = &((*iter)->GetSeq_id(1));
            if(is_first_aln || (!is_first_aln && !subid->Match(*previous_id))){
                ++num_align;
            }
            is_first_aln = false;
            previous_id = subid;
        }
        new_aln.Set().push_back(*iter);
    }
}

void 
CBlastFormatUtil::GetAlignLengths(CAlnVec& salv, int& align_length, 
                                  int& num_gaps, int& num_gap_opens)
{
    num_gaps = num_gap_opens = align_length = 0;

    for (int row = 0; row < salv.GetNumRows(); row++) {
        CRef<CAlnMap::CAlnChunkVec> chunk_vec
            = salv.GetAlnChunks(row, salv.GetSeqAlnRange(0));
        for (int i=0; i<chunk_vec->size(); i++) {
            CConstRef<CAlnMap::CAlnChunk> chunk = (*chunk_vec)[i];
            int chunk_length = chunk->GetAlnRange().GetLength();
            // Gaps are counted on all rows: gap can only be in one of the rows
            // for any given segment.
            if (chunk->IsGap()) {
                ++num_gap_opens;
                num_gaps += chunk_length;
            }
            // To calculate alignment length, only one row is needed.
            if (row == 0)
                align_length += chunk_length;
        }
    }
}

void 
CBlastFormatUtil::ExtractSeqalignSetFromDiscSegs(CSeq_align_set& target,
                                                 const CSeq_align_set& source)
{
    if (source.IsSet() && source.CanGet()) {
        
        for(CSeq_align_set::Tdata::const_iterator iter = source.Get().begin();
            iter != source.Get().end(); iter++) {
            if((*iter)->IsSetSegs()){
                const CSeq_align::TSegs& seg = (*iter)->GetSegs();
                if(seg.IsDisc()){
                    const CSeq_align_set& set = seg.GetDisc();
                    for(CSeq_align_set::Tdata::const_iterator iter2 =
                            set.Get().begin(); iter2 != set.Get().end(); 
                        iter2 ++) {
                        target.Set().push_back(*iter2);
                    }
                } else {
                    target.Set().push_back(*iter);
                }
            }
        }
    }
}

CRef<CSeq_align> 
CBlastFormatUtil::CreateDensegFromDendiag(const CSeq_align& aln) 
{
    CRef<CSeq_align> sa(new CSeq_align);
    if ( !aln.GetSegs().IsDendiag()) {
        NCBI_THROW(CException, eUnknown, "Input Seq-align should be Dendiag!");
    }
    
    if(aln.IsSetType()){
        sa->SetType(aln.GetType());
    }
    if(aln.IsSetDim()){
        sa->SetDim(aln.GetDim());
    }
    if(aln.IsSetScore()){
        sa->SetScore() = aln.GetScore();
    }
    if(aln.IsSetBounds()){
        sa->SetBounds() = aln.GetBounds();
    }
    
    CDense_seg& ds = sa->SetSegs().SetDenseg();
    
    int counter = 0;
    ds.SetNumseg() = 0;
    ITERATE (CSeq_align::C_Segs::TDendiag, iter, aln.GetSegs().GetDendiag()){
        
        if(counter == 0){//assume all dendiag segments have same dim and ids
            if((*iter)->IsSetDim()){
                ds.SetDim((*iter)->GetDim());
            }
            if((*iter)->IsSetIds()){
                ds.SetIds() = (*iter)->GetIds();
            }
        }
        ds.SetNumseg() ++;
        if((*iter)->IsSetStarts()){
            ITERATE(CDense_diag::TStarts, iterStarts, (*iter)->GetStarts()){
                ds.SetStarts().push_back(*iterStarts);
            }
        }
        if((*iter)->IsSetLen()){
            ds.SetLens().push_back((*iter)->GetLen());
        }
        if((*iter)->IsSetStrands()){
            ITERATE(CDense_diag::TStrands, iterStrands, (*iter)->GetStrands()){
                ds.SetStrands().push_back(*iterStrands);
            }
        }
        if((*iter)->IsSetScores()){
            ITERATE(CDense_diag::TScores, iterScores, (*iter)->GetScores()){
                ds.SetScores().push_back(*iterScores); //this might not have
                                                       //right meaning
            }
        }
        counter ++;
    }
    
    return sa;
}

int CBlastFormatUtil::GetTaxidForSeqid(const CSeq_id& id, CScope& scope)
{
    int taxid = 0;
    try{
        const CBioseq_Handle& handle = scope.GetBioseqHandle(id);
        const CRef<CBlast_def_line_set> bdlRef = 
            CBlastFormatUtil::GetBlastDefline(handle);
        const list< CRef< CBlast_def_line > >& bdl = bdlRef->Get();
        ITERATE(list<CRef<CBlast_def_line> >, iter_bdl, bdl) {
            CConstRef<CSeq_id> bdl_id = 
                GetSeq_idByType((*iter_bdl)->GetSeqid(), id.Which());
            if(bdl_id && bdl_id->Match(id) && 
               (*iter_bdl)->IsSetTaxid() && (*iter_bdl)->CanGetTaxid()){
                taxid = (*iter_bdl)->GetTaxid();
                break;
            }
        }
    } catch (CException&) {
        
    }
    return taxid;
}

int CBlastFormatUtil::GetFrame (int start, ENa_strand strand, 
                                const CBioseq_Handle& handle) 
{
    int frame = 0;
    if (strand == eNa_strand_plus) {
        frame = (start % 3) + 1;
    } else if (strand == eNa_strand_minus) {
        frame = -(((int)handle.GetBioseqLength() - start - 1)
                  % 3 + 1);
        
    }
    return frame;
}


CBlastFormattingMatrix::CBlastFormattingMatrix(int** data, unsigned int nrows, 
                                               unsigned int ncols)
{
    const int kAsciiSize = 256;
    Resize(kAsciiSize, kAsciiSize, INT_MIN);
    
    // Create a CSeq_data object from a vector of values from 0 to the size of
    // the matrix (26).
    const int kNumValues = max(ncols, nrows);
    vector<char> ncbistdaa_values(kNumValues);
    for (int index = 0; index < kNumValues; ++index)
        ncbistdaa_values[index] = (char) index;

    CSeq_data ncbistdaa_seq(ncbistdaa_values, CSeq_data::e_Ncbistdaa);

    // Convert to IUPACaa using the CSeqportUtil::Convert method.
    CSeq_data iupacaa_seq;
    CSeqportUtil::Convert(ncbistdaa_seq, &iupacaa_seq, CSeq_data::e_Iupacaa);
    
    // Extract the IUPACaa values
    vector<char> iupacaa_values(kNumValues);
    for (int index = 0; index < kNumValues; ++index)
        iupacaa_values[index] = iupacaa_seq.GetIupacaa().Get()[index];

    // Fill the 256x256 output matrix.
    for (unsigned int row = 0; row < nrows; ++row) {
        for (unsigned int col = 0; col < ncols; ++col) {
            if (iupacaa_values[row] >= 0 && iupacaa_values[col] >= 0) {
                (*this)((int)iupacaa_values[row], (int)iupacaa_values[col]) = 
                    data[row][col];
            }
        }
    }
}

END_NCBI_SCOPE
