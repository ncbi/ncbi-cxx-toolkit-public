#ifndef PTRINFO__HPP
#define PTRINFO__HPP

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
* Revision 1.8  1999/08/13 15:53:44  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.7  1999/07/20 18:22:55  vasilche
* Added interface to old ASN.1 routines.
* Added fixed choice of subclasses to use for pointers.
*
* Revision 1.6  1999/07/13 20:18:06  vasilche
* Changed types naming.
*
* Revision 1.5  1999/07/07 19:58:46  vasilche
* Reduced amount of data allocated on heap
* Cleaned ASN.1 structures info
*
* Revision 1.4  1999/06/30 16:04:34  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.3  1999/06/24 14:44:42  vasilche
* Added binary ASN.1 output.
*
* Revision 1.2  1999/06/15 16:20:05  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.1  1999/06/04 20:51:36  vasilche
* First compilable version of serialization.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/typeinfo.hpp>
#include <serial/typeref.hpp>
#include <serial/typemap.hpp>

BEGIN_NCBI_SCOPE

class CPointerTypeInfo : public CTypeInfo
{
public:
    typedef void* TObjectType;

    static TObjectPtr& GetObject(TObjectPtr object)
        { return *static_cast<TObjectPtr*>(object); }
    static const TConstObjectPtr& GetObject(TConstObjectPtr object)
        { return *static_cast<const TConstObjectPtr*>(object); }

    CPointerTypeInfo(TTypeInfo type)
        : CTypeInfo(type->GetName() + '*'), m_DataType(type)
        { }
    CPointerTypeInfo(const string& name, TTypeInfo type)
        : CTypeInfo(name), m_DataType(type)
        { }

    static TTypeInfo GetTypeInfo(TTypeInfo base)
        {
            return sm_Map.GetTypeInfo(base);
        }

    TTypeInfo GetDataTypeInfo(void) const
        {
            return m_DataType;
        }

    virtual size_t GetSize(void) const;

    virtual TObjectPtr Create(void) const;

    virtual TConstObjectPtr GetDefault(void) const;

    virtual bool Equals(TConstObjectPtr object1, TConstObjectPtr object2) const;

    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const;

protected:
    virtual void CollectExternalObjects(COObjectList& list,
                                        TConstObjectPtr object) const;

    virtual void WriteData(CObjectOStream& out, TConstObjectPtr obejct) const;

    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const;

private:
    TTypeInfo m_DataType;

    static CTypeInfoMap<CPointerTypeInfo> sm_Map;
};

class CAutoPointerTypeInfo : public CPointerTypeInfo {
public:
    CAutoPointerTypeInfo(TTypeInfo type)
        : CPointerTypeInfo(type->GetName(), type)
        { }

    static TTypeInfo GetTypeInfo(TTypeInfo base)
        {
            return sm_Map.GetTypeInfo(base);
        }

protected:
    
    void WriteData(CObjectOStream& out, TConstObjectPtr object) const;

    void ReadData(CObjectIStream& in, TObjectPtr object) const;

private:
    static CTypeInfoMap<CAutoPointerTypeInfo> sm_Map;
};

//#include <ptrinfo.inl>

END_NCBI_SCOPE

#endif  /* PTRINFO__HPP */
