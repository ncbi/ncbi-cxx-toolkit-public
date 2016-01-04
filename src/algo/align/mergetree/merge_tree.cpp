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
* Authors: Nathan Bouk
*
* File Description:
*   Alignment merge by way of depth-first tree search
*
* ===========================================================================
*/

#include <ncbi_pch.hpp> 
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seqalign/seqalign__.hpp>

#include <objmgr/util/sequence.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include <algo/align/mergetree/equiv_range.hpp>
#include <algo/align/mergetree/merge_tree_core.hpp>
#include <algo/align/mergetree/merge_tree.hpp>

#include <algo/align/nw/align_exception.hpp>

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);


//////////////////////


set<int> 
s_AlignIdsFromEquivList(const TEquivList& Equivs) {
    set<int> AlignIds;
    ITERATE(TEquivList, EquivIter, Equivs) {
        AlignIds.insert(EquivIter->AlignId);
    }
    return AlignIds;
}
size_t s_ScoreFromEquivList(const TEquivList& Equivs) {
    size_t Matches = 0;
    size_t MisMatches = 0;
    ITERATE(TEquivList, EquivIter, Equivs) {
        Matches += EquivIter->Matches;
        MisMatches += EquivIter->MisMatches;
    }
    return Matches;
}

bool s_SortSeqAlignByQuery_Subjt(CRef<CSeq_align> A, CRef<CSeq_align> B) 
{
    if(A->GetSeqStart(0) != B->GetSeqStart(0))
        return (A->GetSeqStart(0) < B->GetSeqStart(0));
    else if(A->GetSeqStop(0) != B->GetSeqStop(0))
        return (A->GetSeqStop(0) < B->GetSeqStop(0));
    else if(A->GetSeqStart(1) != B->GetSeqStart(1))
        return (A->GetSeqStart(1) < B->GetSeqStart(1));
    else if(A->GetSeqStop(1) != B->GetSeqStop(1))
        return (A->GetSeqStop(1) < B->GetSeqStop(1));
    else
        return A->GetSeqStrand(0) < B->GetSeqStrand(0);
}

bool s_SortSeqAlignByQueryMinus_Subjt(CRef<CSeq_align> A, CRef<CSeq_align> B) 
{
    if(A->GetSeqStart(0) != B->GetSeqStart(0))
        return (A->GetSeqStart(0) > B->GetSeqStart(0));
    else if(A->GetSeqStop(0) != B->GetSeqStop(0))
        return (A->GetSeqStop(0) > B->GetSeqStop(0));
    else if(A->GetSeqStart(1) != B->GetSeqStart(1))
        return (A->GetSeqStart(1) < B->GetSeqStart(1));
    else if(A->GetSeqStop(1) != B->GetSeqStop(1))
        return (A->GetSeqStop(1) < B->GetSeqStop(1));
    else
        return A->GetSeqStrand(0) < B->GetSeqStrand(0);
}

TSignedSeqPos s_SeqAlignIntercept(const CSeq_align& A) 
{
    TSignedSeqPos InterA;
    
    if(A.GetSeqStrand(0) == eNa_strand_plus) 
		InterA = int(A.GetSeqStart(1)) - int(A.GetSeqStart(0));
	else
		InterA = A.GetSeqStop(1) + A.GetSeqStart(0);
    
    return InterA;
}

bool s_SortSeqAlignByIntercept(CRef<CSeq_align> A, CRef<CSeq_align> B) 
{
    TSignedSeqPos InterA, InterB;
    InterA = s_SeqAlignIntercept(*A);
    InterB = s_SeqAlignIntercept(*B);

    return (InterA < InterB);
}

bool s_SortSeqAlignByLength(CRef<CSeq_align> A, CRef<CSeq_align> B) 
{
    return (A->GetAlignLength(false) < B->GetAlignLength(false));
}

void s_EquivDiff(const TEquivList& A, const TEquivList& B)
{
    
    TEquivList::const_iterator AI, BI;
    TEquivList::const_iterator AE, BE;
    
    AI = A.begin();
    BI = B.begin();
    AE = A.end();
    BE = B.end();
    
    while(true) {
        
        if(AI == AE || BI == BE) {
            break;
        }

        if(AI->Query == BI->Query && AI->Subjt == BI->Subjt) {
            ++AI;
            ++BI;
            continue;
        }

        if(AI->Query.GetFrom() < BI->Query.GetFrom()) {
#ifdef MERGE_TREE_VERBOSE_DEBUG
            cerr << " <  " << *AI << endl;
#endif
            ++AI;
            continue;
        }
        else if(BI->Query.GetFrom() < AI->Query.GetFrom()) {
#ifdef MERGE_TREE_VERBOSE_DEBUG
            cerr << "  > " << *BI << endl;
#endif
            ++BI;
            continue;
        }
        else {
            if(AI->Subjt.GetFrom() < BI->Subjt.GetFrom()) {
#ifdef MERGE_TREE_VERBOSE_DEBUG
                cerr << " <  " << *AI << endl;
#endif
                ++AI;
                continue;
            }
            else if(BI->Subjt.GetFrom() < AI->Subjt.GetFrom()) {
#ifdef MERGE_TREE_VERBOSE_DEBUG
                cerr << "  > " << *BI << endl;
#endif
                ++BI;
                continue;
            }
            else {
#ifdef MERGE_TREE_VERBOSE_DEBUG
                cerr << " <  " << *AI << endl;
                cerr << "  > " << *BI << endl;
#endif
                ++AI;
                ++BI;
                continue;
            }
        }
    }


    while(AI != AE) {
#ifdef MERGE_TREE_VERBOSE_DEBUG
        cerr << "  > " << *AI << endl;
#endif
        ++AI;
    }
    while(BI != BE) {
#ifdef MERGE_TREE_VERBOSE_DEBUG
        cerr << " <  " << *BI << endl;
#endif
        ++BI;
    }
}


