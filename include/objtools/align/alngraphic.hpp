#ifndef ALNGRAPHIC_HPP
#define ALNGRAPHIC_HPP
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

#include <html/html.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objmgr/scope.hpp>
#include <util/range.hpp>

BEGIN_NCBI_SCOPE 
BEGIN_SCOPE(objects)

class NCBI_XALNTOOL_EXPORT CAlnGraphic{
public:
    //view options
    enum ViewOption {
        eCompactView = (1 << 0),    //put as many seq as possible on one line
                                    //default sequence with same id on one line
        eMouseOverInfo = (1 << 1),  //show info when mouse over alignment graph
        eAnchorLink = (1 << 2),      //quick link in blast resutl html page
        eAnchorLinkDynamic = (1 << 3) //quick link in blast resutl html page for new design
    };

    //graph bar height
    enum BarPixel {       
        e_Height3 = 3,          
        e_Height4 = 4,    //default 
        e_Height5 = 5,           
        e_Height6 = 6,           
        e_Height7 = 7
    };
    
    // Constructors
    CAlnGraphic(const CSeq_align_set& seqalign, 
                CScope& scope,
                CRange<TSeqPos>* master_range = NULL);

    // Destructor
    ~CAlnGraphic();

    //view in ViewOption. default eCompactView
    void SetViewOption(int option) { 
        m_View = option;
    }

    void SetImagePath(string path) { //i.e. "mypath/". defaul "./"
        m_ImagePath = path;
    }  

    void SetGraphBarHeight(BarPixel height) { 
        m_BarHeight = height;
    }

    //form name that include the text input area to show 
    //info, i.e. "document.forms[0]"
    void SetMouseOverFormName(string form_name) { 
        m_MouseOverFormName = form_name;
    }
    
    
    void SetOnClickFunctionName(string onClickFunction) { 
        m_onClickFunction = m_onClickFunction;
    }
    // Display top num seqalign
    void SetNumAlignToShow(int num) {  //internal default = 1000
        m_NumAlignToShow = num;
    }

    /*Number of maximal line to show. Note this is different than 
      alignment number as each line may contains > 1 alignment, especially
      when using eCompactView */
    void SetNumLineToShow(int num) { //internal default = 50
        m_NumLine = num;
    }

    //show alignment graphic view
    void AlnGraphicDisplay(CNcbiOstream& out);

private:
    struct SAlignInfo {
        CConstRef<CSeq_id> id;
        TGi gi;
        double bits;
        string info;
        CRange<TSeqPos>* range;
    };
    //callback for sorting range
    inline static bool FromRangeAscendingSort(SAlignInfo* const& info1,
                                              SAlignInfo* const& info2)
    {
        return info1->range->GetFrom() < info2->range->GetFrom();
    }
    
    typedef list<SAlignInfo*> TAlnInfoList;
    typedef list<TAlnInfoList*> TAlnInfoListList;
    CConstRef <CSeq_align_set> m_AlnSet; 
    CRef <CScope> m_Scope;
    int m_NumAlignToShow; 
    int m_View;
    int m_BarHeight;
    string m_ImagePath;
    string m_MouseOverFormName;   //the text input window to show mouseover info
    string m_onClickFunction;
    int m_NumLine;
    
    //blast sub-sequence query
    CRange<TSeqPos>* m_MasterRange; 

    TAlnInfoListList m_AlninfoListList;
    void x_DisplayMaster(int master_len, CNCBINode* center, 
                         CHTML_table* tbl_box, CHTML_tc*& tbl_box_tc);
    void x_GetAlnInfo(const CSeq_align& aln, const CSeq_id& id, 
                      SAlignInfo* aln_info);
    void x_BuildHtmlTable(int master_len, CHTML_table* tbl_box, 
                          CHTML_tc*& tbl_box_tc);
    CRange<TSeqPos>* x_GetEffectiveRange(TAlnInfoList& alninfo_list);
    void x_MergeDifferentSeq(double pixel_factor);
    void x_MergeSameSeq(TAlnInfoList& alninfo_list);
    void x_PrintTop (CNCBINode* center, CHTML_table* tbl_box, 
                     CHTML_tc*& tbl_box_tc);
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif
