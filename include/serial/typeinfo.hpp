#ifndef TYPEINFO__HPP
#define TYPEINFO__HPP

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
* Revision 1.1  1999/05/19 19:56:32  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <map>

BEGIN_NCBI_SCOPE

class CObjectIStream;
class CObjectOStream;
class CObjectList;

class CTypeInfo
{
public:
    typedef const CTypeInfo* TTypeInfo;
    typedef map<string, TTypeInfo> TTypes;
    typedef void* TObjectPtr;
    typedef const void* TConstObjectPtr;

    CTypeInfo(const string& name);
    virtual ~CTypeInfo(void);

    const string& GetName(void) const
        { return m_Name; }

    // finds type info (throws runtime_error if absent)
    static TTypeInfo GetTypeInfo(const string& name);

    // returns type info of pointer to this type
    TTypeInfo GetPointerTypeInfo(void) const;

    virtual size_t GetSize(void) const = 0;

    // creates object of this type in heap (can be deleted by operator delete)
    virtual TObjectPtr Create(void) const = 0;

protected:

    virtual CTypeInfo* CreatePointerTypeInfo(void);

    friend class CObjectOStream;
    friend class CObjectIStream;

    // read object
    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const = 0;

    // collect info about all memory chunks for writing
    static void AddObject(CObjectList& list,
                          TConstObjectPtr object, TTypeInfo typeInfo);

    void CollectObjects(CObjectList& list, TConstObjectPtr object) const
        {
            AddObject(list, object, this);
        }

    virtual void CollectMembers(CObjectList& list, TConstObjectPtr object) const;

    // write object
    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const = 0;

private:
    const string m_Name;

    mutable auto_ptr<CTypeInfo> m_PointerTypeInfo;

    static TTypes sm_Types;
};

// define type info getter for user classes
template<typename CLASS>
inline
CTypeInfo::TTypeInfo GetTypeInfo(const CLASS& object)
{
    return CTypeInfo::GetTypeInfo(typeid(object).name());
}

class CPointerTypeInfo : public CTypeInfo
{
public:
    CPointerTypeInfo(TTypeInfo dataTypeInfo)
        : CTypeInfo('*' + dataTypeInfo->GetName()), m_DataTypeInfo(dataTypeInfo)
        { }

    TTypeInfo GetDataTypeInfo(void) const
        { return m_DataTypeInfo; }

    virtual size_t GetSize(void) const;

    virtual TObjectPtr Create(void) const;

protected:
    virtual void CollectMembers(CObjectList& list, TConstObjectPtr object) const;

    virtual void WriteData(CObjectOStream& out, TConstObjectPtr obejct) const;

    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const;

    virtual TTypeInfo GetRealDataTypeInfo(TConstObjectPtr object) const;

private:
    const TTypeInfo m_DataTypeInfo;
};

template<class CLASS>
class CClassPointerTypeInfo : public CPointerTypeInfo
{
public:
    CClassPointerType(TTypeInfo dataTypeInfo) const
        : CPointerTypeInfo(dataTypeInfo)
        {
        }

protected:
    virtual TTypeInfo GetRealDataTypeInfo(TConstObjectPtr object) const
        {
            return GetTypeInfo(*static_cast<const CLASS*>(object));
        }
};

template<typename CLASS>
inline
CTypeInfo::TTypeInfo GetTypeInfo(const CLASS* object)
{
    return GetTypeInfo(*object).GetPointerTypeInfo();
}

// this macro is used to do something for all basic C types
#define FOR_ALL_STD_TYPES \
PROCESS_STD_TYPE(char) \
PROCESS_STD_TYPE(unsigned char) \
PROCESS_STD_TYPE(signed char) \
PROCESS_STD_TYPE(short) \
PROCESS_STD_TYPE(unsigned short) \
PROCESS_STD_TYPE(int) \
PROCESS_STD_TYPE(unsigned int) \
PROCESS_STD_TYPE(long) \
PROCESS_STD_TYPE(unsigned long) \
PROCESS_STD_TYPE(float) \
PROCESS_STD_TYPE(double)

#include <serial/typeinfo.inl>

END_NCBI_SCOPE

#endif  /* TYPEINFO__HPP */
