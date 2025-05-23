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
 * File Name: valnode.cpp
 *
 * Author: Alexey Dobronadezhdin
 *
 * File Description:
 *      ValNode functionality from C-toolkit.
 */

#include <ncbi_pch.hpp>

#include "ftacpp.hpp"
#include "ftaerr.hpp"
#include "valnode.h"

#ifdef THIS_FILE
#  undef THIS_FILE
#endif
#define THIS_FILE "valnode.cpp"


BEGIN_NCBI_SCOPE
/*****************************************************************************
 *
 *   ValNodeNew(prev)
 *      adds after prev if not NULL
 *
 *****************************************************************************/
ValNodePtr ValNodeNew(ValNodePtr prev, const char* data)
{
    ValNodePtr newnode = new ValNode;

    if (prev) {
        prev->next = newnode;
    }

    if (data) {
        newnode->data = StringSave(data);
    }

    return newnode;
}

/*****************************************************************************
 *
 *   ValNodeFree(vnp)
 *      frees whole chain of ValNodes
 *       Does NOT free associated data pointers
 *           see ValNodeFreeData()
 *
 *****************************************************************************/
ValNodePtr ValNodeFree(ValNodePtr vnp)
{
    ValNodePtr next;

    while (vnp) {
        next = vnp->next;
        delete vnp;
        vnp = next;
    }
    return nullptr;
}

/*****************************************************************************
 *
 *   ValNodeFreeData(vnp)
 *      frees whole chain of ValNodes
 *       frees associated data pointers - BEWARE of this if these are not
 *           allocated single memory block structures.
 *
 *****************************************************************************/
void ValNodeFreeData(ValNodeList& L)
{
    ValNodePtr vnp = L.head;

    while (vnp) {
        MemFree(vnp->data);
        ValNodePtr next = vnp->next;
        delete vnp;
        vnp = next;
    }
    L.head = nullptr;
}

/*****************************************************************************
 *
 *   ValNodeLink(head, newnode)
 *      adds newnode at end of chain
 *      if (*head == NULL) *head = newnode
 *      ALWAYS returns pointer to START of chain
 *
 *****************************************************************************/
ValNodePtr ValNodeLink(ValNodeList& L, ValNodePtr newnode)
{
    ValNodePtr vnp = L.head;

    if (vnp) {
        while (vnp->next)
            vnp = vnp->next;
        vnp->next = newnode;
    } else
        L.head = newnode;

    return L.head;
}


ValNodePtr ValNodeCopyStrEx(ValNodeList& L, ValNodePtr* tail, short choice, const char* str)
{
    size_t     len;
    ValNodePtr newnode = nullptr, vnp;
    char*      ptr;
    string     tmp;

    if (! str)
        return nullptr;

    newnode = ValNodeNew(nullptr);
    if (! newnode)
        return nullptr;

    len = StringLen(str);

    if (! L.head) {
        L.head = newnode;
    }

    if (tail) {
        if (*tail) {
            vnp = *tail;
            while (vnp->next) {
                vnp = vnp->next;
            }
            vnp->next = newnode;
        }
        *tail = newnode;
    }

    ptr = StringNew(len + 1);
    if (! ptr)
        return nullptr;

    tmp.reserve(len);
    if (len > 0) {
        tmp.append(str);
    }

    StringCpy(ptr, tmp.c_str());

    if (newnode) {
        newnode->choice = (unsigned char)choice;
        newnode->data   = ptr;
    }

    return newnode;
}

END_NCBI_SCOPE
