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

template<typename T>
class CStdTypeInfoTmpl : public CTypeInfo
{
public:
    typedef T TObjectType;

    static TObjectType& Get(TObjectPtr object)
        {
            return *static_cast<TObjectType*>(object);
        }
    static const TObjectType& Get(TConstObjectPtr object)
        {
            return *static_cast<const TObjectType*>(object);
        }

    virtual size_t GetSize(void) const
        {
            return sizeof(TObjectType);
        }

    virtual bool Equals(TConstObjectPtr object1, TConstObjectPtr object2) const
        {
            return Get(object1) == Get(object2);
        }

    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const
        {
            Get(dst) = Get(src);
        }

protected:
    CStdTypeInfoTmpl()
        : CTypeInfo(typeid(TObjectType))
        {
        }
};

template<typename T>
class CStdTypeInfo : public CStdTypeInfoTmpl<T>
{
public:
    virtual TObjectPtr Create(void) const
        {
            return new TObjectType(0);
        }

    virtual TConstObjectPtr GetDefault(void) const
        {
            static TObjectType def = 0;
            return &def;
        }

    static TTypeInfo GetTypeInfo(void)
        {
            static TTypeInfo typeInfo = new CStdTypeInfo;
            return typeInfo;
        }

protected:
    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const
        {
            in.ReadStd(Get(object));
        }
    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const
        {
            out.WriteStd(Get(object));
        }
};

template<>
class CStdTypeInfo<void> : public CTypeInfo
{
public:
    virtual size_t GetSize(void) const;

    static TTypeInfo GetTypeInfo(void);

    virtual bool Equals(TConstObjectPtr , TConstObjectPtr ) const;

    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const;

protected:
    virtual void ReadData(CObjectIStream& , TObjectPtr ) const;
    virtual void WriteData(CObjectOStream& , TConstObjectPtr ) const;

private:
    CStdTypeInfo(void)
        : CTypeInfo(typeid(void))
        {
        }
};

template<>
class CStdTypeInfo<char*> : public CStdTypeInfoTmpl<char*>
{
public:
    virtual TConstObjectPtr GetDefault(void) const;

    static TTypeInfo GetTypeInfo(void);

protected:
    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const;
    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const;
};

template<>
class CStdTypeInfo<const char*> : public CStdTypeInfoTmpl<const char*>
{
public:
public:
    virtual TConstObjectPtr GetDefault(void) const;

    static TTypeInfo GetTypeInfo(void);

protected:
    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const;
    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const;
};

template<>
class CStdTypeInfo<string> : public CStdTypeInfoTmpl<string>
{
public:
    virtual TObjectPtr Create(void) const;
    
    virtual TConstObjectPtr GetDefault(void) const;

    static TTypeInfo GetTypeInfo(void);

protected:
    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const;
    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const;
};

#include <serial/stdtypes.inl>

END_NCBI_SCOPE

#endif  /* STDTYPES__HPP */
