#ifndef STDTYPES__HPP
#define STDTYPES__HPP

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
* Revision 1.16  2000/01/10 19:46:34  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.15  2000/01/05 19:43:47  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.14  1999/12/28 18:55:40  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.13  1999/12/17 19:04:54  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.12  1999/09/22 20:11:51  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.11  1999/09/14 18:54:06  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.10  1999/08/13 15:53:45  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.9  1999/07/13 20:18:08  vasilche
* Changed types naming.
*
* Revision 1.8  1999/07/07 21:56:33  vasilche
* Fixed problem with static variables in templates on SunPro
*
* Revision 1.7  1999/07/07 21:15:03  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.6  1999/07/07 19:58:46  vasilche
* Reduced amount of data allocated on heap
* Cleaned ASN.1 structures info
*
* Revision 1.5  1999/06/30 16:04:37  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.4  1999/06/24 14:44:45  vasilche
* Added binary ASN.1 output.
*
* Revision 1.3  1999/06/09 18:39:00  vasilche
* Modified templates to work on Sun.
*
* Revision 1.2  1999/06/04 20:51:38  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:29  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/typeinfo.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

BEGIN_NCBI_SCOPE

class CObjectIStream;
class CObjectOStream;

// template to standard C types with default value 0 (int, double, char* etc.)
template<typename T>
class CStdTypeInfo : public CTypeInfo
{
    typedef CTypeInfo CParent;
    typedef T TObjectType;
    typedef CType<TObjectType> TType;
public:
    static TObjectType& Get(TObjectPtr object)
        {
            return TType::Get(object);
        }
    static const TObjectType& Get(TConstObjectPtr object)
        {
            return TType::Get(object);
        }

    virtual size_t GetSize(void) const
        {
            return TType::GetSize();
        }
    virtual TObjectPtr Create(void) const
        {
            return new TObjectType(0);
        }

    virtual bool IsDefault(TConstObjectPtr object) const
        {
            return Get(object) == 0;
        }

    virtual void SetDefault(TObjectPtr dst) const
        {
            Get(dst) = 0;
        }

    virtual bool Equals(TConstObjectPtr object1, TConstObjectPtr object2) const
        {
            return Get(object1) == Get(object2);
        }

    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const
        {
            Get(dst) = Get(src);
        }

    static TTypeInfo GetTypeInfo(void);

protected:
    CStdTypeInfo(const string& name)
        : CParent(name)
        {
        }
    CStdTypeInfo(const char* name)
        : CParent(name)
        {
        }

    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const
        {
            in.ReadStd(Get(object));
        }
    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const
        {
            out.WriteStd(Get(object));
        }
};

// CTypeInfo for C type void
template<>
class CStdTypeInfo<void> : public CTypeInfo
{
    typedef CTypeInfo CParent;
public:
    virtual size_t GetSize(void) const;

    static TTypeInfo GetTypeInfo(void);

    virtual bool IsDefault(TConstObjectPtr object) const;
    virtual bool Equals(TConstObjectPtr , TConstObjectPtr ) const;
    virtual void SetDefault(TObjectPtr dst) const;
    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const;

protected:
    virtual void ReadData(CObjectIStream& , TObjectPtr ) const;
    virtual void WriteData(CObjectOStream& , TConstObjectPtr ) const;

    CStdTypeInfo(void);
    CStdTypeInfo(const char* typeName);
    ~CStdTypeInfo(void);
};

// CTypeInfo for C++ STL type string
template<>
class CStdTypeInfo<string> : public CTypeInfo
{
    typedef CTypeInfo CParent;
    typedef string TObjectType;
    typedef CType<TObjectType> TType;
public:
    CStdTypeInfo(void);
    CStdTypeInfo(const char* typeName);
    ~CStdTypeInfo(void);

    static TObjectType& Get(TObjectPtr object)
        {
            return TType::Get(object);
        }
    static const TObjectType& Get(TConstObjectPtr object)
        {
            return TType::Get(object);
        }

    virtual size_t GetSize(void) const;
    virtual TObjectPtr Create(void) const;
    
    virtual bool IsDefault(TConstObjectPtr object) const;
    virtual bool Equals(TConstObjectPtr , TConstObjectPtr ) const;
    virtual void SetDefault(TObjectPtr dst) const;
    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const;

    static TTypeInfo GetTypeInfo(void);

protected:
    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const;
    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const;
};

class CStringStoreTypeInfo : public CStdTypeInfo<string>
{
    typedef CStdTypeInfo<string> CParent;
public:
    CStringStoreTypeInfo(void);
    ~CStringStoreTypeInfo(void);

    static TTypeInfo GetTypeInfo(void);

protected:
    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const;
    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const;
};

class CNullBoolTypeInfo : public CStdTypeInfo<bool>
{
    typedef CStdTypeInfo<bool> CParent;
public:
    CNullBoolTypeInfo(void);
    ~CNullBoolTypeInfo(void);

    static TTypeInfo GetTypeInfo(void);

protected:
    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const;
    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const;
};

//#include <serial/stdtypes.inl>

END_NCBI_SCOPE

#endif  /* STDTYPES__HPP */
