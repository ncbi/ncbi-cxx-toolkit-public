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

class NCBI_XSERIAL_EXPORT CObjectIStreamAsn : public CObjectIStream
{
public:
    CObjectIStreamAsn(EFixNonPrint how = eFNP_Default);
    CObjectIStreamAsn(CNcbiIstream& in,
                      EFixNonPrint how = eFNP_Default);
    CObjectIStreamAsn(CNcbiIstream& in,
                      bool deleteIn,
                      EFixNonPrint how = eFNP_Default);

    virtual string GetPosition(void) const;

    virtual string ReadFileHeader(void);
    virtual TEnumValueType ReadEnum(const CEnumeratedTypeValues& values);

    virtual void ReadNull(void);

    void ReadAnyContent(string& value);
    virtual void ReadAnyContentObject(CAnyContentObject& obj);
    virtual void SkipAnyContentObject(void);

    EFixNonPrint FixNonPrint(EFixNonPrint how)
        {
            EFixNonPrint tmp = m_FixMethod;
            m_FixMethod = how;
            return tmp;
        }

protected:
    TObjectIndex ReadIndex(void);

    // action: read ID into local buffer
    // return: ID pointer and length
    // note: it is not zero ended
    CLightString ScanEndOfId(bool isId);
    CLightString ReadTypeId(char firstChar);
    CLightString ReadMemberId(char firstChar);
    CLightString ReadUCaseId(char firstChar);
    CLightString ReadLCaseId(char firstChar);
    CLightString ReadNumber(void);

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

protected:
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

    virtual void ReadChoice(const CChoiceTypeInfo* choiceType,
                            TObjectPtr choicePtr);
    virtual void SkipChoice(const CChoiceTypeInfo* choiceType);
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

    void SkipObjectData(void);
    void SkipObjectPointer(void);
    void SkipBlock(void);

private:
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
    void UnexpectedMember(const CLightString& id, const CItemsInfo& items);
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
    TMemberIndex GetMemberIndex(const CClassTypeInfo* classType,
                                const CLightString& id);
    TMemberIndex GetMemberIndex(const CClassTypeInfo* classType,
                                const CLightString& id,
                                const TMemberIndex pos);
    TMemberIndex GetChoiceIndex(const CChoiceTypeInfo* choiceType,
                                const CLightString& id);

    bool m_BlockStart;
    EFixNonPrint m_FixMethod; // method of fixing non-printable chars
};


/* @} */


//#include <objistrb.inl>

END_NCBI_SCOPE

