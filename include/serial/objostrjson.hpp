#ifndef OBJOSTRJSON__HPP
#define OBJOSTRJSON__HPP

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
* Author: Andrei Gourianov
*
* File Description:
*   Encode data object using JSON format
*/

#include <corelib/ncbistd.hpp>
#include <serial/objostr.hpp>
#include <stack>


/** @addtogroup ObjStreamSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
///
/// CObjectOStreamJson --
///
/// Encode serial data object using JSON format
class NCBI_XSERIAL_EXPORT CObjectOStreamJson : public CObjectOStream
{
public:

    /// Constructor.
    ///
    /// @param out
    ///   Output stream    
    /// @param deleteOut
    ///   when TRUE, the output stream will be deleted automatically
    ///   when the writer is deleted
    /// @deprecated
    ///   Use one with EOwnership enum instead
    NCBI_DEPRECATED_CTOR(CObjectOStreamJson(CNcbiOstream& out, bool deleteOut));

    /// Constructor.
    ///
    /// @param out
    ///   Output stream    
    /// @param deleteOut
    ///   When eTakeOwnership, the output stream will be deleted automatically
    ///   when the writer is deleted
    CObjectOStreamJson(CNcbiOstream& out, EOwnership deleteOut);

    /// Destructor.
    virtual ~CObjectOStreamJson(void);

    /// Set default encoding of 'string' objects
    ///
    /// @param enc
    ///   Encoding
    void SetDefaultStringEncoding(EEncoding enc);

    /// Get default encoding of 'string' objects
    ///
    /// @return
    ///   Encoding
    EEncoding GetDefaultStringEncoding(void) const;

    /// formatting of binary data ('OCTET STRING', 'hexBinary', 'base64Binary')
    enum EBinaryDataFormat {
        eDefault,       ///< default
        eArray_Bool,    ///< array of 'true' and 'false'
        eArray_01,      ///< array of 1 and 0
        eArray_Uint,    ///< array of unsigned integers
        eString_Hex,    ///< HEX string
        eString_01,     ///< string of 0 and 1
        eString_01B,    ///< string of 0 and 1, plus 'B' at the end
        eString_Base64  ///< Base64Binary string
    };
    /// Get formatting of binary data
    ///
    /// @return
    ///   Formatting type
    EBinaryDataFormat GetBinaryDataFormat(void) const;
    
    /// Set formatting of binary data
    ///
    /// @param fmt
    ///   Formatting type
    void SetBinaryDataFormat(EBinaryDataFormat fmt);

    /// Get current stream position as string.
    /// Useful for diagnostic and information messages.
    ///
    /// @return
    ///   string
    virtual string GetPosition(void) const override;

    /// Set JSONP mode
    /// JSONP prefix will become "function_name("
    /// JSONP suffix will become ");"
    void SetJsonpMode(const string& function_name);

    /// Get JSONP padding (prefix and suffix)
    ///
    /// @param prefix
    ///   Receives JSONP prefix
    /// @param suffix
    ///   Receives JSONP suffix
    void GetJsonpPadding(string* prefix, string* suffix) const;

    virtual void WriteFileHeader(TTypeInfo type) override;
    virtual void EndOfWrite(void) override;

protected:
    virtual void WriteBool(bool data) override;
    virtual void WriteChar(char data) override;
    virtual void WriteInt4(Int4 data) override;
    virtual void WriteUint4(Uint4 data) override;
    virtual void WriteInt8(Int8 data) override;
    virtual void WriteUint8(Uint8 data) override;
    virtual void WriteFloat(float data) override;
    virtual void WriteDouble(double data) override;
    void WriteDouble2(double data, unsigned digits);
    virtual void WriteCString(const char* str) override;
    virtual void WriteString(const string& s,
                             EStringType type = eStringTypeVisible) override;
    virtual void WriteStringStore(const string& s) override;
    virtual void CopyString(CObjectIStream& in,
                            EStringType type = eStringTypeVisible) override;
    virtual void CopyStringStore(CObjectIStream& in) override;

    virtual void WriteNullPointer(void) override;
    virtual void WriteObjectReference(TObjectIndex index) override;
    virtual void WriteOtherBegin(TTypeInfo typeInfo) override;
    virtual void WriteOtherEnd(TTypeInfo typeInfo) override;
    virtual void WriteOther(TConstObjectPtr object, TTypeInfo typeInfo) override;

    virtual void WriteNull(void) override;
    virtual void WriteAnyContentObject(const CAnyContentObject& obj) override;
    virtual void CopyAnyContentObject(CObjectIStream& in) override;

    virtual void WriteBitString(const CBitString& obj) override;
    virtual void CopyBitString(CObjectIStream& in) override;

    virtual void WriteEnum(const CEnumeratedTypeValues& values,
                           TEnumValueType value) override;
    virtual void CopyEnum(const CEnumeratedTypeValues& values,
                          CObjectIStream& in) override;

#ifdef VIRTUAL_MID_LEVEL_IO
    virtual void WriteClassMember(const CMemberId& memberId,
                                  TTypeInfo memberType,
                                  TConstObjectPtr memberPtr) override;
    virtual bool WriteClassMember(const CMemberId& memberId,
                                  const CDelayBuffer& buffer) override;
    virtual void WriteClassMemberSpecialCase(
        const CMemberId& memberId, TTypeInfo memberType,
        TConstObjectPtr memberPtr, ESpecialCaseWrite how) override;
#endif

    // low level I/O
    virtual void BeginNamedType(TTypeInfo namedTypeInfo) override;
    virtual void EndNamedType(void) override;

    virtual void BeginContainer(const CContainerTypeInfo* containerType) override;
    virtual void EndContainer(void) override;
    virtual void BeginContainerElement(TTypeInfo elementType) override;
    virtual void EndContainerElement(void) override;

    virtual void BeginClass(const CClassTypeInfo* classInfo) override;
    virtual void EndClass(void) override;
    virtual void BeginClassMember(const CMemberId& id) override;

    virtual void EndClassMember(void) override;

    virtual void BeginChoice(const CChoiceTypeInfo* choiceType) override;
    virtual void EndChoice(void) override;
    virtual void BeginChoiceVariant(const CChoiceTypeInfo* choiceType,
                                    const CMemberId& id) override;
    virtual void EndChoiceVariant(void) override;

    virtual void BeginBytes(const ByteBlock& block) override;
    virtual void WriteBytes(const ByteBlock& block,
                            const char* bytes, size_t length) override;
    virtual void EndBytes(const ByteBlock& block) override;

    virtual void WriteChars(const CharBlock& block,
                            const char* chars, size_t length) override;

    // Write current separator to the stream
    virtual void WriteSeparator(void) override;

private:
    void WriteBase64Bytes(const char* bytes, size_t length);
    void WriteBytes(const char* bytes, size_t length);
    void WriteCustomBytes(const char* bytes, size_t length);

    void WriteMemberId(const CMemberId& id);
    void WriteSkippedMember(void);
    void WriteEscapedChar(char c, EEncoding enc_in);
    void WriteEncodedChar(const char*& src, EStringType type);
    void x_WriteString(const string& value,
                       EStringType type = eStringTypeVisible);
    void WriteKey(const string& key);
    void BeginValue(void);
    void WriteValue(const string& value,
                    EStringType type = eStringTypeVisible);
    void WriteKeywordValue(const string& value);
    void StartBlock(void);
    void EndBlock(void);
    void NextElement(void);
    void BeginArray(void);
    void EndArray(void);
    void NameSeparator(void);

    bool m_FileHeader;
    bool m_BlockStart;
    bool m_ExpectValue;
    string m_SkippedMemberId;
    EEncoding m_StringEncoding;
    EBinaryDataFormat m_BinaryFormat;
    string m_JsonpPrefix;
    string m_JsonpSuffix;
    size_t m_WrapAt;
};


/* @} */

END_NCBI_SCOPE

#endif
