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
 *   Alignment graphic overview (using HTML table) 
 *
 */

#include <ncbi_pch.hpp>
#include <objtools/align/alngraphic.hpp>
#include <util/range.hpp>
#include <serial/iterator.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/seqalign_exception.hpp>
#include <objects/general/Object_id.hpp>
#include <objmgr/util/sequence.hpp>
#include <html/html.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE (objects)
USING_SCOPE (sequence);

static string kDigitGif[] = {"0.gif", "1.gif", "2.gif", "3.gif", "4.gif",
                             "5.gif", "6.gif", "7.gif", "8.gif", "9.gif"};

static const TSeqPos kScoreMargin = 50;
static const TSeqPos kScoreLength = 500;
static const TSeqPos kScoreHeight = 40;

static const TSeqPos kBlankBarHeight = 4; 

static const TSeqPos kMasterHeight = 10;
static const TSeqPos kMasterPixel = kScoreLength;
static const TSeqPos kMasterBarLength = 550;

static const TSeqPos kScaleMarginAdj = 1;
static const TSeqPos kScaleWidth = 2;
static const TSeqPos kScaleHeight = 10;

static const TSeqPos kDigitWidth = 10;
static const TSeqPos kDigitHeight = 13;

static const TSeqPos kGapHeight = 1;
static const TSeqPos kGreakHeight = 6;
static const TSeqPos kNumMark = 6;

static const int kDeflineLength = 55;

/*two ranges are considered overlapping even if they do not overlap but
  they differ only by this number*/
static const int kOverlapDiff = 5;

//gif images
static const string kGifWhite = "white.gif";
static const string kGifMaster ="query_no_scale.gif";
static const string kGifScore = "score.gif";
static const string kGifGrey = "grey.gif";
static const string kGifScale = "scale.gif";
static const string kGifBlack = "black.gif";

static string s_GetGif(int bits){
    string gif = NcbiEmptyString;
    if(bits < 40){
        gif = "black.gif";
    } else if (bits < 50) {
        gif = "blue.gif";
    } else if (bits < 80) {
        gif = "green.gif";
    } else if (bits < 200) {
        gif = "purple.gif";
    } else  {
        gif = "red.gif";
    } 
    //for alignment with no bits info
    if(bits == 0){
        gif = "red.gif";
    }
    return gif;
}

//Round the number.  i.e., 290 => 250, 735 => 700. 
static int s_GetRoundNumber (int number){
    int round = 0;
   
    if (number > 10) {
        string num_str = NStr::IntToString(number);
        int trail_num = NStr::StringToInt(num_str.substr(1, num_str.size() - 1));
        round = number - trail_num;
        string mid_num_str;
        int mid_num = 0;
        if(num_str.size() > 2) {
            string trail_zero (num_str.size() - 2, '0');
            mid_num_str = "5" +  trail_zero;
            mid_num = NStr::StringToInt(mid_num_str);
        }
       
        if (number >= round + mid_num) {
            round += mid_num;
        } 
    } else {
        round = number;
        if(round < 1){
            round = 1;
        }
    }
    return round;
}

CRange<TSeqPos>* CAlnGraphic::x_GetEffectiveRange(TAlnInfoList& alninfo_list){
    CRange<TSeqPos>* range = NULL;
    if(!alninfo_list.empty()){
        range = new CRange<TSeqPos>(alninfo_list.front()->range->GetFrom(), 
                                    alninfo_list.back()->range->GetTo());
    }
    return range;
}