TSeqPos s_AlignDist(const CSeq_align& A, const CSeq_align& B) 
{
    TSeqRange AQR = A.GetSeqRange(0);
    TSeqRange ASR = A.GetSeqRange(1);
    TSeqRange BQR = B.GetSeqRange(0);
    TSeqRange BSR = B.GetSeqRange(1);

    TSeqRange QI = AQR.IntersectionWith(BQR);
    TSeqRange SI = ASR.IntersectionWith(BSR);

    TSeqPos QD=0, SD=0;
    if(QI.Empty()) {
        if( AQR.GetFrom() >= BQR.GetTo() ) {
            QD = AQR.GetFrom() - BQR.GetTo();
        } else {
            QD = BQR.GetFrom() - AQR.GetTo();
        }
    } 
    if(SI.Empty()) {
        if( ASR.GetFrom() >= BSR.GetTo() ) {
            SD = ASR.GetFrom() - BSR.GetTo();
        } else {
            SD = BSR.GetFrom() - ASR.GetTo();
        }
    } 
    TSeqPos D = QD + SD;

    TSignedSeqPos AINT = s_SeqAlignIntercept(A);
    TSignedSeqPos BINT = s_SeqAlignIntercept(B);
    TSeqPos DeltaInt = abs(AINT - BINT);

    return max(D, DeltaInt);
}




void s_FindBestSubRange(const CMergeTree& Tree, const TEquivList& Path) 
{

    int F = 0, L = 0;
#ifdef MERGE_TREE_VERBOSE_DEBUG
    int BF = numeric_limits<int>::max();
#endif
    int BL = -1;
    int BestScore = numeric_limits<int>::min();

    TEquivList CurrPath;

    for(L=0; L < (int)Path.size(); L++) {
        CurrPath.push_back( Path[L] );
        int CurrScore = Tree.Score(CurrPath);
        if(CurrScore >= BestScore) {
            BL = L;
            BestScore = CurrScore;
        }
    }

    CurrPath.clear();

    for(F = BL; F >= 0; F--) {
        CurrPath.insert(CurrPath.begin(),Path[F]);
        int CurrScore = Tree.Score(CurrPath);
        if(CurrScore >= BestScore) {
#ifdef MERGE_TREE_VERBOSE_DEBUG
            BF = F;
#endif
            BestScore = CurrScore;
        }
    }


#ifdef MERGE_TREE_VERBOSE_DEBUG
    if(BF > 0 || BL < (int)(Path.size()-1) ) {
        cerr << "s_FindBestSubRange : " << BF << " : " << BL << " : " << BestScore << "   of " << Path.size()-1 << endl;
    }
#endif
}


void CTreeAlignMerger::Merge(const list< CRef<CSeq_align> >& Input,
                             list< CRef<CSeq_align> >& Output ) 
{
    Merge_Dist(Input, Output);
}

