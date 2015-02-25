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
* File Name:  CTreeCont.cpp
*
* Author:  Vladimir Soussov, Yuri Sadykov, Michael Domrachev
*
* File Description:  General purpose tree container implementation
*
*/

#include <ncbi_pch.hpp>
#include "ctreecont.hpp"


BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE


bool CTreeIterator::BelongSubtree(const CTreeContNodeBase* subtree_root)
{
    if(m_node == subtree_root)
        return true;
    for(CTreeContNodeBase* pN= m_node->m_parent; pN != 0; pN= pN->m_parent) {
        if(pN == subtree_root)
            return true;
    }
    return false;
}

bool CTreeIterator::AboveNode(CTreeContNodeBase* node)
{
    if(node == 0) return false;
    do {
        if(node->m_parent == m_node) return true;
    }
    while((node= node->m_parent) != 0);

    return false;
}

// move cursor to the nearest common ancestor
bool CTreeIterator::GoAncestor(CTreeContNodeBase* node)
{
    if(BelongSubtree(node)) {
        m_node= node;
        return true;
    }
    CTreeContNodeBase* pN= m_node;;
    while(!AboveNode(node)) {
        if(m_node->m_parent == 0) {
            m_node= pN;
            return false;
        }
        m_node= m_node->m_parent;
    }
    return true;
}


// add child to a node pointed by cursor
bool CTreeIterator::AddChild(CTreeContNodeBase* new_node)
{
    if(new_node) {
        m_tree->AddChild(m_node);
        new_node->m_parent= m_node;
        new_node->m_sibling= m_node->m_child;
        new_node->m_child= 0;
        m_node->m_child= new_node;
        m_tree->Done(new_node);
        return true;
    }
    return false;
}

// add sibling to a node pointed by cursor
bool CTreeIterator::AddSibling(CTreeContNodeBase* new_node)
{
    if(new_node && m_node->m_parent) {
        m_tree->AddChild(m_node->m_parent);
        new_node->m_parent= m_node->m_parent;
        new_node->m_sibling= m_node->m_sibling;
        new_node->m_child= 0;
        m_node->m_sibling= new_node;
        m_tree->Done(new_node);
        return true;
    }
    return false;
}

bool CTreeIterator::MoveNode(CTreeContNodeBase* to_node)
{
    if((to_node == 0) || AboveNode(to_node)) {
        return false;
    }

    if(m_node->m_parent == to_node) return true;

    // notify the spies
    m_tree->MoveNode(m_node, to_node);


    // detach the node
    if(m_node->m_parent->m_child == m_node) { // this is a first child
        m_node->m_parent->m_child= m_node->m_sibling;
    }
    else {
        CTreeContNodeBase* pN;
        for( pN= m_node->m_parent->m_child;
             pN->m_sibling != m_node;
             pN= pN->m_sibling );
        pN->m_sibling= m_node->m_sibling;
    }

    // attach it
    m_node->m_sibling= to_node->m_child;
    m_node->m_parent= to_node;
    to_node->m_child= m_node;

    // notify the spies
    m_tree->Done(m_node);

    return true;
}

bool CTreeIterator::MoveChildren(CTreeContNodeBase* to_node)
{
    if((to_node == 0) || AboveNode(to_node)) {
        return false;
    }

    if((m_node == to_node) || (m_node->m_child == 0)) return true;

    // notify the spies
    m_tree->MoveChildren(m_node, to_node);

    CTreeContNodeBase* pN= m_node->m_child;

    do {
        pN->m_parent= to_node;
        if(pN->m_sibling == 0) break;
    }
    while((pN= pN->m_sibling) != 0);

    pN->m_sibling= to_node->m_child;
    to_node->m_child= m_node->m_child;
    m_node->m_child= 0;

    // notify the spies
    m_tree->Done(m_node);

    return true;
}

void CTreeCont::DelNodeInternal(CTreeContNodeBase* pN)
{
    CTreeContNodeBase* pNxt;
    for(CTreeContNodeBase* pChild= pN->m_child; pChild != 0; pChild= pNxt) {
        pNxt= pChild->m_sibling;
        DelNodeInternal(pChild);
    }
    delete pN;
}

