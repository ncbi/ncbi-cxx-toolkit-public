#ifndef PUBSEQ_GATEWAY_STAT__HPP
#define PUBSEQ_GATEWAY_STAT__HPP

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
 * Authors: Sergey Satskiy
 *
 * File Description:
 *
 */



// The class is used to collect information about errors
class CPubseqGatewayErrorCounters
{
public:
    CPubseqGatewayErrorCounters() :
        m_BadUrlPath(0), m_InsufficientArguments(0), m_MalformedArguments(0),
        m_ResolveNotFound(0), m_ResolveError(0), m_GetBlobNotFound(0),
        m_GetBlobError(0), m_UnknownError(0)
    {}

    void IncBadUrlPath(void)
    { ++m_BadUrlPath; }

    void IncInsufficientArguments(void)
    { ++m_InsufficientArguments; }

    void IncMalformedArguments(void)
    { ++m_MalformedArguments; }

    void IncResolveNotFound(void)
    { ++m_ResolveNotFound; }

    void IncResolveError(void)
    { ++m_ResolveError; }

    void IncGetBlobNotFound(void)
    { ++m_GetBlobNotFound; }

    void IncGetBlobError(void)
    { ++m_GetBlobError; }

    void IncUnknownError(void)
    { ++m_UnknownError; }

    void IncSatToSatName(void)
    { ++m_SatToSatNameError; }

    void IncCanonicalSeqIdError(void)
    { ++m_CanonicalSeqIdError; }


    uint64_t GetBadUrlPath(void) const
    { return m_BadUrlPath; }

    uint64_t GetInsufficientArguments(void) const
    { return m_InsufficientArguments; }

    uint64_t GetMalformedArguments(void) const
    { return m_MalformedArguments; }

    uint64_t GetResolveNotFound(void) const
    { return m_ResolveNotFound; }

    uint64_t GetResolveError(void) const
    { return m_ResolveError; }

    uint64_t GetGetBlobNotFound(void) const
    { return m_GetBlobNotFound; }

    uint64_t GetGetBlobError(void) const
    { return m_GetBlobError; }

    uint64_t GetUnknownError(void) const
    { return m_UnknownError; }

    uint64_t GetSatToSatNameError(void) const
    { return m_SatToSatNameError; }

    uint64_t GetCanonicalSeqIdError(void) const
    { return m_CanonicalSeqIdError; }

private:
    atomic_uint_fast64_t        m_BadUrlPath;
    atomic_uint_fast64_t        m_InsufficientArguments;
    atomic_uint_fast64_t        m_MalformedArguments;
    atomic_uint_fast64_t        m_ResolveNotFound;      // 404
    atomic_uint_fast64_t        m_ResolveError;         // 502
    atomic_uint_fast64_t        m_GetBlobNotFound;      // 404
    atomic_uint_fast64_t        m_GetBlobError;         // 502
    atomic_uint_fast64_t        m_UnknownError;         // 503
    atomic_uint_fast64_t        m_SatToSatNameError;
    atomic_uint_fast64_t        m_CanonicalSeqIdError;
};



class CPubseqGatewayRequestCounters
{
public:
    CPubseqGatewayRequestCounters() :
        m_Admin(0), m_Resolve(0),
        m_GetBlobBySeqId(0), m_GetBlobBySatSatKey(0)
    {}

    void IncAdmin(void)
    { ++m_Admin; }

    void IncResolve(void)
    { ++m_Resolve; }

    void IncGetBlobBySeqId(void)
    { ++m_GetBlobBySeqId; }

    void IncGetBlobBySatSatKey(void)
    { ++m_GetBlobBySatSatKey; }

    uint64_t GetAdmin(void) const
    { return m_Admin; }

    uint64_t GetResolve(void) const
    { return m_Resolve; }

    uint64_t GetGetBlobBySeqId(void) const
    { return m_GetBlobBySeqId; }

    uint64_t GetGetBlobBySatSatKey(void) const
    { return m_GetBlobBySatSatKey; }

private:
    atomic_uint_fast64_t        m_Admin;
    atomic_uint_fast64_t        m_Resolve;
    atomic_uint_fast64_t        m_GetBlobBySeqId;
    atomic_uint_fast64_t        m_GetBlobBySatSatKey;
};


#endif
