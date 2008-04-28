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
 * Author:  Kevin Bealer
 *
 */

/// @file seqdbobj.cpp
/// Definitions of various helper functions for SeqDB.

#include <ncbi_pch.hpp>
#include <objtools/readers/seqdb/seqdbcommon.hpp>

#include <objmgr/seq_vector.hpp>
#include <util/sequtil/sequtil_convert.hpp>

BEGIN_NCBI_SCOPE

/// Include definitions from the objects namespace.
USING_SCOPE(objects);


/// Compute a sequence hash from a (generic) source of ncbi8na values.
/// @param TSource Type of object for `src' parameter.
/// @param src Object providing sequence data as ncbi8na values.
template<class TSource>
unsigned SeqDB_ComputeSequenceHash(TSource & src)
{
    unsigned retval = 0;
    
    while(src.More()) {
        unsigned seq_i = unsigned(src.Get()) & 0xFF;
        
        retval *= 1103515245;
        retval += seq_i + 12345;
    }
    
    return retval;
}


/// Forward iteration (only) for an array of sequence data.
struct SSeqDB_ArraySource {
    /// Construct sequence data source from existing array.
    /// @param ptr Pointer to beginning of ncbi8na data.
    /// @param len Length of sequence in bases (== bytes).
    SSeqDB_ArraySource(const char * ptr, int len)
        : begin(ptr), end(ptr + len)
    {
    }
    
    /// Check whether there is more data to fetch.
    /// @return True if any unfetched data remains.
    bool More()
    {
        return begin != end;
    }
    
    /// Get a nucleotide base value and move iteration forward.
    /// @return One nucleotide base value.
    unsigned char Get()
    {
        return *(begin++);
    }
    
private:
    /// Pointer to the first unprocessed byte of sequence data.
    const char *begin;
    
    /// Pointer to the end of the sequence data array.
    const char *end;
};


/// Forward iteration (only) for sequence data of a Bioseq object.
struct SSeqDB_SVCISource {
    /// Constructor.
    /// @param bs Bioseq providing the sequence over which to iterate.
    SSeqDB_SVCISource(const CBioseq & bs)
        : index(0), size(0)
    {
        // Note: the CSeqVector API provides eCoding_Ncbi, which is
        // labelled as ncbi4na, but op[] provides one residue or base
        // per byte, which is what the SeqDB and ASN.1 formats refer
        // to as the "ncbi8na" format, which is what we need here.
        
        seqvector = CSeqVector(bs,
                               0,
                               CBioseq_Handle::eCoding_Ncbi,
                               eNa_strand_plus);
        
        size = seqvector.size();
    }
    
    /// Check whether there is more data to fetch.
    /// @return True if any unfetched data remains.
    bool More()
    {
        return index < size;
    }
    
    /// Get a nucleotide base value and move iteration forward.
    /// @return One nucleotide base value.
    unsigned char Get()
    {
        return seqvector[index++];
    }
    
private:
    /// Pointer to the first unprocessed byte of sequence data.
    CSeqVector seqvector;
    
    /// Index of the next base of sequence data to return.
    TSeqPos index;
    
    /// Total number of bases of sequence data to iterate over.
    TSeqPos size;
};


/// Compute the hash of a sequence in ncbi8na format.
/// @param sequence A sequence in ncbi8na format.
/// @param length Length of the sequence in bases (== bytes).
/// @return The hash value of the sequence data.
unsigned SeqDB_SequenceHash(const char * sequence,
                            int          length)
{
    SSeqDB_ArraySource src(sequence, length);
    return SeqDB_ComputeSequenceHash(src);
}

/// Compute the hash of a sequence in a Bioseq.
/// @param bs The Bioseq containing the sequence.
/// @return The hash value of the sequence data.
unsigned SeqDB_SequenceHash(const CBioseq & bs)
{
    SSeqDB_SVCISource src(bs);
    return SeqDB_ComputeSequenceHash(src);
}

END_NCBI_SCOPE

