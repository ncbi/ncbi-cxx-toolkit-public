#ifndef DATA_LOADER_FACTORY__HPP
#define DATA_LOADER_FACTORY__HPP

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
* Author:
*
* File Description:
*
*/


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataLoader;

/////////////////////////////////////////////////////////////////////////////

class NCBI_XOBJMGR_EXPORT CDataLoaderFactory : public CObject
{
public:
    CDataLoaderFactory(const string& name)
        : m_LoaderName(name) {}
    virtual ~CDataLoaderFactory() {}

    virtual CDataLoader* Create(void) = 0;

    const string& GetName(void) {return m_LoaderName;}

private:
    string m_LoaderName;
};


template  <class TDataLoader>
class NCBI_XOBJMGR_EXPORT CSimpleDataLoaderFactory : public CDataLoaderFactory
{
public:
    CSimpleDataLoaderFactory(const string& name)
        : CDataLoaderFactory(name) {
        return;
    }
    virtual ~CSimpleDataLoaderFactory() {
        return;
    }

    virtual CDataLoader* Create(void) {
        return new TDataLoader(GetName());
    } 
};

/////////////////////////////////////////////////////////////////////////////
typedef CObject* (*TFACTORY_AUTOCREATE)();

#define DECLARE_AUTOCREATE(class_name) \
    static class_name* AutoCreate##class_name() \
    { return new class_name;}

#define CLASS_AUTOCREATE(class_name) \
    (reinterpret_cast<TFACTORY_AUTOCREATE>(&(class_name::AutoCreate##class_name)))

#define CREATE_AUTOCREATE(class_name, factory) \
    (dynamic_cast<class_name*>((*factory)()))


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2002/12/26 20:51:35  dicuccio
* Added Win32 export specifier
*
* Revision 1.2  2002/07/08 20:50:56  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.1  2002/01/11 19:04:00  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#endif // DATA_LOADER_FACTORY__HPP
