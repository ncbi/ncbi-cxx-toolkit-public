#ifndef OBJOSTRASNB__HPP
#define OBJOSTRASNB__HPP

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
* Revision 1.19  2000/04/13 14:50:18  vasilche
* Added CObjectIStream::Open() and CObjectOStream::Open() for easier use.
*
* Revision 1.18  2000/04/06 16:10:51  vasilche
* Fixed bug with iterators in choices.
* Removed unneeded calls to ReadExternalObject/WriteExternalObject.
* Added output buffering to text ASN.1 data.
*
* Revision 1.17  2000/02/17 20:02:28  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.16  2000/01/10 20:12:37  vasilche
* Fixed duplicate argument names.
* Fixed conflict between template and variable name.
*
* Revision 1.15  2000/01/10 19:46:32  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.14  1999/09/24 18:19:14  vasilche
* Removed dependency on NCBI toolkit.
*
* Revision 1.13  1999/09/23 18:56:54  vasilche
* Fixed bugs with overloaded methods in objistr*.hpp & objostr*.hpp
*
* Revision 1.12  1999/09/22 20:11:50  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.11  1999/08/16 16:07:44  vasilche
* Added ENUMERATED type.
*
* Revision 1.10  1999/07/26 18:31:31  vasilche
* Implemented skipping of unused values.
* Added more useful error report.
*
* Revision 1.9  1999/07/22 17:33:45  vasilche
* Unified reading/writing of objects in all three formats.
*
* Revision 1.8  1999/07/21 14:20:00  vasilche
* Added serialization of bool.
*
* Revision 1.7  1999/07/19 15:50:19  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.6  1999/07/09 16:32:54  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.5  1999/07/07 21:15:03  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.4  1999/07/02 21:31:47  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.3  1999/07/01 17:55:21  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.2  1999/06/30 16:04:33  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.1  1999/06/24 14:44:42  vasilche
* Added binary ASN.1 output.
*
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objostr.hpp>
#include <serial/objstrasnb.hpp>
#include <serial/strbuffer.hpp>
#include <stack>

BEGIN_NCBI_SCOPE

class CObjectOStreamAsnBinary : public CObjectOStream
{
public:
    typedef CObjectStreamAsnBinaryDefs::TByte TByte;
    typedef CObjectStreamAsnBinaryDefs::TTag TTag;
    typedef CObjectStreamAsnBinaryDefs::ETag ETag;
    typedef CObjectStreamAsnBinaryDefs::EClass EClass;

    CObjectOStreamAsnBinary(CNcbiOstream& out);
    CObjectOStreamAsnBinary(CNcbiOstream& out, bool deleteOut);
    virtual ~CObjectOStreamAsnBinary(void);

    virtual bool WriteEnum(const CEnumeratedTypeValues& values, long value);

    void WriteNull(void);
    void WriteByte(TByte byte);
    void WriteBytes(const char* bytes, size_t size);

    void WriteTag(EClass c, bool constructed, TTag index);
    void WriteShortTag(EClass c, bool constructed, TTag index);
    void WriteSysTag(ETag index);
    void WriteClassTag(TTypeInfo typeInfo);

    void WriteLength(size_t length);
    void WriteShortLength(size_t length);
    void WriteIndefiniteLength(void);

    void WriteEndOfContent(void);

protected:
    virtual void WriteBool(bool data);
    virtual void WriteChar(char data);
    virtual void WriteInt(int data);
    virtual void WriteUInt(unsigned data);
    virtual void WriteLong(long data);
    virtual void WriteULong(unsigned long data);
    virtual void WriteDouble(double data);
    virtual void WriteString(const string& s);
    virtual void WriteCString(const char* str);
    virtual void WriteStringStore(const string& s);

#if HAVE_NCBI_C
    virtual unsigned GetAsnFlags(void);
    virtual void AsnWrite(AsnIo& asn, const char* data, size_t length);
#endif

    virtual void WriteMemberPrefix(void);
    virtual void WriteMemberSuffix(const CMemberId& id);
    virtual void WriteNullPointer(void);
    virtual void WriteObjectReference(TIndex index);
    virtual void WriteOther(TConstObjectPtr object,
                            CWriteObjectInfo& info);

    virtual void VBegin(Block& block);
    virtual void VEnd(const Block& block);
    virtual void StartMember(Member& member, const CMemberId& id);
    virtual void StartMember(Member& member,
                             const CMembers& members, TMemberIndex index);
    virtual void EndMember(const Member& member);
	virtual void Begin(const ByteBlock& block);
	virtual void WriteBytes(const ByteBlock& block, const char* bytes,
                            size_t length);

private:
    COStreamBuffer m_Output;

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

//#include <serial/objostrasnb.inl>

END_NCBI_SCOPE

#endif  /* OBJOSTRASNB__HPP */
