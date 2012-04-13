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
*   Constants used in serial library
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

/// Data file format
enum ESerialDataFormat {
    eSerial_None         = 0,
    eSerial_AsnText      = 1,      ///< ASN.1 text
    eSerial_AsnBinary    = 2,      ///< ASN.1 binary
    eSerial_Xml          = 3,      ///< XML
    eSerial_Json         = 4       ///< JSON
};

/// Formatting flags
enum ESerial_AsnText_Flags {
    fSerial_AsnText_NoIndentation = 1<<7, ///< do not use indentation
    fSerial_AsnText_NoEol         = 1<<6  ///< do not write end-of-line symbol
};
typedef unsigned int TSerial_AsnText_Flags;

enum ESerial_Xml_Flags {
    fSerial_Xml_NoIndentation = 1<<7, ///< do not use indentation
    fSerial_Xml_NoEol         = 1<<6, ///< do not write end-of-line symbol
    fSerial_Xml_NoXmlDecl     = 1<<5, ///< do not write XMLDecl
    fSerial_Xml_NoRefDTD      = 1<<4, ///< do not use reference to a DTD
    fSerial_Xml_RefSchema     = 1<<3, ///< use reference to a Schema
    fSerial_Xml_NoSchemaLoc   = 1<<2  ///< do not write schemaLocation data
};
typedef unsigned int TSerial_Xml_Flags;

enum ESerial_Json_Flags {
    fSerial_Json_NoIndentation = 1<<7, ///< do not use indentation
    fSerial_Json_NoEol         = 1<<6  ///< do not write end-of-line symbol
};
typedef unsigned int TSerial_Json_Flags;

#define SERIAL_VERIFY_DATA_GET    "SERIAL_VERIFY_DATA_GET"
#define SERIAL_VERIFY_DATA_WRITE  "SERIAL_VERIFY_DATA_WRITE"
#define SERIAL_VERIFY_DATA_READ   "SERIAL_VERIFY_DATA_READ"

/// Data verification parameters
enum ESerialVerifyData {
    eSerialVerifyData_Default = 0,   ///< use current default
    eSerialVerifyData_No,            ///< do not verify
    eSerialVerifyData_Never,         ///< never verify (even if set to verify later on)
    eSerialVerifyData_Yes,           ///< do verify
    eSerialVerifyData_Always,        ///< always verify (even if set not to later on)
    eSerialVerifyData_DefValue,      ///< initialize field with default
    eSerialVerifyData_DefValueAlways ///< initialize field with default
};

#define SERIAL_SKIP_UNKNOWN_MEMBERS    "SERIAL_SKIP_UNKNOWN_MEMBERS"
/// Skip unknown members parameters
enum ESerialSkipUnknown {
    eSerialSkipUnknown_Default = 0, ///< use current default
    eSerialSkipUnknown_No,          ///< do not skip (throw exception)
    eSerialSkipUnknown_Never,       ///< never skip (even if set to skip later on)
    eSerialSkipUnknown_Yes,         ///< do skip
    eSerialSkipUnknown_Always       ///< always skip (even if set not to later on)
};

/// File open flags
enum ESerialOpenFlags {
    eSerial_StdWhenEmpty = 1 << 0, ///< use std stream when filename is empty
    eSerial_StdWhenDash  = 1 << 1, ///< use std stream when filename is "-"
    eSerial_StdWhenStd   = 1 << 2, ///< use std when filename is "stdin"/"stdout"
    eSerial_StdWhenMask  = 15,
    eSerial_StdWhenAny   = eSerial_StdWhenMask,
    eSerial_UseFileForReread = 1 << 4
};
typedef int TSerialOpenFlags;

/// Type family
enum ETypeFamily {
    eTypeFamilyPrimitive,
    eTypeFamilyClass,
    eTypeFamilyChoice,
    eTypeFamilyContainer,
    eTypeFamilyPointer
};

/// Primitive value type
enum EPrimitiveValueType {
    ePrimitiveValueSpecial,        ///< null, void
    ePrimitiveValueBool,           ///< bool
    ePrimitiveValueChar,           ///< char
    ePrimitiveValueInteger,        ///< (signed|unsigned) (char|short|int|long)
    ePrimitiveValueReal,           ///< float|double
    ePrimitiveValueString,         ///< string|char*|const char*
    ePrimitiveValueEnum,           ///< enum
    ePrimitiveValueOctetString,    ///< vector<(signed|unsigned)? char>
    ePrimitiveValueBitString,      //
    ePrimitiveValueAny,
    ePrimitiveValueOther
};

enum EContainerType {
    eContainerVector,              ///< allows indexing & access to size
    eContainerList,                ///< only sequential access
    eContainerSet,
    eContainerMap
};


/// How to process non-printing character in the ASN VisibleString
enum EFixNonPrint {
    eFNP_Allow,            ///< pass through unchanged, post no error message
    eFNP_Replace,          ///< replace with '#' silently
    eFNP_ReplaceAndWarn,   ///< replace with '#', post an error of severity ERROR
    eFNP_Throw,            ///< replace with '#', throw an exception
    eFNP_Abort,            ///< replace with '#', post an error of severity FATAL

    eFNP_Default = eFNP_ReplaceAndWarn
};

/// String type
enum EStringType {
    eStringTypeVisible,  ///< VisibleString (in ASN.1 sense)
    eStringTypeUTF8      ///< UTF8-encoded string
};

/// How to assign and compare child sub-objects of serial objects
enum ESerialRecursionMode {
    eRecursive,            ///< Recursively
    eShallow,              ///< Assign/Compare pointers only
    eShallowChildless      ///< Set sub-object pointers to 0
};

/// Defines namespace qualification of XML tags
enum ENsQualifiedMode {
    eNSQNotSet,
    eNSUnqualified,
    eNSQualified
};

/// Type used for indexing class members and choice variants
typedef size_t TMemberIndex;

typedef int TEnumValueType;

/// Start if member indexing
const TMemberIndex kFirstMemberIndex = 1;
/// Special value returned from FindMember
const TMemberIndex kInvalidMember = kFirstMemberIndex - 1;
/// Special value for marking empty choice
const TMemberIndex kEmptyChoice = kInvalidMember;

typedef ssize_t TPointerOffsetType;


/* @} */


END_NCBI_SCOPE

#endif  /* SERIALDEF__HPP */
