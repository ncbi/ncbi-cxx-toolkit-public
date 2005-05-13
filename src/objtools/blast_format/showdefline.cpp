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
 *   Display blast defline
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbireg.hpp>
#include <serial/objostrasnb.hpp> 
#include <serial/objistrasnb.hpp> 
#include <connect/ncbi_conn_stream.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>

#include <serial/iterator.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/blastdb/Blast_def_line.hpp>
#include <objects/blastdb/Blast_def_line_set.hpp>
#include <objects/blastdb/defline_extra.hpp>
#include <objtools/blast_format/showdefline.hpp>
#include <objtools/blast_format/blastfmtutil.hpp>

#include <stdio.h>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
USING_SCOPE(sequence);

//margins constant

static string kOneSpaceMargin = "  ";
static string kTwoSpaceMargin = "  ";

//const strings
static const string kHeader = "Sequences producing significant alignments:";
static const string kScore = "Score";
static const string kE = "E";
static const string kBits = "(Bits)";
static const string kValue = "Value";
static const string kN = "N";
static const string kRepeatHeader = "Sequences used in model and found again:";
static const string kNewSeqHeader = "Sequences not found previously or not pr\
eviously below threshold:";


//psiblast related
static const string kPsiblastNewSeqGif = "<IMG SRC=\"/blast/images/new.gif\" \
WIDTH=30 HEIGHT=15 ALT=\"New sequence mark\">";

static const string kPsiblastNewSeqBackgroundGif = "<IMG SRC=\"/blast/images/\
bg.gif\" WIDTH=30 HEIGHT=15 ALT=\" \">";

static const string kPsiblastCheckedBackgroundGif = "<IMG SRC=\"/blast/images\
/bg.gif\" WIDTH=15 HEIGHT=15 ALT=\" \">";

static const string kPsiblastCheckedGif = "<IMG SRC=\"/blast/images/checked.g\
if\" WIDTH=15 HEIGHT=15 ALT=\"Checked mark\">";

static const string kPsiblastEvalueLink = "<a name = Evalue></a>";

static const string  kPsiblastCheckboxChecked = "<INPUT TYPE=\"checkbox\" NAME\
=\"checked_GI\" VALUE=\"%d\" CHECKED>  <INPUT TYPE=\"hidden\" NAME =\"good_G\
I\" VALUE = \"%d\">";

static const string  kPsiblastCheckbox =  "<INPUT TYPE=\"checkbox\" NAME=\"ch\
ecked_GI\" VALUE=\"%d\">  ";


