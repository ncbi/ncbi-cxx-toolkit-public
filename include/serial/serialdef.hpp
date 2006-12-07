#ifndef SERIALDEF__HPP
#define SERIALDEF__HPP

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


/** @addtogroup GenClassSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

// forward declaration of two main classes
class CTypeRef;
class CTypeInfo;

class CEnumeratedTypeValues;

class CObjectIStream;
class CObjectOStream;

class CObjectStreamCopier;

// typedef for object references (constant and nonconstant)
typedef void* TObjectPtr;
typedef const void* TConstObjectPtr;

// shortcut typedef: almost everywhere in code we have pointer to const CTypeInfo
typedef const CTypeInfo* TTypeInfo;
typedef TTypeInfo (*TTypeInfoGetter)(void);
typedef TTypeInfo (*TTypeInfoGetter1)(TTypeInfo);
typedef TTypeInfo (*TTypeInfoGetter2)(TTypeInfo, TTypeInfo);
typedef CTypeInfo* (*TTypeInfoCreator)(void);
typedef CTypeInfo* (*TTypeInfoCreator1)(TTypeInfo);
typedef CTypeInfo* (*TTypeInfoCreator2)(TTypeInfo, TTypeInfo);

enum ESerialDataFormat {
    eSerial_None         = 0,
    eSerial_AsnText      = 1,      // open ASN.1 text format
    eSerial_AsnBinary    = 2,      // open ASN.1 binary format
    eSerial_Xml          = 3       // open XML format
};


#define SERIAL_VERIFY_DATA_GET    "SERIAL_VERIFY_DATA_GET"
#define SERIAL_VERIFY_DATA_WRITE  "SERIAL_VERIFY_DATA_WRITE"
#define SERIAL_VERIFY_DATA_READ   "SERIAL_VERIFY_DATA_READ"

enum ESerialVerifyData {
    eSerialVerifyData_Default = 0, // use current default
    eSerialVerifyData_No,          // do not verify
    eSerialVerifyData_Never,       // never verify (even if set to verify later on)
    eSerialVerifyData_Yes,         // do verify
    eSerialVerifyData_Always,      // always verify (even if set not to later on)
    eSerialVerifyData_DefValue,    // initialize field with default
    eSerialVerifyData_DefValueAlways // initialize field with default
};

#define SERIAL_SKIP_UNKNOWN_MEMBERS    "SERIAL_SKIP_UNKNOWN_MEMBERS"
enum ESerialSkipUnknown {
    eSerialSkipUnknown_Default = 0, // use current default
    eSerialSkipUnknown_No,          // do not skip (throw exception)
    eSerialSkipUnknown_Never,       // never skip (even if set to skip later on)
    eSerialSkipUnknown_Yes,         // do skip
    eSerialSkipUnknown_Always       // always skip (even if set not to later on)
};

enum ESerialOpenFlags {
    eSerial_StdWhenEmpty = 1 << 0, // use std stream when filename is empty
    eSerial_StdWhenDash  = 1 << 1, // use std stream when filename is "-"
    eSerial_StdWhenStd   = 1 << 2, // use std when filename is "stdin"/"stdout"
    eSerial_StdWhenMask  = 15,
    eSerial_StdWhenAny   = eSerial_StdWhenMask,
    eSerial_UseFileForReread = 1 << 4
};
typedef int TSerialOpenFlags;

// type family
enum ETypeFamily {
    eTypeFamilyPrimitive,
    eTypeFamilyClass,
    eTypeFamilyChoice,
    eTypeFamilyContainer,
    eTypeFamilyPointer
};

enum EPrimitiveValueType {
    ePrimitiveValueSpecial,        // null, void
    ePrimitiveValueBool,           // bool
    ePrimitiveValueChar,           // char
    ePrimitiveValueInteger,        // (signed|unsigned) (char|short|int|long)
    ePrimitiveValueReal,           // float|double
    ePrimitiveValueString,         // string|char*|const char*
    ePrimitiveValueEnum,           // enum
    ePrimitiveValueOctetString,    // vector<(signed|unsigned)? char>
    ePrimitiveValueBitString,      //
    ePrimitiveValueOther
};

enum EContainerType {
    eContainerVector,              // allows indexing & access to size
    eContainerList,                // only sequential access
    eContainerSet,
    eContainerMap
};


// How to process non-printing character in the ASN VisibleString
//
enum EFixNonPrint {
    eFNP_Allow,            // pass through unchanged, post no error message
    eFNP_Replace,          // replace with '#' silently
    eFNP_ReplaceAndWarn,   // replace with '#', post an error of severity ERROR
    eFNP_Throw,            // replace with '#', throw an exception
    eFNP_Abort,            // replace with '#', post an error of severity FATAL

    eFNP_Default = eFNP_ReplaceAndWarn
};

enum EStringType {
    eStringTypeVisible,
    eStringTypeUTF8
};

// How to assign and compare child sub-objects of serial objects

enum ESerialRecursionMode {
    eRecursive,            // Recursively
    eShallow,              // Assign/Compare pointers only
    eShallowChildless      // Set sub-object pointers to 0
};

//type used for indexing class members and choice variants
typedef size_t TMemberIndex;

typedef int TEnumValueType;

// start if member indexing
const TMemberIndex kFirstMemberIndex = 1;
// special value returned from FindMember
const TMemberIndex kInvalidMember = kFirstMemberIndex - 1;
// special value for marking empty choice
const TMemberIndex kEmptyChoice = kInvalidMember;

typedef ssize_t TPointerOffsetType;


/* @} */


