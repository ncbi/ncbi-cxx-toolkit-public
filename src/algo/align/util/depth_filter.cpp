/*  $Id$
 * ===========================================================================
 *
 *                            PUBliC DOMAIN NOTICE
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
 * Author:  Nathan Bouk
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <algo/align/util/depth_filter.hpp>


using namespace ncbi;
using namespace objects;





struct SRangeDepth {
    SRangeDepth(TSeqRange range) : Range(range), Depth(1) { ; }  
    SRangeDepth(TSeqRange range, size_t depth) : Range(range), Depth(depth) { ; }  

    TSeqRange Range;
    size_t Depth;
};
bool operator<(const SRangeDepth& A, const SRangeDepth& B) {
    if(A.Range.GetFrom() != B.Range.GetFrom())
        return A.Range.GetFrom() < B.Range.GetFrom();
    else
        return A.Range.GetTo() < B.Range.GetTo();
}


class CDepthCollection {
public:
    CDepthCollection() { ; }

    void AddRange(TSeqRange NewRange);
   
    void CollapseOnDepth(size_t CollapseDepth);

    size_t MinDepthForRange(TSeqRange CheckRange) const;
   
    void ZeroFill();

    void CalcStats() const;

    void Print() const { 
        ITERATE(vector<SRangeDepth>, RangeIter, x_Ranges) {
            cerr << RangeIter->Range << " : " << RangeIter->Depth << endl;
        }
    }
private:

    vector<SRangeDepth> x_Ranges;   

};

void CDepthCollection::AddRange(TSeqRange NewRange) 
{
    if(x_Ranges.empty()) {
        SRangeDepth NewRangeDepth(NewRange);
        x_Ranges.push_back(NewRangeDepth);
        return;
    }

    vector<SRangeDepth> News;
    TSeqRange RemainRange = NewRange;
    
    ERASE_ITERATE(vector<SRangeDepth>, RangeDepthIter, x_Ranges) {
        
        if(RemainRange.Empty()) {
            break;
        }

        if( RangeDepthIter->Range == RemainRange) {
            RangeDepthIter->Depth++;
            RemainRange = TSeqRange();
            break;
        }

        // past it
        if(RemainRange.GetTo() < RangeDepthIter->Range.GetFrom()) {
            break;
        }

        bool EraseCurr = false;
        TSeqRange Inter = RangeDepthIter->Range.IntersectionWith(RemainRange);

        if(Inter.NotEmpty()) {
            SRangeDepth InterRangeDepth(Inter, RangeDepthIter->Depth + 1);
            News.push_back(InterRangeDepth);
            EraseCurr = true;
        
            
            if(RemainRange.GetFrom() < Inter.GetFrom()) {
                TSeqRange NewPrevRange;
                NewPrevRange.SetFrom(RemainRange.GetFrom());
                NewPrevRange.SetTo(Inter.GetFrom()-1);
                SRangeDepth NewPrev(NewPrevRange, 1);
                News.push_back(NewPrev);
            } 
            if(RemainRange.GetTo() >= Inter.GetTo()) {
                TSeqRange NewAfterRange;
                NewAfterRange.SetFrom(Inter.GetTo()+1);
                NewAfterRange.SetTo(RemainRange.GetTo());
                RemainRange = NewAfterRange;
            }
            
            if(RangeDepthIter->Range.GetFrom() < Inter.GetFrom()) {
                TSeqRange OldPrevRange;
                OldPrevRange.SetFrom(RangeDepthIter->Range.GetFrom());
                OldPrevRange.SetTo(Inter.GetFrom()-1);
                SRangeDepth OldPrev(OldPrevRange, RangeDepthIter->Depth);
                News.push_back(OldPrev);
            } 
            if(RangeDepthIter->Range.GetTo() > Inter.GetTo()) {
                TSeqRange OldAfterRange;
                OldAfterRange.SetFrom(Inter.GetTo()+1);
                OldAfterRange.SetTo(RangeDepthIter->Range.GetTo());
                SRangeDepth OldAfter(OldAfterRange, RangeDepthIter->Depth);
                News.push_back(OldAfter);
            //    VECTOR_ERASE(RangeDepthIter, x_Ranges);
            //    break;
            } 
        }
        
        if(EraseCurr) {
            VECTOR_ERASE(RangeDepthIter, x_Ranges);
        }
    }
    
    if(RemainRange.NotEmpty()) {
        SRangeDepth RemainRangeDepth(RemainRange);
        x_Ranges.push_back(RemainRangeDepth);
    }


    x_Ranges.insert(x_Ranges.end(), News.begin(), News.end());
    sort(x_Ranges.begin(), x_Ranges.end());
}


void CDepthCollection::CollapseOnDepth(size_t CollapseDepth)
{
    vector<SRangeDepth> Result;
    
    ITERATE(vector<SRangeDepth>, RangeDepthIter, x_Ranges) {
    
        // break the merging on empty results, 
        // or the segments aren't adjacent
        if (Result.empty() ||             
           !RangeDepthIter->Range.AbuttingWith( Result.back().Range )) {
            Result.push_back(*RangeDepthIter);
            continue;
        }
        
        // or when the two are not on the same side of the line
        if ((Result.back().Depth <= CollapseDepth) ^
            (RangeDepthIter->Depth <= CollapseDepth)) { 
            Result.push_back(*RangeDepthIter);
            continue;
        }
        
        // merge both unders, or both overs, together
        Result.back().Depth = min(Result.back().Depth, RangeDepthIter->Depth);
        Result.back().Range += RangeDepthIter->Range;
    }
    
    x_Ranges.swap(Result);
    sort(x_Ranges.begin(), x_Ranges.end());
}


size_t CDepthCollection::MinDepthForRange(TSeqRange CheckRange) const
{
    size_t Result = numeric_limits<size_t>::max();
    ITERATE(vector<SRangeDepth>, RangeIter, x_Ranges) {
        if(CheckRange.IntersectingWith(RangeIter->Range)) {
            Result = min(Result, RangeIter->Depth);
        }
    }
    return Result;
}

void CDepthCollection::ZeroFill() 
{
    vector<SRangeDepth> News;
    TSeqRange Prev = x_Ranges.front().Range;
    
    ITERATE(vector<SRangeDepth>, RangeIter, x_Ranges) {
        
        if (Prev != RangeIter->Range &&
            !Prev.AbuttingWith(RangeIter->Range)) {
            TSeqRange Zero;
            Zero.SetFrom(Prev.GetTo()+1);
            Zero.SetTo(RangeIter->Range.GetFrom()-1);
            SRangeDepth ZeroRD(Zero, 0);
            News.push_back(ZeroRD);
        }
        Prev = RangeIter->Range;
    }
    
    x_Ranges.insert(x_Ranges.end(), News.begin(), News.end());
    sort(x_Ranges.begin(), x_Ranges.end());
}

void CDepthCollection::CalcStats() const 
{
    size_t AccumBases = 0;
    size_t AccumBaseDepth = 0;
    size_t AccumRangeDepth = 0;
    size_t RangeCount = 0;
    
    vector<size_t> SortBaseDepths;
    vector<size_t> SortRangeDepths;

    // mean mode median
    ITERATE(vector<SRangeDepth>, RangeDepthIter, x_Ranges) {
        AccumBases += RangeDepthIter->Range.GetLength();
        AccumBaseDepth += (RangeDepthIter->Range.GetLength() * RangeDepthIter->Depth);
        AccumRangeDepth += RangeDepthIter->Depth;
        RangeCount++;

        for(size_t i = 0; i < RangeDepthIter->Range.GetLength(); i++) {
            SortBaseDepths.push_back(RangeDepthIter->Depth);
        }
        SortRangeDepths.push_back(RangeDepthIter->Depth);
    }

    double BaseMean = (double(AccumBaseDepth) / double(AccumBases));
    double RangeMean = (double(AccumRangeDepth) / double(RangeCount));

    sort(SortBaseDepths.begin(), SortBaseDepths.end());
    sort(SortRangeDepths.begin(), SortRangeDepths.end());

    size_t BaseMedian = SortBaseDepths[SortBaseDepths.size()/2];
    size_t RangeMedian = SortRangeDepths[SortRangeDepths.size()/2];

    size_t PrevValue = SortRangeDepths.front();
    size_t PrevCounts = 0;
    size_t BestValue=PrevValue, BestCounts=0;
    ITERATE(vector<size_t>, ValueIter, SortRangeDepths) {
        if( (*ValueIter) == PrevValue) {
            PrevCounts++;
        } else {
            if(PrevCounts > BestCounts) {
                BestValue = PrevValue;
                BestCounts = PrevCounts;
            }
            PrevValue = *ValueIter;
            PrevCounts = 1;
        }
    }
    size_t RangeMode = BestValue;

    PrevValue = SortBaseDepths.front();
    PrevCounts = 0;
    BestValue=PrevValue, BestCounts=0;
    ITERATE(vector<size_t>, ValueIter, SortBaseDepths) {
        if( (*ValueIter) == PrevValue) {
            PrevCounts++;
        } else {
            if(PrevCounts > BestCounts) {
                BestValue = PrevValue;
                BestCounts = PrevCounts;
            }
            PrevValue = *ValueIter;
            PrevCounts = 1;
        }
    }
    size_t BaseMode = BestValue;

    
    cerr << "BaseMean:    " << BaseMean << endl;
    cerr << "RangeMean:   " << RangeMean << endl;
    cerr << "BaseMedian:  " << BaseMedian << endl;
    cerr << "RangeMedian: " << RangeMedian << endl;
    cerr << "BaseMode:    " << BaseMode << endl;
    cerr << "RangeMode:   " << RangeMode << endl;

}

BEGIN_NCBI_SCOPE;

typedef list<CRef<CSeq_align> > TAlignList;

void 
CAlignDepthFilter::FilterOneRow(const list<CRef<CSeq_align> >& Input, 
                                      list<CRef<CSeq_align> >& Output, 
                                      int FilterOnRow, 
                                      size_t DepthCutoff, 
                                      double PctIdentRescue)
{
    //int FilterOnRow = 1; // filter on Query or Subjt coverage
    //double PctIdentRescue = 97.0; // >= Keep, pre-empting DepthCutoff. 
    //TSeqPos DepthCutoff = 3; // <= Keep, > Toss.

    CDepthCollection Depths;

    // calculate depths
    ITERATE(TAlignList, AlignIter, Input) {
        TSeqRange Range = (*AlignIter)->GetSeqRange(FilterOnRow);
        Depths.AddRange(Range);
    }
    Depths.ZeroFill();
    Depths.CollapseOnDepth(DepthCutoff);

    //Depths.Print();
    //Depths.CalcStats();


    // filter into output
    ITERATE(TAlignList, AlignIter, Input) {
        TSeqRange Range = (*AlignIter)->GetSeqRange(FilterOnRow);
        
        TSeqPos CurrDepth = Depths.MinDepthForRange(Range);

        if(CurrDepth <= DepthCutoff) {
            Output.push_back(*AlignIter);
        } else {
            // (make an exception for very high pct-ident alignments)
            // the region might have one perfect hit under all the cruft
            double PctIdentUngap = 0.0;
            bool HasScore = (*AlignIter)->GetNamedScore(CSeq_align::eScore_PercentIdentity_Ungapped, PctIdentUngap);
            if(!HasScore) {
                int NumIdent = 0;
                HasScore = (*AlignIter)->GetNamedScore(CSeq_align::eScore_IdentityCount, NumIdent);
                if(HasScore) {
                    TSeqPos AlignLen = (*AlignIter)->GetAlignLength(false);
                    PctIdentUngap = (double(NumIdent) / AlignLen) * 100.0;
                }
            }

            if(PctIdentUngap >= PctIdentRescue) {
                Output.push_back(*AlignIter);
            }
        }
    }

}


void 
CAlignDepthFilter::FilterBothRows(const list<CRef<CSeq_align> >& Input, 
                                        list<CRef<CSeq_align> >& Output, 
                                        size_t DepthCutoff, double PctIdentRescue)
{
    TAlignList FilteredQ, FilteredS;
    
    FilterOneRow(Input, FilteredQ, 0, DepthCutoff, PctIdentRescue);
    FilterOneRow(Input, FilteredS, 1, DepthCutoff, PctIdentRescue);

    // Walk the input in unison with the pair of intermediate filtered
    // lists (sharing the same collation), and emit results in common.
    TAlignList::const_iterator AlignIterQ = FilteredQ.begin(),
                               EndQ = FilteredQ.end();
    TAlignList::const_iterator AlignIterS = FilteredS.begin(),
                               EndS = FilteredS.end();
    ITERATE(TAlignList, AlignIter, Input) {
        // Break if we reach the end of a filtered list.
        if (AlignIterQ == EndQ  ||  AlignIterS == EndS) break;
        // Emit result if it's in common with both filtered lists.
        if (*AlignIter == *AlignIterQ  &&  *AlignIter == *AlignIterS) {
            Output.push_back(*AlignIter);
        }
        // Move to next in the filtered lists, if this alignment was present.
        if (*AlignIter == *AlignIterQ) ++AlignIterQ;
        if (*AlignIter == *AlignIterS) ++AlignIterS;
    }
}

END_NCBI_SCOPE

