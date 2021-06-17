#ifndef MERGE_TREE__HPP
#define MERGE_TREE__HPP

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

#include <objects/seq/seq_id_handle.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>



#include <objects/seqloc/seqloc__.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/bioseq_handle.hpp>

#include <util/range.hpp>


#include <algo/align/mergetree/bitvec.hpp>
#include <algo/align/mergetree/equiv_range.hpp>
#include <algo/align/mergetree/merge_tree_core.hpp>


BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
class CSeq_id;
class CSeq_align;
END_SCOPE(objects)


class CTreeAlignMerger 
{
public:
    
    CTreeAlignMerger() : m_Scope(NULL), Callback(NULL), CallbackData(NULL) { ; }
    
    void SetScope(objects::CScope* Scope) { m_Scope = Scope; }
    void SetScoring(CMergeTree::SScoring Scoring) { m_Scoring = Scoring; }
   
    CMergeTree::TInterruptFnPtr 
    SetInterruptCallback(CMergeTree::TInterruptFnPtr callback, void* callback_data) 
        { this->Callback = callback; this->CallbackData = callback_data; return callback; }
  
 
    // Calls Merge_Dist
    void Merge(const list< CRef<objects::CSeq_align> >& Input,
               list< CRef<objects::CSeq_align> >& Output ) ;
   
    void Merge_AllAtOnce(const list< CRef<objects::CSeq_align> >& Input,
               list< CRef<objects::CSeq_align> >& Output ) ;
    void Merge_Pairwise(const list< CRef<objects::CSeq_align> >& Input,
               list< CRef<objects::CSeq_align> >& Output ) ;
    
    void Merge_Dist(const list< CRef<objects::CSeq_align> >& Input,
               list< CRef<objects::CSeq_align> >& Output ) ;


private:
    
    objects::CScope* m_Scope;
    CMergeTree::SScoring m_Scoring;

    CMergeTree::TInterruptFnPtr Callback;
    void* CallbackData;

    CRef<objects::CSeq_align> 
    x_MakeSeqAlign(TEquivList& Equivs, 
	    			objects::CSeq_id_Handle QueryIDH, 
					objects::CSeq_id_Handle SubjtIDH);
    
    
    typedef pair<objects::CSeq_id_Handle, objects::ENa_strand> TSeqIdPair;
    typedef pair<TSeqIdPair, TSeqIdPair> TMapKey;
    typedef vector<CRef<objects::CSeq_align> > TAlignVec;
    typedef map<TMapKey, TAlignVec> TAlignGroupMap;
    
    void
    x_MakeMergeableGroups(list<CRef<objects::CSeq_align> > Input, 
                          TAlignGroupMap& AlignGroupMap);

    void x_SplitGlobalUnique(const TAlignVec& Input, TAlignVec& Unique, TAlignVec& Other);

    void 
    x_Merge_Dist_Impl(TAlignVec& Aligns,
                      objects::CSeq_id_Handle QueryIDH, objects::CSeq_id_Handle SubjtIDH,
                      objects::CBioseq_Handle QueryBSH, objects::CBioseq_Handle SubjtBSH,
                      list< CRef<objects::CSeq_align> >& Output);

};


END_NCBI_SCOPE


#endif // end MERGE_TREE__HPP