bool CTreeIterator::DeleteSubtree()
{
    if(m_node->m_parent == 0) return false; // can't delete the whole tree

    // notify the spies
    m_tree->DeleteSubtree(m_node, this);

    CTreeContNodeBase* pN;

    // detach the node
    if(m_node->m_parent->m_child == m_node) { // this is a first child
        m_node->m_parent->m_child= m_node->m_sibling;
    }
    else {
        for(pN= m_node->m_parent->m_child;
            pN->m_sibling != m_node;
            pN= pN->m_sibling);
        pN->m_sibling= m_node->m_sibling;
    }

    pN= m_node->m_parent;

    m_tree->DelNodeInternal(m_node);

    // notify the spies
    m_node= pN;
    m_tree->Done(m_node);

    return true;
}


bool CTreeIterator::DeleteNode()
{
    if(m_node->m_parent == 0) return false; // can't delete the root
    if(m_node->m_child) {
        MoveChildren(m_node->m_parent);
    }

    return DeleteSubtree();
}

bool CTreeIterator::Merge(CTreeContNodeBase* to_node)
{
    if(MoveChildren(to_node)) {
        m_tree->Merge(m_node, to_node, this);
        to_node->Merge(m_node);
	
        // detach the node
        if(m_node->m_parent->m_child == m_node) { // this is a first child
            m_node->m_parent->m_child= m_node->m_sibling;
        }
        else {
            CTreeContNodeBase* pN;
            for(pN= m_node->m_parent->m_child;
                pN->m_sibling != m_node;
                pN= pN->m_sibling);
            pN->m_sibling= m_node->m_sibling;
        }
        delete m_node;
        m_node= to_node;
        m_tree->Done(m_node);
        return true;
    }
    return false;	
}

CTreeIterator::EAction
CTreeIterator::ForEachDownward(C4Each& cb)
{
    switch( cb.Execute(m_node) ) {
    default:
    case eCont:
        if(!m_node->IsTerminal()) {
            switch( cb.LevelBegin(m_node) ) {
            case eStop: return eStop;
            default:
            case eCont:
                if(GoChild()) {
                    do {
                        if(ForEachDownward(cb)==eStop)
                            return eStop;
                    } while(GoSibling());
                }
            case eSkip: // Means skip this level
                break;
            }
            GoParent();
            if( cb.LevelEnd(m_node) == eStop )
                return eStop;
        }
    case eSkip:	break;
    case eStop: return eStop;
    }
    return eCont;
}

CTreeIterator::EAction
CTreeIterator::ForEachDownward(ForEachFunc ucb, void* user_data)
{
    switch( (*ucb)(m_node, user_data) ) {
    default:
    case eCont:
        if(GoChild()) {
            do {
                if(ForEachDownward(ucb, user_data)==eStop)
                    return eStop;
            } while(GoSibling());
            GoParent();
        }
    case eSkip:	break;
    case eStop: return eStop;
    }
    return eCont;
}

CTreeIterator::EAction
CTreeIterator::ForEachDownwardLimited(C4Each& cb, int levels)
{
    if(levels > 0) {
        switch( cb.Execute(m_node) ) {
        default:
        case eCont:
            if(!m_node->IsTerminal()) {
                switch( cb.LevelBegin(m_node) ) {
                case eStop: return eStop;
                default:
                case eCont:
                    if(GoChild()) {
                        do {
                            if(ForEachDownwardLimited(cb, levels-1)==eStop)
                                return eStop;
                        } while(GoSibling());
                    }
                case eSkip: // Means skip this level
                    break;
                }
                GoParent();
                if( cb.LevelEnd(m_node) == eStop )
                    return eStop;
            }
        case eSkip: break;
        case eStop: return eStop;
        }
    }
    return eCont;
}

CTreeIterator::EAction
CTreeIterator::ForEachDownwardLimited(ForEachFunc ucb, void* user_data,
                                      int levels)
{
    if(levels > 0) {
        switch( (*ucb)(m_node, user_data) ) {
        default:
        case eCont:
            if(GoChild()) {
                do {
                    if(ForEachDownwardLimited(ucb, user_data,
                                              levels-1)==eStop)
                        return eStop;
                } while(GoSibling());
                GoParent();
            }
        case eSkip: break;
        case eStop: return eStop;
        }
    }
    return eCont;
}

