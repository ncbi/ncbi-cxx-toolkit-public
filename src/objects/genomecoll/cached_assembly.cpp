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

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CCachedAssembly::CCachedAssembly(CRef<CGC_Assembly> assembly)
        : m_assembly(assembly)
{}

CCachedAssembly::CCachedAssembly(const string& blob)
        : m_blob(blob)
{}

CCachedAssembly::CCachedAssembly(const vector<char>& blob)
        : m_blob(blob.begin(), blob.end())
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

static
CCompressStream::EMethod Compression(const string& blob)
{
    if (!CCachedAssembly::ValidBlob(blob.size()))
        NCBI_THROW(CCoreException, CCoreException::eCore, "Invalid blob size detected: " + blob.size());
    const char bzip2Header[] = {0x42, 0x5a, 0x68};
    const char zlibHeader[] = {0x78};
    if (NStr::StartsWith(blob, CTempString(bzip2Header, sizeof(bzip2Header))))
        return CCompressStream::eBZip2;
    if (NStr::StartsWith(blob, CTempString(zlibHeader, sizeof(zlibHeader))))
        return CCompressStream::eZip;
    NCBI_THROW(CCoreException, CCoreException::eInvalidArg, "Cant determine compression method: " + blob.substr(0, 10));
}

CRef<CGC_Assembly> CCachedAssembly::Assembly()
{
    if (m_assembly.NotNull()) {
        return m_assembly;
    }

    if (ValidBlob(m_blob.size())) {
        m_assembly = UncomressAndCreate(m_blob, Compression(m_blob));
    }
    return m_assembly;
}

static
void CompressAssembly(string& blob, CRef<CGC_Assembly> assembly, CCompressStream::EMethod method)
{
    LOG_POST(Info << "Creating blob with compression: " << method);
    _ASSERT(blob.empty());

    CNcbiOstrstream out;
    CCompressOStream compress(out, method);

    compress << MSerial_AsnBinary << (*assembly);
    compress.Finalize();

    blob = CNcbiOstrstreamToString(out);
}

static
string ChangeCompression(const string& blob,
                         CCompressStream::EMethod from, CCompressStream::EMethod to)
{
    LOG_POST(Info << "Changing compression from " << from << " to " << to);
    _ASSERT(from != to);

    CNcbiIstrstream in(blob.data(), blob.size());
    CDecompressIStream from_stream(in, from);
    CNcbiOstrstream out;
    CCompressOStream to_stream(out, to);

    to_stream << from_stream.rdbuf();
    to_stream.Finalize();
    LOG_POST(Info << "Compression done - processed: " << to_stream.GetProcessedSize() << ", old size:" << blob.size() << ", new size: " << to_stream.GetOutputSize());

    return CNcbiOstrstreamToString(out);
}

const string& CCachedAssembly::Blob(CCompressStream::EMethod neededCompression)
{
    _ASSERT(neededCompression == CCompressStream::eBZip2 || neededCompression == CCompressStream::eZip);
    LOG_POST(Info << "Requested blob with compression: " << neededCompression);
    if (m_blob.empty()) {
        CompressAssembly(m_blob, m_assembly, neededCompression);
    }
    else if (neededCompression != Compression(m_blob)) {
        //TODO: remove it once all be switched to new gc_access (conversion will be done inside CGencollCache)
        m_blob = ChangeCompression(m_blob, Compression(m_blob), neededCompression);
    }
    return m_blob;
}

bool CCachedAssembly::ValidBlob(int blobSize)
{
    const int kSmallestZip = 200; // No assembly, let alone a compressed one, will be smaller than this.
    return blobSize >= kSmallestZip;
}

END_NCBI_SCOPE