void CAlnGraphic::x_MergeDifferentSeq(double pixel_factor){
  
    TAlnInfoListList::iterator iter_temp;
    list<CRange<TSeqPos>* > effective_range_list;
    CRange<TSeqPos> *effective_range = NULL, *temp_range = NULL;
   
    NON_CONST_ITERATE(TAlnInfoListList, iter, m_AlninfoListList) {
        iter_temp = iter; //current list
        iter_temp ++;  //the next list

        if(!(*iter)->empty() && iter_temp != m_AlninfoListList.end()){
            /*effective ranges means the range covering all ranges for
              alignment from the same seq*/
            temp_range = x_GetEffectiveRange(**iter);
            if(temp_range){
                effective_range_list.push_back(temp_range);
            }
  
            /*compare the range in current list to range in all other lists.
              If the latter does not overlap with the ranges in the current
              list, move it to the current one*/
            while(iter_temp != m_AlninfoListList.end()){
                /* Get effective range. Note that, except the current list,
                   all other list contains only one effective range. The
                   current list may have more than one because the range 
                   from other list may be moved to it*/
                effective_range = x_GetEffectiveRange(**iter_temp);
                bool overlap = false;
                if(effective_range){
                    NON_CONST_ITERATE(list<CRange<TSeqPos>*>,
                                      iter2, 
                                      effective_range_list){
                        CRange<TSeqPos>* temp_range2 = *iter2;
                        int min_to = min(effective_range->GetTo(), 
                                         temp_range2->GetTo());
                        int max_from =  max(effective_range->GetFrom(), 
                                            temp_range2->GetFrom());
                        //iter2 = ranges in current list
                        if((int)((max_from - min_to)*pixel_factor) <= kOverlapDiff){
                            //Not overlap but has only overlap_diff is still
                            //considered overlap as it's difficult to see the
                            //difference of only 1 pixel 

                            overlap = true;
                            break;  //overlaps, do nothing
                        } 
                    }
                    //if no overlap, move this list to the current list
                    if(!overlap){
                        (*iter)->merge(**iter_temp);
                        effective_range_list.push_back(effective_range);
                    }
                    if(overlap){
                        delete effective_range;
                    }
                }
                iter_temp ++;
            }
            ITERATE(list<CRange<TSeqPos>* >, iter4, effective_range_list){
                delete *iter4;
            }
            effective_range_list.clear();
        }
        (*iter)->sort(FromRangeAscendingSort);
    }
}
void CAlnGraphic::x_MergeSameSeq(TAlnInfoList& alninfo_list){  
    TAlnInfoList::iterator prev_iter; 
    int i = 0;
 
    NON_CONST_ITERATE (TAlnInfoList, iter, alninfo_list){
        if(i > 0 && (*prev_iter)->range->IntersectingWith(*((*iter)->range))){
            //merge range
            (*iter)->range->Set(min((*prev_iter)->range->GetFrom(), 
                                    (*iter)->range->GetFrom()),
                                max((*prev_iter)->range->GetTo(), 
                                    (*iter)->range->GetTo()));
            //take the maximal score and evalue 
            if((*iter)->bits < (*prev_iter)->bits){
                (*iter)->bits = (*prev_iter)->bits;
                (*iter)->info = (*prev_iter)->info;
            }
            delete (*prev_iter)->range;
            delete *prev_iter;
            alninfo_list.erase(prev_iter);     
        }
        i ++;
        prev_iter = iter;
    }    
}

template<class container> 
static bool s_GetBlastScore(const container& scoreList,  
                            int& score, 
                            double& bits, 
                            double& evalue){
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
            } 
        }
    }
    return hasScore;
}

static void s_GetAlnScores(const CSeq_align& aln, 
                           int& score, 
                           double& bits, 
                           double& evalue){
    bool hasScore = false;
    //look for scores at seqalign level first
    hasScore = s_GetBlastScore(aln.GetScore(),  score, bits, evalue);
    
    //look at the seg level
    if(!hasScore){
        const CSeq_align::TSegs& seg = aln.GetSegs();
        if(seg.Which() == CSeq_align::C_Segs::e_Std){
            s_GetBlastScore(seg.GetStd().front()->GetScores(), 
                            score, bits, evalue);
        } else if (seg.Which() == CSeq_align::C_Segs::e_Dendiag){
            s_GetBlastScore(seg.GetDendiag().front()->GetScores(), 
                            score, bits, evalue);
        }  else if (seg.Which() == CSeq_align::C_Segs::e_Denseg){
            s_GetBlastScore(seg.GetDenseg().GetScores(),  score, bits, evalue);
        }
    }
}


