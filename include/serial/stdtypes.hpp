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

template<class TYPE>
class CStdTypeInfoTmpl : public CTypeInfo
{
public:
    typedef TYPE TObjectType;

    static TObjectType& Get(TObjectPtr object)
        { return *static_cast<TObjectType*>(object); }
    static const TObjectType& Get(TConstObjectPtr object)
        { return *static_cast<const TObjectType*>(object); }

    virtual size_t GetSize(void) const
        { return sizeof(TObjectType); }
};

template<class TYPE>
class CStdTypeInfo : public CStdTypeInfoTmpl<TYPE>
{
public:
    virtual TObjectPtr Create(void) const
        { return new TObjectType(0); }

    static const CStdTypeInfo sm_TypeInfo;

protected:
    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const
        { in.ReadStd(Get(object)); }

    virtual void WriteData(CObjectOStream& out, TConstObjectPtr obejct) const
        { out.WriteStd(Get(object)); }
};

template<>
class CStdTypeInfo<void> : public CTypeInfo
{
public:
    virtual size_t GetSize(void) const
        { return 0; }

    virtual TObjectPtr Create(void) const
        { throw runtime_error("void cannot be created"); }
    
    static const CStdTypeInfo<void> sm_TypeInfo;

protected:
    virtual void ReadData(CObjectIStream& , TObjectPtr ) const
        { throw runtime_error("void cannot be read"); }
    
    virtual void WriteData(CObjectOStream& , TConstObjectPtr ) const
        { throw runtime_error("void cannot be written"); }
};

template<>
class CStdTypeInfo<string> : public CStdTypeInfoTmpl<string>
{
public:
    virtual TObjectPtr Create(void) const
        { return new TObjectType(); }

    static const CStdTypeInfo<string> sm_TypeInfo;

protected:
    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const
        { in.ReadStd(Get(object)); }
    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const
        { out.WriteStd(Get(object)); }
};

/*
template<>
class CStdTypeInfo<char*> : public CStdTypeInfoTmpl<char*>
{
public:
    virtual TObjectPtr Create(void) const
        { return new TObjectType(0); }

    static const CStdTypeInfo<char*> sm_TypeInfo;

protected:
    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const
        { in.ReadCString(Get(object)); }
    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const
        { out.WriteCString(Get(object)); }
};
*/

#include <serial/stdtypes.inl>

END_NCBI_SCOPE

#endif  /* STDTYPES__HPP */
