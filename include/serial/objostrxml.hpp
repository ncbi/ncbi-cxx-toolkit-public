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
*/

#include <corelib/ncbistd.hpp>
#include <serial/objostr.hpp>
#include <stack>


/** @addtogroup ObjStreamSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class NCBI_XSERIAL_EXPORT CObjectOStreamXml : public CObjectOStream
{
public:
    CObjectOStreamXml(CNcbiOstream& out, bool deleteOut);
    virtual ~CObjectOStreamXml(void);

    virtual string GetPosition(void) const;

    enum EEncoding {
        eEncoding_Unknown,
        eEncoding_UTF8,
        eEncoding_ISO8859_1,
        eEncoding_Windows_1252
    };
    void SetEncoding(EEncoding enc);
    EEncoding GetEncoding(void) const;

    void SetReferenceSchema(bool use_schema = true);
    bool GetReferenceSchema(void) const;
    void SetReferenceDTD(bool use_dtd = true);
    bool GetReferenceDTD(void) const;

    void SetUseSchemaLocation(bool use_loc = true);
    bool GetUseSchemaLocation(void) const;

    static void   SetDefaultSchemaNamespace(const string& schema_ns);
    static string GetDefaultSchemaNamespace(void);

    virtual void WriteFileHeader(TTypeInfo type);
    virtual void EndOfWrite(void);

    // DTD file name and prefix. The final module name is built as
    // DTDFilePrefix + DTDFileName.

    // If "DTDFilePrefix" has never been set for this stream, then
    // the global "DefaultDTDFilePrefix" will be used.
    // If it has been set to any value (including empty string), then
    // that value will be used.
    void   SetDTDFilePrefix(const string& prefix);
    string GetDTDFilePrefix(void) const;

    // Default (global) DTD file prefix.
    static void   SetDefaultDTDFilePrefix(const string& prefix);
    static string GetDefaultDTDFilePrefix(void);

    // If "DTDFileName" is not set or set to empty string for this stream,
    // then type name will be used as the file name.
    void   SetDTDFileName(const string& filename);
    string GetDTDFileName(void) const;

    // Enable/disable DTD public identifier.
    // If disabled (it is ENABLED by default), only system identifier
    // will be written in the output XML stream
    void EnableDTDPublicId(void);
    void DisableDTDPublicId(void);
    // DTD public identifier. If set to non-empty string, the stream will
    // write this in the output XML file. Otherwise the "default" public id
    // will be generated
    void SetDTDPublicId(const string& publicId);
    string GetDTDPublicId(void) const;

    enum ERealValueFormat {
        eRealFixedFormat,
        eRealScientificFormat
    };
    ERealValueFormat GetRealValueFormat(void) const;
    void SetRealValueFormat(ERealValueFormat fmt);
    void SetEnforcedStdXml(bool set = true);
    bool GetEnforcedStdXml(void)     {return m_EnforcedStdXml;}

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
    virtual void CopyString(CObjectIStream& in);
    virtual void CopyStringStore(CObjectIStream& in);

    virtual void WriteNullPointer(void);
    virtual void WriteObjectReference(TObjectIndex index);
    virtual void WriteOtherBegin(TTypeInfo typeInfo);
    virtual void WriteOtherEnd(TTypeInfo typeInfo);
    virtual void WriteOther(TConstObjectPtr object, TTypeInfo typeInfo);
    void WriteId(const string& str);

    void WriteNull(void);
    virtual void WriteAnyContentObject(const CAnyContentObject& obj);
    virtual void CopyAnyContentObject(CObjectIStream& in);

    void WriteEscapedChar(char c);

    virtual void WriteEnum(const CEnumeratedTypeValues& values,
                           TEnumValueType value);
    virtual void CopyEnum(const CEnumeratedTypeValues& values,
                          CObjectIStream& in);
    void WriteEnum(const CEnumeratedTypeValues& values,
                   TEnumValueType value, const string& valueName);

#ifdef VIRTUAL_MID_LEVEL_IO
    virtual void WriteNamedType(TTypeInfo namedTypeInfo,
                                TTypeInfo typeInfo, TConstObjectPtr object);
    virtual void CopyNamedType(TTypeInfo namedTypeInfo,
                               TTypeInfo typeInfo,
                               CObjectStreamCopier& copier);

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
    void WriteContainerContents(const CContainerTypeInfo* containerType,
                                TConstObjectPtr containerPtr);
    void WriteChoiceContents(const CChoiceTypeInfo* choiceType,
                             TConstObjectPtr choicePtr);
    // low level I/O
    virtual void BeginNamedType(TTypeInfo namedTypeInfo);
    virtual void EndNamedType(void);

    virtual void BeginContainer(const CContainerTypeInfo* containerType);
    virtual void EndContainer(void);
    virtual void BeginContainerElement(TTypeInfo elementType);
    virtual void EndContainerElement(void);
    void BeginArrayElement(TTypeInfo elementType);
    void EndArrayElement(void);

    void CheckStdXml(const CClassTypeInfoBase* classType);
    ETypeFamily GetRealTypeFamily(TTypeInfo typeInfo);
    ETypeFamily GetContainerElementTypeFamily(TTypeInfo typeInfo);

    virtual void BeginClass(const CClassTypeInfo* classInfo);
    virtual void EndClass(void);
    virtual void BeginClassMember(const CMemberId& id);
    void BeginClassMember(TTypeInfo memberType,
                          const CMemberId& id);
    virtual void EndClassMember(void);

    virtual void BeginChoice(const CChoiceTypeInfo* choiceType);
    virtual void EndChoice(void);
    virtual void BeginChoiceVariant(const CChoiceTypeInfo* choiceType,
                                    const CMemberId& id);
    virtual void EndChoiceVariant(void);

    virtual void WriteBytes(const ByteBlock& block,
                            const char* bytes, size_t length);
    virtual void WriteChars(const CharBlock& block,
                            const char* chars, size_t length);

    // Write current separator to the stream
    virtual void WriteSeparator(void);

#if defined(NCBI_SERIAL_IO_TRACE)
    void TraceTag(const string& name);
#endif

private:
    void WriteString(const char* str, size_t length);

    void OpenTagStart(void);
    void OpenTagEnd(void);
    void OpenTagEndBack(void);
    void SelfCloseTagEnd(void);
    void CloseTagStart(void);
    void CloseTagEnd(void);

    void WriteTag(const string& name);
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
    bool x_IsStdXml(void) {return m_StdXml || m_EnforcedStdXml;}

    void x_WriteClassNamespace(TTypeInfo type);
    bool x_ProcessTypeNamespace(TTypeInfo type);
    void x_EndTypeNamespace(void);
    bool x_BeginNamespace(const string& ns_name, const string& ns_prefix);
    void x_EndNamespace(const string& ns_name);

    enum ETagAction {
        eTagOpen,
        eTagClose,
        eTagSelfClosed,
        eAttlistTag
    };
    ETagAction m_LastTagAction;
    bool m_EndTag;

    // DTD file name and prefix
    bool   m_UseDefaultDTDFilePrefix;
    string m_DTDFilePrefix;
    string m_DTDFileName;
    bool   m_UsePublicId;
    string m_PublicId;
    static string sm_DefaultDTDFilePrefix;
    bool m_Attlist;
    bool m_StdXml;
    bool m_EnforcedStdXml;
    ERealValueFormat m_RealFmt;
    EEncoding m_Encoding;
    bool m_UseSchemaRef;
    bool m_UseSchemaLoc;
    bool m_UseDTDRef;
    static string sm_DefaultSchemaNamespace;
    string m_CurrNsPrefix;
    map<string,string> m_NsNameToPrefix;
    map<string,string> m_NsPrefixToName;
    stack<string> m_NsPrefixes;
    bool m_SkipIndent;
};


/* @} */


