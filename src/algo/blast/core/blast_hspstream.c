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
 * Author:  Christiam Camacho
 *
 */

/** @file blast_hspstream.c
 * Definition of ADT to save and retrieve lists of HSPs in the BLAST engine.
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/blast_hspstream.h>

/** Complete type definition of Blast Hsp Stream ADT */
struct BlastHSPStream {
   BlastHSPStreamConstructor NewFnPtr;    /**< Constructor */
   BlastHSPStreamDestructor  DeleteFnPtr; /**< Destructor */
   
   /* The operational interface */
   
   BlastHSPStreamMethod      WriteFnPtr;  /**< Write to BlastHSPStream */
   BlastHSPStreamMethod      ReadFnPtr;   /**< Read from BlastHSPStream */
   BlastHSPStreamMergeFnType MergeFnPtr;  /**< Merge two BlastHSPStreams */
   BlastHSPStreamBatchReadMethod  BatchReadFnPtr;  /**< Batch read from 
                                                     BlastHSPStream */
   BlastHSPStreamCloseFnType CloseFnPtr;  /**< Close BlastHSPStream for
                                             writing */
   void* DataStructure;                   /**< ADT holding HSPStream */
};

BlastHSPStream* BlastHSPStreamNew(const BlastHSPStreamNewInfo* bhsn_info)
{
    BlastHSPStream* retval = NULL;
    BlastHSPStreamFunctionPointerTypes fnptr;

    if ( bhsn_info == NULL ) {
        return NULL;
    }

    if ( !(retval = (BlastHSPStream*) calloc(1, sizeof(BlastHSPStream)))) {
        return NULL;
    }

    /* Save the constructor and invoke it */
    fnptr.ctor = bhsn_info->constructor;
    SetMethod(retval, eConstructor, fnptr);
    if (retval->NewFnPtr) {
        retval = (*retval->NewFnPtr)(retval, bhsn_info->ctor_argument);
    } else {
        sfree(retval);
    }

    ASSERT(retval->DeleteFnPtr);
    ASSERT(retval->WriteFnPtr);
    ASSERT(retval->ReadFnPtr);

    return retval;
}

BlastHSPStream* BlastHSPStreamFree(BlastHSPStream* hsp_stream)
{
    BlastHSPStreamDestructor destructor_fnptr = NULL;

    if (!hsp_stream) {
        return (BlastHSPStream*) NULL;
    }

    if ( !(destructor_fnptr = (*hsp_stream->DeleteFnPtr))) {
        sfree(hsp_stream);
        return NULL;
    }

    return (BlastHSPStream*) (*destructor_fnptr)(hsp_stream);
}

int BlastHSPStreamMerge(SSplitQueryBlk *squery_blk,
                        Uint4 chunk_num,
                        BlastHSPStream* hsp_stream,
                        BlastHSPStream* combined_hsp_stream)
{
    BlastHSPStreamMergeFnType merge_fnptr = NULL;

    if (!hsp_stream || !combined_hsp_stream)
       return kBlastHSPStream_Error;

    /* Make sure the input streams are compatible */
    if (hsp_stream->MergeFnPtr != hsp_stream->MergeFnPtr)
       return kBlastHSPStream_Error;

    /** Merge functionality is optional. If merge function is not provided,
        just do nothing. */
    if ( !(merge_fnptr = (*hsp_stream->MergeFnPtr))) {
       return kBlastHSPStream_Success;
    }

    return (*merge_fnptr)(squery_blk, chunk_num, 
                          hsp_stream, combined_hsp_stream);
}


BlastHSPStreamResultBatch * 
Blast_HSPStreamResultBatchInit(Int4 num_hsplists)
{
    BlastHSPStreamResultBatch *retval = (BlastHSPStreamResultBatch *)
                             calloc(1, sizeof(BlastHSPStreamResultBatch));

    retval->hsplist_array = (BlastHSPList **)calloc((size_t)num_hsplists,
                                               sizeof(BlastHSPList *));
    return retval;
}

BlastHSPStreamResultBatch * 
Blast_HSPStreamResultBatchFree(BlastHSPStreamResultBatch *batch)
{
    if (batch != NULL) {
        sfree(batch->hsplist_array);
        sfree(batch);
    }
    return NULL;
}

void Blast_HSPStreamResultBatchReset(BlastHSPStreamResultBatch *batch)
{
    Int4 i;
    for (i = 0; i < batch->num_hsplists; i++) {
        batch->hsplist_array[i] = 
           Blast_HSPListFree(batch->hsplist_array[i]);
    }
    batch->num_hsplists = 0;
}

