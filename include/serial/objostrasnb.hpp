#ifndef OBJOSTRASNB__HPP
#define OBJOSTRASNB__HPP

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
#include <serial/objostr.hpp>
#include <serial/objstrasnb.hpp>
#include <stack>


/** @addtogroup ObjStreamSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CObjectIStreamAsnBinary;

class NCBI_XSERIAL_EXPORT CObjectOStreamAsnBinary : public CObjectOStream,
                                                    public CAsnBinaryDefs
{
public:
    CObjectOStreamAsnBinary(CNcbiOstream& out,
                            EFixNonPrint how = eFNP_Default);
    CObjectOStreamAsnBinary(CNcbiOstream& out,
                            bool deleteOut,
                            EFixNonPrint how = eFNP_Default);
    virtual ~CObjectOStreamAsnBinary(void);

    virtual void WriteEnum(const CEnumeratedTypeValues& values, 
                           TEnumValueType value);
    virtual void CopyEnum(const CEnumeratedTypeValues& values,
                          CObjectIStream& in);

    void WriteNull(void);
    virtual void WriteAnyContentObject(const CAnyContentObject& obj);
    virtual void CopyAnyContentObject(CObjectIStream& in);

    void WriteLongTag(ETagClass tag_class,
                      ETagConstructed tag_constructed,
                      TLongTag tag_value);
    void WriteClassTag(TTypeInfo typeInfo);
    void WriteLongLength(size_t length);
    EFixNonPrint FixNonPrint(EFixNonPrint how)
        {
            EFixNonPrint tmp = m_FixMethod;
            m_FixMethod = how;
            return tmp;
        }

    void WriteByte(Uint1 byte);
protected:
    void WriteBytes(const char* bytes, size_t size);
    void WriteShortTag(ETagClass tag_class, 
                       ETagConstructed tag_constructed,
                       ETagValue tag_value);
    void WriteSysTag(ETagValue tag);
    void WriteTag(ETagClass tag_class,
                  ETagConstructed tag_constructed,
                  TLongTag tag_value);
    void WriteIndefiniteLength(void);
    void WriteShortLength(size_t length);
    void WriteLength(size_t length);
    void WriteEndOfContent(void);

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
    void CopyStringValue(CObjectIStreamAsnBinary& in,
                         bool checkVisible = false);

    virtual void WriteNullPointer(void);
    virtual void WriteObjectReference(TObjectIndex index);
    virtual void WriteOtherBegin(TTypeInfo typeInfo);
    virtual void WriteOtherEnd(TTypeInfo typeInfo);
    virtual void WriteOther(TConstObjectPtr object, TTypeInfo typeInfo);

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

    virtual void WriteChoice(const CChoiceTypeInfo* choiceType,
                             TConstObjectPtr choicePtr);
    // COPY
    virtual void CopyContainer(const CContainerTypeInfo* containerType,
                               CObjectStreamCopier& copier);
    virtual void CopyClassRandom(const CClassTypeInfo* objectType,
                                 CObjectStreamCopier& copier);
    virtual void CopyClassSequential(const CClassTypeInfo* objectType,
                                     CObjectStreamCopier& copier);
    virtual void CopyChoice(const CChoiceTypeInfo* choiceType,
                            CObjectStreamCopier& copier);
#endif
    virtual void BeginContainer(const CContainerTypeInfo* containerType);
    virtual void EndContainer(void);

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

    virtual void BeginChars(const CharBlock& block);
    virtual void WriteChars(const CharBlock& block,
                            const char* chars, size_t length);

#if HAVE_NCBI_C
    friend class CObjectOStream::AsnIo;
#endif

private:
    void WriteNumberValue(Int4 data);
    void WriteNumberValue(Int8 data);
    void WriteNumberValue(Uint4 data);
    void WriteNumberValue(Uint8 data);

#if CHECK_STREAM_INTEGRITY
    CNcbiStreamoff m_CurrentPosition;
    enum ETagState {
        eTagStart,
        eTagValue,
        eLengthStart,
        eLengthValueFirst,
        eLengthValue,
        eData
    };
    ETagState m_CurrentTagState;
    CNcbiStreamoff m_CurrentTagPosition;
    Uint1 m_CurrentTagCode;
    size_t m_CurrentTagLengthSize;
    size_t m_CurrentTagLength;
    CNcbiStreamoff m_CurrentTagLimit;
    stack<CNcbiStreamoff> m_Limits;

    void StartTag(TByte code);
    void EndTag(void);
    void SetTagLength(size_t length);
#endif
    EFixNonPrint m_FixMethod; // method of fixing non-printable chars
};


/* @} */


#include <serial/objostrasnb.inl>

END_NCBI_SCOPE

