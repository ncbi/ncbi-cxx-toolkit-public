#ifndef OBJISTRASNB__HPP
#define OBJISTRASNB__HPP

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
#include <serial/objstrasnb.hpp>
#include <stack>


/** @addtogroup ObjStreamSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CObjectOStreamAsnBinary;

class NCBI_XSERIAL_EXPORT CObjectIStreamAsnBinary : public CObjectIStream,
                                                    public CAsnBinaryDefs
{
public:
    CObjectIStreamAsnBinary(EFixNonPrint how = eFNP_Default);
    CObjectIStreamAsnBinary(CNcbiIstream& in,
                            EFixNonPrint how = eFNP_Default);
    CObjectIStreamAsnBinary(CNcbiIstream& in,
                            bool deleteIn,
                            EFixNonPrint how = eFNP_Default);
    CObjectIStreamAsnBinary(CByteSourceReader& reader,
                            EFixNonPrint how = eFNP_Default);

    virtual TEnumValueType ReadEnum(const CEnumeratedTypeValues& values);

    virtual void ReadNull(void);

    bool ReadAnyContent();
    virtual void ReadAnyContentObject(CAnyContentObject& obj);
    virtual void SkipAnyContentObject(void);

    EFixNonPrint FixNonPrint(EFixNonPrint how)
        {
            EFixNonPrint tmp = m_FixMethod;
            m_FixMethod = how;
            return tmp;
        }

protected:
    virtual bool ReadBool(void);
    virtual char ReadChar(void);
    virtual Int4 ReadInt4(void);
    virtual Uint4 ReadUint4(void);
    virtual Int8 ReadInt8(void);
    virtual Uint8 ReadUint8(void);
    virtual double ReadDouble(void);
    virtual void ReadString(string& s,EStringType type = eStringTypeVisible);
    virtual void ReadString(string& s,
                            CPackString& pack_string,
                            EStringType type);
    virtual char* ReadCString(void);
    virtual void ReadStringStore(string& s);

    virtual void SkipBool(void);
    virtual void SkipChar(void);
    virtual void SkipSNumber(void);
    virtual void SkipUNumber(void);
    virtual void SkipFNumber(void);
    virtual void SkipString(EStringType type = eStringTypeVisible);
    virtual void SkipStringStore(void);
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
    virtual void EndClassMember(void);

    virtual TMemberIndex BeginChoiceVariant(const CChoiceTypeInfo* choiceType);
    virtual void EndChoiceVariant(void);

    virtual void BeginBytes(ByteBlock& block);
    virtual size_t ReadBytes(ByteBlock& block, char* dst, size_t length);
    virtual void EndBytes(const ByteBlock& block);

    virtual void BeginChars(CharBlock& block);
    virtual size_t ReadChars(CharBlock& block, char* dst, size_t length);
    virtual void EndChars(const CharBlock& block);

#if HAVE_NCBI_C
    friend class CObjectIStream::AsnIo;
#endif

private:
    virtual EPointerType ReadPointerType(void);
    virtual TObjectIndex ReadObjectPointer(void);
    virtual string ReadOtherPointer(void);
    virtual void ReadOtherPointerEnd(void);

    bool SkipRealValue(void);

    // states:
    // before StartTag (Peek*Tag/ExpectSysTag) tag:
    //     m_CurrentTagLength == 0
    //     stream position on tag start
    // after Peek*Tag/ExpectSysTag tag:
    //     m_CurrentTagLength == beginning of LENGTH field
    //     stream position on tag start
    // after FlushTag (Read*Length/ExpectIndefiniteLength):
    //     m_CurrentTagLength == 0
    //     stream position on tad DATA start
    //     tag limit is pushed on stack and new tag limit is updated
    // after EndOfTag
    //     m_CurrentTagLength == 0
    //     stream position on tag DATA end
    //     tag limit is popped from stack
    // m_CurrentTagLength == beginning of LENGTH field
    //                         -- after any of Peek?Tag or ExpectSysTag
    // 
    size_t m_CurrentTagLength;  // length of tag header (without length field)
    CNcbiStreamoff m_CurrentTagLimit;   // end of current tag data (INT_MAX if unlimited)
    stack<CNcbiStreamoff> m_Limits;
    EFixNonPrint m_FixMethod; // method of fixing non-printable chars

#if CHECK_STREAM_INTEGRITY
    enum ETagState {
        eTagStart,
        eTagParsed,
        eLengthValue,
        eData
    };
    ETagState m_CurrentTagState;
#endif

    // low level interface
private:
    TByte PeekTagByte(size_t index = 0);
    TByte StartTag(TByte first_tag_byte);
    TLongTag PeekTag(TByte first_tag_byte);
    void ExpectTagClassByte(TByte first_tag_byte,
                            TByte expected_class_byte);
    void UnexpectedTagClassByte(TByte first_tag_byte,
                                TByte expected_class_byte);
    TLongTag PeekTag(TByte first_tag_byte,
                     ETagClass tag_class,
                     ETagConstructed tag_constructed);
    string PeekClassTag(void);
    TByte PeekAnyTagFirstByte(void);
    void ExpectSysTagByte(TByte byte);
    void UnexpectedSysTagByte(TByte byte);
    void ExpectSysTag(ETagClass tag_class,
                      ETagConstructed tag_constructed,
                      ETagValue tag_value);
    void ExpectSysTag(ETagValue tag_value);
    TByte FlushTag(void);
    void ExpectIndefiniteLength(void);
    bool PeekIndefiniteLength(void);
    void ExpectContainer(bool random);
public:
    size_t ReadShortLength(void);
private:
    size_t ReadLength(void);
    size_t StartTagData(size_t length);
    void ExpectShortLength(size_t length);
    void ExpectEndOfContent(void);
public:
    void EndOfTag(void);
    Uint1 ReadByte(void);
    Int1 ReadSByte(void);
    void ExpectByte(Uint1 byte);
private:
    void ReadBytes(char* buffer, size_t count);
    void ReadBytes(string& str, size_t count);
    void SkipBytes(size_t count);

    void ReadStringValue(size_t length, string& s, EFixNonPrint fix_type);
    void SkipTagData(void);
    bool HaveMoreElements(void);
    void UnexpectedMember(TLongTag tag);
    void UnexpectedByte(TByte byte);

    friend class CObjectOStreamAsnBinary;
};


/* @} */