///////////////////////////////////////////////////////
// Get iterators
///////////////////////////////////////////////////////
CTreeIterator*
CTreeCont::GetIterator()
{
    return new CTreeIterator(this);
}

CTreeConstIterator*
CTreeCont::GetConstIterator() const
{
    return new CTreeConstIterator(this);
}
///////////////////////////////////////////////////////
// delete subtree notification (private method)
///////////////////////////////////////////////////////
void
CTreeCont::DeleteSubtree(CTreeContNodeBase*, CTreeIterator*)
{
    //     int i;

    //     if((n= m_cursorPot.nof()) > 1) {
    // 	// move all cursors out of deleted subtree
    // 	CTreeIterator* iCursor;

    // 	for(i= 0; i < n; i++) {
    // 	    iCursor= (CTreeIterator*)(m_cursorPot.get(i));
    // 	    if((iCursor != pCursor) && iCursor->belongSubtree(stroot)) {
    // 		iCursor->GoNode(stroot->m_parent); // move it up
    // 	    }
    // 	}
    //     }
}

////////////////////////////////////////////////////////////
// notify others that operation completed (private method)
////////////////////////////////////////////////////////////
void CTreeCont::Done(CTreeContNodeBase* /*node*/)
{
    //     int n= m_spyPot.nof();

    //     if(n > 0) { // notify all spies about delSubtree operation
    // 	int i;
    // 	CTreeContSpy* pSpy;

    // 	for(i= 0; i < n; i++) {
    // 	    pSpy= (CTreeContSpy*)(m_spyPot.get(i));
    // 	    pSpy->Done(node);
    // 	}
    //     }
}

///////////////////////////////////////////////////////////
// move node notification (private method)
///////////////////////////////////////////////////////////
void CTreeCont::MoveNode(CTreeContNodeBase*, CTreeContNodeBase*)
{
    //     int n= m_spyPot.nof();

    //     if(n > 0) { // notify all spies about delSubtree operation
    // 	int i;
    // 	CTreeContSpy* pSpy;

    // 	for(i= 0; i < n; i++) {
    // 	    pSpy= (CTreeContSpy*)(m_spyPot.get(i));
    // 	    pSpy->node_move(node2move, new_parent);
    // 	}
    //     }
}

//////////////////////////////////////////////////////////////
// move children (private method)
//////////////////////////////////////////////////////////////
void CTreeCont::MoveChildren(CTreeContNodeBase*, CTreeContNodeBase*)
{
    //     int n= m_spyPot.nof();

    //     if(n > 0) { // notify all spies about delSubtree operation
    // 	int i;
    // 	CTreeContSpy* pSpy;

    // 	for(i= 0; i < n; i++) {
    // 	    pSpy= (CTreeContSpy*)(m_spyPot.get(i));
    // 	    pSpy->children_move(old_parent, new_parent);
    // 	}
    //     }
}

///////////////////////////////////////////////////////////////
// add child notification (private method)
///////////////////////////////////////////////////////////////
void CTreeCont::AddChild(CTreeContNodeBase*)
{
    //     int n= m_spyPot.nof();

    //     if(n > 0) { // notify all spies about delSubtree operation
    // 	int i;
    // 	CTreeContSpy* pSpy;

    // 	for(i= 0; i < n; i++) {
    // 	    pSpy= (CTreeContSpy*)(m_spyPot.get(i));
    // 	    pSpy->child_add(parent);
    // 	}
    //     }
}

//////////////////////////////////////////////////////
// merge nodes notification (private method)
//////////////////////////////////////////////////////
void CTreeCont::Merge(CTreeContNodeBase* , CTreeContNodeBase* ,
                      CTreeIterator* )
{
    //     int i;
    //     int n= m_spyPot.nof();

    //     if(n > 0) { // notify all spies about delSubtree operation
    // 	CTreeContSpy* pSpy;

    // 	for(i= 0; i < n; i++) {
    // 	    pSpy= (CTreeContSpy*)(m_spyPot.get(i));
    // 	    pSpy->node_merge(src, dst);
    // 	}
    //     }

    //     if((n= m_cursorPot.nof()) > 1) {
    // 	// move all cursors out of src
    // 	CTreeIterator* iCursor;

    // 	for(i= 0; i < n; i++) {
    // 	    iCursor= (CTreeIterator*)(m_cursorPot.get(i));
    // 	    if((iCursor != pCursor) && (iCursor->getNode() == src)) {
    // 		iCursor->GoNode(dst); // move it to merged node
    // 	    }
    // 	}
    //     }
}

