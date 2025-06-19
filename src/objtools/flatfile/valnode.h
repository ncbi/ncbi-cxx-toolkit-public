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
 * File Name: valnode.h
 *
 * Author: Alexey Dobronadezhdin
 *
 * File Description:
 *      ValNode functionality from C-toolkit.
 */

#ifndef VALNODE_H
#define VALNODE_H

#include <objects/seqloc/Seq_id.hpp>

/*****************************************************************************
 *
 *   DataVal = a universal data type
 *   ValNode = a linked list of DataVal
 *
 *****************************************************************************/

BEGIN_NCBI_SCOPE
using objects::CSeq_id;

struct ValNode {
    CSeq_id::E_Choice choice = CSeq_id::e_not_set; /* to pick a choice */
    char*         data     = nullptr; /* attached data */
    ValNode*      next     = nullptr; /* next in linked list */

    ValNode(CSeq_id::E_Choice, string_view);
};
using ValNodePtr = ValNode*;

struct ValNodeList {
    ValNode* head = nullptr;

    bool                     empty() const { return head == nullptr; }
    ValNode*                 begin() { return head; }
    const ValNode*           cbegin() const { return head; }
    constexpr ValNode*       end() { return nullptr; }
    constexpr const ValNode* cend() const { return nullptr; }
    const ValNode&           front() const { return *head; }

    void emplace_front(CSeq_id::E_Choice choice, string_view data)
    {
        head = new ValNode(choice, data);
    }
    static ValNode* emplace_after(ValNode* pos, CSeq_id::E_Choice choice, string_view data)
    {
        ValNode* newnode = new ValNode(choice, data);
        pos->next        = newnode;
        return newnode;
    }
    void pop_front()
    {
        ValNode* p = head;
        head       = p->next;
        delete p;
    }
    void clear();
};

END_NCBI_SCOPE

#endif