#include <serial/objostrxml.inl>

END_NCBI_SCOPE

#endif



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.40  2005/02/09 14:23:53  gouriano
* Implemented seriaization of mixed content elements
*
* Revision 1.39  2004/06/08 20:23:37  gouriano
* Moved several functions out of VIRTUAL_MID_LEVEL_IO condition:
* there is no need for them to be there
*
* Revision 1.38  2004/01/08 17:42:34  gouriano
* Added encoding Windows-1252.
* Made it possible to omit document type declaration
*
* Revision 1.37  2003/11/26 19:59:39  vasilche
* GetPosition() and GetDataFormat() methods now are implemented
* in parent classes CObjectIStream and CObjectOStream to avoid
* pure virtual method call in destructors.
*
* Revision 1.36  2003/09/16 14:49:15  gouriano
* Enhanced AnyContent objects to support XML namespaces and attribute info items.
*
* Revision 1.35  2003/08/25 15:58:32  gouriano
* added possibility to use namespaces in XML i/o streams
*
* Revision 1.34  2003/08/13 15:47:02  gouriano
* implemented serialization of AnyContent objects
*
* Revision 1.33  2003/07/02 13:01:00  gouriano
* added ability to read/write XML files with reference to schema
*
* Revision 1.32  2003/06/30 15:40:18  gouriano
* added encoding (utf-8 or iso-8859-1)
*
* Revision 1.31  2003/05/22 20:08:42  gouriano
* added UTF8 strings
*
* Revision 1.30  2003/04/15 16:18:32  siyan
* Added doxygen support
*
* Revision 1.29  2003/03/26 16:13:33  vasilche
* Removed TAB symbols. Some formatting.
*
* Revision 1.28  2003/02/05 17:08:51  gouriano
* added possibility to read/write objects generated from an ASN.1 spec as "standard" XML - without scope prefixes
*
* Revision 1.27  2003/01/22 20:51:10  gouriano
* more control on how a float value is to be written
*
* Revision 1.26  2003/01/10 16:53:36  gouriano
* fixed a bug with optional class members
*
* Revision 1.25  2002/12/26 19:33:06  gouriano
* changed XML I/O streams to properly handle object copying
*
* Revision 1.24  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.23  2002/12/12 21:10:26  gouriano
* implemented handling of complex XML containers
*
* Revision 1.22  2002/11/14 20:52:55  gouriano
* added support of attribute lists
*
* Revision 1.21  2002/10/18 14:25:52  gouriano
* added possibility to enable/disable/set public identifier
*
* Revision 1.20  2002/03/07 22:02:00  grichenk
* Added "Separator" modifier for CObjectOStream
*
* Revision 1.19  2001/11/09 19:07:22  grichenk
* Fixed DTDFilePrefix functions
*
* Revision 1.18  2001/10/17 20:41:20  grichenk
* Added CObjectOStream::CharBlock class
*
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