void CTreeAlignMerger::Merge_AllAtOnce(const list< CRef<CSeq_align> >& Input,
                             list< CRef<CSeq_align> >& Output ) 
{
    CEquivRangeBuilder EquivRangeBuilder;

    typedef pair<CSeq_id_Handle, ENa_strand> TSeqIdPair;
    typedef pair<TSeqIdPair, TSeqIdPair> TMapKey;
    typedef vector<CRef<CSeq_align> > TAlignVec;
    typedef map<TMapKey, TAlignVec> TAlignMap;
    TAlignMap AlignMap;

    ITERATE(list<CRef<CSeq_align> >, AlignIter, Input) {
        const CSeq_align& Align = **AlignIter;

        TMapKey Key;
        Key.first.first   = CSeq_id_Handle::GetHandle(Align.GetSeq_id(0));
        Key.first.second  = Align.GetSeqStrand(0);
        Key.second.first  = CSeq_id_Handle::GetHandle(Align.GetSeq_id(1));
        Key.second.second = Align.GetSeqStrand(1);

        AlignMap[Key].push_back(*AlignIter);
    }

    NON_CONST_ITERATE(TAlignMap, MapIter, AlignMap) {
        if(MapIter->first.first.second == eNa_strand_plus)
            sort(MapIter->second.begin(), MapIter->second.end(), s_SortSeqAlignByQuery_Subjt);
        else if(MapIter->first.first.second == eNa_strand_minus) 
            sort(MapIter->second.begin(), MapIter->second.end(), s_SortSeqAlignByQueryMinus_Subjt);
    }

    ITERATE(TAlignMap, MapIter, AlignMap) {
        CSeq_id_Handle QueryIDH = MapIter->first.first.first;
        CSeq_id_Handle SubjtIDH = MapIter->first.second.first;

        CBioseq_Handle QueryBSH, SubjtBSH;
        if(m_Scope != NULL) {
            QueryBSH = m_Scope->GetBioseqHandle(QueryIDH);
            SubjtBSH = m_Scope->GetBioseqHandle(SubjtIDH);
        }

    
        TEquivList OrigEquivs, SplitEquivs;
        int AID = 0;
        ITERATE(TAlignVec, AlignIter, MapIter->second) {
#ifdef MERGE_TREE_VERBOSE_DEBUG
            cerr << AID << "\t" << (*AlignIter)->GetSeqRange(0) 
                        << "\t" << (*AlignIter)->GetSeqRange(1) << endl;
#endif
            EquivRangeBuilder.ExtractRangesFromSeqAlign(**AlignIter, OrigEquivs);
            AID++;
        }
#ifdef MERGE_TREE_VERBOSE_DEBUG
        cerr << "OrigEquivs; " << OrigEquivs.size() << endl; 
        cerr << "Verify AlignIds: " << s_AlignIdsFromEquivList(OrigEquivs).size() << endl;
#endif

        if(QueryBSH && SubjtBSH) {
            EquivRangeBuilder.CalcMatches(QueryBSH, SubjtBSH, OrigEquivs);
        } else {
            NON_CONST_ITERATE(TEquivList, Iter, OrigEquivs) {
                Iter->Matches  = Iter->Query.GetLength();
                Iter->MisMatches = 0;
            }
        }
        
        
        //cerr << "A: " << MapIter->second.size() << " OS: " << OrigEquivs.size() << " SE: " << SplitEquivs.size() << endl;
        /*{{
         /// Mostly Failed effort to pre-screen the alignments 
            CRangeCollection<TSeqPos> QRC, SRC;
            CRangeCollection<TSeqPos> DQRC, DSRC;
            ITERATE(TEquivList, Iter, OrigEquivs) {
                if(QRC.IntersectingWith(Iter->Query)) {
                    CRangeCollection<TSeqPos> T(Iter->Query);
                    T.IntersectWith(QRC);
                    DQRC += T;
                }
                if(SRC.IntersectingWith(Iter->Subjt)) {
                    CRangeCollection<TSeqPos> T(Iter->Subjt);
                    T.IntersectWith(SRC);
                    DSRC += T;
                }
                QRC += Iter->Query;
                SRC += Iter->Subjt;
            }
            CRangeCollection<TSeqPos> NQRC, NSRC;
            NQRC += QRC.GetLimits();
            NSRC += SRC.GetLimits();
            NQRC -= DQRC;//QRC;
            NSRC -= DSRC;//SRC;

            vector<TSeqPos> RQs, RSs;
            RQs.push_back(QRC.GetFrom());
            ITERATE(CRangeCollection<TSeqPos>, Iter, NQRC) {
                RQs.push_back(Iter->GetFrom());
                RQs.push_back(Iter->GetTo());
                cerr << "QZ: " << *Iter << endl;
            }
            RQs.push_back(QRC.GetTo()+1);
            RSs.push_back(SRC.GetFrom());
            ITERATE(CRangeCollection<TSeqPos>, Iter, NSRC) {
                RSs.push_back(Iter->GetFrom());
                RSs.push_back(Iter->GetTo());
                cerr << "SZ: " << *Iter << endl;
            }
            RSs.push_back(SRC.GetTo()+1);
            {{
                vector<TSeqPos>::iterator NE;
                sort(RQs.begin(), RQs.end());
                NE = unique(RQs.begin(), RQs.end());
		        RQs.erase(NE, RQs.end());
                sort(RSs.begin(), RSs.end());
                NE = unique(RSs.begin(), RSs.end());
		        RSs.erase(NE, RSs.end());
            }}
        
        
            CMergeTree::SScoring QuadScoring;
            QuadScoring.GapExtend = 0;
            TSeqRange QR, SR;
            ITERATE(TEquivList, Iter, OrigEquivs) {
                QR.CombineWith(Iter->Query);
                SR.CombineWith(Iter->Subjt);
            }
            cerr << QR << "\t" << SR << endl;
            CQuadTree QT(64, QR, SR);
            QT.SetRecs(RQs, RSs);
            ITERATE(TEquivList, Iter, OrigEquivs) {
                QT.Insert(*Iter);
            }
            QT.Print();
           
            int NodesVisited = 0;
            TEquivList Path;
            NON_CONST_ITERATE(CQuadTree, QuadIter, QT) {
                NodesVisited++;

                if(!QuadIter->IsLeaf)
                    QuadIter->Merge();
                    
                if(QuadIter->Storage.empty())
                    continue;
               
                set<int> AlignIds = s_AlignIdsFromEquivList(QuadIter->Storage);
                cerr << QuadIter->XR << "\t" << QuadIter->YR 
                    << "\t" << QuadIter->Depth
                    << "\t" << QuadIter->Storage.size() 
                    << "\t" << AlignIds.size() 
                    << "\t" << s_ScoreFromEquivList(QuadIter->Storage)
                    << endl;
                ITERATE(set<int>, AlignIdIter, AlignIds) {
                    cerr << "\t" << *AlignIdIter;
                }
                cerr << endl;
                {
                    TEquivList LocalSplits;
                    EquivRangeBuilder.SplitIntersections(QuadIter->Storage, LocalSplits);
                    QuadIter->Storage.clear();
                    
                    CMergeTree Tree;
                    Tree.SetScoring(QuadScoring); //m_Scoring);
                    Tree.AddEquivs(LocalSplits);
                    LocalSplits.clear();
                    Path.clear();
                    Tree.Search(Path);
                    

                    cerr << "\t" << Path.size() << endl;
                    
                    //{{
                    //    CRef<CSeq_align> Merged;
                    //    Merged = x_MakeSeqAlign(Path, QueryIDH, SubjtIDH);
                    //    if(Merged)
                    //        Output.push_back(Merged);
                    //}}   
                   
                    EquivRangeBuilder.MergeAbuttings(Path, QuadIter->Storage);

                    //QuadIter->Storage.insert(QuadIter->Storage.end(), Path.begin(), Path.end());
                    AlignIds = s_AlignIdsFromEquivList(QuadIter->Storage);
                    cerr << "\t" << QuadIter->Storage.size()
                        << "\t" << AlignIds.size()
                        << "\t" << s_ScoreFromEquivList(QuadIter->Storage)
                        << endl;
                    ITERATE(set<int>, AlignIdIter, AlignIds) {
                        cerr << "\t" << *AlignIdIter;
                    }
                    cerr << endl;
                }
            }
            cerr << "NodesVisited: " << NodesVisited << endl;
      
            {{
                CRef<CSeq_align> Merged;
                Merged = x_MakeSeqAlign(Path, QueryIDH, SubjtIDH);
            
                if(Merged)
                    Output.push_back(Merged);
            }}

            set<int> AlignIds;
            ITERATE(TEquivList, PathIter, Path) {
                AlignIds.insert(PathIter->AlignId);
            }
            cerr << "Aligns: " << AlignIds.size() << endl;
            TEquivList Filtered;
            ITERATE(TEquivList, EraseIter, OrigEquivs) {
                if( AlignIds.find(EraseIter->AlignId) != AlignIds.end()) {
                    Filtered.push_back(*EraseIter);
                }
            }
            cerr << " Orig: " << OrigEquivs.size() << " Filt: " << Filtered.size() << endl;
            OrigEquivs.swap(Filtered);
        cerr << "_______________________________" << endl;
        }}*/
        EquivRangeBuilder.SplitIntersections(OrigEquivs, SplitEquivs);
        
        CMergeTree Tree;
        if(Callback != NULL)
            Tree.SetInterruptCallback(Callback, CallbackData);
        Tree.SetScoring(m_Scoring);

        Tree.AddEquivs(SplitEquivs);
        
        while(!Tree.IsEmpty()) {
            TEquivList Path;
           
            Tree.Search(Path);
    
            if(Tree.GetInterrupted())
                return;
            
            if(Path.empty())
                break;

            CRef<CSeq_align> Merged;
            Merged = x_MakeSeqAlign(Path, QueryIDH, SubjtIDH);
            
            if(Merged)
                Output.push_back(Merged);
        //break;
        } // end Tree not empty
    } // end Id-Strand-Map
}