void CAlnGraphic::x_GetAlnInfo(const CSeq_align& aln, const CSeq_id& id,
                               SAlignInfo* aln_info){
    string info;
    int score = 0;
    double evalue = 0;
    double bits = 0;
    string bit_str, evalue_str;
    string title;
    
    try{
        const CBioseq_Handle& handle = m_Scope->GetBioseqHandle(id);
        if(handle){
            const CBioseq::TId& ids = handle.GetBioseqCore()->GetId();
            CRef<CSeq_id> wid = FindBestChoice(ids, CSeq_id::WorstRank);
            aln_info->id = wid;
            aln_info->gi =  FindGi(ids);
            wid->GetLabel(&info, CSeq_id::eContent, 0);
            title = GetTitle(handle);
            
            if ((int)title.size() > kDeflineLength){
                title = title.substr(0, kDeflineLength) + "..";
            }
            info += " " + title;   
        } else {
            aln_info->gi = 0;
            aln_info->id = &id;
            aln_info->id->GetLabel(&info, CSeq_id::eContent, 0);
        }
    } catch (const CException&){
        aln_info->gi = 0;
        aln_info->id = &id;
        aln_info->id->GetLabel(&info, CSeq_id::eContent, 0);        
    }
    s_GetAlnScores(aln, score, bits, evalue);
    NStr::DoubleToString(bit_str, (int)bits);
    NStr::DoubleToString(evalue_str, evalue);
    
    CNcbiOstrstream ostream; 
    ostream << setprecision (2) << evalue;
    string formatted_evalue = CNcbiOstrstreamToString(ostream);
    info += " S=" + bit_str + " E=" + formatted_evalue;
    aln_info->info = info;
    aln_info->bits = bits;
  
}

CAlnGraphic::CAlnGraphic(const CSeq_align_set& seqalign, 
                         CScope& scope,
                         CRange<TSeqPos>* master_range)
    :m_AlnSet(&seqalign), m_Scope(&scope), m_MasterRange(master_range) {
    m_NumAlignToShow = 1200;
    m_View = eCompactView;
    m_BarHeight = e_Height4;
    m_ImagePath = "./";
    m_MouseOverFormName = "document.forms[0]";
    m_NumLine = 55;
    if (m_MasterRange) {
        if (m_MasterRange->GetFrom() >= m_MasterRange->GetTo()) {
            m_MasterRange = NULL;
        }
    }
}

CAlnGraphic::~CAlnGraphic(){
    //deallocate memory
    ITERATE(TAlnInfoListList, iter, m_AlninfoListList) {
        ITERATE(TAlnInfoList, iter2, **iter){
            delete (*iter2)->range;
            delete *iter2;
        }
        (*iter)->clear();
    }
    m_AlninfoListList.clear();
}


