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
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.74  2005/04/27 17:02:17  vasilche
* Converted namespace CObjectStreamAsnBinaryDefs to class CAsnBinaryDefs.
* Used enums to represent ASN.1 constants whenever possible.
* Use vector<Uint1> instead of inefficient vector<bool>.
*
* Revision 1.73  2005/04/26 14:55:48  vasilche
* Use named constant for indefinite length byte.
*
* Revision 1.72  2005/04/26 14:13:27  vasilche
* Optimized binary ASN.1 parsing.
*
* Revision 1.71  2004/08/30 18:19:39  gouriano
* Use CNcbiStreamoff instead of size_t for stream offset operations
*
* Revision 1.70  2004/05/17 21:03:03  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.69  2004/03/23 15:39:23  gouriano
* Added setup options for skipping unknown data members
*
* Revision 1.68  2004/03/16 17:48:21  gouriano
* make it possible to skip unknown data members
*
* Revision 1.67  2003/11/26 19:59:40  vasilche
* GetPosition() and GetDataFormat() methods now are implemented
* in parent classes CObjectIStream and CObjectOStream to avoid
* pure virtual method call in destructors.
*
* Revision 1.66  2003/10/15 18:00:32  vasilche
* Fixed integer overflow in asn binary input stream after 2GB.
* Added constructor of CObjectIStreamAsnBinary() from CByteSourceReader.
*
* Revision 1.65  2003/08/25 15:59:09  gouriano
* added possibility to use namespaces in XML i/o streams
*
* Revision 1.64  2003/08/19 18:32:38  vasilche
* Optimized reading and writing strings.
* Avoid string reallocation when checking char values.
* Try to reuse old string data when string reference counting is not working.
*
* Revision 1.63  2003/08/14 20:03:58  vasilche
* Avoid memory reallocation when reading over preallocated object.
* Simplified CContainerTypeInfo iterators interface.
*
* Revision 1.62  2003/08/13 15:47:45  gouriano
* implemented serialization of AnyContent objects
*
* Revision 1.61  2003/05/22 20:10:02  gouriano
* added UTF8 strings
*
* Revision 1.60  2003/05/16 18:02:18  gouriano
* revised exception error messages
*
* Revision 1.59  2003/03/26 16:14:23  vasilche
* Removed TAB symbols. Some formatting.
*
* Revision 1.58  2002/10/25 14:49:27  vasilche
* NCBI C Toolkit compatibility code extracted to libxcser library.
* Serial streams flags names were renamed to fXxx.
*
* Names of flags
*
* Revision 1.57  2002/01/23 21:58:31  grichenk
* Fixed minor bug in log tags reader
*
* Revision 1.56  2002/01/17 20:52:35  grichenk
* Fixed another bug in long binary tags processing
*
* Revision 1.55  2002/01/14 17:58:11  grichenk
* Fixed long tags processing
*
* Revision 1.54  2001/10/17 20:41:24  grichenk
* Added CObjectOStream::CharBlock class
*
* Revision 1.53  2001/06/07 17:12:51  grichenk
* Redesigned checking and substitution of non-printable characters
* in VisibleString
*
* Revision 1.52  2001/05/17 15:07:08  lavr
* Typos corrected
*
* Revision 1.51  2001/01/22 23:12:57  vakatov
* CObjectIStreamAsnBinary::{Read,Skip}ClassSequential() - use cur.member "pos"
*
* Revision 1.50  2001/01/03 15:22:26  vasilche
* Fixed limited buffer size for REAL data in ASN.1 binary format.
* Fixed processing non ASCII symbols in ASN.1 text format.
*
* Revision 1.49  2000/12/26 22:24:11  vasilche
* Fixed errors of compilation on Mac.
*
* Revision 1.48  2000/12/15 21:29:02  vasilche
* Moved some typedefs/enums from corelib/ncbistd.hpp.
* Added flags to CObjectIStream/CObjectOStream: eFlagAllowNonAsciiChars.
* TByte typedef replaced by Uint1.
*
* Revision 1.47  2000/12/15 15:38:44  vasilche
* Added support of Int8 and long double.
* Enum values now have type Int4 instead of long.
*
* Revision 1.46  2000/10/20 15:51:41  vasilche
* Fixed data error processing.
* Added interface for constructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.45  2000/10/17 18:45:34  vasilche
* Added possibility to turn off object cross reference detection in
* CObjectIStream and CObjectOStream.
*
* Revision 1.44  2000/09/29 16:18:23  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.43  2000/09/18 20:00:23  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.42  2000/09/01 13:16:18  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.41  2000/08/15 19:44:49  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.40  2000/07/03 20:47:22  vasilche
* Removed unused variables/functions.
*
* Revision 1.39  2000/07/03 20:39:56  vasilche
* Fixed comments.
*
* Revision 1.38  2000/07/03 18:42:45  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.37  2000/06/16 16:31:20  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.36  2000/06/07 19:45:59  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.35  2000/06/01 19:07:04  vasilche
* Added parsing of XML data.
*
* Revision 1.34  2000/05/24 20:08:47  vasilche
* Implemented XML dump.
*
* Revision 1.33  2000/05/09 16:38:39  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.32  2000/04/28 16:58:13  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.31  2000/04/13 14:50:27  vasilche
* Added CObjectIStream::Open() and CObjectOStream::Open() for easier use.
*
* Revision 1.30  2000/03/29 15:55:28  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.29  2000/03/14 14:42:31  vasilche
* Fixed error reporting.
*
* Revision 1.28  2000/03/10 17:59:20  vasilche
* Fixed error reporting.
* Added EOF bug workaround on MIPSpro compiler (not finished).
*
* Revision 1.27  2000/03/07 14:41:34  vasilche
* Removed default argument.
*
* Revision 1.26  2000/03/07 14:06:23  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
* Added generation of reference counted objects.
*
* Revision 1.25  2000/02/17 20:02:44  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.24  2000/01/10 19:46:40  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.23  2000/01/05 19:43:54  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.22  1999/12/17 19:05:03  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.21  1999/10/19 15:41:03  vasilche
* Fixed reference to IOS_BASE
*
* Revision 1.20  1999/10/18 20:21:41  vasilche
* Enum values now have long type.
* Fixed template generation for enums.
*
* Revision 1.19  1999/10/08 21:00:43  vasilche
* Implemented automatic generation of unnamed ASN.1 types.
*
* Revision 1.18  1999/10/04 16:22:17  vasilche
* Fixed bug with old ASN.1 structures.
*
* Revision 1.17  1999/09/24 18:19:18  vasilche
* Removed dependency on NCBI toolkit.
*
* Revision 1.16  1999/09/23 21:16:08  vasilche
* Removed dependence on asn.h
*
* Revision 1.15  1999/09/23 18:56:59  vasilche
* Fixed bugs with overloaded methods in objistr*.hpp & objostr*.hpp
*
* Revision 1.14  1999/09/22 20:11:55  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.13  1999/09/14 18:54:18  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.12  1999/08/16 16:08:31  vasilche
* Added ENUMERATED type.
*
* Revision 1.11  1999/08/13 20:22:57  vasilche
* Fixed lot of bugs in datatool
*
* Revision 1.10  1999/08/13 15:53:51  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.9  1999/07/26 18:31:37  vasilche
* Implemented skipping of unused values.
* Added more useful error report.
*
* Revision 1.8  1999/07/22 20:36:38  vasilche
* Fixed 'using namespace' declaration for MSVC.
*
* Revision 1.7  1999/07/22 19:40:56  vasilche
* Fixed bug with complex object graphs (pointers to members of other objects).
*
* Revision 1.6  1999/07/22 17:33:52  vasilche
* Unified reading/writing of objects in all three formats.
*
* Revision 1.5  1999/07/21 20:02:54  vasilche
* Added embedding of ASN.1 binary output from ToolKit to our binary format.
* Fixed bugs with storing pointers into binary ASN.1
*
* Revision 1.4  1999/07/21 14:20:05  vasilche
* Added serialization of bool.
*
* Revision 1.3  1999/07/09 16:32:54  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.2  1999/07/07 21:15:02  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.1  1999/07/02 21:31:56  vasilche
* Implemented reading from ASN.1 binary format.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.hpp>

#include <serial/objistrasnb.hpp>
#include <serial/member.hpp>
#include <serial/memberid.hpp>
#include <serial/enumvalues.hpp>
#include <serial/memberlist.hpp>
#include <serial/objhook.hpp>
#include <serial/classinfo.hpp>
#include <serial/choice.hpp>
#include <serial/continfo.hpp>
#include <serial/objistrimpl.hpp>
#include <serial/pack_string.hpp>

BEGIN_NCBI_SCOPE


CObjectIStream* CObjectIStream::CreateObjectIStreamAsnBinary(void)
{
    return new CObjectIStreamAsnBinary();
}


CObjectIStreamAsnBinary::CObjectIStreamAsnBinary(EFixNonPrint how)
    : CObjectIStream(eSerial_AsnBinary), m_FixMethod(how)
{
#if CHECK_STREAM_INTEGRITY
    m_CurrentTagState = eTagStart;
    m_CurrentTagLimit = numeric_limits<CNcbiStreamoff>::max();
#endif
    m_CurrentTagLength = 0;
}

CObjectIStreamAsnBinary::CObjectIStreamAsnBinary(CNcbiIstream& in,
                                                 EFixNonPrint how)
    : CObjectIStream(eSerial_AsnBinary), m_FixMethod(how)
{
#if CHECK_STREAM_INTEGRITY
    m_CurrentTagState = eTagStart;
    m_CurrentTagLimit = numeric_limits<CNcbiStreamoff>::max();
#endif
    m_CurrentTagLength = 0;
    Open(in);
}

CObjectIStreamAsnBinary::CObjectIStreamAsnBinary(CNcbiIstream& in,
                                                 bool deleteIn,
                                                 EFixNonPrint how)
    : CObjectIStream(eSerial_AsnBinary), m_FixMethod(how)
{
#if CHECK_STREAM_INTEGRITY
    m_CurrentTagState = eTagStart;
    m_CurrentTagLimit = numeric_limits<CNcbiStreamoff>::max();
#endif
    m_CurrentTagLength = 0;
    Open(in, deleteIn);
}

CObjectIStreamAsnBinary::CObjectIStreamAsnBinary(CByteSourceReader& reader,
                                                 EFixNonPrint how)
    : CObjectIStream(eSerial_AsnBinary), m_FixMethod(how)
{
#if CHECK_STREAM_INTEGRITY
    m_CurrentTagState = eTagStart;
    m_CurrentTagLimit = numeric_limits<CNcbiStreamoff>::max();
#endif
    m_CurrentTagLength = 0;
    Open(reader);
}

#if CHECK_STREAM_INTEGRITY
CObjectIStreamAsnBinary::TByte
CObjectIStreamAsnBinary::PeekTagByte(size_t index)
{
    if ( m_CurrentTagState != eTagStart )
        ThrowError(fIllegalCall,
            "illegal PeekTagByte call: only allowed at tag start");
    return m_Input.PeekChar(index);
}

CObjectIStreamAsnBinary::TByte
CObjectIStreamAsnBinary::StartTag(TByte first_tag_byte)
{
    if ( m_CurrentTagLength != 0 )
        ThrowError(fIllegalCall,
            "illegal StartTag call: current tag length != 0");
    _ASSERT(PeekTagByte() == first_tag_byte);
    return first_tag_byte;
}
#endif

CObjectIStreamAsnBinary::TLongTag
CObjectIStreamAsnBinary::PeekTag(TByte first_tag_byte)
{
    TByte byte = StartTag(first_tag_byte);
    ETagValue sysTag = GetTagValue(byte);
    if ( sysTag != eLongTag ) {
        m_CurrentTagLength = 1;
#if CHECK_STREAM_INTEGRITY
        m_CurrentTagState = eTagParsed;
#endif
        return sysTag;
    }
    TLongTag tag = 0;
    size_t i = 1;
    const size_t KBitsInByte = 8; // ?
    const size_t KTagBits = sizeof(tag) * KBitsInByte - 1;
    do {
        if ( tag >= (1 << (KTagBits - 7)) ) {
            ThrowError(fOverflow,
                       "tag number is too big: "+NStr::UIntToString(tag));
        }
        byte = PeekTagByte(i++);
        tag = (tag << 7) | (byte & 0x7f);
    } while ( (byte & 0x80) != 0 );
    m_CurrentTagLength = i;
#if CHECK_STREAM_INTEGRITY
    m_CurrentTagState = eTagParsed;
#endif
    return tag;
}

void CObjectIStreamAsnBinary::UnexpectedTagClassByte(TByte first_tag_byte,
                                                     TByte expected_class_byte)
{
    ThrowError(fFormatError, "unexpected tag class/constructed: #"
               + NStr::UIntToString(first_tag_byte) + ", should be #"
               + NStr::UIntToString(expected_class_byte));
}

string CObjectIStreamAsnBinary::PeekClassTag(void)
{
    TByte byte = StartTag(PeekTagByte());
    if ( GetTagValue(byte) != eLongTag ) {
        ThrowError(fFormatError, "long tag expected");
    }
    string name;
    size_t i = 1;
    TByte c;
    while ( ((c = PeekTagByte(i++)) & 0x80) != 0 ) {
        name += char(c & 0x7f);
        if ( i > 1024 ) {
            ThrowError(fOverflow, "tag number is too big (greater than 1024)");
        }
    }
    m_CurrentTagLength = i;
#if CHECK_STREAM_INTEGRITY
    m_CurrentTagState = eTagParsed;
#endif
    name += char(c & 0x7f);
    return name;
}

CObjectIStreamAsnBinary::TByte
CObjectIStreamAsnBinary::PeekAnyTagFirstByte(void)
{
    TByte fByte = StartTag(PeekTagByte());
    if ( GetTagValue(fByte) != eLongTag ) {
        m_CurrentTagLength = 1;
#if CHECK_STREAM_INTEGRITY
        m_CurrentTagState = eTagParsed;
#endif
        return fByte;
    }
    size_t i = 1;
    TByte byte;
    do {
        if ( i > 1024 ) {
            ThrowError(fOverflow, "tag number is too big (greater than 1024)");
        }
        byte = PeekTagByte(i++);
    } while ( (byte & 0x80) != 0 );
    m_CurrentTagLength = i;
#if CHECK_STREAM_INTEGRITY
    m_CurrentTagState = eTagParsed;
#endif
    return fByte;
}

void CObjectIStreamAsnBinary::UnexpectedSysTagByte(TByte tag_byte)
{
    ThrowError(fFormatError,
               "unexpected tag: " + NStr::IntToString(PeekTagByte()) +
               ", should be: " + NStr::IntToString(tag_byte));
}

void CObjectIStreamAsnBinary::UnexpectedByte(TByte byte)
{
    ThrowError(fFormatError,
               "byte " + NStr::IntToString(byte) + " expected");
}

#if CHECK_STREAM_INTEGRITY
CObjectIStreamAsnBinary::TByte
CObjectIStreamAsnBinary::FlushTag(void)
{
    if ( m_CurrentTagState != eTagParsed || m_CurrentTagLength == 0 )
        ThrowError(fIllegalCall, "illegal FlushTag call");
    m_Input.SkipChars(m_CurrentTagLength);
    m_CurrentTagState = eLengthValue;
    return TByte(m_Input.GetChar());
}

bool CObjectIStreamAsnBinary::PeekIndefiniteLength(void)
{
    if ( m_CurrentTagState != eTagParsed )
        ThrowError(fIllegalCall, "illegal PeekIndefiniteLength call");
    return
        TByte(m_Input.PeekChar(m_CurrentTagLength)) == eIndefiniteLengthByte;
}

void CObjectIStreamAsnBinary::ExpectIndefiniteLength(void)
{
    // indefinite length allowed only for constructed tags
    if ( !GetTagConstructed(m_Input.PeekChar()) )
        ThrowError(fFormatError, "illegal ExpectIndefiniteLength call");
    if ( FlushTag() != eIndefiniteLengthByte ) {
        ThrowError(fFormatError, "indefinite length is expected");
    }
    _ASSERT(m_CurrentTagLimit == numeric_limits<CNcbiStreamoff>::max());
    // save tag limit
    // tag limit is not changed
    m_Limits.push(m_CurrentTagLimit);
    m_CurrentTagState = eTagStart;
    m_CurrentTagLength = 0;
}

size_t CObjectIStreamAsnBinary::StartTagData(size_t length)
{
    CNcbiStreamoff cur_pos = m_Input.GetStreamOffset();
    CNcbiStreamoff newLimit = cur_pos + length;
    _ASSERT(newLimit >= cur_pos);
    CNcbiStreamoff currentLimit = m_CurrentTagLimit;
    _ASSERT(newLimit <= currentLimit);
    m_Limits.push(currentLimit);
    m_CurrentTagLimit = newLimit;
    m_CurrentTagState = eData;
    return length;
}
#endif

size_t CObjectIStreamAsnBinary::ReadShortLength(void)
{
    TByte byte = FlushTag();
    if ( byte >= 0x80 ) {
        ThrowError(fFormatError, "short length expected");
    }
    return StartTagData(byte);
}

size_t CObjectIStreamAsnBinary::ReadLength(void)
{
    TByte byte = FlushTag();
    if ( byte < 0x80 ) {
        return StartTagData(byte);
    }
    size_t lengthLength = byte - 0x80;
    if ( lengthLength == 0 ) {
        ThrowError(fFormatError, "unexpected indefinite length");
    }
    if ( lengthLength > sizeof(size_t) ) {
        ThrowError(fOverflow, "length overflow");
    }
    byte = m_Input.GetChar();
    if ( byte == 0 ) {
        ThrowError(fFormatError, "illegal length start");
    }
    if ( size_t(-1) < size_t(0) ) {        // size_t is signed
        // check for sign overflow
        if ( lengthLength == sizeof(size_t) && (byte & 0x80) != 0 ) {
            ThrowError(fOverflow, "length overflow");
        }
    }
    lengthLength--;
    size_t length = byte;
    while ( lengthLength-- > 0 )
        length = (length << 8) | Uint1(m_Input.GetChar());
    return StartTagData(length);
}

void CObjectIStreamAsnBinary::ExpectShortLength(size_t length)
{
    if ( ReadShortLength() != length ) {
        ThrowError(fFormatError,
                   "length expected: "+NStr::UIntToString(length));
    }
}

#if CHECK_STREAM_INTEGRITY
void CObjectIStreamAsnBinary::EndOfTag(void)
{
    if ( m_CurrentTagState != eData )
        ThrowError(fIllegalCall, "illegal EndOfTag call");
    // check for all bytes read
    if ( m_CurrentTagLimit != numeric_limits<CNcbiStreamoff>::max() ) {
        if ( m_Input.GetStreamOffset() != m_CurrentTagLimit )
            ThrowError(fIllegalCall,
                       "illegal EndOfTag call: not all data bytes read");
    }
    m_CurrentTagState = eTagStart;
    m_CurrentTagLength = 0;
    // restore tag limit from stack
    m_CurrentTagLimit = m_Limits.top();
    m_Limits.pop();
    _ASSERT(m_CurrentTagLimit == numeric_limits<CNcbiStreamoff>::max());
}

void CObjectIStreamAsnBinary::ExpectEndOfContent(void)
{
    if ( m_CurrentTagState != eTagStart || m_CurrentTagLength != 0 )
        ThrowError(fFormatError, "illegal ExpectEndOfContent call");
    if ( !m_Input.SkipExpectedChars(char(eEndOfContentsByte),
                                    char(eZeroLengthByte)) ) {
        ThrowError(eFormatError, "end of content expected");
    }
    _ASSERT(m_CurrentTagLimit == numeric_limits<CNcbiStreamoff>::max());
    // restore tag limit from stack
    m_CurrentTagLimit = m_Limits.top();
    m_Limits.pop();
    _ASSERT(m_CurrentTagLimit == numeric_limits<CNcbiStreamoff>::max());
    m_CurrentTagState = eTagStart;
    m_CurrentTagLength = 0;
}

Uint1 CObjectIStreamAsnBinary::ReadByte(void)
{
    if ( m_CurrentTagState != eData )
        ThrowError(fIllegalCall, "illegal ReadByte call");
    if ( m_Input.GetStreamOffset() >= m_CurrentTagLimit )
        ThrowError(fOverflow, "tag size overflow");
    return m_Input.GetChar();
}
#endif

void CObjectIStreamAsnBinary::ReadBytes(char* buffer, size_t count)
{
#if CHECK_STREAM_INTEGRITY
    if ( m_CurrentTagState != eData ) {
        ThrowError(fIllegalCall, "illegal ReadBytes call");
    }
#endif
    if ( count == 0 )
        return;
#if CHECK_STREAM_INTEGRITY
    CNcbiStreamoff cur_pos = m_Input.GetStreamOffset();
    CNcbiStreamoff end_pos = cur_pos + count;
    if ( end_pos < cur_pos || end_pos > m_CurrentTagLimit )
        ThrowError(fOverflow, "tag size overflow");
#endif
    m_Input.GetChars(buffer, count);
}

void CObjectIStreamAsnBinary::ReadBytes(string& str, size_t count)
{
#if CHECK_STREAM_INTEGRITY
    if ( m_CurrentTagState != eData ) {
        ThrowError(fIllegalCall, "illegal ReadBytes call");
    }
#endif
    if ( count == 0 )
        return;
#if CHECK_STREAM_INTEGRITY
    CNcbiStreamoff cur_pos = m_Input.GetStreamOffset();
    CNcbiStreamoff end_pos = cur_pos + count;
    if ( end_pos < cur_pos || end_pos > m_CurrentTagLimit )
        ThrowError(fOverflow, "tag size overflow");
#endif
    m_Input.GetChars(str, count);
}

void CObjectIStreamAsnBinary::SkipBytes(size_t count)
{
#if CHECK_STREAM_INTEGRITY
    if ( m_CurrentTagState != eData ) {
        ThrowError(fIllegalCall, "illegal ReadBytes call");
    }
#endif
    if ( count == 0 )
        return;
#if CHECK_STREAM_INTEGRITY
    CNcbiStreamoff cur_pos = m_Input.GetStreamOffset();
    CNcbiStreamoff end_pos = cur_pos + count;
    if ( end_pos < cur_pos || end_pos > m_CurrentTagLimit )
        ThrowError(fOverflow, "tag size overflow");
#endif
    m_Input.GetChars(count);
}

template<typename T>
void ReadStdSigned(CObjectIStreamAsnBinary& in, T& data)
{
    size_t length = in.ReadShortLength();
    if ( length == 0 ) {
        in.ThrowError(in.fFormatError, "zero length of number");
    }
    T n;
    if ( length > sizeof(data) ) {
        // skip
        --length;
        Int1 c = in.ReadSByte();
        if ( c != 0 && c != -1 ) {
            in.ThrowError(in.fOverflow, "overflow error");
        }
        while ( length > sizeof(data) ) {
            --length;
            if ( in.ReadSByte() != c ) {
                in.ThrowError(in.fOverflow, "overflow error");
            }
        }
        --length;
        n = in.ReadSByte();
        if ( ((n ^ c) & 0x80) != 0 ) {
            in.ThrowError(in.fOverflow, "overflow error");
        }
    }
    else {
        --length;
        n = in.ReadSByte();
    }
    while ( length > 0 ) {
        --length;
        n = (n << 8) | in.ReadByte();
    }
    data = n;
    in.EndOfTag();
}