void CTreeAlignMerger::Merge_Pairwise(const list< CRef<CSeq_align> >& Input,
                             list< CRef<CSeq_align> >& Output ) 
{
    
    typedef pair<CSeq_id_Handle, ENa_strand> TSeqIdPair;
    typedef pair<TSeqIdPair, TSeqIdPair> TMapKey;
    typedef vector<CRef<CSeq_align> > TAlignVec;
    typedef map<TMapKey, TAlignVec> TAlignMap;
    TAlignMap AlignMap;

    CEquivRangeBuilder EquivRangeBuilder;
    
    ITERATE(list<CRef<CSeq_align> >, AlignIter, Input) {
        const CSeq_align& Align = **AlignIter;

        TMapKey Key;
        Key.first.first   = CSeq_id_Handle::GetHandle(Align.GetSeq_id(0));
        Key.first.second  = Align.GetSeqStrand(0);
        Key.second.first  = CSeq_id_Handle::GetHandle(Align.GetSeq_id(1));
        Key.second.second = Align.GetSeqStrand(1);

        AlignMap[Key].push_back(*AlignIter);
    }
    
    NON_CONST_ITERATE(TAlignMap, MapIter, AlignMap) {
        
        //sort(MapIter->second.begin(), MapIter->second.end(), s_SortSeqAlignByIntercept);
        sort(MapIter->second.begin(), MapIter->second.end(), s_SortSeqAlignByLength);

        //if(MapIter->first.first.second == eNa_strand_plus)
        //    sort(MapIter->second.begin(), MapIter->second.end(), s_SortSeqAlignByQuery_Subjt);
        //else if(MapIter->first.first.second == eNa_strand_minus) 
        //    sort(MapIter->second.begin(), MapIter->second.end(), s_SortSeqAlignByQueryMinus_Subjt);
        
        //reverse(MapIter->second.begin(), MapIter->second.end());
    }


    ITERATE(TAlignMap, MapIter, AlignMap) {
        CSeq_id_Handle QueryIDH = MapIter->first.first.first;
        CSeq_id_Handle SubjtIDH = MapIter->first.second.first;

        CBioseq_Handle QueryBSH, SubjtBSH;
        if(m_Scope != NULL) {
            QueryBSH = m_Scope->GetBioseqHandle(QueryIDH);
            SubjtBSH = m_Scope->GetBioseqHandle(SubjtIDH);
        }
        
        typedef map<int, TEquivList> TAlignEquivListMap;
        TAlignEquivListMap AlignEquivs;
        TEquivList OrigEquivs, SplitEquivs;
        int AID = 0;
        ITERATE(TAlignVec, AlignIter, MapIter->second) {
            TEquivList Temp;
            //EquivRangeBuilder.ExtractRangesFromSeqAlign(**AlignIter, OrigEquivs);
            EquivRangeBuilder.ExtractRangesFromSeqAlign(**AlignIter, Temp);
            
            if(QueryBSH && SubjtBSH) {
                EquivRangeBuilder.CalcMatches(QueryBSH, SubjtBSH, Temp);
            } else { 
                NON_CONST_ITERATE(TEquivList, Iter, Temp) {
                    Iter->Matches  = Iter->Query.GetLength();
                    Iter->MisMatches = 0;
                }
            }


            AlignEquivs[AID].insert(AlignEquivs[AID].end(), Temp.begin(), Temp.end());
            OrigEquivs.insert(OrigEquivs.end(), Temp.begin(), Temp.end());
#ifdef MERGE_TREE_VERBOSE_DEBUG
            cerr << AID << "\t" << (*AlignIter)->GetSeqRange(0) 
                        << "\t" << (*AlignIter)->GetSeqRange(1) << endl;
#endif
            AID++;
        }
#ifdef MERGE_TREE_VERBOSE_DEBUG
        cerr << "OrigEquivs; " << OrigEquivs.size() << endl; 
        cerr << "Verify AlignIds: " << s_AlignIdsFromEquivList(OrigEquivs).size() << endl;
#endif

        if(QueryBSH && SubjtBSH) {
            EquivRangeBuilder.CalcMatches(QueryBSH, SubjtBSH, OrigEquivs);
        } else {
            NON_CONST_ITERATE(TEquivList, Iter, OrigEquivs) {
                Iter->Matches  = Iter->Query.GetLength();
                Iter->MisMatches = 0;
            }
        }
        
#ifdef MERGE_TREE_VERBOSE_DEBUG
        int AccumScore = 0;
#endif
        TEquivList AccumEquivs;
        for(int Loop = 0; Loop < 3; Loop++) {
        ITERATE(TAlignEquivListMap, OrigAlignIter, AlignEquivs) {
            const TEquivList& CurrAlignEquivs = OrigAlignIter->second;
            
            TEquivList CurrEquivs;
            CurrEquivs.insert(CurrEquivs.end(), AccumEquivs.begin(), AccumEquivs.end());
            CurrEquivs.insert(CurrEquivs.end(), CurrAlignEquivs.begin(), CurrAlignEquivs.end());

            //cerr << "A: " << MapIter->second.size() << " OS: " << OrigEquivs.size() << " SE: " << SplitEquivs.size() << endl;
            SplitEquivs.clear();
            EquivRangeBuilder.SplitIntersections(CurrEquivs, SplitEquivs);
         
       
            CMergeTree Tree;
            if(Callback != NULL)
                Tree.SetInterruptCallback(Callback, CallbackData);
            Tree.SetScoring(m_Scoring);

            Tree.AddEquivs(SplitEquivs);
            //Tree.Print(cout);
            
            while(!Tree.IsEmpty()) {
                TEquivList Path;
           
                Tree.Search(Path);
    
                if(Tree.GetInterrupted())
                    return;
            
                if(Path.empty())
                    break;
                
#ifdef MERGE_TREE_VERBOSE_DEBUG
                int NewScore = Tree.Score(Path);
                if(AccumScore > NewScore)
                    cerr << "BACKWARDS!" << endl;
                AccumScore = NewScore;
#endif

                //AccumEquivs.clear();
                TEquivList Abutted;
                EquivRangeBuilder.MergeAbuttings(Path, Abutted);
                sort(Abutted.begin(), Abutted.end());
                s_EquivDiff(AccumEquivs, Abutted);
                AccumEquivs = Abutted;
                //AccumEquivs.insert(AccumEquivs.end(), Path.begin(), Path.end());

#ifdef MERGE_TREE_VERBOSE_DEBUG
            cerr << __LINE__ << ":" << AccumEquivs.size() 
                             << ":" << CurrAlignEquivs.size() 
                             << ":" << SplitEquivs.size() 
                             << ":" << Path.size()
                             << ":" << Tree.Size()
                             << ":" << Tree.Links()<< endl;    
#endif
            break;
            } // end Tree not empty

        }  // end orig align loop
        } // end Loop loop
    
        {{
            CRef<CSeq_align> Merged;
            Merged = x_MakeSeqAlign(AccumEquivs, QueryIDH, SubjtIDH);
            
            if(Merged)
                Output.push_back(Merged);
        }}
    
    } // end Id-Strand-Map
}


