#ifndef OBJISTRXML__HPP
#define OBJISTRXML__HPP

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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*/

#include <corelib/ncbistd.hpp>
#include <serial/objistr.hpp>
#include <util/lightstr.hpp>


/** @addtogroup ObjStreamSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CItemsInfo;

class NCBI_XSERIAL_EXPORT CObjectIStreamXml : public CObjectIStream
{
public:
    CObjectIStreamXml(void);
    ~CObjectIStreamXml(void);

    virtual string GetPosition(void) const;

    virtual string ReadFileHeader(void);
    virtual string PeekNextTypeName(void);

    enum EEncoding {
        eEncoding_Unknown,
        eEncoding_UTF8,
        eEncoding_ISO8859_1,
        eEncoding_Windows_1252
    };
    EEncoding GetEncoding(void) const;

    void SetEnforcedStdXml(bool set = true);
    bool GetEnforcedStdXml(void)     {return m_EnforcedStdXml;}

protected:
    EPointerType ReadPointerType(void);
    TObjectIndex ReadObjectPointer(void);
    string ReadOtherPointer(void);

    virtual bool ReadBool(void);
    virtual char ReadChar(void);
    virtual Int4 ReadInt4(void);
    virtual Uint4 ReadUint4(void);
    virtual Int8 ReadInt8(void);
    virtual Uint8 ReadUint8(void);
    virtual double ReadDouble(void);
    virtual void ReadNull(void);
    virtual void ReadString(string& s,EStringType type = eStringTypeVisible);
    virtual char* ReadCString(void);
    TEnumValueType ReadEnum(const CEnumeratedTypeValues& values);

    void ReadAnyContentTo(const string& ns_prefix, string& value,
                          const CLightString& tagName);
    virtual void ReadAnyContentObject(CAnyContentObject& obj);
    virtual void SkipAnyContentObject(void);

    virtual void SkipBool(void);
    virtual void SkipChar(void);
    virtual void SkipSNumber(void);
    virtual void SkipUNumber(void);
    virtual void SkipFNumber(void);
    virtual void SkipString(EStringType type = eStringTypeVisible);
    virtual void SkipNull(void);
    virtual void SkipByteBlock(void);

    CLightString SkipTagName(CLightString tag, const char* s, size_t length);
    CLightString SkipTagName(CLightString tag, const char* s);
    CLightString SkipTagName(CLightString tag, const string& s);
    CLightString SkipStackTagName(CLightString tag, size_t level);
    CLightString SkipStackTagName(CLightString tag, size_t level, char c);

    bool HasAttlist(void);
    bool NextIsTag(void);
    bool NextTagIsClosing(void);
    bool ThisTagIsSelfClosed(void);
    void OpenTag(const string& e);
    void CloseTag(const string& e);
    void OpenStackTag(size_t level);
    void CloseStackTag(size_t level);
    void OpenTag(TTypeInfo type);
    void CloseTag(TTypeInfo type);
    void OpenTagIfNamed(TTypeInfo type);
    void CloseTagIfNamed(TTypeInfo type);
    bool WillHaveName(TTypeInfo elementType);

    bool HasAnyContent(const CClassTypeInfoBase* classType);
    bool HasMoreElements(TTypeInfo elementType);
    TMemberIndex FindDeep(TTypeInfo type, const CLightString& name) const;
#ifdef VIRTUAL_MID_LEVEL_IO
    virtual void ReadNamedType(TTypeInfo namedTypeInfo,
                               TTypeInfo typeInfo,
                               TObjectPtr object);

    virtual void ReadContainer(const CContainerTypeInfo* containerType,
                               TObjectPtr containerPtr);
    virtual void SkipContainer(const CContainerTypeInfo* containerType);


    virtual void ReadChoice(const CChoiceTypeInfo* choiceType,
                            TObjectPtr choicePtr);
    virtual void SkipChoice(const CChoiceTypeInfo* choiceType);
#endif
    void ReadContainerContents(const CContainerTypeInfo* containerType,
                               TObjectPtr containerPtr);
    void SkipContainerContents(const CContainerTypeInfo* containerType);
    void ReadChoiceContents(const CChoiceTypeInfo* choiceType,
                            TObjectPtr choicePtr);
    void SkipChoiceContents(const CChoiceTypeInfo* choiceType);

    // low level I/O
    virtual void BeginNamedType(TTypeInfo namedTypeInfo);
    virtual void EndNamedType(void);

    virtual void BeginContainer(const CContainerTypeInfo* containerType);
    virtual void EndContainer(void);
    virtual bool BeginContainerElement(TTypeInfo elementType);
    virtual void EndContainerElement(void);
    void BeginArrayElement(TTypeInfo elementType);
    void EndArrayElement(void);

    void CheckStdXml(const CClassTypeInfoBase* classType);
    ETypeFamily GetRealTypeFamily(TTypeInfo typeInfo);
    ETypeFamily GetContainerElementTypeFamily(TTypeInfo typeInfo);

    virtual void BeginClass(const CClassTypeInfo* classInfo);
    virtual void EndClass(void);
    virtual TMemberIndex BeginClassMember(const CClassTypeInfo* classType);
    virtual TMemberIndex BeginClassMember(const CClassTypeInfo* classType,
                                          TMemberIndex pos);
    void EndClassMember(void);
    virtual void UndoClassMember(void);

    virtual void BeginChoice(const CChoiceTypeInfo* choiceType);
    virtual void EndChoice(void);
    virtual TMemberIndex BeginChoiceVariant(const CChoiceTypeInfo* choiceType);
    virtual void EndChoiceVariant(void);

    void BeginBytes(ByteBlock& );
    int GetHexChar(void);
    size_t ReadBytes(ByteBlock& block, char* dst, size_t length);

    void BeginChars(CharBlock& );
    size_t ReadChars(CharBlock& block, char* dst, size_t length);

private:
    bool OutsideTag(void) const;
    bool InsideTag(void) const;
    bool InsideOpeningTag(void) const;
    bool InsideClosingTag(void) const;
    bool SelfClosedTag(void) const;

    void Found_lt(void);
    void Back_lt(void);
    void Found_lt_slash(void);
    void Found_gt(void);
    void Found_slash_gt(void);

    void EndSelfClosedTag(void);

    void EndTag(void);
    void EndOpeningTag(void);
    bool EndOpeningTagSelfClosed(void); // true if '/>' found, false if '>'
    void EndClosingTag(void);
    char BeginOpeningTag(void);
    char BeginClosingTag(void);
    void BeginData(void);

    int ReadEscapedChar(char endingChar, bool* encoded=0);
    void ReadTagData(string& s);

    CLightString ReadName(char c);
    CLightString RejectedName(void);
    CLightString ReadAttributeName(void);
    void ReadAttributeValue(string& value, bool skipClosing=false);
    char ReadUndefinedAttributes(void);

    void SkipAttributeValue(char c);
    void SkipQDecl(void);

    char SkipWS(void);
    char SkipWSAndComments(void);

    void UnexpectedMember(const CLightString& id, const CItemsInfo& items);
    bool x_IsStdXml(void) {return m_StdXml || m_EnforcedStdXml;}
    void x_EndTypeNamespace(void);

    enum ETagState {
        eTagOutside,
        eTagInsideOpening,
        eTagInsideClosing,
        eTagSelfClosed
    };
    ETagState m_TagState;
    string m_LastTag;
    string m_RejectedTag;
    bool m_Attlist;
    bool m_StdXml;
    bool m_EnforcedStdXml;
    string m_LastPrimitive;
    EEncoding m_Encoding;
    string m_CurrNsPrefix;
    map<string,string> m_NsPrefixToName;
    map<string,string> m_NsNameToPrefix;
};


/* @} */


