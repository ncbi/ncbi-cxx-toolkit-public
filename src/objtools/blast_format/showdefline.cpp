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
#include <html/htmlhelper.hpp>


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
                         string& rid, int query_number, int taxid)
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
            sprintf(url_buf, kEntrezUrl.c_str(), "", db, gi, dopt,
                    open_new_window ? "TARGET=\"EntrezView\"" : "");
            url_link = url_buf;
        } else {//seqid general, dbtag specified
            if(wid->Which() == CSeq_id::e_General){
                const CDbtag& dtg = wid->GetGeneral();
                const string& dbname = dtg.GetDb();
                if(NStr::CompareNocase(dbname, "TI") == 0){
                    string actual_id;
                    wid->GetLabel(&actual_id, CSeq_id::eContent);
                    sprintf(url_buf, kTraceUrl.c_str(), "", actual_id.c_str());
                    url_link = url_buf;
                }
            }
        }
    } else { //need to use user_url 

        string url_with_parameters = CBlastFormatUtil::BuildUserUrl(ids, taxid, user_url,
                                                                    db_name,
                                                                    is_db_na, rid,
                                                                    query_number);
        if (url_with_parameters != NcbiEmptyString) {
            url_link += "<a href=\"";
            url_link += url_with_parameters;
            url_link += "\">";
        }
    }
    return url_link;
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
        sprintf(buf, kUnigeneUrl.c_str(), is_na ? "nucleotide" : "protein", 
                is_na ? "nucleotide" : "protein", gi);
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

string 
CShowBlastDefline::GetSeqIdListString(const list<CRef<objects::CSeq_id> >& id,
                                      bool show_gi)
{
    string id_str = NcbiEmptyString;

    ITERATE(list<CRef<CSeq_id> >, itr, id) {
        string id_token;
        // For local ids, make sure the "lcl|" part is not printed
        if ((*itr)->IsLocal())
            (*itr)->GetLabel(&id_token, CSeq_id::eContent, 0);
        else if (show_gi || !(*itr)->IsGi())
            id_token = (*itr)->AsFastaString();
        if (id_token != NcbiEmptyString)
            id_str += id_token + "|";
    }
    if (id_str.size() > 0)
        id_str.erase(id_str.size() - 1);

    return id_str;
}

void 
CShowBlastDefline::GetSeqIdList(const objects::CBioseq_Handle& bh,
                                list<CRef<objects::CSeq_id> >& ids)
{
    ids.clear();

    // Check for ids of type "gnl|BL_ORD_ID". These are the artificial ids
    // created in a BLAST database when it is formatted without indexing.
    // For such ids, create new fake local Seq-ids, saving the first token of 
    // the Bioseq's title, if it's available.
    ITERATE(CBioseq_Handle::TId, itr, bh.GetId()) {
        CRef<CSeq_id> next_seqid(new CSeq_id());
        string id_token = NcbiEmptyString;
        
        if (next_seqid->IsGeneral() &&
            next_seqid->AsFastaString().find("gnl|BL_ORD_ID") 
            != string::npos) {
            vector<string> title_tokens;
            id_token = 
                NStr::Tokenize(sequence::GetTitle(bh), " ", title_tokens)[0];
        }
        if (id_token != NcbiEmptyString) {
            // Create a new local id with a label containing the extracted
            // token and save it in the next_seqid instead of the original 
            // id.
            CObject_id* obj_id = new CObject_id();
            obj_id->SetStr(id_token);
            next_seqid->SetLocal(*obj_id);
        } else {
                next_seqid->Assign(*itr->GetSeqId());
        }
        ids.push_back(next_seqid);
    }
}