void CTreeAlignMerger::Merge_Dist(const list< CRef<CSeq_align> >& Input,
                             list< CRef<CSeq_align> >& Output ) 
{
    TAlignGroupMap AlignGroupMap;
    x_MakeMergeableGroups(Input, AlignGroupMap);
        
    ITERATE(TAlignGroupMap, MapIter, AlignGroupMap) {
        CSeq_id_Handle QueryIDH = MapIter->first.first.first;
        CSeq_id_Handle SubjtIDH = MapIter->first.second.first;
        
#ifdef MERGE_TREE_VERBOSE_DEBUG
        cerr << "Starting " << QueryIDH.AsString() << (MapIter->first.first.second == eNa_strand_plus ? "+":"-")
            << " x " << SubjtIDH.AsString() << (MapIter->first.second.second == eNa_strand_plus ? "+":"-") << endl;
#endif

        CBioseq_Handle QueryBSH, SubjtBSH;
        if(m_Scope != NULL) {
            QueryBSH = m_Scope->GetBioseqHandle(QueryIDH);
            SubjtBSH = m_Scope->GetBioseqHandle(SubjtIDH);
        }
       
        
        TAlignVec Aligns  = MapIter->second;
        
        // length checks
        if (QueryBSH && SubjtBSH && 
            QueryBSH.CanGetInst_Length() && 
            SubjtBSH.CanGetInst_Length()) {
            TSeqPos QueryLen = QueryBSH.GetInst_Length();
            TSeqPos SubjtLen = SubjtBSH.GetInst_Length();
            TSeqRange QueryRange, SubjtRange;
            ITERATE(TAlignVec, AlignIter, Aligns) {
                QueryRange += (*AlignIter)->GetSeqRange(0);
                SubjtRange += (*AlignIter)->GetSeqRange(1);
            }
            if (QueryRange.GetTo() >= QueryLen  || 
                SubjtRange.GetTo() >= SubjtLen) {
                NCBI_THROW(CAlgoAlignException, CAlgoAlignException::eBadParameter, "Alignment ranges are outside of sequence lengths");
            }
        }

        x_Merge_Dist_Impl(Aligns, QueryIDH, SubjtIDH, QueryBSH, SubjtBSH, Output);

    } // end id-strand-map loop

}



CRef<CSeq_align> 
CTreeAlignMerger::x_MakeSeqAlign(TEquivList& Equivs, 
							CSeq_id_Handle QueryIDH, 
							CSeq_id_Handle SubjtIDH)
{
	CRef<CSeq_align> New(new CSeq_align);
	New->SetType(CSeq_align::eType_partial);
	CDense_seg& Denseg = New->SetSegs().SetDenseg();

	Denseg.SetDim(2);
	Denseg.SetNumseg(Equivs.size());

    CRef<CSeq_id> QueryId(new CSeq_id), SubjtId(new CSeq_id);
    QueryId->Assign(*QueryIDH.GetSeqId());
    SubjtId->Assign(*SubjtIDH.GetSeqId());
	Denseg.SetIds().push_back(QueryId);
	Denseg.SetIds().push_back(SubjtId);


    sort(Equivs.begin(), Equivs.end(), s_SortEquivBySubjt);

    ITERATE(TEquivList, EquivIter, Equivs) {
        
        if(EquivIter->Empty())
            continue;
    
        Denseg.SetStarts().push_back(EquivIter->Query.GetFrom());
        Denseg.SetStarts().push_back(EquivIter->Subjt.GetFrom());

        Denseg.SetLens().push_back(EquivIter->Query.GetLength());

        Denseg.SetStrands().push_back(EquivIter->Strand);
        Denseg.SetStrands().push_back(eNa_strand_plus);
    }
    
	Denseg.Compact();
	try {
		CRef<CDense_seg> Temp = Denseg.FillUnaligned();
		Denseg.Assign(*Temp);
	} catch(CException& e) {
		LOG_POST(Error << e.ReportAll() << MSerial_AsnText << Denseg);
        throw;
	}
	Denseg.Compact();

	try {
		New->Validate(true);
		return New;
	} catch(CException& e) {
		LOG_POST(Error << e.ReportAll() << MSerial_AsnText << Denseg);
        throw;
	}

	return CRef<CSeq_align>();
}


void 
CTreeAlignMerger::x_MakeMergeableGroups(list<CRef<CSeq_align> > Input, 
                                        TAlignGroupMap& AlignGroupMap) {
    ITERATE(list<CRef<CSeq_align> >, AlignIter, Input) {
        const CSeq_align& Align = **AlignIter;

        TMapKey Key;
        Key.first.first   = CSeq_id_Handle::GetHandle(Align.GetSeq_id(0));
        Key.first.second  = Align.GetSeqStrand(0);
        Key.second.first  = CSeq_id_Handle::GetHandle(Align.GetSeq_id(1));
        Key.second.second = Align.GetSeqStrand(1);

        AlignGroupMap[Key].push_back(*AlignIter);
    }
}



class CAlignDistGraph 
{
public:
    CAlignDistGraph(CEquivRangeBuilder& Builder) : m_EquivRangeBuilder(Builder), m_NextIndex(0) { ; }
    CAlignDistGraph(const CAlignDistGraph& Original);

    size_t Size() const { return AlignEquivMap.size(); }
    bool  Empty() const { return AlignEquivMap.empty(); }


    void AddAlignment(const TEquivList& NewAlignEquivs);
    bool GetAndRemoveNearestPair(TEquivList& First, TEquivList& Second);
    bool GetLastAlignEquivs(TEquivList& LastEquivs);
    void RemoveEquivs(const TEquivList& RemoveEquivs);

