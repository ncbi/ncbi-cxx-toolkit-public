#ifndef OBJECT__HPP
#define OBJECT__HPP

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
* Revision 1.4  2000/03/29 15:55:20  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.3  2000/03/07 14:05:30  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
*
* Revision 1.2  1999/08/13 20:22:57  vasilche
* Fixed lot of bugs in datatool
*
* Revision 1.1  1999/08/13 15:53:43  vasilche
* C++ analog of asntool: datatool
*
* ===========================================================================
*/

#include <corelib/ncbiobj.hpp>
#include <serial/serialdef.hpp>
#include <memory>

BEGIN_NCBI_SCOPE

class CObjectTypeInfo;
class CConstObjectInfo;
class CObjectInfo;

class CObjectTypeInfo {
protected:
    CObjectTypeInfo(TTypeInfo typeinfo = 0)
        : m_TypeInfo(typeinfo)
        {
        }
    
    void ResetTypeInfo(void)
        {
            m_TypeInfo = 0;
        }
    void SetTypeInfo(TTypeInfo typeinfo)
        {
            m_TypeInfo = typeinfo;
        }

public:
    TTypeInfo GetTypeInfo(void) const
        {
            return m_TypeInfo;
        }

    static bool IsCObject(TTypeInfo typeInfo);

private:
    TTypeInfo m_TypeInfo;
};

class CObjectInfo : public CObjectTypeInfo {
public:
    typedef NCBI_NS_NCBI::TObjectPtr TObjectPtr;
    typedef NCBI_NS_NCBI::TConstObjectPtr TConstObjectPtr;

    // create empty CObjectInfo
    CObjectInfo(void)
        : m_ObjectPtr(0)
        {
        }
    // create CObjectInfo with new object
    CObjectInfo(TTypeInfo typeInfo);
    // initialize CObjectInfo
    CObjectInfo(pair<TObjectPtr, TTypeInfo> object);
    CObjectInfo(TObjectPtr objectPtr, TTypeInfo typeInfo);


    // reset CObjectInfo to empty state
    void Reset(void);
    // set CObjectInfo
    void Set(pair<TObjectPtr, TTypeInfo> object);
    void Set(TObjectPtr objectPtr, TTypeInfo typeInfo);
    void Set(const CObjectInfo& object);

    // check if CObjectInfo initialized with valid object
    operator bool(void) const
        {
            return m_ObjectPtr != 0;
        }

    // get pointer to object
    TObjectPtr GetObjectPtr(void)
        {
            return m_ObjectPtr;
        }
    TConstObjectPtr GetObjectPtr(void) const
        {
            return m_ObjectPtr;
        }
    const CRef<CObject>& GetObjectRef(void) const
        {
            return m_Ref;
        }

private:
    // set m_Ref field for reference counted CObject
    static CObject* GetCObjectPtr(TObjectPtr objectPtr, TTypeInfo typeInfo);

    TObjectPtr m_ObjectPtr; // object pointer
    CRef<CObject> m_Ref;    // hold reference to CObject for correct removal
};

class CConstObjectInfo : public CObjectTypeInfo {
public:
    typedef NCBI_NS_NCBI::TConstObjectPtr TObjectPtr;

    // create empty CObjectInfo
    CConstObjectInfo(void)
        : m_ObjectPtr(0)
        {
        }
    // initialize CObjectInfo
    CConstObjectInfo(TObjectPtr objectPtr, TTypeInfo typeInfo);
    CConstObjectInfo(NCBI_NS_NCBI::TObjectPtr objectPtr, TTypeInfo typeInfo);
    CConstObjectInfo(pair<TObjectPtr, TTypeInfo> object);
    CConstObjectInfo(pair<NCBI_NS_NCBI::TObjectPtr, TTypeInfo> object);
    CConstObjectInfo(const CObjectInfo& object);

    // reset CObjectInfo to empty state
    void Reset(void);
    // set CObjectInfo
    void Set(TObjectPtr objectPtr, TTypeInfo typeInfo);
    void Set(NCBI_NS_NCBI::TObjectPtr objectPtr, TTypeInfo typeInfo);
    void Set(pair<TObjectPtr, TTypeInfo> object);
    void Set(pair<NCBI_NS_NCBI::TObjectPtr, TTypeInfo> object);
    void Set(const CConstObjectInfo& object);
    void Set(const CObjectInfo& object);

    // check if CObjectInfo initialized with valid object
    operator bool(void) const
        {
            return m_ObjectPtr != 0;
        }

    // get pointer to object
    TObjectPtr GetObjectPtr(void) const
        {
            return m_ObjectPtr;
        }

private:
    // set m_Ref field for reference counted CObject
    static const CObject* GetCObjectPtr(TObjectPtr objectPtr,
                                        TTypeInfo typeInfo);

    TObjectPtr m_ObjectPtr;   // object pointer
    CConstRef<CObject> m_Ref; // hold reference to CObject for correct removal
};

#include <serial/object.inl>

END_NCBI_SCOPE

#endif  /* OBJECT__HPP */
