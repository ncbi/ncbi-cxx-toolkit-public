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
* Revision 1.1  1999/06/04 20:51:36  vasilche
* First compilable version of serialization.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/typeinfo.hpp>
#include <serial/typeref.hpp>

BEGIN_NCBI_SCOPE

class CPointerTypeInfo : public CTypeInfo
{
public:
    typedef void* TObjectType;

    static TObjectPtr& GetObject(TObjectPtr object)
        { return *static_cast<TObjectPtr*>(object); }
    static const TConstObjectPtr& GetObject(TConstObjectPtr object)
        { return *static_cast<const TConstObjectPtr*>(object); }

    CPointerTypeInfo(const type_info& id, const CTypeRef& typeRef)
        : CTypeInfo(id), m_DataTypeRef(typeRef)
        { }

    TTypeInfo GetDataTypeInfo(void) const
        {
            return m_DataTypeRef.Get();
        }

    TTypeInfo GetRealDataTypeInfo(TConstObjectPtr object) const
        {
            return GetDataTypeInfo()->GetRealTypeInfo(GetObject(object));
        }


    virtual size_t GetSize(void) const;

    virtual TObjectPtr Create(void) const;

    virtual bool IsDefault(TConstObjectPtr object) const;

protected:
    virtual void CollectObjects(COObjectList& list, TConstObjectPtr object) const;

    virtual void WriteData(CObjectOStream& out, TConstObjectPtr obejct) const;

    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const;

private:
    CTypeRef m_DataTypeRef;
};

//#include <ptrinfo.inl>

END_NCBI_SCOPE

#endif  /* PTRINFO__HPP */
