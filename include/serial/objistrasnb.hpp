#ifndef OBJISTRASNB__HPP
#define OBJISTRASNB__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.12  1999/10/18 20:11:16  vasilche
* Enum values now have long type.
* Fixed template generation for enums.
*
* Revision 1.11  1999/09/24 18:19:13  vasilche
* Removed dependency on NCBI toolkit.
*
* Revision 1.10  1999/09/23 18:56:53  vasilche
* Fixed bugs with overloaded methods in objistr*.hpp & objostr*.hpp
*
* Revision 1.9  1999/09/22 20:11:49  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.8  1999/08/16 16:07:43  vasilche
* Added ENUMERATED type.
*
* Revision 1.7  1999/07/26 18:31:30  vasilche
* Implemented skipping of unused values.
* Added more useful error report.
*
* Revision 1.6  1999/07/22 17:33:42  vasilche
* Unified reading/writing of objects in all three formats.
*
* Revision 1.5  1999/07/21 20:02:14  vasilche
* Added embedding of ASN.1 binary output from ToolKit to our binary format.
*
* Revision 1.4  1999/07/21 14:19:57  vasilche
* Added serialization of bool.
*
* Revision 1.3  1999/07/09 16:32:53  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.2  1999/07/07 21:15:03  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.1  1999/07/02 21:31:45  vasilche
* Implemented reading from ASN.1 binary format.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objistr.hpp>
#include <serial/objstrasnb.hpp>
#include <stack>

#ifdef _DEBUG
#define CHECK_STREAM_INTEGRITY 1
#else
#undef CHECK_STREAM_INTEGRITY
#endif

BEGIN_NCBI_SCOPE

class CObjectIStreamAsnBinary : public CObjectIStream
{
public:
    typedef CObjectStreamAsnBinaryDefs::TByte TByte;
    typedef CObjectStreamAsnBinaryDefs::TTag TTag;
    typedef CObjectStreamAsnBinaryDefs::ETag ETag;
    typedef CObjectStreamAsnBinaryDefs::EClass EClass;

    CObjectIStreamAsnBinary(CNcbiIstream& in);
    virtual ~CObjectIStreamAsnBinary(void);

    virtual long ReadEnumValue(void);

    virtual void SkipValue(void);

    unsigned char ReadByte(void);
    signed char ReadSByte(void);
    void ReadBytes(char* buffer, size_t count);
    void SkipBytes(size_t count);

protected:
    virtual bool ReadBool(void);
    virtual char ReadChar(void);
    virtual int ReadInt(void);
    virtual unsigned ReadUInt(void);
    virtual long ReadLong(void);
    virtual unsigned long ReadULong(void);
    virtual double ReadDouble(void);
    virtual string ReadString(void);
    virtual char* ReadCString(void);

public:
    void ExpectByte(TByte byte);

    ETag ReadSysTag(void);
    void BackSysTag(void);
    void FlushSysTag(bool constructed = false);
    TTag ReadTag(void);

    ETag ReadSysTag(EClass c, bool constructed);
    TTag ReadTag(EClass c, bool constructed);

    void ExpectSysTag(ETag tag);
    void ExpectSysTag(EClass c, bool constructed, ETag tag);
    bool LastTagWas(EClass c, bool constructed);
    bool LastTagWas(EClass c, bool constructed, ETag tag);

    size_t ReadShortLength(void);
    size_t ReadLength(bool allowIndefinite = false);

    void ExpectShortLength(size_t length);
    void ExpectIndefiniteLength(void);
    void ExpectEndOfContent(void);

protected:
    virtual void VBegin(Block& block);
    virtual bool VNext(const Block& block);
    virtual void StartMember(Member& member);
    virtual void EndMember(const Member& member);
	virtual void Begin(ByteBlock& block);
	virtual size_t ReadBytes(const ByteBlock& block, char* dst, size_t length);

#if HAVE_NCBI_C
    virtual unsigned GetAsnFlags(void);
    virtual void AsnOpen(AsnIo& asn);
    virtual size_t AsnRead(AsnIo& asn, char* data, size_t length);
#endif

private:
    virtual EPointerType ReadPointerType(void);
    virtual string ReadMemberPointer(void);
    virtual void ReadMemberPointerEnd(void);
    virtual TIndex ReadObjectPointer(void);
    virtual string ReadOtherPointer(void);
    virtual void ReadOtherPointerEnd(void);

    string ReadClassTag(void);

    bool SkipRealValue(void);

    CNcbiIstream& m_Input;

    TByte m_LastTagByte;
    enum ELastTagState {
        eNoTagRead,
        eSysTagRead,
        eSysTagBack
    };
    ELastTagState m_LastTagState;

#if CHECK_STREAM_INTEGRITY
    size_t m_CurrentPosition;
    enum ETagState {
        eTagStart,
        eTagValue,
        eLengthStart,
        eLengthValueFirst,
        eLengthValue,
        eData
    };
    ETagState m_CurrentTagState;
    size_t m_CurrentTagPosition;
    TByte m_CurrentTagCode;
    size_t m_CurrentTagLengthSize;
    size_t m_CurrentTagLength;
    size_t m_CurrentTagLimit;
    stack<size_t> m_Limits;

    void StartTag(TByte code);
    void EndTag(void);
    void SetTagLength(size_t length);
#endif
};

//#include <serial/objistrasnb.inl>

END_NCBI_SCOPE

#endif  /* OBJISTRASNB__HPP */
