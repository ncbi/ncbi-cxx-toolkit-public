#ifndef CHOICE__HPP
#define CHOICE__HPP

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
* Revision 1.8  2000/04/03 18:47:09  vasilche
* Added main include file for generated headers.
* serialimpl.hpp is included in generated sources with GetTypeInfo methods
*
* Revision 1.7  2000/03/07 14:05:28  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
*
* Revision 1.6  2000/02/17 20:02:27  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.5  2000/02/01 21:44:34  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Added buffering to CObjectIStreamAsn.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
*
* Revision 1.4  1999/12/28 18:55:39  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.3  1999/12/17 19:04:52  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.2  1999/10/08 21:00:39  vasilche
* Implemented automatic generation of unnamed ASN.1 types.
*
* Revision 1.1  1999/09/24 18:20:05  vasilche
* Removed dependency on NCBI toolkit.
*
*
* ===========================================================================
*/

#include <serial/typeinfo.hpp>
#include <serial/typeref.hpp>
//#include <serial/memberid.hpp>
#include <serial/memberlist.hpp>

BEGIN_NCBI_SCOPE

class CChoiceTypeInfoBase : public CTypeInfo {
    typedef CTypeInfo CParent;
public:
    CChoiceTypeInfoBase(const string& name);
    CChoiceTypeInfoBase(const char* name);
    ~CChoiceTypeInfoBase(void);

    void AddVariant(const CMemberId& id, const CTypeRef& type);
    void AddVariant(const string& name, const CTypeRef& type);
    void AddVariant(const char* name, const CTypeRef& type);

    virtual bool IsDefault(TConstObjectPtr object) const;
    virtual bool Equals(TConstObjectPtr obj1, TConstObjectPtr obj2) const;
    virtual void SetDefault(TObjectPtr dst) const;
    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const;

    TMemberIndex GetVariantsCount(void) const;
    TTypeInfo GetVariantTypeInfo(TMemberIndex index) const;

    CMembersInfo& GetMembers(void)
        {
            return m_Members;
        }
    const CMembersInfo& GetMembers(void) const
        {
            return m_Members;
        }

protected:
    virtual TMemberIndex GetIndex(TConstObjectPtr object) const = 0;
    virtual void SetIndex(TObjectPtr object, TMemberIndex index) const = 0;
    virtual TObjectPtr x_GetData(TObjectPtr object,
                                 TMemberIndex index) const = 0;
    TConstObjectPtr GetData(TConstObjectPtr object, TMemberIndex index) const
        {
            return x_GetData(const_cast<TObjectPtr>(object), index);
        }
    TObjectPtr GetData(TObjectPtr object, TMemberIndex index) const
        {
            return x_GetData(object, index);
        }

    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const;

    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const;

    virtual void SkipData(CObjectIStream& in) const;

private:
    CMembersInfo m_Members;
};

class CGeneratedChoiceInfo : public CChoiceTypeInfoBase
{
    typedef CChoiceTypeInfoBase CParent;
public:
    typedef int TChoiceIndex;
    typedef TObjectPtr (*TCreateFunction)(void);
    typedef TChoiceIndex (*TGetIndexFunction)(TConstObjectPtr object);
    typedef void (*TSetIndexFunction)(TObjectPtr object, TChoiceIndex index);
    typedef void (*TPostReadFunction)(TObjectPtr object);
    typedef void (*TPreWriteFunction)(TConstObjectPtr object);

    CGeneratedChoiceInfo(const char* name,
                         size_t size,
                         TCreateFunction createFunction,
                         TGetIndexFunction getIndexFunction,
                         TSetIndexFunction setIndexFunction);
    
    void SetPostRead(TPostReadFunction func);
    void SetPreWrite(TPreWriteFunction func);
    
protected:
    size_t GetSize(void) const;
    TObjectPtr Create(void) const;

    TMemberIndex GetIndex(TConstObjectPtr object) const;
    void SetIndex(TObjectPtr object, TMemberIndex index) const;
    TObjectPtr x_GetData(TObjectPtr object, TMemberIndex index) const;

protected:
    virtual void WriteData(CObjectOStream& out, TConstObjectPtr object) const;

    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const;

private:
    size_t m_Size;
    TCreateFunction m_CreateFunction;
    TGetIndexFunction m_GetIndexFunction;
    TSetIndexFunction m_SetIndexFunction;
    TPostReadFunction m_PostReadFunction;
    TPreWriteFunction m_PreWriteFunction;
};

END_NCBI_SCOPE

#endif  /* CHOICE__HPP */
