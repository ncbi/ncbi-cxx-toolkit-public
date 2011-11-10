#ifndef UTIL_SEQUTIL___SEQUTIL_CONVERT__HPP
#define UTIL_SEQUTIL___SEQUTIL_CONVERT__HPP

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
 * Author:  Mati Shomrat
 *
 * File Description:
 *      Sequence conversion utility.
 */   
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbi_limits.hpp>

#include <util/sequtil/sequtil.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//
// Sequence Conversions
//

class NCBI_XUTIL_EXPORT CSeqConvert
{
public:

    // types
    typedef CSeqUtil::ECoding   TCoding;

    // Conversion Methods:
    // Convert sequence from one coding to another.
    //
    // PARAMETERS:
    // src - input container containing the sequence.
    // src_coding - the input's coding
    // pos, length - specify the initial offset and the length
    // dst - output container, or raw memory
    // dst_coding - the desired output coding
    //
    // RETURN VALUE:
    // All methds return the number of converted characers
    // Methods will throw an exception if the conversion can't be 
    // performed. For example when trying to convert NA coding to an AA one.

   
    // string to string
    static SIZE_TYPE Convert(const string& src, TCoding src_coding,
                             TSeqPos pos, TSeqPos length,
                             string& dst, TCoding dst_coding);

    // string to vector
    static SIZE_TYPE Convert(const string& src, TCoding src_coding,
                             TSeqPos pos, TSeqPos length,
                             vector< char >& dst, TCoding dst_coding);

    // vector to string
    static SIZE_TYPE Convert(const vector< char >& src, TCoding src_coding,
                             TSeqPos pos, TSeqPos length,
                             string& dst, TCoding dst_coding);

    // vector to vector
    static SIZE_TYPE Convert(const vector< char >& src, TCoding src_coding,
                             TSeqPos pos, TSeqPos length,
                             vector< char >& dst, TCoding dst_coding);

    // char[] to char[]
    static SIZE_TYPE Convert(const char src[], TCoding src_coding,
                             TSeqPos pos, TSeqPos length,
                             char dst[], TCoding dst_coding);

    // Subseq
    // 
    // The function returns an object whose sequence is a copy of up to 
    // length elements of the controlled sequence beginning at position 
    // pos.
    static SIZE_TYPE Subseq(const string& src, TCoding src_coding,
                            TSeqPos pos, TSeqPos length,
                            string& dst);
    static SIZE_TYPE Subseq(const string& src, TCoding src_coding,
                            TSeqPos pos, TSeqPos length,
                            vector<char>& dst);
    static SIZE_TYPE Subseq(const vector<char>& src, TCoding src_coding,
                            TSeqPos pos, TSeqPos length,
                            string& dst);
    static SIZE_TYPE Subseq(const vector<char>& src, TCoding src_coding,
                            TSeqPos pos, TSeqPos length,
                            vector<char>& dst);
    static SIZE_TYPE Subseq(const char* src, TCoding src_coding,
                            TSeqPos pos, TSeqPos length,
                            char* dst);


    // Packing:
    // Pack will convert a given sequnece to its most condensed form
    // without the loss of information. Hence, sequences containing 
    // ambiguities will be converted to ncbi4na, while those that are
    // comprised of only A, C, G and T will be converted to ncbi2na.

    static SIZE_TYPE Pack(const string& src, TCoding src_coding,
        vector<char>& dst, TCoding& dst_coding, 
        TSeqPos length = ncbi::numeric_limits<TSeqPos>::max());
    static SIZE_TYPE Pack(const vector<char>& src, TCoding src_coding,
        vector<char>& dst, TCoding& dst_coding, 
        TSeqPos length = ncbi::numeric_limits<TSeqPos>::max());
    static SIZE_TYPE Pack(const char* src, TSeqPos length, TCoding src_coding,
                          char* dst, TCoding& dst_coding);

    // For packing piecemeal into a Delta-ext or the like, with ambiguities
    // isolated when doing so is worth the overhead of having more segments
    class IPackTarget
    {
    public:
        typedef CSeqUtil::ECoding     TCoding;
        typedef CSeqUtil::ECodingType TCodingType;

        // Virtual destructor, if only as a matter of principle.
        virtual ~IPackTarget() {}

        // Return the approximate memory overhead per segment, in bytes
        virtual SIZE_TYPE GetOverhead(TCoding coding) const = 0;

        // Returning true signals that Pack may call NewSegment with
        // coding = CSeqUtil::e_not_set as appropriate.
        virtual bool GapsOK(TCodingType coding_type) const = 0;

        // Return a pointer to a character array suitable for storing
        // the given number of residues in the given format.
        virtual char* NewSegment(TCoding coding, TSeqPos length) = 0;
    };

    static SIZE_TYPE Pack(const string& src, TCoding src_coding,
                          IPackTarget& dst,
                          TSeqPos length = numeric_limits<TSeqPos>::max());
    static SIZE_TYPE Pack(const vector<char>& src, TCoding src_coding,
                          IPackTarget& dst,
                          TSeqPos length = numeric_limits<TSeqPos>::max());
    static SIZE_TYPE Pack(const char* src, TSeqPos length, TCoding src_coding,
                          IPackTarget& dst);
};


END_NCBI_SCOPE


#endif  /* UTIL_SEQUTIL___SEQUTIL_CONVERT__HPP */