template<typename T>
void ReadStdUnsigned(CObjectIStreamAsnBinary& in, T& data)
{
    size_t length = in.ReadShortLength();
    if ( length == 0 ) {
        in.ThrowError(in.fFormatError, "zero length of number");
    }
    T n;
    if ( length > sizeof(data) ) {
        // skip
        while ( length > sizeof(data) ) {
            --length;
            if ( in.ReadSByte() != 0 ) {
                in.ThrowError(in.fOverflow, "overflow error");
            }
        }
        --length;
        n = in.ReadByte();
        if ( (n & 0x80) != 0 ) {
            in.ThrowError(in.fOverflow, "overflow error");
        }
    }
    else if ( length == sizeof(data) ) {
        --length;
        n = in.ReadByte();
        if ( (n & 0x80) != 0 ) {
            in.ThrowError(in.fOverflow, "overflow error");
        }
    }
    else {
        n = 0;
    }
    while ( length > 0 ) {
        --length;
        n = (n << 8) | in.ReadByte();
    }
    data = n;
    in.EndOfTag();
}

bool CObjectIStreamAsnBinary::ReadBool(void)
{
    ExpectSysTag(eBoolean);
    ExpectShortLength(1);
    bool ret = ReadByte() != 0;
    EndOfTag();
    return ret;
}

char CObjectIStreamAsnBinary::ReadChar(void)
{
    ExpectSysTag(eGeneralString);
    ExpectShortLength(1);
    char ret = ReadByte();
    EndOfTag();
    return ret;
}

Int4 CObjectIStreamAsnBinary::ReadInt4(void)
{
    ExpectSysTag(eInteger);
    Int4 data;
    ReadStdSigned(*this, data);
    return data;
}

Uint4 CObjectIStreamAsnBinary::ReadUint4(void)
{
    ExpectSysTag(eInteger);
    Uint4 data;
    ReadStdUnsigned(*this, data);
    return data;
}

Int8 CObjectIStreamAsnBinary::ReadInt8(void)
{
    ExpectSysTag(eInteger);
    Uint8 data;
    ReadStdSigned(*this, data);
    return data;
}

Uint8 CObjectIStreamAsnBinary::ReadUint8(void)
{
    ExpectSysTag(eInteger);
    Uint8 data;
    ReadStdUnsigned(*this, data);
    return data;
}

static const size_t kMaxDoubleLength = 256;

double CObjectIStreamAsnBinary::ReadDouble(void)
{
    ExpectSysTag(eReal);
    size_t length = ReadLength();
    if ( length < 2 ) {
        ThrowError(fFormatError, "too short REAL data: length < 2");
    }
    if ( length > kMaxDoubleLength ) {
        ThrowError(fFormatError, "too long REAL data: length > "
            + NStr::UIntToString(kMaxDoubleLength));
    }

    ExpectByte(eDecimal);
    length--;
    char buffer[kMaxDoubleLength + 2];
    ReadBytes(buffer, length);
    EndOfTag();
    buffer[length] = 0;
    char* endptr;
    double data = strtod(buffer, &endptr);
    if ( *endptr != 0 ) {
        ThrowError(fFormatError, "bad REAL data string");
    }
    return data;
}

namespace {
    inline
    bool BadVisibleChar(char c)
    {
        return Uint1(c-' ') > Uint1('~' - ' ');
    }

    inline
    void ReplaceVisibleCharMethod(char& c, EFixNonPrint fix_method)
    {
        c = ReplaceVisibleChar(c, fix_method, 0);
    }

    inline
    void FixVisibleCharAlways(char& c)
    {
        if ( BadVisibleChar(c) ) {
            c = '#';
        }
    }
    inline
    void FixVisibleCharMethod(char& c, EFixNonPrint fix_method)
    {
        if ( BadVisibleChar(c) ) {
            ReplaceVisibleCharMethod(c, fix_method);
        }
    }

#define FIND_BAD_CHAR(ptr)                      \
        do {                                    \
            if ( !count ) {                     \
                return false;                   \
            }                                   \
            --count;                            \
        } while ( !BadVisibleChar(*ptr++) );    \
        --ptr

#define REPLACE_BAD_CHARS_ALWAYS(ptr)           \
        *--ptr = '#';                           \
        while ( count-- ) {                     \
            FixVisibleCharAlways(*++ptr);       \
        }                                       \
        return true
    
#define REPLACE_BAD_CHARS_METHOD(ptr, fix_method)          \
        ReplaceVisibleCharMethod(*--ptr, fix_method);      \
        while ( count-- ) {                                \
            FixVisibleCharMethod(*++ptr, fix_method);      \
        }                                                  \
        return true
    
    bool FixVisibleCharsAlways(char* ptr, size_t count)
    {
        FIND_BAD_CHAR(ptr);
        REPLACE_BAD_CHARS_ALWAYS(ptr);
    }

    bool FixVisibleCharsMethod(char* ptr, size_t count,
                               EFixNonPrint fix_method)
    {
        FIND_BAD_CHAR(ptr);
        REPLACE_BAD_CHARS_METHOD(ptr, fix_method);
    }

    bool FixVisibleCharsAlways(string& s)
    {
        size_t count = s.size();
        const char* ptr = s.data();
        FIND_BAD_CHAR(ptr);
        string::iterator it = s.begin()+(ptr-s.data());
        REPLACE_BAD_CHARS_ALWAYS(it);
    }

