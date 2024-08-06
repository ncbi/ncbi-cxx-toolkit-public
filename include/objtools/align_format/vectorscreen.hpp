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
 * @file vectorscreen.hpp
 *   vector screen display (using HTML table) 
 *
 */

#ifndef OBJTOOLS_ALIGN_FORMAT___VECTORSCREEN_HPP
#define OBJTOOLS_ALIGN_FORMAT___VECTORSCREEN_HPP

#include <html/html.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objmgr/scope.hpp>
#include <util/range.hpp>
#include <objtools/align_format/align_format_util.hpp>

BEGIN_NCBI_SCOPE 
BEGIN_SCOPE(align_format)


/**
 * Example:
 * @code
 * CVecscreen vec(align_set, master_length);
 * vec.SetImagePath("images/");
 * CRef<objects::CSeq_align_set> temp_aln = vec.ProcessSeqAlign();
 * vec.VecscreenPrint(out);
 * @endcode
 */

class NCBI_ALIGN_FORMAT_EXPORT CVecscreen {
public:
  
    ///vector match defines
    enum MatchType{
        eStrong = 0,
        eModerate,
        eWeak,
        eSuspect,
        eNoMatch
    };

    vector<CRef<objects::CSeq_align> > x_OrigAlignsById;
    typedef map<int, list<int> > TIdToDropIdMap;
    TIdToDropIdMap x_IdToDropIdMap;
    typedef map<int, int > TDropToKeepMap;
    TDropToKeepMap x_DropToKeepMap;

    int x_GetId(const objects::CSeq_align& a) {
        int id=0;
        a.GetNamedScore("vs_id", id);
        return id;
    }

    void x_GetAllDropIdsForKeepId(int keep_id, set<int>& drop_ids) 
    {
        size_t prev_size = drop_ids.size();
        drop_ids.insert(x_IdToDropIdMap[keep_id].begin(), x_IdToDropIdMap[keep_id].end());
        
        while(prev_size != drop_ids.size()) {
            prev_size = drop_ids.size();
            ITERATE(set<int>, ci, drop_ids) {
                drop_ids.insert(x_IdToDropIdMap[*ci].begin(), x_IdToDropIdMap[*ci].end());
            }
        }
    }

    ///Match info
    struct AlnInfo {
        TSeqRange range;
        MatchType type;

        typedef list<CRef<objects::CSeq_align> > TAlignList;
        TAlignList align_parts;
        TAlignList align_drops;
        const TAlignList& get_aligns() const { return align_parts; }
        const TAlignList& get_drops() const { return align_drops; }
        void add_align(CRef<objects::CSeq_align> a) { align_parts.push_back(a); }
        void add_aligns(const TAlignList& al) {
            x_add_aligns(align_parts, al);
        }
        void add_drops(const TAlignList& al) {
            x_add_aligns(align_drops, al);
        }
        void x_add_aligns(TAlignList & dest, const TAlignList& al) {
            ITERATE(TAlignList, iter, al) {
                if( (*iter)->GetSeqRange(0).IntersectingWith(range) ) {
                    dest.push_back(*iter);
                }
            }
        }


        AlnInfo(TSeqRange r = TSeqRange::GetEmpty(), MatchType m = eNoMatch, const TAlignList& l=TAlignList()) 
            : range(r), type(m) { add_aligns(l); }
        /// to allow sorting in std::list
        int operator<(const AlnInfo& rhs) const {
            if (this == &rhs) {
                return 0;
            }
            int rv = static_cast<int>(this->type < rhs.type);
            if (rv == 0) {
                // range is the tie-breaker
                rv = this->range < rhs.range;
            }
            return rv;
        }
    };

    ///Constructors
    ///@param seqalign: alignment to show
    ///@param master_length: master seq length
    ///@param terminal_flexibility: wiggle room from sequence edge, default 25
    ///
    CVecscreen(const objects::CSeq_align_set& seqalign, TSeqPos master_length, TSeqPos terminal_flexibility=25);
    
    ///Destructor
    ~CVecscreen();
    
    ///Set path to pre-made image gif files with different colors
    ///@param path: the path.  i.e. "mypath/". Internal default "./"
    ///
    void SetImagePath(string path) { 
        m_ImagePath = path;
    }  
    
