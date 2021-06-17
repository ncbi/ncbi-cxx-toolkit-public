#ifndef NCBI_OBJECTS_CTREECONT_HPP
#define NCBI_OBJECTS_CTREECONT_HPP

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
* File Name:  CTreeCont.hpp
*
* Author:  Vladimir Soussov, Yuri Sadykov, Michael Domrachev
*
* File Description: General purpose tree container
*
*/
#include <corelib/ncbistl.hpp>
#include <objects/taxon1/taxon1.hpp>

BEGIN_NCBI_SCOPE

#ifndef BEGIN_objects_SCOPE
#  define BEGIN_objects_SCOPE BEGIN_SCOPE(objects)
#  define END_objects_SCOPE END_SCOPE(objects)
#endif
BEGIN_objects_SCOPE // namespace ncbi::objects::

class CTreeIterator;
class CTreeConstIterator;
class CTreeCont;

class CTreeContNodeBase  {
    friend class CTreeIterator;
    friend class CTreeConstIterator;
    friend class CTreeCont;
public:
    //////////////////////////////////////////////////////////////
    // the following method are used by tree manipulation methods
    // from CTreeCursor and CTree classes
    //////////////////////////////////////////////////////////////
    CTreeContNodeBase() {
	m_parent= m_sibling= m_child= 0;
    }

    bool IsTerminal() const {
	return (m_child == 0);
    }

    bool IsRoot() const {
	return (m_parent == 0);
    }

    bool IsLastChild() const {
	return (m_sibling == 0);
    }

    bool IsFirstChild() const {
	return ((m_parent == 0) || (m_parent->m_child == this));
    }

    void Merge( CTreeContNodeBase* ) {}

    const CTreeContNodeBase* Parent() const  { return m_parent;  }
    const CTreeContNodeBase* Sibling() const { return m_sibling; }
    const CTreeContNodeBase* Child() const   { return m_child;   }
protected:
    CTreeContNodeBase*       Parent()        { return m_parent;  }
    CTreeContNodeBase*       Sibling()       { return m_sibling; }
    CTreeContNodeBase*       Child()         { return m_child;   }
    virtual ~CTreeContNodeBase(){};
private:
    CTreeContNodeBase* m_parent;
    CTreeContNodeBase* m_sibling;
    CTreeContNodeBase* m_child;
};

class CTreeCont {
friend class CTreeIterator;
friend class CTreeConstIterator;
public:

    CTreeCont() {
	m_root= 0;
    }

    CTreeIterator*      GetIterator();
    CTreeConstIterator* GetConstIterator() const;

    bool SetRoot(CTreeContNodeBase* root) {
	if((!m_root) && root) {
	    m_root= root;
	    m_root->m_parent= m_root->m_sibling= m_root->m_child= 0;
	}
	return (m_root == root);
    }

    const CTreeContNodeBase* GetRoot() const {
	return m_root;
    }

    bool AddNode(CTreeContNodeBase* pParentNode, CTreeContNodeBase* pNewNode);
    void Clear()    { if (m_root) { DelNodeInternal(m_root); m_root = 0; } }

    ~CTreeCont();

private:
    CTreeContNodeBase* m_root;
//    CPntrPot m_cursorPot;
//    CPntrPot m_spyPot;
    void DeleteSubtree(CTreeContNodeBase* stroot, CTreeIterator* pCur);
    void Done(CTreeContNodeBase* node);
    void MoveNode(CTreeContNodeBase* node2move, CTreeContNodeBase* new_parent);
    void MoveChildren(CTreeContNodeBase* old_parent, CTreeContNodeBase* new_parent);
    void Merge(CTreeContNodeBase* src, CTreeContNodeBase* dst, CTreeIterator* pCur);
    void AddChild(CTreeContNodeBase* parent);
    void DelNodeInternal(CTreeContNodeBase* pN);
};


class CTreeIterator {
public:

    // navigation
    void GoRoot() {// move cursor to the root node
	m_node= m_tree->m_root;
    }
    bool GoParent() {// move cursor to the parent node
	if(m_node->m_parent) {
	    m_node= m_node->m_parent;
	    return true;
	}
	return false;
    }
    bool GoChild() { // move cursor to the child node
	if(m_node->m_child) {
	    m_node= m_node->m_child;
	    return true;
	}
	return false;
    }
    bool GoSibling() { // move cursor to the sibling node
	if(m_node->m_sibling) {
	    m_node= m_node->m_sibling;
	    return true;
	}
	return false;
    }
    bool GoNode(CTreeContNodeBase* node) { // move cursor to the node with given node_id
	if(node) {
	    m_node= node;
	    return true;
	}
	return false;
    }

    bool GoAncestor(CTreeContNodeBase* node); // move cursor to the nearest common ancestor
                                     // between node pointed by cursor and the node
                                     // with given node_id