    bool FixVisibleCharsMethod(string& s, EFixNonPrint fix_method)
    {
        size_t count = s.size();
        const char* ptr = s.data();
        FIND_BAD_CHAR(ptr);
        string::iterator it = s.begin()+(ptr-s.data());
        REPLACE_BAD_CHARS_METHOD(it, fix_method);
    }

    inline
    bool FixVisibleChars(char* ptr, size_t count, EFixNonPrint fix_method)
    {
        return fix_method == eFNP_Allow? false:
            fix_method == eFNP_Replace? FixVisibleCharsAlways(ptr, count):
            FixVisibleCharsMethod(ptr, count, fix_method);
    }

    inline
    bool FixVisibleChars(string& s, EFixNonPrint fix_method)
    {
        return fix_method == eFNP_Allow? false:
            fix_method == eFNP_Replace? FixVisibleCharsAlways(s):
            FixVisibleCharsMethod(s, fix_method);
    }
}

void CObjectIStreamAsnBinary::ReadString(string& s,
                                         CPackString& pack_string,
                                         EStringType type)
{
    ExpectSysTag(eVisibleString);
    size_t length = ReadLength();
    static const size_t BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE];
    if ( length > BUFFER_SIZE || length > pack_string.GetLengthLimit() ) {
        pack_string.Skipped();
        ReadStringValue(length, s,
                        type == eStringTypeVisible? m_FixMethod: eFNP_Allow);
    }
    else {
        ReadBytes(buffer, length);
        EndOfTag();
        pair<CPackString::iterator, bool> found =
            pack_string.Locate(buffer, length);
        if ( found.second ) {
            pack_string.AddOld(s, found.first);
        }
        else {
            if ( type == eStringTypeVisible &&
                 FixVisibleChars(buffer, length, m_FixMethod) ) {
                // do not remember fixed strings
                pack_string.Pack(s, buffer, length);
                return;
            }
            pack_string.AddNew(s, buffer, length, found.first);
        }
    }
}

void CObjectIStreamAsnBinary::ReadString(string& s, EStringType type)
{
    ExpectSysTag(eVisibleString);
    ReadStringValue(ReadLength(), s,
                    type == eStringTypeVisible? m_FixMethod: eFNP_Allow);
}


void CObjectIStreamAsnBinary::ReadStringStore(string& s)
{
    ExpectSysTagByte(MakeTagByte(eApplication, ePrimitive, eStringStore));
    ReadStringValue(ReadLength(), s, m_FixMethod);
}

void CObjectIStreamAsnBinary::ReadStringValue(size_t length,
                                              string& s,
                                              EFixNonPrint fix_method)
{
    static const size_t BUFFER_SIZE = 1024;
    if ( length != s.size() || length > BUFFER_SIZE ) {
        // new string
        ReadBytes(s, length);
        FixVisibleChars(s, fix_method);
    }
    else {
        char buffer[BUFFER_SIZE];
        // try to reuse old value
        ReadBytes(buffer, length);
        FixVisibleChars(buffer, length, fix_method);
        if ( memcmp(s.data(), buffer, length) != 0 ) {
            s.assign(buffer, length);
        }
    }
    EndOfTag();
}

char* CObjectIStreamAsnBinary::ReadCString(void)
{
    ExpectSysTag(eVisibleString);
    size_t length = ReadLength();
    char* s = static_cast<char*>(malloc(length + 1));
    ReadBytes(s, length);
    s[length] = 0;
    FixVisibleChars(s, length, m_FixMethod);
    EndOfTag();
    return s;
}

void CObjectIStreamAsnBinary::BeginContainer(const CContainerTypeInfo* contType)
{
    ExpectContainer(contType->RandomElementsOrder());
}

void CObjectIStreamAsnBinary::EndContainer(void)
{
    ExpectEndOfContent();
}

bool CObjectIStreamAsnBinary::BeginContainerElement(TTypeInfo /*elementType*/)
{
    return HaveMoreElements();
}

#ifdef VIRTUAL_MID_LEVEL_IO
void CObjectIStreamAsnBinary::ReadContainer(const CContainerTypeInfo* cType,
                                            TObjectPtr containerPtr)
{
    ExpectContainer(cType->RandomElementsOrder());

    BEGIN_OBJECT_FRAME(eFrameArrayElement);

    CContainerTypeInfo::CIterator iter;
    bool old_element = cType->InitIterator(iter, containerPtr);
    TTypeInfo elementType = cType->GetElementType();
    while ( HaveMoreElements() ) {
        if ( old_element ) {
            elementType->ReadData(*this, cType->GetElementPtr(iter));
            old_element = cType->NextElement(iter);
        }
        else {
            cType->AddElement(containerPtr, *this);
        }
    }
    if ( old_element ) {
        cType->EraseAllElements(iter);
    }

    END_OBJECT_FRAME();

    ExpectEndOfContent();
}

void CObjectIStreamAsnBinary::SkipContainer(const CContainerTypeInfo* cType)
{
    ExpectContainer(cType->RandomElementsOrder());

    TTypeInfo elementType = cType->GetElementType();
    BEGIN_OBJECT_FRAME(eFrameArrayElement);

    while ( HaveMoreElements() ) {
        SkipObject(elementType);
    }

    END_OBJECT_FRAME();

    ExpectEndOfContent();
}
#endif

void CObjectIStreamAsnBinary::BeginClass(const CClassTypeInfo* classInfo)
{
    ExpectContainer(classInfo->RandomOrder());
}

void CObjectIStreamAsnBinary::EndClass(void)
{
    ExpectEndOfContent();
}

void CObjectIStreamAsnBinary::UnexpectedMember(TLongTag tag)
{
    ThrowError(fFormatError,
               "unexpected member: ["+NStr::IntToString(tag)+"]");
}

