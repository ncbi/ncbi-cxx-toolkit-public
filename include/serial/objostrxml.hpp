#ifndef OBJOSTRXML__HPP
#define OBJOSTRXML__HPP

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
*   XML objects output stream
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.10  2000/10/04 19:18:54  vasilche
* Fixed processing floating point data.
*
* Revision 1.9  2000/09/29 16:18:15  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.8  2000/09/18 20:00:07  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.7  2000/09/01 13:16:02  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.6  2000/08/15 19:44:41  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.5  2000/07/03 18:42:36  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.4  2000/06/16 16:31:07  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.3  2000/06/07 19:45:44  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.2  2000/06/01 19:06:58  vasilche
* Added parsing of XML data.
*
* Revision 1.1  2000/05/24 20:08:14  vasilche
* Implemented XML dump.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objostr.hpp>

BEGIN_NCBI_SCOPE

class CObjectOStreamXml : public CObjectOStream
{
public:
    typedef unsigned char TByte;

    CObjectOStreamXml(CNcbiOstream& out, bool deleteOut);
    virtual ~CObjectOStreamXml(void);

    ESerialDataFormat GetDataFormat(void) const;

    virtual void WriteFileHeader(TTypeInfo type);
    virtual void WriteEnum(const CEnumeratedTypeValues& values, long value);
    virtual void CopyEnum(const CEnumeratedTypeValues& values,
                          CObjectIStream& in);
    void WriteEnum(const CEnumeratedTypeValues& values,
                   long value, const string& valueName);

protected:
    virtual void WriteBool(bool data);
    virtual void WriteChar(char data);
    virtual void WriteInt(int data);
    virtual void WriteUInt(unsigned data);
    virtual void WriteLong(long data);
    virtual void WriteULong(unsigned long data);
    virtual void WriteFloat(float data);
    virtual void WriteDouble(double data);
    void WriteDouble2(double data, size_t digits);
    virtual void WriteCString(const char* str);
    virtual void WriteString(const string& s);
    virtual void WriteStringStore(const string& s);
    virtual void CopyString(CObjectIStream& in);
    virtual void CopyStringStore(CObjectIStream& in);

    virtual void WriteNullPointer(void);
    virtual void WriteObjectReference(TObjectIndex index);
    virtual void WriteOtherBegin(TTypeInfo typeInfo);
    virtual void WriteOtherEnd(TTypeInfo typeInfo);
    virtual void WriteOther(TConstObjectPtr object, TTypeInfo typeInfo);
    void WriteId(const string& str);

    void WriteNull(void);
    void WriteEscapedChar(char c);

#ifdef VIRTUAL_MID_LEVEL_IO
    virtual void WriteNamedType(TTypeInfo namedTypeInfo,
                                TTypeInfo typeInfo, TConstObjectPtr object);
    virtual void CopyNamedType(TTypeInfo namedTypeInfo,
                               TTypeInfo typeInfo,
                               CObjectStreamCopier& copier);

    virtual void WriteContainer(const CContainerTypeInfo* containerType,
                                TConstObjectPtr containerPtr);
    void WriteContainerContents(const CContainerTypeInfo* containerType,
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
    void WriteChoiceContents(const CChoiceTypeInfo* choiceType,
                             TConstObjectPtr choicePtr);
/*
    // COPY
    virtual void CopyNamedType(TTypeInfo namedTypeInfo,
                               TTypeInfo typeInfo,
                               CObjectStreamCopier& copier);
    virtual void CopyContainer(const CContainerTypeInfo* containerType,
                               CObjectStreamCopier& copier);
    virtual void CopyClassRandom(const CClassTypeInfo* objectType,
                                 CObjectStreamCopier& copier);
    virtual void CopyClassSequential(const CClassTypeInfo* objectType,
                                     CObjectStreamCopier& copier);
    virtual void CopyChoice(const CChoiceTypeInfo* choiceType,
                            CObjectStreamCopier& copier);
*/
#endif
    // low level I/O
    virtual void BeginNamedType(TTypeInfo namedTypeInfo);
    virtual void EndNamedType(void);

    virtual void BeginContainer(const CContainerTypeInfo* containerType);
    virtual void EndContainer(void);
    virtual void BeginContainerElement(TTypeInfo elementType);
    virtual void EndContainerElement(void);

    virtual void BeginClass(const CClassTypeInfo* classInfo);
    virtual void EndClass(void);
    virtual void BeginClassMember(const CMemberId& id);
    virtual void EndClassMember(void);

    virtual void BeginChoiceVariant(const CChoiceTypeInfo* choiceType,
                                    const CMemberId& id);
    virtual void EndChoiceVariant(void);

	virtual void BeginBytes(const ByteBlock& block);
	virtual void WriteBytes(const ByteBlock& block,
                            const char* bytes, size_t length);
	virtual void EndBytes(const ByteBlock& block);

private:
    void WriteString(const char* str, size_t length);

    void OpenTagStart(void);
    void OpenTagEnd(void);
    bool CloseTagStart(bool forceEolBefore);
    void CloseTagEnd(void);

    void OpenTag(const string& name);
    void CloseTag(const string& name, bool forceEolBefore = false);
    void OpenStackTag(size_t level);
    void CloseStackTag(size_t level, bool forceEolBefore = false);
    void OpenTag(TTypeInfo type);
    void CloseTag(TTypeInfo type, bool forceEolBefore = false);
    void OpenTagIfNamed(TTypeInfo type);
    void CloseTagIfNamed(TTypeInfo type, bool forceEolBefore = false);
    void PrintTagName(size_t level);
    bool WillHaveName(TTypeInfo elementType);
    enum ETagAction {
        eTagOpen,
        eTagClose,
        eTagSelfClosed
    };
    ETagAction m_LastTagAction;
};

#include <serial/objostrxml.inl>

END_NCBI_SCOPE

#endif
