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
* Authors:  Sergiy Gotvyanskyy
*
* File Description:
*
*
*/

#include <ncbi_pch.hpp>

#include <objtools/edit/huge_file.hpp>
#include <objtools/readers/format_guess_ex.hpp>
#include <corelib/ncbifile.hpp>
#include <objtools/readers/reader_exception.hpp>

#include <serial/objistr.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

class CMemoryStreamBuf : public std::streambuf
{
public:
#ifndef NCBI_OS_MSWIN
    CMemoryStreamBuf(const char* ptr, size_t size)
    {
        setg((char*)ptr, (char*)ptr, (char*)ptr + size);
    }
#else
    CMemoryStreamBuf(const char* ptr, size_t size)
    {
        m_real_begin = const_cast<char*>(ptr);
        m_real_end   = m_real_begin + size;
        // see RW-2258 and https://github.com/microsoft/STL/blob/main/stl/inc/streambuf
        // MS implementation of std::streambuf has _Gcount of type int
        size         = std::min<size_t>(size, bmax);
        setg((char*)ptr, (char*)ptr, (char*)ptr + size);
    }

protected:
    using _MyBase = std::streambuf;
    using _Traits = _MyBase::traits_type;

    static constexpr streamsize bmax = std::numeric_limits<int>::max();

    int_type underflow(void) override
    {
        auto pos = gptr();

        if (pos >= m_real_end)
            return _Traits::eof();

        streamsize size = m_real_end - pos;
        if (size > bmax)
            size = bmax;

        setg(pos, pos, pos + size);

        return *pos;
    }


    streamsize xsgetn(CT_CHAR_TYPE* buf, streamsize m) override
    {
        auto pos = gptr();

        if (pos >= m_real_end)
            return 0;

        streamsize size = m_real_end - pos;
        if (m > size)
            m = size;

        memcpy(buf, pos, m);
        size -= m;
        pos += m;

        if (size > bmax)
            size = bmax;

        setg(pos, pos, pos + size);

        return m;
    }

    int_type pbackfail(int_type = _Traits::eof()) override
    {
        // put a character back to stream (always fail)
        return _Traits::eof();
    }
    pos_type seekoff(off_type, ios_base::seekdir, ios_base::openmode /*__mode*/) override
    {
        return pos_type(off_type(-1));
    }

    pos_type seekpos(pos_type, ios_base::openmode /*__mode*/) override
    {
        return pos_type(off_type(-1));
    }
    streamsize showmanyc() override
    {
        auto avail = egptr() - gptr();
        return avail;
    }


private:
    char* m_real_begin{};
    char* m_real_end{};
#endif // NCBI_OS_MSWIN
};

CHugeFile::CHugeFile(){}
CHugeFile::~CHugeFile(){}

void CHugeFile::Open(const std::string& filename, const set<TTypeInfo>* supported_types)
{
    auto filesize = CFile(filename).GetLength();
    if (filesize > 0) {
        if (x_TryOpenMemoryFile(filename) ||
            x_TryOpenStreamFile(filename, filesize)) {
            m_supported_types = supported_types;
            m_content = RecognizeContent(*m_stream);
        }
    }
    if (m_filesize <= 0)
        NCBI_THROW(CFileException, eNotExists, "Cannot open " + filename);
}

bool CHugeFile::x_TryOpenMemoryFile(const string& filename)
{
    try
    {
        auto memfile = std::make_unique<CMemoryFile>(filename,
            CMemoryFile_Base::eMMP_Read,
            CMemoryFile_Base::eMMS_Private);

        m_filesize = memfile->GetFileSize();
        m_filename = filename;
        m_memory = (const char*)memfile->Map(0, 0);

        if (m_filesize == 0 || m_memory == 0)
            return false;

        m_memfile = std::move(memfile);

        m_streambuf.reset(new CMemoryStreamBuf(m_memory, m_filesize));
        m_stream.reset(new std::istream(m_streambuf.get()));

        return true;
    }
    catch(const CFileException& e)
    {
        if (e.GetErrCode() == CFileException::eMemoryMap &&
            NStr::StartsWith(e.GetMsg(), "To be memory mapped the file must exist"))
            return false;
        else
            throw;
    }

}