void
CShowBlastDefline::GetBioseqHandleDeflineAndId(const CBioseq_Handle& handle,
                                               list<int>& use_this_gi,
                                               string& seqid, string& defline, 
                                               bool show_gi)
{
    // Retrieve the CBlast_def_line_set object and save in a CRef, preventing
    // its destruction; then extract the list of CBlast_def_line objects.
    const CRef<CBlast_def_line_set> bdlRef = 
        CBlastFormatUtil::GetBlastDefline(handle);
    const list< CRef< CBlast_def_line > >& bdl = bdlRef->Get();

    if (bdl.empty()){
        list<CRef<objects::CSeq_id> > ids;
        GetSeqIdList(handle, ids);
        seqid = GetSeqIdListString(ids, show_gi);
        defline = GetTitle(handle);
    } else { 
        bool is_first = true;
        ITERATE(list<CRef<CBlast_def_line> >, iter, bdl) {
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
                if (is_first)
                    seqid = GetSeqIdListString(cur_id, show_gi);

                if((*iter)->IsSetTitle()){
                    if(is_first){
                        defline = (*iter)->GetTitle();
                    } else {
                        string concat_acc;
                        CConstRef<CSeq_id> wid = 
                            FindBestChoice(cur_id, CSeq_id::WorstRank);
                        wid->GetLabel(&concat_acc, CSeq_id::eFasta, 0);
                        if( show_gi && cur_gi > 0){
                            defline = defline + " >" + "gi|" + 
                                NStr::IntToString(cur_gi) + "|" + 
                                concat_acc + " " + (*iter)->GetTitle();
                        } else {
                            defline = defline + " >" + concat_acc + " " + 
                                (*iter)->GetTitle();
                        }
                    }
                    is_first = false;
                }
            }
        }
    }
}

