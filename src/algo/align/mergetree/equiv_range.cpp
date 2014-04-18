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
 * Author:  Nathan Bouk
 *
 * File Description:
 *
 */


#include <objects/seq/seq_id_handle.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/scope.hpp>


#include <algo/align/mergetree/equiv_range.hpp>

USING_SCOPE(ncbi);
USING_SCOPE(objects);


BEGIN_SCOPE(ncbi);
CNcbiOstream& operator<<(CNcbiOstream& out, const CEquivRange& range)
{
	return out << range.Query 
		<< (range.Strand == eNa_strand_minus ? "-" : "+")
		<< " to " << range.Subjt << "+" 
        << " (" << range.AlignId
        << "," << range.SegmtId 
        << "," << range.SplitId << ")";
}

string s_RelToStr(CEquivRange::ERelative rel) {
    if(rel == CEquivRange::eWtf)
        return "WT";
   
    string Ret;
    if(rel & CEquivRange::eBefore)
        Ret = "B";
    else if(rel & CEquivRange::eAfter)
        Ret = "F";
    else if(rel & CEquivRange::eAbove)
        Ret = "A";
    else if(rel & CEquivRange::eUnder)
        Ret = "U";

    if(rel & CEquivRange::eIntersects)
        Ret += "I";
    else if(rel & CEquivRange::eInterQuery)
        Ret += "Q";
    else if(rel & CEquivRange::eInterSubjt)
        Ret += "S";

    return Ret;
}


bool operator==(const CEquivRange& A, const CEquivRange& B) {
    return (A.Query == B.Query && A.Subjt == B.Subjt);
}
	
bool operator<(const CEquivRange& A, const CEquivRange& B) {
    if(A.Query.GetFrom() != B.Query.GetFrom())
        return (A.Query.GetFrom() < B.Query.GetFrom());
    else if(A.Query.GetTo() != B.Query.GetTo())
        return (A.Query.GetTo() < B.Query.GetTo());
    else if(A.Subjt.GetFrom() != B.Subjt.GetFrom())
        return (A.Subjt.GetFrom() < B.Subjt.GetFrom());
    else if(A.Subjt.GetTo() != B.Subjt.GetTo())
        return (A.Subjt.GetTo() < B.Subjt.GetTo());
    else
        return A.Strand < B.Strand;
}



bool s_SortEquivBySubjt(const CEquivRange& A, const CEquivRange& B) {
    if(A.Subjt.GetFrom() != B.Subjt.GetFrom())
        return (A.Subjt.GetFrom() < B.Subjt.GetFrom());
    else if(A.Subjt.GetTo() != B.Subjt.GetTo())
        return (A.Subjt.GetTo() < B.Subjt.GetTo());
    else if(A.Query.GetFrom() != B.Query.GetFrom())
        return (A.Query.GetFrom() < B.Query.GetFrom());
    else if(A.Query.GetTo() != B.Query.GetTo())
        return (A.Query.GetTo() < B.Query.GetTo());
    else
        return A.Strand < B.Strand;
}



END_SCOPE(ncbi);


int CEquivRange::GAlignId = 0;
int CEquivRange::GSegmtId = 0;
int CEquivRange::GSplitId = 0;

CEquivRange 
CEquivRange::SliceOnQuery(const CRange<TSeqPos>& pQuery) const
{
	CEquivRange Slice;
	CRange<TSeqPos> Inter = Query.IntersectionWith(pQuery);
	
	Slice.Strand = Strand;
	Slice.Intercept = Intercept;
	Slice.Matches = 0;
    Slice.MisMatches = 0;
	if(Inter.Empty()) {
		Slice.Query = Inter;
		return Slice;
	} else if(Query == Inter) {
		return *this;
	}

    Slice.Query = Inter;
	TSeqPos SOffset = Inter.GetFrom() - Query.GetFrom();

	if(Strand == eNa_strand_plus) {
	    Slice.Subjt.SetFrom(Subjt.GetFrom() + SOffset);
		Slice.Subjt.SetLength(Inter.GetLength());
	} else {
		Slice.Subjt.SetTo(Subjt.GetTo() - SOffset);
		Slice.Subjt.SetLengthDown(Inter.GetLength());
	}
    
    ITERATE(vector<TSeqPos>, MisPointIter, MisMatchSubjtPoints) {
        if (Slice.Subjt.GetFrom() <= *MisPointIter &&
            Slice.Subjt.GetTo() >= *MisPointIter) {
            Slice.MisMatchSubjtPoints.push_back(*MisPointIter);
        }
    }
    Slice.MisMatches = Slice.MisMatchSubjtPoints.size();
    Slice.Matches = Slice.Subjt.GetLength() - Slice.MisMatches;

    Slice.AlignId = AlignId;
    Slice.SegmtId = SegmtId;
    Slice.SplitId = GSplitId; GSplitId++;

	return Slice;
}


