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

/** @file seqsrc_mock.cpp
 * Mock object implementations of the BlastSeqSrc interface.
 */

#include <ncbi_pch.hpp>
#include "seqsrc_mock.hpp"

#include <algo/blast/core/blast_seqsrc_impl.h>
#include <algo/blast/core/blast_util.h>
#include "test_objmgr.hpp"
#include <blast_objmgr_priv.hpp>

#include <corelib/ncbistr.hpp>

const Int4 CRandomlyFailMockBlastSeqSrc::kDefaultInt4 = 50;
const Int8 CRandomlyFailMockBlastSeqSrc::kDefaultInt8 = 50;
const char* CRandomlyFailMockBlastSeqSrc::kDefaultString = "Hello";
const char* CRandomlyFailMockBlastSeqSrc::kNullString = NULL;
const Int4 CRandomlyFailMockBlastSeqSrc::kDefaultOid = 7;

CRandomlyFailMockBlastSeqSrc::CRandomlyFailMockBlastSeqSrc()
    : m_RandGen(new ncbi::CRandom(time(NULL)))
{
    m_IsProtein = x_SelectRandomlyBetween(TRUE, FALSE);
}

CRandomlyFailMockBlastSeqSrc::~CRandomlyFailMockBlastSeqSrc()
{
    delete m_RandGen;
}

Int4 
CRandomlyFailMockBlastSeqSrc::GetNumSeqs() 
{
    return x_SelectRandomlyBetween(kDefaultInt4,
                            static_cast<Int4>(BLAST_SEQSRC_ERROR));
}

Int4 
CRandomlyFailMockBlastSeqSrc::GetMaxSeqLen() 
{
    return x_SelectRandomlyBetween(kDefaultInt4,
                            static_cast<Int4>(BLAST_SEQSRC_ERROR));
}

Int4 
CRandomlyFailMockBlastSeqSrc::GetAvgSeqLen() 
{
    return x_SelectRandomlyBetween(kDefaultInt4,
                            static_cast<Int4>(BLAST_SEQSRC_ERROR));
}

Int8
CRandomlyFailMockBlastSeqSrc::GetTotLen()
{
    return x_SelectRandomlyBetween(kDefaultInt8, 
                            static_cast<Int8>(BLAST_SEQSRC_ERROR));
}

const char*
CRandomlyFailMockBlastSeqSrc::GetSeqSrcName()
{
    return x_SelectRandomlyBetween(kDefaultString, kNullString);
}

Boolean
CRandomlyFailMockBlastSeqSrc::GetIsProt()
{
    return m_IsProtein;
}

Int4
CRandomlyFailMockBlastSeqSrc::IteratorNext(BlastSeqSrcIterator* /*itr*/)
{
    return x_SelectRandomlyBetween(kDefaultOid, 
                            static_cast<Int4>(BLAST_SEQSRC_EOF));
}

Int4
CRandomlyFailMockBlastSeqSrc::GetSequenceLength(Int4 oid)
{
    return (oid == kDefaultOid ? kDefaultInt4 : BLAST_SEQSRC_ERROR);
}

Int2
CRandomlyFailMockBlastSeqSrc::GetSequence(BlastSeqSrcGetSeqArg* seq_arg)
{
    if (x_SelectRandomlyBetween(TRUE, FALSE)) {
        return BLAST_SEQSRC_ERROR;
    } else {
        if (seq_arg->oid == BLAST_SEQSRC_EOF) {
            return BLAST_SEQSRC_EOF;
        } else if (seq_arg->oid == kDefaultOid) {
            try { x_PopulateBLAST_SequenceBlk(seq_arg); }
            catch (const CException&) { return BLAST_SEQSRC_ERROR; }
            return BLAST_SEQSRC_SUCCESS;
        } else {
            return BLAST_SEQSRC_ERROR;
        }
    }
}

void 
CRandomlyFailMockBlastSeqSrc::ReleaseSequence(BlastSeqSrcGetSeqArg* seq_arg) 
{
    ASSERT(seq_arg);
    seq_arg->seq = BlastSequenceBlkFree(seq_arg->seq);
}

void
CRandomlyFailMockBlastSeqSrc::x_PopulateBLAST_SequenceBlk(BlastSeqSrcGetSeqArg*
                                                          seq_arg)
{
    std::string seqid_string(seq_arg->encoding == eBlastEncodingProtein ?
                        "gi|129295" : "gi|555");
    Int4 sequence_length = kDefaultInt4;
    pair<TSeqPos, TSeqPos> sequence_range(0, kDefaultInt4);

    objects::CSeq_id seqid(seqid_string);
    auto_ptr<blast::SSeqLoc> sl
        (CTestObjMgr::Instance().CreateSSeqLoc(seqid, sequence_range));

    blast::SBlastSequence seq = 
        blast::GetSequence(*sl->seqloc, seq_arg->encoding, sl->scope);

    // no exceptions in the code below
    Int2 rv = BlastSeqBlkNew(&seq_arg->seq);
    ASSERT(rv == 0);
    rv = BlastSeqBlkSetSequence(seq_arg->seq, seq.data.release(), 
                                sequence_length);
    ASSERT(rv == 0);
}

extern "C" {

static Int2
s_MockBlastSeqSrcGetSequence(void* mock_handle, BlastSeqSrcGetSeqArg* arg)
{
    ASSERT(mock_handle);
    ASSERT(arg);

    IMockBlastSeqSrc* impl = static_cast<IMockBlastSeqSrc*>(mock_handle);
    return impl->GetSequence(arg);
}

static Int4
s_MockBlastSeqSrcGetSeqLen(void* mock_handle, void* oid)
{
    ASSERT(mock_handle);
    ASSERT(oid);

    IMockBlastSeqSrc* impl = static_cast<IMockBlastSeqSrc*>(mock_handle);
    Int4 ordinal_id = *(static_cast<Int4*>(oid));
    return impl->GetSequenceLength(ordinal_id);
}

static Int4
s_MockBlastSeqSrcGetNumSeqs(void* mock_handle, void*)
{
    ASSERT(mock_handle);
    IMockBlastSeqSrc* impl = static_cast<IMockBlastSeqSrc*>(mock_handle);
    return impl->GetNumSeqs();
}

static Int4
s_MockBlastSeqSrcGetMaxSeqLen(void* mock_handle, void*)
{
    ASSERT(mock_handle);
    IMockBlastSeqSrc* impl = static_cast<IMockBlastSeqSrc*>(mock_handle);
    return impl->GetMaxSeqLen();
}

static Int4
s_MockBlastSeqSrcGetAvgSeqLen(void* mock_handle, void*)
{
    ASSERT(mock_handle);
    IMockBlastSeqSrc* impl = static_cast<IMockBlastSeqSrc*>(mock_handle);
    return impl->GetAvgSeqLen();
}

static Int8
s_MockBlastSeqSrcGetTotLen(void* mock_handle, void*)
{
    ASSERT(mock_handle);
    IMockBlastSeqSrc* impl = static_cast<IMockBlastSeqSrc*>(mock_handle);
    return impl->GetTotLen();
}

static const char*
s_MockBlastSeqSrcGetSeqSrcName(void* mock_handle, void*)
{
    ASSERT(mock_handle);
    IMockBlastSeqSrc* impl = static_cast<IMockBlastSeqSrc*>(mock_handle);
    return impl->GetSeqSrcName();
}

static Boolean
s_MockBlastSeqSrcGetIsProt(void* mock_handle, void*)
{
    ASSERT(mock_handle);
    IMockBlastSeqSrc* impl = static_cast<IMockBlastSeqSrc*>(mock_handle);
    return impl->GetIsProt();
}

static void
s_MockBlastSeqSrcReleaseSequence(void* mock_handle, BlastSeqSrcGetSeqArg* arg)
{
    ASSERT(mock_handle);
    ASSERT(arg);
    IMockBlastSeqSrc* impl = static_cast<IMockBlastSeqSrc*>(mock_handle);
    impl->ReleaseSequence(arg);
}

static Int4
s_MockBlastSeqSrcItrNext(void* mock_handle, BlastSeqSrcIterator* itr)
{
    ASSERT(mock_handle);
    IMockBlastSeqSrc* impl = static_cast<IMockBlastSeqSrc*>(mock_handle);
    return impl->IteratorNext(itr);
}

// Destructor
BlastSeqSrc* s_MockBlastSeqSrcFree(BlastSeqSrc* seq_src)
{
    if ( !seq_src ) {
        return NULL;
    }

    const BlastSeqSrc* seqsrc = static_cast<const BlastSeqSrc*>(seq_src);
    IMockBlastSeqSrc* impl = static_cast<IMockBlastSeqSrc*>
        (_BlastSeqSrcImpl_GetDataStructure(seqsrc));
    delete impl;
    return NULL;
}

// Constructor
static BlastSeqSrc*
s_MockBlastSeqSrcNew(BlastSeqSrc* retval, void* args)
{
    ASSERT(retval);

    EMockBlastSeqSrcMode mode = *(static_cast<EMockBlastSeqSrcMode*>(args));
    IMockBlastSeqSrc* impl = NULL;

    switch (mode) {
    case eMBSS_AlwaysFail:
        impl = new CAlwaysFailMockBlastSeqSrc;
        break;
    case eMBSS_RandomlyFail:
        impl = new CRandomlyFailMockBlastSeqSrc;
        break;
    default:
        {
            std::string msg = "Invalid EMockBlastSeqSrcMode: " +
                ncbi::NStr::IntToString(static_cast<int>(mode));
            _BlastSeqSrcImpl_SetInitErrorStr(retval, strdup(msg.c_str()));
        }
        return retval;
    }

    _BlastSeqSrcImpl_SetDeleteFnPtr   (retval, &s_MockBlastSeqSrcFree);
    _BlastSeqSrcImpl_SetDataStructure (retval, (void*) impl);
    _BlastSeqSrcImpl_SetGetNumSeqs    (retval, &s_MockBlastSeqSrcGetNumSeqs);
    _BlastSeqSrcImpl_SetGetMaxSeqLen  (retval, &s_MockBlastSeqSrcGetMaxSeqLen);
    _BlastSeqSrcImpl_SetGetAvgSeqLen  (retval, &s_MockBlastSeqSrcGetAvgSeqLen);
    _BlastSeqSrcImpl_SetGetTotLen     (retval, &s_MockBlastSeqSrcGetTotLen);
    _BlastSeqSrcImpl_SetGetName       (retval, &s_MockBlastSeqSrcGetSeqSrcName);
    _BlastSeqSrcImpl_SetGetIsProt     (retval, &s_MockBlastSeqSrcGetIsProt);
    _BlastSeqSrcImpl_SetGetSequence   (retval, &s_MockBlastSeqSrcGetSequence);
    _BlastSeqSrcImpl_SetGetSeqLen     (retval, &s_MockBlastSeqSrcGetSeqLen);
    _BlastSeqSrcImpl_SetReleaseSequence(retval,
                                        &s_MockBlastSeqSrcReleaseSequence);
    _BlastSeqSrcImpl_SetIterNext      (retval, &s_MockBlastSeqSrcItrNext);

    return retval;
}

} // end extern "C"

BlastSeqSrc*
MockBlastSeqSrcInit(EMockBlastSeqSrcMode mode)
{
    BlastSeqSrc* retval = NULL;
    BlastSeqSrcNewInfo bssn_info;

    bssn_info.constructor = &s_MockBlastSeqSrcNew;
    bssn_info.ctor_argument = static_cast<void*>(&mode);

    retval = BlastSeqSrcNew(&bssn_info);

    return retval;
}
