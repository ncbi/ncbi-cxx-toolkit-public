#ifndef PSG_CACHE_BYTES_UTIL__HPP_
#define PSG_CACHE_BYTES_UTIL__HPP_

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
 * Authors: Dmitrii Saprykin
 *
 * File Description: utility to pack/unpack LMDB cache key
 *
 */

#include <corelib/ncbistl.hpp>

#include <string>

#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

BEGIN_IDBLOB_SCOPE

class CPubseqGatewayCachePackBytes
{
 public:
    template<int count, typename TInt>
    void Pack(string& value, TInt bytes) const
    {
        static_assert(sizeof(TInt) >= count, "Pack source has size less than byte count");
        using TUInt = typename make_unsigned<TInt>::type;
        using TIsLast = typename conditional<count == 0, true_type, false_type>::type;
        TUInt unsigned_bytes = static_cast<TUInt>(bytes);
        PackImpl<count>(value, unsigned_bytes, TIsLast());
    }

 private:
    template<int count, typename TUInt>
    void PackImpl(string& value, TUInt unsigned_bytes, false_type /*zero_count*/) const
    {
        using TIsLast = typename conditional<count == 1, true_type, false_type>::type;
        unsigned char c = (unsigned_bytes >> ((count - 1) * 8)) & 0xFF;
        value.append(1, static_cast<char>(c));
        PackImpl<count - 1>(value, unsigned_bytes, TIsLast());
    }

    template<int count, typename TUInt>
    void PackImpl(string&, TUInt, true_type /*zero_count*/) const
    {
    }
};

class CPubseqGatewayCacheUnpackBytes
{
 public:
    template<int count, typename TInt>
    TInt Unpack(const char* key) const
    {
        static_assert(sizeof(TInt) >= count, "Unpack target has size less than byte count");
        using TUInt = typename make_unsigned<TInt>::type;
        using TIsLast = typename conditional<count == 0, true_type, false_type>::type;
        const unsigned char* ukey = reinterpret_cast<const unsigned char*>(key);
        TUInt result{};
        UnpackImpl<count>(ukey, result, TIsLast());
        return static_cast<TInt>(result);
    }

 private:
    template<int count, typename TUInt>
    void UnpackImpl(const unsigned char* ukey, TUInt& result, false_type /*zero_count*/) const
    {
        using TIsLast = typename conditional<count == 1, true_type, false_type>::type;
        result |= static_cast<TUInt>(ukey[0]) << ((count - 1) * 8);
        UnpackImpl<count - 1>(ukey + 1, result, TIsLast());
    }

    template<int count, typename TUInt>
    void UnpackImpl(const unsigned char*, TUInt&, true_type /*zero_count*/) const
    {
    }
};

END_IDBLOB_SCOPE

#endif  // PSG_CACHE_BYTES_UTIL__HPP_