CEquivRange 
CEquivRange::SliceOnSubjt(const CRange<TSeqPos>& pSubjt) const
{
	CEquivRange Slice;
	CRange<TSeqPos> Inter = Subjt.IntersectionWith(pSubjt);
	
	Slice.Strand = Strand;
	Slice.Intercept = Intercept;
	Slice.Matches = 0;
	Slice.MisMatches = 0;
    if(Inter.Empty()) {
		Slice.Subjt = Inter;
		return Slice;
	} else if(Subjt == Inter) {
		return *this;
	}

    Slice.Subjt = Inter;
    TSeqPos QOffset = Inter.GetFrom() - Subjt.GetFrom();
    		
	if(Strand == eNa_strand_plus) {
		Slice.Query.SetFrom(Query.GetFrom() + QOffset);
		Slice.Query.SetLength(Inter.GetLength());
	} else {
		Slice.Query.SetTo(Query.GetTo() - QOffset);
		Slice.Query.SetLengthDown(Inter.GetLength());
	}
 
    ITERATE(vector<TSeqPos>, MisPointIter, MisMatchSubjtPoints) {
        if (Slice.Subjt.GetFrom() <= *MisPointIter &&
            Slice.Subjt.GetTo() >= *MisPointIter) {
            Slice.MisMatchSubjtPoints.push_back(*MisPointIter);
        }
    }
    Slice.MisMatches = Slice.MisMatchSubjtPoints.size();
    Slice.Matches = Slice.Subjt.GetLength() - Slice.MisMatches;

    Slice.AlignId = AlignId;
    Slice.SegmtId = SegmtId;
    Slice.SplitId = GSplitId; GSplitId++;

	return Slice;
}


CEquivRange 
CEquivRange::Merge(const CEquivRange& Other) const
{
    CEquivRange Merged;

    Merged.Query = Query + Other.Query;
    Merged.Subjt = Subjt + Other.Subjt;

    Merged.Strand = Strand;
    Merged.Intercept = Intercept;
    Merged.Matches = Matches + Other.Matches;
    Merged.MisMatches = MisMatches + Other.MisMatches;

    Merged.MisMatchSubjtPoints.insert(Merged.MisMatchSubjtPoints.end(),
            MisMatchSubjtPoints.begin(), MisMatchSubjtPoints.end());
    Merged.MisMatchSubjtPoints.insert(Merged.MisMatchSubjtPoints.end(),
            Other.MisMatchSubjtPoints.begin(), Other.MisMatchSubjtPoints.end());

    Merged.AlignId = AlignId;
    Merged.SegmtId = SegmtId;
    Merged.SplitId = GSplitId; GSplitId++;
   
    if(Merged.Query.GetLength() != Merged.Subjt.GetLength())
       cerr << __LINE__ << " ERROR" << endl;
    if(Merged.MisMatchSubjtPoints.size() != (size_t)Merged.MisMatches) 
       cerr << __LINE__ << " ERROR" << endl;
    if(Merged.Query.GetLength() != (size_t)(Merged.Matches + Merged.MisMatches))
       cerr << __LINE__ << " ERROR" << endl;

    return Merged;
}


