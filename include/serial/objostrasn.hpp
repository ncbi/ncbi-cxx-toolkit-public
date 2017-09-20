#ifndef OBJOSTRASN__HPP
#define OBJOSTRASN__HPP

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
*   Encode data object using ASN text format
*/

#include <corelib/ncbistd.hpp>
#include <serial/objostr.hpp>


/** @addtogroup ObjStreamSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
///
/// CObjectOStreamAsn --
///
/// Encode serial data object using ASN text format
class NCBI_XSERIAL_EXPORT CObjectOStreamAsn : public CObjectOStream
{
public:
    
    /// Constructor.
    ///
    /// @param out
    ///   Output stream
    /// @param how
    ///   Defines how to fix unprintable characters in ASN VisiableString
    CObjectOStreamAsn(CNcbiOstream& out,
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
    /// @deprecated
    ///   Use one with EOwnership enum instead
    NCBI_DEPRECATED_CTOR(CObjectOStreamAsn(CNcbiOstream& out,
                                           bool deleteOut,
                                           EFixNonPrint how = eFNP_Default));

    /// Constructor.
    ///
    /// @param out
    ///   Output stream    
    /// @param deleteOut
    ///   When eTakeOwnership, the output stream will be deleted automatically
    ///   when the writer is deleted
    /// @param how
    ///   Defines how to fix unprintable characters in ASN VisiableString
    CObjectOStreamAsn(CNcbiOstream& out,
                      EOwnership deleteOut,
                      EFixNonPrint how = eFNP_Default);

    /// Destructor.
    virtual ~CObjectOStreamAsn(void);

    /// Get current stream position as string.
    /// Useful for diagnostic and information messages.
    ///
    /// @return
    ///   string
    virtual string GetPosition(void) const override;


    virtual void WriteFileHeader(TTypeInfo type) override;
    virtual void WriteEnum(const CEnumeratedTypeValues& values,
                           TEnumValueType value) override;
    virtual void CopyEnum(const CEnumeratedTypeValues& values,
                          CObjectIStream& in) override;

protected:
    void WriteEnum(const CEnumeratedTypeValues& values,
                   TEnumValueType value, const string& valueName);
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
    virtual void WriteString(const string& str,
                             EStringType type = eStringTypeVisible) override;
    virtual void WriteStringStore(const string& str) override;
    virtual void CopyString(CObjectIStream& in,
                            EStringType type = eStringTypeVisible) override;
    virtual void CopyStringStore(CObjectIStream& in) override;

    virtual void WriteNullPointer(void) override;
    virtual void WriteObjectReference(TObjectIndex index) override;
    virtual void WriteOtherBegin(TTypeInfo typeInfo) override;
    virtual void WriteOther(TConstObjectPtr object, TTypeInfo typeInfo) override;
    void WriteId(const string& str, bool checkCase = false);

    virtual void WriteNull(void) override;
    virtual void WriteAnyContentObject(const CAnyContentObject& obj) override;
    virtual void CopyAnyContentObject(CObjectIStream& in) override;

    virtual void WriteBitString(const CBitString& obj) override;
    virtual void CopyBitString(CObjectIStream& in) override;

#ifdef VIRTUAL_MID_LEVEL_IO
    virtual void WriteContainer(const CContainerTypeInfo* containerType,
                                TConstObjectPtr containerPtr) override;

    virtual void WriteClass(const CClassTypeInfo* objectType,
                            TConstObjectPtr objectPtr) override;
    virtual void WriteClassMember(const CMemberId& memberId,
                                  TTypeInfo memberType,
                                  TConstObjectPtr memberPtr) override;
    virtual bool WriteClassMember(const CMemberId& memberId,
                                  const CDelayBuffer& buffer) override;

    // COPY
    virtual void CopyContainer(const CContainerTypeInfo* containerType,
                               CObjectStreamCopier& copier) override;
    virtual void CopyClassRandom(const CClassTypeInfo* objectType,
                                 CObjectStreamCopier& copier) override;
    virtual void CopyClassSequential(const CClassTypeInfo* objectType,
                                     CObjectStreamCopier& copier) override;
#endif
    // low level I/O
    virtual void BeginContainer(const CContainerTypeInfo* containerType) override;
    virtual void EndContainer(void) override;
    virtual void BeginContainerElement(TTypeInfo elementType) override;

    virtual void BeginClass(const CClassTypeInfo* classInfo) override;
    virtual void EndClass(void) override;
    virtual void BeginClassMember(const CMemberId& id) override;

    virtual void BeginChoice(const CChoiceTypeInfo* choiceType) override;
    virtual void EndChoice(void) override;
    virtual void BeginChoiceVariant(const CChoiceTypeInfo* choiceType,
                                    const CMemberId& id) override;

    virtual void BeginBytes(const ByteBlock& block) override;
    virtual void WriteBytes(const ByteBlock& block,
                            const char* bytes, size_t length) override;
    virtual void EndBytes(const ByteBlock& block) override;

    virtual void BeginChars(const CharBlock& block) override;
    virtual void WriteChars(const CharBlock& block,
                            const char* chars, size_t length) override;
    virtual void EndChars(const CharBlock& block) override;

    // Write current separator to the stream
    virtual void WriteSeparator(void) override;

private:
    void WriteBytes(const char* bytes, size_t length);
    void WriteString(const char* str, size_t length);
    void WriteMemberId(const CMemberId& id);

    void StartBlock(void);
    void NextElement(void);
    void EndBlock(void);

    bool m_BlockStart;
};


/* @} */


END_NCBI_SCOPE

#endif  /* OBJOSTRASN__HPP */