    void CalculateAllDistances() { x_CalculateAllDistances(); }

    void Print();

private:
    CEquivRangeBuilder& m_EquivRangeBuilder;

    size_t m_NextIndex;
    
    typedef map<size_t, TEquivList> TAlignEquivListMap;
    typedef map<size_t, size_t> TIndexIndexMap;
    typedef map<size_t, TSeqPos> TIndexDistMap;
   
    TAlignEquivListMap AlignEquivMap;
    TIndexIndexMap  NearestMap;
    TIndexDistMap   NearestDistMap;
    
    void x_CalculateAllDistances();
    void x_CalculateOneDistance(size_t CalcIndex);
};

void CAlignDistGraph::Print() 
{
    {{
#ifdef MERGE_TREE_VERBOSE_DEBUG
        cerr << "NearestMap: " << NearestMap.size() << endl;
        cerr << "NearestDistMap: " << NearestDistMap.size() << endl;
        cerr << "AlignEquivMap: " << AlignEquivMap.size() << endl;
        TSeqPos MinD = numeric_limits<TSeqPos>::max();
        size_t MinI;
        ITERATE(TIndexIndexMap, IndexIter, NearestMap) {
            size_t CI = IndexIter->first;
            TSeqPos CD = NearestDistMap[CI];
            size_t OI = IndexIter->second;
            cerr << "Near Pair : " << CI << " x " << OI << " => " << CD << endl;
            
            if(CD < MinD) {
                MinD = CD;
                MinI = CI;
            }
        }
        cerr << "Minimum : " << MinI << " => " << MinD << endl;
        if(AlignEquivMap.size() != NearestMap.size()) {
            ITERATE(TAlignEquivListMap, AlignIter, AlignEquivMap) {
                cerr << "Aligns : " << AlignIter->first << " : " << AlignIter->second.size() << endl;
            }
        }
#endif
    }}
}

CAlignDistGraph::CAlignDistGraph(const CAlignDistGraph& Original) 
    : m_EquivRangeBuilder(Original.m_EquivRangeBuilder), m_NextIndex(0)
{
    m_NextIndex = Original.m_NextIndex;
    ITERATE(TAlignEquivListMap, AlignIter, Original.AlignEquivMap) {        
        ITERATE(TEquivList, EquivIter, AlignIter->second) {            
            AlignEquivMap[AlignIter->first].push_back(*EquivIter);
        }
    }

    NearestMap.insert(Original.NearestMap.begin(), Original.NearestMap.end());
    NearestDistMap.insert(Original.NearestDistMap.begin(), Original.NearestDistMap.end());
    
    //Print();

}


void CAlignDistGraph::AddAlignment(const TEquivList& NewAlignEquivs) 
{
    size_t NewI = m_NextIndex;
    m_NextIndex++;
    //NewI = 

    AlignEquivMap[NewI].insert(AlignEquivMap[NewI].end(), 
                                NewAlignEquivs.begin(), NewAlignEquivs.end());

    //x_CalculateAllDistances();
    x_CalculateOneDistance(NewI);
    return;

    const TEquivList& NewEquivs = AlignEquivMap[NewI];
    TSeqPos BestD = numeric_limits<TSeqPos>::max();
    size_t BestI = NewI;
    ITERATE(TIndexIndexMap, IndexIter, NearestMap) {
        size_t CI = IndexIter->first;
        const TEquivList& CurrEquivs = AlignEquivMap[CI];

        TSeqPos CurrDist = CEquivRange::Distance(NewEquivs, CurrEquivs);

        if(CurrDist < BestD) {
            BestD = CurrDist;
            BestI = CI;
        }
    }
        
    NearestMap[NewI] = BestI;
    NearestDistMap[NewI] = BestD;
    
    if(BestD != numeric_limits<TSeqPos>::max()) {
        NearestMap[BestI] = NewI;
        NearestDistMap[BestI] = BestD;
    }
}


bool CAlignDistGraph::GetAndRemoveNearestPair(TEquivList& First, TEquivList& Second)
{
    if(AlignEquivMap.empty() || AlignEquivMap.size() == 1) {
        return false;
    }

    TSeqPos MinD = numeric_limits<TSeqPos>::max();
    size_t MinI = numeric_limits<size_t>::max();
    ITERATE(TIndexIndexMap, IndexIter, NearestMap) {
        size_t CI = IndexIter->first;
        TSeqPos CD = NearestDistMap[CI];
        if(CD < MinD) {
            MinD = CD;
            MinI = CI;
        }
    }

    size_t OtherI = NearestMap[MinI];
#ifdef MERGE_TREE_VERBOSE_DEBUG
    cerr << "Merging " << MinI << " x " << OtherI 
        << " of " << /*Aligns.size() <<*/ " (" << AlignEquivMap.size() << ")" << endl; 
#endif
    
    const TEquivList& MinEs = AlignEquivMap[MinI];
    const TEquivList& OtherEs = AlignEquivMap[OtherI];

    First.insert(First.end(), MinEs.begin(), MinEs.end());
    Second.insert(Second.end(), OtherEs.begin(), OtherEs.end());


    NearestMap.erase(MinI);
    NearestMap.erase(OtherI);
    NearestDistMap.erase(MinI);
    NearestDistMap.erase(OtherI);
    AlignEquivMap.erase(MinI);
    AlignEquivMap.erase(OtherI);
    
    ITERATE(TIndexIndexMap, NearestIter, NearestMap) {
        if (NearestIter->second == MinI ||
            NearestIter->second == OtherI) {
            //x_CalculateAllDistances();
            NearestDistMap[NearestIter->first] = numeric_limits<TSeqPos>::max();
            x_CalculateOneDistance(NearestIter->first);
            //break;
        }
    }


    return true;
}


bool CAlignDistGraph::GetLastAlignEquivs(TEquivList& LastEquivs)
{
    if(AlignEquivMap.empty() || AlignEquivMap.size() != 1) {
        return false;
    }

    ITERATE(TAlignEquivListMap, AlignIter, AlignEquivMap) {
        const TEquivList& MinEs = AlignIter->second;
        LastEquivs.insert(LastEquivs.end(), MinEs.begin(), MinEs.end());
        break;
    }

    return true;
}