void CEquivRange::CalcMatches(CBioseq_Handle QueryHandle, 
							  CBioseq_Handle SubjtHandle) 
{
 	CSeqVector QueryVec(QueryHandle, CBioseq_Handle::eCoding_Iupac, Strand);
 	CSeqVector SubjtVec(SubjtHandle, CBioseq_Handle::eCoding_Iupac, eNa_strand_plus);

	string QueryStr, SubjtStr;
	if(Strand == eNa_strand_plus)
		QueryVec.GetSeqData(Query.GetFrom(), Query.GetTo()+1, QueryStr); 
	else
		QueryVec.GetSeqData(QueryVec.size()-Query.GetTo()-1, 
							QueryVec.size()-Query.GetFrom(), QueryStr); 
	SubjtVec.GetSeqData(Subjt.GetFrom(), Subjt.GetTo()+1, SubjtStr); 


	Matches = 0;
    MisMatches = 0;
	for(size_t Loop = 0; Loop < QueryStr.length(); Loop++) {
		if(QueryStr[Loop] == SubjtStr[Loop]) {
			Matches++;
        } else {
            MisMatches++;
            MisMatchSubjtPoints.push_back(Subjt.GetFrom()+Loop);
        }
	}

}


bool
CEquivRange::SplitIntersections(const TEquivList& Originals,
								TEquivList& Splits)
{
	//set<CEquivRange> Sources;
	//set<CEquivRange> Equivs;
	TEquivList Sources, Equivs;

    bool MadeCuts = false;
	do {
        MadeCuts = false;
		
		if(Equivs.empty()) {
			ITERATE(TEquivList, OrigIter, Originals) {
				//Sources.insert(*OrigIter);
			    Sources.push_back(*OrigIter);
            }
            TEquivList::iterator NE;
            sort(Sources.begin(), Sources.end());
            NE = unique(Sources.begin(), Sources.end());
		    Sources.erase(NE, Sources.end());
        }	
		else {
			Sources.clear();
			Equivs.swap(Sources);
			Equivs.clear();
		}

		//cerr << "IS: " << Equivs.size() << " : " << Sources.size() << endl;
		typedef vector<TSeqPos> TPointContainer;
		//typedef set<TSeqPos> TPointContainer;
        TPointContainer QueryPoints, SubjtPoints;
        QueryPoints.reserve(Sources.size()*2);
        SubjtPoints.reserve(Sources.size()*2);

		ITERATE(vector<CEquivRange>, SourceIter, Sources) {
		//	QueryPoints.insert(SourceIter->Query.GetFrom());
		//	QueryPoints.insert(SourceIter->Query.GetTo()+1);
		//	SubjtPoints.insert(SourceIter->Subjt.GetFrom());
		//	SubjtPoints.insert(SourceIter->Subjt.GetTo()+1);
			
            QueryPoints.push_back(SourceIter->Query.GetFrom());
			QueryPoints.push_back(SourceIter->Query.GetTo()+1);
			SubjtPoints.push_back(SourceIter->Subjt.GetFrom());
			SubjtPoints.push_back(SourceIter->Subjt.GetTo()+1);
        }

        {{
            vector<TSeqPos>::iterator NE;
            sort(QueryPoints.begin(), QueryPoints.end());
            NE = unique(QueryPoints.begin(), QueryPoints.end());
            QueryPoints.erase(NE, QueryPoints.end());
            sort(SubjtPoints.begin(), SubjtPoints.end());
            NE = unique(SubjtPoints.begin(), SubjtPoints.end());
            SubjtPoints.erase(NE, SubjtPoints.end());
        }}
		

        //cerr << "S" << endl;
        //cerr << CTime(CTime::eCurrent).AsString("m:s.l") << endl;
		
        
        ITERATE(TEquivList, SourceIter, Sources) {
            const CEquivRange& Equiv = *SourceIter;
           
            size_t JumpTo = 0;
            {{
                size_t F = 0, T = QueryPoints.size()-1;
                size_t I = 0;
                while(true) {
                    if(F+1 >= T) {
                        I = F;
                        break;
                    }
                    I = (F+((T-F)/2));
                    TSeqPos IV = QueryPoints[I];
                    //cerr << " F " << F << "  T " << T <<  "  I " << I << "  IV  " << IV << endl;
                    if( IV == Equiv.Query.GetFrom()) {
                        break;
                    } else if( Equiv.Query.GetFrom() < IV ) {
                        T = I;
                    } else {
                        F = I;
                    }
                }
                JumpTo = I;
            }}
            //cerr << "Q JumpTo: " << JumpTo << "\t" << Equiv << endl;

            //ITERATE(TPointContainer, StartIter, QueryPoints) {
            for(TPointContainer::const_iterator StartIter = QueryPoints.begin()+JumpTo;
                StartIter != QueryPoints.end(); ++StartIter) {
                
                TPointContainer::const_iterator Next = StartIter;
			    ++Next;
			    if(Next == QueryPoints.end())
				    continue;
                    
			    //CRange<TSeqPos> Range(*StartIter, *Next-1);
			    CRange<TSeqPos> Range(*StartIter, *Next-1);
		        Range.SetFrom(*StartIter);
                Range.SetTo(*Next-1);

			    if(Range.Empty())
				    continue;
                
                if(Range == Equiv.Query) {
                    Equivs.push_back(Equiv);
                } else if(Range.IntersectingWith(Equiv.Query)) {
                    CEquivRange Temp = Equiv.SliceOnQuery(Range);
				    //if(Temp.NotEmpty() && Temp.Matches > 0) {
				    if(Temp.NotEmpty()) {
                        Equivs.push_back(Temp);
                        MadeCuts = true;
                    }
                }

                if(*StartIter > Equiv.Query.GetTo()) {
                    break;
                }
            }
            
            {{
                size_t F = 0, T = SubjtPoints.size()-1;
                size_t I = 0;
                while(true) {
                    if(F+1 >= T) {
                        I = F;
                        break;
                    }
                    I = (F+((T-F)/2));
                    TSeqPos IV = SubjtPoints[I];
                    if( IV == Equiv.Subjt.GetFrom()) {
                        break;
                    } else if( Equiv.Subjt.GetFrom() < IV ) {
                        T = I;
                    } else {
                        F = I;
                    }
                }
                JumpTo = I;
            }}
            
            //ITERATE(TPointContainer, StartIter, SubjtPoints) {
    	    for(TPointContainer::const_iterator StartIter = SubjtPoints.begin()+JumpTo;
                StartIter != SubjtPoints.end(); ++StartIter) {
             
                TPointContainer::const_iterator Next = StartIter;
			    ++Next;
			    if(Next == SubjtPoints.end())
				    continue;
    
			    //CRange<TSeqPos> Range(*StartIter, *Next-1);
		        CRange<TSeqPos> Range(*StartIter, *Next-1);
		        Range.SetFrom(*StartIter);
                Range.SetTo(*Next-1);


			    if(Range.Empty())
				    continue;
                
                if(Range == Equiv.Subjt) {
                    Equivs.push_back(Equiv);
                } else if(Range.IntersectingWith(Equiv.Subjt)) {
                    CEquivRange Temp = Equiv.SliceOnSubjt(Range);
				    //if(Temp.NotEmpty() && Temp.Matches > 0) {
				    if(Temp.NotEmpty()) {
                        Equivs.push_back(Temp);
                        MadeCuts = true;
                    }
                }
                if(*StartIter > Equiv.Subjt.GetTo()) {
                    break;
                }
            }
        }

        /*
        cerr << "Q" << endl;
        cerr << CTime(CTime::eCurrent).AsString("m:s.l") << endl;
		TEquivList::iterator LastSourceStart;
        LastSourceStart = Sources.end();
        int LCI = 0;
        LCI = 0;
        ITERATE(TPointContainer, StartIter, QueryPoints) {

			TPointContainer::const_iterator Next = StartIter;
			++Next;
			if(Next == QueryPoints.end())
				continue;

			CRange<TSeqPos> Range(*StartIter, *Next-1);
		
			if(Range.Empty())
				continue;
            

            TEquivList::iterator SourceIter = LastSourceStart;
            LastSourceStart = Sources.end();
            if(SourceIter == Sources.end())
                SourceIter = Sources.begin();
            size_t CI = LCI;
            LCI = 0;
			//ITERATE(vector<CEquivRange>, SourceIter, Sources) {
			for( ; SourceIter != Sources.end(); ++SourceIter) { 
                
                if(*Next < SourceIter->Query.GetFrom()) {
                    continue;
                }
                
                if (LastSourceStart == Sources.end()) {
                    LastSourceStart = SourceIter;
                    LCI = CI;
                } 
                
                if(Range == SourceIter->Query) {
                    cerr << "QE : CI : " << CI << "\t" << Range << "\t" << SourceIter->Query << endl;
                    Equivs.push_back(*SourceIter);
                } else if(Range.IntersectingWith(SourceIter->Query)) {
                    cerr << "QI : CI : " << CI << "\t" << Range << "\t" << SourceIter->Query << endl;
                    CEquivRange Temp = SourceIter->SliceOnQuery(Range);
				    if(Temp.NotEmpty()) {
					    //Equivs.insert(Temp);
                        Equivs.push_back(Temp);
                    }
                } else if( *StartIter > SourceIter->Query.GetFrom() &&
                           *StartIter > LastSourceStart->Query.GetFrom() ) {
                    LastSourceStart = SourceIter;
                    ++LastSourceStart;
                    LCI = CI + 1;
                    break;
                }
                CI++;
			}
		}

        sort(Sources.begin(), Sources.end(), s_SortEquivBySubjt);
        
        cerr << "S" << endl;
        cerr << CTime(CTime::eCurrent).AsString("m:s.l") << endl;
		LastSourceStart = Sources.end();
		ITERATE(TPointContainer, StartIter, SubjtPoints) {

			TPointContainer::const_iterator Next = StartIter;
			++Next;
			if(Next == SubjtPoints.end())
			    continue;

			CRange<TSeqPos> Range(*StartIter, *Next-1);
		
			if(Range.Empty())
				continue;

            TEquivList::iterator SourceIter = LastSourceStart;
            LastSourceStart = Sources.end();
            if(SourceIter == Sources.end())
                SourceIter = Sources.begin();
    
            //ITERATE(vector<CEquivRange>, SourceIter, Sources) {
			for( ; SourceIter != Sources.end(); ++SourceIter) { 
                
                if(*Next < SourceIter->Subjt.GetFrom()) {
                    continue;
                }
                
                if (LastSourceStart == Sources.end()) {
                    LastSourceStart = SourceIter;
                }

                if(Range == SourceIter->Subjt) {
                    Equivs.push_back(*SourceIter);
                } else if(Range.IntersectingWith(SourceIter->Subjt)) {
                    CEquivRange Temp = SourceIter->SliceOnSubjt(Range);
				    if(Temp.NotEmpty()) {
					    //Equivs.insert(Temp);
                        Equivs.push_back(Temp);
                    }
                } else if( *StartIter > SourceIter->Query.GetFrom() &&
                           *StartIter > LastSourceStart->Query.GetFrom() ) {
                    LastSourceStart = SourceIter;
                    ++LastSourceStart;
                    continue;
                }
			}
		}
        */

        //cerr << "E" << endl;
        //cerr << CTime(CTime::eCurrent).AsString("m:s.l") << endl;

		//ITERATE(set<CEquivRange>, EquivIter, Equivs) {
	    //		cout << *EquivIter << endl;
		//}

        {{
            TEquivList::iterator NE;
            sort(Equivs.begin(), Equivs.end());
            NE = unique(Equivs.begin(), Equivs.end());
            Equivs.erase(NE, Equivs.end());
		    //cerr << "ES: " << Equivs.size() << " : " << Sources.size() << endl;
	    }}
    //} while(Equivs.size() > Sources.size());
    } while(MadeCuts);

	ITERATE(vector<CEquivRange>, EquivIter, Equivs) {
		Splits.push_back(*EquivIter);
	}

	return !Splits.empty();
}

