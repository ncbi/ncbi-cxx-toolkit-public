#ifndef CORELIB___NCBI_TREE__HPP
#define CORELIB___NCBI_TREE__HPP
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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:
 *	 Tree container.
 *
 */

#include <corelib/ncbistd.hpp>
#include <list>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
///
///    Bi-directionaly linked N way tree.
///

template <class V> class CTreeNWay
{
public:
    typedef CTreeNWay<V>               TTreeType;
    typedef list<TTreeType*>           TNodeList;
    typedef TNodeList::iterator        nodelist_iterator;
    typedef TNodeList::const_iterator  const_nodelist_iterator;
    
    CTreeNWay();
    ~CTreeNWay();

    const TTreeType* GetParent() const;
    TTreeType* GetParent();

    const_nodelist_iterator SubNodeBegin() const;
    nodelist_iterator SubNodeBegin();    
    const_nodelist_iterator SubNodeEnd() const;
    nodelist_iterator SubNodeEnd();

    const V& GetValue() const;
    V& GetValue();

    void RemoveNode(TTreeType* subnode);
    void RemoveNode(nodelist_iterator it);

    TTreeType* DetachNode(TTreeType* subnode);
    TTreeType* DetachNode(nodelist_iterator it);

    void AddNode(TTreeType* subnode);
    void InsertNode(nodelist_iterator it, TTreeType* subnode);

protected:
    Destroy(TTreeType*  node);

protected:
    CTree*             m_Parent; ///< Pointer on the parent node
    list<TTreeType*>   m_Nodes;  ///< List of dependent node
    V                  m_Value;  ///< Node value
};

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/01/07 13:17:10  kuznets
 * Initial revision
 *
 *
 * ==========================================================================
 */

#endif