void CAlignDistGraph::RemoveEquivs(const TEquivList& RemoveEquivs) 
{
    TAlignEquivListMap RemoveGroups;

    ITERATE(TEquivList, RemoveIter, RemoveEquivs) {
        RemoveGroups[RemoveIter->AlignId].push_back(*RemoveIter);
    }

    ERASE_ITERATE(TAlignEquivListMap, AlignIter, AlignEquivMap) {        
        size_t CurrIndex = AlignIter->first;
        TEquivList& CurrEquivs = AlignIter->second;
        
        if(RemoveGroups.find(CurrEquivs.front().AlignId) == RemoveGroups.end()) {
            continue;
        }

        const TEquivList& RemoveGroupEquivs = RemoveGroups[CurrEquivs.front().AlignId];
        
        TEquivList Origs, Splits;
        Origs.insert(Origs.end(), CurrEquivs.begin(), CurrEquivs.end());
        Origs.insert(Origs.end(), RemoveGroupEquivs.begin(), RemoveGroupEquivs.end());
        m_EquivRangeBuilder.SplitIntersections(Origs, Splits);
        Origs.clear();

        TEquivList Remaining;
        ITERATE(TEquivList, SplitIter, Splits) {
            bool Keep = true;
            ITERATE(TEquivList, RemoveIter, RemoveGroupEquivs) {
                if (SplitIter->AlignId == RemoveIter->AlignId &&
                    SplitIter->SegmtId == RemoveIter->SegmtId) {
                    if(SplitIter->IntersectingWith(*RemoveIter)) {
                        Keep = false;
                        break;
                    }
                }
            }
            if(Keep) {
                Remaining.push_back(*SplitIter);
            }
        }
        CurrEquivs.clear();
        m_EquivRangeBuilder.MergeAbuttings(Remaining, CurrEquivs);
    

        if(CurrEquivs.empty()) {
            NearestMap.erase(CurrIndex);
            NearestDistMap.erase(CurrIndex);
            AlignEquivMap.erase(AlignIter);
        }
    }
   

    ITERATE(TAlignEquivListMap, RemoveGroupIter, RemoveGroups) {
        ITERATE(TIndexIndexMap, NearestIter, NearestMap) {
            if (NearestIter->second == RemoveGroupIter->first) {
                //x_CalculateAllDistances();
                NearestDistMap[NearestIter->first] = numeric_limits<TSeqPos>::max();
                x_CalculateOneDistance(NearestIter->first);
                //break;
            }
        }
    }
}


void CAlignDistGraph::x_CalculateAllDistances()
{
    NearestMap.clear();
    NearestDistMap.clear();

    vector<size_t> Indexes;
    ITERATE(TAlignEquivListMap, AlignIter, AlignEquivMap) {
        Indexes.push_back(AlignIter->first);
    }

    size_t OII, III;
    size_t OI, II;
    for( OII = 0; OII < Indexes.size(); OII++) {
        OI = Indexes[OII];
        TSeqPos BestDist = numeric_limits<TSeqPos>::max();
        size_t BestI = 0;
        if(NearestMap.find(OI) != NearestMap.end()) {
            BestI = NearestMap[OI];
            BestDist = NearestDistMap[OI];
        }
        bool Found = false;
        for( III = OII+1; III < Indexes.size(); III++) {
        //for( III = 0; III < Indexes.size(); III++) {
        //    if(OII == III)
        //        continue;
            II = Indexes[III];
            TSeqPos CurrDist = CEquivRange::Distance(AlignEquivMap[OI], AlignEquivMap[II]);
            if(CurrDist < BestDist) {
                BestI = II;
                BestDist = CurrDist;
                Found = true;
            }
        }
        if(Found) {
            NearestMap[OI] = BestI;
            NearestDistMap[OI] = BestDist;
            if (NearestMap.find(BestI) == NearestMap.end() || 
                NearestDistMap[BestI] > BestDist) {
                NearestMap[BestI] = OI; 
                NearestDistMap[BestI] = BestDist;
            }
        }
    }
}

void CAlignDistGraph::x_CalculateOneDistance(size_t CalcIndex)
{
    TSeqPos BestDist = numeric_limits<TSeqPos>::max();
    size_t BestI = 0;
    
    bool Found = false;
    ITERATE(TAlignEquivListMap, AlignIter, AlignEquivMap) {
        
        size_t CurrIndex = AlignIter->first;
        if(CurrIndex == CalcIndex)
            continue;

        TSeqPos CurrDist = CEquivRange::Distance(AlignEquivMap[CalcIndex], AlignEquivMap[CurrIndex]);
        if(CurrDist < BestDist) {
            BestI = CurrIndex;
            BestDist = CurrDist;
            Found = true;
        }
    }

    if(Found) {
        NearestMap[CalcIndex] = BestI;
        NearestDistMap[CalcIndex] = BestDist;
        if (NearestMap.find(BestI) == NearestMap.end() || 
            NearestDistMap[BestI] > BestDist) {
            NearestMap[BestI] = CalcIndex; 
            NearestDistMap[BestI] = BestDist;
        }
    }
}

int s_UniformAlignId(const TEquivList& Equivs) 
{
    if (Equivs.empty())
        return -1;

    bool Uniform = true;
    ITERATE(TEquivList, EquivIter, Equivs) {
        if(EquivIter->AlignId != Equivs.front().AlignId) {
            Uniform = false;
            break;
        }
    }

    if (Uniform)
        return Equivs.front().AlignId;
    
    return -1;
}