#endif  /* OBJOSTRASNB__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.46  2005/04/27 17:01:38  vasilche
* Converted namespace CObjectStreamAsnBinaryDefs to class CAsnBinaryDefs.
* Used enums to represent ASN.1 constants whenever possible.
*
* Revision 1.45  2004/08/30 18:13:24  gouriano
* use CNcbiStreamoff instead of size_t for stream offset operations
*
* Revision 1.44  2004/06/08 20:25:42  gouriano
* Made functions, that are not visible from the outside, protected
*
* Revision 1.43  2003/11/26 19:59:39  vasilche
* GetPosition() and GetDataFormat() methods now are implemented
* in parent classes CObjectIStream and CObjectOStream to avoid
* pure virtual method call in destructors.
*
* Revision 1.42  2003/08/13 15:47:02  gouriano
* implemented serialization of AnyContent objects
*
* Revision 1.41  2003/05/22 20:08:42  gouriano
* added UTF8 strings
*
* Revision 1.40  2003/04/15 16:18:31  siyan
* Added doxygen support
*
* Revision 1.39  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.38  2002/10/25 14:49:29  vasilche
* NCBI C Toolkit compatibility code extracted to libxcser library.
* Serial streams flags names were renamed to fXxx.
*
* Names of flags
*
* Revision 1.37  2001/10/17 20:41:20  grichenk
* Added CObjectOStream::CharBlock class
*
* Revision 1.36  2001/06/07 20:36:31  grichenk
* Fixed location of m_FixMethod member
*
* Revision 1.35  2001/06/07 17:12:46  grichenk
* Redesigned checking and substitution of non-printable characters
* in VisibleString
*
* Revision 1.34  2001/05/17 14:59:47  lavr
* Typos corrected
*
* Revision 1.33  2000/12/15 21:28:48  vasilche
* Moved some typedefs/enums from corelib/ncbistd.hpp.
* Added flags to CObjectIStream/CObjectOStream: eFlagAllowNonAsciiChars.
* TByte typedef replaced by Uint1.
*
* Revision 1.32  2000/12/15 15:38:01  vasilche
* Added support of Int8 and long double.
* Enum values now have type Int4 instead of long.
*
* Revision 1.31  2000/10/20 15:51:27  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.30  2000/10/04 19:18:54  vasilche
* Fixed processing floating point data.
*
* Revision 1.29  2000/09/29 16:18:14  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.28  2000/09/18 20:00:07  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.27  2000/09/01 13:16:02  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.26  2000/08/15 19:44:41  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.25  2000/07/03 18:42:36  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.24  2000/06/16 16:31:07  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.23  2000/06/07 19:45:43  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.22  2000/06/01 19:06:57  vasilche
* Added parsing of XML data.
*
* Revision 1.21  2000/05/24 20:08:14  vasilche
* Implemented XML dump.
*
* Revision 1.20  2000/04/28 16:58:03  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.19  2000/04/13 14:50:18  vasilche
* Added CObjectIStream::Open() and CObjectOStream::Open() for easier use.
*
* Revision 1.18  2000/04/06 16:10:51  vasilche
* Fixed bug with iterators in choices.
* Removed unneeded calls to ReadExternalObject/WriteExternalObject.
* Added output buffering to text ASN.1 data.
*
* Revision 1.17  2000/02/17 20:02:28  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.16  2000/01/10 20:12:37  vasilche
* Fixed duplicate argument names.
* Fixed conflict between template and variable name.
*
* Revision 1.15  2000/01/10 19:46:32  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.14  1999/09/24 18:19:14  vasilche
* Removed dependency on NCBI toolkit.
*
* Revision 1.13  1999/09/23 18:56:54  vasilche
* Fixed bugs with overloaded methods in objistr*.hpp & objostr*.hpp
*
* Revision 1.12  1999/09/22 20:11:50  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.11  1999/08/16 16:07:44  vasilche
* Added ENUMERATED type.
*
* Revision 1.10  1999/07/26 18:31:31  vasilche
* Implemented skipping of unused values.
* Added more useful error report.
*
* Revision 1.9  1999/07/22 17:33:45  vasilche
* Unified reading/writing of objects in all three formats.
*
* Revision 1.8  1999/07/21 14:20:00  vasilche
* Added serialization of bool.
*
* Revision 1.7  1999/07/19 15:50:19  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.6  1999/07/09 16:32:54  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.5  1999/07/07 21:15:03  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.4  1999/07/02 21:31:47  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.3  1999/07/01 17:55:21  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.2  1999/06/30 16:04:33  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.1  1999/06/24 14:44:42  vasilche
* Added binary ASN.1 output.
*
*
* ===========================================================================
*/
