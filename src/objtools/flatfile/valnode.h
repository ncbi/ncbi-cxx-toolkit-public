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

/*****************************************************************************
 *
 *   DataVal = a universal data type
 *   ValNode = a linked list of DataVal
 *
 *****************************************************************************/

BEGIN_NCBI_SCOPE

struct ValNode {
    unsigned char choice   = 0;       /* to pick a choice */
    char*         data     = nullptr; /* attached data */
    ValNode*      next     = nullptr; /* next in linked list */
};
using ValNodePtr = ValNode*;

ValNodePtr ValNodeNew(ValNodePtr prev, const char* data = nullptr);
ValNodePtr ValNodeFree(ValNodePtr vnp);

struct ValNodeList {
    ValNode* head = nullptr;
};

void       ValNodeFreeData(ValNodeList&);
ValNodePtr ValNodeLink(ValNodeList&, ValNodePtr newnode);
ValNodePtr ValNodeCopyStrEx(ValNodeList&, ValNodePtr* tail, short choice, const char* str);

END_NCBI_SCOPE

#endif