#endif  /* OBJISTRB__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.61  2004/04/30 13:28:40  gouriano
* Remove obsolete function declarations
*
* Revision 1.60  2004/03/05 20:28:37  gouriano
* make it possible to skip unknown data fields
*
* Revision 1.59  2003/11/26 19:59:38  vasilche
* GetPosition() and GetDataFormat() methods now are implemented
* in parent classes CObjectIStream and CObjectOStream to avoid
* pure virtual method call in destructors.
*
* Revision 1.58  2003/08/19 18:32:37  vasilche
* Optimized reading and writing strings.
* Avoid string reallocation when checking char values.
* Try to reuse old string data when string reference counting is not working.
*
* Revision 1.57  2003/08/13 15:47:02  gouriano
* implemented serialization of AnyContent objects
*
* Revision 1.56  2003/05/22 20:08:41  gouriano
* added UTF8 strings
*
* Revision 1.55  2003/04/15 16:18:21  siyan
* Added doxygen support
*
* Revision 1.54  2003/03/26 16:13:33  vasilche
* Removed TAB symbols. Some formatting.
*
* Revision 1.53  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.52  2002/10/25 14:49:29  vasilche
* NCBI C Toolkit compatibility code extracted to libxcser library.
* Serial streams flags names were renamed to fXxx.
*
* Names of flags
*
* Revision 1.51  2001/10/17 20:41:19  grichenk
* Added CObjectOStream::CharBlock class
*
* Revision 1.50  2001/06/11 14:34:55  grichenk
* Added support for numeric tags in ASN.1 specifications and data streams.
*
* Revision 1.49  2001/06/07 17:12:46  grichenk
* Redesigned checking and substitution of non-printable characters
* in VisibleString
*
* Revision 1.48  2001/05/17 14:59:47  lavr
* Typos corrected
*
* Revision 1.47  2001/01/05 20:10:35  vasilche
* CByteSource, CIStrBuffer, COStrBuffer, CLightString, CChecksum, CWeakMap
* were moved to util.
*
* Revision 1.46  2000/12/15 21:28:47  vasilche
* Moved some typedefs/enums from corelib/ncbistd.hpp.
* Added flags to CObjectIStream/CObjectOStream: eFlagAllowNonAsciiChars.
* TByte typedef replaced by Uint1.
*
* Revision 1.45  2000/12/15 15:38:00  vasilche
* Added support of Int8 and long double.
* Enum values now have type Int4 instead of long.
*
* Revision 1.44  2000/10/20 15:51:26  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.43  2000/10/17 18:45:24  vasilche
* Added possibility to turn off object cross reference detection in
* CObjectIStream and CObjectOStream.
*
* Revision 1.42  2000/10/04 19:18:54  vasilche
* Fixed processing floating point data.
*
* Revision 1.41  2000/10/03 17:22:34  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.40  2000/09/29 16:18:13  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.39  2000/09/18 20:00:05  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.38  2000/09/01 13:16:00  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.37  2000/08/15 19:44:40  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.36  2000/07/03 18:42:35  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.35  2000/06/16 16:31:06  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.34  2000/06/07 19:45:42  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.33  2000/06/01 19:06:56  vasilche
* Added parsing of XML data.
*
* Revision 1.32  2000/05/24 20:08:13  vasilche
* Implemented XML dump.
*
* Revision 1.31  2000/05/09 16:38:33  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.30  2000/04/28 16:58:02  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.29  2000/04/13 14:50:17  vasilche
* Added CObjectIStream::Open() and CObjectOStream::Open() for easier use.
*
* Revision 1.28  2000/04/06 16:10:50  vasilche
* Fixed bug with iterators in choices.
* Removed unneeded calls to ReadExternalObject/WriteExternalObject.
* Added output buffering to text ASN.1 data.
*
* Revision 1.27  2000/03/14 14:43:30  vasilche
* Fixed error reporting.
*
* Revision 1.26  2000/03/07 14:05:30  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
*
* Revision 1.25  2000/02/17 20:02:28  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.24  2000/02/11 17:10:19  vasilche
* Optimized text parsing.
*
* Revision 1.23  2000/02/01 21:44:35  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Added buffering to CObjectIStreamAsn.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
*
* Revision 1.22  2000/01/10 20:12:37  vasilche
* Fixed duplicate argument names.
* Fixed conflict between template and variable name.
*
* Revision 1.21  2000/01/10 19:46:31  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.20  2000/01/05 19:43:44  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.19  1999/09/24 18:19:13  vasilche
* Removed dependency on NCBI toolkit.
*
* Revision 1.18  1999/09/23 18:56:52  vasilche
* Fixed bugs with overloaded methods in objistr*.hpp & objostr*.hpp
*
* Revision 1.17  1999/09/22 20:11:49  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.16  1999/08/17 15:13:03  vasilche
* Comments are allowed in ASN.1 text files.
* String values now parsed in accordance with ASN.1 specification.
*
* Revision 1.15  1999/08/13 15:53:43  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.14  1999/07/26 18:31:29  vasilche
* Implemented skipping of unused values.
* Added more useful error report.
*
* Revision 1.13  1999/07/22 17:33:41  vasilche
* Unified reading/writing of objects in all three formats.
*
* Revision 1.12  1999/07/21 14:19:57  vasilche
* Added serialization of bool.
*
* Revision 1.11  1999/07/19 15:50:16  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.10  1999/07/09 16:32:53  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.9  1999/07/07 21:15:03  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.8  1999/07/07 19:58:45  vasilche
* Reduced amount of data allocated on heap
* Cleaned ASN.1 structures info
*
* Revision 1.7  1999/07/07 18:18:32  vasilche
* Fixed some bugs found by MS VC++
*
* Revision 1.6  1999/07/02 21:31:44  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.5  1999/06/30 16:04:28  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.4  1999/06/18 16:26:49  vasilche
* Fixed bug with unget() in MSVS
*
* Revision 1.3  1999/06/17 21:08:49  vasilche
* Fixed bug with unget()
*
* Revision 1.2  1999/06/17 20:42:01  vasilche
* Fixed storing/loading of pointers.
*
* Revision 1.1  1999/06/16 20:35:22  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.4  1999/06/15 16:20:02  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.3  1999/06/10 21:06:38  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.2  1999/06/04 20:51:32  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:25  vasilche
* Commit just in case.
*
* ===========================================================================
*/
