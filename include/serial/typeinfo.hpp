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
* Revision 1.6  1999/06/15 16:20:09  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.5  1999/06/10 21:06:42  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.4  1999/06/07 19:59:38  vasilche
* offset_t -> size_t
*
* Revision 1.3  1999/06/07 19:30:21  vasilche
* More bug fixes
*
* Revision 1.2  1999/06/04 20:51:40  vasilche
* First compilable version of serialization.
*
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
class CClassInfoTmpl;
class COObjectList;
class CTypeRef;
class CMemberInfo;

class CTypeInfoOrder
{
public:
    bool operator()(const type_info* const i1, const type_info* const i2) const
        { return i1->before(*i2); }
};

class CTypeInfo
{
public:
    typedef const CTypeInfo* TTypeInfo;
    typedef void* TObjectPtr;
    typedef const void* TConstObjectPtr;

    string GetName(void) const
        { return m_Name; }

    // finds type info (throws runtime_error if absent)
    static TTypeInfo GetTypeInfoByName(const string& name);
    static TTypeInfo GetTypeInfoById(const type_info& id);
    static TTypeInfo GetTypeInfoBy(const type_info& id, void (*creator)(void));

    // returns type info of pointer to this type
    static TTypeInfo GetPointerTypeInfo(const type_info& id,
                                        const CTypeRef& typeRef);

    virtual size_t GetSize(void) const = 0;

    static TObjectPtr Add(TObjectPtr object, size_t offset)
        { return static_cast<char*>(object) + offset; }
    static TConstObjectPtr Add(TConstObjectPtr object, size_t offset)
        { return static_cast<const char*>(object) + offset; }
    static size_t Sub(TConstObjectPtr first, TConstObjectPtr second)
        { return static_cast<const char*>(first) -
              static_cast<const char*>(second); }

    TObjectPtr EndOf(TObjectPtr object) const
        { return Add(object, GetSize()); }
    TConstObjectPtr EndOf(TConstObjectPtr object) const
        { return Add(object, GetSize()); }

    // creates object of this type in heap (can be deleted by operator delete)
    virtual TObjectPtr Create(void) const = 0;

    virtual bool IsDefault(TConstObjectPtr object) const;

    virtual TTypeInfo GetRealTypeInfo(TConstObjectPtr ) const
        {
            return this;
        }

    virtual ~CTypeInfo(void);

    virtual const CMemberInfo* FindMember(const string& name) const;
    virtual const CMemberInfo* LocateMember(TConstObjectPtr object,
                                            TConstObjectPtr member,
                                            TTypeInfo memberTypeInfo) const;

    // collect info about all memory chunks for writing
    void CollectObjects(COObjectList& objectList,
                        TConstObjectPtr object) const;
    virtual void CollectExternalObjects(COObjectList& list,
                                        TConstObjectPtr object) const;

protected:

    CTypeInfo(void);
    CTypeInfo(const type_info& id);

    friend class CObjectOStream;
    friend class CObjectIStream;
    friend class CClassInfoTmpl;

    // read object
    virtual void ReadData(CObjectIStream& in,
                          TObjectPtr object) const = 0;

    // write object
    virtual void WriteData(CObjectOStream& out,
                           TConstObjectPtr object) const = 0;

private:
    typedef map<string, TTypeInfo> TTypesByName;
    typedef map<const type_info*, TTypeInfo, CTypeInfoOrder> TTypesById;

    const type_info& m_Id;
    string m_Name;

    static TTypesByName* sm_TypesByName;
    static TTypesById* sm_TypesById;

    static TTypesById& TypesById(void);
    static TTypesByName& TypesByName(void);
};

#include <serial/typeinfo.inl>

END_NCBI_SCOPE

#endif  /* TYPEINFO__HPP */