TMemberIndex
CObjectIStreamAsnBinary::BeginClassMember(const CClassTypeInfo* classType)
{
    TByte first_tag_byte = PeekTagByte();
    if ( first_tag_byte == eEndOfContentsByte )
        return kInvalidMember;

    TLongTag tag = PeekTag(first_tag_byte, eContextSpecific, eConstructed);
    ExpectIndefiniteLength();
    TMemberIndex index = classType->GetMembers().Find(tag);
    if ( index == kInvalidMember ) {
        if (GetSkipUnknownMembers() == eSerialSkipUnknown_Yes) {
            ReadAnyContent();
            ExpectEndOfContent();
            return BeginClassMember(classType);
        }
        else {
            UnexpectedMember(tag);
        }
    }
    return index;
}

TMemberIndex
CObjectIStreamAsnBinary::BeginClassMember(const CClassTypeInfo* classType,
                                          TMemberIndex pos)
{
    TByte first_tag_byte = PeekTagByte();
    if ( first_tag_byte == eEndOfContentsByte )
        return kInvalidMember;

    TLongTag tag = PeekTag(first_tag_byte, eContextSpecific, eConstructed);
    ExpectIndefiniteLength();
    TMemberIndex index = classType->GetMembers().Find(tag, pos);
    if ( index == kInvalidMember ) {
        if (GetSkipUnknownMembers() == eSerialSkipUnknown_Yes) {
            ReadAnyContent();
            ExpectEndOfContent();
            return BeginClassMember(classType, pos);
        }
        else {
            UnexpectedMember(tag);
        }
    }
    return index;
}

void CObjectIStreamAsnBinary::EndClassMember(void)
{
    ExpectEndOfContent();
}

#ifdef VIRTUAL_MID_LEVEL_IO
void CObjectIStreamAsnBinary::ReadClassRandom(const CClassTypeInfo* classType,
                                              TObjectPtr classPtr)
{
    BeginClass(classType);

    ReadClassRandomContentsBegin(classType);

    TMemberIndex index;
    while ( (index = BeginClassMember(classType)) != kInvalidMember ) {
        ReadClassRandomContentsMember(classPtr);
        EndClassMember();
    }

    ReadClassRandomContentsEnd();

    EndClass();
}

void
CObjectIStreamAsnBinary::ReadClassSequential(const CClassTypeInfo* classType,
                                             TObjectPtr classPtr)
{
    BeginClass(classType);
    ReadClassSequentialContentsBegin(classType);

    TMemberIndex index;
    while ( (index = BeginClassMember(classType,*pos)) != kInvalidMember ) {
        ReadClassSequentialContentsMember(classPtr);
        EndClassMember();
    }

    ReadClassSequentialContentsEnd(classPtr);
    EndClass();
}

void CObjectIStreamAsnBinary::SkipClassRandom(const CClassTypeInfo* classType)
{
    BeginClass(classType);
    SkipClassRandomContentsBegin(classType);

    TMemberIndex index;
    while ( (index = BeginClassMember(classType)) != kInvalidMember ) {
        SkipClassRandomContentsMember();
        EndClassMember();
    }

    SkipClassRandomContentsEnd();
    EndClass();
}

void
CObjectIStreamAsnBinary::SkipClassSequential(const CClassTypeInfo* classType)
{
    BeginClass(classType);
    SkipClassSequentialContentsBegin(classType);

    TMemberIndex index;
    while ( (index = BeginClassMember(classType,*pos)) != kInvalidMember ) {
        SkipClassSequentialContentsMember();
        EndClassMember();
    }

    SkipClassSequentialContentsEnd();
    EndClass();
}
#endif

TMemberIndex
CObjectIStreamAsnBinary::BeginChoiceVariant(const CChoiceTypeInfo* choiceType)
{
    TLongTag tag = PeekTag(PeekTagByte(), eContextSpecific, eConstructed);
    ExpectIndefiniteLength();
    TMemberIndex index = choiceType->GetVariants().Find(tag);
    if ( index == kInvalidMember )
        UnexpectedMember(tag);
    return index;
}

void CObjectIStreamAsnBinary::EndChoiceVariant(void)
{
    ExpectEndOfContent();
}

#ifdef VIRTUAL_MID_LEVEL_IO
void CObjectIStreamAsnBinary::ReadChoice(const CChoiceTypeInfo* choiceType,
                                         TObjectPtr choicePtr)
{
    TMemberIndex index = BeginChoiceVariant(choiceType);

    const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
    BEGIN_OBJECT_FRAME2(eFrameChoiceVariant, variantInfo->GetId());

    variantInfo->ReadVariant(*this, choicePtr);

    END_OBJECT_FRAME();

    EndChoiceVariant();
}

void CObjectIStreamAsnBinary::SkipChoice(const CChoiceTypeInfo* choiceType)
{
    TMemberIndex index = BeginChoiceVariant(choiceType);

    const CVariantInfo* variantInfo = choiceType->GetVariantInfo(index);
    BEGIN_OBJECT_FRAME2(eFrameChoiceVariant, variantInfo->GetId());

    variantInfo->SkipVariant(*this);

    END_OBJECT_FRAME();

    EndChoiceVariant();
}
#endif

void CObjectIStreamAsnBinary::BeginBytes(ByteBlock& block)
{
    ExpectSysTag(eOctetString);
    block.SetLength(ReadLength());
}

size_t CObjectIStreamAsnBinary::ReadBytes(ByteBlock& ,
                                          char* dst, size_t length)
{
    ReadBytes(dst, length);
    return length;
}

void CObjectIStreamAsnBinary::EndBytes(const ByteBlock& )
{
    EndOfTag();
}

void CObjectIStreamAsnBinary::BeginChars(CharBlock& block)
{
    ExpectSysTag(eVisibleString);
    block.SetLength(ReadLength());
}

