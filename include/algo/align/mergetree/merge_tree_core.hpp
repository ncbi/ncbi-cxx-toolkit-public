#ifndef MERGE_TREE_CORE__HPP
#define MERGE_TREE_CORE__HPP

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

#include <corelib/ncbistr.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbi_system.hpp>

#include <serial/serialbase.hpp>
#include <serial/serial.hpp>
 
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>



#include <objects/seqloc/seqloc__.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <util/range.hpp>


#include <algo/align/mergetree/bitvec.hpp>
#include <algo/align/mergetree/equiv_range.hpp>

//
// Uncomment this to enable verbose debug output
//
//#define MERGE_TREE_VERBOSE_DEBUG 1


BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
class CSeq_id;
class CSeq_align;
END_SCOPE(objects)





class CMergeNode : public CObject 
{
public:
    CMergeNode(int NId) { Id = NId; SelfScore = ChainScore = numeric_limits<ssize_t>::min(); QI = SI = -1; }
    CMergeNode(CEquivRange Value, int NId) : Equiv(Value) { Id = NId; SelfScore = ChainScore = numeric_limits<ssize_t>::min(); QI= SI= -1; }
    
    CEquivRange Equiv;
    int Id;

    set< CRef<CMergeNode> > Parents;
    set< CRef<CMergeNode> > Children; 
   
    ssize_t SelfScore;
    ssize_t ChainScore;
    CRef<CMergeNode> BestChild;

    int QI, SI;
};
bool operator<(const CMergeNode& A, const CMergeNode& B);

typedef CRef<CMergeNode> TMergeNode;
typedef vector<TMergeNode> TMergeNodeVec;

bool operator<(const TMergeNode& A, const TMergeNode& B);

// Only Valid for Split Equivs
//   100 equivs is instant
//   1000 takes a second
//   10000 takes a 30 minutes
//   100000 probably takes eternity
class CMergeTree 
{
public:
    CMergeTree() : m_Callback(NULL), m_CallbackData(NULL), m_Interrupted(false), m_InterruptCounter(0) { 
        m_NodeIdCounter = 0;
        m_Root.Reset(new CMergeNode(m_NodeIdCounter));
        m_NodeIdCounter++;
    }
    ~CMergeTree();

    struct SScoring {
        int Match;
        int MisMatch;
        int GapOpen;
        int GapExtend;
        SScoring() : Match(3), MisMatch(-1), GapOpen(-1), GapExtend(-1) { ; }
        SScoring(int M, int X, int O, int E) : Match(M), MisMatch(X), GapOpen(O), GapExtend(E) { ; }
    };

    void SetScoring(SScoring Scoring) { m_Scoring = Scoring; }
    
    void AddEquiv( CEquivRange NewEquiv ) ;
    void AddEquivs(const TEquivList& Equivs);
    
    void Search(TEquivList& Path, bool Once=false);
   
    int Score(const TEquivList& Path) const;

    bool IsEmpty() const;
    size_t Size() const;
    size_t Links() const;

    void Print(CNcbiOstream& Out);
    
    
    // mod from algo/blast/core/blast_def.h , 
    // but didnt want to require the blast include
    typedef bool (*TInterruptFnPtr) (void* callback_data);
    TInterruptFnPtr SetInterruptCallback(TInterruptFnPtr Callback, void* CallbackData) 
        { m_Callback = Callback; m_CallbackData = CallbackData; return m_Callback; }
    bool GetInterrupted() { return m_Interrupted; }
private: 

    TMergeNode m_Root;
    set<TMergeNode> m_Leaves;

    void x_AddChild(TMergeNode Parent, TMergeNode Child);
    void x_RemoveChild(TMergeNode Parent, TMergeNode Child);
    void x_AddParent(TMergeNode Child, TMergeNode Parent);
    void x_RemoveParent(TMergeNode Child, TMergeNode Parent);

    void x_LinkNodes(TMergeNode Parent, TMergeNode Child);
    void x_UnLinkNodes(TMergeNode Parent, TMergeNode Child);


    int m_NodeIdCounter;

    SScoring m_Scoring;

    TInterruptFnPtr m_Callback;
    void* m_CallbackData;
    bool m_Interrupted;
    unsigned int m_InterruptCounter;
    void x_CheckInterruptCallback();
    const static unsigned int INTERRUPT_CHECK_RATE = 100;

    typedef map<int, TMergeNode> TNodeCache;
    TNodeCache m_NodeCache;
    TMergeNode x_GetNode(CEquivRange Equiv);

    typedef bitvec<unsigned int> TBitVec;
    
    void x_FindLeafs(TMergeNode Curr, set<TMergeNode>& Leafs, TBitVec& Explored);
    
    // Trickles down
    bool x_FindBefores(TMergeNode New, TMergeNode Curr, 
                      set<TMergeNode>& Befores, TBitVec& Explored, TBitVec& Inserted, int& Depth);
    bool x_FindAfters(TMergeNode New, TMergeNode Curr, 
                      set<TMergeNode>& Afters, TBitVec& Explored, TBitVec& Inserted);
    // Trickles up, start from leafs
    bool x_FindBefores_Up_Recur(TMergeNode New, TMergeNode Curr, 
                      set<TMergeNode>& Befores, TBitVec& Explored, TBitVec& Inserted, int& Depth);
    bool x_FindBefores_Up_Iter(TMergeNode New, TMergeNode StartCurr, 
                      set<TMergeNode>& Befores, TBitVec& Explored, TBitVec& Inserted, int& Depth);
    bool x_FindAfters_Up(TMergeNode New, TMergeNode Curr, 
                      set<TMergeNode>& Afters, TBitVec& Explored, TBitVec& Inserted);
    


    // returns best answer starting point
    TMergeNode x_Search_Recur(TMergeNode CurrNode,
                         TBitVec& Explored,
                         TMergeNode& UnBest);
    TMergeNode x_Search_Iter(TMergeNode StartNode,
                             TBitVec& Explored,
                             TMergeNode& UnBest);
    
    void x_EvalGap(const CEquivRange& Early, const CEquivRange& Late,  
                   ssize_t& GapOpen, ssize_t& GapExtend) const;
    

    bool x_IsEventualChildOf(TMergeNode Parent, TMergeNode ToFind, TBitVec& Explored);

    void x_Print(CNcbiOstream& Out, TMergeNode Node, int Level, int& Count, TBitVec& Explored); 
   
    void x_Dot(CNcbiOstream& Out, TMergeNode Node);
    void x_Dot_Nodes(CNcbiOstream& Out, TMergeNode Node, TBitVec& Explored);
    void x_Dot_Edges(CNcbiOstream& Out, TMergeNode Node, TBitVec& Explored);

    size_t x_CountChildNodes(TMergeNode Node, TBitVec& Explored) const;
    size_t x_CountChildLinks(TMergeNode Node, TBitVec& Explored) const;
};


END_NCBI_SCOPE


#endif // end MERGE_TREE_CORE__HPP
