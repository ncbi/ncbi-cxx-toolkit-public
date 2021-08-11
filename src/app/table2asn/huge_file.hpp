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

#ifndef _HUGE_FILE_HPP_INCLUDED_
#define _HUGE_FILE_HPP_INCLUDED_

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <serial/serialdef.hpp>
#include <util/format_guess.hpp>

BEGIN_NCBI_SCOPE

class CMemoryFile;

namespace objects
{
    class CSeq_entry;
    class CSeq_submit;
    class ILineErrorListener;
}

class CHugeFile
{
public:
    CHugeFile();
    virtual ~CHugeFile();
    set<TTypeInfo>*                 m_supported_types = nullptr;
    std::unique_ptr<CMemoryFile>    m_memfile;
    std::unique_ptr<std::istream>   m_stream;
    std::unique_ptr<std::streambuf> m_streambuf;
    std::string                     m_filename;
    const char*                     m_memory = nullptr;
    std::streampos                  m_filesize = 0;

    ESerialDataFormat               m_serial_format = eSerial_None;
    CFormatGuess::EFormat           m_format = CFormatGuess::eUnknown;

    class CMemoryStreamBuf: public std::streambuf
    {
    public:
        CMemoryStreamBuf(const char* ptr, size_t size)
        {
            setg((char*)ptr, (char*)ptr, (char*)ptr+size);
        }
    };

    void Reset();
    void Open(const std::string& filename);
    TTypeInfo RecognizeContent(std::streampos pos);
    TTypeInfo RecognizeContent(std::istream& istr);
private:
    bool x_TryOpenStreamFile(const string& filename);
    bool x_TryOpenMemoryFile(const string& filename);
};

class IHugeAsnSource
{
public:
    virtual void Open(CHugeFile* file, objects::ILineErrorListener * pMessageListener) = 0;
    virtual bool GetNextBlob() = 0;
    virtual CRef<objects::CSeq_entry> GetNextSeqEntry() = 0;
    virtual bool IsMultiSequence() = 0;
    virtual CConstRef<objects::CSeq_submit> GetSubmit() = 0;
    virtual ~IHugeAsnSource(){};
protected:
    IHugeAsnSource(){};
};

END_NCBI_SCOPE

#endif // _HUGE_ASN_READER_HPP_INCLUDED_