#include <serial/objistrxml.inl>

END_NCBI_SCOPE

#endif  /* OBJISTRXML__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.36  2004/06/08 20:23:37  gouriano
* Moved several functions out of VIRTUAL_MID_LEVEL_IO condition:
* there is no need for them to be there
*
* Revision 1.35  2004/04/28 19:24:53  gouriano
* Corrected reading of containers
*
* Revision 1.34  2004/01/30 20:31:05  gouriano
* Corrected reading white spaces
*
* Revision 1.33  2004/01/22 20:47:55  gouriano
* Corrected reading of AnyContent object attributes
*
* Revision 1.32  2004/01/08 17:38:23  gouriano
* Added encoding Windows-1252
*
* Revision 1.31  2003/11/26 19:59:38  vasilche
* GetPosition() and GetDataFormat() methods now are implemented
* in parent classes CObjectIStream and CObjectOStream to avoid
* pure virtual method call in destructors.
*
* Revision 1.30  2003/09/16 14:49:15  gouriano
* Enhanced AnyContent objects to support XML namespaces and attribute info items.
*
* Revision 1.29  2003/08/25 15:58:32  gouriano
* added possibility to use namespaces in XML i/o streams
*
* Revision 1.28  2003/08/13 15:47:02  gouriano
* implemented serialization of AnyContent objects
*
* Revision 1.27  2003/06/30 15:40:18  gouriano
* added encoding (utf-8 or iso-8859-1)
*
* Revision 1.26  2003/06/24 20:54:13  gouriano
* corrected code generation and serialization of non-empty unnamed containers (XML)
*
* Revision 1.25  2003/05/22 20:08:41  gouriano
* added UTF8 strings
*
* Revision 1.24  2003/04/15 16:18:25  siyan
* Added doxygen support
*
* Revision 1.23  2003/02/05 17:08:51  gouriano
* added possibility to read/write objects generated from an ASN.1 spec as "standard" XML - without scope prefixes
*
* Revision 1.22  2003/01/21 19:28:44  gouriano
* corrected reading containers of primitive types
*
* Revision 1.21  2002/12/26 19:33:06  gouriano
* changed XML I/O streams to properly handle object copying
*
* Revision 1.20  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.19  2002/12/12 21:10:26  gouriano
* implemented handling of complex XML containers
*
* Revision 1.18  2002/11/26 22:01:38  gouriano
* added HasMoreElements method
*
* Revision 1.17  2002/11/20 21:21:58  gouriano
* corrected processing of unnamed sequences as choice variants
*
* Revision 1.16  2002/11/14 20:51:27  gouriano
* added support of attribute lists
*
* Revision 1.15  2002/10/15 13:45:15  gouriano
* added "UndoClassMember" function
*
* Revision 1.14  2001/10/17 20:41:19  grichenk
* Added CObjectOStream::CharBlock class
*
* Revision 1.13  2001/01/05 20:10:36  vasilche
* CByteSource, CIStrBuffer, COStrBuffer, CLightString, CChecksum, CWeakMap
* were moved to util.
*
* Revision 1.12  2000/12/15 21:28:47  vasilche
* Moved some typedefs/enums from corelib/ncbistd.hpp.
* Added flags to CObjectIStream/CObjectOStream: eFlagAllowNonAsciiChars.
* TByte typedef replaced by Uint1.
*
* Revision 1.11  2000/12/15 15:38:00  vasilche
* Added support of Int8 and long double.
* Enum values now have type Int4 instead of long.
*
* Revision 1.10  2000/11/07 17:25:12  vasilche
* Fixed encoding of XML:
*     removed unnecessary apostrophes in OCTET STRING
*     removed unnecessary content in NULL
* Added module names to CTypeInfo and CEnumeratedTypeValues
*
* Revision 1.9  2000/10/03 17:22:35  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.8  2000/09/29 16:18:14  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.7  2000/09/18 20:00:06  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.6  2000/09/01 13:16:01  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.5  2000/08/15 19:44:40  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.4  2000/07/03 18:42:36  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.3  2000/06/16 16:31:06  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.2  2000/06/07 19:45:43  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.1  2000/06/01 19:06:57  vasilche
* Added parsing of XML data.
*
* ===========================================================================
*/