void CAlnGraphic::AlnGraphicDisplay(CNcbiOstream& out){
    /*Note we can't just show each alnment as we go because we will 
      need to put  all hsp's with the same id on one line*/
   
    TAlnInfoList* alninfo_list = NULL;
    bool is_first_aln = true;
    int num_align = 0;
    int master_len = 0;
    CNodeRef center; 
    CRef<CHTML_table> tbl_box;
    CHTML_tc* tbl_box_tc;

    tbl_box = new CHTML_table;  //draw a box around the graphics
    center = new CHTML_center;  //align all graphic in center
  
    tbl_box->SetCellSpacing(0)->SetCellPadding(10)->SetAttribute("border", "1");    
    tbl_box->SetAttribute("bordercolorlight", "#0000FF");
    tbl_box->SetAttribute("bordercolordark", "#0000FF");
  
    CConstRef<CSeq_id> previous_id, subid, master_id;
    for (CSeq_align_set::Tdata::const_iterator iter = m_AlnSet->Get().begin(); 
         iter != m_AlnSet->Get().end() && num_align < m_NumAlignToShow; 
         iter++, num_align++){
        if(!alninfo_list){
            alninfo_list = new TAlnInfoList; 
        }
        //get start and end seq position for master sequence
        CRange<TSeqPos>* seq_range = new CRange<TSeqPos>((*iter)->GetSeqRange(0));;
        if (m_MasterRange) {           
            seq_range->Set(seq_range->GetFrom() - m_MasterRange->GetFrom(),
                           seq_range->GetTo() - m_MasterRange->GetFrom());
        } 
                
        //for minus strand
        if(seq_range->GetFrom() > seq_range->GetTo()){
            seq_range->Set(seq_range->GetTo(), seq_range->GetFrom());
        }
        subid = &((*iter)->GetSeq_id(1));
        if(is_first_aln) {
            master_id =  &((*iter)->GetSeq_id(0));
            const CBioseq_Handle& handle = m_Scope->GetBioseqHandle(*master_id);
            if(!handle){
                NCBI_THROW(CException, eUnknown, "Master sequence is not found!");
            }
            if (m_MasterRange) {
                master_len = m_MasterRange->GetLength();
            } else {
                master_len = handle.GetBioseqLength();
            }
            x_DisplayMaster(master_len, &(*center), &(*tbl_box), tbl_box_tc);
        }
        if(!is_first_aln && !subid->Match(*previous_id)) {
            //this aln is a new id, show result for previous id
            //save ranges for the same seqid
            m_AlninfoListList.push_back(alninfo_list);
            alninfo_list =  new TAlnInfoList;
        }
        SAlignInfo* alninfo = new SAlignInfo;
        alninfo->range = seq_range;
        //get aln info 
        x_GetAlnInfo(**iter, *subid, alninfo);
        alninfo_list->push_back(alninfo);
        is_first_aln = false;
        previous_id = subid;
    }
 
    //save last set of seqs with the same id
    if(alninfo_list){
        m_AlninfoListList.push_back(alninfo_list);
    }
    //merge range for seq with the same id
    ITERATE(TAlnInfoListList, iter, m_AlninfoListList) {
        (*iter)->sort(FromRangeAscendingSort);
        //    x_MergeSameSeq(**iter);  
    }
    
    //merge non-overlapping range list so that they can be put on 
    //the same line to compress the results
    if(m_View & eCompactView){
        double pixel_factor = ((double)kMasterPixel)/master_len;

        x_MergeDifferentSeq(pixel_factor);  
    }
    //display the graphic
    x_BuildHtmlTable(master_len, &(*tbl_box), tbl_box_tc);
    center->AppendChild(&(*tbl_box));
    center->Print(out);
    CHTML_hr hr;
    hr.Print(out);
}

//print score legend, master label
void CAlnGraphic::x_PrintTop (CNCBINode* center, CHTML_table* tbl_box, CHTML_tc*& tbl_box_tc){
    if(m_View & eMouseOverInfo){
     
        CRef<CHTML_input> textbox(new CHTML_input("text", "defline"));
        CNodeRef centered_text;       
        textbox->SetAttribute("size", 85);
        textbox->SetAttribute("value", 
                              "Mouse over to see the defline, click to show alignments");
        center->AppendChild(&(*textbox));
      
    }
   
    //score legend graph
    CRef<CHTML_table> tbl(new CHTML_table);
    CHTML_tc* tc;
    tbl->SetCellSpacing(1)->SetCellPadding(0)->SetAttribute("border", "0");
    CRef<CHTML_img> score_margin_img(new CHTML_img(m_ImagePath + kGifWhite,
                                                   kScoreMargin, m_BarHeight));
    score_margin_img->SetAttribute("alt","");
 
    tc = tbl->InsertAt(0, 0, score_margin_img);
    tc->SetAttribute("align", "LEFT");
    tc->SetAttribute("valign", "CENTER");

    CRef<CHTML_img> score(new CHTML_img(m_ImagePath + kGifScore, kScoreLength,
                                        kScoreHeight));
    score->SetAttribute("alt","");
    tc = tbl->InsertAt(0, 1, score);
    tc->SetAttribute("align", "LEFT");
    tc->SetAttribute("valign", "CENTER");
  
    tbl_box_tc = tbl_box->InsertAt(0, 0, tbl);
    tbl_box_tc->SetAttribute("align", "LEFT");
    tbl_box_tc->SetAttribute("valign", "CENTER");
    //master graph
  
    tbl = new CHTML_table;
    tbl->SetCellSpacing(1)->SetCellPadding(0)->SetAttribute("border", "0");
   
    CRef<CHTML_img> master(new CHTML_img(m_ImagePath + kGifMaster,
                                         kMasterBarLength, kMasterHeight));
    
    master->SetAttribute("alt","");
    tc = tbl->InsertAt(0, 0, master);
    tc->SetAttribute("align", "LEFT");
    tc->SetAttribute("valign", "CENTER");
    tbl_box_tc->AppendChild(&(*tbl));
   
}