///Return url for seqid
///@param ids: input seqid list
///@param gi: gi to use
///@param user_url: use this user specified url if it's non-empty
///@param is_db_na: is the database nucleotide or not
///@param db_name: name of the database
///@param open_new_window: click the url to open a new window?
///@param rid: RID
///@param query_number: the query number
///
static string s_GetIdUrl(const CBioseq::TId& ids, int gi, string& user_url,
                         bool is_db_na, string& db_name, bool open_new_window, 
                         string& rid, int query_number)
{
    string url_link = NcbiEmptyString;
    CConstRef<CSeq_id> wid = FindBestChoice(ids, CSeq_id::WorstRank);
    char dopt[32], db[32];
    if(user_url == NcbiEmptyString ||
       (gi > 0 && user_url.find("dumpgnl.cgi") != string::npos)){ 
        //use entrez or dbtag specified
        if(is_db_na) {
            strcpy(dopt, "GenBank");
            strcpy(db, "Nucleotide");
        } else {
            strcpy(dopt, "GenPept");
            strcpy(db, "Protein");
        }    
        
        char url_buf[2048];
        if (gi > 0) {
            sprintf(url_buf, kEntrezUrl.c_str(), db, gi, dopt,
                    open_new_window ? "TARGET=\"EntrezView\"" : "");
            url_link = url_buf;
        } else {//seqid general, dbtag specified
            if(wid->Which() == CSeq_id::e_General){
                const CDbtag& dtg = wid->GetGeneral();
                const string& dbname = dtg.GetDb();
                if(NStr::CompareNocase(dbname, "TI") == 0){
                    string actual_id;
                    wid->GetLabel(&actual_id, CSeq_id::eContent);
                    sprintf(url_buf, kTraceUrl.c_str(), actual_id.c_str());
                    url_link = url_buf;
                }
            }
        }
    } else { //need to use user_url 
        bool nodb_path =  false;
        CConstRef<CSeq_id> id_general = GetSeq_idByType(ids,
                                                        CSeq_id::e_General);
        CConstRef<CSeq_id> id_other = GetSeq_idByType(ids, CSeq_id::e_Other);       
        if(!id_general.Empty() && 
           id_general->AsFastaString().find("gnl|BL_ORD_ID") != string::npos){ 
            /* We do need to make security protected link to BLAST gnl */
            return NcbiEmptyString;
        }
        
        /* dumpgnl.cgi need to use path  */
        if (user_url.find("dumpgnl.cgi") ==string::npos){
            nodb_path = true;
        }  
        size_t length = db_name.size();
        string actual_db;
        char  *chptr, *dbtmp;
        Char tmpbuff[256];
        char* dbname = new char[sizeof(char)*length + 2];
        strcpy(dbname, db_name.c_str());
        if(nodb_path) {
            size_t i, j;
            /* aditional space and NULL */
            dbtmp = new char[sizeof(char)*length + 2]; 
            
            memset(dbtmp, '\0', sizeof(char)*length + 2);
            for(i = 0; i < length; i++) {    
                /* Rolling spaces */
                if(isspace(dbname[i]) || dbname[i] == ',') {
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
        actual_db = dbtmp ? dbtmp : "nr";
        url_link += "<a href=\"";
        if (user_url.find("?") == string::npos){
            url_link += user_url + "?" + "db=" + actual_db + "&na=" + 
                (is_db_na? "1" : "0");
        } else {
            url_link += user_url + "&db=" + actual_db + "&na=" + 
                (is_db_na? "1" : "0");
        }
        const CSeq_id* bestid = NULL;
        if (id_general.Empty()){
            bestid = id_other;
            if (id_other.Empty()){
                bestid = wid;
            }
        } else {
            bestid = id_general;
        }
        
        char gnl[256];
        if (bestid && bestid->Which() !=  CSeq_id::e_Gi){
            strcpy(gnl, bestid->AsFastaString().c_str());
        } else {
            gnl[0] = '\0';
        }
        
        if (gnl[0] != '\0'){
            url_link += "&";
            url_link += "gnl=";
            url_link += gnl;
           
        }
        if (gi > 0){
            url_link += "&";
            url_link += "gi=" + NStr::IntToString(gi);
        }
        if (rid != NcbiEmptyString){
            url_link += "&";
            url_link += "RID=" + rid;
        }        
        if ( query_number > 0){
            url_link += "&";
            url_link += "QUERY_NUMBER=" + NStr::IntToString(query_number);
        }
        url_link += "\">";
        if(nodb_path){
            delete [] dbtmp;
        }
        delete [] dbname;
    }
    return url_link;
}


///Get linkout membership
///@param bdl: blast defline to get linkout membership from
///@return the value representing the membership bits set
///
static int s_GetLinkout(const CBlast_def_line& bdl)
{
    int linkout = 0;
    
    if (bdl.IsSetLinks()){
        for (list< int >::const_iterator iter = bdl.GetLinks().begin();
             iter != bdl.GetLinks().end(); iter ++){
            if ((*iter) & eUnigene) {
                linkout += eUnigene;
            }
            if ((*iter) & eStructure){
                linkout += eStructure;
            } 
            if ((*iter) & eGeo){
                linkout += eGeo;
            } 
            if ((*iter) & eGene){
                linkout += eGene;
            }
        }
    }
    return linkout;
}


///Get the url for linkout
///@param linkout: the membership value
///@param gi: the actual gi or 0
///@param rid: RID
///@param cdd_rid: CDD RID
///@param entrez_term: entrez query term
///@param is_na: is this sequence nucleotide or not
///
static list<string> s_GetLinkoutString(int linkout, int gi, string& rid, 
                                       string& cdd_rid, string& entrez_term,
                                       bool is_na)
{
    list<string> linkout_list;
    char molType[8]={""};
    if(!is_na){
        sprintf(molType, "[pgi]");
    }
    else {
        sprintf(molType, "[ngi]");
    }
    
    char buf[1024];
    
    if (linkout & eUnigene) {
        sprintf(buf, kUnigeneUrl.c_str(),  gi);
        linkout_list.push_back(buf);
    }
    if (linkout & eStructure){
        sprintf(buf, kStructureUrl.c_str(), rid.c_str(), gi, gi, 
                cdd_rid.c_str(), "onegroup", 
                (entrez_term == NcbiEmptyString) ? 
                "none":((char*) entrez_term.c_str()));
        linkout_list.push_back(buf);
    }
    if (linkout & eGeo){
        sprintf(buf, kGeoUrl.c_str(), gi);
        linkout_list.push_back(buf);
    }
    if(linkout & eGene){
        sprintf(buf, kGeneUrl.c_str(), gi, !is_na ? "PUID" : "NUID");
        linkout_list.push_back(buf);
    }
    return linkout_list;
}


void CShowBlastDefline::x_FillDeflineAndId(const CBioseq_Handle& handle,
                                           const CSeq_id& aln_id,
                                           list<int>& use_this_gi,
                                           SDeflineInfo* sdl)
{
    const CRef<CBlast_def_line_set> bdlRef = CBlastFormatUtil::GetBlastDefline(handle);
    const list< CRef< CBlast_def_line > >& bdl = bdlRef->Get();
    const CBioseq::TId& ids = handle.GetBioseqCore()->GetId();
    CRef<CSeq_id> wid;
    sdl->defline = NcbiEmptyString;
 
    sdl->gi = 0;
    sdl->id_url = NcbiEmptyString;
    sdl->score_url = NcbiEmptyString;
    sdl->linkout = 0;
    sdl->is_new = false;
    sdl->was_checked = false;

    //get psiblast stuff
    
    if(m_SeqStatus){
        string aln_id_str;
        aln_id.GetLabel(&aln_id_str, CSeq_id::eContent);
        int seq_status;
        
        map<string, int>::const_iterator iter = m_SeqStatus->find(aln_id_str);
        if ( iter != m_SeqStatus->end() ){
            seq_status = iter->second;
        }
        if((m_PsiblastStatus == eFirstPass) ||
           ((m_PsiblastStatus == eRepeatPass) && (seq_status & eRepeatSeq))
           || ((m_PsiblastStatus == eNewPass) && (seq_status & eRepeatSeq))){
            if(!(seq_status & eGoodSeq)){
                sdl->is_new = true;
            }
            if(seq_status & eCheckedSeq){
                sdl->was_checked = true;
            }   
        }
    }
        
    //get id
    if(use_this_gi.empty() || bdl.empty()){
     
        wid = FindBestChoice(ids, CSeq_id::WorstRank);
        sdl->id = wid;
        sdl->gi = FindGi(ids);      
    } else {        
        bool found = false;
        for(list< CRef< CBlast_def_line > >::const_iterator iter = bdl.begin();
            iter != bdl.end(); iter++){
            const CBioseq::TId& cur_id = (*iter)->GetSeqid();
            int cur_gi =  FindGi(cur_id);
            ITERATE(list<int>, iter_gi, use_this_gi){
                if(cur_gi == *iter_gi){
                    wid = FindBestChoice(cur_id, CSeq_id::WorstRank);
                    sdl->id =  wid;
                    sdl->gi = cur_gi;     
                    found = true;
                    break;
                }
            }
            if(found){
                break;
            }
        }
    }

    //get linkout
    if((m_Option & eLinkout)){
        bool linkout_not_found = true;
        for(list< CRef< CBlast_def_line > >::const_iterator iter = bdl.begin();
            iter != bdl.end(); iter++){
            const CBioseq::TId& cur_id = (*iter)->GetSeqid();
            int cur_gi =  FindGi(cur_id);            
            if(use_this_gi.empty()){
                if(sdl->gi == cur_gi){                 
                    sdl->linkout = s_GetLinkout((**iter));
                    sdl->linkout_list =
                        s_GetLinkoutString(sdl->linkout,
                                           sdl->gi, m_Rid, 
                                           m_CddRid, 
                                           m_EntrezTerm, 
                                           handle.GetBioseqCore()->IsNa());
                    break;
                }
            } else {
                ITERATE(list<int>, iter_gi, use_this_gi){
                    if(cur_gi == *iter_gi){                     
                        sdl->linkout = s_GetLinkout((**iter));
                        sdl->linkout_list = 
                            s_GetLinkoutString(sdl->linkout, 
                                               cur_gi, m_Rid,
                                               m_CddRid,
                                               m_EntrezTerm,
                                               handle.GetBioseqCore()->IsNa());
                        linkout_not_found = false;
                        break;
                    }
                }
                if(!linkout_not_found){
                    break;
                } 
            }
            
        }
    }
    //get score and id url
    if(m_Option & eHtml){
        string accession;
        sdl->id->GetLabel(&accession, CSeq_id::eContent);
        sdl->score_url = "<a href=#";
        sdl->score_url += sdl->gi == 0 ? accession : 
            NStr::IntToString(sdl->gi);
        sdl->score_url += ">";

        string user_url= m_Reg->Get(m_BlastType, "TOOL_URL");
        sdl->id_url = s_GetIdUrl(ids, FindGi(ids), user_url,
                                 m_IsDbNa, m_Database, 
                                 (m_Option & eNewTargetWindow) ? true : false,
                                 m_Rid, m_QueryNumber);
    }
    
    //get defline
    if (bdl.empty()){
        sdl->defline = GetTitle(handle);
    } else { 
        bool is_first = true;
        for(list< CRef< CBlast_def_line > >::const_iterator iter = bdl.begin();
            iter != bdl.end(); iter++){
            const CBioseq::TId& cur_id = (*iter)->GetSeqid();
            int cur_gi =  FindGi(cur_id);
            int gi_in_use_this_gi = 0;
            ITERATE(list<int>, iter_gi, use_this_gi){
                if(cur_gi == *iter_gi){
                    gi_in_use_this_gi = *iter_gi;                 
                    break;
                }
            }
            if(use_this_gi.empty() || gi_in_use_this_gi > 0) {
                
                if((*iter)->IsSetTitle()){
                    if(is_first){
                        sdl->defline = (*iter)->GetTitle();
                    } else {
                        string concat_acc;
                        wid = FindBestChoice(cur_id, CSeq_id::WorstRank);
                        wid->GetLabel(&concat_acc, CSeq_id::eFasta, 0);
                        if( (m_Option & eShowGi) && cur_gi > 0){
                            sdl->defline =  sdl->defline + " >" + "gi|" + 
                                NStr::IntToString(cur_gi) + "|" + 
                                concat_acc + " " + (*iter)->GetTitle();
                        } else {
                            sdl->defline = sdl->defline + " >" + concat_acc +
                                " " + 
                                (*iter)->GetTitle();
                        }
                    }
                    is_first = false;
                }
            }
        }
    }
}


//Constructor
CShowBlastDefline::CShowBlastDefline(const CSeq_align_set& seqalign,
                                     CScope& scope,
                                     size_t line_length,
                                     size_t num_defline_to_show):
    m_AlnSetRef(&seqalign), 
    m_ScopeRef(&scope),
    m_LineLen(line_length),
    m_NumToShow(num_defline_to_show)
{
    
    m_Option = 0;
    m_EntrezTerm = NcbiEmptyString;
    m_QueryNumber = 0;
    m_Rid = NcbiEmptyString;
    m_CddRid = NcbiEmptyString;
    m_IsDbNa = true;
    m_BlastType = NcbiEmptyString;
    m_PsiblastStatus = eFirstPass;
    m_SeqStatus = NULL;
    

}

CShowBlastDefline::~CShowBlastDefline()
{
   
    ITERATE(list<SDeflineInfo*>, iter, m_DeflineList){
        delete *iter;
    }
}

void CShowBlastDefline::DisplayBlastDefline(CNcbiOstream & out)
{
    /*Note we can't just show each alnment as we go because we will 
      need to show defline only once for all hsp's with the same id*/
    
    bool is_first_aln = true;
    size_t num_align = 0;
    CConstRef<CSeq_id> previous_id, subid;
    size_t max_score_len = kBits.size(), max_evalue_len = kValue.size();
    size_t max_sum_n_len =1;

    if(m_Option & eHtml){
        m_ConfigFile.reset(new CNcbiIfstream(".ncbirc"));
        m_Reg.reset(new CNcbiRegistry(*m_ConfigFile));  
    }
    //prepare defline
    for (CSeq_align_set::Tdata::const_iterator 
             iter = m_AlnSetRef->Get().begin(); 
         iter != m_AlnSetRef->Get().end() && num_align < m_NumToShow; 
         iter++){
        
        subid = &((*iter)->GetSeq_id(1));
        if(is_first_aln || (!is_first_aln && !subid->Match(*previous_id))) {
            SDeflineInfo* sdl = x_GetDeflineInfo(**iter);
            if(sdl){
                m_DeflineList.push_back(sdl);
                if(max_score_len < sdl->bit_string.size()){
                    max_score_len = sdl->bit_string.size();
                }
                if(max_evalue_len < sdl->evalue_string.size()){
                    max_evalue_len = sdl->evalue_string.size();
                }

                if( max_sum_n_len < NStr::IntToString(sdl->sum_n).size()){
                    max_sum_n_len = NStr::IntToString(sdl->sum_n).size();
                }
            }
            num_align++;
        }
        is_first_aln = false;
        previous_id = subid;
      
    }
    //actual output
    if((m_Option & eLinkout) && (m_Option & eHtml)){
        ITERATE(list<SDeflineInfo*>, iter, m_DeflineList){
            if((*iter)->linkout & eStructure){
                char buf[512];
                sprintf(buf, kStructure_Overview.c_str(), m_Rid.c_str(),
                        0, 0, m_CddRid.c_str(), "overview",
                        m_EntrezTerm == NcbiEmptyString ?
                        m_EntrezTerm.c_str() : "none");
                out << buf <<endl<<endl;
                break;
            }
        }
    }

    if(!(m_Option & eNoShowHeader)) {
        if((m_PsiblastStatus == eFirstPass) ||
           (m_PsiblastStatus == eRepeatPass)){
            CBlastFormatUtil::AddSpace(out, m_LineLen + kOneSpaceMargin.size());
            if(m_Option & eHtml){
                if((m_Option & eShowNewSeqGif)){
                    out << kPsiblastNewSeqBackgroundGif;
                    out << kPsiblastCheckedBackgroundGif;
                }
                if (m_Option & eCheckbox) {
                    out << kPsiblastNewSeqBackgroundGif;
                    out << kPsiblastCheckedBackgroundGif;
                }
            }
            out << kScore;
            CBlastFormatUtil::AddSpace(out, max_score_len - kScore.size());
            CBlastFormatUtil::AddSpace(out, kTwoSpaceMargin.size());
            CBlastFormatUtil::AddSpace(out, 2); //E align to l of value
            out << kE;
            out << endl;
            out << kHeader;
            if(m_Option & eHtml){
                if((m_Option & eShowNewSeqGif)){
                    out << kPsiblastNewSeqBackgroundGif;
                    out << kPsiblastCheckedBackgroundGif;
                }
                if (m_Option & eCheckbox) {
                    out << kPsiblastNewSeqBackgroundGif;
                    out << kPsiblastCheckedBackgroundGif;
                }
            }
            CBlastFormatUtil::AddSpace(out, m_LineLen - kHeader.size());
            CBlastFormatUtil::AddSpace(out, kOneSpaceMargin.size());
            out << kBits;
            //in case max_score_len > kBits.size()
            CBlastFormatUtil::AddSpace(out, max_score_len - kBits.size()); 
            CBlastFormatUtil::AddSpace(out, kTwoSpaceMargin.size());
            out << kValue;
            if(m_Option & eShowSumN){
                CBlastFormatUtil::AddSpace(out, max_evalue_len - kValue.size()); 
                CBlastFormatUtil::AddSpace(out, kTwoSpaceMargin.size());
                out << kN;
            }
            out << endl<<endl;
        }
        if(m_PsiblastStatus == eRepeatPass){
            out << kRepeatHeader <<endl;
        }
        if(m_PsiblastStatus == eNewPass){
            out << kNewSeqHeader <<endl;
        }
    }
    
    bool first_new =true;
    ITERATE(list<SDeflineInfo*>, iter, m_DeflineList){
        size_t line_length = 0;
        string line_component;
        if ((m_Option & eHtml) && ((*iter)->gi > 0)){
            if((m_Option & eShowNewSeqGif)) { 
                if ((*iter)->is_new) {
                    if (first_new) {
                        first_new = false;
                        out << kPsiblastEvalueLink;
                    }
                    out << kPsiblastNewSeqGif;
                
                } else {
                    out << kPsiblastNewSeqBackgroundGif;
                }
                if ((*iter)->was_checked) {
                    out << kPsiblastCheckedGif;
                    
                } else {
                    out << kPsiblastCheckedBackgroundGif;
                }
            }
            char buf[256];
            if((m_Option & eCheckboxChecked)){
                sprintf(buf, kPsiblastCheckboxChecked.c_str(), (*iter)->gi, 
                        (*iter)->gi);
                out << buf;
            } else if (m_Option & eCheckbox) {
                sprintf(buf, kPsiblastCheckbox.c_str(), (*iter)->gi);
                out << buf;
            }
        }
        
        
        if((m_Option & eHtml) && ((*iter)->id_url != NcbiEmptyString)) {
            out << (*iter)->id_url;
        }
        if(m_Option & eShowGi){
            if((*iter)->gi > 0){
                line_component = "gi|" + NStr::IntToString((*iter)->gi) + "|";
                out << line_component;
                line_length += line_component.size();
            }
        }
        if(!(*iter)->id.Empty()){
            if(!((*iter)->id->AsFastaString().find("gnl|BL_ORD_ID") 
                 != string::npos)){
                out << (*iter)->id->AsFastaString();
                line_length += (*iter)->id->AsFastaString().size();
            }
        }
        if((m_Option & eHtml) && ((*iter)->id_url != NcbiEmptyString)) {
            out << "</a>";
        }        
        line_component = "  " + (*iter)->defline; 
        string actual_line_component;
        if(line_component.size() > m_LineLen){
            actual_line_component = line_component.substr(0, m_LineLen - 
                                                          line_length - 3);
            actual_line_component += "...";
        } else {
            actual_line_component = line_component.substr(0, m_LineLen - 
                                                          line_length);
        }
        out << actual_line_component; 
        line_length += actual_line_component.size();
        //pad the short lines
        CBlastFormatUtil::AddSpace(out, m_LineLen - line_length);
        out << kTwoSpaceMargin;
       
        if((m_Option & eHtml) && ((*iter)->score_url != NcbiEmptyString)) {
            out << (*iter)->score_url;
        }
        out << (*iter)->bit_string;
        if((m_Option & eHtml) && ((*iter)->score_url != NcbiEmptyString)) {
            out << "</a>";
        }   
        CBlastFormatUtil::AddSpace(out, max_score_len - (*iter)->bit_string.size());
        out << kTwoSpaceMargin << (*iter)->evalue_string;
        CBlastFormatUtil::AddSpace(out, max_evalue_len - (*iter)->evalue_string.size());
        if(m_Option & eShowSumN){ 
            out << kTwoSpaceMargin << (*iter)->sum_n;   
            CBlastFormatUtil::AddSpace(out, max_sum_n_len - 
                     NStr::IntToString((*iter)->sum_n).size());
        }
        if((m_Option & eLinkout) && (m_Option & eHtml)){
            bool is_first = true;
            ITERATE(list<string>, iter_linkout, (*iter)->linkout_list){
                if(is_first){
                    out << kOneSpaceMargin;
                    is_first = false;
                }
                out << *iter_linkout;
            }
        }
        out <<endl;
    }
}

CShowBlastDefline::SDeflineInfo* 
CShowBlastDefline::x_GetDeflineInfo(const CSeq_align& aln)
{
    SDeflineInfo* sdl = NULL;
    const CSeq_id& id = aln.GetSeq_id(1); 
    string evalue_buf, bit_score_buf;
    int score = 0;
    double bits = 0;
    double evalue = 0;
    int sum_n = 0;
    int num_ident = 0;
    list<int> use_this_gi;       
      
    try{
        const CBioseq_Handle& handle = m_ScopeRef->GetBioseqHandle(id);
      
        CBlastFormatUtil::GetAlnScores(aln, score, bits, evalue, sum_n, 
                                       num_ident, use_this_gi);
        CBlastFormatUtil::GetScoreString(evalue, bits, evalue_buf, 
                                         bit_score_buf);
        sdl = new SDeflineInfo;

        x_FillDeflineAndId(handle, id, use_this_gi, sdl);
        sdl->sum_n = sum_n;
        sdl->bit_string = bit_score_buf;
        sdl->evalue_string = evalue_buf;
    } catch (CException& ){
        sdl = new SDeflineInfo;
        CBlastFormatUtil::GetAlnScores(aln, score, bits, evalue, sum_n,
                                       num_ident, use_this_gi);
        CBlastFormatUtil::GetScoreString(evalue, bits, evalue_buf, bit_score_buf);
        sdl->sum_n = sum_n;
        sdl->bit_string = bit_score_buf;
        sdl->evalue_string = evalue_buf;
        sdl->defline = "Unknown";
        sdl->is_new = false;
        sdl->was_checked = false;

        if(id.Which() == CSeq_id::e_Gi){
            sdl->gi = id.GetGi();
        } else {            
            sdl->id = &id;
            sdl->gi = 0;
        }
        if(m_Option & eHtml){
            string user_url= m_Reg->Get(m_BlastType, "TOOL_URL");
            CBioseq::TId ids;
            CRef<CSeq_id> id_ref;
            id_ref = &(const_cast<CSeq_id&>(id));
            ids.push_back(id_ref);
            sdl->id_url = s_GetIdUrl(ids, sdl->gi, user_url,
                                     m_IsDbNa, m_Database, 
                                     (m_Option & eNewTargetWindow) ? 
                                     true : false, m_Rid, m_QueryNumber);
            sdl->score_url = NcbiEmptyString;
        }
    }
    
    return sdl;
}

END_NCBI_SCOPE
/*===========================================
*$Log$
*Revision 1.12  2005/05/13 14:21:37  jianye
*No showing internal blastdb id
*
*Revision 1.11  2005/04/12 16:40:51  jianye
*Added gnl id to url
*
*Revision 1.10  2005/02/23 16:28:03  jianye
*change due to num_ident addition
*
*Revision 1.9  2005/02/15 17:47:29  grichenk
*Replaces CRef<CSeq_id> with CConstRef<CSeq_id>
*
*Revision 1.8  2005/02/15 15:49:03  jianye
*fix compiler warning
*
*Revision 1.7  2005/02/14 15:42:47  jianye
*fixed compiler warning
*
*Revision 1.6  2005/02/09 17:34:37  jianye
*move common url to blastfmtutil.hpp
*
*Revision 1.5  2005/02/02 16:33:18  jianye
*int to size_t to get rid of compiler warning
*
*Revision 1.4  2005/01/31 19:47:31  jianye
*correct type error
*
*Revision 1.3  2005/01/31 17:42:37  jianye
*change unsigned int to size_t
*
*Revision 1.2  2005/01/25 22:14:10  ucko
*#include <stdio.h> due to use of sprintf()
*
*Revision 1.1  2005/01/25 15:38:12  jianye
*Initial check in
*

*===========================================
*/

