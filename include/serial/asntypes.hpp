#ifndef ASNTYPES__HPP
#define ASNTYPES__HPP

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
* Revision 1.2  1999/06/30 18:54:53  vasilche
* Fixed some errors under MSVS
*
* Revision 1.1  1999/06/30 16:04:18  vasilche
* Added support for old ASN.1 structures.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/typeinfo.hpp>
#include <serial/typeref.hpp>
#include <serial/memberid.hpp>
#include <vector>

BEGIN_NCBI_SCOPE

#define NAME2(n1, n2) n1##n2
#define NAME3(n1, n2, n3) n1##n2##n3

#define ASN_TYPE_INFO_GETTER_NAME(name) NAME2(GetTypeInfo_struct_, name)
#define ASN_STRUCT_NAME(name) NAME2(struct_, name)

#define ASN_TYPE_INFO_GETTER_DECL(name) \
BEGIN_NCBI_SCOPE \
const CTypeInfo* ASN_TYPE_INFO_GETTER_NAME(name)(void); \
END_NCBI_SCOPE

#define ASN_TYPE_REF(name) \
struct ASN_STRUCT_NAME(name); \
ASN_TYPE_INFO_GETTER_DECL(name) \
BEGIN_NCBI_SCOPE \
inline CTypeRef GetTypeRef(const ASN_STRUCT_NAME(name)* ) \
{ return CTypeRef(typeid(void), ASN_TYPE_INFO_GETTER_NAME(name)); } \
END_NCBI_SCOPE

#define ASN_CHOICE_REF(name) \
ASN_TYPE_INFO_GETTER_DECL(name)

class CSequenceTypeInfo : public CTypeInfo {
public:
    CSequenceTypeInfo(const CTypeRef& typeRef);

    static TObjectPtr& Get(TObjectPtr object)
        {
            return *static_cast<TObjectPtr*>(object);
        }
    static const TConstObjectPtr& Get(TConstObjectPtr object)
        {
            return *static_cast<const TObjectPtr*>(object);
        }

    virtual size_t GetSize(void) const;

    virtual TObjectPtr Create(void) const;

    TTypeInfo GetDataTypeInfo(void) const
        {
            return m_DataType.Get();
        }

    virtual bool Equals(TConstObjectPtr object1,
                        TConstObjectPtr object2) const;

    virtual void Assign(TObjectPtr dst,
                        TConstObjectPtr src) const;

protected:

    virtual void CollectExternalObjects(COObjectList& list,
                                        TConstObjectPtr object) const;

    virtual void WriteData(CObjectOStream& out,
                           TConstObjectPtr obejct) const;

    virtual void ReadData(CObjectIStream& in,
                          TObjectPtr object) const;

private:
    CTypeRef m_DataType;
};

class CSetTypeInfo : public CSequenceTypeInfo {
public:
    CSetTypeInfo(const CTypeRef& typeRef);
};

class CSequenceOfTypeInfo : public CSequenceTypeInfo {
public:
    CSequenceOfTypeInfo(const CTypeRef& typeRef);

    virtual TObjectPtr Create(void) const;

    virtual bool Equals(TConstObjectPtr object1,
                        TConstObjectPtr object2) const;

    virtual void Assign(TObjectPtr dst,
                        TConstObjectPtr src) const;

protected:

    virtual void CollectExternalObjects(COObjectList& list,
                                        TConstObjectPtr object) const;

    virtual void WriteData(CObjectOStream& out,
                           TConstObjectPtr obejct) const;

    virtual void ReadData(CObjectIStream& in,
                          TObjectPtr object) const;
};

class CSetOfTypeInfo : public CSequenceOfTypeInfo {
public:
    CSetOfTypeInfo(const CTypeRef& typeRef);
};

class CChoiceTypeInfo : public CTypeInfo {
public:
    CChoiceTypeInfo(const CTypeRef& choices);
    ~CChoiceTypeInfo(void);

    virtual size_t GetSize(void) const;

    virtual TObjectPtr Create(void) const;

    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const;

    virtual bool Equals(TConstObjectPtr obj1, TConstObjectPtr obj2) const;

protected:
    
    void CollectExternalObjects(COObjectList& list,
                                TConstObjectPtr object) const;

    void WriteData(CObjectOStream& out, TConstObjectPtr object) const;

    void ReadData(CObjectIStream& in, TObjectPtr object) const;
};

class CChoiceValNodeInfo : public CTypeInfo {
public:
    CChoiceValNodeInfo(void);

    void AddVariant(const CMemberId& id, const CTypeRef& type);
    void AddVariant(const string& name, const CTypeRef& type);

    virtual size_t GetSize(void) const;

    virtual TObjectPtr Create(void) const;

    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const;

    virtual bool Equals(TConstObjectPtr obj1, TConstObjectPtr obj2) const;

protected:
    
    void CollectExternalObjects(COObjectList& list,
                                TConstObjectPtr object) const;

    void WriteData(CObjectOStream& out, TConstObjectPtr object) const;

    void ReadData(CObjectIStream& in, TObjectPtr object) const;

private:
    vector< pair<CMemberId, CTypeRef> > m_Variants;
};

//#include <serial/asntypes.inl>

END_NCBI_SCOPE

#endif  /* ASNTYPES__HPP */