bool
CEquivRange::MergeAbuttings(const TEquivList& Originals,
							TEquivList& Merges)
{
    bool MadeChanges = false;
    do {

        TEquivList Sources;
        Sources.clear();
        if(Merges.empty())
            Sources.insert(Sources.end(), Originals.begin(), Originals.end());
        else
            Sources.insert(Sources.end(), Merges.begin(), Merges.end());
        {{
            TEquivList::iterator NE;
            sort(Sources.begin(), Sources.end(), s_SortEquivBySubjt);
            NE = unique(Sources.begin(), Sources.end());
            Sources.erase(NE, Sources.end());
        }}
        
        MadeChanges = false;
        Merges.clear();
        
        CEquivRange Curr;
        ITERATE(TEquivList, SourceIter, Sources) {
            if(SourceIter->Empty()) {
                continue;
            }

            if(Curr.Empty()) {
                Curr = *SourceIter;
                continue;
            }

            if (Curr.AbuttingWith(*SourceIter) && 
                Curr.AlignId == SourceIter->AlignId &&
                Curr.SegmtId == SourceIter->SegmtId) {            
                Curr = Curr.Merge(*SourceIter);
                MadeChanges = true;
                continue;
            }

            else {
                Merges.push_back(Curr);
                Curr = *SourceIter;
            }

        }
        if(!Curr.Empty())
            Merges.push_back(Curr);

    } while(MadeChanges);

    return true;
}

