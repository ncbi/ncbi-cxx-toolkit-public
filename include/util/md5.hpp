#ifndef UTIL___MD5__HPP
#define UTIL___MD5__HPP

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
 * Author:  Aaron Ucko (C++ interface); original author unknown
 *
 */

/// @file md5.hpp
/// CMD5 - class for computing Message Digest version 5 checksums.

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup Checksum
 *
 * @{
 */

class NCBI_XUTIL_EXPORT CMD5
{
public:
    CMD5(void);
    void   Update   (const char* buf, size_t length);
    void   Finalize (unsigned char digest[16]);
    string GetHexSum(void);

    // for convenience
    static string GetHexSum(unsigned char digest[16]);

protected:
    enum {
        // Block size defined by algorithm; DO NOT CHANGE.
        kBlockSize = 64
    };

private:
    void Transform(void);

    Uint4         m_Buf[4];
    Int8          m_Bits; // must be a 64-bit count
    unsigned char m_In[kBlockSize];
    bool          m_Finalized;
};

/* @} */

inline string CMD5::GetHexSum(void)
{
    unsigned char digest[16];
    Finalize(digest);
    return GetHexSum(digest);
}

END_NCBI_SCOPE

#endif  /* UTIL___MD5__HPP */
