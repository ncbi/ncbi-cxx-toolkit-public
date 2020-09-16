/* valnode.h
 *
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
 * File Name:  valnode.h
 *
 * Author: Alexey Dobronadezhdin
 *
 * File Description:
 * -----------------
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

typedef union dataval {
    void* ptrvalue;
    int intvalue;
    double realvalue;
    bool        boolvalue;
    int	(*funcvalue)();
    Int8    bigintvalue;
}	DataVal, *DataValPtr;

typedef struct valnode {
    unsigned char choice;          /* to pick a choice */
    unsigned char extended;        /* extra fields reserved to NCBI allocated in structure */
    DataVal data;              /* attached data */
    bool    fatal;
    struct valnode *next;  /* next in linked list */
} ValNode, *ValNodePtr;

typedef union _IntPnt_ {
    int intvalue;
    void* ptrvalue;
} IntPnt;

typedef struct _Choice_ {
    unsigned char choice;
    IntPnt value;
} Choice, *ChoicePtr;

/* convenience structure for holding head and tail of ValNode list for efficient tail insertion */
typedef struct valnodeblock {
    ValNodePtr  head;
    ValNodePtr  tail;
} ValNodeBlock, *ValNodeBlockPtr;

ValNodePtr ValNodeNew(ValNodePtr vnp);
ValNodePtr ValNodeFree(ValNodePtr vnp);
ValNodePtr ValNodeFreeData(ValNodePtr vnp);
ValNodePtr ValNodeLink(ValNodePtr* head, ValNodePtr newnode);

ValNodePtr ValNodeCopyStrEx(ValNodePtr* head, ValNodePtr* tail, short choice, const char* str);
char* ValNodeMergeStrsEx(ValNodePtr list, char* separator);

END_NCBI_SCOPE

#endif