CEquivRange::ERelative 
CEquivRange::CalcRelative(const CEquivRange& Check) const
{
/*
	To make a valid Denseg, all equivs must be before/after
	 each other, in the proper order.

				| 
				|  after
		above	|----
				/
			   /
			  /    under
      -------|
      before |
			 |
*/

	ERelative Result = eWtf;
    
    if(Empty() || Check.Empty())
        return eWtf;

	if(Strand != Check.Strand)
		Result = eWtf;
		
	if(IntersectingWith(Check))
		Result = eIntersects;
	
	if(Result != eWtf)
		goto end;

	if(Strand == eNa_strand_plus) {
		if (Check.Query.GetFrom() > Query.GetTo() &&
			Check.Subjt.GetFrom() > Subjt.GetTo())
			Result = eAfter;
		else if(Check.Query.GetTo() < Query.GetFrom() &&
				Check.Subjt.GetTo() < Subjt.GetTo())
			Result = eBefore;
		else if(Check.Query.GetFrom() > Query.GetFrom() &&
				Check.Subjt.GetTo()   < Subjt.GetTo())
			Result = eAbove;
		else if(Check.Query.GetTo()   < Query.GetTo() &&
				Check.Subjt.GetFrom() > Subjt.GetFrom())
			Result = eUnder;
	}
	else if(Strand == eNa_strand_minus) {
		if (Check.Query.GetFrom() < Query.GetTo() &&
			Check.Subjt.GetTo()   > Subjt.GetTo())
			Result = eBefore;
		else if(Check.Query.GetFrom() > Query.GetTo() &&
				Check.Subjt.GetTo()   < Subjt.GetFrom())
			Result = eAfter;
		else if(Check.Query.GetFrom() > Query.GetFrom() &&
				Check.Subjt.GetFrom() > Subjt.GetFrom())
			Result = eAbove;
		else if(Check.Query.GetTo() < Query.GetTo() &&
				Check.Subjt.GetTo() < Subjt.GetTo())
			Result = eUnder;
	}

end:
	if(Result == eWtf) {
		ERR_POST(Info << "CEquivRange::CalcRelative:: Got a eWTF (" 
				<< *this << ") vs (" << Check << ")" );
	}
	else {
	//	ERR_POST(Info << *this << " vs " << Check << " result: " << Result);
	}
	return Result;
}