END_NCBI_SCOPE

#endif  /* SERIALDEF__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.33  2006/12/07 18:59:30  gouriano
* Reviewed doxygen groupping, added documentation
*
* Revision 1.32  2006/10/12 15:08:28  gouriano
* Some header files moved into impl
*
* Revision 1.31  2006/10/05 19:23:04  gouriano
* Some headers moved into impl
*
* Revision 1.30  2005/11/29 17:42:49  gouriano
* Added CBitString class
*
* Revision 1.29  2004/03/25 15:56:28  gouriano
* Added possibility to copy and compare serial object non-recursively
*
* Revision 1.28  2004/03/23 15:39:52  gouriano
* Added setup options for skipping unknown data members
*
* Revision 1.27  2003/12/01 19:04:22  grichenk
* Moved Add and Sub from serialutil to ncbimisc, made them methods
* of CRawPointer class.
*
* Revision 1.26  2003/11/13 14:06:44  gouriano
* Elaborated data verification on read/write/get to enable skipping mandatory class data members
*
* Revision 1.25  2003/09/10 20:57:23  gouriano
* added possibility to ignore missing mandatory members on reading
*
* Revision 1.24  2003/08/19 18:32:38  vasilche
* Optimized reading and writing strings.
* Avoid string reallocation when checking char values.
* Try to reuse old string data when string reference counting is not working.
*
* Revision 1.23  2003/08/13 15:47:02  gouriano
* implemented serialization of AnyContent objects
*
* Revision 1.22  2003/04/29 18:29:06  gouriano
* object data member initialization verification
*
* Revision 1.21  2003/04/15 16:18:51  siyan
* Added doxygen support
*
* Revision 1.20  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.19  2002/10/25 14:49:29  vasilche
* NCBI C Toolkit compatibility code extracted to libxcser library.
* Serial streams flags names were renamed to fXxx.
*
* Names of flags
*
* Revision 1.18  2001/06/07 17:12:46  grichenk
* Redesigned checking and substitution of non-printable characters
* in VisibleString
*
* Revision 1.17  2000/12/15 21:28:49  vasilche
* Moved some typedefs/enums from corelib/ncbistd.hpp.
* Added flags to CObjectIStream/CObjectOStream: eFlagAllowNonAsciiChars.
* TByte typedef replaced by Uint1.
*
* Revision 1.16  2000/12/15 15:38:01  vasilche
* Added support of Int8 and long double.
* Enum values now have type Int4 instead of long.
*
* Revision 1.15  2000/11/07 17:25:12  vasilche
* Fixed encoding of XML:
*     removed unnecessary apostrophes in OCTET STRING
*     removed unnecessary content in NULL
* Added module names to CTypeInfo and CEnumeratedTypeValues
*
* Revision 1.14  2000/10/17 18:45:26  vasilche
* Added possibility to turn off object cross reference detection in
* CObjectIStream and CObjectOStream.
*
* Revision 1.13  2000/10/13 20:22:46  vasilche
* Fixed warnings on 64 bit compilers.
* Fixed missing typename in templates.
*
* Revision 1.12  2000/09/18 20:00:09  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.11  2000/08/15 19:44:42  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.10  2000/07/03 18:42:37  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.9  2000/06/01 19:06:58  vasilche
* Added parsing of XML data.
*
* Revision 1.8  2000/04/28 16:58:03  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.7  2000/04/13 14:50:18  vasilche
* Added CObjectIStream::Open() and CObjectOStream::Open() for easier use.
*
* Revision 1.6  2000/04/06 16:10:52  vasilche
* Fixed bug with iterators in choices.
* Removed unneeded calls to ReadExternalObject/WriteExternalObject.
* Added output buffering to text ASN.1 data.
*
* Revision 1.5  2000/04/03 18:47:10  vasilche
* Added main include file for generated headers.
* serialimpl.hpp is included in generated sources with GetTypeInfo methods
*
* Revision 1.4  2000/01/10 19:46:33  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.3  1999/12/17 19:04:54  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.2  1999/09/22 20:11:51  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.1  1999/06/24 14:44:44  vasilche
* Added binary ASN.1 output.
*
* ===========================================================================
*/
