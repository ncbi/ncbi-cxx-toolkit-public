#ifndef OBJISTRASN__HPP
#define OBJISTRASN__HPP

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
*   Decode input data in ASN text format
*/

#include <corelib/ncbistd.hpp>
#include <serial/objistr.hpp>
#include <corelib/tempstr.hpp>


/** @addtogroup ObjStreamSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
///
/// CObjectIStreamAsn --
///
/// Decode input data in ASN.1 text format
class NCBI_XSERIAL_EXPORT CObjectIStreamAsn : public CObjectIStream
{
public:
    /// Constructor.
    ///
    /// @param how
    ///   Defines how to fix unprintable characters in ASN VisiableString
    CObjectIStreamAsn(EFixNonPrint how = eFNP_Default);

    /// Constructor.
    ///
    /// @param in
    ///   input stream    
    /// @param how
    ///   Defines how to fix unprintable characters in ASN VisiableString
    CObjectIStreamAsn(CNcbiIstream& in,
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
    NCBI_DEPRECATED_CTOR(CObjectIStreamAsn(CNcbiIstream& in,
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
    CObjectIStreamAsn(CNcbiIstream& in,
                      EOwnership deleteIn,
                      EFixNonPrint how = eFNP_Default);

    /// Constructor.
    ///
    /// @param buffer
    ///   Data source memory buffer
    /// @param size
    ///   Memory buffer size
    /// @param how
    ///   Defines how to fix unprintable characters in ASN VisiableString
    CObjectIStreamAsn(const char* buffer, size_t size,
                      EFixNonPrint how = eFNP_Default);

    /// Check if there is still some meaningful data that can be read;
    /// this function will skip white spaces and comments
    ///
    /// @return
    ///   TRUE if there is no more data
    virtual bool EndOfData(void);

    /// Get current stream position as string.
    /// Useful for diagnostic and information messages.
    ///
    /// @return
    ///   string
    virtual string GetPosition(void) const;


    virtual string ReadFileHeader(void);
    virtual TEnumValueType ReadEnum(const CEnumeratedTypeValues& values);
    virtual void ReadNull(void);

    void ReadAnyContent(string& value);
    virtual void ReadAnyContentObject(CAnyContentObject& obj);
    void SkipAnyContent(void);
    virtual void SkipAnyContentObject(void);

    virtual void ReadBitString(CBitString& obj);
    virtual void SkipBitString(void);

protected:
    TObjectIndex ReadIndex(void);

    // action: read ID into local buffer
    // return: ID pointer and length
    // note: it is not zero ended
    CTempString ScanEndOfId(bool isId);
    CTempString ReadTypeId(char firstChar);
    CTempString ReadMemberId(char firstChar);
    CTempString ReadUCaseId(char firstChar);
    CTempString ReadLCaseId(char firstChar);
    CTempString ReadNumber(void);

    virtual bool ReadBool(void);
    virtual char ReadChar(void);
    virtual Int4 ReadInt4(void);
    virtual Uint4 ReadUint4(void);
    virtual Int8 ReadInt8(void);
    virtual Uint8 ReadUint8(void);
    virtual double ReadDouble(void);
    virtual void ReadString(string& s,EStringType type = eStringTypeVisible);
    void ReadStringValue(string& s, EFixNonPrint fix_method);
    void FixInput(size_t count, EFixNonPrint fix_method, size_t line);

    virtual void SkipBool(void);
    virtual void SkipChar(void);
    virtual void SkipSNumber(void);
    virtual void SkipUNumber(void);
    virtual void SkipFNumber(void);
    virtual void SkipString(EStringType type = eStringTypeVisible);
    virtual void SkipNull(void);
    virtual void SkipByteBlock(void);

#ifdef VIRTUAL_MID_LEVEL_IO
    virtual void ReadContainer(const CContainerTypeInfo* containerType,
                               TObjectPtr containerPtr);
    virtual void SkipContainer(const CContainerTypeInfo* containerType);

    virtual void ReadClassSequential(const CClassTypeInfo* classType,
                                     TObjectPtr classPtr);
    virtual void ReadClassRandom(const CClassTypeInfo* classType,
                                 TObjectPtr classPtr);
    virtual void SkipClassSequential(const CClassTypeInfo* classType);
    virtual void SkipClassRandom(const CClassTypeInfo* classType);
#endif
    // low level I/O
    virtual void BeginContainer(const CContainerTypeInfo* containerType);
    virtual void EndContainer(void);
    virtual bool BeginContainerElement(TTypeInfo elementType);

    virtual void BeginClass(const CClassTypeInfo* classInfo);
    virtual void EndClass(void);

    virtual TMemberIndex BeginClassMember(const CClassTypeInfo* classType);
    virtual TMemberIndex BeginClassMember(const CClassTypeInfo* classType,
                                          TMemberIndex pos);

    virtual void BeginChoice(const CChoiceTypeInfo* choiceType);
    virtual void EndChoice(void);
    virtual TMemberIndex BeginChoiceVariant(const CChoiceTypeInfo* choiceType);

    virtual void BeginBytes(ByteBlock& block);
    int GetHexChar(void);
    virtual size_t ReadBytes(ByteBlock& block, char* dst, size_t length);
    virtual void EndBytes(const ByteBlock& block);

    virtual void BeginChars(CharBlock& block);
    virtual size_t ReadChars(CharBlock& block, char* dst, size_t length);

private:
    virtual EPointerType ReadPointerType(void);
    virtual TObjectIndex ReadObjectPointer(void);
    virtual string ReadOtherPointer(void);
    virtual void StartDelayBuffer(void);

    // low level methods
    char GetChar(void);
    char PeekChar(void);

    // parse methods
    char GetChar(bool skipWhiteSpace);
    char PeekChar(bool skipWhiteSpace);

    bool GetChar(char c, bool skipWhiteSpace = false);
    void Expect(char c, bool skipWhiteSpace = false);
    bool Expect(char charTrue, char charFalse, bool skipWhiteSpace = false);
    void ExpectString(const char* s, bool skipWhiteSpace = false);

    static bool FirstIdChar(char c);
    static bool IdChar(char c);

    void SkipEndOfLine(char c);
    char SkipWhiteSpace(void);
    char SkipWhiteSpaceAndGetChar(void);
    void SkipComments(void);
    void UnexpectedMember(const CTempString& id, const CItemsInfo& items);
    void BadStringChar(size_t startLine, char c);
    void UnendedString(size_t startLine);

    void AppendStringData(string& s,
                          size_t count,
                          EFixNonPrint fix_method,
                          size_t line);
    void AppendLongStringData(string& s,
                              size_t count,
                              EFixNonPrint fix_method,
                              size_t line);

    void StartBlock(void);
    bool NextElement(void);
    void EndBlock(void);
    TMemberIndex GetAltItemIndex(const CClassTypeInfoBase* classType,
                                 const CTempString& id,
                                 const TMemberIndex pos = kInvalidMember);
    TMemberIndex GetMemberIndex(const CClassTypeInfo* classType,
                                const CTempString& id);
    TMemberIndex GetMemberIndex(const CClassTypeInfo* classType,
                                const CTempString& id,
                                const TMemberIndex pos);
    TMemberIndex GetChoiceIndex(const CChoiceTypeInfo* choiceType,
                                const CTempString& id);

    bool m_BlockStart;
};


/* @} */


END_NCBI_SCOPE

#endif  /* OBJISTRB__HPP */