int BlastHSPStreamBatchRead(BlastHSPStream* hsp_stream,
                            BlastHSPStreamResultBatch *batch)
{
    BlastHSPStreamBatchReadMethod batch_read = NULL;

    if (!hsp_stream || !batch)
       return kBlastHSPStream_Error;

    /** Batch read functionality is optional. If batch read 
        function is not provided, just do nothing. */
    if ( !(batch_read = (*hsp_stream->BatchReadFnPtr))) {
       return kBlastHSPStream_Success;
    }

    return (*batch_read)(hsp_stream, batch);
}

void BlastHSPStreamClose(BlastHSPStream* hsp_stream)
{
    BlastHSPStreamCloseFnType close_fnptr = NULL;

    if (!hsp_stream)
       return;

    /** Close functionality is optional. If closing function is not provided,
        just do nothing. */
    if ( !(close_fnptr = (*hsp_stream->CloseFnPtr))) {
       return;
    }

    (*close_fnptr)(hsp_stream);
}

const int kBlastHSPStream_Error = -1;
const int kBlastHSPStream_Success = 0;
const int kBlastHSPStream_Eof = 1;

/** This method is akin to a vtable dispatcher, invoking the method registered
 * upon creation of the implementation of the BlastHSPStream interface 
 * @param hsp_stream The BlastHSPStream object [in]
 * @param name Name of the method to invoke on hsp_stream [in]
 * @param hsp_list HSP list to work with [in] [out]
 * @return kBlastHSPStream_Error on NULL hsp_stream or NULL method pointer 
 * (i.e.: unimplemented or uninitialized method on the BlastHSPStream 
 * interface) or return value of the implementation.
 */
static int 
_MethodDispatcher(BlastHSPStream* hsp_stream, EMethodName name, 
                  BlastHSPList** hsp_list)
{
    BlastHSPStreamMethod method_fnptr = NULL;

    if (!hsp_stream) {
        return kBlastHSPStream_Error;
    }

    ASSERT(name < eMethodBoundary); 

    switch (name) {
    case eRead:
        method_fnptr = (*hsp_stream->ReadFnPtr);
        break;

    case eWrite:
        method_fnptr = (*hsp_stream->WriteFnPtr);
        break;

    default:
        abort();    /* should never happen */
    }

    if (!method_fnptr) {
        return kBlastHSPStream_Error;
    }

    return (*method_fnptr)(hsp_stream, hsp_list);
}

int BlastHSPStreamRead(BlastHSPStream* hsp_stream, BlastHSPList** hsp_list)
{
    return _MethodDispatcher(hsp_stream, eRead, hsp_list);
}

int BlastHSPStreamWrite(BlastHSPStream* hsp_stream, BlastHSPList** hsp_list)
{
    return _MethodDispatcher(hsp_stream, eWrite, hsp_list);
}

/*****************************************************************************/

void* GetData(BlastHSPStream* hsp_stream)
{
    if ( !hsp_stream ) {
        return NULL;
    }

    return hsp_stream->DataStructure;
}

int SetData(BlastHSPStream* hsp_stream, void* data)
{
    if ( !hsp_stream ) {
        return kBlastHSPStream_Error;
    }

    hsp_stream->DataStructure = data;

    return kBlastHSPStream_Success;
}

int SetMethod(BlastHSPStream* hsp_stream, 
              EMethodName name,
              BlastHSPStreamFunctionPointerTypes fnptr_type)
{
    if ( !hsp_stream ) {
        return kBlastHSPStream_Error;
    }

    ASSERT(name < eMethodBoundary); 

    switch (name) {
    case eConstructor:
        hsp_stream->NewFnPtr = fnptr_type.ctor;
        break;

    case eDestructor:
        hsp_stream->DeleteFnPtr = fnptr_type.dtor;
        break;

    case eRead:
        hsp_stream->ReadFnPtr = fnptr_type.method;
        break;

    case eBatchRead:
        hsp_stream->BatchReadFnPtr = fnptr_type.batch_read;
        break;

    case eWrite:
        hsp_stream->WriteFnPtr = fnptr_type.method;
        break;

    case eMerge:
        hsp_stream->MergeFnPtr = fnptr_type.mergeFn;
        break;

    case eClose:
       hsp_stream->CloseFnPtr = fnptr_type.closeFn;
       break;

    default:
        abort();    /* should never happen */
    }

    return kBlastHSPStream_Success;
}
