#ifndef SERIAL__HPP
#define SERIAL__HPP

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
*   Serialization classes.
*/

#include <serial/typeref.hpp>


/** @addtogroup UserCodeSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CObjectOStream;
class CObjectIStream;

NCBI_XSERIAL_EXPORT
TTypeInfo CPointerTypeInfoGetTypeInfo(TTypeInfo type);

// define type info getter for classes
template<class Class>
inline TTypeInfoGetter GetTypeInfoGetter(const Class* object);


// define type info getter for pointers
template<typename T>
inline
CTypeRef GetPtrTypeRef(const T* const* object)
{
    const T* p = 0;
    return CTypeRef(&CPointerTypeInfoGetTypeInfo, GetTypeInfoGetter(p));
}

// define type info getter for user classes
template<class Class>
inline
TTypeInfoGetter GetTypeInfoGetter(const Class* )
{
    return &Class::GetTypeInfo;
}

template<typename T>
inline
TTypeInfoGetter GetTypeRef(const T* object)
{
    return GetTypeInfoGetter(object);
}

NCBI_XSERIAL_EXPORT
void Write(CObjectOStream& out, TConstObjectPtr object, const CTypeRef& type);

NCBI_XSERIAL_EXPORT
void Read(CObjectIStream& in, TObjectPtr object, const CTypeRef& type);

// reader/writer
template<typename T>
inline
CObjectOStream& Write(CObjectOStream& out, const T& object)
{
    Write(out, &object, GetTypeRef(&object));
    return out;
}

template<typename T>
inline
CObjectIStream& Read(CObjectIStream& in, T& object)
{
    Read(in, &object, GetTypeRef(&object));
    return in;
}

template<typename T>
inline
CObjectOStream& operator<<(CObjectOStream& out, const T& object)
{
    return Write(out, object);
}

template<typename T>
inline
CObjectIStream& operator>>(CObjectIStream& in, T& object)
{
    return Read(in, object);
}


/* @} */


//#include <serial/serial.inl>

END_NCBI_SCOPE

#endif  /* SERIAL__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.47  2004/08/17 14:39:06  dicuccio
* Added export specifiers
*
* Revision 1.46  2003/04/15 16:18:44  siyan
* Added doxygen support
*
* Revision 1.45  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.44  2002/08/26 18:32:28  grichenk
* Added Get/SetAutoSeparator() to CObjectOStream to control
* output of separators.
*
* Revision 1.43  2002/08/23 16:50:39  grichenk
* Write separator after every object
*
* Revision 1.42  2001/05/17 14:59:47  lavr
* Typos corrected
*
* Revision 1.41  1999/12/17 19:04:53  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.40  1999/11/22 21:04:33  vasilche
* Cleaned main interface headers. Now generated files should include serial/serialimpl.hpp and user code should include serial/serial.hpp which became might lighter.
*
* Revision 1.39  1999/11/19 15:47:20  vasilche
* Added specification of several templates with <char*> and <const char*> to
* avoid warnings on MAC
*
* Revision 1.38  1999/11/16 15:40:14  vasilche
* Added plain pointer choice.
*
* Revision 1.37  1999/10/25 19:07:13  vasilche
* Fixed coredump on non initialized choices.
* Fixed compilation warning.
*
* Revision 1.36  1999/10/19 13:21:59  vasilche
* Avoid macros names conflict.
*
* Revision 1.35  1999/10/18 20:11:16  vasilche
* Enum values now have long type.
* Fixed template generation for enums.
*
* Revision 1.34  1999/10/15 16:37:47  vasilche
* Added namespace specifiers.
*
* Revision 1.33  1999/10/15 13:58:43  vasilche
* Added namespace specifiers in definition of all macros.
*
* Revision 1.32  1999/10/04 19:39:45  vasilche
* Fixed bug in CObjectOStreamBinary.
* Start using of BSRead/BSWrite.
* Added ASNCALL macro for prototypes of old ASN.1 functions.
*
* Revision 1.31  1999/10/04 16:22:10  vasilche
* Fixed bug with old ASN.1 structures.
*
* Revision 1.30  1999/09/24 19:01:17  vasilche
* Removed dependency on NCBI toolkit.
*
* Revision 1.29  1999/09/14 18:54:05  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.28  1999/09/08 20:31:18  vasilche
* Added BEGIN_CLASS_INFO3 macro
*
* Revision 1.27  1999/09/07 20:57:44  vasilche
* Forgot to add some files.
*
* Revision 1.26  1999/09/01 17:38:02  vasilche
* Fixed vector<char> implementation.
* Added explicit naming of class info.
* Moved IMPLICIT attribute from member info to class info.
*
* Revision 1.25  1999/08/31 17:50:04  vasilche
* Implemented several macros for specific data types.
* Added implicit members.
* Added multimap and set.
*
* Revision 1.24  1999/08/13 15:53:45  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.23  1999/07/21 14:20:01  vasilche
* Added serialization of bool.
*
* Revision 1.22  1999/07/20 18:22:56  vasilche
* Added interface to old ASN.1 routines.
* Added fixed choice of subclasses to use for pointers.
*
* Revision 1.21  1999/07/19 15:50:19  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.20  1999/07/15 16:54:43  vasilche
* Implemented vector<X> & vector<char> as special case.
*
* Revision 1.19  1999/07/14 18:58:04  vasilche
* Fixed ASN.1 types/field naming.
*
* Revision 1.18  1999/07/13 20:54:05  vasilche
* Fixed minor bugs.
*
* Revision 1.17  1999/07/13 20:18:07  vasilche
* Changed types naming.
*
* Revision 1.16  1999/07/09 16:32:54  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.15  1999/07/07 18:18:32  vasilche
* Fixed some bugs found by MS VC++
*
* Revision 1.14  1999/06/30 18:54:54  vasilche
* Fixed some errors under MSVS
*
* Revision 1.13  1999/06/30 16:04:35  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.12  1999/06/24 14:44:43  vasilche
* Added binary ASN.1 output.
*
* Revision 1.11  1999/06/17 18:38:49  vasilche
* Fixed order of members in class.
* Added checks for overlapped members.
*
* Revision 1.10  1999/06/16 20:58:04  vasilche
* Added GetPtrTypeRef to avoid conflict in MSVS.
*
* Revision 1.9  1999/06/15 16:20:06  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.8  1999/06/11 19:15:49  vasilche
* Working binary serialization and deserialization of first test object.
*
* Revision 1.7  1999/06/09 19:58:31  vasilche
* Added specialized templates for compilation in MS VS
*
* Revision 1.6  1999/06/09 18:38:58  vasilche
* Modified templates to work on Sun.
*
* Revision 1.5  1999/06/07 20:42:58  vasilche
* Fixed compilation under MS VS
*
* Revision 1.4  1999/06/07 19:30:20  vasilche
* More bug fixes
*
* Revision 1.3  1999/06/04 20:51:37  vasilche
* First compilable version of serialization.
*
* Revision 1.2  1999/05/19 19:56:28  vasilche
* Commit just in case.
*
* Revision 1.1  1999/03/25 19:11:58  vasilche
* Beginning of serialization library.
*
* ===========================================================================
*/
