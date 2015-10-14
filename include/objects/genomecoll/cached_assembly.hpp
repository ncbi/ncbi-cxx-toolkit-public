/* $Id
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
 */

#ifndef INTERNAL_GPIPE_OBJECTS_GENOMECOLL_CACHED_ASSEMBLY_HPP
#define INTERNAL_GPIPE_OBJECTS_GENOMECOLL_CACHED_ASSEMBLY_HPP

#include <objects/genomecoll/GC_Assembly.hpp>
#include <util/compress/stream_util.hpp>

BEGIN_NCBI_SCOPE

class CCachedAssembly : public CObject
{
public:
    CCachedAssembly(CRef<objects::CGC_Assembly> assembly);
    CCachedAssembly(const string& blob);
    CCachedAssembly(const vector<char>& blob);

    CRef<objects::CGC_Assembly> Assembly();
    const string& Blob(CCompressStream::EMethod neededCompression);

public:
    static bool ValidBlob(int blobSize);

private:
    CRef<objects::CGC_Assembly> m_assembly;
    //TODO: replace with vector<char> after migration to NetCache
    string m_blob;
};

END_NCBI_SCOPE

#endif