#include <serial/objistrasnb.inl>

END_NCBI_SCOPE

#endif  /* OBJISTRASNB__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.47  2005/04/27 17:01:38  vasilche
* Converted namespace CObjectStreamAsnBinaryDefs to class CAsnBinaryDefs.
* Used enums to represent ASN.1 constants whenever possible.
*
* Revision 1.46  2005/04/26 14:13:27  vasilche
* Optimized binary ASN.1 parsing.
*
* Revision 1.45  2004/08/30 18:13:24  gouriano
* use CNcbiStreamoff instead of size_t for stream offset operations
*
* Revision 1.44  2004/03/16 17:48:39  gouriano
* make it possible to skip unknown data members
*
* Revision 1.43  2003/11/26 19:59:38  vasilche
* GetPosition() and GetDataFormat() methods now are implemented
* in parent classes CObjectIStream and CObjectOStream to avoid
* pure virtual method call in destructors.
*
* Revision 1.42  2003/10/15 18:00:32  vasilche
* Fixed integer overflow in asn binary input stream after 2GB.
* Added constructor of CObjectIStreamAsnBinary() from CByteSourceReader.
*
* Revision 1.41  2003/08/19 18:32:37  vasilche
* Optimized reading and writing strings.
* Avoid string reallocation when checking char values.
* Try to reuse old string data when string reference counting is not working.
*
* Revision 1.40  2003/08/13 15:47:02  gouriano
* implemented serialization of AnyContent objects
*
* Revision 1.39  2003/05/22 20:08:41  gouriano
* added UTF8 strings
*
* Revision 1.38  2003/04/15 16:18:23  siyan
* Added doxygen support
*
* Revision 1.37  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.36  2002/10/25 14:49:29  vasilche
* NCBI C Toolkit compatibility code extracted to libxcser library.
* Serial streams flags names were renamed to fXxx.
*
* Names of flags
*
* Revision 1.35  2001/10/17 20:41:19  grichenk
* Added CObjectOStream::CharBlock class
*
* Revision 1.34  2001/06/07 17:12:46  grichenk
* Redesigned checking and substitution of non-printable characters
* in VisibleString
*
* Revision 1.33  2000/12/15 21:28:47  vasilche
* Moved some typedefs/enums from corelib/ncbistd.hpp.
* Added flags to CObjectIStream/CObjectOStream: eFlagAllowNonAsciiChars.
* TByte typedef replaced by Uint1.
*
* Revision 1.32  2000/12/15 15:38:00  vasilche
* Added support of Int8 and long double.
* Enum values now have type Int4 instead of long.
*
* Revision 1.31  2000/10/17 18:45:25  vasilche
* Added possibility to turn off object cross reference detection in
* CObjectIStream and CObjectOStream.
*
* Revision 1.30  2000/09/18 20:00:05  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.29  2000/09/01 13:16:01  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.28  2000/08/15 19:44:40  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.27  2000/07/03 18:42:35  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.26  2000/06/16 16:31:06  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.25  2000/06/07 19:45:43  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.24  2000/06/01 19:06:56  vasilche
* Added parsing of XML data.
*
* Revision 1.23  2000/05/24 20:08:13  vasilche
* Implemented XML dump.
*
* Revision 1.22  2000/05/09 16:38:33  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.21  2000/04/28 16:58:02  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.20  2000/04/13 14:50:17  vasilche
* Added CObjectIStream::Open() and CObjectOStream::Open() for easier use.
*
* Revision 1.19  2000/04/06 16:10:51  vasilche
* Fixed bug with iterators in choices.
* Removed unneeded calls to ReadExternalObject/WriteExternalObject.
* Added output buffering to text ASN.1 data.
*
* Revision 1.18  2000/03/14 14:43:30  vasilche
* Fixed error reporting.
*
* Revision 1.17  2000/03/07 14:05:31  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
*
* Revision 1.16  2000/02/17 20:02:28  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.15  2000/01/10 20:04:08  vasilche
* Fixed duplicate name.
*
* Revision 1.14  2000/01/10 19:46:32  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.13  2000/01/05 19:43:45  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.12  1999/10/18 20:11:16  vasilche
* Enum values now have long type.
* Fixed template generation for enums.
*
* Revision 1.11  1999/09/24 18:19:13  vasilche
* Removed dependency on NCBI toolkit.
*
* Revision 1.10  1999/09/23 18:56:53  vasilche
* Fixed bugs with overloaded methods in objistr*.hpp & objostr*.hpp
*
* Revision 1.9  1999/09/22 20:11:49  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.8  1999/08/16 16:07:43  vasilche
* Added ENUMERATED type.
*
* Revision 1.7  1999/07/26 18:31:30  vasilche
* Implemented skipping of unused values.
* Added more useful error report.
*
* Revision 1.6  1999/07/22 17:33:42  vasilche
* Unified reading/writing of objects in all three formats.
*
* Revision 1.5  1999/07/21 20:02:14  vasilche
* Added embedding of ASN.1 binary output from ToolKit to our binary format.
*
* Revision 1.4  1999/07/21 14:19:57  vasilche
* Added serialization of bool.
*
* Revision 1.3  1999/07/09 16:32:53  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.2  1999/07/07 21:15:03  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.1  1999/07/02 21:31:45  vasilche
* Implemented reading from ASN.1 binary format.
*
* ===========================================================================
*/