CEquivRange::ERelative 
CEquivRange::CalcRelativeDuo(const CEquivRange& Check) const
{
/*
	To make a valid Denseg, all equivs must be before/after
	 each other, in the proper order.

             |	 | 
		above| IQ|  after
	  --------------------
             |I /|  
         IS  | / | IS  
			 |/ I|  
      -------|------------
      before | IQ|  under
			 |   |

    Only behaves reasonably if parts are split
*/

	ERelative Result = eWtf;

	if(Strand != Check.Strand) {
		Result = eWtf;
        return Result;
    }
	
    bool IQ, IS;

    IQ = Query.IntersectingWith(Check.Query);
    IS = Subjt.IntersectingWith(Check.Subjt);

	//if(IntersectingWith(Check))
	//	Result = eIntersects;
	
	//if(Result != eWtf)
	//	goto end;

	if(Strand == eNa_strand_plus) {
		if (Check.Query.GetFrom() > Query.GetTo() &&
			Check.Subjt.GetFrom() > Subjt.GetTo())
			Result = eAfter;
		else if(Check.Query.GetTo() < Query.GetFrom() &&
				Check.Subjt.GetTo() < Subjt.GetTo())
			Result = eBefore;
		else if(Check.Query.GetFrom() > Query.GetFrom() &&
				Check.Subjt.GetTo()   < Subjt.GetTo())
			Result = eAbove;
		else if(Check.Query.GetTo()   < Query.GetTo() &&
				Check.Subjt.GetFrom() > Subjt.GetFrom())
			Result = eUnder;
	}
	else if(Strand == eNa_strand_minus) {
		if (Check.Query.GetFrom() < Query.GetTo() &&
			Check.Subjt.GetTo()   > Subjt.GetTo())
			Result = eBefore;
		else if(Check.Query.GetFrom() > Query.GetTo() &&
				Check.Subjt.GetTo()   < Subjt.GetFrom())
			Result = eAfter;
		else if(Check.Query.GetFrom() > Query.GetFrom() &&
				Check.Subjt.GetFrom() > Subjt.GetFrom())
			Result = eAbove;
		else if(Check.Query.GetTo() < Query.GetTo() &&
				Check.Subjt.GetTo() < Subjt.GetTo())
			Result = eUnder;
	}

    if (IQ || IS)
        Result = (ERelative)(Result | eIntersects);
    else if (IQ)
        Result = (ERelative)(Result | eInterQuery);
    else if (IS)
        Result = (ERelative)(Result | eInterSubjt);

    //end:
	//if(Result == eWtf) {
	//	ERR_POST(Info << "CEquivRange::CalcRelative:: Got a eWTF (" 
	//			<< *this << ") vs (" << Check << ")" );
	//}
	//else {
	//	ERR_POST(Info << *this << " vs " << Check << " result: " << Result);
	//}
    //
    
	return Result;
}


