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
* Revision 1.12  2000/01/05 19:43:57  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.11  1999/12/28 18:55:52  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.10  1999/12/17 19:05:04  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.9  1999/09/23 18:57:02  vasilche
* Fixed bugs with overloaded methods in objistr*.hpp & objostr*.hpp
*
* Revision 1.8  1999/09/14 18:54:20  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.7  1999/07/07 21:56:35  vasilche
* Fixed problem with static variables in templates on SunPro
*
* Revision 1.6  1999/07/07 21:15:03  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.5  1999/07/07 19:59:08  vasilche
* Reduced amount of data allocated on heap
* Cleaned ASN.1 structures info
*
* Revision 1.4  1999/06/30 16:05:05  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.3  1999/06/24 14:45:01  vasilche
* Added binary ASN.1 output.
*
* Revision 1.2  1999/06/04 20:51:49  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:57  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <serial/stdtypes.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

BEGIN_NCBI_SCOPE

#define INIT_TYPE_INFO(T) \
template<> \
TTypeInfo CStdTypeInfo<T>::GetTypeInfo(void) \
{ \
    static const TTypeInfo typeInfo = \
        new CStdTypeInfo<T>(typeid(T).name()); \
    return typeInfo; \
}

INIT_TYPE_INFO(bool)
INIT_TYPE_INFO(char)
INIT_TYPE_INFO(signed char)
INIT_TYPE_INFO(unsigned char)
INIT_TYPE_INFO(char*)
INIT_TYPE_INFO(const char*)
INIT_TYPE_INFO(short)
INIT_TYPE_INFO(unsigned short)
INIT_TYPE_INFO(int)
INIT_TYPE_INFO(unsigned)
INIT_TYPE_INFO(long)
INIT_TYPE_INFO(unsigned long)
INIT_TYPE_INFO(float)
INIT_TYPE_INFO(double)


CStdTypeInfo<void>::CStdTypeInfo(void)
    : CParent("void")
{
}

CStdTypeInfo<void>::CStdTypeInfo(const char* typeName)
    : CParent(typeName)
{
}

CStdTypeInfo<void>::~CStdTypeInfo(void)
{
}

size_t CStdTypeInfo<void>::GetSize(void) const
{
    return 0;
}

TTypeInfo CStdTypeInfo<void>::GetTypeInfo(void)
{
    static TTypeInfo typeInfo = new CStdTypeInfo;
    return typeInfo;
}

bool CStdTypeInfo<void>::IsDefault(TConstObjectPtr ) const
{
    return true;
}

bool CStdTypeInfo<void>::Equals(TConstObjectPtr , TConstObjectPtr ) const
{
    throw runtime_error("void cannot be compared");
}

void CStdTypeInfo<void>::SetDefault(TObjectPtr ) const
{
}

void CStdTypeInfo<void>::Assign(TObjectPtr , TConstObjectPtr ) const
{
    throw runtime_error("void cannot be assigned");
}

void CStdTypeInfo<void>::ReadData(CObjectIStream& , TObjectPtr ) const
{
    throw runtime_error("void cannot be read");
}
    
void CStdTypeInfo<void>::WriteData(CObjectOStream& , TConstObjectPtr ) const
{
    throw runtime_error("void cannot be written");
}

CStdTypeInfo<string>::CStdTypeInfo(void)
    : CParent("string")
{
}

CStdTypeInfo<string>::~CStdTypeInfo(void)
{
}

size_t CStdTypeInfo<string>::GetSize(void) const
{
    return TType::GetSize();
}

TObjectPtr CStdTypeInfo<string>::Create(void) const
{
    return new TObjectType();
}

bool CStdTypeInfo<string>::IsDefault(TConstObjectPtr object) const
{
    return Get(object).empty();
}

bool CStdTypeInfo<string>::Equals(TConstObjectPtr object1,
                                  TConstObjectPtr object2) const
{
    return Get(object1) == Get(object2);
}

void CStdTypeInfo<string>::SetDefault(TObjectPtr object) const
{
    Get(object).erase();
}

void CStdTypeInfo<string>::Assign(TObjectPtr dst,
                                  TConstObjectPtr src) const
{
    Get(dst) = Get(src);
}

TTypeInfo CStdTypeInfo<string>::GetTypeInfo(void)
{
    static TTypeInfo typeInfo = new CStdTypeInfo<string>;
    return typeInfo;
}

void CStdTypeInfo<string>::ReadData(CObjectIStream& in, TObjectPtr object) const
{
	in.ReadStd(Get(object));
}

void CStdTypeInfo<string>::WriteData(CObjectOStream& out, TConstObjectPtr object) const
{
	out.WriteStd(Get(object));
}

END_NCBI_SCOPE
