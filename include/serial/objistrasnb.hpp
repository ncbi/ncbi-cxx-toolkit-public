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
* Revision 1.25  2000/06/07 19:45:43  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.24  2000/06/01 19:06:56  vasilche
* Added parsing of XML data.
*
* Revision 1.23  2000/05/24 20:08:13  vasilche
* Implemented XML dump.
*
* Revision 1.22  2000/05/09 16:38:33  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.21  2000/04/28 16:58:02  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.20  2000/04/13 14:50:17  vasilche
* Added CObjectIStream::Open() and CObjectOStream::Open() for easier use.
*
* Revision 1.19  2000/04/06 16:10:51  vasilche
* Fixed bug with iterators in choices.
* Removed unneeded calls to ReadExternalObject/WriteExternalObject.
* Added output buffering to text ASN.1 data.
*
* Revision 1.18  2000/03/14 14:43:30  vasilche
* Fixed error reporting.
*
* Revision 1.17  2000/03/07 14:05:31  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
*
* Revision 1.16  2000/02/17 20:02:28  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.15  2000/01/10 20:04:08  vasilche
* Fixed duplicate name.
*
* Revision 1.14  2000/01/10 19:46:32  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.13  2000/01/05 19:43:45  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
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

BEGIN_NCBI_SCOPE

class CObjectIStreamAsnBinary : public CObjectIStream
{
public:
    typedef CObjectStreamAsnBinaryDefs::TByte TByte;
    typedef CObjectStreamAsnBinaryDefs::TTag TTag;
    typedef CObjectStreamAsnBinaryDefs::ETag ETag;
    typedef CObjectStreamAsnBinaryDefs::EClass EClass;

    CObjectIStreamAsnBinary(void);
    CObjectIStreamAsnBinary(CNcbiIstream& in);
    CObjectIStreamAsnBinary(CNcbiIstream& in, bool deleteIn);

    ESerialDataFormat GetDataFormat(void) const;

    virtual string GetPosition(void) const;

    virtual pair<long, bool> ReadEnum(const CEnumeratedTypeValues& values);

    virtual void ReadNull(void);
    virtual void SkipValue(void);

protected:
    virtual bool ReadBool(void);
    virtual char ReadChar(void);
    virtual int ReadInt(void);
    virtual unsigned ReadUInt(void);
    virtual long ReadLong(void);
    virtual unsigned long ReadULong(void);
    virtual double ReadDouble(void);
    virtual void ReadString(string& s);
    virtual char* ReadCString(void);
    virtual void ReadStringStore(string& s);

    virtual void SkipBool(void);
    virtual void SkipChar(void);
    virtual void SkipSNumber(void);
    virtual void SkipFNumber(void);
    virtual void SkipString(void);
    virtual void SkipStringStore(void);
    virtual void SkipNull(void);
    virtual void SkipByteBlock(void);

protected:
    virtual void ReadArray(CObjectArrayReader& reader,
                           TTypeInfo arrayType, bool randomOrder,
                           TTypeInfo elementType);

    virtual void BeginClass(CObjectStackClass& cls);
    virtual void EndClass(CObjectStackClass& cls);
    virtual TMemberIndex BeginClassMember(CObjectStackClassMember& m,
                                          const CMembers& members);
    virtual TMemberIndex BeginClassMember(CObjectStackClassMember& m,
                                          const CMembers& members,
                                          CClassMemberPosition& pos);
    virtual void EndClassMember(CObjectStackClassMember& m);
    virtual void ReadClassRandom(CObjectClassReader& reader,
                                 TTypeInfo classType,
                                 const CMembersInfo& members);
    virtual void ReadClassSequential(CObjectClassReader& reader,
                                     TTypeInfo classType,
                                     const CMembersInfo& members);

#if 0
    virtual TMemberIndex BeginChoiceVariant(CObjectStackChoiceVariant& v,
                                            const CMembers& variants);
    virtual void EndChoiceVariant(CObjectStackChoiceVariant& v);
#endif
    virtual void ReadChoice(CObjectChoiceReader& reader,
                            TTypeInfo classType,
                            const CMembersInfo& variants);

	virtual void BeginBytes(ByteBlock& block);
	virtual size_t ReadBytes(ByteBlock& block, char* dst, size_t length);
	virtual void EndBytes(const ByteBlock& block);

#if HAVE_NCBI_C
    virtual unsigned GetAsnFlags(void);
    virtual void AsnOpen(AsnIo& asn);
    virtual size_t AsnRead(AsnIo& asn, char* data, size_t length);
#endif

private:
    virtual EPointerType ReadPointerType(void);
    virtual TIndex ReadObjectPointer(void);
    virtual string ReadOtherPointer(void);
    virtual void ReadOtherPointerEnd(void);

    bool SkipRealValue(void);

    // states:
    // before StartTag (Peek*Tag/ExpectSysTag) tag:
    //     m_CurrentTagLength == 0
    //     stream position on tag start
    // after Peek*Tag/ExpectSysTag tag:
    //     m_CurrentTagLength == beginning of LENGTH field
    //     stream position on tag start
    // after FlushTag (Read*Length/ExpectIndefiniteLength):
    //     m_CurrentTagLength == 0
    //     stream position on tad DATA start
    //     tag limit is pushed on stack and new tag limit is updated
    // after EndOfTag
    //     m_CurrentTagLength == 0
    //     stream position on tag DATA end
    //     tag limit is popped from stack
    // m_CurrentTagLength == beginning of LENGTH field
    //                         -- after any of Peek?Tag or ExpectSysTag
    // 
    size_t m_CurrentTagLength;  // length of tag header (without length field)
    size_t m_CurrentTagLimit;   // end of current tag data (INT_MAX if unlimited)
    stack<size_t> m_Limits;

#if CHECK_STREAM_INTEGRITY
    enum ETagState {
        eTagStart,
        eTagParsed,
        eLengthValue,
        eData
    };
    ETagState m_CurrentTagState;
#endif

    // low level interface
private:
    TByte PeekTagByte(size_t index = 0);
    TByte StartTag(void);
    TTag PeekTag(void);
    TTag PeekTag(EClass c, bool constructed);
    string PeekClassTag(void);
    TByte PeekAnyTag(void);
    void ExpectSysTag(EClass c, bool constructed, ETag tag);
    void ExpectSysTag(ETag tag);
    TByte FlushTag(void);
    void ExpectIndefiniteLength(void);
    bool PeekIndefiniteLength(void);
public:
    size_t ReadShortLength(void);
private:
    size_t ReadLength(void);
    size_t StartTagData(size_t length);
    void ExpectShortLength(size_t length);
    void ExpectEndOfContent(void);
public:
    void EndOfTag(void);
    TByte ReadByte(void);
    signed char ReadSByte(void);
    void ExpectByte(TByte byte);
private:
    void ReadBytes(char* buffer, size_t count);
    void SkipBytes(size_t count);

    void ReadStringValue(string& s);
    void SkipTagData(void);
    bool HaveMoreElements(void);
    void UnexpectedMember(TTag tag);
    void UnexpectedTag(TTag tag);
    void UnexpectedByte(TByte byte);
};

//#include <serial/objistrasnb.inl>

END_NCBI_SCOPE

#endif  /* OBJISTRASNB__HPP */