void CEquivRange::ExtractRangesFromSeqAlign(const CSeq_align& Align, 
												 TEquivList& Equivs)
{
	if(Align.GetSegs().IsDisc()) {
		ITERATE(CSeq_align_set::Tdata, AlignIter, 
				Align.GetSegs().GetDisc().Get()) {
			ExtractRangesFromSeqAlign(**AlignIter, Equivs);
		}
		return;
	}
	
	const CDense_seg& Denseg = Align.GetSegs().GetDenseg();

	int SegCount = Denseg.GetNumseg();

	//cout << MSerial_AsnText << Denseg;
	for(int Loop = 0; Loop < SegCount; Loop++) {
		size_t StartIndex = (Loop*Denseg.GetDim());
		
		if (Denseg.GetStarts()[StartIndex] == -1 ||
			Denseg.GetStarts()[StartIndex+1] == -1)
			continue;

		CEquivRange Curr;
		Curr.Query.SetFrom(Denseg.GetStarts()[StartIndex]);
		Curr.Query.SetLength(Denseg.GetLens()[Loop]);

		Curr.Subjt.SetFrom(Denseg.GetStarts()[StartIndex+1]);
		Curr.Subjt.SetLength(Denseg.GetLens()[Loop]);
		
		if(Denseg.CanGetStrands() && Denseg.GetStrands().size() > StartIndex)
			Curr.Strand = Denseg.GetStrands()[StartIndex];
		else
			Curr.Strand = eNa_strand_plus;

        {{
            // Enforce Query Strand can vary, but SubjtStrand is Plus
            ENa_strand SubjtStrand = eNa_strand_plus;
            if(Denseg.CanGetStrands() && Denseg.GetStrands().size() > StartIndex+1)
                SubjtStrand = Denseg.GetStrands()[StartIndex+1];
            if(SubjtStrand == eNa_strand_minus) {
                Curr.Strand = (Curr.Strand == eNa_strand_plus ? eNa_strand_minus : eNa_strand_plus);
            }
        }}

		if(Curr.Strand == eNa_strand_plus) 
			Curr.Intercept = int(Curr.Subjt.GetFrom()) - int(Curr.Query.GetFrom());
		else
			Curr.Intercept = Curr.Subjt.GetTo() + Curr.Query.GetFrom();

		Curr.Matches = 0;
        Curr.MisMatches = 0;

        Curr.AlignId = GAlignId;
        Curr.SegmtId = GSegmtId; GSegmtId++;
        Curr.SplitId = GSplitId; GSplitId++;

        Equivs.push_back(Curr);
		//cout << Curr.Query << " to " << Curr.Subjt << endl;
	}
    
    GAlignId++;
}


