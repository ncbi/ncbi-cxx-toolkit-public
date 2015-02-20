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
    virtual string GetPosition(void) const;


    virtual void WriteFileHeader(TTypeInfo type);
    virtual void WriteEnum(const CEnumeratedTypeValues& values,
                           TEnumValueType value);
    virtual void CopyEnum(const CEnumeratedTypeValues& values,
                          CObjectIStream& in);

protected:
    void WriteEnum(const CEnumeratedTypeValues& values,
                   TEnumValueType value, const string& valueName);
    virtual void WriteBool(bool data);
    virtual void WriteChar(char data);
    virtual void WriteInt4(Int4 data);
    virtual void WriteUint4(Uint4 data);
    virtual void WriteInt8(Int8 data);
    virtual void WriteUint8(Uint8 data);
    virtual void WriteFloat(float data);
    virtual void WriteDouble(double data);
    void WriteDouble2(double data, unsigned digits);
    virtual void WriteCString(const char* str);
    virtual void WriteString(const string& str,
                             EStringType type = eStringTypeVisible);
    virtual void WriteStringStore(const string& str);
    virtual void CopyString(CObjectIStream& in,
                            EStringType type = eStringTypeVisible);
    virtual void CopyStringStore(CObjectIStream& in);

    virtual void WriteNullPointer(void);
    virtual void WriteObjectReference(TObjectIndex index);
    virtual void WriteOtherBegin(TTypeInfo typeInfo);
    virtual void WriteOther(TConstObjectPtr object, TTypeInfo typeInfo);
    void WriteId(const string& str);

    void WriteNull(void);
    virtual void WriteAnyContentObject(const CAnyContentObject& obj);
    virtual void CopyAnyContentObject(CObjectIStream& in);

    virtual void WriteBitString(const CBitString& obj);
    virtual void CopyBitString(CObjectIStream& in);

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

    // COPY
    virtual void CopyContainer(const CContainerTypeInfo* containerType,
                               CObjectStreamCopier& copier);
    virtual void CopyClassRandom(const CClassTypeInfo* objectType,
                                 CObjectStreamCopier& copier);
    virtual void CopyClassSequential(const CClassTypeInfo* objectType,
                                     CObjectStreamCopier& copier);
#endif
    // low level I/O
    virtual void BeginContainer(const CContainerTypeInfo* containerType);
    virtual void EndContainer(void);
    virtual void BeginContainerElement(TTypeInfo elementType);

    virtual void BeginClass(const CClassTypeInfo* classInfo);
    virtual void EndClass(void);
    virtual void BeginClassMember(const CMemberId& id);

    virtual void BeginChoice(const CChoiceTypeInfo* choiceType);
    virtual void EndChoice(void);
    virtual void BeginChoiceVariant(const CChoiceTypeInfo* choiceType,
                                    const CMemberId& id);

    virtual void BeginBytes(const ByteBlock& block);
    virtual void WriteBytes(const ByteBlock& block,
                            const char* bytes, size_t length);
    virtual void EndBytes(const ByteBlock& block);

    virtual void BeginChars(const CharBlock& block);
    virtual void WriteChars(const CharBlock& block,
                            const char* chars, size_t length);
    virtual void EndChars(const CharBlock& block);

    // Write current separator to the stream
    virtual void WriteSeparator(void);

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