CTreeCont::~CTreeCont()
{
    //     int i, n;

    //     // delete all cursors
    //     if((n= m_cursorPot.nof()) > 1) {
    // 	// move all cursors out of deleted subtree
    // 	CTreeIterator* iCursor;

    // 	for(i= 0; i < n; i++) {
    // 	    iCursor= (CTreeIterator*)(m_cursorPot.get(i));
    // 	    delete iCursor;
    // 	}
    //     }
    if(m_root)
        DelNodeInternal(m_root);
}


bool CTreeCont::AddNode(CTreeContNodeBase* pParentNode,
                        CTreeContNodeBase* pNewNode)
{
    if(pNewNode && pParentNode) {
        pNewNode->m_parent = pParentNode;
        pNewNode->m_sibling = pParentNode->m_child;
        pNewNode->m_child = 0;
        pParentNode->m_child = pNewNode;

        return true;
    }

    return false;
}


bool CTreeConstIterator::BelongSubtree(const CTreeContNodeBase* subtree_root)
    const
{
    if(m_node == subtree_root)
        return true;
    for(const CTreeContNodeBase* pN= m_node->m_parent; pN != 0;
        pN= pN->m_parent) {
        if(pN == subtree_root)
            return true;
    }
    return false;
}

bool CTreeConstIterator::AboveNode(const CTreeContNodeBase* node) const
{
    if(node == 0)
        return false;
    do {
        if(node->m_parent == m_node)
            return true;
    }
    while((node= node->m_parent) != 0);

    return false;
}

// move cursor to the nearest common ancestor
bool CTreeConstIterator::GoAncestor(const CTreeContNodeBase* node)
{
    if(BelongSubtree(node)) {
        m_node= node;
        return true;
    }
    const CTreeContNodeBase* pN= m_node;;
    while(!AboveNode(node)) {
        if(m_node->m_parent == 0) {
            m_node= pN;
            return false;
        }
        m_node= m_node->m_parent;
    }
    return true;
}

// CTreeConstIterator::EAction
// CTreeConstIterator::ForEachDownward(C4Each& cb)
// {
//     switch( cb.Execute(m_node) ) {
//     default:
//     case eCont:
//         if(!m_node->IsTerminal()) {
//             switch( cb.LevelBegin(m_node) ) {
//             case eStop: return eStop;
//             default:
//             case eCont:
//                 if(GoChild()) {
//                     do {
//                         if(ForEachDownward(cb)==eStop) return eStop;
//                     } while(GoSibling());
//                 }
//             case eSkip: // Means skip this level
//                 break;
//             }
//             GoParent();
//             if( cb.LevelEnd(m_node) == eStop )
//                 return eStop;
//         }
//     case eSkip:	break;
//     case eStop: return eStop;
//     }
//     return eCont;
// }

// CTreeConstIterator::EAction
// CTreeConstIterator::ForEachDownward(ForEachFunc ucb, void* user_data)
// {
//     switch( (*ucb)(m_node, user_data) ) {
//     default:
//     case eCont:
//         if(GoChild()) {
//             do {
//                 if(ForEachDownward(ucb, user_data)==eStop) return eStop;
//             } while(GoSibling());
//             GoParent();
//         }
//     case eSkip:	break;
//     case eStop: return eStop;
//     }
//     return eCont;
// }

// CTreeConstIterator::EAction
// CTreeConstIterator::ForEachDownwardLimited(C4Each& cb, int levels)
// {
//     if(levels > 0) {
//         switch( cb.Execute(m_node) ) {
//         default:
//         case eCont:
//             if(!m_node->IsTerminal()) {
//                 switch( cb.LevelBegin(m_node) ) {
//                 case eStop: return eStop;
//                 default:
//                 case eCont:
//                     if(GoChild()) {
//                         do {
//                             if(ForEachDownwardLimited(cb, levels-1)==eStop)
//                                 return eStop;
//                         } while(GoSibling());
//                     }
//                 case eSkip: // Means skip this level
//                     break;
//                 }
//                 GoParent();
//                 if( cb.LevelEnd(m_node) == eStop )
//                     return eStop;
//             }
//         case eSkip: break;
//         case eStop: return eStop;
//         }
//     }
//     return eCont;
// }