void CEquivRange::CalcMatches(objects::CBioseq_Handle QueryHandle, 
                              objects::CBioseq_Handle SubjtHandle,
                              TEquivList& Equivs)
{
 	if(Equivs.empty())
        return;
    
    CSeqVector QueryVec(QueryHandle, CBioseq_Handle::eCoding_Iupac, Equivs.front().Strand);
 	//CSeqVector QueryVec(QueryHandle, CBioseq_Handle::eCoding_Iupac, eNa_strand_plus);
 	CSeqVector SubjtVec(SubjtHandle, CBioseq_Handle::eCoding_Iupac, eNa_strand_plus);

    
    TSeqRange QRange, SRange;
    NON_CONST_ITERATE(TEquivList, EquivIter, Equivs) {
        CEquivRange& CurrEquiv = *EquivIter;
        QRange += CurrEquiv.Query;
        SRange += CurrEquiv.Subjt;
    }
  //cerr << "FR: " << QRange << "\t" << SRange << endl; 
    string QueryStr, SubjtStr;
	if(Equivs.front().Strand == eNa_strand_plus)
	    QueryVec.GetSeqData(QRange.GetFrom(), QRange.GetTo()+1, QueryStr); 
	else
	    QueryVec.GetSeqData(QueryVec.size()-QRange.GetTo()-1, 
			    			QueryVec.size()-QRange.GetFrom(), QueryStr);	    
    SubjtVec.GetSeqData(SRange.GetFrom(), SRange.GetTo()+1, SubjtStr); 

  //cerr << "FL: " << QueryStr.length() << "\t" << SubjtStr.length() << endl;
  //size_t Counter = 0;
    NON_CONST_ITERATE(TEquivList, EquivIter, Equivs) {
        CEquivRange& CurrEquiv = *EquivIter;
        
        //cerr << "CR: " << CurrEquiv.Query << "\t" << CurrEquiv.Subjt << endl;

        CurrEquiv.Matches = 0;
        CurrEquiv.MisMatches = 0;
       
        size_t QPOffset = CurrEquiv.Query.GetFrom()-QRange.GetFrom();
        size_t QMOffset = QRange.GetTo()-CurrEquiv.Query.GetTo();

        size_t QOffset = (CurrEquiv.Strand == eNa_strand_plus ? QPOffset : QMOffset); 
        size_t SOffset = CurrEquiv.Subjt.GetFrom()-SRange.GetFrom();

        for(size_t Loop = 0; Loop < CurrEquiv.Subjt.GetLength(); Loop++) {
		    size_t QLoop = QOffset+Loop;
            size_t SLoop = SOffset+Loop;
            //if(CurrEquiv.Strand == eNa_strand_minus)
            //    QLoop = (QRange.GetTo()-CurrEquiv.Query.GetTo()) + Loop;
                //QLoop = QueryStr.length() - QLoop - 1;
         //   cerr << "CI: " << QLoop << "\t" << SLoop << endl;
            if(QueryStr[QLoop] == SubjtStr[SLoop]) {
			    CurrEquiv.Matches++;
            } else {
                CurrEquiv.MisMatches++;
                CurrEquiv.MisMatchSubjtPoints.push_back(CurrEquiv.Subjt.GetFrom()+Loop);
            }
	    }        
        
        //cerr << "MM: " << CurrEquiv.Matches << "\t" << CurrEquiv.MisMatches << endl;
        //if(Counter % 1000 == 0) 
        //    cerr << "CalcMatches: " << Counter << endl;
        //Counter++;
    }

}


