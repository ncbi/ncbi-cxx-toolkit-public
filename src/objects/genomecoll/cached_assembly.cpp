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
#include <ncbi_pch.hpp>
#include <objects/genomecoll/cached_assembly.hpp>
#include <sstream>
#include <util/compress/stream_util.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CCachedAssembly::CCachedAssembly(CRef<CGC_Assembly> assembly)
        : m_assembly(assembly)
{}

CCachedAssembly::CCachedAssembly(const string& blob)
        : m_blob(blob)
{}

static
CRef<CGC_Assembly> UncomressAndCreate(const string& blob, CCompressStream::EMethod method) {
    CStopWatch sw(CStopWatch::eStart);

    CNcbiIstrstream in(blob.data(), blob.size());
    CDecompressIStream decompress(in, method);

    CRef<CGC_Assembly> m_assembly(new CGC_Assembly);
    decompress >> MSerial_AsnBinary
                >> MSerial_SkipUnknownMembers(eSerialSkipUnknown_Yes)   // Make reading cache backward compatible
                >> MSerial_SkipUnknownVariants(eSerialSkipUnknown_Yes)
                >> (*m_assembly);

    LOG_POST(Info << "Assebmly uncomressed and created in (sec): " << sw.Elapsed());
    return m_assembly;
}

//static
//void Uncomress(const string& blob, CCompressStream::EMethod m) {
//    CStopWatch g(CStopWatch::eStart);
//
//    CNcbiIstrstream in(blob.data(), blob.size());
//    CDecompressIStream lzip(in, m);
//
//    size_t n = 1024*1024;
//    char* buf = new char[n];
//    while (!lzip.eof()) lzip.read(buf, n);
//    delete [] buf;
//
//    LOG_POST(Info << "processed: " << lzip.GetProcessedSize() << ", out: " << lzip.GetOutputSize());
//    LOG_POST(Info << "Assebmly uncomressed in (sec): " << g.Elapsed());
//}

CRef<CGC_Assembly> CCachedAssembly::Assembly()
{
    if (m_assembly.NotNull()) {
        return m_assembly;
    }

    if (ValidBlob(m_blob.size())) {
        m_assembly = UncomressAndCreate(m_blob, BZip2Compression(m_blob) ?
                                                CCompressStream::eBZip2 :
                                                CCompressStream::eZip);
    }
    return m_assembly;
}

static
void CompressAssembly(string& blob, CRef<CGC_Assembly> assembly, CCompressStream::EMethod method)
{
    _ASSERT(assembly.NotNull());
    CNcbiOstrstream out;
    CCompressOStream compress(out, method);

    compress << MSerial_AsnBinary << (*assembly);
    compress.Finalize();

    blob = CNcbiOstrstreamToString(out);
}

const string& CCachedAssembly::Blob()
{
    if (m_blob.empty()) {
        //TODO: change compression to lzip here once all be switched to new gc_access (affects newly created caches)
        //leave BZip2 for DB compression now
        CompressAssembly(m_blob, m_assembly, CCompressStream::eBZip2);
    }
    else if (BZip2Compression(m_blob)) {
        //TODO: remove it once all be switched to new gc_access (conversion will be done inside CGencollCache)
        m_blob = ChangeCompressionBZip2ToZip(m_blob);
    }

    return m_blob;
}

bool CCachedAssembly::ValidBlob(int blobSize) {
    const int kSmallestZip = 200; // No assembly, let alone a compressed one, will be smaller than this.
    return blobSize >= kSmallestZip;
}

bool CCachedAssembly::BZip2Compression(const string& blob) {
    const char bzip2Header[] = {0x42, 0x5a, 0x68};
    return ValidBlob(blob.size()) &&
            NStr::StartsWith(blob, CTempString(bzip2Header, sizeof(bzip2Header)));
}

string CCachedAssembly::ChangeCompressionBZip2ToZip(const string& blob) {
    LOG_POST(Info << "Old bzip2 compression method detected. Changing to zip...");
    _ASSERT(BZip2Compression(blob));

    CNcbiIstrstream in(blob.data(), blob.size());
    CDecompressIStream bzip2(in, CCompressStream::eBZip2);
    CNcbiOstrstream out;
    CCompressOStream lzip(out, CCompressStream::eZip);

    lzip << bzip2.rdbuf();
    lzip.Finalize();
    LOG_POST(Info << "Compression done - processed: " << lzip.GetProcessedSize() << ", old bzip2 size:" << blob.size() << ", new zip size: " << lzip.GetOutputSize());

    return CNcbiOstrstreamToString(out);
}

END_NCBI_SCOPE