void CAlnGraphic::x_DisplayMaster(int master_len, CNCBINode* center, CHTML_table* tbl_box, CHTML_tc*& tbl_box_tc){
    x_PrintTop(&(*center), &(*tbl_box), tbl_box_tc);
 
    //scale 
    CRef<CHTML_table> tbl(new CHTML_table);
    CHTML_tc* tc;
    CRef<CHTML_img> image;
    int column = 0;
    tbl->SetCellSpacing(0)->SetCellPadding(0)->SetAttribute("border", "0");
    image = new CHTML_img(m_ImagePath + kGifWhite, kScoreMargin + kScaleMarginAdj, m_BarHeight);
    image->SetAttribute("alt","");
    tc = tbl->InsertAt(0, column, image);
    tc->SetAttribute("align", "LEFT");
    tc->SetAttribute("valign", "CENTER");
    column ++;
    //first scale 
    image = new CHTML_img(m_ImagePath + kGifScale, kScaleWidth, kScaleHeight);
    image->SetAttribute("alt","");
    tc = tbl->InsertAt(0, column, image);
    tc->SetAttribute("align", "LEFT");
    tc->SetAttribute("valign", "CENTER");
    column ++;

    //second scale mark and on 
    float scale_unit = ((float)(master_len))/(kNumMark - 1);
    int round_number = s_GetRoundNumber((int)scale_unit);
    int spacer_length;
    double pixel_factor = ((double)kMasterPixel)/master_len;

    for (int i = 1; i*round_number <= master_len; i++) {
        spacer_length = (int)(pixel_factor*round_number) - kScaleWidth;
        image = new CHTML_img(m_ImagePath + kGifWhite, spacer_length, m_BarHeight);
        image->SetAttribute("alt","");
        tc = tbl->InsertAt(0, column, image);
        tc->SetAttribute("align", "LEFT");
        tc->SetAttribute("valign", "CENTER");
        column ++;  
        image = new CHTML_img(m_ImagePath + kGifScale, kScaleWidth, kScaleHeight);
        image->SetAttribute("alt","");
        tc = tbl->InsertAt(0, column, image);
        tc->SetAttribute("align", "LEFT");
        tc->SetAttribute("valign", "CENTER");
        column ++; 
    }
    tbl_box_tc->AppendChild(&(*tbl));

    //digits 
    //first scale digit
    string digit_str, previous_digitstr, first_digit_str;

    int first_digit = 0;
    column = 0;
    first_digit = m_MasterRange ? m_MasterRange->GetFrom() : 0;
    first_digit_str = NStr::IntToString(first_digit + 1);

    image = new CHTML_img(m_ImagePath + kGifWhite, 
                          kScoreMargin - 
                          kDigitWidth*((int)(first_digit_str.size()/2)),
                          m_BarHeight);
    image->SetAttribute("alt","");
    tbl = new CHTML_table;
    tbl->SetCellSpacing(0)->SetCellPadding(0)->SetAttribute("border", "0");
    tc = tbl->InsertAt(0, column, image);
    tc->SetAttribute("align", "LEFT");
    tc->SetAttribute("valign", "CENTER");
    column ++;
 
    //inserting actual digits
    for(size_t j = 0; j < first_digit_str.size(); j ++){
        string one_digit(1, first_digit_str[j]); 
        int digit = NStr::StringToInt(one_digit);
        image = new CHTML_img(m_ImagePath + kDigitGif[digit], kDigitWidth, 
            kDigitHeight,NStr::IntToString(digit));
        image->SetAttribute("alt","");
        tc = tbl->InsertAt(0, column, image);
        tc->SetAttribute("align", "LEFT");
        tc->SetAttribute("valign", "CENTER");
        column ++;
    }  
    

    previous_digitstr = first_digit_str;
  
    //print scale digits from second mark and on
    for (TSeqPos i = 1; (int)i*round_number <= master_len; i++) {
       
        digit_str = NStr::IntToString(i*round_number + 
                                      (m_MasterRange ?
                                       m_MasterRange->GetFrom() : 0)); 

        spacer_length = (int)(pixel_factor*round_number) 
            - kDigitWidth*(previous_digitstr.size() 
                           - previous_digitstr.size()/2) 
            - kDigitWidth*(digit_str.size()/2);
        previous_digitstr = digit_str;

        image = new CHTML_img(m_ImagePath + kGifWhite, spacer_length, m_BarHeight);
        image->SetAttribute("alt","");
        tc = tbl->InsertAt(0, column, image);
        tc->SetAttribute("align", "LEFT");
        tc->SetAttribute("valign", "CENTER");
        column ++;
        //digits
        for(size_t j = 0; j < digit_str.size(); j ++){
            string one_digit(1, digit_str[j]); 
            int digit = NStr::StringToInt(one_digit);
            image = new CHTML_img(m_ImagePath + kDigitGif[digit], kDigitWidth, 
                                  kDigitHeight,NStr::IntToString(digit));
            image->SetAttribute("alt","");
            tc = tbl->InsertAt(0, column, image);
            tc->SetAttribute("align", "LEFT");
            tc->SetAttribute("valign", "CENTER");
            column ++;
        }  
    }

    tbl_box_tc->AppendChild(&(*tbl));
    CRef<CHTML_br> br(new CHTML_br);
    tbl_box_tc->AppendChild(br);
}