bool CHugeFile::x_TryOpenStreamFile(const string& filename, std::streampos filesize)
{
    std::unique_ptr<std::ifstream> stream{new std::ifstream(filename, ios::binary)};
    if (!stream->is_open())
        return false;

    m_filesize = filesize;

    //stream->seekg(m_filesize-1);
    //stream->seekg(0);

    m_stream = std::move(stream);
    m_filename = filename;
    return true;
}

TTypeInfo CHugeFile::RecognizeContent(std::streampos pos)
{
    if (!m_memory  &&  !m_stream) {
        return nullptr;
    }
    if (m_memory) {
        CMemoryStreamBuf strbuf(m_memory + pos, m_filesize - pos);
        std::istream istr(&strbuf);
        return RecognizeContent(istr);
    }
    m_stream->seekg(pos);
    return RecognizeContent(*m_stream);
}

TTypeInfo CHugeFile::RecognizeContent(std::istream& istr)
{
    CFileContentInfo content_info;

    CFormatGuessEx FG(istr);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eBinaryASN);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eTextASN);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eFasta);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eGff3);

    // See RW-1892, sometimes compressed files are wrongly
    // guessed as binary ASN.1
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eZip);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eGZip);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eBZip2);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eLzo);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eZstd);

    FG.GetFormatHints().DisableAllNonpreferred();
    if (m_supported_types)
        FG.SetRecognizedGenbankTypes(*m_supported_types);

    m_format = FG.GuessFormatAndContent(content_info);

    switch (m_format)
    {
        case CFormatGuess::eBinaryASN:
            m_serial_format = eSerial_AsnBinary;
            break;
        case CFormatGuess::eTextASN:
            m_serial_format = eSerial_AsnText;
            break;
        case CFormatGuess::eFasta:
        case CFormatGuess::eGff3:
        case CFormatGuess::eZip:
        case CFormatGuess::eGZip:
        case CFormatGuess::eBZip2:
        case CFormatGuess::eLzo:
        case CFormatGuess::eZstd:
            return nullptr;
            break;
        default:
            NCBI_THROW2(CObjReaderParseException, eFormat, "File format not supported", 0);
    }

    TTypeInfo object_type = nullptr;

    if (m_supported_types)
        if (m_supported_types->find(content_info.mInfoGenbank.mTypeInfo) != m_supported_types->end())
            object_type = content_info.mInfoGenbank.mTypeInfo;

    if (object_type == nullptr)
        NCBI_THROW2(CObjReaderParseException, eFormat, "Object type not supported", 0);

    return object_type;
}

unique_ptr<CObjectIStream> CHugeFile::MakeObjStream(std::streampos pos) const
{
    unique_ptr<CObjectIStream> str;

    if (m_memory) {
        auto chunk = Ref(new CMemoryChunk(m_memory+pos, m_filesize-pos, {}, CMemoryChunk::eNoCopyData));
        CMemoryByteSource source(chunk);
        str.reset(CObjectIStream::Create( m_serial_format, source ));
        //str->SetDelayBufferParsingPolicy(CObjectIStream::eDelayBufferPolicyNeverParse);
        str->SetDelayBufferParsingPolicy(CObjectIStream::eDelayBufferPolicyAlwaysParse);
    } else {
        std::unique_ptr<std::ifstream> stream{new std::ifstream(m_filename, ios::binary)};
        stream->seekg(pos);
        str.reset(CObjectIStream::Open(m_serial_format, *stream.release(), eTakeOwnership));
    }

    str->UseMemoryPool();

    return str;
}


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE
