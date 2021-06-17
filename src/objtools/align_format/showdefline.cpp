/* $Id$
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

#include <objtools/align_format/showdefline.hpp>
#include <objtools/align_format/align_format_util.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>    // for CSeqDB::ExtractBlastDefline

#include <stdio.h>
#include <html/htmlhelper.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
USING_SCOPE(sequence);
BEGIN_SCOPE(align_format)

//margins constant

static string kOneSpaceMargin = " ";
static string kTwoSpaceMargin = "  ";

//const strings
static const string kHeader = "Sequences producing significant alignments:";
static const string kScore = "Score";
static const string kE = "E";
static const string kBits =
    (getenv("CTOOLKIT_COMPATIBLE") ? "(bits)" : "(Bits)");
static const string kEvalue = "E value";
static const string kValue = "Value";
static const string kN = "N";
static const string kRepeatHeader = "Sequences used in model and found again:";
static const string kNewSeqHeader = "Sequences not found previously or not pr\
eviously below threshold:";
static const string kMaxScore = "Max score";
static const string kTotalScore = "Total score";
static const string kTotal = "Total";
static const string kIdentity = "Max ident";
static const string kPercent = "Percent";
static const string kHighest = "Highest";
static const string kQuery = "Query";
static const string kCoverage = "Query coverage";
static const string kEllipsis = "...";

static const string kMax = "Max";
static const string kIdentLine2  = "Ident";
static const string kScoreLine2 = "Score";
static const string kQueryCov = "Query";
static const string kQueryCovLine2 = "cover";
static const string kPerc = "Per.";
static const string kAccession = "Accession";
static const string kDescription = "Description";
static const string kScientific = "Scientific";
static const string kCommon = "Common";
static const string kName = "Name";
static const string kAccAbbr = "Acc.";
static const string kLenAbbr = "Len";
static const string kTaxid = "Taxid";

//psiblast related
static const string kPsiblastNewSeqGif = "<IMG SRC=\"images/new.gif\" \
WIDTH=30 HEIGHT=15 ALT=\"New sequence mark\">";

static const string kPsiblastNewSeqBackgroundGif = "<IMG SRC=\"images/\
bg.gif\" WIDTH=30 HEIGHT=15 ALT=\" \">";

static const string kPsiblastCheckedBackgroundGif = "<IMG SRC=\"images\
/bg.gif\" WIDTH=15 HEIGHT=15 ALT=\" \">";

static const string kPsiblastCheckedGif = "<IMG SRC=\"images/checked.g\
if\" WIDTH=15 HEIGHT=15 ALT=\"Checked mark\">";

static const string kPsiblastEvalueLink = "<a name = Evalue></a>";

static const string  kPsiblastCheckboxChecked = "<INPUT TYPE=\"checkbox\" NAME\
=\"checked_GI\" VALUE=\"%d\" CHECKED>  <INPUT TYPE=\"hidden\" NAME =\"good_G\
I\" VALUE = \"%d\">";

static const string  kPsiblastCheckbox =  "<INPUT TYPE=\"checkbox\" NAME=\"ch\
ecked_GI\" VALUE=\"%d\">  ";

//Max length of title string for the the link
static const int kMaxDescrLength = 4096;

string
CShowBlastDefline::GetSeqIdListString(const list<CRef<objects::CSeq_id> >& id,
                                      bool show_gi)
{
    string id_string = NcbiEmptyString;
    bool found_gi = false;

    CRef<CSeq_id> best_id = FindBestChoice(id, CSeq_id::Score);

    if (show_gi) {
        ITERATE(list<CRef<CSeq_id> >, itr, id) {
             if ((*itr)->IsGi()) {
                id_string += (*itr)->AsFastaString();
                found_gi = true;
                break;
             }
        }
    }

    if (best_id.NotEmpty()  &&  !best_id->IsGi() ) {
         if (found_gi)
             id_string += "|";

         if (best_id->IsLocal()) {
             string id_token;
             best_id->GetLabel(&id_token, CSeq_id::eContent, 0);
             id_string += id_token;
         }
         else
             id_string += best_id->AsFastaString();
    }

    return id_string;
}

void
CShowBlastDefline::GetSeqIdList(const objects::CBioseq_Handle& bh,
                                list<CRef<objects::CSeq_id> >& ids)
{
    ids.clear();


    vector< CConstRef<CSeq_id> > original_seqids;

    ITERATE(CBioseq_Handle::TId, itr, bh.GetId()) {
      original_seqids.push_back(itr->GetSeqId());
    }

    // Check for ids of type "gnl|BL_ORD_ID". These are the artificial ids
    // created in a BLAST database when it is formatted without indexing.
    // For such ids, create new fake local Seq-ids, saving the first token of
    // the Bioseq's title, if it's available.
    GetSeqIdList(bh,original_seqids,ids);
}



void
CShowBlastDefline::GetSeqIdList(const objects::CBioseq_Handle& bh,
                                vector< CConstRef<CSeq_id> > &original_seqids,
                                list<CRef<objects::CSeq_id> >& ids)
{
    ids.clear();
    ITERATE(vector< CConstRef<CSeq_id> >, itr, original_seqids) {
        CRef<CSeq_id> next_seqid(new CSeq_id());
        string id_token = NcbiEmptyString;

        if (((*itr)->IsGeneral() &&
            (*itr)->AsFastaString().find("gnl|BL_ORD_ID")
            != string::npos) ||
		(*itr)->AsFastaString().find("lcl|Subject_") != string::npos) {
            vector<string> title_tokens;
            string defline = sequence::CDeflineGenerator().GenerateDefline(bh);
            if (defline != NcbiEmptyString) {
                id_token =
                    NStr::Split(defline, " ", title_tokens)[0];
            }
        }
        if (id_token != NcbiEmptyString) {
            // Create a new local id with a label containing the extracted
            // token and save it in the next_seqid instead of the original
            // id.
            CObject_id* obj_id = new CObject_id();
            obj_id->SetStr(id_token);
            next_seqid->SetLocal(*obj_id);
        } else {
            next_seqid->Assign(**itr);
        }
        ids.push_back(next_seqid);
    }
}

void
CShowBlastDefline::GetBioseqHandleDeflineAndId(const CBioseq_Handle& handle,
                                               list<TGi>& use_this_gi,
                                               string& seqid, string& defline,
                                               bool show_gi /* = true */,
                                               TGi this_gi_first /* = -1 */)
{
    // Retrieve the CBlast_def_line_set object and save in a CRef, preventing
    // its destruction; then extract the list of CBlast_def_line objects.
    if( !handle ) return; //  No bioseq for this handle ( deleted accession ? )
    CRef<CBlast_def_line_set> bdlRef =
        CSeqDB::ExtractBlastDefline(handle);

    if(bdlRef.Empty()){
        list<CRef<objects::CSeq_id> > ids;
        GetSeqIdList(handle, ids);
        seqid = GetSeqIdListString(ids, show_gi);
        defline = sequence::CDeflineGenerator().GenerateDefline(handle);
    } else {
        bdlRef->PutTargetGiFirst(this_gi_first);
        const list< CRef< CBlast_def_line > >& bdl = bdlRef->Get();
        bool is_first = true;
        ITERATE(list<CRef<CBlast_def_line> >, iter, bdl) {
            const CBioseq::TId& cur_id = (*iter)->GetSeqid();
            TGi cur_gi =  FindGi(cur_id);
            TGi gi_in_use_this_gi = ZERO_GI;
            ITERATE(list<TGi>, iter_gi, use_this_gi){
                if(cur_gi == *iter_gi){
                    gi_in_use_this_gi = *iter_gi;
                    break;
                }
            }
            if(use_this_gi.empty() || gi_in_use_this_gi > ZERO_GI) {
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
                        if( show_gi && cur_gi > ZERO_GI){
                            defline = defline + " >" + "gi|" +
                                NStr::NumericToString(cur_gi) + "|" +
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

static void s_LimitDescrLength(string &descr, size_t maxDescrLength = kMaxDescrLength)
{
	if(descr.length() > maxDescrLength) {
        descr = descr.substr(0,maxDescrLength);
        size_t end = NStr::Find(descr," ",NStr::eNocase,NStr::eReverseSearch);

        if(end != NPOS) {
            descr = descr.substr(0,end);
            descr += "...";
        }
    }

}

void CShowBlastDefline::x_InitLinkOutInfo(SDeflineInfo* sdl,
                                            CBioseq::TId& cur_id,
                                            int blast_rank,
                                            bool getIdentProteins)
{

    bool is_mixed_database = (m_IsDbNa == true && m_Ctx)? CAlignFormatUtil::IsMixedDatabase(*m_Ctx): false;
    if (m_DeflineTemplates && m_DeflineTemplates->advancedView && !is_mixed_database)  return;


    string linkout_list;

    sdl->linkout = CAlignFormatUtil::GetSeqLinkoutInfo(cur_id,
                                    &m_LinkoutDB,
                                    m_MapViewerBuildName,
                                    sdl->gi);
    if(!m_LinkoutDB) {
        m_Option &= ~eLinkout;
        return;
    }


    if(m_LinkoutOrder.empty()) {
        m_ConfigFile.reset(new CNcbiIfstream(".ncbirc"));
        m_Reg.reset(new CNcbiRegistry(*m_ConfigFile));
        if(!m_BlastType.empty()) m_LinkoutOrder = m_Reg->Get(m_BlastType,"LINKOUT_ORDER");
        m_LinkoutOrder = (!m_LinkoutOrder.empty()) ? m_LinkoutOrder : kLinkoutOrderStr;
    }
    if (m_DeflineTemplates == NULL || !m_DeflineTemplates->advancedView) {
        if(m_Option & eRealtedInfoLinks){
            string user_url = m_Reg.get() ? m_Reg->Get(m_BlastType, "TOOL_URL") : kEmptyStr;
            sdl->linkout_list =  CAlignFormatUtil::GetFullLinkoutUrl(cur_id,
                                                            m_Rid,
                                                            m_CddRid,
                                                            m_EntrezTerm,
                                                            m_IsDbNa,
                                                            false,
                                                            true,
                                                            blast_rank,
                                                            m_LinkoutOrder,
                                                            sdl->taxid,
                                                            m_Database,
                                                            m_QueryNumber,
                                                            user_url,
                                                            m_PreComputedResID,
                                                            m_LinkoutDB,
                                                            m_MapViewerBuildName,
                                                            getIdentProteins);
        }
        else {
             sdl->linkout_list =  CAlignFormatUtil::GetLinkoutUrl(sdl->linkout,
                                                         cur_id,
                                                         m_Rid,
                                                         m_CddRid,
                                                         m_EntrezTerm,
                                                         m_IsDbNa,
                                                         ZERO_GI,
                                                         true,
                                                         false,
                                                         blast_rank,
                                                         m_PreComputedResID);
        }
    }
}


void CShowBlastDefline::x_FillDeflineAndId(const CBioseq_Handle& handle,
                                           const CSeq_id& aln_id,
                                           list<string> &use_this_seqid,
                                           SDeflineInfo* sdl,
                                           int blast_rank)
{
    if( !handle ) return; // invalid handle.

    const CRef<CBlast_def_line_set> bdlRef = CSeqDB::ExtractBlastDefline(handle);
    const list< CRef< CBlast_def_line > > &bdl = (bdlRef.Empty()) ? list< CRef< CBlast_def_line > >() : bdlRef->Get();

    CRef<CSeq_id> wid;
    sdl->defline = NcbiEmptyString;

    sdl->gi = ZERO_GI;
    sdl->id_url = NcbiEmptyString;
    sdl->score_url = NcbiEmptyString;
    sdl->linkout = 0;
    sdl->is_new = false;
    sdl->was_checked = false;
    sdl->taxid = ZERO_TAX_ID;
    //get psiblast stuff

    if(m_SeqStatus){
        string aln_id_str;
        aln_id.GetLabel(&aln_id_str, CSeq_id::eContent);
        PsiblastSeqStatus seq_status = eUnknown;

        TIdString2SeqStatus::const_iterator itr = m_SeqStatus->find(aln_id_str);
        if ( itr != m_SeqStatus->end() ){
            seq_status = itr->second;
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
    //get id (sdl->id, sdl-gi)
    sdl->id = CAlignFormatUtil::GetDisplayIds(handle,aln_id,use_this_seqid,&sdl->gi,&sdl->taxid,&sdl->textSeqID);
    sdl->alnIDFasta = aln_id.AsFastaString();

    //get linkout****
    if((m_Option & eLinkout)){
        bool getIdentProteins = !m_IsDbNa && bdl.size() > 1;
        for(list< CRef< CBlast_def_line > >::const_iterator iter = bdl.begin();
            iter != bdl.end(); iter++){
            CBioseq::TId& cur_id = (CBioseq::TId &)(*iter)->GetSeqid();
            TGi cur_gi =  FindGi(cur_id);
            bool match = false;
            if(!use_this_seqid.empty()){
                wid = FindBestChoice(cur_id, CSeq_id::WorstRank);
                match = CAlignFormatUtil::MatchSeqInSeqList(cur_gi, wid, use_this_seqid);
            }
            if((use_this_seqid.empty() && sdl->gi == cur_gi) || match) {
                x_InitLinkOutInfo(sdl,cur_id,blast_rank,getIdentProteins); //only initialized if !(m_DeflineTemplates->advancedView && !is_mixed_database)
                break;
            }
        }
        //The following is the case when for whatever reason the seq is not found in Blast database (bdl list is empty) and is retrived from genbank
        if(bdl.size()  == 0) {         
            CBioseq::TId& cur_id = (CBioseq::TId &) handle.GetBioseqCore()->GetId();            
            x_InitLinkOutInfo(sdl,cur_id,blast_rank,getIdentProteins); //only initialized if !(m_DeflineTemplates->advancedView && !is_mixed_database)
        }        
    }

    //get score and id url
    if(m_Option & (eHtml | eShowCSVDescr)){
        bool useTemplates = m_DeflineTemplates != NULL;
        bool advancedView = (m_DeflineTemplates != NULL) ? m_DeflineTemplates->advancedView : false;
        string accession;
        sdl->id->GetLabel(&accession, CSeq_id::eContent);
        sdl->score_url = !useTemplates ? "<a href=#" : "";
        if (!useTemplates && m_PositionIndex >= 0) {
            sdl->score_url += "_" + NStr::IntToString(m_PositionIndex) + "_";
        }
        sdl->score_url += sdl->gi == ZERO_GI ? accession :
            NStr::NumericToString(sdl->gi);
        sdl->score_url += !useTemplates ? ">" : "";

		string user_url = m_Reg.get() ? m_Reg->Get(m_BlastType, "TOOL_URL") : kEmptyStr;
		//blast_rank = num_align + 1
		CRange<TSeqPos> seqRange = ((int)m_ScoreList.size() >= blast_rank)? m_ScoreList[blast_rank - 1]->subjRange : CRange<TSeqPos>(0,0);
        bool flip = ((int)m_ScoreList.size() >= blast_rank) ? m_ScoreList[blast_rank - 1]->flip : false;
        CAlignFormatUtil::SSeqURLInfo seqUrlInfo(user_url,m_BlastType,m_IsDbNa,m_Database,m_Rid,
                                                 m_QueryNumber,sdl->gi, accession, 0, //linkout = 0, not used any more
                                                 blast_rank,false,(m_Option & eNewTargetWindow) ? true : false,seqRange,flip);
        seqUrlInfo.resourcesUrl = m_Reg.get() ? m_Reg->Get(m_BlastType, "RESOURCE_URL") : kEmptyStr;
        seqUrlInfo.useTemplates = useTemplates;
        seqUrlInfo.advancedView = advancedView;

        if(sdl->id->Which() == CSeq_id::e_Local && (m_Option & eHtml)){
            //get taxid info for local blast db such as igblast db
            ITERATE(list<CRef<CBlast_def_line> >, iter_bdl, bdl) {
                if ((*iter_bdl)->IsSetTaxid() && (*iter_bdl)->CanGetTaxid()){
                    seqUrlInfo.taxid = (*iter_bdl)->GetTaxid();
                    break;
                }
            }
        }
        sdl->id_url = CAlignFormatUtil::GetIDUrl(&seqUrlInfo,aln_id,*m_ScopeRef);
    }

    //get defline
    sdl->defline = CDeflineGenerator().GenerateDefline(m_ScopeRef->GetBioseqHandle(*(sdl->id)));
	sdl->fullDefline = sdl->defline;
    if (!(bdl.empty())) {
        for(list< CRef< CBlast_def_line > >::const_iterator iter = bdl.begin();
            iter != bdl.end(); iter++){
            const CBioseq::TId& cur_id = (*iter)->GetSeqid();
            TGi cur_gi =  FindGi(cur_id);
            wid = FindBestChoice(cur_id, CSeq_id::WorstRank);
            bool match = CAlignFormatUtil::MatchSeqInSeqList(cur_gi, wid, use_this_seqid);
            if(use_this_seqid.empty() || match) {

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
                        if( (m_Option & eShowGi) && cur_gi > ZERO_GI){
                            sdl->fullDefline =  sdl->fullDefline + " >" + "gi|" +
                                NStr::NumericToString(cur_gi) + "|" +
                                concat_acc + " " + (*iter)->GetTitle();
                        } else {
                            sdl->fullDefline = sdl->fullDefline + " >" + concat_acc +
                                " " +
                                (*iter)->GetTitle();
                        }
                        if(sdl->fullDefline.length() > kMaxDescrLength) {
                            break;
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
                                     size_t num_defline_to_show,
                                     bool translated_nuc_alignment,
                                     CRange<TSeqPos>* master_range):

    m_AlnSetRef(&seqalign),
    m_ScopeRef(&scope),
    m_LineLen(line_length),
    m_NumToShow(num_defline_to_show),
    m_TranslatedNucAlignment(translated_nuc_alignment),
    m_SkipFrom(-1),
    m_SkipTo(-1),
    m_MasterRange(master_range),
    m_LinkoutDB(NULL)
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
    m_Ctx = NULL;
    m_StructureLinkout = false;
    if(m_MasterRange) {
        if(m_MasterRange->GetFrom() >= m_MasterRange->GetTo()) {
            m_MasterRange = NULL;
        }
    }
    m_DeflineTemplates = NULL;
    m_StartIndex = 0;
    m_PositionIndex = -1;
    m_AppLogInfo = NULL;
}

CShowBlastDefline::~CShowBlastDefline()
{
    ITERATE(vector<SScoreInfo*>, iter, m_ScoreList){
        delete *iter;
    }

    ITERATE(vector<SDeflineFormattingInfo*>, iter, m_SdlFormatInfoVec){
        delete *iter;
    }

}


void CShowBlastDefline::Init(void)
{
	if (m_DeflineTemplates != NULL) {
        x_InitDeflineTable();
    }
    else {
        x_InitDefline();
    }
}


void CShowBlastDefline::Display(CNcbiOstream & out)
{
    if (m_DeflineTemplates != NULL) {
        if(m_Option & eHtml) {//text        
            x_DisplayDeflineTableTemplate(out);
        }
        else if(m_Option & eShowCSVDescr) {
            x_DisplayDeflineTableTemplateCSV(out);
        }
        else {//text        
            x_DisplayDeflineTableTemplateText(out);
        }
    }
    else {
        x_DisplayDefline(out);
    }
}

bool CShowBlastDefline::x_CheckForStructureLink()
{
      bool struct_linkout = false;
      int count = 0;
      const int k_CountMax = 200; // Max sequences to check.

      ITERATE(vector<SScoreInfo*>, iter, m_ScoreList) {
          const CBioseq_Handle& handle = m_ScopeRef->GetBioseqHandle(*(*iter)->id);
	  if( !handle ) continue;  // invalid handle.
          const CRef<CBlast_def_line_set> bdlRef = CSeqDB::ExtractBlastDefline(handle);
          const list< CRef< CBlast_def_line > > &bdl = (bdlRef.Empty()) ? list< CRef< CBlast_def_line > >() : bdlRef->Get();
          for(list< CRef< CBlast_def_line > >::const_iterator bdl_iter = bdl.begin();
              bdl_iter != bdl.end() && struct_linkout == false; bdl_iter++){
              if ((*bdl_iter)->IsSetLinks())
              {
                 for (list< int >::const_iterator link_iter = (*bdl_iter)->GetLinks().begin();
                      link_iter != (*bdl_iter)->GetLinks().end(); link_iter++)
                 {
                      if (*link_iter & eStructure) {
                         struct_linkout = true;
                         break;
                      }
                 }
              }
          }
          if (struct_linkout == true || count > k_CountMax)
            break;
          count++;
      }
      return struct_linkout;
}

//size_t max_score_len = kBits.size(), max_evalue_len = kValue.size();
//size_t max_sum_n_len =1;
//size_t m_MaxScoreLen , m_MaxEvalueLen,m_MaxSumNLen;
//bool m_StructureLinkout
void CShowBlastDefline::x_InitDefline(void)
{
    /*Note we can't just show each alnment as we go because we will
      need to show defline only once for all hsp's with the same id*/

    bool is_first_aln = true;
    size_t num_align = 0;
    CConstRef<CSeq_id> previous_id, subid;

    m_MaxScoreLen = kBits.size();
    m_MaxEvalueLen = kValue.size();
    m_MaxSumNLen =1;

    m_MaxPercentIdentityLen = kIdentity.size();    
    m_MaxQueryCoverLen = kCoverage.size();
    m_MaxTotalScoreLen = kTotal.size();
    


    if(m_Option & eHtml){
        m_ConfigFile.reset(new CNcbiIfstream(".ncbirc"));
        m_Reg.reset(new CNcbiRegistry(*m_ConfigFile));
    }
    bool master_is_na = false;
    //prepare defline

    int ialn = 0;
    for (CSeq_align_set::Tdata::const_iterator
             iter = m_AlnSetRef->Get().begin();
         iter != m_AlnSetRef->Get().end() && num_align < m_NumToShow;
         iter++, ialn++){
        if (ialn < m_SkipTo && ialn >= m_SkipFrom) continue;

        if (is_first_aln) {
            _ASSERT(m_ScopeRef);
            CBioseq_Handle bh = m_ScopeRef->GetBioseqHandle((*iter)->GetSeq_id(0));
            _ASSERT(bh);
            master_is_na = bh.GetBioseqCore()->IsNa();
        }
        subid = &((*iter)->GetSeq_id(1));
        if(is_first_aln || (!is_first_aln && !subid->Match(*previous_id))) {
            SScoreInfo* sci = x_GetScoreInfo(**iter, num_align);            
            if(sci){
                m_ScoreList.push_back(sci);
                if(m_MaxScoreLen < sci->bit_string.size()){
                    m_MaxScoreLen = sci->bit_string.size();
                }
                if(m_MaxEvalueLen < sci->evalue_string.size()){
                    m_MaxEvalueLen = sci->evalue_string.size();
                }

                if(m_MaxTotalScoreLen < sci->total_bit_string.size()){
                    m_MaxTotalScoreLen = sci->total_bit_string.size();
                }
                int percent_identity = CAlignFormatUtil::GetPercentMatch(sci->match,sci->align_length);
                if(m_MaxPercentIdentityLen < NStr::IntToString(percent_identity).size()) {
                    m_MaxPercentIdentityLen = NStr::IntToString(percent_identity).size();
                }

                if( m_MaxSumNLen < NStr::IntToString(sci->sum_n).size()){
                    m_MaxSumNLen = NStr::IntToString(sci->sum_n).size();
                }
            }
            num_align++;
        }
        is_first_aln = false;
        previous_id = subid;

    }


    if((m_Option & eLinkout) && (m_Option & eHtml) && !m_IsDbNa && !master_is_na)
        m_StructureLinkout = x_CheckForStructureLink();
}



void CShowBlastDefline::x_DisplayDefline(CNcbiOstream & out)
{
    bool use_long_seqids = (m_Option & eLongSeqId) != 0;

    if(!(m_Option & eNoShowHeader)) {
        if((m_PsiblastStatus == eFirstPass) ||
           (m_PsiblastStatus == eRepeatPass)){
            CAlignFormatUtil::AddSpace(out, m_LineLen + kTwoSpaceMargin.size());
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
            CAlignFormatUtil::AddSpace(out, m_MaxScoreLen - kScore.size());
            CAlignFormatUtil::AddSpace(out, kTwoSpaceMargin.size());
            
            
            if (m_Option & eShowTotalScore) {
                out << kTotal;
                CAlignFormatUtil::AddSpace(out, m_MaxTotalScoreLen - kTotal.size());
                CAlignFormatUtil::AddSpace(out, kTwoSpaceMargin.size());
            }
            if (m_Option & eShowQueryCoverage) {
                out << kQueryCov;                
                CAlignFormatUtil::AddSpace(out, kTwoSpaceMargin.size());
            }

            CAlignFormatUtil::AddSpace(out, 2); //E align to l of value
            out << kE;         

            if (m_Option & eShowPercentIdent) {                
                CAlignFormatUtil::AddSpace(out, m_MaxEvalueLen - kValue.size()); 
                CAlignFormatUtil::AddSpace(out, kTwoSpaceMargin.size());
                CAlignFormatUtil::AddSpace(out, kTwoSpaceMargin.size());
                CAlignFormatUtil::AddSpace(out, kOneSpaceMargin.size());
                out << kMax;//"Max" - "ident" -second line                
            }

            out << "\n";
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
            CAlignFormatUtil::AddSpace(out, m_LineLen - kHeader.size());
            CAlignFormatUtil::AddSpace(out, kOneSpaceMargin.size());
            out << kBits;
            //in case m_MaxScoreLen > kBits.size()
            CAlignFormatUtil::AddSpace(out, m_MaxScoreLen - kBits.size());
            CAlignFormatUtil::AddSpace(out, kTwoSpaceMargin.size());

            
            if (m_Option & eShowTotalScore) {
                CAlignFormatUtil::AddSpace(out, kOneSpaceMargin.size());
                out << kScoreLine2;//"score"
                CAlignFormatUtil::AddSpace(out, m_MaxTotalScoreLen - kTotal.size()); 
                CAlignFormatUtil::AddSpace(out, kTwoSpaceMargin.size());
            }
            if (m_Option & eShowQueryCoverage) {
                out << kQueryCovLine2;//"cov"                
                CAlignFormatUtil::AddSpace(out, kTwoSpaceMargin.size());
                CAlignFormatUtil::AddSpace(out, kOneSpaceMargin.size());
            }

            out << kValue;
            if((m_Option & eShowSumN) || (m_Option & eShowPercentIdent)){
                CAlignFormatUtil::AddSpace(out, m_MaxEvalueLen - kValue.size()); 
                CAlignFormatUtil::AddSpace(out, kTwoSpaceMargin.size());                
            }
            if(m_Option & eShowSumN){
                out << kN;
            }
            if (m_Option & eShowPercentIdent) {
                out << kIdentLine2;//"ident"
            }
            out << "\n";
        }
        if(m_PsiblastStatus == eRepeatPass){
            out << kRepeatHeader << "\n";
        }
        if(m_PsiblastStatus == eNewPass){
            out << kNewSeqHeader << "\n";
        }
        out << "\n";
    }

    bool first_new =true;
    ITERATE(vector<SScoreInfo*>, iter, m_ScoreList){
        SDeflineInfo* sdl = x_GetDeflineInfo((*iter)->id, (*iter)->use_this_seqid, (*iter)->blast_rank);
        size_t line_length = 0;
        string line_component;
        if ((m_Option & eHtml) && (sdl->gi > ZERO_GI)){
            if((m_Option & eShowNewSeqGif)) {
                if (sdl->is_new) {
                    if (first_new) {
                        first_new = false;
                        out << kPsiblastEvalueLink;
                    }
                    out << kPsiblastNewSeqGif;

                } else {
                    out << kPsiblastNewSeqBackgroundGif;
                }
                if (sdl->was_checked) {
                    out << kPsiblastCheckedGif;

                } else {
                    out << kPsiblastCheckedBackgroundGif;
                }
            }
            char buf[256];
            if((m_Option & eCheckboxChecked)){
                sprintf(buf, kPsiblastCheckboxChecked.c_str(), sdl->gi,
                        sdl->gi);
                out << buf;
            } else if (m_Option & eCheckbox) {
                sprintf(buf, kPsiblastCheckbox.c_str(), sdl->gi);
                out << buf;
            }
        }


        if((m_Option & eHtml) && (sdl->id_url != NcbiEmptyString)) {
            out << sdl->id_url;
        }
        if(m_Option & eShowGi){
            if(sdl->gi > ZERO_GI){
                line_component = "gi|" + NStr::NumericToString(sdl->gi) + "|";
                out << line_component;
                line_length += line_component.size();
            }
        }
        if(!sdl->id.Empty()){
            if(!(sdl->id->AsFastaString().find("gnl|BL_ORD_ID") != string::npos ||
		sdl->id->AsFastaString().find("lcl|Subject_") != string::npos)){
                string idStr;
                if (use_long_seqids || ((m_Option & eShowGi) && !sdl->id->IsGi())) {
                    idStr = sdl->id->AsFastaString();
                }
                else {
                    idStr = CAlignFormatUtil::GetBareId(*sdl->id) + " ";
                }
            	if (strncmp(idStr.c_str(), "lcl|", 4) == 0) {
            		idStr = sdl->id->AsFastaString().substr(4);
            	}
                out << idStr;
                line_length += idStr.size();
            }
        }
        if((m_Option & eHtml) && (sdl->id_url != NcbiEmptyString)) {
            out << "</a>";
        }
        line_component = (line_component.empty() ? "" : "  ") + sdl->defline;
        string actual_line_component;
        if(line_component.size()+line_length > m_LineLen){
            actual_line_component = line_component.substr(0, m_LineLen -
                                                          line_length - 3);
            actual_line_component += kEllipsis;
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
        CAlignFormatUtil::AddSpace(out, m_LineLen - line_length);
        out << kTwoSpaceMargin;

        if((m_Option & eHtml) && (sdl->score_url != NcbiEmptyString)) {
            out << sdl->score_url;
        }
        out << (*iter)->bit_string;
        if((m_Option & eHtml) && (sdl->score_url != NcbiEmptyString)) {
            out << "</a>";
        }
        CAlignFormatUtil::AddSpace(out, m_MaxScoreLen - (*iter)->bit_string.size());
        if (m_Option & eShowTotalScore) {
            out << kTwoSpaceMargin << kOneSpaceMargin << (*iter)->total_bit_string;
            CAlignFormatUtil::AddSpace(out, m_MaxTotalScoreLen - 
                                   (*iter)->total_bit_string.size());
        }

        if (m_Option & eShowQueryCoverage) {        
            //int percent_coverage = 100*(*iter)->master_covered_length/m_QueryLength;        
            int percent_coverage = (*iter)->percent_coverage;
            
            out << kTwoSpaceMargin << percent_coverage << "%";        
            //minus one due to % sign
            CAlignFormatUtil::AddSpace(out, kQueryCov.size() - 
                                       NStr::IntToString(percent_coverage).size() - 1);
        }


        out << kTwoSpaceMargin << (*iter)->evalue_string;
        CAlignFormatUtil::AddSpace(out, m_MaxEvalueLen - (*iter)->evalue_string.size());

        if(m_Option & eShowSumN){ 
            out << kTwoSpaceMargin << (*iter)->sum_n;   
            CAlignFormatUtil::AddSpace(out, m_MaxSumNLen - 
                     NStr::IntToString((*iter)->sum_n).size());
        }
        if(m_Option & eShowPercentIdent){
            int percent_identity =(int) (0.5 + (*iter)->percent_identity);
            if(percent_identity > 100) {
                percent_identity = min(99, percent_identity);                        
            }            
            out << kTwoSpaceMargin << percent_identity <<"%";
            CAlignFormatUtil::AddSpace(out, m_MaxPercentIdentityLen - 
                                           NStr::IntToString(percent_identity).size());
        }
        if((m_Option & eLinkout) && (m_Option & eHtml)){
            bool is_first = true;
            ITERATE(list<string>, iter_linkout, sdl->linkout_list){
                if(is_first){
                    out << kOneSpaceMargin;
                    is_first = false;
                }
                out << *iter_linkout;
            }
        }
        out <<"\n";
        delete sdl;
    }
}

void CShowBlastDefline::DisplayBlastDefline(CNcbiOstream & out)
{
    x_InitDeflineTable();
    if(m_StructureLinkout){
        char buf[512];
        string  mapCDDParams = (NStr::Find(m_CddRid,"data_cache") != NPOS) ? "" : "blast_CD_RID=" + m_CddRid;
        sprintf(buf, kStructure_Overview, m_Rid.c_str(),
                        0, 0, mapCDDParams.c_str(), "overview",
                        m_EntrezTerm == NcbiEmptyString ?
                        "none": m_EntrezTerm.c_str());
        out << buf <<"\n\n";
    }
    x_DisplayDefline(out);    
}

static void s_DisplayDescrColumnHeader(CNcbiOstream & out,
                                       int currDisplaySort,
                                       string query_buf,
                                       int columnDisplSort,
                                       int columnHspSort,
                                       string columnText,
                                       int max_data_len,
                                       bool html)


{
    if (html) {
        if(currDisplaySort == columnDisplSort) {
            out << "<th class=\"sel\">";
        }
        else {
            out << "<th>";
        }

        out << "<a href=\"Blast.cgi?"
            << "CMD=Get&" << query_buf
            << "&DISPLAY_SORT=" << columnDisplSort
            << "&HSP_SORT=" << columnHspSort
            << "#sort_mark\">";

    }
    out << columnText;
    if (html) {
        out << "</a></th>\n";
    }
    else {
        CAlignFormatUtil::AddSpace(out, max_data_len - columnText.size());
        CAlignFormatUtil::AddSpace(out, kTwoSpaceMargin.size());
    }

}

void CShowBlastDefline::x_InitDeflineTable(void)
{
    /*Note we can't just show each alnment as we go because we will
      need to show defline only once for all hsp's with the same id*/

    bool is_first_aln = true;
    size_t num_align = 0;
    CConstRef<CSeq_id> previous_id, subid;
    m_MaxScoreLen = kBits.size();
    m_MaxEvalueLen = kValue.size();
    m_MaxSumNLen =1;
    m_MaxTotalScoreLen = kTotal.size();    
    m_MaxPercentIdentityLen = kIdentity.size();
    int percent_identity = 0;
    m_MaxQueryCoverLen = kQueryCov.size();    
    
    
    if(m_Option & eHtml){
        m_ConfigFile.reset(new CNcbiIfstream(".ncbirc"));
        m_Reg.reset(new CNcbiRegistry(*m_ConfigFile));
        if(!m_BlastType.empty()) m_LinkoutOrder = m_Reg->Get(m_BlastType,"LINKOUT_ORDER");
        m_LinkoutOrder = (!m_LinkoutOrder.empty()) ? m_LinkoutOrder : kLinkoutOrderStr;
    }

    CSeq_align_set hit;
    m_QueryLength = 1;
    bool master_is_na = false;
    //prepare defline

    int ialn = 0;
    for (CSeq_align_set::Tdata::const_iterator
             iter = m_AlnSetRef->Get().begin();
         iter != m_AlnSetRef->Get().end() && num_align < m_NumToShow;
         iter++, ialn++){

        if (ialn < m_SkipTo && ialn >= m_SkipFrom) continue;
        if (is_first_aln) {
            m_QueryLength = m_MasterRange ?
                m_MasterRange->GetLength() :
                m_ScopeRef->GetBioseqHandle((*iter)->GetSeq_id(0)).GetBioseqLength();
            master_is_na = m_ScopeRef->GetBioseqHandle((*iter)->GetSeq_id(0)).
                GetBioseqCore()->IsNa();
        }

        subid = &((*iter)->GetSeq_id(1));

        // This if statement is working on the last CSeq_align_set, stored in "hit"
        // This is confusing and the loop should probably be restructured at some point.
        if(!is_first_aln && !(subid->Match(*previous_id))) {
            SScoreInfo* sci = x_GetScoreInfoForTable(hit, num_align);
            if(sci){
                m_ScoreList.push_back(sci);
                if(m_MaxScoreLen < sci->bit_string.size()){
                    m_MaxScoreLen = sci->bit_string.size();
                }
                if(m_MaxTotalScoreLen < sci->total_bit_string.size()){
                    m_MaxTotalScoreLen = sci->total_bit_string.size();
                }
                percent_identity = CAlignFormatUtil::GetPercentMatch(sci->match,sci->align_length);
                if(m_MaxPercentIdentityLen < NStr::IntToString(percent_identity).size()) {
                    m_MaxPercentIdentityLen = NStr::IntToString(percent_identity).size();
                }
                if(m_MaxEvalueLen < sci->evalue_string.size()){
                    m_MaxEvalueLen = sci->evalue_string.size();
                }

                if( m_MaxSumNLen < NStr::IntToString(sci->sum_n).size()){
                    m_MaxSumNLen = NStr::IntToString(sci->sum_n).size();
                }
                hit.Set().clear();
            }

            num_align++; // Only increment if new subject ID found.
        }
        if (num_align < m_NumToShow) { //no adding if number to show already reached
            hit.Set().push_back(*iter);
        }
        is_first_aln = false;
        previous_id = subid;
    }

    //the last hit
    SScoreInfo* sci = x_GetScoreInfoForTable(hit, num_align);
    if(sci){
         m_ScoreList.push_back(sci);
        if(m_MaxScoreLen < sci->bit_string.size()){
            m_MaxScoreLen = sci->bit_string.size();
        }
        if(m_MaxTotalScoreLen < sci->total_bit_string.size()){
            m_MaxScoreLen = sci->total_bit_string.size();
        }
        percent_identity = CAlignFormatUtil::GetPercentMatch(sci->match,sci->align_length);
        if(m_MaxPercentIdentityLen < NStr::IntToString(percent_identity).size()) {
            m_MaxPercentIdentityLen =  NStr::IntToString(percent_identity).size();
        }
        if(m_MaxEvalueLen < sci->evalue_string.size()){
            m_MaxEvalueLen = sci->evalue_string.size();
        }

        if( m_MaxSumNLen < NStr::IntToString(sci->sum_n).size()){
            m_MaxSumNLen = NStr::IntToString(sci->sum_n).size();
        }
        hit.Set().clear();
    }

    if((m_Option & eLinkout) && (m_Option & eHtml) && !m_IsDbNa && !master_is_na)
        m_StructureLinkout = x_CheckForStructureLink();

}

void CShowBlastDefline::x_DisplayDeflineTable(CNcbiOstream & out)
{
    //This is max number of columns in the table - later should be probably put in enum DisplayOption
        if((m_PsiblastStatus == eFirstPass) ||
           (m_PsiblastStatus == eRepeatPass)){

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
            //This is done instead of code displaying titles
            if(!(m_Option & eNoShowHeader)) {

                if(m_Option & eHtml){

                    out << "<b>";
                }
                out << kHeader << "\n";
                if(m_Option & eHtml){
                    out << "</b>";
                    out << "(Click headers to sort columns)\n";
                }
            }
            if(m_Option & eHtml){
                out << "<div id=\"desctbl\">" << "<table id=\"descs\">" << "\n" << "<thead>" << "\n";
                out << "<tr class=\"first\">" << "\n" << "<th>Accession</th>" << "\n" << "<th>Description</th>" << "\n";
            }

            string query_buf;
            map< string, string> parameters_to_change;
            parameters_to_change.insert(map<string, string>::
                                        value_type("DISPLAY_SORT", ""));
            parameters_to_change.insert(map<string, string>::
                                        value_type("HSP_SORT", ""));
            CAlignFormatUtil::BuildFormatQueryString(*m_Ctx,
                                                     parameters_to_change,
                                                     query_buf);

            parameters_to_change.clear();

            string display_sort_value = m_Ctx->GetRequestValue("DISPLAY_SORT").
                GetValue();
            int display_sort = display_sort_value == NcbiEmptyString ?
                CAlignFormatUtil::eEvalue : NStr::StringToInt(display_sort_value);

            s_DisplayDescrColumnHeader(out,display_sort,query_buf,CAlignFormatUtil::eHighestScore,CAlignFormatUtil::eScore,kMaxScore,m_MaxScoreLen,m_Option & eHtml);

            s_DisplayDescrColumnHeader(out,display_sort,query_buf,CAlignFormatUtil::eTotalScore,CAlignFormatUtil::eScore,kTotalScore,m_MaxTotalScoreLen,m_Option & eHtml);
            s_DisplayDescrColumnHeader(out,display_sort,query_buf,CAlignFormatUtil::eQueryCoverage,CAlignFormatUtil::eHspEvalue,kCoverage,m_MaxQueryCoverLen,m_Option & eHtml);
            s_DisplayDescrColumnHeader(out,display_sort,query_buf,CAlignFormatUtil::eEvalue,CAlignFormatUtil::eHspEvalue,kEvalue,m_MaxEvalueLen,m_Option & eHtml);
            if(m_Option & eShowPercentIdent){
                s_DisplayDescrColumnHeader(out,display_sort,query_buf,CAlignFormatUtil::ePercentIdentity,CAlignFormatUtil::eHspPercentIdentity,kIdentity,m_MaxPercentIdentityLen,m_Option & eHtml);
            }else {
            }

            if(m_Option & eShowSumN){
                out << "<th>" << kN << "</th>" << "\n";

            }
            if (m_Option & eLinkout) {
                out << "<th>Links</th>\n";
                out << "</tr>\n";
                out << "</thead>\n";
            }
        }

    if (m_Option & eHtml) {
        out << "<tbody>\n";
    }

    x_DisplayDeflineTableBody(out);

    if (m_Option & eHtml) {
    out << "</tbody>\n</table></div>\n";
    }
}

void CShowBlastDefline::x_DisplayDeflineTableBody(CNcbiOstream & out)
{
    int percent_identity = 0;
    int tableColNumber = (m_Option & eShowPercentIdent) ? 9 : 8;
    bool first_new =true;
    int prev_database_type = 0, cur_database_type = 0;
    bool is_first = true;
    // Mixed db is genomic + transcript and this does not apply to proteins.
    bool is_mixed_database = false;
    if (m_IsDbNa == true)
       is_mixed_database = CAlignFormatUtil::IsMixedDatabase(*m_Ctx);

    map< string, string> parameters_to_change;
    string query_buf;
    if (is_mixed_database && m_Option & eHtml) {
        parameters_to_change.insert(map<string, string>::
                                    value_type("DATABASE_SORT", ""));
        CAlignFormatUtil::BuildFormatQueryString(*m_Ctx,
                                                 parameters_to_change,
                                                 query_buf);
    }
    ITERATE(vector<SScoreInfo*>, iter, m_ScoreList){
        SDeflineInfo* sdl = x_GetDeflineInfo((*iter)->id, (*iter)->use_this_seqid, (*iter)->blast_rank);
        size_t line_length = 0;
        string line_component;
        cur_database_type = (sdl->linkout & eGenomicSeq);
        if (is_mixed_database) {
            if (is_first) {
                if (m_Option & eHtml) {
                    out << "<tr>\n<th colspan=\"" << tableColNumber<< "\" class=\"l sp\">";
                }
                if (cur_database_type) {
                    out << "Genomic sequences";
                } else {
                    out << "Transcripts";
                }
                if (!(m_Option & eHtml)) {
                    out << ":\n";
                }
                if (m_Option & eHtml) {
                    out << "</th></tr>\n";
                }
            } else if (prev_database_type != cur_database_type) {
                if (m_Option & eHtml) {
                    out << "<tr>\n<th colspan=\"" << tableColNumber<< "\" class=\"l sp\">";
                }
                if (cur_database_type) {
                    out << "Genomic sequences";
                } else {
                    out << "Transcripts";
                }
                if (m_Option & eHtml) {
                    out << "<span class=\"slink\">"
                        << " [<a href=\"Blast.cgi?CMD=Get&"
                        << query_buf
                        << "&DATABASE_SORT=";
                    if (cur_database_type) {
                        out << CAlignFormatUtil::eGenomicFirst;
                    } else {
                        out << CAlignFormatUtil::eNonGenomicFirst;
                    }
                    out << "#sort_mark\">show first</a>]</span>";
                }
                else {
                    out << ":\n";
                }
                if (m_Option & eHtml) {
                    out << "</th></tr>\n";
                }
            }
        }
        prev_database_type = cur_database_type;
        is_first = false;
        if (m_Option & eHtml) {
            out << "<tr>\n";
            out << "<td class=\"l\">\n";
        }
        if ((m_Option & eHtml) && (sdl->gi > ZERO_GI)){
            if((m_Option & eShowNewSeqGif)) {
                if (sdl->is_new) {
                    if (first_new) {
                        first_new = false;
                        out << kPsiblastEvalueLink;
                    }
                    out << kPsiblastNewSeqGif;

                } else {
                    out << kPsiblastNewSeqBackgroundGif;
                }
                if (sdl->was_checked) {
                    out << kPsiblastCheckedGif;

                } else {
                    out << kPsiblastCheckedBackgroundGif;
                }
            }
            char buf[256];
            if((m_Option & eCheckboxChecked)){
                sprintf(buf, kPsiblastCheckboxChecked.c_str(), sdl->gi,
                        sdl->gi);
                out << buf;
            } else if (m_Option & eCheckbox) {
                sprintf(buf, kPsiblastCheckbox.c_str(), sdl->gi);
                out << buf;
            }
        }


        if((m_Option & eHtml) && (sdl->id_url != NcbiEmptyString)) {
            out << sdl->id_url;
        }
        if(m_Option & eShowGi){
            if(sdl->gi > ZERO_GI){
                line_component = "gi|" + NStr::NumericToString(sdl->gi) + "|";
                out << line_component;
                line_length += line_component.size();
            }
        }
        if(!sdl->id.Empty()){
            if(!(sdl->id->AsFastaString().find("gnl|BL_ORD_ID") != string::npos ||
		sdl->id->AsFastaString().find("lcl|Subject_") != string::npos)){
                string id_str;
                sdl->id->GetLabel(&id_str, CSeq_id::eContent);
                out << id_str;
                line_length += id_str.size();
            }
        }
        if((m_Option & eHtml) && (sdl->id_url != NcbiEmptyString)) {
            out << "</a>";
        }
        if (m_Option & eHtml) {
            out << "</td><td class=\"lim l\"><div class=\"lim\">";
        }
        line_component = "  " + sdl->defline;
        string actual_line_component;
        actual_line_component = line_component;

        if (m_Option & eHtml) {
            out << CHTMLHelper::HTMLEncode(actual_line_component);
            out << "</div></td><td>";
        } else {
            out << actual_line_component;
        }

        if((m_Option & eHtml) && (sdl->score_url != NcbiEmptyString)) {
            out << sdl->score_url;
        }
        out << (*iter)->bit_string;
        if((m_Option & eHtml) && (sdl->score_url != NcbiEmptyString)) {
            out << "</a>";
        }
        if(m_Option & eHtml) {
            out << "</td>";
            out << "<td>" << (*iter)->total_bit_string << "</td>";
        }
        if (!(m_Option & eHtml)) {
            CAlignFormatUtil::AddSpace(out, m_MaxScoreLen - (*iter)->bit_string.size());

            out << kTwoSpaceMargin << kOneSpaceMargin << (*iter)->total_bit_string;
            CAlignFormatUtil::AddSpace(out, m_MaxTotalScoreLen -
                                   (*iter)->total_bit_string.size());
        }

                int percent_coverage = 100*(*iter)->master_covered_length/m_QueryLength;
        if (m_Option & eHtml) {
            out << "<td>" << percent_coverage << "%</td>";
        }
        else {
            out << kTwoSpaceMargin << percent_coverage << "%";

            //minus one due to % sign
            CAlignFormatUtil::AddSpace(out, m_MaxQueryCoverLen -
                                       NStr::IntToString(percent_coverage).size() - 1);
        }

        if (m_Option & eHtml) {
            out << "<td>" << (*iter)->evalue_string << "</td>";
        }
        else {
            out << kTwoSpaceMargin << (*iter)->evalue_string;
            CAlignFormatUtil::AddSpace(out, m_MaxEvalueLen - (*iter)->evalue_string.size());
        }
        if(m_Option & eShowPercentIdent){
            percent_identity = CAlignFormatUtil::GetPercentMatch((*iter)->match,(*iter)->align_length);
            if (m_Option & eHtml) {
                out << "<td>" << percent_identity << "%</td>";
            }
            else {
                out << kTwoSpaceMargin << percent_identity <<"%";

                CAlignFormatUtil::AddSpace(out, m_MaxPercentIdentityLen -
                                           NStr::IntToString(percent_identity).size());
            }
        }
        //???
        if(m_Option & eShowSumN){
            if (m_Option & eHtml) {
                out << "<td>";
            }
            out << kTwoSpaceMargin << (*iter)->sum_n;
            if (m_Option & eHtml) {
                out << "</td>";
            } else {
                CAlignFormatUtil::AddSpace(out, m_MaxSumNLen -
                                           NStr::IntToString((*iter)->sum_n).size());
            }
        }

        if((m_Option & eLinkout) && (m_Option & eHtml)){

            out << "<td>";
            bool first_time = true;
            ITERATE(list<string>, iter_linkout, sdl->linkout_list){
                if(first_time){
                    out << kOneSpaceMargin;
                    first_time = false;
                }
                out << *iter_linkout;
            }
            out << "</td>";
        }
        if (m_Option & eHtml) {
            out << "</tr>";
        }
        if (!(m_Option & eHtml)) {
            out <<"\n";
        }
        delete sdl;
    }
}


void CShowBlastDefline::DisplayBlastDeflineTable(CNcbiOstream & out)
{
    x_InitDeflineTable();
    if(m_StructureLinkout){
        char buf[512];
        sprintf(buf, kStructure_Overview, m_Rid.c_str(),
                        0, 0, m_CddRid.c_str(), "overview",
                        m_EntrezTerm == NcbiEmptyString ?
                        "none": m_EntrezTerm.c_str());
        out << buf <<"\n\n";
    }
    x_DisplayDeflineTable(out);
}

CShowBlastDefline::SScoreInfo*
CShowBlastDefline::x_GetScoreInfo(const CSeq_align& aln, int blast_rank)
{
    string evalue_buf, bit_score_buf, total_bit_score_buf, raw_score_buf;
    int score = 0;
    double bits = 0;
    double evalue = 0;
    int sum_n = 0;
    int num_ident = 0;
    list<string> use_this_seq;

    use_this_seq.clear();
    CAlignFormatUtil::GetAlnScores(aln, score, bits, evalue, sum_n,
                                       num_ident, use_this_seq);

    CAlignFormatUtil::GetScoreString(evalue, bits, 0, score,
                              evalue_buf, bit_score_buf, total_bit_score_buf,
                              raw_score_buf);

    unique_ptr<SScoreInfo> score_info(new SScoreInfo);
    score_info->sum_n = sum_n == -1 ? 1:sum_n ;
    score_info->id = &(aln.GetSeq_id(1));

    score_info->use_this_seqid = use_this_seq;

    score_info->bit_string = bit_score_buf;
    score_info->raw_score_string = raw_score_buf;
    score_info->evalue_string = evalue_buf;
    score_info->id = &(aln.GetSeq_id(1));
    score_info->blast_rank = blast_rank+1;
    score_info->subjRange = CRange<TSeqPos>(0,0);
    score_info->flip = false;
    return score_info.release();
}

CShowBlastDefline::SScoreInfo*
CShowBlastDefline::x_GetScoreInfoForTable(const CSeq_align_set& aln, int blast_rank)
{
    string evalue_buf, bit_score_buf, total_bit_score_buf, raw_score_buf;

    if(aln.Get().empty())
        return NULL;

    unique_ptr<SScoreInfo> score_info(new SScoreInfo);

    unique_ptr<CAlignFormatUtil::SSeqAlignSetCalcParams> seqSetInfo( CAlignFormatUtil::GetSeqAlignSetCalcParamsFromASN(aln)); 
    if(seqSetInfo->hspNum == 0) {//calulated params are not in ASN - calculate now
        seqSetInfo.reset( CAlignFormatUtil::GetSeqAlignSetCalcParams(aln,m_QueryLength,m_TranslatedNucAlignment)); 
    }

    CAlignFormatUtil::GetScoreString(seqSetInfo->evalue, seqSetInfo->bit_score, seqSetInfo->total_bit_score, seqSetInfo->raw_score,
                              evalue_buf, bit_score_buf, total_bit_score_buf,
                              raw_score_buf);
    score_info->id = seqSetInfo->id;

    score_info->total_bit_string = total_bit_score_buf;
    score_info->bit_string = bit_score_buf;
    score_info->evalue_string = evalue_buf;
    score_info->percent_coverage = seqSetInfo->percent_coverage;
    score_info->percent_identity = seqSetInfo->percent_identity;
    score_info->hspNum = seqSetInfo->hspNum;
    score_info->totalLen = seqSetInfo->totalLen;

    score_info->use_this_seqid = seqSetInfo->use_this_seq;
    score_info->sum_n = seqSetInfo->sum_n == -1 ? 1:seqSetInfo->sum_n ;

    score_info->raw_score_string = raw_score_buf;//check if used
    score_info->match = seqSetInfo->match; //check if used
    score_info->align_length = seqSetInfo->align_length;//check if used
    score_info->master_covered_length = seqSetInfo->master_covered_length;//check if used


    score_info->subjRange = seqSetInfo->subjRange;    //check if used
    score_info->flip = seqSetInfo->flip;//check if used

    score_info->blast_rank = blast_rank+1;

    return score_info.release();
}

vector <CShowBlastDefline::SDeflineInfo*>
CShowBlastDefline::GetDeflineInfo(vector< CConstRef<CSeq_id> > &seqIds)
{
    vector <CShowBlastDefline::SDeflineInfo*>  sdlVec;
    for(size_t i = 0; i < seqIds.size(); i++) {
        list<string> use_this_seq;
        CShowBlastDefline::SDeflineInfo* sdl = x_GetDeflineInfo(seqIds[i], use_this_seq, i + 1 );
        sdlVec.push_back(sdl);
    }
    return sdlVec;
}



CShowBlastDefline::SDeflineInfo*
CShowBlastDefline::x_GetDeflineInfo(CConstRef<CSeq_id> id, list<string> &use_this_seqid, int blast_rank)
{
    SDeflineInfo* sdl = NULL;
    sdl = new SDeflineInfo;
    sdl->id = id;
    sdl->defline = "Unknown";

    try{
        const CBioseq_Handle& handle = m_ScopeRef->GetBioseqHandle(*id);
        x_FillDeflineAndId(handle, *id, use_this_seqid, sdl, blast_rank);
    } catch (const CException&){
        sdl->defline = "Unknown";
        sdl->is_new = false;
        sdl->was_checked = false;
        sdl->linkout = 0;

        if((*id).Which() == CSeq_id::e_Gi){
            sdl->gi = (*id).GetGi();
        } else {
            sdl->gi = ZERO_GI;
        }
        sdl->id = id;
        if(m_Option & eHtml){
            _ASSERT(m_Reg.get());

            string user_url= m_Reg->Get(m_BlastType, "TOOL_URL");
            string accession;
            sdl->id->GetLabel(&accession, CSeq_id::eContent);
            CRange<TSeqPos> seqRange(0,0);
            CAlignFormatUtil::SSeqURLInfo seqUrlInfo(user_url,m_BlastType,m_IsDbNa,m_Database,m_Rid,
                                                     m_QueryNumber,sdl->gi,accession,0,blast_rank,false,(m_Option & eNewTargetWindow) ? true : false,seqRange,false,ZERO_TAX_ID);
            sdl->id_url = CAlignFormatUtil::GetIDUrl(&seqUrlInfo,*id,*m_ScopeRef);
            sdl->score_url = NcbiEmptyString;
        }
    }

    return sdl;
}

void CShowBlastDefline::x_DisplayDeflineTableTemplate(CNcbiOstream & out)
{
    bool first_new =true;
    int prev_database_type = 0, cur_database_type = 0;
    bool is_first = true;

    // Mixed db is genomic + transcript and this does not apply to proteins.
    bool is_mixed_database = (m_Ctx && m_IsDbNa == true)? CAlignFormatUtil::IsMixedDatabase(*m_Ctx): false;
    string rowType = "odd";
    string subHeaderID;    
    ITERATE(vector<SScoreInfo*>, iter, m_ScoreList){
        SDeflineInfo* sdl = x_GetDeflineInfo((*iter)->id, (*iter)->use_this_seqid, (*iter)->blast_rank);
        cur_database_type = (sdl->linkout & eGenomicSeq);
        string subHeader;
        bool formatHeaderSort = !is_first && (prev_database_type != cur_database_type);
        if (is_mixed_database && (is_first || formatHeaderSort)) {
            subHeader = x_FormatSeqSetHeaders(cur_database_type, formatHeaderSort);
            subHeaderID = cur_database_type ? "GnmSeq" : "Transcr";
            //This is done for 508 complience
            subHeader = CAlignFormatUtil::MapTemplate(subHeader,"defl_header_id",subHeaderID);
        }
        prev_database_type = cur_database_type;

        string defLine = x_FormatDeflineTableLine(sdl,*iter,first_new);
        
        //This is done for 508 complience
        defLine = CAlignFormatUtil::MapTemplate(defLine,"defl_header_id",subHeaderID);

        string firstSeq = (is_first) ? "firstSeq" : "";
        defLine = CAlignFormatUtil::MapTemplate(defLine,"firstSeq",firstSeq);
        defLine = CAlignFormatUtil::MapTemplate(defLine,"trtp",rowType);

        rowType = (rowType == "odd") ? "even" : "odd";

        if(!subHeader.empty()) {
            defLine = subHeader + defLine;
        }
        is_first = false;
        out << defLine;

        delete sdl;
    }
}

SSeqDBTaxInfo taxInfo;
static void s_GetTaxonomyInfoForTaxID(TTaxId sdlTaxid, SSeqDBTaxInfo &taxInfo)
{
    string taxid;
    if(sdlTaxid > 0) {        
        taxid = NStr::IntToString(sdlTaxid);
        try{
            CSeqDB::GetTaxInfo(sdlTaxid, taxInfo);         
            taxInfo.common_name = (taxInfo.common_name == taxInfo.scientific_name || taxInfo.common_name.empty()) ? "NA" : taxInfo.common_name;
        } catch (const CException&){
            taxInfo.common_name = "Unknown";
            taxInfo.scientific_name  = "Unknown";
            taxInfo.blast_name  = "Unknown";
        }        
    }    
}          

void CShowBlastDefline::x_DisplayDeflineTableTemplateCSV(CNcbiOstream & out)
{
    ITERATE(vector<SScoreInfo*>, iter, m_ScoreList){
        SDeflineInfo* sdl = x_GetDeflineInfo((*iter)->id, (*iter)->use_this_seqid, (*iter)->blast_rank);        
        string defLine = m_DeflineTemplates->defLineTmpl;
        string seqid;
        if(!sdl->id.Empty()){
            if(!(sdl->id->AsFastaString().find("gnl|BL_ORD_ID") != string::npos ||sdl->id->AsFastaString().find("lcl|Subject_") != string::npos)) {
                sdl->id->GetLabel(&seqid, CSeq_id::eContent);
            }
        }

        if(sdl->id_url != NcbiEmptyString) {
	        string seqInfo  = CAlignFormatUtil::MapTemplate(m_DeflineTemplates->seqInfoTmpl,"dfln_url",sdl->id_url);        
            seqInfo = CAlignFormatUtil::MapTemplate(seqInfo,"dfln_seqid",seqid);            
            defLine = CAlignFormatUtil::MapTemplate(defLine,"seq_info",seqInfo);                 
        }
        else {
            defLine = CAlignFormatUtil::MapTemplate(defLine,"seq_info",seqid);
        }

	    string descr = (!sdl->defline.empty()) ? sdl->defline : "None provided";
	    s_LimitDescrLength(descr);
        if(NStr::Find(descr,",") != NPOS) {
            descr = "\"" + descr + "\"";    
        }
	    defLine = CAlignFormatUtil::MapTemplate(defLine,"dfln_defline",descr);

        SSeqDBTaxInfo taxInfo;
        s_GetTaxonomyInfoForTaxID(sdl->taxid, taxInfo);
        defLine = CAlignFormatUtil::MapTemplate(defLine,"common_name",taxInfo.common_name);
        defLine = CAlignFormatUtil::MapTemplate(defLine,"scientific_name",taxInfo.scientific_name);        
        defLine = CAlignFormatUtil::MapTemplate(defLine,"taxid",NStr::IntToString(sdl->taxid));            
              
            
        defLine = CAlignFormatUtil::MapTemplate(defLine,"score_info",(*iter)->bit_string);
        defLine = CAlignFormatUtil::MapTemplate(defLine,"total_bit_string",(*iter)->total_bit_string);
        defLine = CAlignFormatUtil::MapTemplate(defLine,"percent_coverage",NStr::IntToString((*iter)->percent_coverage) + "%");
        defLine = CAlignFormatUtil::MapTemplate(defLine,"evalue_string",(*iter)->evalue_string);    
        
        defLine = CAlignFormatUtil::MapTemplate(defLine,"percent_identity",NStr::DoubleToString((*iter)->percent_identity,2));
        int len = sequence::GetLength(*sdl->id, m_ScopeRef);
        defLine = CAlignFormatUtil::MapTemplate(defLine,"acclen",NStr::IntToString(len));        
        //defLine = CAlignFormatUtil::MapTemplate(defLine,"seq_info",seqid);    

        out << defLine;
        delete sdl;
    }

}


void CShowBlastDefline::x_DisplayDeflineTableTemplateText(CNcbiOstream & out)
{
        m_MaxPercentIdentityLen = kIdentLine2.size() + 1;
        m_MaxAccLength = 16;
        m_MaxTaxonomyNameLength = 15;
        m_MaxTaxidLength = 10;
        m_AccLenLength = 10;
                
        string descrHeader = CAlignFormatUtil::MapSpaceTemplate(m_DeflineTemplates->deflineTxtHeader,"descr_hd1"," ",m_LineLen); //empty string
        descrHeader = CAlignFormatUtil::MapSpaceTemplate(descrHeader,"sciname_hd1",kScientific,m_MaxTaxonomyNameLength);        
        descrHeader = CAlignFormatUtil::MapSpaceTemplate(descrHeader,"comname_hd1",kCommon,m_MaxTaxonomyNameLength);        
        descrHeader = CAlignFormatUtil::MapSpaceTemplate(descrHeader,"taxid_hd1"," ",m_MaxTaxidLength);        
        descrHeader = CAlignFormatUtil::MapSpaceTemplate(descrHeader,"score_hd1",kMax,m_MaxScoreLen);        
        descrHeader = CAlignFormatUtil::MapSpaceTemplate(descrHeader,"total_hd1",kTotal,m_MaxTotalScoreLen);
        descrHeader = CAlignFormatUtil::MapSpaceTemplate(descrHeader,"querycov_hd1",kQueryCov,m_MaxQueryCoverLen);
        descrHeader = CAlignFormatUtil::MapSpaceTemplate(descrHeader,"evalue_hd1","  " + kE + "  ",m_MaxEvalueLen);    
        descrHeader = CAlignFormatUtil::MapSpaceTemplate(descrHeader,"percident_hd1",kPerc,m_MaxPercentIdentityLen);
        descrHeader = CAlignFormatUtil::MapSpaceTemplate(descrHeader,"acclen_hd1",kAccAbbr,m_AccLenLength);        
        descrHeader = CAlignFormatUtil::MapSpaceTemplate(descrHeader,"acc_hd1"," ",m_MaxAccLength);        
        //kBits 	kTotalLine2 kQueryCovLine2 kValue kIdentLine2 
        descrHeader = CAlignFormatUtil::MapSpaceTemplate(descrHeader,"descr_hd2",kDescription,m_LineLen);
        descrHeader = CAlignFormatUtil::MapSpaceTemplate(descrHeader,"sciname_hd2",kName,m_MaxTaxonomyNameLength);        
        descrHeader = CAlignFormatUtil::MapSpaceTemplate(descrHeader,"comname_hd2",kName,m_MaxTaxonomyNameLength);        
        descrHeader = CAlignFormatUtil::MapSpaceTemplate(descrHeader,"taxid_hd2",kTaxid,m_MaxTaxidLength);        
        descrHeader = CAlignFormatUtil::MapSpaceTemplate(descrHeader,"score_hd2",kScoreLine2,m_MaxScoreLen);
        descrHeader = CAlignFormatUtil::MapSpaceTemplate(descrHeader,"total_hd2",kScoreLine2,m_MaxTotalScoreLen);
        descrHeader = CAlignFormatUtil::MapSpaceTemplate(descrHeader,"querycov_hd2",kQueryCovLine2,m_MaxQueryCoverLen);
        descrHeader = CAlignFormatUtil::MapSpaceTemplate(descrHeader,"evalue_hd2",kValue,m_MaxEvalueLen);    
        descrHeader = CAlignFormatUtil::MapSpaceTemplate(descrHeader,"percident_hd2",kIdentLine2,m_MaxPercentIdentityLen);        
        descrHeader = CAlignFormatUtil::MapSpaceTemplate(descrHeader,"acclen_hd2",kLenAbbr,m_AccLenLength);        
        descrHeader = CAlignFormatUtil::MapSpaceTemplate(descrHeader,"acc_hd2",kAccession,m_MaxAccLength);
        
        out << descrHeader;
    
    ITERATE(vector<SScoreInfo*>, iter, m_ScoreList){
        SDeflineInfo* sdl = x_GetDeflineInfo((*iter)->id, (*iter)->use_this_seqid, (*iter)->blast_rank);        
        string defLine = m_DeflineTemplates->defLineTmpl;
        string seqid;
        if(!sdl->id.Empty()){
            if(!(sdl->id->AsFastaString().find("gnl|BL_ORD_ID") != string::npos ||sdl->id->AsFastaString().find("lcl|Subject_") != string::npos)) {
                sdl->id->GetLabel(&seqid, CSeq_id::eContent);
            }
        }
    
        string descr = (!sdl->defline.empty()) ? sdl->defline : "None provided";
	    s_LimitDescrLength(descr,m_LineLen);
	    defLine = CAlignFormatUtil::MapSpaceTemplate(defLine,"dfln_defline",descr, m_LineLen);
        SSeqDBTaxInfo taxInfo;
        s_GetTaxonomyInfoForTaxID(sdl->taxid, taxInfo);
        defLine = CAlignFormatUtil::MapSpaceTemplate(defLine,"common_name",taxInfo.common_name,m_MaxTaxonomyNameLength);
        defLine = CAlignFormatUtil::MapSpaceTemplate(defLine,"scientific_name",taxInfo.scientific_name,m_MaxTaxonomyNameLength);        
        defLine = CAlignFormatUtil::MapSpaceTemplate(defLine,"taxid",NStr::IntToString(sdl->taxid),m_MaxTaxidLength);
        defLine = CAlignFormatUtil::MapSpaceTemplate(defLine,"score_info",(*iter)->bit_string,m_MaxScoreLen);
        defLine = CAlignFormatUtil::MapSpaceTemplate(defLine,"total_bit_string",(*iter)->total_bit_string,m_MaxTotalScoreLen);
        defLine = CAlignFormatUtil::MapSpaceTemplate(defLine,"percent_coverage",NStr::IntToString((*iter)->percent_coverage) + "%",m_MaxQueryCoverLen);
        defLine = CAlignFormatUtil::MapSpaceTemplate(defLine,"evalue_string",(*iter)->evalue_string,m_MaxEvalueLen);    
        defLine = CAlignFormatUtil::MapSpaceTemplate(defLine,"percent_identity",NStr::DoubleToString((*iter)->percent_identity,2),m_MaxPercentIdentityLen);
        
        int len = sequence::GetLength(*sdl->id, m_ScopeRef);
        defLine = CAlignFormatUtil::MapSpaceTemplate(defLine,"acclen",NStr::IntToString(len),m_AccLenLength);        
        defLine = CAlignFormatUtil::MapSpaceTemplate(defLine,"seq_info",seqid,m_MaxAccLength);    
        

        out << defLine;
        delete sdl;
    }
}


string CShowBlastDefline::x_FormatSeqSetHeaders(int isGenomicSeq, bool formatHeaderSort)
{
    string seqSetType = isGenomicSeq ? "Genomic sequences" : "Transcripts";
    string subHeader = CAlignFormatUtil::MapTemplate(m_DeflineTemplates->subHeaderTmpl,"defl_seqset_type",seqSetType);
    if (formatHeaderSort) {
        int database_sort = isGenomicSeq ? CAlignFormatUtil::eGenomicFirst : CAlignFormatUtil::eNonGenomicFirst;
        string deflnSubHeaderSort = CAlignFormatUtil::MapTemplate(m_DeflineTemplates->subHeaderSort,"database_sort",database_sort);
        subHeader = CAlignFormatUtil::MapTemplate(subHeader,"defl_header_sort",deflnSubHeaderSort);
    }
    else {
        subHeader = CAlignFormatUtil::MapTemplate(subHeader,"defl_header_sort","");
    }
    return subHeader;
}


string CShowBlastDefline::x_FormatDeflineTableLine(SDeflineInfo* sdl,SScoreInfo* iter,bool &first_new)
{
    string defLine = (((m_Option & eCheckboxChecked) || (m_Option & eCheckbox))) ? x_FormatPsi(sdl, first_new) : m_DeflineTemplates->defLineTmpl;
    string dflGi = (m_Option & eShowGi) && (sdl->gi > ZERO_GI) ? "gi|" + NStr::NumericToString(sdl->gi) + "|" : "";
    string seqid;
    if(!sdl->id.Empty()){
        if(!(sdl->id->AsFastaString().find("gnl|BL_ORD_ID") != string::npos ||
		sdl->id->AsFastaString().find("lcl|Subject_") != string::npos)) {
            sdl->id->GetLabel(&seqid, CSeq_id::eContent);
        }
    }

    if(sdl->id_url != NcbiEmptyString) {
		string seqInfo  = CAlignFormatUtil::MapTemplate(m_DeflineTemplates->seqInfoTmpl,"dfln_url",sdl->id_url);
        string trgt = (m_Option & eNewTargetWindow) ? "TARGET=\"EntrezView\"" : "";
        seqInfo = CAlignFormatUtil::MapTemplate(seqInfo,"dfln_target",trgt);
        defLine = CAlignFormatUtil::MapTemplate(defLine,"seq_info",seqInfo);
        defLine = CAlignFormatUtil::MapTemplate(defLine,"dfln_gi",dflGi);
        defLine = CAlignFormatUtil::MapTemplate(defLine,"dfln_seqid",seqid);
    }
    else {
        defLine = CAlignFormatUtil::MapTemplate(defLine,"seq_info",dflGi + seqid);
    }

    string descr = (!sdl->defline.empty()) ? sdl->defline : "None provided";
	s_LimitDescrLength(descr);
	defLine = CAlignFormatUtil::MapTemplate(defLine,"dfln_defline",CHTMLHelper::HTMLEncode(descr));

	descr = (!sdl->fullDefline.empty()) ? sdl->fullDefline : seqid;
	s_LimitDescrLength(descr);
	defLine = CAlignFormatUtil::MapTemplate(defLine,"full_dfln_defline",CHTMLHelper::HTMLEncode(descr));
    if(sdl->score_url != NcbiEmptyString) {
        string scoreInfo  = CAlignFormatUtil::MapTemplate(m_DeflineTemplates->scoreInfoTmpl,"score_url",sdl->score_url);
        scoreInfo = CAlignFormatUtil::MapTemplate(scoreInfo,"bit_string",iter->bit_string);
        scoreInfo = CAlignFormatUtil::MapTemplate(scoreInfo,"score_seqid",seqid);
        defLine = CAlignFormatUtil::MapTemplate(defLine,"score_info",scoreInfo);
    }
    else {
        defLine = CAlignFormatUtil::MapTemplate(defLine,"score_info",iter->bit_string);
    }
    /*****************This block of code is for future use with AJAX begin***************************/
    string deflId,deflFrmID,deflFastaSeq,deflAccs;
    if(sdl->gi == ZERO_GI) {
        sdl->id->GetLabel(& deflId, CSeq_id::eContent);
        deflFrmID =  CAlignFormatUtil::GetLabel(sdl->id);//Just accession without db part like GNOMON: or ti:
        deflFastaSeq = NStr::TruncateSpaces(sdl->alnIDFasta);
        deflAccs = sdl->id->AsFastaString();
    }
    else {
        deflFrmID = deflId = NStr::NumericToString(sdl->gi);
        deflFastaSeq = "gi|" + NStr::NumericToString(sdl->gi);
        deflFastaSeq = NStr::TruncateSpaces(sdl->alnIDFasta);
        sdl->id->GetLabel(&deflAccs, CSeq_id::eContent);
    }       
    SSeqDBTaxInfo taxInfo;
    s_GetTaxonomyInfoForTaxID(sdl->taxid, taxInfo);    
    defLine = CAlignFormatUtil::MapTemplate(defLine,"common_name",taxInfo.common_name);
    defLine = CAlignFormatUtil::MapTemplate(defLine,"scientific_name",taxInfo.scientific_name);
    defLine = CAlignFormatUtil::MapTemplate(defLine,"blast_name",taxInfo.blast_name);
    defLine = CAlignFormatUtil::MapTemplate(defLine,"taxid",NStr::IntToString(sdl->taxid));            
        
    int len = sequence::GetLength(*sdl->id, m_ScopeRef);
    defLine = CAlignFormatUtil::MapTemplate(defLine,"acclen",NStr::IntToString(len));        
    //Setup applog info structure
    if(m_AppLogInfo && (m_AppLogInfo->currInd < m_AppLogInfo->topMatchesNum)) {
        m_AppLogInfo->deflIdVec.push_back(deflId);
        m_AppLogInfo->accVec.push_back(deflAccs);
        m_AppLogInfo->taxidVec.push_back(NStr::NumericToString(sdl->taxid));
        m_AppLogInfo->queryCoverageVec.push_back(NStr::IntToString(iter->percent_coverage));
        m_AppLogInfo->percentIdentityVec.push_back(NStr::DoubleToString(iter->percent_identity));
        m_AppLogInfo->currInd++;
    }

    //If gi - deflFrmID and deflId are the same and equal to gi "555",deflFastaSeq will have "gi|555"
    //If gnl - deflFrmID=number, deflId=ti:number,deflFastaSeq=gnl|xxx
    //like  "268252125","ti:268252125","gnl|ti|961433.m" or "961433.m" and "GNOMON:961433.m" "gnl|GNOMON|961433.m"
    defLine = CAlignFormatUtil::MapTemplate(defLine,"dfln_id",deflId);
    defLine = CAlignFormatUtil::MapTemplate(defLine,"dflnFrm_id",deflFrmID);
    defLine = CAlignFormatUtil::MapTemplate(defLine,"dflnFASTA_id",deflFastaSeq);
    defLine = CAlignFormatUtil::MapTemplate(defLine,"dflnAccs",deflAccs);
    defLine = CAlignFormatUtil::MapTemplate(defLine,"dfln_rid",m_Rid);
    defLine = CAlignFormatUtil::MapTemplate(defLine,"dfln_hspnum",iter->hspNum);
    defLine = CAlignFormatUtil::MapTemplate(defLine,"dfln_alnLen",iter->totalLen);
    defLine = CAlignFormatUtil::MapTemplate(defLine,"dfln_blast_rank",m_StartIndex + iter->blast_rank);

	/*****************This block of code is for future use with AJAX end***************************/

    defLine = CAlignFormatUtil::MapTemplate(defLine,"total_bit_string",iter->total_bit_string);


    defLine = CAlignFormatUtil::MapTemplate(defLine,"percent_coverage",NStr::IntToString(iter->percent_coverage));

    defLine = CAlignFormatUtil::MapTemplate(defLine,"evalue_string",iter->evalue_string);

    if(m_Option & eShowPercentIdent){
        defLine = CAlignFormatUtil::MapTemplate(defLine,"percent_identity",NStr::DoubleToString(iter->percent_identity,2));
    }

    if(m_Option & eShowSumN){
        defLine = CAlignFormatUtil::MapTemplate(defLine,"sum_n",NStr::IntToString(iter->sum_n));
    }

	string links;
    //sdl->linkout_list may contain linkouts + mapview link + seqview link
    ITERATE(list<string>, iter_linkout, sdl->linkout_list){
        links += *iter_linkout;
    }
    defLine = CAlignFormatUtil::MapTemplate(defLine,"linkout",links);

    return defLine;
}

string CShowBlastDefline::x_FormatPsi(SDeflineInfo* sdl, bool &first_new)
{
    string defline = m_DeflineTemplates->defLineTmpl;
    string show_new,psi_new,psi_new_accesible,show_checked,replaceBy,psiNewSeq;
    if((m_Option & eShowNewSeqGif)) {
        replaceBy = (sdl->is_new && first_new) ? m_DeflineTemplates->psiFirstNewAnchorTmpl : "";
        first_new = (sdl->is_new && first_new) ? false : first_new;
        if (!sdl->is_new) {
            show_new = "hidden";
        }
        if (sdl->is_new && m_StepNumber > 1) {
            psi_new = "psi_new";
            psi_new_accesible = "psiNw";
            psiNewSeq = "on";
        }
        else {
            psiNewSeq = "off";
        }

        if(!sdl->was_checked) {
            show_checked = "hidden";
        }
        string psiUsedToBuildPssm = (sdl->was_checked) ? "on" : "off";
        

        defline = CAlignFormatUtil::MapTemplate(defline,"first_new",replaceBy);
        defline = CAlignFormatUtil::MapTemplate(defline,"psi_new_gi",show_new);
        defline = CAlignFormatUtil::MapTemplate(defline,"psi_new_gi_hl",psi_new);
        defline = CAlignFormatUtil::MapTemplate(defline,"psi_new_gi_accs",psi_new_accesible);//insert for accesibilty
        defline = CAlignFormatUtil::MapTemplate(defline,"psi_checked_gi",show_checked);

        defline = CAlignFormatUtil::MapTemplate(defline,"psi_new_seq",psiNewSeq);
        defline = CAlignFormatUtil::MapTemplate(defline,"psi_used_in_pssm",psiUsedToBuildPssm);
    }

    replaceBy = (m_Option & eCheckboxChecked) ? m_DeflineTemplates->psiGoodGiHiddenTmpl : "";//<@psi_good_gi@>
    defline = CAlignFormatUtil::MapTemplate(defline,"psi_good_gi",replaceBy);

    replaceBy = (m_Option & eCheckboxChecked) ? "checked=\"checked\"" : "";
    defline = CAlignFormatUtil::MapTemplate(defline,"gi_checked",replaceBy);
    if(sdl->gi > ZERO_GI) {
        defline = CAlignFormatUtil::MapTemplate(defline,"psiGi",NStr::NumericToString(sdl->gi));
    }
    else {
        defline = CAlignFormatUtil::MapTemplate(defline,"psiGi",sdl->textSeqID);
    }

    return defline;
}




void CShowBlastDefline::x_InitFormattingInfo(SScoreInfo* sci)
{
    SDeflineFormattingInfo* sdlFormatInfo = new SDeflineFormattingInfo;
    SDeflineInfo* sdl = x_GetDeflineInfo(sci->id, sci->use_this_seqid, sci->blast_rank);

                string dflGi = (m_Option & eShowGi) && (sdl->gi > ZERO_GI) ? "gi|" + NStr::NumericToString(sdl->gi) + "|" : "";
                string seqid;
                if(!sdl->id.Empty()){
                    if(!(sdl->id->AsFastaString().find("gnl|BL_ORD_ID") != string::npos || sdl->id->AsFastaString().find("lcl|Subject_") != string::npos)) {
                        sdl->id->GetLabel(&seqid, CSeq_id::eContent);
                    }
                }

		        sdlFormatInfo->dfln_url = sdl->id_url;

                sdlFormatInfo->dfln_rid = m_Rid;
                sdlFormatInfo->dfln_gi = dflGi;
                sdlFormatInfo->dfln_seqid = seqid;


                string descr = (!sdl->defline.empty()) ? sdl->defline : "None provided";
	            s_LimitDescrLength(descr);

                sdlFormatInfo->dfln_defline = CHTMLHelper::HTMLEncode(descr);

	            descr = (!sdl->fullDefline.empty()) ? sdl->fullDefline : seqid;
	            s_LimitDescrLength(descr);
                sdlFormatInfo->full_dfln_defline = CHTMLHelper::HTMLEncode(descr);



                string deflId,deflFrmID,deflFastaSeq,deflAccs;
                if(sdl->gi == ZERO_GI) {
                    sdl->id->GetLabel(& deflId, CSeq_id::eContent);
                    deflFrmID =  CAlignFormatUtil::GetLabel(sdl->id);//Just accession without db part like GNOMON: or ti:
                    deflFastaSeq = NStr::TruncateSpaces(sdl->alnIDFasta);
                    deflAccs = sdl->id->AsFastaString();
                }
                else {
                    deflFrmID = deflId = NStr::NumericToString(sdl->gi);
                    deflFastaSeq = "gi|" + NStr::NumericToString(sdl->gi);
                    deflFastaSeq = NStr::TruncateSpaces(sdl->alnIDFasta);
                    sdl->id->GetLabel(&deflAccs, CSeq_id::eContent);
                }

                sdlFormatInfo->dfln_id = deflId;
                sdlFormatInfo->dflnFrm_id = deflFrmID;
                sdlFormatInfo->dflnFASTA_id = deflFastaSeq;
                sdlFormatInfo->dflnAccs =deflAccs;

                sdlFormatInfo->score_info = sci->bit_string;
                sdlFormatInfo->dfln_hspnum =  NStr::IntToString(sci->hspNum);
                sdlFormatInfo->dfln_alnLen =  NStr::NumericToString(sci->totalLen);
                sdlFormatInfo->dfln_blast_rank =  NStr::IntToString(m_StartIndex + sci->blast_rank);
	            sdlFormatInfo->total_bit_string = sci->total_bit_string;
                sdlFormatInfo->percent_coverage = NStr::IntToString(sci->percent_coverage);
                sdlFormatInfo->evalue_string = sci->evalue_string;
                sdlFormatInfo->percent_identity = NStr::DoubleToString(sci->percent_identity);
                m_SdlFormatInfoVec.push_back(sdlFormatInfo);
}


vector <CShowBlastDefline::SDeflineFormattingInfo *> CShowBlastDefline::GetFormattingInfo(void)
{
    /*Note we can't just show each alnment as we go because we will
      need to show defline only once for all hsp's with the same id*/

    bool is_first_aln = true;
    size_t num_align = 0;
    CConstRef<CSeq_id> previous_id, subid;

    CSeq_align_set hit;
    m_QueryLength = 1;

    //prepare defline


    for (CSeq_align_set::Tdata::const_iterator iter = m_AlnSetRef->Get().begin();
         iter != m_AlnSetRef->Get().end() && num_align < m_NumToShow;
         iter++)
    {

        if (is_first_aln) {
            m_QueryLength = m_MasterRange ? m_MasterRange->GetLength() : m_ScopeRef->GetBioseqHandle((*iter)->GetSeq_id(0)).GetBioseqLength();
        }

        subid = &((*iter)->GetSeq_id(1));

        if(!is_first_aln && !(subid->Match(*previous_id))) {

            SScoreInfo* sci = x_GetScoreInfoForTable(hit, num_align);
            if(sci) {
                x_InitFormattingInfo(sci);
                hit.Set().clear();
            }

            num_align++; // Only increment if new subject ID found.
        }
        if (num_align < m_NumToShow) { //no adding if number to show already reached
            hit.Set().push_back(*iter);
        }
        is_first_aln = false;
        previous_id = subid;
    }
    //the last hit
    SScoreInfo* sci = x_GetScoreInfoForTable(hit, num_align);

    if(sci) {
        x_InitFormattingInfo(sci);
        hit.Set().clear();
    }

    return m_SdlFormatInfoVec;
}


END_SCOPE(align_format)
END_NCBI_SCOPE