void 
CTreeAlignMerger::x_Merge_Dist_Impl(TAlignVec& Aligns, 
                                    CSeq_id_Handle QueryIDH, CSeq_id_Handle SubjtIDH,
                                    CBioseq_Handle QueryBSH, CBioseq_Handle SubjtBSH,
                                    list< CRef<objects::CSeq_align> >& Output)
{
    const int PASSES_BEFORE_EARLY_REMOVE = 5;
    list<CRef<CSeq_align> > EarlyRemoves;

    CEquivRangeBuilder EquivRangeBuilder;
    
    CAlignDistGraph OriginalGraph(EquivRangeBuilder);

#ifdef MERGE_TREE_VERBOSE_DEBUG
    cerr << "Starting : " << Aligns.size() << endl;
#endif

    // Sanity checking, same bases in as out
    TSeqPos OrigMatches = 0, AllMergeMatches = 0;
    
    ITERATE(TAlignVec, AlignIter, Aligns) {
        TEquivList Temp;
        EquivRangeBuilder.ExtractRangesFromSeqAlign(**AlignIter, Temp);
        if(QueryBSH && SubjtBSH) {
            EquivRangeBuilder.CalcMatches(QueryBSH, SubjtBSH, Temp);
        } else { 
            NON_CONST_ITERATE(TEquivList, Iter, Temp) {
                Iter->Matches  = Iter->Query.GetLength();
                Iter->MisMatches = 0;
            }
        }
        ITERATE(TEquivList, EquivIter, Temp) {
            OrigMatches += EquivIter->Matches;
        }
        OriginalGraph.AddAlignment(Temp);
    }

    OriginalGraph.CalculateAllDistances();
 
    int PassCounter = 0;
    while(!OriginalGraph.Empty()) {
        CAlignDistGraph CurrGraph(OriginalGraph);
     
        // Sort dists. Pick smallest Dist.
        // TreeMerge the two aligns with the smallest Dist.
        // Block out those 2, Insert the Merged result. 
        // Repeat until the list only contains 1.
        
        while(CurrGraph.Size() > 1) {

            TEquivList MinEs, OtherEs;
            CurrGraph.GetAndRemoveNearestPair(MinEs, OtherEs);
           
            if(MinEs.empty() || OtherEs.empty()) {
                if(!MinEs.empty())
                    CurrGraph.AddAlignment(MinEs);
                else if(!OtherEs.empty())
                    CurrGraph.AddAlignment(OtherEs);
                continue;
            }
           
            TEquivList CurrEquivs;
            CurrEquivs.insert(CurrEquivs.end(), MinEs.begin(), MinEs.end());
            CurrEquivs.insert(CurrEquivs.end(), OtherEs.begin(), OtherEs.end());
            sort(CurrEquivs.begin(), CurrEquivs.end());
            TEquivList SplitEquivs;
            EquivRangeBuilder.SplitIntersections(CurrEquivs, SplitEquivs);
        
            CMergeTree Tree;
            if(Callback != NULL)
                Tree.SetInterruptCallback(Callback, CallbackData);
            Tree.SetScoring(m_Scoring);
            Tree.AddEquivs(SplitEquivs);
            //Tree.Print(cout);
            
            TEquivList Path;
            Tree.Search(Path, true);
            
            // Only happens if the Tree's Interrupt is triggered
            if(Path.empty()) {
                Output.insert(Output.end(), EarlyRemoves.begin(), EarlyRemoves.end());
                return;
            }

            TEquivList Abutted;
            EquivRangeBuilder.MergeAbuttings(Path, Abutted);
            sort(Abutted.begin(), Abutted.end());
           
            if(!Abutted.empty())
                CurrGraph.AddAlignment(Abutted);

            // Early Removes. In large enough data sets, going through the N-th 
            //   passes to find every single base a home is slow, and mostly 
            //   pointless. Most of this grinding is repeatedly trying to merge 
            //   low-quality whole alignment with others like it, but never 
            //   creating a new alignment, just picking the better-scoring of the two.
            //   After the first few passes, the lower scoring alignments of such pairings
            //   can just be removed from consideration early, because they are not 
            //   contributing to any merges. 
            if(!Abutted.empty() && PassCounter > PASSES_BEFORE_EARLY_REMOVE) {{
                int ResultId = s_UniformAlignId(Abutted);
                if(ResultId != -1) {
                    int MinEId = s_UniformAlignId(MinEs);
                    int OtherEId = s_UniformAlignId(OtherEs);
                    if ( (Abutted.size() == MinEs.size() &&
                          ResultId == MinEId) ||
                         (Abutted.size() == OtherEs.size() &&
                          ResultId == OtherEId) ) {
                        
                        // Merges that go entirely one-way, 
                        // we can just remove the un-used whole alignment
                        // from future consideration
                        TEquivList* LoserEs = NULL;

                        if(ResultId == MinEId) {
                            LoserEs = &OtherEs;
                        } else if(ResultId == OtherEId) {
                            LoserEs = &MinEs;
                        }
                        if(LoserEs != NULL) {
                            ITERATE(TEquivList, EquivIter, *LoserEs) {
                                AllMergeMatches += EquivIter->Matches;
                            }
                            CRef<CSeq_align> Merged;
                            Merged = x_MakeSeqAlign(*LoserEs, QueryIDH, SubjtIDH);
                            if(Merged)
                                EarlyRemoves.push_back(Merged);           
                            OriginalGraph.RemoveEquivs(*LoserEs);
                        }
                    }
                }
            }}

            {{
#ifdef MERGE_TREE_VERBOSE_DEBUG
                //s_EquivDiff(CurrEquivs, Abutted);
                int MinScore = Tree.Score(MinEs);
                int OtherScore = Tree.Score(OtherEs);
                int NewScore = Tree.Score(Abutted);
                cerr << " Scores: " << MinScore << " x " << OtherScore << " => " << NewScore << endl; 
                if(NewScore < MinScore || NewScore < OtherScore) {
                    cerr << "BACKWARDS!" << endl;
                }
                //s_FindBestSubRange(Tree, Abutted);
#endif
            }}
        
        } // end while CurrGraph
    
        if(!CurrGraph.Empty()) {
            TEquivList LastEquivs;
            CurrGraph.GetLastAlignEquivs(LastEquivs);
            ITERATE(TEquivList, EquivIter, LastEquivs) {
                AllMergeMatches += EquivIter->Matches;
            }
            if(!LastEquivs.empty()) {
                CRef<CSeq_align> Merged;
                Merged = x_MakeSeqAlign(LastEquivs, QueryIDH, SubjtIDH);
                if(Merged)
                    Output.push_back(Merged);
            }
            OriginalGraph.RemoveEquivs(LastEquivs);
        }

        PassCounter++;
    } // end while OriginalGraph
    
    Output.insert(Output.end(), EarlyRemoves.begin(), EarlyRemoves.end());

#ifdef MERGE_TREE_VERBOSE_DEBUG
    cerr << "Orig : " << OrigMatches << " vs " << AllMergeMatches << endl; 
    if(OrigMatches != AllMergeMatches)
        cerr << "MATCHES PROBLEM!" << endl;
#endif
}


END_NCBI_SCOPE


