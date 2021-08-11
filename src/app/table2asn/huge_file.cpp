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

#include "huge_file.hpp"

#include <objtools/readers/format_guess_ex.hpp>
#include <corelib/ncbifile.hpp>
#include <objtools/readers/reader_exception.hpp>


BEGIN_NCBI_SCOPE

CHugeFile::CHugeFile(){}
CHugeFile::~CHugeFile(){}

void CHugeFile::Open(const std::string& filename)
{
    if (!x_TryOpenMemoryFile(filename))
        x_TryOpenStreamFile(filename);

    RecognizeContent(*m_stream);
}

bool CHugeFile::x_TryOpenMemoryFile(const string& filename)
{
    std::unique_ptr<CMemoryFile> memfile{new CMemoryFile(filename,
        CMemoryFile_Base::eMMP_Read,
        CMemoryFile_Base::eMMS_Private)};

    m_filesize = memfile->GetFileSize();
    m_memory = (const char*)memfile->Map(0, 0);

    if (m_filesize == 0 || m_memory == 0)
        return false;

    m_memfile = std::move(memfile);

    m_streambuf.reset(new CMemoryStreamBuf(m_memory, m_filesize));
    m_stream.reset(new std::istream(m_streambuf.get()));

    return true;
}

bool CHugeFile::x_TryOpenStreamFile(const string& filename)
{
    std::unique_ptr<std::ifstream> stream{new std::ifstream(filename, ios::binary)};
    if (!stream->is_open())
        return false;

    m_filesize = CFile(filename).GetLength();
    if (m_filesize == 0)
        return false;

    //stream->seekg(m_filesize-1);
    //stream->seekg(0);

    m_stream = std::move(stream);
    m_filename = filename;
    return true;
}

TTypeInfo CHugeFile::RecognizeContent(std::streampos pos)
{
    if (m_memory)
    {
        CMemoryStreamBuf strbuf(m_memory + pos, m_filesize - pos);
        std::istream istr(&strbuf);
        return RecognizeContent(istr);
    } else {
        m_stream->seekg(pos);
        return RecognizeContent(*m_stream);
    }
}

TTypeInfo CHugeFile::RecognizeContent(std::istream& istr)
{
    CFileContentInfo content_info;

    CFormatGuessEx FG(istr);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eBinaryASN);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eTextASN);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eFasta);
    FG.GetFormatHints().AddPreferredFormat(CFormatGuess::eGff3);
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
            return nullptr;
            break;
        default:
            NCBI_THROW2(CObjReaderParseException, eFormat, "File format not supported", 0);
    }

    TTypeInfo object_type = nullptr;

    if (m_supported_types)
        for (auto rec: *m_supported_types)
        {
            if (rec->GetName() == content_info.mInfoGenbank.mObjectType)
            {
                object_type = rec;
                break;
            }
        }

    if (object_type == nullptr)
        NCBI_THROW2(CObjReaderParseException, eFormat, "File format not supported", 0);

    return object_type;
}


END_NCBI_SCOPE
