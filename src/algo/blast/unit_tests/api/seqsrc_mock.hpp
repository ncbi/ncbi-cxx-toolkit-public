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
 * Author: Christiam Camacho
 *
 */

/** @file seqsrc_mock.hpp
 * Mock object which implements the BlastSeqSrc interface for testing purposes
 */

#ifndef _SEQSRC_MOCK_HPP
#define _SEQSRC_MOCK_HPP

#include <algo/blast/core/blast_seqsrc.h>
#include <util/random_gen.hpp>

enum EMockBlastSeqSrcMode {
    eMBSS_AlwaysFail,    ///< returns failure on all operations
    eMBSS_RandomlyFail,  ///< returns failure randomly on operations
    eMBSS_Invalid        ///< Sets a limit on the number of valid mock modes
};

/// Mock BlastSeqSrc initialization function
BlastSeqSrc*
MockBlastSeqSrcInit(EMockBlastSeqSrcMode mode = eMBSS_AlwaysFail);

/// Abstract base class which defines a common interface for mock BlastSeqSrc
/// objects to implement
struct IMockBlastSeqSrc 
{
    virtual ~IMockBlastSeqSrc() {}
    virtual Int2 GetSequence(BlastSeqSrcGetSeqArg* seq_arg) = 0;
    virtual Int4 GetSequenceLength(Int4 oid) = 0;
    virtual void ReleaseSequence(BlastSeqSrcGetSeqArg* seq_arg) = 0;
    virtual Int4 GetNumSeqs() = 0;
    virtual Int4 GetMaxSeqLen() = 0;
    virtual Int4 GetAvgSeqLen() = 0;
    virtual Int8 GetTotLen() = 0;
    virtual const char* GetSeqSrcName() = 0;
    virtual Boolean GetIsProt() =  0;
    virtual Int4 IteratorNext(BlastSeqSrcIterator* itr) = 0;
};

/// Mock BlastSeqSrc implementation which always fails
struct CAlwaysFailMockBlastSeqSrc : public IMockBlastSeqSrc
{
    virtual ~CAlwaysFailMockBlastSeqSrc() {}
    Int2 GetSequence(BlastSeqSrcGetSeqArg*) { return BLAST_SEQSRC_ERROR; }
    Int4 GetSequenceLength(Int4) { return BLAST_SEQSRC_ERROR; }
    void ReleaseSequence(BlastSeqSrcGetSeqArg*) {}
    Int4 GetNumSeqs() { return BLAST_SEQSRC_ERROR; }
    Int4 GetMaxSeqLen() { return BLAST_SEQSRC_ERROR; }
    Int4 GetAvgSeqLen() { return BLAST_SEQSRC_ERROR; }
    Int8 GetTotLen() { return BLAST_SEQSRC_ERROR; }
    const char* GetSeqSrcName() { return NULL; }
    Boolean GetIsProt() { return FALSE; }
    Int4 IteratorNext(BlastSeqSrcIterator*) { return BLAST_SEQSRC_EOF; }
};

/// Mock BlastSeqSrc implementation which fails randomly.
/// Its allocation never fails, it is the interface it implements that fails
/// randomly.
/// Note that when the IteratorNext function returns a valid ordinal id
/// (kDefaultOid), GetSequence and GetSequenceLength should work with that 
/// argument (assuming they don't fail randomly ;) ).
class CRandomlyFailMockBlastSeqSrc : public IMockBlastSeqSrc
{
public:
    CRandomlyFailMockBlastSeqSrc();
    virtual ~CRandomlyFailMockBlastSeqSrc();

    // Class constants
    static const Int4 kDefaultInt4;
    static const Int8 kDefaultInt8;
    static const char* kDefaultString;
    static const char* kNullString;
    static const Int4 kDefaultOid;

    Int4 GetNumSeqs();
    Int4 GetMaxSeqLen();
    Int4 GetAvgSeqLen();
    Int8 GetTotLen();
    const char* GetSeqSrcName();
    Boolean GetIsProt();
    Int4 IteratorNext(BlastSeqSrcIterator* itr);
    Int4 GetSequenceLength(Int4 oid);
    Int2 GetSequence(BlastSeqSrcGetSeqArg* seq_arg);
    void ReleaseSequence(BlastSeqSrcGetSeqArg* seq_arg);

private:

    /// Auxiliary function to randomly select a value among the two possible
    /// options passed in as arguments
    template <typename T>
    T x_SelectRandomlyBetween(T success_value, T failure_value) {
        if (m_RandGen->GetRand() % 2 == 0) {
            return success_value;
        } else {
            return failure_value;
        }
    }

    /// Populate structure with some dummy data
    void x_PopulateBLAST_SequenceBlk(BlastSeqSrcGetSeqArg* seq_arg);

    /// The random number generator
    ncbi::CRandom* m_RandGen;
    /// Determine whether this mock BlastSeqSrc contains protein or nucleotide
    /// sequences
    Boolean        m_IsProtein;
};

#endif /* _SEQSRC_MOCK_HPP */
