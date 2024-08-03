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
class CObjectIStream;

BEGIN_SCOPE(objects)

class CSeq_entry;
class CSeq_submit;
class CSubmit_block;
class ILineErrorListener;

BEGIN_SCOPE(edit)

class NCBI_XHUGEASN_EXPORT CHugeFile : public CObject
{
public:
    CHugeFile();
    virtual ~CHugeFile();
    const set<TTypeInfo>*           m_supported_types = nullptr;

    std::unique_ptr<CMemoryFile>    m_memfile;
    std::unique_ptr<std::istream>   m_stream;
    std::unique_ptr<std::streambuf> m_streambuf;
    std::string                     m_filename;
    const char*                     m_memory = nullptr;
    std::streampos                  m_filesize = 0;

    ESerialDataFormat               m_serial_format = eSerial_None;
    CFormatGuess::EFormat           m_format = CFormatGuess::eUnknown;
    TTypeInfo                       m_content = nullptr;

    void Reset();
    void Open(const std::string& filename, const set<TTypeInfo>* supported_types);
    void OpenPlain(const std::string& filename);
    bool IsOpen() const { return m_filesize != 0; }
    TTypeInfo RecognizeContent(std::streampos pos);
    TTypeInfo RecognizeContent(std::istream& istr);
    unique_ptr<CObjectIStream> MakeObjStream(std::streampos pos = 0) const;

private:
    bool x_TryOpenStreamFile(const string& filename, std::streampos filesize);
    bool x_TryOpenMemoryFile(const string& filename);
};

class NCBI_XHUGEASN_EXPORT CHugeFileException : public CException
{
public:
    enum EErrCode
    {
        eDuplicateSeqIds,
        eDuplicateFeatureIds,
    };
    //virtual const char* GetErrCodeString(void) const override;
    NCBI_EXCEPTION_DEFAULT(CHugeFileException,CException);
};

class NCBI_XHUGEASN_EXPORT IHugeAsnSource
{
public:
    virtual void Open(CHugeFile* file, ILineErrorListener * pMessageListener) = 0;
    virtual bool GetNextBlob() = 0;
    virtual CRef<CSeq_entry> GetNextSeqEntry() = 0;
    virtual bool IsMultiSequence() const = 0;
    virtual CConstRef<CSubmit_block> GetSubmitBlock() const = 0;
    virtual ~IHugeAsnSource(){};
protected:
    IHugeAsnSource(){};
};

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _HUGE_ASN_READER_HPP_INCLUDED_
