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
* Revision 1.17  2001/10/17 18:18:28  grichenk
* Added CObjectOStreamXml::xxxFilePrefix() and
* CObjectOStreamXml::xxxFileName()
*
* Revision 1.16  2001/05/17 14:59:47  lavr
* Typos corrected
*
* Revision 1.15  2001/04/13 14:57:21  kholodov
* Added: SetDTDFileName function to set DTD module name in XML header
*
* Revision 1.14  2000/12/15 21:28:48  vasilche
* Moved some typedefs/enums from corelib/ncbistd.hpp.
* Added flags to CObjectIStream/CObjectOStream: eFlagAllowNonAsciiChars.
* TByte typedef replaced by Uint1.
*
* Revision 1.13  2000/12/15 15:38:01  vasilche
* Added support of Int8 and long double.
* Enum values now have type Int4 instead of long.
*
* Revision 1.12  2000/11/07 17:25:12  vasilche
* Fixed encoding of XML:
*     removed unnecessary apostrophes in OCTET STRING
*     removed unnecessary content in NULL
* Added module names to CTypeInfo and CEnumeratedTypeValues
*
* Revision 1.11  2000/10/20 15:51:27  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
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
    CObjectOStreamXml(CNcbiOstream& out, bool deleteOut);
    virtual ~CObjectOStreamXml(void);

    ESerialDataFormat GetDataFormat(void) const;

    virtual string GetPosition(void) const;

    virtual void WriteFileHeader(TTypeInfo type);
    virtual void WriteEnum(const CEnumeratedTypeValues& values,
                           TEnumValueType value);
    virtual void CopyEnum(const CEnumeratedTypeValues& values,
                          CObjectIStream& in);
    void WriteEnum(const CEnumeratedTypeValues& values,
                   TEnumValueType value, const string& valueName);

    // DTD file name and prefix. The final module name is built as
    // DTDFilePrefix + DTDFileName. If no DTDFilePrefix was defined,
    // DefaultDTDFilePrefix is used instead. If no DTDFileName
    // was set, type name is used as the file name.
    void SetDTDFilePrefix(const string prefix);
    void SetDTDFileName(const string filename);
    static void SetDefaultDTDFilePrefix(const string def_prefix = "");

    string GetDTDFilePrefix(void);
    string GetDTDFileName(void);
    static string s_GetDefaultDTDFilePrefix(void);

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

	virtual void WriteBytes(const ByteBlock& block,
                            const char* bytes, size_t length);

private:
    void WriteString(const char* str, size_t length);

    void OpenTagStart(void);
    void OpenTagEnd(void);
    void OpenTagEndBack(void);
    void SelfCloseTagEnd(void);
    void CloseTagStart(void);
    void CloseTagEnd(void);

    void OpenTag(const string& name);
    void EolIfEmptyTag(void);
    void CloseTag(const string& name);
    void OpenStackTag(size_t level);
    void CloseStackTag(size_t level);
    void OpenTag(TTypeInfo type);
    void CloseTag(TTypeInfo type);
    void OpenTagIfNamed(TTypeInfo type);
    void CloseTagIfNamed(TTypeInfo type);
    void PrintTagName(size_t level);
    bool WillHaveName(TTypeInfo elementType);
    string GetModuleName(TTypeInfo type);

    enum ETagAction {
        eTagOpen,
        eTagClose,
        eTagSelfClosed
    };
    ETagAction m_LastTagAction;

    // DTD file name and prefix
    string m_DTDFilePrefix;
    string m_DTDFileName;
    static string sm_DefaultDTDFilePrefix;
};

#include <serial/objostrxml.inl>

END_NCBI_SCOPE

#endif