    ///provide url link to help docs.  Default is 
    ///"/VecScreen/VecScreen_docs.html"
    ///@param url: the url
    ///
    void SetHelpDocsUrl(string url) { 
        m_HelpDocsUrl = url;
    }  
       
    ///Do not show weak(eWeak) match
    void NoShowWeakMatch() {
        m_ShowWeakMatch = false;
    }
    
    ///Process alignment to show    
    ///@return: the processed seqalign ref
    /// 
    CRef<objects::CSeq_align_set> ProcessSeqAlign(void);

    ///return alignment info list
    ///@return: the info list
    ///
    const list<AlnInfo*>* GetAlnInfoList() const {
        return &m_AlnInfoList;
    }

    ///show alignment graphic view
    ///@param out: stream for display    
    ///
    void VecscreenPrint(CNcbiOstream& out);

    ///Returns a string concerning the strength of the match for a given enum value
    static string GetStrengthString(MatchType match_type);
 
protected:
    
    
    ///the current seqalign
    CConstRef<objects::CSeq_align_set> m_SeqalignSetRef;
    ///the processed seqalign
    CRef<objects::CSeq_align_set> m_FinalSeqalign;
    ///gif image file path
    string m_ImagePath;
    ///help url
    string m_HelpDocsUrl;
    ///master seq length
    TSeqPos m_MasterLen;
    ///internal match list
    list<AlnInfo*> m_AlnInfoList;
    ///Show weak match?
    bool m_ShowWeakMatch;
    // how close to an edge still counts as an 'edge', defaults to 25, 
    TSeqPos m_TerminalFlexibility;

    ///Sort on range from
    ///@param info1: the first range
    ///@param info2: the second range
    ///
    inline static bool FromRangeAscendingSort(AlnInfo* const& info1,
                                              AlnInfo* const& info2)
    {
        if (info1->range.GetFrom() == info2->range.GetFrom()){
            return info1->range.GetTo() < info2->range.GetTo();
        } else {
            return info1->range.GetFrom() < info2->range.GetFrom();
        }
    } 

    ///merge overlapping seqalign
    ///@param seqalign: the seqalign to merge
    ///
    void x_MergeSeqalign(objects::CSeq_align_set& seqalign);
   
    ///merge a seqalign if its range is in another seqalign
    ///@param seqalign: the seqalign to merge
    ///
    void x_MergeInclusiveSeqalign(objects::CSeq_align_set& seqalign);

    ///merge a seqalign if its range is in another higher ranked seqalign
    ///@param seqalign_higher: higher-ranked seqalign
    ///@param seqalign_lower: lower-ranked seqalign
    ///
    void x_MergeLowerRankSeqalign(objects::CSeq_align_set& seqalign_higher,
                                  objects::CSeq_align_set& seqalign_lower);


    void x_GetEdgeRanges(const objects::CSeq_align& seqalign,
                         TSeqPos master_len,
                         TSeqPos& start_edge,
                         TSeqPos& end_edge);
    ///Get match type
    ///@param seqalign: the seqalign
    ///@param master_len: the master seq length
    ///@return: the type
    ///
    MatchType x_GetMatchType(const objects::CSeq_align& seqalign,
                             TSeqPos master_len,
                             TSeqPos start_edge,
                             TSeqPos end_edge);

    ///Build non overlapping internal match list
    ///@param seqalign_vec: a vecter of catagorized seqalign set
    ///
    void x_BuildNonOverlappingRange(vector<CRef<objects::CSeq_align_set> > seqalign_vec);

    ///get align info
    ///@param from: align from
    ///@param to: align to
    ///@param type: the match type
    ///
    AlnInfo* x_GetAlnInfo(TSeqPos from, TSeqPos to, MatchType type, const AlnInfo::TAlignList aligns=AlnInfo::TAlignList() );

    ///Output the graphic
    ///@param out: the stream for output
    ///
    void x_BuildHtmlBar(CNcbiOstream& out);
};

END_SCOPE(align_format)
END_NCBI_SCOPE

#endif /* OBJTOOLS_ALIGN_FORMAT___VECTORSCREEN_HPP */