//alignment bar graph
void CAlnGraphic::x_BuildHtmlTable(int master_len, CHTML_table* tbl_box, CHTML_tc*& tbl_box_tc) {
    CHTML_tc* tc;
    double pixel_factor = ((double)kMasterPixel)/master_len;
    int count = 0;
    //print out each alignment
    ITERATE(TAlnInfoListList, iter, m_AlninfoListList){  //iter =  each line
        if(count > m_NumLine){
            break;
        }
        CRef<CHTML_table> tbl;  //each table represents one line
        CRef<CHTML_img> image;
        CConstRef<CSeq_id> previous_id, current_id;
        CRef<CHTML_a> ad;
        double temp_value, temp_value2 ;
        int previous_end = -1;
        int front_margin = 0;
        int bar_length = 0;
        int column = 0;
        double prev_round = 0;
        

        if(!(*iter)->empty()){ //table for starting white spacer
            count ++;
            tbl = new CHTML_table;
            tbl->SetCellSpacing(0)->SetCellPadding(0)->SetAttribute("border", "0");  
            image = new CHTML_img(m_ImagePath + kGifWhite, kScoreMargin, m_BarHeight);
            image->SetAttribute("alt","");
            tc = tbl->InsertAt(0, column, image);
            column ++;
            tc->SetAttribute("align", "LEFT");
            tc->SetAttribute("valign", "CENTER");
        }

        bool is_first_segment = true;
        ITERATE(TAlnInfoList, iter2, **iter){ //iter2 = each alignments on one line
            current_id = (*iter2)->id;
            //white space in front of this alignment
            //need to take into account of previous round as
            //even 1 pixel difference would show up
            int from = (*iter2)->range->GetFrom();
            int stop = (*iter2)->range->GetTo();
            if (from <= previous_end) { //some hits overlap
                if (stop > previous_end) {
                    from = previous_end + 1;
                    (*iter2)->range->SetFrom(from);
                } else {
                    continue;  //this hsp is contained in previous hsp
                }
            }
            
            int break_len = from - (previous_end + 1); //distance between two hsp
            bool break_len_added = false;
            temp_value = break_len*pixel_factor + prev_round;
            
            //rounding to int this way as round() is not portable
            front_margin = (int)(temp_value + (temp_value < 0.0 ? -0.5 : 0.5));
            if (front_margin > 0) {
                prev_round = temp_value - front_margin;
            } else {
                prev_round = temp_value;
            }

            //the alignment box
            temp_value2 = (*iter2)->range->GetLength()*pixel_factor + 
                prev_round;
            bar_length = (int)(temp_value2 + (temp_value2 < 0.0 ? -0.5 : 0.5));
            
            //no round if bar itself is more than 0.5 pixel
            if(bar_length == 0 && (*iter2)->range->GetLength()*pixel_factor >= 0.50){
                bar_length = 1;
            }
          
            if (bar_length > 0) {
                prev_round = temp_value2 - bar_length;
            } else {
                prev_round = temp_value2;
            }
            

            if (!is_first_segment && front_margin == 0 && bar_length > 1) {
                front_margin = 1;  //show break for every segment that is > 1 pixel
                bar_length --; //minus the added break len between two segments
                break_len_added = true;
            }
            
            //Need to add white space
            if(front_margin > 0) {
                //connecting the alignments with the same id
                if(m_View & eCompactView && !previous_id.Empty() 
                   && previous_id->Match(*current_id)){
                    if (break_len_added) {
                        image = new CHTML_img(m_ImagePath + kGifBlack, front_margin,
                                              kGreakHeight);
                    } else {
                        image = new CHTML_img(m_ImagePath + kGifGrey, front_margin,
                                          kGapHeight);
                    }
                } else {
                    
                    image = new CHTML_img(m_ImagePath + kGifWhite, front_margin,
                                          m_BarHeight);
                }
                image->SetAttribute("alt","");
                tc = tbl->InsertAt(0, column, image);
                column ++;
                tc->SetAttribute("align", "LEFT");
                tc->SetAttribute("valign", "CENTER");
            }
            
            previous_end = stop;
            previous_id = current_id;
          
            if(bar_length > 0){
                is_first_segment = false;
                image = new CHTML_img(m_ImagePath + s_GetGif((int)(*iter2)->bits), 
                    bar_length, m_BarHeight,"score " + NStr::IntToString((int)(*iter2)->bits));
                image->SetAttribute("border", 0);
                if(m_View & eMouseOverInfo){
                    image->SetAttribute("ONMouseOver", m_MouseOverFormName 
                                        + ".defline.value=" + "'" + 
                                        NStr::JavaScriptEncode((*iter2)->info)
                                        + "'");
                    image->SetAttribute("ONMOUSEOUT", m_MouseOverFormName + 
                                        ".defline.value='Mouse-over to show defline and scores, click to show alignments'");
                }
                if(m_View & eAnchorLink){
                    string acc;
                    (*iter2)->id->GetLabel(&acc, CSeq_id::eContent, 0);
                    string seqid = (*iter2)->gi == 
                        0 ? acc : NStr::IntToString((*iter2)->gi);
                    ad = new CHTML_a("#" + seqid, image);
                    tc = tbl->InsertAt(0, column, ad);
                } else {
                    tc = tbl->InsertAt(0, column, image);
                }
                column ++;
                tc->SetAttribute("valign", "CENTER");
                tc->SetAttribute("align", "LEFT");     
            }
            
        }
        if(!tbl.Empty()){   
            tbl_box_tc->AppendChild(&(*tbl));
            //table for space between bars
            tbl = new CHTML_table;
            image = new CHTML_img(m_ImagePath + kGifWhite, kScoreMargin 
                                  + kScoreLength, kBlankBarHeight);
            image->SetAttribute("alt","");
            tbl->SetCellSpacing(0)->SetCellPadding(0)->SetAttribute("border", "0");  
            tbl->InsertAt(0, 0, image);
            tbl_box_tc->AppendChild(&(*tbl));
        }
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE
