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
#include <cmath>

#include <objmgr/util/sequence.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

#include <algo/align/mergetree/bitvec.hpp>
#include <algo/align/mergetree/equiv_range.hpp>
#include <algo/align/mergetree/merge_tree_core.hpp>

/* Clear out any possible macro definition of CS, which shows up on
 * Solaris/x86 when building with GCC (per sys/ucontext.h).
 */
#ifdef CS
#  undef CS
#endif

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);

bool operator<(const CMergeNode& A, const CMergeNode& B) {
    return (A.Equiv < B.Equiv);
}
bool operator<(const TMergeNode& A, const TMergeNode& B) {
    if(A.IsNull())
        return true;
    if(B.IsNull())
        return false;
    return ((*A) < (*B));
}


CMergeTree::~CMergeTree()
{
    NON_CONST_ITERATE(TNodeCache, CacheIter, m_NodeCache) {
        CacheIter->second->Parents.clear();
        CacheIter->second->Children.clear();
        CacheIter->second->BestChild.Reset(NULL);
    }
    m_NodeCache.clear();
    

    // The tree is doubly-linked, defeating CRef re-counting
    // So it needs to be explicitly dismantled
    // The CRef's will handle their own deleting, just the
    // Parent and Children sets need to be cleared out.
    vector< TMergeNode > Children;
    Children.push_back(m_Root);
    
    while(!Children.empty()) {
        TMergeNode Curr = Children.back();
        Children.pop_back();

        Curr->Parents.clear();

        ITERATE(set<TMergeNode>, ChildIter, Curr->Children) {
            ChildIter->GetNCPointer()->Parents.clear();
            Children.push_back(*ChildIter);
        }
        Curr->Children.clear();
        Curr->BestChild.Reset(NULL);
    }

    m_Root.Reset(NULL);
    m_Leaves.clear();
}


void CMergeTree::x_AddChild(TMergeNode Parent, TMergeNode Child)
{
    m_Leaves.erase(Parent);
    Parent->Children.insert(Child);
}
void CMergeTree::x_RemoveChild(TMergeNode Parent, TMergeNode Child) 
{
    Parent->Children.erase(Child);
    if(Parent->Children.empty())
        m_Leaves.insert(Parent);
}
void CMergeTree::x_AddParent(TMergeNode Child, TMergeNode Parent)
{
    m_Leaves.erase(Parent);
    Child->Parents.insert(Parent);
}
void CMergeTree::x_RemoveParent(TMergeNode Parent, TMergeNode Child) 
{
    Child->Parents.erase(Parent);
    if(Parent->Children.empty())
        m_Leaves.insert(Parent);
}

void CMergeTree::x_LinkNodes(TMergeNode Parent, TMergeNode Child)
{
    Parent->Children.insert(Child);
    Child->Parents.insert(Parent);
    m_Leaves.erase(Parent);
    if(Child->Children.empty())
        m_Leaves.insert(Child);
}
void CMergeTree::x_UnLinkNodes(TMergeNode Parent, TMergeNode Child)
{
    Parent->Children.erase(Child);
    Child->Parents.erase(Parent);
    if(Parent->Children.empty())
        m_Leaves.insert(Parent);
}



TMergeNode CMergeTree::x_GetNode(CEquivRange Equiv)
{
    TNodeCache::iterator Found = m_NodeCache.find(Equiv.SplitId);
    if(Found != m_NodeCache.end())
        return Found->second;
    else  {
        TMergeNode NewNode(new CMergeNode(Equiv, m_NodeIdCounter));
        m_NodeIdCounter++;
        m_NodeCache[NewNode->Equiv.SplitId] = NewNode;
        return NewNode;
    }
}


bool s_SortMergeNodeByQuery(const TMergeNode& A, const TMergeNode& B) {
    if(A->Equiv.Query.GetFrom() != B->Equiv.Query.GetFrom())
        return (A->Equiv.Query.GetFrom() < B->Equiv.Query.GetFrom());
    else if(A->Equiv.Query.GetTo() != B->Equiv.Query.GetTo())
        return (A->Equiv.Query.GetTo() < B->Equiv.Query.GetTo());
    else if(A->Equiv.Subjt.GetFrom() != B->Equiv.Subjt.GetFrom())
        return (A->Equiv.Subjt.GetFrom() < B->Equiv.Subjt.GetFrom());
    else if(A->Equiv.Subjt.GetTo() != B->Equiv.Subjt.GetTo())
        return (A->Equiv.Subjt.GetTo() < B->Equiv.Subjt.GetTo());
    else
        return A->Equiv.Strand < B->Equiv.Strand;
}

bool s_SortMergeNodeByQuery_Minus(const TMergeNode& A, const TMergeNode& B) {
    if(A->Equiv.Query.GetFrom() != B->Equiv.Query.GetFrom())
        return (A->Equiv.Query.GetFrom() > B->Equiv.Query.GetFrom());
    else if(A->Equiv.Query.GetTo() != B->Equiv.Query.GetTo())
        return (A->Equiv.Query.GetTo() > B->Equiv.Query.GetTo());
    else if(A->Equiv.Subjt.GetFrom() != B->Equiv.Subjt.GetFrom())
        return (A->Equiv.Subjt.GetFrom() < B->Equiv.Subjt.GetFrom());
    else if(A->Equiv.Subjt.GetTo() != B->Equiv.Subjt.GetTo())
        return (A->Equiv.Subjt.GetTo() < B->Equiv.Subjt.GetTo());
    else
        return A->Equiv.Strand < B->Equiv.Strand;
}



bool s_SortMergeNodeBySubjt(const TMergeNode& A, const TMergeNode& B) {
    if(A->Equiv.Subjt.GetFrom() != B->Equiv.Subjt.GetFrom())
        return (A->Equiv.Subjt.GetFrom() < B->Equiv.Subjt.GetFrom());
    else if(A->Equiv.Subjt.GetTo() != B->Equiv.Subjt.GetTo())
        return (A->Equiv.Subjt.GetTo() < B->Equiv.Subjt.GetTo());
    else if(A->Equiv.Query.GetFrom() != B->Equiv.Query.GetFrom())
        return (A->Equiv.Query.GetFrom() < B->Equiv.Query.GetFrom());
    else if(A->Equiv.Query.GetTo() != B->Equiv.Query.GetTo())
        return (A->Equiv.Query.GetTo() < B->Equiv.Query.GetTo());
    else
        return A->Equiv.Strand < B->Equiv.Strand;
}



void CMergeTree::AddEquiv(CEquivRange NewEquiv) 
{
    bitvec<unsigned int> Explored(512), Inserted(512);
        
    TMergeNode NewNode = x_GetNode(NewEquiv);
   
    if(m_Root->Children.empty()) {
        x_LinkNodes(m_Root, NewNode);
        return;
    }
    
    // Quickest checks are shallow
    // So, first shallow root-childs
    // Then shallow leafs (DO NOT, DAMAGES RESULTS)
    // Then deep checks (leaves up, then root down)
    // FIXME: Make a smart way to choose between up or down search ?
    //        Doing a full Up when the Before is 3 nodes from the top is a waste
    //        Same for Down, when its near a leaf
    
    set<TMergeNode> Befores, Afters;
    bool HasAfters = false;
    ITERATE(set<TMergeNode>, RootChildIter, m_Root->Children) {
        CEquivRange::ERelative Rel = (*RootChildIter)->Equiv.CalcRelative(NewNode->Equiv);
        if(Rel == CEquivRange::eAfter) { 
            HasAfters = true;
            break;
        }
    }

    if(HasAfters) { 
        //size_t BUps=0, BDowns=0;
        int MaxUD=0, MaxDD=0;
        if(Befores.empty()) {
            ITERATE(set<TMergeNode>, LeafIter, m_Leaves) {
                int Depth= 0;
                //x_FindBefores_Up_Recur(NewNode, *LeafIter, Befores, Explored, Inserted, Depth);
                x_FindBefores_Up_Iter(NewNode, *LeafIter, Befores, Explored, Inserted, Depth);
                MaxUD = max(MaxUD, Depth);
            }
            Explored.clear(); Inserted.clear();
            //BUps = Befores.size();
        }
        
        if(Befores.empty()) {
            // this is never called, x_FindBefores_Up finds everything
            // and because of input sorting, finds them in less time than top-down
            x_FindBefores(NewNode, m_Root, Befores, Explored, Inserted, MaxDD);
            Explored.clear(); Inserted.clear();
            //BDowns = Befores.size();
        }

        #ifdef MERGE_TREE_VERBOSE_DEBUG
        /*
        TSeqPos RootD = numeric_limits<TSeqPos>::max();
        TSeqPos LeafD = numeric_limits<TSeqPos>::max();
        ITERATE(set<TMergeNode>, RootChildIter, m_Root->Children) {
            RootD = min(RootD, CEquivRange::Distance( (*RootChildIter)->Equiv, NewNode->Equiv));
        }
        ITERATE(set<TMergeNode>, LeafIter, m_Leaves) {
            LeafD = min(LeafD, CEquivRange::Distance( (*LeafIter)->Equiv, NewNode->Equiv));
        }

        //if(MaxDD < MaxUD) {
        if(RootD < LeafD) {
        cerr << "AddEquiv " << NewNode->Equiv << " " << BUps << " v " << BDowns << endl;
        cerr << "AE Depth: " << MaxUD << " v " << MaxDD << endl;
        cerr << "AE Dists: " << LeafD << " v " << RootD << endl;
        }
        */
        #endif
    }


    if(Befores.empty())
        Befores.insert(m_Root);

    ITERATE(set<TMergeNode>, BeforeIter, Befores) {
        TMergeNode BeforeNode = *BeforeIter;
        ITERATE(set<TMergeNode>, AfterIter, Afters) {
            TMergeNode AfterNode = *AfterIter;
            x_UnLinkNodes(BeforeNode, AfterNode);
        }
    }
    
    ITERATE(set<TMergeNode>, BeforeIter, Befores) {
        TMergeNode BeforeNode = *BeforeIter;
        NewNode->Parents.insert(BeforeNode);
        x_LinkNodes(BeforeNode, NewNode);
        ITERATE(set<TMergeNode>, AfterIter, Afters) {
            TMergeNode AfterNode = *AfterIter;
            x_LinkNodes(NewNode, AfterNode);
        }
    }  

}


void CMergeTree::AddEquivs(const TEquivList& Equivs)
{
    TMergeNodeVec QVec, SVec;
     
    ITERATE(TEquivList, EquivIter, Equivs) {
        TMergeNode NewNode = x_GetNode(*EquivIter);
        QVec.push_back(NewNode);
        SVec.push_back(NewNode);
    }

    if(QVec.front()->Equiv.Strand == eNa_strand_plus)
        sort(QVec.begin(), QVec.end(), s_SortMergeNodeByQuery);
    else
        sort(QVec.begin(), QVec.end(), s_SortMergeNodeByQuery_Minus);
    
    sort(SVec.begin(), SVec.end(), s_SortMergeNodeBySubjt);
    // print interleaving of these? 
    
    int QI = 0;
    TSeqPos PQ = QVec[0]->Equiv.Query.GetFrom();
    QVec[0]->QI = QI;
    for(size_t I = 1; I < QVec.size(); I++) {
        TSeqPos CQ = QVec[I]->Equiv.Query.GetFrom();
        if(PQ != CQ) {
            QI++;
            PQ = CQ;
        }
        QVec[I]->QI = QI;
    }
    
    int SI = 0;
    TSeqPos PS = SVec[0]->Equiv.Subjt.GetFrom();
    SVec[0]->SI = SI;
    for(size_t I = 1; I < SVec.size(); I++) {
        TSeqPos CS = SVec[I]->Equiv.Subjt.GetFrom();
        if(PS != CS) {
            SI++;
            PS = CS;
        }
        SVec[I]->SI = SI;
    }
   
    // unsort
    if(QVec.front()->Equiv.Strand == eNa_strand_minus)
        sort(QVec.begin(), QVec.end(), s_SortMergeNodeByQuery);
    

    //ITERATE(TMergeNodeVec, QIter, QVec) {
    //    cerr << (*QIter)->QI  << "\t"
    //        << (*QIter)->SI << "\t"
    //        << (*QIter)->Equiv 
    //       << endl;
    //}
    

    ITERATE(TMergeNodeVec, Iter, QVec) {
        AddEquiv( (*Iter)->Equiv );
    }

}


void CMergeTree::x_FindLeafs(TMergeNode Curr, set<TMergeNode>& Leafs, TBitVec& Explored)
{
    if(Explored.get(Curr->Id)) { 
        return;
    }
    Explored.set(Curr->Id, true);

    if(Curr->Children.empty()) {
        Leafs.insert(Curr);
        return;
    }

    ITERATE(set<TMergeNode>, ChildIter, Curr->Children) {
        x_FindLeafs(*ChildIter, Leafs, Explored);
    }
}


bool CMergeTree::x_FindBefores(TMergeNode New, TMergeNode Curr, set<TMergeNode>& Befores, 
                               TBitVec& Explored, TBitVec& Inserted, int& Depth) 
{
    Depth++;
    if(Explored.get(Curr->Id)) { 
        return Inserted.get(Curr->Id);
    }

    Explored.set(Curr->Id, true);

    CEquivRange::ERelative Rel = New->Equiv.CalcRelative(Curr->Equiv);

    if(Rel == CEquivRange::eAfter && !Curr->Equiv.Empty()) {
        return false;
    }
    else {        
        bool SubInsert = false;
        ITERATE(set<TMergeNode>, ChildIter, Curr->Children) {
            SubInsert |= x_FindBefores(New, *ChildIter, Befores, Explored, Inserted, Depth);
        }
        
        if(SubInsert) {
            //cerr << "SubInserted: " << s_RelToStr(Rel) << "\t" 
            //    << "("<<New->QI<<","<<New->SI<<")" << "\t"
            //    << "("<<Curr->QI<<","<<Curr->SI<<")" << "\t"
            //    << "\t" << New->Equiv << "\t" << Curr->Equiv << endl;
            Inserted.set(Curr->Id, true);
            return true;
        } else if(!SubInsert && Rel == CEquivRange::eBefore) {
            Befores.insert(Curr);
            Inserted.set(Curr->Id, true);
            //cerr << "Inserted: " << s_RelToStr(Rel) << "\t" 
            //    << "("<<New->QI<<","<<New->SI<<")" << "\t"
            //    << "("<<Curr->QI<<","<<Curr->SI<<")" << "\t"
            //    << "\t" << New->Equiv << "\t" << Curr->Equiv << endl;
            return true;
        } else {
            return false;
        }
    }
}


bool CMergeTree::x_FindAfters(TMergeNode New, TMergeNode Curr, set<TMergeNode>& Afters, 
                              TBitVec& Explored, TBitVec& Inserted) 
{
    if(Explored.get(Curr->Id)) { 
        return Inserted.get(Curr->Id);
    }

    Explored.set(Curr->Id, true);

    CEquivRange::ERelative Rel = New->Equiv.CalcRelative(Curr->Equiv);

    if(Rel == CEquivRange::eBefore) {
        return false;
    }
    else if(Rel == CEquivRange::eAfter && !Curr->Equiv.Empty()) {
       // cerr << "After: " << New->Equiv << "\t" << Curr->Equiv << endl;
        Afters.insert(Curr);
        Inserted.set(Curr->Id, true);
        return true;
    } else {
        bool SubInsert = !Curr->Children.empty();
        ITERATE(set<TMergeNode>, ChildIter, Curr->Children) {
            SubInsert &= x_FindAfters(New, *ChildIter, Afters, Explored, Inserted);
        }
        
        if(SubInsert) {
            Inserted.set(Curr->Id, true);
            return true;
        } else {
            return false;
        }
    }

    return false;
}



bool CMergeTree::x_FindBefores_Up_Recur(TMergeNode New, TMergeNode Curr, set<TMergeNode>& Befores, 
                              TBitVec& Explored, TBitVec& Inserted, int& Depth) 
{
    Depth++;
    if(Explored.get(Curr->Id)) { 
        return Inserted.get(Curr->Id);
    }
    Explored.set(Curr->Id, true);

    if(Curr->Equiv.Empty())
        return false;


    CEquivRange::ERelative Rel = New->Equiv.CalcRelative(Curr->Equiv);

    //cerr << "BeforeU: " << s_RelToStr(Rel) << "\t" 
    //    << "("<<New->QI<<","<<New->SI<<")" << "\t"
    //    << "("<<Curr->QI<<","<<Curr->SI<<")" << "\t"
    //    << "\t" << New->Equiv << "\t" << Curr->Equiv << endl;
    
    if(Rel == CEquivRange::eAfter) {
        return false;
    } 
    else if(Rel == CEquivRange::eBefore) {
        ERASE_ITERATE(set<TMergeNode>, Others, Befores) {
            CEquivRange::ERelative Rel = (*Others)->Equiv.CalcRelative(Curr->Equiv);
            //cerr << __LINE__ << "\t"
            //    << s_RelToStr(Rel) << "\t"
            //    << "("<<(*Others)->QI<<","<<(*Others)->SI<<")"<<"\t"
            //    << "("<<Curr->QI<<","<<Curr->SI<<")"<<"\t"
            //    << endl;
            if(Rel == CEquivRange::eBefore) {
                return false;
            } else if(Rel == CEquivRange::eAfter) {
                Befores.erase(Others);
            }
        }
        
        Befores.insert(Curr);
        Inserted.set(Curr->Id, true);
        return true;
    }
    else {
        bool SubInsert = false; //!Curr->Children.empty();
        ITERATE(set<TMergeNode>, ParentIter, Curr->Parents) {
            SubInsert |= x_FindBefores_Up_Recur(New, *ParentIter, Befores, Explored, Inserted, Depth);
        }
        return SubInsert;    
    }

    return false;
}


struct SFindBeforesIterFrame : public CObject {
    TMergeNode Curr;
    bool Returned;

    int VisitCount;
    list< CRef< SFindBeforesIterFrame> > ChildFrames;
};

bool CMergeTree::x_FindBefores_Up_Iter(TMergeNode New, TMergeNode StartCurr, set<TMergeNode>& Befores, 
                              TBitVec& Explored, TBitVec& Inserted, int& Depth) 
{
    vector< CRef<SFindBeforesIterFrame> > FrameStack;

    CRef<SFindBeforesIterFrame> FirstFrame(new SFindBeforesIterFrame);
    FirstFrame->Curr = StartCurr;
    FirstFrame->Returned = false;
    FirstFrame->VisitCount = 0;
    FrameStack.push_back(FirstFrame);

    while(!FrameStack.empty()) {
//cerr << FrameStack.size() << "  " << New->Equiv << endl;
        CRef<SFindBeforesIterFrame> Frame = FrameStack.back();
       
        if(Frame->Curr.IsNull()) {
            FrameStack.pop_back();
            continue;
        }

        Depth++;
        if(Explored.get(Frame->Curr->Id)) { 
            Frame->Returned = Inserted.get(Frame->Curr->Id); 
            FrameStack.pop_back();
            continue;
        }
        Explored.set(Frame->Curr->Id, true);

        if(Frame->Curr->Equiv.Empty()) {
            Frame->Returned = false;
            FrameStack.pop_back();
            continue;
        }


        CEquivRange::ERelative Rel = New->Equiv.CalcRelative(Frame->Curr->Equiv);

        //cerr << "BeforeU: " << s_RelToStr(Rel) << "\t" 
        //    << "("<<New->QI<<","<<New->SI<<")" << "\t"
        //    << "("<<Curr->QI<<","<<Curr->SI<<")" << "\t"
        //    << "\t" << New->Equiv << "\t" << Curr->Equiv << endl;
        
        if(Rel == CEquivRange::eAfter) {
            Frame->Returned = false;
            FrameStack.pop_back();
            continue;
        } 
        else if(Rel == CEquivRange::eBefore) {
            bool BeforeFound = false;
            ERASE_ITERATE(set<TMergeNode>, Others, Befores) {
                CEquivRange::ERelative Rel = (*Others)->Equiv.CalcRelative(Frame->Curr->Equiv);
                //cerr << __LINE__ << "\t"
                //    << s_RelToStr(Rel) << "\t"
                //    << "("<<(*Others)->QI<<","<<(*Others)->SI<<")"<<"\t"
                //    << "("<<Curr->QI<<","<<Curr->SI<<")"<<"\t"
                //    << endl;
                if(Rel == CEquivRange::eBefore) {
                    BeforeFound = true;
                    break;
                } else if(Rel == CEquivRange::eAfter) {
                    Befores.erase(Others);
                }
            }
           
            if(BeforeFound) {
                Frame->Returned = false;
                FrameStack.pop_back();
                continue;
            }
            
            Befores.insert(Frame->Curr);
            Inserted.set(Frame->Curr->Id, true);

            Frame->Returned = true;
            FrameStack.pop_back();
            continue;
        }
        else {
            if(Frame->VisitCount == 0) {
                Explored.set(Frame->Curr->Id, false); // unset the non-backtrack Explored
                ITERATE(set<TMergeNode>, ParentIter, Frame->Curr->Parents) {
                    CRef<SFindBeforesIterFrame> NewFrame(new SFindBeforesIterFrame);
                    NewFrame->Curr = *ParentIter;
                    NewFrame->Returned = false;
                    NewFrame->VisitCount = 0;
                    FrameStack.push_back(NewFrame);
                    Frame->ChildFrames.push_back(NewFrame);
                }
                Frame->VisitCount++;
                continue;
            } else {
                bool SubInsert = false; //!Curr->Children.empty();
                ITERATE(list<CRef<SFindBeforesIterFrame> >, ChildFrameIter, Frame->ChildFrames) {
                    SubInsert |= (*ChildFrameIter)->Returned;
                }
                Frame->ChildFrames.clear();
                Frame->Returned = SubInsert;
                FrameStack.pop_back();
                continue;
            }
        }
        
        Frame->Returned = false;
        FrameStack.pop_back();
        continue;
    }  // end stack loop
    
    FirstFrame->ChildFrames.clear();
    return FirstFrame->Returned;
}



bool CMergeTree::x_FindAfters_Up(TMergeNode New, TMergeNode Curr, set<TMergeNode>& Afters, 
                              TBitVec& Explored, TBitVec& Inserted) 
{
    if(Explored.get(Curr->Id)) { 
        return Inserted.get(Curr->Id);
    }


    Explored.set(Curr->Id, true);

    CEquivRange::ERelative Rel = New->Equiv.CalcRelative(Curr->Equiv);

    if(Rel == CEquivRange::eBefore) {
        return false;
    }
    else {
        bool SubInsert = false; //!Curr->Children.empty();
        ITERATE(set<TMergeNode>, ParentIter, Curr->Parents) {
            SubInsert |= x_FindAfters_Up(New, *ParentIter, Afters, Explored, Inserted);
        }
        
        if(SubInsert) {
            Inserted.set(Curr->Id, true);
            return true;
        } else if(Rel == CEquivRange::eAfter && !Curr->Equiv.Empty()) {
            Afters.insert(Curr);
            Inserted.set(Curr->Id, true);
        //    cerr << "AfterU: " << New->Equiv << "\t" << Curr->Equiv << endl;
            return true;
        } else {
            return false;
        }
    }

    return false;
}




bool CMergeTree::IsEmpty() const 
{
    return m_Root->Children.empty();
}

size_t CMergeTree::Size() const 
{
    TBitVec Explored(128);
    return x_CountChildNodes(m_Root, Explored)-1;
}
size_t CMergeTree::Links() const 
{
    TBitVec Explored(128);
    return x_CountChildLinks(m_Root, Explored);
}

void CMergeTree::Search(TEquivList& Path, bool Once) 
{
    ssize_t BestChildScore = numeric_limits<ssize_t>::min();
    TMergeNode BestChild, UnBest;
   
    m_InterruptCounter = 0;

    TBitVec Explored(512);
    NON_CONST_ITERATE(set<TMergeNode>, ChildNodeIter, m_Root->Children) {
        if((m_InterruptCounter++ % INTERRUPT_CHECK_RATE) == 0)
            x_CheckInterruptCallback();
        if(m_Interrupted)
            return;

        //TMergeNode Result = x_Search_Recur( *ChildNodeIter, Explored, UnBest);    
        TMergeNode Result = x_Search_Iter( *ChildNodeIter, Explored, UnBest);    
        //Result = *ChildNodeIter;

        if (Result &&  Result->ChainScore > BestChildScore) {
            BestChildScore = Result->ChainScore;
            BestChild = Result;
        } 
    }


    if (UnBest.NotNull() && BestChild.NotNull()) { 
        if(UnBest->ChainScore > BestChild->ChainScore) {
            BestChild = UnBest;
        }
    } else if(UnBest.NotNull() && BestChild.IsNull()) {
        BestChild = UnBest;
    }
   
    //Print(cout);
    //x_Dot(cerr, m_Root);
  

    if(BestChild.IsNull()) {
        return ;
    } else {
        Path.clear();
        Path.reserve(512);

        TBitVec RebuildExplore(128);
        
        TMergeNode CurrChild = BestChild;
        while(!CurrChild.IsNull()) {
            
            if(CurrChild->ChainScore > BestChild->ChainScore)
                Path.clear();

            Path.push_back(CurrChild->Equiv);
            
            // Only do the messy tree re-arrengment if the tree will be re-used
            if(!Once) {            
                ERASE_ITERATE(set<TMergeNode>, ParentIter, CurrChild->Parents) {
                    TMergeNode Parent = *ParentIter;
                    Parent->BestChild.Reset(NULL);
                    x_UnLinkNodes(Parent, CurrChild);

                    ERASE_ITERATE(set<TMergeNode>, ChildIter, CurrChild->Children) {
                        TMergeNode Child = *ChildIter;

                        x_UnLinkNodes(CurrChild, Child);
                      
                        RebuildExplore.clear();
                        if( !x_IsEventualChildOf(Parent, Child, RebuildExplore) ) {
                            x_LinkNodes(Parent, Child);
                        }
                    }
                }
            }
            CurrChild = CurrChild->BestChild;
        }
    }
}


// the returned result assumes that the caller always wants to use the 
//  CurrNode segment. The UnBest parameter is a side-channel that stores 
//  the global best node, so that the caller can tell if a sub-section, 
//  that does not include CurrNode is a better choice.
TMergeNode CMergeTree::x_Search_Recur(TMergeNode CurrNode, 
                                TBitVec& Explored, TMergeNode& UnBest)
{
#ifdef MERGE_TREE_VERBOSE_DEBUG
    bool DEBUG_CURR_NODE = false;
    if(CurrNode->Id == -1)
        DEBUG_CURR_NODE = true;
#endif
    

    if(Explored.get(CurrNode->Id)) {
#ifdef MERGE_TREE_VERBOSE_DEBUG
        if(DEBUG_CURR_NODE) cerr << __LINE__ << endl;
#endif
        return CurrNode;
    }

    Explored.set(CurrNode->Id, true);
    
    ssize_t SelfScore = (m_Scoring.Match * CurrNode->Equiv.Matches)
                      + (m_Scoring.MisMatch * CurrNode->Equiv.MisMatches);
    CurrNode->SelfScore = SelfScore; 
    CurrNode->ChainScore = SelfScore; 
#ifdef MERGE_TREE_VERBOSE_DEBUG
    if(DEBUG_CURR_NODE)
        cerr << "Self: " << CurrNode->SelfScore << endl;
#endif
    
    ssize_t BestChildModScore = numeric_limits<ssize_t>::min();
    TMergeNode BestChildMod;
    
    TMergeNode BestChildUnMod;

    NON_CONST_ITERATE(set<TMergeNode>, ChildNodeIter, CurrNode->Children) {
        if((m_InterruptCounter++ % INTERRUPT_CHECK_RATE) == 0)
            x_CheckInterruptCallback();
        if(m_Interrupted)
            return TMergeNode();

        TMergeNode Result;
        Result = x_Search_Recur(*ChildNodeIter, Explored, UnBest);    
      
        if(Result) {
#ifdef MERGE_TREE_VERBOSE_DEBUG
            if(DEBUG_CURR_NODE) {
                cerr << "Id: " << Result->Id << "  CScore: " << Result->ChainScore << endl;
            }
#endif
            if (BestChildUnMod.IsNull() || 
                Result->ChainScore > BestChildUnMod->ChainScore) {
#ifdef MERGE_TREE_VERBOSE_DEBUG
                if(DEBUG_CURR_NODE)
                    cerr << "  UM: " << Result->ChainScore << endl;
#endif
                BestChildUnMod = Result;
            }

            ssize_t CurrChildModScore = Result->ChainScore;
                       
            ssize_t GapOpen = 0, GapExtend = 0;
            x_EvalGap(CurrNode->Equiv, Result->Equiv, GapOpen, GapExtend);

#ifdef MERGE_TREE_VERBOSE_DEBUG
            if(DEBUG_CURR_NODE)
                cerr << " GO: " << GapOpen << "\tGE: " << GapExtend << endl;
#endif
            
            CurrChildModScore += (m_Scoring.GapOpen * GapOpen);
            CurrChildModScore += (m_Scoring.GapExtend * GapExtend);


#ifdef MERGE_TREE_VERBOSE_DEBUG
            if(DEBUG_CURR_NODE) 
                cerr << "  Mod: " << CurrChildModScore << "  Best: " << BestChildModScore << endl; 
#endif


            if (CurrChildModScore > BestChildModScore) {
                BestChildModScore = CurrChildModScore;
                BestChildMod = Result;
#ifdef MERGE_TREE_VERBOSE_DEBUG
                if(DEBUG_CURR_NODE)
                    cerr << "  New Best Mod" << endl;
#endif
            }
        }
    }

    if(BestChildUnMod.NotNull()) {
        if(UnBest.IsNull()) {
            UnBest = BestChildUnMod;
        } else if(BestChildUnMod->ChainScore > UnBest->ChainScore) {
            UnBest = BestChildUnMod;
        }
    }

   
    // Cases where Score is left zero are intended to be pass-through cases
    

    if (BestChildMod.IsNull()) {
        CurrNode->ChainScore = CurrNode->SelfScore;
        CurrNode->BestChild.Reset(NULL);
#ifdef MERGE_TREE_VERBOSE_DEBUG
        if(DEBUG_CURR_NODE) cerr << __LINE__ << endl;
#endif
        return CurrNode;
    } else {
        //if(BestChildModScore > numeric_limits<ssize_t>::min()) {
        if(BestChildModScore > 0) {
            
            ssize_t SumScore = CurrNode->SelfScore+BestChildModScore; 
            CurrNode->ChainScore = SumScore;
            CurrNode->BestChild = BestChildMod;
#ifdef MERGE_TREE_VERBOSE_DEBUG
            if(DEBUG_CURR_NODE) cerr << __LINE__ << endl;
#endif
            return CurrNode;
        } 
        else {
            CurrNode->ChainScore = CurrNode->SelfScore;
            CurrNode->BestChild.Reset(NULL);
#ifdef MERGE_TREE_VERBOSE_DEBUG
            if(DEBUG_CURR_NODE) cerr << __LINE__ << endl;
#endif
            return CurrNode;
        }
    }  

    return TMergeNode();
}