void CShowBlastDefline::x_FillDeflineAndId(const CBioseq_Handle& handle,
                                           const CSeq_id& aln_id,
                                           list<int>& use_this_gi,
                                           SDeflineInfo* sdl)
{
    const CRef<CBlast_def_line_set> bdlRef = CBlastFormatUtil::GetBlastDefline(handle);
    const list< CRef< CBlast_def_line > >& bdl = bdlRef->Get();
    const CBioseq::TId* ids = &handle.GetBioseqCore()->GetId();
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
        int seq_status = 0;
        
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
    if(bdl.empty()){
        wid = FindBestChoice(*ids, CSeq_id::WorstRank);
        sdl->id = wid;
        sdl->gi = FindGi(*ids);    
    } else {        
        bool found = false;
        for(list< CRef< CBlast_def_line > >::const_iterator iter = bdl.begin();
            iter != bdl.end(); iter++){
            const CBioseq::TId* cur_id = &((*iter)->GetSeqid());
            int cur_gi =  FindGi(*cur_id);
            wid = FindBestChoice(*cur_id, CSeq_id::WorstRank);
            if (!use_this_gi.empty()) {
                ITERATE(list<int>, iter_gi, use_this_gi){
                    if(cur_gi == *iter_gi){
                        found = true;
                        break;
                    }
                }
            } else {
                ITERATE(CBioseq::TId, iter_id, *cur_id) {
                    if ((*iter_id)->Match(aln_id)) {
                        found = true;
                    }
                }
            }
            if(found){
                sdl->id = wid;
                sdl->gi = cur_gi;
                ids = cur_id;
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
					sdl->linkout = CBlastFormatUtil::GetLinkout((**iter));
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
                        sdl->linkout = CBlastFormatUtil::GetLinkout((**iter));
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

        string user_url = m_Reg->Get(m_BlastType, "TOOL_URL");
        string type_temp = m_BlastType;
        type_temp = NStr::TruncateSpaces(NStr::ToLower(type_temp));
        int taxid = 0;
        if (type_temp == "mapview" || type_temp == "mapview_prev" || 
            type_temp == "gsfasta") {
            taxid = 
                CBlastFormatUtil::GetTaxidForSeqid(aln_id, *m_ScopeRef);
        }
           
        sdl->id_url = s_GetIdUrl(*ids, sdl->gi, user_url,
                                 m_IsDbNa, m_Database, 
                                 (m_Option & eNewTargetWindow) ? true : false,
                                 m_Rid, m_QueryNumber, taxid);
    }

  
    sdl->defline = GetTitle(m_ScopeRef->GetBioseqHandle(*(sdl->id)));
    if (!(bdl.empty())) { 
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
                    bool id_used_already = false;
                    ITERATE(CBioseq::TId, iter_id, cur_id) {
                        if ((*iter_id)->Match(*(sdl->id))) {
                            id_used_already = true;
                            break;
                        }
                    }
                    if (!id_used_already) {
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
    CSeq_align_set actual_aln_list;
    CBlastFormatUtil::ExtractSeqalignSetFromDiscSegs(actual_aln_list, 
                                                     *m_AlnSetRef);

    for (CSeq_align_set::Tdata::const_iterator 
             iter = actual_aln_list.Get().begin(); 
         iter != actual_aln_list.Get().end() && num_align < m_NumToShow; 
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
        if (m_Option & eHtml) {
            out << CHTMLHelper::HTMLEncode(actual_line_component);
        } else {
            out << actual_line_component; 
        }
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
    string evalue_buf, bit_score_buf, total_bit_score_buf;
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
        CBlastFormatUtil::GetScoreString(evalue, bits, 0, evalue_buf, 
                                         bit_score_buf, total_bit_score_buf);
        sdl = new SDeflineInfo;

        x_FillDeflineAndId(handle, id, use_this_gi, sdl);
        sdl->sum_n = sum_n == -1 ? 1:sum_n ;
        sdl->bit_string = bit_score_buf;
        sdl->evalue_string = evalue_buf;
    } catch (CException& ){
        sdl = new SDeflineInfo;
        CBlastFormatUtil::GetAlnScores(aln, score, bits, evalue, sum_n,
                                       num_ident, use_this_gi);
        CBlastFormatUtil::GetScoreString(evalue, bits, 0, evalue_buf, 
                                         bit_score_buf, total_bit_score_buf);
        sdl->sum_n = sum_n == -1 ? 1:sum_n;
        sdl->bit_string = bit_score_buf;
        sdl->evalue_string = evalue_buf;
        sdl->defline = "Unknown";
        sdl->is_new = false;
        sdl->was_checked = false;
        sdl->linkout = 0;
        
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
                                     true : false, m_Rid, m_QueryNumber, 0);
            sdl->score_url = NcbiEmptyString;
        }
    }
    
    return sdl;
}

END_NCBI_SCOPE
/*===========================================
*$Log$
*Revision 1.32  2006/09/19 16:36:01  jianye
*handles disc alignment
*
*Revision 1.31  2006/08/24 16:17:51  jianye
*make id consistent in use_this_gi case
*
*Revision 1.30  2006/07/26 18:14:07  jianye
*fix unigene url
*
*Revision 1.29  2006/07/17 14:52:49  jianye
*consolidate custom url functions
*
*Revision 1.28  2006/05/15 16:19:05  zaretska
*Moved s_GetLinkout() function from shodefline.cpp to blastfmtutil.cpp
*
*Revision 1.27  2006/05/12 17:33:15  jianye
*encode html string
*
*Revision 1.26  2006/05/08 15:31:15  jianye
* delete the space in front of database name in url
*
*Revision 1.25  2006/04/05 17:40:06  jianye
*adjust to new defs
*
*Revision 1.24  2006/03/15 16:46:55  jianye
*use title from seqalign bioseq as the first one
*
*Revision 1.23  2006/03/14 22:21:55  jianye
*use only seqids in seqalign
*
*Revision 1.22  2006/03/08 19:01:56  jianye
*added mapview_prev url link
*
*Revision 1.21  2005/08/11 15:30:26  jianye
*add taxid to user url
*
*Revision 1.20  2005/08/01 15:03:27  dondosha
*For local seqids, do not print the lcl| part
*
*Revision 1.19  2005/07/26 14:19:40  dondosha
*Fix for Solaris compiler warning
*
*Revision 1.18  2005/07/26 13:59:15  dondosha
*Fix for MSVC compiler warning
*
*Revision 1.17  2005/07/21 16:55:55  jianye
*fix showing sum_n
*
*Revision 1.16  2005/07/21 16:27:09  dondosha
*Added 3 static functions for extraction of defline parts; fixed bug when order of redundant sequences in a Blast-def-line set changes
*
*Revision 1.15  2005/07/20 18:18:43  dondosha
*Uninitialized variable compiler warning fix
*
*Revision 1.14  2005/07/05 16:33:02  jianye
*initialize linkout to 0 for unknown bisoeq
*
*Revision 1.13  2005/06/03 16:58:34  lavr
*Explicit (unsigned char) casts in ctype routines
*
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

