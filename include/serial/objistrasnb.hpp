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
*   Decode input data in ASN binary format (BER)
*/

#include <corelib/ncbistd.hpp>
#include <serial/objistr.hpp>
#include <serial/impl/objstrasnb.hpp>
#include <stack>


/** @addtogroup ObjStreamSupport
 *
 * @{
 */

#define USE_DEF_LEN  1

BEGIN_NCBI_SCOPE

class CObjectOStreamAsnBinary;

/////////////////////////////////////////////////////////////////////////////
///
/// CObjectIStreamAsnBinary --
///
/// Decode input data in ASN.1 binary format (BER)
class NCBI_XSERIAL_EXPORT CObjectIStreamAsnBinary : public CObjectIStream,
                                                    public CAsnBinaryDefs
{
public:

    /// Constructor.
    ///
    /// @param how
    ///   Defines how to fix unprintable characters in ASN VisiableString
    CObjectIStreamAsnBinary(EFixNonPrint how = eFNP_Default);

    /// Constructor.
    ///
    /// @param in
    ///   input stream    
    /// @param how
    ///   Defines how to fix unprintable characters in ASN VisiableString
    CObjectIStreamAsnBinary(CNcbiIstream& in,
                            EFixNonPrint how = eFNP_Default);

    /// Constructor.
    ///
    /// @param in
    ///   input stream    
    /// @param deleteIn
    ///   when TRUE, the input stream will be deleted automatically
    ///   when the reader is deleted
    /// @param how
    ///   Defines how to fix unprintable characters in ASN VisiableString
    /// @deprecated
    ///   Use one with EOwnership enum instead
    NCBI_DEPRECATED_CTOR(CObjectIStreamAsnBinary(CNcbiIstream& in,
                            bool deleteIn,
                            EFixNonPrint how = eFNP_Default));

    /// Constructor.
    ///
    /// @param in
    ///   input stream    
    /// @param deleteIn
    ///   When eTakeOwnership, the input stream will be deleted automatically
    ///   when the reader is deleted
    /// @param how
    ///   Defines how to fix unprintable characters in ASN VisiableString
    CObjectIStreamAsnBinary(CNcbiIstream& in,
                            EOwnership deleteIn,
                            EFixNonPrint how = eFNP_Default);

    /// Constructor.
    ///
    /// @param reader
    ///   Data source
    /// @param how
    ///   Defines how to fix unprintable characters in ASN VisiableString
    CObjectIStreamAsnBinary(CByteSourceReader& reader,
                            EFixNonPrint how = eFNP_Default);

    /// Constructor.
    ///
    /// @param buffer
    ///   Data source memory buffer
    /// @param size
    ///   Memory buffer size
    /// @param how
    ///   Defines how to fix unprintable characters in ASN VisiableString
    CObjectIStreamAsnBinary(const char* buffer,
                            size_t size,
                            EFixNonPrint how = eFNP_Default);


    virtual set<TTypeInfo> GuessDataType(set<TTypeInfo>& known_types,
                                         size_t max_length = 16,
                                         size_t max_bytes  = 1024*1024) override;

    virtual TEnumValueType ReadEnum(const CEnumeratedTypeValues& values) override;
    virtual void ReadNull(void) override;

    virtual void ReadAnyContentObject(CAnyContentObject& obj) override;
    void SkipAnyContent(void);
    virtual void SkipAnyContentObject(void) override;
    virtual void SkipAnyContentVariant(void) override;

    virtual void ReadBitString(CBitString& obj) override;
    virtual void SkipBitString(void) override;

protected:
    virtual bool ReadBool(void) override;
    virtual char ReadChar(void) override;
    virtual Int4 ReadInt4(void) override;
    virtual Uint4 ReadUint4(void) override;
    virtual Int8 ReadInt8(void) override;
    virtual Uint8 ReadUint8(void) override;
    virtual double ReadDouble(void) override;
    virtual void ReadString(string& s,EStringType type = eStringTypeVisible) override;
    virtual void ReadPackedString(string& s,
                                  CPackString& pack_string,
                                  EStringType type) override;
    virtual char* ReadCString(void) override;
    virtual void ReadStringStore(string& s) override;

    virtual void SkipBool(void) override;
    virtual void SkipChar(void) override;
    virtual void SkipSNumber(void) override;
    virtual void SkipUNumber(void) override;
    virtual void SkipFNumber(void) override;
    virtual void SkipString(EStringType type = eStringTypeVisible) override;
    virtual void SkipStringStore(void) override;
    virtual void SkipNull(void) override;
    virtual void SkipByteBlock(void) override;

#ifdef VIRTUAL_MID_LEVEL_IO
    virtual void ReadNamedType(TTypeInfo namedTypeInfo,
                               TTypeInfo typeInfo, TObjectPtr object) override;
    virtual void SkipNamedType(TTypeInfo namedTypeInfo,
                               TTypeInfo typeInfo) override;
    virtual void ReadContainer(const CContainerTypeInfo* containerType,
                               TObjectPtr containerPtr) override;
    virtual void SkipContainer(const CContainerTypeInfo* containerType) override;

    virtual void ReadClassSequential(const CClassTypeInfo* classType,
                                     TObjectPtr classPtr) override;
    virtual void ReadClassRandom(const CClassTypeInfo* classType,
                                 TObjectPtr classPtr) override;
    virtual void SkipClassSequential(const CClassTypeInfo* classType) override;
    virtual void SkipClassRandom(const CClassTypeInfo* classType) override;

    virtual void ReadChoiceSimple(const CChoiceTypeInfo* choiceType,
                                  TObjectPtr choicePtr) override;
    virtual void SkipChoiceSimple(const CChoiceTypeInfo* choiceType) override;

#endif

    // low level I/O
    virtual void BeginNamedType(TTypeInfo namedTypeInfo) override;
    virtual void EndNamedType(void) override;

    virtual void BeginContainer(const CContainerTypeInfo* containerType) override;
    virtual void EndContainer(void) override;
    virtual bool BeginContainerElement(TTypeInfo elementType) override;

    virtual void BeginClass(const CClassTypeInfo* classInfo) override;
    virtual void EndClass(void) override;
    virtual TMemberIndex BeginClassMember(const CClassTypeInfo* classType) override;
    virtual TMemberIndex BeginClassMember(const CClassTypeInfo* classType,
                                          TMemberIndex pos) override;
    virtual void EndClassMember(void) override;

    virtual void BeginChoice(const CChoiceTypeInfo* choiceType) override;
    virtual void EndChoice(void) override;
    virtual TMemberIndex BeginChoiceVariant(const CChoiceTypeInfo* choiceType) override;
    virtual void EndChoiceVariant(void) override;

    virtual void BeginBytes(ByteBlock& block) override;
    virtual size_t ReadBytes(ByteBlock& block, char* dst, size_t length) override;
    virtual void EndBytes(const ByteBlock& block) override;

    virtual void BeginChars(CharBlock& block) override;
    virtual size_t ReadChars(CharBlock& block, char* dst, size_t length) override;
    virtual void EndChars(const CharBlock& block) override;

#if HAVE_NCBI_C
    friend class CObjectIStream::AsnIo;
#endif
    void ResetThisState(void);
    virtual void ResetState(void) override;

private:
    virtual EPointerType ReadPointerType(void) override;
    virtual TObjectIndex ReadObjectPointer(void) override;
    virtual string ReadOtherPointer(void) override;
    virtual void ReadOtherPointerEnd(void) override;
    virtual pair<TObjectPtr, TTypeInfo> ReadPointer(TTypeInfo declaredType) override;
    virtual void SkipPointer(TTypeInfo declaredType) override;

    bool SkipRealValue(void);

#if CHECK_INSTREAM_STATE
    enum ETagState {
        eTagStart,
        eTagParsed,
        eLengthValue,
        eData
    };
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
    ETagState m_CurrentTagState;
#endif
#if CHECK_INSTREAM_LIMITS
    Int8 m_CurrentTagLimit;   // end of current tag data
    stack<Int8> m_Limits;
#endif
    size_t m_CurrentTagLength;  // length of tag header (without length field)
    bool m_SkipNextTag;
#if USE_DEF_LEN
    Int8 m_CurrentDataLimit;
    vector<Int8> m_DataLimits;
#endif

    // low level interface
private:
    TByte PeekByte(size_t index = 0);
    TByte PeekTagByte(size_t index = 0);
    TByte StartTag(TByte first_tag_byte);
    TLongTag PeekTag(TByte first_tag_byte);
    TLongTag PeekLongTag(void);
    void ExpectTagClassByte(TByte first_tag_byte,
                            TByte expected_class_byte);
    void UnexpectedTagClassByte(TByte first_tag_byte,
                                TByte expected_class_byte);
    void UnexpectedShortLength(size_t got_length,
                               size_t expected_length);
    void UnexpectedFixedLength(void);
    void UnexpectedContinuation(void);
    void UnexpectedLongLength(void);
    void UnexpectedTagValue(ETagClass tag_class, TLongTag tag_got, TLongTag tag_expected);
    TLongTag PeekTag(TByte first_tag_byte,
                     ETagClass tag_class,
                     ETagConstructed tag_constructed);
    string PeekClassTag(void);
    TByte PeekAnyTagFirstByte(void);
    void ExpectSysTagByte(TByte byte);
    void ExpectStringTag(EStringType type);
    string TagToString(TByte byte);
    void UnexpectedSysTagByte(TByte byte);
    void ExpectSysTag(ETagClass tag_class,
                      ETagConstructed tag_constructed,
                      ETagValue tag_value);
    void ExpectSysTag(ETagValue tag_value);
    void ExpectIntegerTag(void);

    void ExpectTag(ETagClass tag_class,
                   ETagConstructed tag_constructed,
                   TLongTag tag_value);
    void ExpectTag(TByte first_tag_byte,
                   ETagClass tag_class,
                   ETagConstructed tag_constructed,
                   TLongTag tag_value);
    void UndoPeekTag(void);

    TByte FlushTag(void);
    void ExpectIndefiniteLength(void);
    bool PeekIndefiniteLength(void);
    void ExpectContainer(bool random);
public:
    size_t ReadShortLength(void);
private:
    size_t ReadLength(void);
    size_t ReadLengthInlined(void);
    size_t ReadLengthLong(TByte byte);
    size_t StartTagData(size_t length);
    void ExpectShortLength(size_t length);
    void ExpectEndOfContent(void);
public:
    void EndOfTag(void);
    Uint1 ReadByte(void);
    Int1 ReadSByte(void);
    void ExpectByte(Uint1 byte);
private:
    void ReadBytes(char* buffer, size_t count);
    void ReadBytes(string& str, size_t count);
    bool FixVisibleChars(char* buffer, size_t& count, EFixNonPrint fix_method);
    bool FixVisibleChars(string& str, EFixNonPrint fix_method);
    void SkipBytes(size_t count);

    void ReadStringValue(size_t length, string& s, EFixNonPrint fix_type);
    void SkipTagData(void);
    bool HaveMoreElements(void);
    void UnexpectedMember(TLongTag tag, const CItemsInfo& items);
    void UnexpectedByte(TByte byte);
    void GetTagPattern(vector<int>& pattern, size_t max_length);

    friend class CObjectOStreamAsnBinary;
};


/* @} */


#include <serial/impl/objistrasnb.inl>

END_NCBI_SCOPE

#endif  /* OBJISTRASNB__HPP */