// iterative version of x_Search_Recur
struct SSearchIterFrame : public CObject {
    TMergeNode CurrNode;
    TMergeNode BestChildMod;
    TMergeNode Returned;
    ssize_t BestChildModScore;
    int VisitCount;
    
    list< CRef<SSearchIterFrame> > ChildFrames;
};

TMergeNode CMergeTree::x_Search_Iter(TMergeNode StartNode, 
                                TBitVec& Explored, TMergeNode& UnBest)
{
    vector< CRef<SSearchIterFrame> > FrameStack;
    
    CRef<SSearchIterFrame> FirstFrame(new SSearchIterFrame);
    FirstFrame->CurrNode = StartNode;
    FirstFrame->VisitCount = 0;
    FrameStack.push_back(FirstFrame);

    while(!FrameStack.empty()) {
        if((m_InterruptCounter++ % INTERRUPT_CHECK_RATE) == 0)
            x_CheckInterruptCallback();
        if(m_Interrupted)
            return TMergeNode();


        CRef<SSearchIterFrame> Frame = FrameStack.back();

        if(Frame->CurrNode.IsNull()) {
            FrameStack.pop_back();
            continue;
        }

#ifdef MERGE_TREE_VERBOSE_DEBUG
        bool DEBUG_CURR_NODE = false;
        if(Frame->CurrNode->Id == -1)
            DEBUG_CURR_NODE = true;
#endif
        
        if(Explored.get(Frame->CurrNode->Id)) {
#ifdef MERGE_TREE_VERBOSE_DEBUG
            if(DEBUG_CURR_NODE) cerr << __LINE__ << endl;
#endif
            Frame->Returned = Frame->CurrNode;
            FrameStack.pop_back();
            continue;
        }
        // cant set Explored until the bottom of this loop
    
        if(Frame->VisitCount == 0) {
            REVERSE_ITERATE(set<TMergeNode>, ChildNodeIter, Frame->CurrNode->Children) {
                CRef<SSearchIterFrame> NextFrame(new SSearchIterFrame);
                NextFrame->CurrNode = *ChildNodeIter;
                NextFrame->VisitCount = 0;
                FrameStack.push_back(NextFrame);
                Frame->ChildFrames.push_front(NextFrame);
            }
            Frame->VisitCount++;
            continue;
        }
        Frame->VisitCount++;
        
        ssize_t SelfScore = (m_Scoring.Match * Frame->CurrNode->Equiv.Matches)
                          + (m_Scoring.MisMatch * Frame->CurrNode->Equiv.MisMatches);
        Frame->CurrNode->SelfScore = SelfScore; 
        Frame->CurrNode->ChainScore = SelfScore; 
#ifdef MERGE_TREE_VERBOSE_DEBUG
        if(DEBUG_CURR_NODE)
            cerr << "Self: " << Frame->CurrNode->SelfScore << endl;
#endif
    
        Frame->BestChildModScore = numeric_limits<ssize_t>::min();
    
 
        //    Result = x_Search_Recur(*ChildNodeIter, Explored, UnBest);    
        //TMergeNode Result ;// = Returned;

        //NON_CONST_ITERATE(set<TMergeNode>, ChildNodeIter, Frame->CurrNode->Children) {
        //    TMergeNode Result = *ChildNodeIter;
        ITERATE(list< CRef<SSearchIterFrame> >, ChildFrameIter, Frame->ChildFrames) {
            TMergeNode Result = (*ChildFrameIter)->Returned;
            // their children have already been searched
            if(Result) {
#ifdef MERGE_TREE_VERBOSE_DEBUG
                if(DEBUG_CURR_NODE) {
                    cerr << "Id: " << Result->Id << "  CScore: " << Result->ChainScore << endl;
                }
#endif
                if (UnBest.IsNull() || 
                    Result->ChainScore > UnBest->ChainScore) {
#ifdef MERGE_TREE_VERBOSE_DEBUG
                    if(DEBUG_CURR_NODE)
                        cerr << "  UM: " << Result->ChainScore << endl;
#endif
                    UnBest = Result;
                }

                ssize_t CurrChildModScore = Result->ChainScore;
                           
                ssize_t GapOpen = 0, GapExtend = 0;
                x_EvalGap(Frame->CurrNode->Equiv, Result->Equiv, GapOpen, GapExtend);

#ifdef MERGE_TREE_VERBOSE_DEBUG
                if(DEBUG_CURR_NODE)
                    cerr << " GO: " << GapOpen << "\tGE: " << GapExtend << endl;
#endif
                
                CurrChildModScore += (m_Scoring.GapOpen * GapOpen);
                CurrChildModScore += (m_Scoring.GapExtend * GapExtend);


#ifdef MERGE_TREE_VERBOSE_DEBUG
                if(DEBUG_CURR_NODE) 
                    cerr << "  Mod: " << CurrChildModScore << "  Best: " << Frame->BestChildModScore << endl; 
#endif


                if (CurrChildModScore > Frame->BestChildModScore) {
                    Frame->BestChildModScore = CurrChildModScore;
                    Frame->BestChildMod = Result;
#ifdef MERGE_TREE_VERBOSE_DEBUG
                    if(DEBUG_CURR_NODE)
                        cerr << "  New Best Mod" << endl;
#endif
                }
                else if(CurrChildModScore == Frame->BestChildModScore && 
                    Frame->CurrNode->Equiv.AlignId == Result->Equiv.AlignId ) {
                    Frame->BestChildModScore = CurrChildModScore;
                    Frame->BestChildMod = Result;
                    //cerr << "  New Align-Match Mod" << endl;
                    //cerr << Frame->CurrNode->Equiv << " => " << Result->Equiv << endl;
#ifdef MERGE_TREE_VERBOSE_DEBUG
                    if(DEBUG_CURR_NODE)
                        cerr << "  New Align-Match Mod" << endl;
#endif
                }
                else if(CurrChildModScore == Frame->BestChildModScore && GapOpen > 0) {
                    Frame->BestChildModScore = CurrChildModScore;
                    Frame->BestChildMod = Result;
                    //cerr << "  New Left Shift Mod" << endl;
                    //cerr << Frame->CurrNode->Equiv << " => " << Result->Equiv << endl;
#ifdef MERGE_TREE_VERBOSE_DEBUG
                    if(DEBUG_CURR_NODE)
                        cerr << "  New Left Shift Mod" << endl;
#endif
                }
            }
        }
        // FIXME: hack fix to prevent tree cycles from creating cref-cycles in the frames
        Frame->ChildFrames.clear();

    
        Explored.set(Frame->CurrNode->Id, true);

        if (Frame->BestChildMod.IsNull()) {
            Frame->CurrNode->ChainScore = Frame->CurrNode->SelfScore;
            Frame->CurrNode->BestChild.Reset(NULL);
#ifdef MERGE_TREE_VERBOSE_DEBUG
            if(DEBUG_CURR_NODE) cerr << __LINE__ << endl;
#endif
            Frame->Returned = Frame->CurrNode;
            FrameStack.pop_back();
            continue;
        } else {
            //if(BestChildModScore > numeric_limits<ssize_t>::min()) {
            if(Frame->BestChildModScore > 0) {
                ssize_t SumScore = Frame->CurrNode->SelfScore + Frame->BestChildModScore; 
                Frame->CurrNode->ChainScore = SumScore;
                Frame->CurrNode->BestChild = Frame->BestChildMod;
#ifdef MERGE_TREE_VERBOSE_DEBUG
                if(DEBUG_CURR_NODE) cerr << __LINE__ << endl;
#endif
                Frame->Returned = Frame->CurrNode;
                FrameStack.pop_back();
                continue;
            } 
            else {
                Frame->CurrNode->ChainScore = Frame->CurrNode->SelfScore;
                Frame->CurrNode->BestChild.Reset(NULL);
#ifdef MERGE_TREE_VERBOSE_DEBUG
                if(DEBUG_CURR_NODE) cerr << __LINE__ << endl;
#endif
                Frame->Returned = Frame->CurrNode;
                FrameStack.pop_back();
                continue;
            }
        }

    } // end stack loop
    
    return FirstFrame->Returned;
}




void CMergeTree::x_EvalGap(const CEquivRange& Early, const CEquivRange& Late,  
                            ssize_t& GapOpen, ssize_t& GapExtend) const
{
    GapOpen = 0;
    GapExtend = 0;
    
    if( !Early.Query.AbuttingWith(Late.Query) ||
        !Early.Subjt.AbuttingWith(Late.Subjt) ) {
        
        ssize_t SubjtDiff = ((ssize_t(Late.Subjt.GetFrom()) - Early.Subjt.GetTo()) - 1);
        ssize_t QueryDiff = ((ssize_t(Late.Query.GetFrom()) - Early.Query.GetTo()) - 1);
        ssize_t QueryDiffM = ((ssize_t(Early.Query.GetFrom()) - Late.Query.GetTo()) - 1);
        ssize_t SubjtDiffM = ((ssize_t(Early.Subjt.GetFrom()) - Late.Subjt.GetTo()) - 1);
        
        
        QueryDiff = max(QueryDiff, QueryDiffM);
        SubjtDiff = max(SubjtDiff, SubjtDiffM);

        GapOpen = 1;
        //GapExtend = max(QueryDiff, SubjtDiff) - 1;  // nicest
        //GapOpen = 2; // meanest
        //GapExtend = QueryDiff + SubjtDiff;  // meanest
        // or goofy pythag version 
        GapExtend =  (ssize_t)sqrt( pow((double)QueryDiff, 2) + pow((double)SubjtDiff, 2) ) ;
    }
}
    


int CMergeTree::Score(const TEquivList& Path) const
{
    SScoring Accum(0,0,0,0); // re-use SScoring as an accumulator
    
    TEquivList::const_iterator Prev = Path.end();
    ITERATE(TEquivList, EquivIter, Path) {
        
        if(EquivIter->Empty()) 
            continue;

        if(Prev != Path.end()) {
           
            ssize_t GapOpen, GapExtend;
            x_EvalGap(*Prev, *EquivIter, GapOpen, GapExtend);
            Accum.GapOpen += GapOpen;
            Accum.GapExtend += GapExtend;
        }

        Accum.Match += EquivIter->Matches;
        Accum.MisMatch  += EquivIter->MisMatches;

        Prev = EquivIter;
    }


    int Score = 0;
    Score = (Accum.Match * m_Scoring.Match)
          + (Accum.MisMatch * m_Scoring.MisMatch)
          + (Accum.GapOpen * m_Scoring.GapOpen)
          + (Accum.GapExtend * m_Scoring.GapExtend);
    //cerr << __LINE__ << "\t" << Score << "\t"
    //    << Accum.Match << ":" << Accum.MisMatch << ":" 
    //    << Accum.GapOpen << ":" << Accum.GapExtend << endl;
    return Score;
}


void CMergeTree::x_CheckInterruptCallback()
{
    if(m_Callback != NULL) {
        m_Interrupted = ((*m_Callback)(m_CallbackData));
    }    
}


struct SEventualChildFrame : public CObject {
    TMergeNode Parent;
    //TMergeNode ToFind;
};

bool CMergeTree::x_IsEventualChildOf(TMergeNode Parent, TMergeNode ToFind, TBitVec& Explored) 
{
    if(Parent == ToFind)
        return true;
    if(Parent->Equiv == ToFind->Equiv)
        return true;  

    if(Explored.get(Parent->Id)) {
        return false;
    }
    Explored.set(Parent->Id, true);

    vector< CRef<SEventualChildFrame> > FrameStack;

    CRef<SEventualChildFrame> FirstFrame(new SEventualChildFrame);
    FirstFrame->Parent = Parent; 
    FrameStack.push_back(FirstFrame);

    while(!FrameStack.empty()) {
        CRef<SEventualChildFrame> Frame = FrameStack.back();
        FrameStack.pop_back();

        if(Frame->Parent == ToFind)
            return true;
        if(Frame->Parent->Equiv == ToFind->Equiv)
            return true;
        
        if(Explored.get(Frame->Parent->Id)) {
            continue;
        }
        Explored.set(Frame->Parent->Id, true);

        REVERSE_ITERATE(set<TMergeNode>, ChildIter, Frame->Parent->Children) {
            if( (*ChildIter) == ToFind) 
                return true;
            if( (*ChildIter)->Equiv == ToFind->Equiv) 
                return true;
       
            if(Explored.get((*ChildIter)->Id)) {
                continue;
            }

            //bool Found = x_IsEventualChildOf(*ChildIter, ToFind, Explored);
            //if(Found)
            //    return true;

            CRef<SEventualChildFrame> NewFrame(new SEventualChildFrame);
            NewFrame->Parent = *ChildIter;
            FrameStack.push_back(NewFrame);
        }

    }

    return false;
}


void CMergeTree::Print(CNcbiOstream& Out) 
{
    int Node = 0;
    TBitVec Explored;
    x_Print(Out, m_Root, 0, Node, Explored);
}


void CMergeTree::x_Print(CNcbiOstream& Out, TMergeNode Node, int Level, int& Count, TBitVec& Explored) 
{
    if(Explored.get(Node->Id))
        return;
    Explored.set(Node->Id);

    Out << Count << "\t" << Node->Id;
    for(int i=0;i<Level;i++) Out << (Count % 2 == 0 ? '-' : '.');
    Out << Node->Equiv << "\t" << Node->SelfScore << "\t" << Node->ChainScore; 
    if(Node->BestChild.NotNull())
        Out << "\t" << Node->BestChild->Id;
    if(Node->Children.empty())
        Out << "\t" << "LEAF";
    Out << endl;
    Count++;
    
    ITERATE(set<TMergeNode>, ChildIter, Node->Children) {
        x_Print(Out, *ChildIter, Level+1, Count, Explored);
    }

}


void CMergeTree::x_Dot(CNcbiOstream& Out, TMergeNode Node) {
    Out << "digraph All {" << endl;
    
    TBitVec Explored(128);
    x_Dot_Nodes(Out, Node, Explored);
    Explored.clear();
    x_Dot_Edges(Out, Node, Explored);

    Out << " } " << endl;
}

void CMergeTree::x_Dot_Nodes(CNcbiOstream& Out, TMergeNode Node, TBitVec& Explored) {
   
    if(Explored.get(Node->Id))
        return;
    Explored.set(Node->Id);

    Out << Node->Id << " ";
    Out << " [ " ;
    
    Out << "label=\"";
    Out << "S: " << Node->ChainScore << "\\n";
    Out << Node->Equiv.Query.GetFrom() << ":"
        << Node->Equiv.Subjt.GetFrom() << ":"
        << Node->Equiv.Matches << ":"
        << (Node->Equiv.Strand == eNa_strand_plus ? "+" : "-");
    Out << ":" << Node->Equiv.AlignId;
    Out << "\" ";

    Out << " ];" << endl;

    
    ITERATE(set<TMergeNode>, ChildIter, Node->Children) {
        x_Dot_Nodes(Out, *ChildIter, Explored);
    }
}

void CMergeTree::x_Dot_Edges(CNcbiOstream& Out, TMergeNode Node, TBitVec& Explored) {
    
    if(Explored.get(Node->Id))
        return;
    Explored.set(Node->Id);

   
    ITERATE(set<TMergeNode>, ChildIter, Node->Children) {
        Out << Node->Id << " -> " << (*ChildIter)->Id;
        Out << " [ ";
        if(Node->BestChild == *ChildIter) {
            Out << "color=blue";
        }
        Out << " ];" << endl;
    }
    
    ITERATE(set<TMergeNode>, ChildIter, Node->Children) {
        x_Dot_Edges(Out, *ChildIter, Explored);
    }
}


size_t CMergeTree::x_CountChildNodes(TMergeNode Node, TBitVec& Explored) const
{
    if(Explored.get(Node->Id))
        return 0;
    Explored.set(Node->Id);

    int Mine = 1;

    ITERATE(set<TMergeNode>, ChildIter, Node->Children) {
        Mine += x_CountChildNodes(*ChildIter, Explored);
    }

    return Mine;
}

size_t CMergeTree::x_CountChildLinks(TMergeNode Node, TBitVec& Explored) const
{
    if(Explored.get(Node->Id))
        return 0;
    Explored.set(Node->Id);

    int Mine = Node->Children.size();

    ITERATE(set<TMergeNode>, ChildIter, Node->Children) {
        Mine += x_CountChildLinks(*ChildIter, Explored);
    }

    return Mine;
}


END_NCBI_SCOPE