// CTreeConstIterator::EAction
// CTreeConstIterator::ForEachDownwardLimited( ForEachFunc ucb,
//                                             void* user_data, int levels)
// {
//     if(levels > 0) {
//         switch( (*ucb)(m_node, user_data) ) {
//         default:
//         case eCont:
//             if(GoChild()) {
//                 do {
//                     if(ForEachDownwardLimited(ucb, user_data,
//                                               levels-1)==eStop)
//                         return eStop;
//                 } while(GoSibling());
//                 GoParent();
//             }
//         case eSkip: break;
//         case eStop: return eStop;
//         }
//     }
//     return eCont;
// }

// add child preserving the sorting order
bool CTreeIterator::AddChild(CTreeContNodeBase* new_node, 
                             CTreeIterator::CSortPredicate& pred )
{
    // Temporary 
    CTreeContNodeBase* prev;
    CTreeContNodeBase* next;
    
    if( GoChild() ) {
        new_node->m_child = 0;
        new_node->m_parent = m_node->Parent();
        prev = 0;
        next = GetNode();
        while( next && pred.Execute( next, new_node ) ) {
            prev = next;
            next = prev->Sibling();
        }
        new_node->m_sibling = next;
        if( prev ) { // insert after prevtmp
            prev->m_sibling = new_node;
        } else { // insert as first child
            new_node->Parent()->m_child = new_node;
        }
        // Restore state
        GoParent();
    } else {
        return AddChild( new_node );
    }
    return true;
}

void CTreeIterator::SortChildren( CTreeIterator::CSortPredicate& pred )
{
    // Sorting the list by insertion
    CTreeContNodeBase* prev;
    CTreeContNodeBase* next;
    CTreeContNodeBase* tmp;
    CTreeContNodeBase* prevtmp;
    
    if( GoChild() ) {
        prev = GetNode();
        if( GoSibling() ) {
            next = GetNode();
            while( next ) {
                if( !pred.Execute( prev, next ) ) { // The order is not right
                    tmp = prev->Parent()->Child();
                    prevtmp = 0;
                    while( tmp != prev && pred.Execute( tmp, next ) &&
                           (prevtmp = tmp) && (tmp = tmp->Sibling()) );
                    if( tmp ) {
                        // Move from prev place
                        prev->m_sibling = next->m_sibling;

                        if( prevtmp ) { // insert after prevtmp
                            next->m_sibling = prevtmp->m_sibling;
                            prevtmp->m_sibling = next;
                        } else { // insert as first child
                            next->m_sibling = prev->Parent()->Child();
                            prev->Parent()->m_child = next;
                        }
                    }
                } else { // the oreder is right, move to the next
                    prev = next;
                }
                next = prev->Sibling();
            }
        }
        // Restore state
        GoParent();
    }
}

class CLevelSort : public CTreeIterator::C4Each {
public:
    CLevelSort( CTreeIterator::CSortPredicate& pred, CTreeCont* tree )
        : m_pred(pred), m_tree(tree) {}
    virtual CTreeIterator::EAction Execute(CTreeContNodeBase* pNode) {
        CTreeIterator::EAction retc = CTreeIterator::eCont;
        CTreeIterator* it = m_tree->GetIterator();
        if( it->GoNode( pNode ) ) {
            it->SortChildren( m_pred );
        } else {
            retc = CTreeIterator::eSkip;
        }
        delete it;
        return retc;
    }
private:
    CTreeIterator::CSortPredicate& m_pred;
    CTreeCont*                     m_tree;
};

void
CTreeIterator::SortAllChildrenInSubtree( CTreeIterator::CSortPredicate& pred )
{
    CLevelSort sorter( pred, m_tree );
    ForEachDownward( sorter );
}

CTreeIterator::EAction
CTreeIterator::ForEachUpward(C4Each& cb)
{
    if(!m_node->IsTerminal()) {
        switch( cb.LevelBegin(m_node) ) {
        case eStop: return eStop;
        default:
        case eCont:
            if(GoChild()) {
                do {
                    if(ForEachUpward(cb)==eStop)
                        return eStop;
                } while(GoSibling());
            }
        case eSkip: // Means skip this level
            break;
        }
        GoParent();
        if( cb.LevelEnd(m_node) == eStop )
            return eStop;
    }
    return cb.Execute(m_node);
}

CTreeIterator::EAction
CTreeIterator::ForEachUpward(ForEachFunc ucb, void* user_data)
{
    if(GoChild()) {
        do {
            if(ForEachUpward(ucb, user_data)==eStop)
                return eStop;
        } while(GoSibling());
        GoParent();
    }
    return (*ucb)(m_node, user_data);
}

CTreeIterator::EAction
CTreeIterator::ForEachUpwardLimited(C4Each& cb, int levels)
{
    if(levels > 0) {
        if(!m_node->IsTerminal()) {
            switch( cb.LevelBegin(m_node) ) {
            case eStop: return eStop;
            default:
            case eCont:
                if(GoChild()) {
                    do {
                        if(ForEachUpwardLimited(cb, levels-1)==eStop)
                            return eStop;
                    } while(GoSibling());
                }
            case eSkip: // Means skip this level
                break;
            }
            GoParent();
            if( cb.LevelEnd(m_node) == eStop )
                return eStop;
        }
        return cb.Execute(m_node);
    }
    return eCont;
}

CTreeIterator::EAction
CTreeIterator::ForEachUpwardLimited(ForEachFunc ucb, void* user_data,
                                    int levels)
{
    if(levels > 0) {
        if(GoChild()) {
            do {
                if(ForEachUpwardLimited(ucb, user_data,
                                        levels-1)==eStop)
                    return eStop;
            } while(GoSibling());
            GoParent();
        }
        return (*ucb)(m_node, user_data);
    }
    return eCont;
}

// CTreeConstIterator::EAction
// CTreeConstIterator::ForEachUpward(C4Each& cb)
// {
//     if(!m_node->IsTerminal()) {
//         switch( cb.LevelBegin(m_node) ) {
//         case eStop: return eStop;
//         default:
//         case eCont:
//             if(GoChild()) {
//                 do {
//                     if(ForEachUpward(cb)==eStop)
//                         return eStop;
//                 } while(GoSibling());
//             }
//         case eSkip: // Means skip this level
//             break;
//         }
//         GoParent();
//         if( cb.LevelEnd(m_node) == eStop )
//             return eStop;
//     }
//     return cb.Execute(m_node);
// }

// CTreeConstIterator::EAction
// CTreeConstIterator::ForEachUpward(ForEachFunc ucb, void* user_data)
// {
//     if(GoChild()) {
//         do {
//             if(ForEachUpward(ucb, user_data)==eStop)
//                 return eStop;
//         } while(GoSibling());
//         GoParent();
//     }
//     return (*ucb)(m_node, user_data);
// }

// CTreeConstIterator::EAction
// CTreeConstIterator::ForEachUpwardLimited(C4Each& cb, int levels)
// {
//     if(levels > 0) {
//         if(!m_node->IsTerminal()) {
//             switch( cb.LevelBegin(m_node) ) {
//             case eStop: return eStop;
//             default:
//             case eCont:
//                 if(GoChild()) {
//                     do {
//                         if(ForEachUpwardLimited(cb, levels-1)==eStop)
//                             return eStop;
//                     } while(GoSibling());
//                 }
//             case eSkip: // Means skip this level
//                 break;
//             }
//             GoParent();
//             if( cb.LevelEnd(m_node) == eStop )
//                 return eStop;
//         }
//         return cb.Execute(m_node);
//     }
//     return eCont;
// }

// CTreeConstIterator::EAction
// CTreeConstIterator::ForEachUpwardLimited(ForEachFunc ucb, void* user_data,
//                                          int levels)
// {
//     if(levels > 0) {
//         if(GoChild()) {
//             do {
//                 if(ForEachUpwardLimited(ucb, user_data,
//                                         levels-1)==eStop)
//                     return eStop;
//             } while(GoSibling());
//             GoParent();
//         }
//         return (*ucb)(m_node, user_data);
//     }
//     return eCont;
// }


END_objects_SCOPE
END_NCBI_SCOPE