size_t CObjectIStreamAsnBinary::ReadChars(CharBlock& ,
                                          char* dst, size_t length)
{
    ReadBytes(dst, length);
    return length;
}

void CObjectIStreamAsnBinary::EndChars(const CharBlock& )
{
    EndOfTag();
}

void CObjectIStreamAsnBinary::ReadNull(void)
{
    ExpectSysTag(eNull);
    ExpectShortLength(0);
    EndOfTag();
}

bool CObjectIStreamAsnBinary::ReadAnyContent()
{
    TByte byte = PeekAnyTagFirstByte();
    if ( GetTagConstructed(byte) && PeekIndefiniteLength() ) {
        ExpectIndefiniteLength();
        if ( ReadAnyContent() ) {
            while (HaveMoreElements()) {
                ReadAnyContent();
            }
            ExpectEndOfContent();
        }
        return true;
    }
    size_t length = ReadLength();
    if (length) {
        SkipBytes(length);
    }
    EndOfTag();
    return (length != 0);
}

void CObjectIStreamAsnBinary::ReadAnyContentObject(CAnyContentObject& )
{
    NCBI_THROW(CSerialException,eNotImplemented,
        "CObjectIStreamAsnBinary::ReadAnyContentObject: "
        "unable to read AnyContent object in ASN binary");
}

void CObjectIStreamAsnBinary::SkipAnyContentObject(void)
{
    NCBI_THROW(CSerialException,eNotImplemented,
        "CObjectIStreamAsnBinary::SkipAnyContentObject: "
        "unable to skip AnyContent object in ASN binary");
}

CObjectIStream::EPointerType CObjectIStreamAsnBinary::ReadPointerType(void)
{
    TByte byte = PeekTagByte();
    // variants:
    //    eUniversal,   !constructed, eNull             -> NULL
    //    eApplication,  constructed, eMemberReference  -> member reference
    //    eApplication,  constructed, eLongTag          -> other class
    //    eApplication, !constructed, eObjectReference  -> object reference
    // any other -> this class
    if ( byte == MakeTagByte(eUniversal, ePrimitive, eNull) ) {
        ExpectShortLength(0);
        EndOfTag();
        return eNullPointer;
    }
    else if ( byte == MakeTagByte(eApplication, eConstructed, eLongTag) ) {
        return eOtherPointer;
    }
    else if ( byte == MakeTagByte(eApplication, ePrimitive, eObjectReference) ) {
        return eObjectPointer;
    }
    // by default: try this class
    return eThisPointer;
}

TEnumValueType
CObjectIStreamAsnBinary::ReadEnum(const CEnumeratedTypeValues& values)
{
    TEnumValueType value;
    if ( values.IsInteger() ) {
        // allow any integer
        ExpectSysTag(eInteger);
        ReadStdSigned(*this, value);
    }
    else {
        // enum element by value
        ExpectSysTag(eEnumerated);
        ReadStdSigned(*this, value);
        values.FindName(value, false); // check value
    }
    return value;
}

CObjectIStream::TObjectIndex CObjectIStreamAsnBinary::ReadObjectPointer(void)
{
    TObjectIndex data;
    ReadStdSigned(*this, data);
    return data;
}

string CObjectIStreamAsnBinary::ReadOtherPointer(void)
{
    string className = PeekClassTag();
    ExpectIndefiniteLength();
    return className;
}

void CObjectIStreamAsnBinary::ReadOtherPointerEnd(void)
{
    ExpectEndOfContent();
}

bool CObjectIStreamAsnBinary::SkipRealValue(void)
{
    if ( PeekTagByte() == eEndOfContentsByte &&
         PeekTagByte(1) == eZeroLengthByte )
        return false;
    TByte byte = PeekAnyTagFirstByte();
    if ( GetTagConstructed(byte) ) {
        // constructed
        ExpectIndefiniteLength();
        while ( SkipRealValue() )
            ;
        ExpectEndOfContent();
    }
    else {
        SkipTagData();
    }
    return true;
}


void CObjectIStreamAsnBinary::SkipBool(void)
{
    ExpectSysTag(eBoolean);
    ExpectShortLength(1);
    ReadByte();
    EndOfTag();
}

void CObjectIStreamAsnBinary::SkipChar(void)
{
    ExpectSysTag(eGeneralString);
    ExpectShortLength(1);
    ReadByte();
    EndOfTag();
}

void CObjectIStreamAsnBinary::SkipSNumber(void)
{
    ExpectSysTag(eInteger);
    SkipTagData();
}

void CObjectIStreamAsnBinary::SkipUNumber(void)
{
    ExpectSysTag(eInteger);
    SkipTagData();
}

void CObjectIStreamAsnBinary::SkipFNumber(void)
{
    ExpectSysTag(eReal);
    size_t length = ReadLength();
    if ( length < 2 )
        ThrowError(fFormatError, "too short REAL data: length < 2");
    if ( length > kMaxDoubleLength )
        ThrowError(fFormatError, "too long REAL data: length > "
            + NStr::UIntToString(kMaxDoubleLength));

    ExpectByte(eDecimal);
    length--;
    SkipBytes(length);
    EndOfTag();
}

void CObjectIStreamAsnBinary::SkipString(EStringType /*type*/)
{
    ExpectSysTag(eVisibleString);
    SkipTagData();
}

void CObjectIStreamAsnBinary::SkipStringStore(void)
{
    ExpectSysTag(eApplication, ePrimitive, eStringStore);
    SkipTagData();
}

void CObjectIStreamAsnBinary::SkipNull(void)
{
    ExpectSysTag(eNull);
    ExpectShortLength(0);
    EndOfTag();
}

void CObjectIStreamAsnBinary::SkipByteBlock(void)
{
    ExpectSysTag(eOctetString);
    SkipTagData();
}

void CObjectIStreamAsnBinary::SkipTagData(void)
{
    SkipBytes(ReadLength());
    EndOfTag();
}

END_NCBI_SCOPE