    // callback for ForEachNodeInSubtree method
    // this function should return eStop if it wants to abandon the nodes scanning
    // or eBreak to abandon scanning of subtree belonging to the current node
    enum EAction {
	eCont,   // Continue scan
	eStop,   // Stop scanning, exit immediately
	eSkip   // Skip current node's subree and continue scanning
    };
    typedef EAction (*ForEachFunc)(CTreeContNodeBase* pNode, void* user_data);
    // "Callback" class for traversing the tree.
    // For 'downward' traverse node (nodes that closer to root processed first)
    // order of execution is: execute(), levelBegin(), and levelEnd(). Latter
    // two functions are called only when node has children.
    // For 'upward' traverse node (nodes that closer to leaves processed first)
    // order of execution is: levelBegin(), levelEnd(), and execute(). Former
    // two functions are called only when node has children.
    class C4Each {
    public:
        virtual ~C4Each(void) {}
	virtual EAction LevelBegin(CTreeContNodeBase* /*pParent*/)
	{ return eCont; }
	virtual EAction Execute(CTreeContNodeBase* pNode)= 0;
	virtual EAction LevelEnd(CTreeContNodeBase* /*pParent*/)
	{ return eCont; }
    };
    // iterator through subtree.
    // it calls the ucb function one time for each node in given subtree
    // (including subtree root)
    // to abandon scanning ucb should return eStop.
    // (the iterator will stay on node which returns this code)

    // 'Downward' traverse functions (nodes that closer to root processed first)
    EAction ForEachDownward(ForEachFunc ucb, void* user_data);
    EAction ForEachDownward(C4Each&);
    EAction ForEachDownwardLimited(ForEachFunc ucb, void* user_data, int levels);
    EAction ForEachDownwardLimited(C4Each&, int levels);
    // 'Upward' traverse node (nodes that closer to leaves processed first)
    EAction ForEachUpward(ForEachFunc ucb, void* user_data);
    EAction ForEachUpward(C4Each&);
    EAction ForEachUpwardLimited(ForEachFunc ucb, void* user_data, int levels);
    EAction ForEachUpwardLimited(C4Each&, int levels);

    // modification of tree
    class CSortPredicate {
    public:
        virtual ~CSortPredicate(void) {}
	virtual bool Execute( CTreeContNodeBase* p1, CTreeContNodeBase* p2 )=0;
    };
    // add child to a node pointed by cursor
    bool AddChild(CTreeContNodeBase* new_node);
    // add sibling AFTER a node pointed by cursor
    bool AddSibling(CTreeContNodeBase* new_node);
    // add child preserving the sorted order
    bool AddChild(CTreeContNodeBase* new_node, CSortPredicate& );

    void SortChildren( CSortPredicate& );
    void SortAllChildrenInSubtree( CSortPredicate& );
    //bool updateNode(const void* node_data, int node_data_size);
    bool DeleteNode(); // delete node pointed by cursor (cursor will be moved to the parent node in the end)
    bool DeleteSubtree(); // delete subtree pointed by cursor (cursor will be moved to the parent node in the end)
    bool MoveNode(CTreeContNodeBase* to_node); // move the node (subtree) pointed by cursor
                                        // to the new parent with given node_id

    bool MoveChildren(CTreeContNodeBase* to_node); // move children from the node pointed by cursor
                                            // to the new parent with given node_id

    bool Merge(CTreeContNodeBase* to_node); // merge node pointed by cursor with the node
                                     // with given node_id

    // notify others about update
    void NodeUpdated() {
	m_tree->Done(m_node);
    }

    // retrieval
    CTreeContNodeBase* GetNode() const {return m_node;}
    bool BelongSubtree(const CTreeContNodeBase* subtree_root); // check if node pointed by cursor
                                                  // is belong to subtree wich root node
                                                  // has given node_id
    bool AboveNode(CTreeContNodeBase* node); // check if node with given node_id belongs
                                    // to subtree pointed by cursor

    CTreeIterator(CTreeCont* tree) {
	m_tree= tree;
//	tree->m_cursorPot.add((PotItem)this);
	GoRoot();
    }

    ~CTreeIterator() {
//	m_tree->m_cursorPot.remove((PotItem)this);
    }

private:
    CTreeIterator() {}
    CTreeContNodeBase* m_node;
    class CTreeCont*   m_tree;
};

class CTreeConstIterator {
public:
    // navigation
    void GoRoot() {// move cursor to the root node
	m_node= m_tree->m_root;
    }
    bool GoParent() {// move cursor to the parent node
	if(m_node->m_parent) {
	    m_node= m_node->m_parent;
	    return true;
	}
	return false;
    }
    bool GoChild() { // move cursor to the child node
	if(m_node->m_child) {
	    m_node= m_node->m_child;
	    return true;
	}
	return false;
    }
    bool GoSibling() { // move cursor to the sibling node
	if(m_node->m_sibling) {
	    m_node= m_node->m_sibling;
	    return true;
	}
	return false;
    }
    bool GoNode(const CTreeContNodeBase* pNode) {
	if(pNode) {
	    m_node= pNode;
	    return true;
	}
	return false;
    }

    bool GoAncestor(const CTreeContNodeBase* node); // move cursor to the nearest common ancestor
                                     // between node pointed by cursor and the node
                                     // with given node_id
    // retrieval
    const CTreeContNodeBase* GetNode() const
    {return m_node;}
    // check if node pointed by cursor
    // is belong to subtree wich root node
    // has given node_id
    bool BelongSubtree(const CTreeContNodeBase* subtree_root) const;
    // check if node with given node_id belongs
    // to subtree pointed by cursor
    bool AboveNode(const CTreeContNodeBase* node) const;

    CTreeConstIterator(const CTreeCont* tree)
	: m_node( tree->m_root ), m_tree( tree ) {
	//tree->m_cursorPot.add((PotItem)this);
	//goRoot();
    }

    virtual ~CTreeConstIterator() {
	//m_tree->m_cursorPot.remove((PotItem)this);
    }

private:
    CTreeConstIterator(){}
    const CTreeContNodeBase* m_node;
    const CTreeCont* m_tree;
};

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

#endif // NCBI_OBJECTS_CTREECONT_HPP
