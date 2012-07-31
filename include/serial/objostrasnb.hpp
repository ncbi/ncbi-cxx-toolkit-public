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
*   Encode data object using ASN binary format (BER)
*/

#include <corelib/ncbistd.hpp>
#include <serial/objostr.hpp>
#include <serial/impl/objstrasnb.hpp>
#include <stack>


/** @addtogroup ObjStreamSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CObjectIStreamAsnBinary;

/////////////////////////////////////////////////////////////////////////////
///
/// CObjectOStreamAsnBinary --
///
/// Encode serial object using ASN binary format (BER)
class NCBI_XSERIAL_EXPORT CObjectOStreamAsnBinary : public CObjectOStream,
                                                    public CAsnBinaryDefs
{
public:

    /// Constructor.
    ///
    /// @param out
    ///   Output stream
    /// @param how
    ///   Defines how to fix unprintable characters in ASN VisiableString
    CObjectOStreamAsnBinary(CNcbiOstream& out,
                            EFixNonPrint how = eFNP_Default);

    /// Constructor.
    ///
    /// @param out
    ///   Output stream    
    /// @param deleteOut
    ///   when TRUE, the output stream will be deleted automatically
    ///   when the writer is deleted
    /// @param how
    ///   Defines how to fix unprintable characters in ASN VisiableString
    CObjectOStreamAsnBinary(CNcbiOstream& out,
                            bool deleteOut,
                            EFixNonPrint how = eFNP_Default);
    /// Destructor.
    virtual ~CObjectOStreamAsnBinary(void);



    virtual void WriteEnum(const CEnumeratedTypeValues& values, 
                           TEnumValueType value);
    virtual void CopyEnum(const CEnumeratedTypeValues& values,
                          CObjectIStream& in);

    void WriteNull(void);
    virtual void WriteAnyContentObject(const CAnyContentObject& obj);
    virtual void CopyAnyContentObject(CObjectIStream& in);

    virtual void WriteBitString(const CBitString& obj);
    virtual void CopyBitString(CObjectIStream& in);

    void WriteLongTag(ETagClass tag_class,
                      ETagConstructed tag_constructed,
                      TLongTag tag_value);
    void WriteClassTag(TTypeInfo typeInfo);
    void WriteLongLength(size_t length);
    EFixNonPrint FixNonPrint(EFixNonPrint how)
        {
            EFixNonPrint tmp = m_FixMethod;
            m_FixMethod = how;
            return tmp;
        }

    // use ASNTOOL-compatible formatting when writing Int8 and Uint8 types
    void SetCStyleBigInt(bool set=true)
    {
        m_CStyleBigInt = set;
    }
    bool GetCStyleBigInt(void) const
    {
        return m_CStyleBigInt;
    }

private:
    void WriteByte(Uint1 byte);
    template<typename T> void WriteBytesOf(const T& value, size_t count);
    void WriteBytes(const char* bytes, size_t size);
    void WriteShortTag(ETagClass tag_class, 
                       ETagConstructed tag_constructed,
                       ETagValue tag_value);
    void WriteSysTag(ETagValue tag);
    static TByte GetUTF8StringTag(void);
    static TByte MakeUTF8StringTag(void);
    void WriteStringTag(EStringType type);
    void WriteTag(ETagClass tag_class,
                  ETagConstructed tag_constructed,
                  TLongTag tag_value);
    void WriteIndefiniteLength(void);
    void WriteShortLength(size_t length);
    void WriteLength(size_t length);
    void WriteEndOfContent(void);

protected:
    virtual void WriteBool(bool data);
    virtual void WriteChar(char data);
    virtual void WriteInt4(Int4 data);
    virtual void WriteUint4(Uint4 data);
    virtual void WriteInt8(Int8 data);
    virtual void WriteUint8(Uint8 data);
    virtual void WriteFloat(float data);
    virtual void WriteDouble(double data);
    void WriteDouble2(double data, size_t digits);
    virtual void WriteCString(const char* str);
    virtual void WriteString(const string& s,
                             EStringType type = eStringTypeVisible);
    virtual void WriteStringStore(const string& s);
    virtual void CopyString(CObjectIStream& in,
                            EStringType type = eStringTypeVisible);
    virtual void CopyStringStore(CObjectIStream& in);
    void CopyStringValue(CObjectIStreamAsnBinary& in,
                         bool checkVisible = false);

    virtual void WriteNullPointer(void);
    virtual void WriteObjectReference(TObjectIndex index);
    virtual void WriteOtherBegin(TTypeInfo typeInfo);
    virtual void WriteOtherEnd(TTypeInfo typeInfo);
    virtual void WriteOther(TConstObjectPtr object, TTypeInfo typeInfo);

#ifdef VIRTUAL_MID_LEVEL_IO
    virtual void WriteContainer(const CContainerTypeInfo* containerType,
                                TConstObjectPtr containerPtr);

    virtual void WriteClass(const CClassTypeInfo* objectType,
                            TConstObjectPtr objectPtr);
    virtual void WriteClassMember(const CMemberId& memberId,
                                  TTypeInfo memberType,
                                  TConstObjectPtr memberPtr);
    virtual bool WriteClassMember(const CMemberId& memberId,
                                  const CDelayBuffer& buffer);

    virtual void WriteChoice(const CChoiceTypeInfo* choiceType,
                             TConstObjectPtr choicePtr);
    // COPY
    virtual void CopyContainer(const CContainerTypeInfo* containerType,
                               CObjectStreamCopier& copier);
    virtual void CopyClassRandom(const CClassTypeInfo* objectType,
                                 CObjectStreamCopier& copier);
    virtual void CopyClassSequential(const CClassTypeInfo* objectType,
                                     CObjectStreamCopier& copier);
//    virtual void CopyChoice(const CChoiceTypeInfo* choiceType,
//                            CObjectStreamCopier& copier);
#endif
    virtual void BeginContainer(const CContainerTypeInfo* containerType);
    virtual void EndContainer(void);

    virtual void BeginClass(const CClassTypeInfo* classInfo);
    virtual void EndClass(void);
    virtual void BeginClassMember(const CMemberId& id);
    virtual void EndClassMember(void);

    virtual void BeginChoice(const CChoiceTypeInfo* choiceType);
    virtual void EndChoice(void);
    virtual void BeginChoiceVariant(const CChoiceTypeInfo* choiceType,
                                    const CMemberId& id);
    virtual void EndChoiceVariant(void);

    virtual void BeginBytes(const ByteBlock& block);
    virtual void WriteBytes(const ByteBlock& block,
                            const char* bytes, size_t length);

    virtual void BeginChars(const CharBlock& block);
    virtual void WriteChars(const CharBlock& block,
                            const char* chars, size_t length);

#if HAVE_NCBI_C
    friend class CObjectOStream::AsnIo;
#endif

private:
    void WriteNumberValue(Int4 data);
    void WriteNumberValue(Int8 data);
    void WriteNumberValue(Uint4 data);
    void WriteNumberValue(Uint8 data);

#if CHECK_OUTSTREAM_INTEGRITY
    Int8 m_CurrentPosition;
    enum ETagState {
        eTagStart,
        eTagValue,
        eLengthStart,
        eLengthValueFirst,
        eLengthValue,
        eData
    };
    ETagState m_CurrentTagState;
    Int8 m_CurrentTagPosition;
    Uint1 m_CurrentTagCode;
    size_t m_CurrentTagLengthSize;
    size_t m_CurrentTagLength;
    Int8 m_CurrentTagLimit;
    stack<Int8> m_Limits;

    void StartTag(TByte code);
    void EndTag(void);
    void SetTagLength(size_t length);
#endif
    EFixNonPrint m_FixMethod; // method of fixing non-printable chars
    bool m_CStyleBigInt;
};


/* @} */


END_NCBI_SCOPE

#endif  /* OBJOSTRASNB__HPP */
