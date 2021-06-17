#ifndef OBJISTRJSON__HPP
#define OBJISTRJSON__HPP

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
*   Decode data object using JSON format
*/

#include <corelib/ncbistd.hpp>
#include <serial/objistr.hpp>
//#include <stack>


/** @addtogroup ObjStreamSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
///
/// CObjectIStreamJson --
///
/// Decode serial data object using JSON format
class NCBI_XSERIAL_EXPORT CObjectIStreamJson : public CObjectIStream
{
public:
    CObjectIStreamJson(void);
    ~CObjectIStreamJson(void);

    /// Constructor.
    ///
    /// @param in
    ///   input stream    
    /// @param deleteIn
    ///   When eTakeOwnership, the input stream will be deleted automatically
    ///   when the reader is deleted
    CObjectIStreamJson(CNcbiIstream& in, EOwnership deleteIn);

    /// Check if there is still some meaningful data that can be read;
    /// this function will skip white spaces and comments
    ///
    /// @return
    ///   TRUE if there is no more data
    virtual bool EndOfData(void) override;

    /// Get current stream position as string.
    /// Useful for diagnostic and information messages.
    ///
    /// @return
    ///   string
    virtual string GetPosition(void) const override;

    /// Set default encoding of 'string' objects
    /// If data encoding is different, string will be converted to
    /// this encoding
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

    virtual string ReadFileHeader(void) override;

protected:

    virtual void EndOfRead(void) override;

    virtual bool ReadBool(void) override;
    virtual void SkipBool(void) override;

    virtual char ReadChar(void) override;
    virtual void SkipChar(void) override;

    virtual Int8 ReadInt8(void) override;
    virtual Uint8 ReadUint8(void) override;

    virtual void SkipSNumber(void) override;
    virtual void SkipUNumber(void) override;

    virtual double ReadDouble(void) override;

    virtual void SkipFNumber(void) override;

    virtual void ReadString(string& s,
                            EStringType type = eStringTypeVisible) override;
    virtual void SkipString(EStringType type = eStringTypeVisible) override;

    virtual void ReadNull(void) override;
    virtual void SkipNull(void) override;

    virtual void ReadAnyContentObject(CAnyContentObject& obj) override;
    void SkipAnyContent(void);
    virtual void SkipAnyContentObject(void) override;

    virtual void ReadBitString(CBitString& obj) override;
    virtual void SkipBitString(void) override;

    virtual void SkipByteBlock(void) override;

    virtual TEnumValueType ReadEnum(const CEnumeratedTypeValues& values) override;

#ifdef VIRTUAL_MID_LEVEL_IO
    virtual void ReadClassSequential(const CClassTypeInfo* classType,
                                     TObjectPtr classPtr) override;
    virtual void SkipClassSequential(const CClassTypeInfo* classType) override;
#endif

    // container
    virtual void BeginContainer(const CContainerTypeInfo* containerType) override;
    virtual void EndContainer(void) override;
    virtual bool BeginContainerElement(TTypeInfo elementType) override;
    virtual void EndContainerElement(void) override;

    // class
    virtual void BeginClass(const CClassTypeInfo* classInfo) override;
    virtual void EndClass(void) override;

    virtual TMemberIndex BeginClassMember(const CClassTypeInfo* classType) override;
    virtual TMemberIndex BeginClassMember(const CClassTypeInfo* classType,
                                          TMemberIndex pos) override;
    virtual void EndClassMember(void) override;
    virtual void UndoClassMember(void) override;

    // choice
    virtual void BeginChoice(const CChoiceTypeInfo* choiceType) override;
    virtual void EndChoice(void) override;
    virtual TMemberIndex BeginChoiceVariant(const CChoiceTypeInfo* choiceType) override;
    virtual void EndChoiceVariant(void) override;

    // byte block
    virtual void BeginBytes(ByteBlock& block) override;
    int GetHexChar(void);
    int GetBase64Char(void);
    virtual size_t ReadBytes(ByteBlock& block, char* buffer, size_t count) override;
    virtual void EndBytes(const ByteBlock& block) override;

    // char block
    virtual void BeginChars(CharBlock& block) override;
    virtual size_t ReadChars(CharBlock& block, char* buffer, size_t count) override;
    virtual void EndChars(const CharBlock& block) override;

    virtual EPointerType ReadPointerType(void) override;
    virtual TObjectIndex ReadObjectPointer(void) override;
    virtual string ReadOtherPointer(void) override;

    virtual void ResetState(void) override;

private:

    char GetChar(void);
    char PeekChar(void);
    char GetChar(bool skipWhiteSpace);
    char PeekChar(bool skipWhiteSpace);

    void SkipEndOfLine(char c);
    char SkipWhiteSpace(void);
    char SkipWhiteSpaceAndGetChar(void);

    bool GetChar(char c, bool skipWhiteSpace = false);
    void Expect(char c, bool skipWhiteSpace = false);
    void UnexpectedMember(const CTempString& id, const CItemsInfo& items);
    template<typename Type>
    Type x_UseMemberDefault(void);

    int ReadEscapedChar(bool* encoded=0);
    char ReadEncodedChar(EStringType type, bool& encoded);
    TUnicodeSymbol ReadUtf8Char(char c);
    string x_ReadString(EStringType type);
    void x_ReadData(string& data, EStringType type = eStringTypeUTF8);
    bool x_ReadDataAndCheck(string& data, EStringType type = eStringTypeUTF8);
    void   x_SkipData(void);
    string ReadKey(void);
    string ReadValue(EStringType type = eStringTypeVisible);

    void StartBlock(char expect);
    void EndBlock(char expect);
    bool NextElement(void);

    TMemberIndex FindDeep(const CItemsInfo& items, const CTempString& name, bool& deep) const;
    size_t ReadCustomBytes(ByteBlock& block, char* buffer, size_t count);
    size_t ReadBase64Bytes(ByteBlock& block, char* buffer, size_t count);
    size_t ReadHexBytes(ByteBlock& block, char* buffer, size_t count);

    bool m_FileHeader;
    bool m_BlockStart;
    bool m_ExpectValue;
    bool m_GotNameless;
    char m_Closing;
    EEncoding m_StringEncoding;
    string m_LastTag;
    string m_RejectedTag;
    EBinaryDataFormat m_BinaryFormat;
    CStringUTF8 m_Utf8Buf;
    CStringUTF8::const_iterator m_Utf8Pos;
};

/* @} */

END_NCBI_SCOPE

#endif
