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
* Revision 1.19  1999/10/04 19:39:45  vasilche
* Fixed bug in CObjectOStreamBinary.
* Start using of BSRead/BSWrite.
* Added ASNCALL macro for prototypes of old ASN.1 functions.
*
* Revision 1.18  1999/10/04 16:22:07  vasilche
* Fixed bug with old ASN.1 structures.
*
* Revision 1.17  1999/09/29 22:36:30  vakatov
* Dont forget to #include ncbistd.hpp before #ifdef HAVE_NCBI_C...
*
* Revision 1.16  1999/09/24 18:55:53  vasilche
* ASN.1 types will not be compiled is we don't have NCBI toolkit.
*
* Revision 1.15  1999/09/24 18:19:12  vasilche
* Removed dependency on NCBI toolkit.
*
* Revision 1.14  1999/09/23 18:56:51  vasilche
* Fixed bugs with overloaded methods in objistr*.hpp & objostr*.hpp
*
* Revision 1.13  1999/09/22 20:11:47  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.12  1999/09/14 18:54:01  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.11  1999/08/16 16:07:42  vasilche
* Added ENUMERATED type.
*
* Revision 1.10  1999/08/13 20:22:56  vasilche
* Fixed lot of bugs in datatool
*
* Revision 1.9  1999/08/13 15:53:42  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.8  1999/07/19 15:50:14  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.7  1999/07/14 18:58:02  vasilche
* Fixed ASN.1 types/field naming.
*
* Revision 1.6  1999/07/13 20:18:04  vasilche
* Changed types naming.
*
* Revision 1.5  1999/07/09 16:32:53  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.4  1999/07/07 19:58:43  vasilche
* Reduced amount of data allocated on heap
* Cleaned ASN.1 structures info
*
* Revision 1.3  1999/07/01 17:55:16  vasilche
* Implemented ASN.1 binary write.
*
* Revision 1.2  1999/06/30 18:54:53  vasilche
* Fixed some errors under MSVS
*
* Revision 1.1  1999/06/30 16:04:18  vasilche
* Added support for old ASN.1 structures.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>

#if HAVE_NCBI_C
#include <serial/typeinfo.hpp>
#include <serial/typeref.hpp>
#include <serial/typemap.hpp>
#include <serial/choice.hpp>
#include <map>

struct valnode;
struct bytestore;
struct asnio;
struct asntype;

BEGIN_NCBI_SCOPE

class CSequenceOfTypeInfo : public CTypeInfoTmpl<void*> {
    typedef CTypeInfoTmpl<TObjectPtr> CParent;
public:
    CSequenceOfTypeInfo(TTypeInfo type);
	CSequenceOfTypeInfo(const string& name, TTypeInfo type);

    static TObjectPtr& First(TObjectPtr object)
        {
            return Get(object);
        }
    static TConstObjectPtr First(TConstObjectPtr object)
        {
            return Get(object);
        }
    TObjectPtr& Next(TObjectPtr object) const
        {
            return Get(Add(object, m_NextOffset));
        }
    TConstObjectPtr Next(TConstObjectPtr object) const
        {
            return Get(Add(object, m_NextOffset));
        }
    TObjectPtr Data(TObjectPtr object) const
        {
            return Add(object, m_DataOffset);
        }
    TConstObjectPtr Data(TConstObjectPtr object) const
        {
            return Add(object, m_DataOffset);
        }

    virtual bool RandomOrder(void) const;
    
    static TTypeInfo GetTypeInfo(TTypeInfo base)
        {
            return sm_Map.GetTypeInfo(base);
        }

    virtual bool IsDefault(TConstObjectPtr object) const;
    virtual bool Equals(TConstObjectPtr object1,
                        TConstObjectPtr object2) const;
    virtual void SetDefault(TObjectPtr dst) const;
    virtual void Assign(TObjectPtr dst,
                        TConstObjectPtr src) const;

private:
	void Init(void);

    // set this sequence to have ValNode as data holder
    // (used for SET OF (INTEGER, STRING, SET OF etc.)
    void SetValNodeNext(void);
    // SET OF CHOICE (use choice's valnode->next field as link)
    void SetChoiceNext(void);

    TTypeInfo GetDataTypeInfo(void) const
        {
            return m_DataType;
        }

    TObjectPtr CreateData(void) const;

protected:

    virtual void CollectExternalObjects(COObjectList& list,
                                        TConstObjectPtr object) const;

    virtual void WriteData(CObjectOStream& out,
                           TConstObjectPtr obejct) const;

    virtual void ReadData(CObjectIStream& in,
                          TObjectPtr object) const;

private:
    static CTypeInfoMap<CSequenceOfTypeInfo> sm_Map;

    TTypeInfo m_DataType;
    size_t m_NextOffset;  // offset in struct of pointer to next object (def 0)
    size_t m_DataOffset;  // offset in struct of data struct (def 0)
};

class CSetOfTypeInfo : public CSequenceOfTypeInfo {
    typedef CSequenceOfTypeInfo CParent;
public:
    CSetOfTypeInfo(TTypeInfo type);
    CSetOfTypeInfo(const string& name, TTypeInfo type);

    static TTypeInfo GetTypeInfo(TTypeInfo base)
        {
            return sm_Map.GetTypeInfo(base);
        }

    virtual bool RandomOrder(void) const;

private:
    static CTypeInfoMap<CSetOfTypeInfo> sm_Map;
};

class CChoiceTypeInfo : public CChoiceTypeInfoBase {
    typedef CChoiceTypeInfoBase CParent;
public:
    typedef valnode TObjectType;

    CChoiceTypeInfo(const string& name);

    // object getters:
    static TObjectType& Get(TObjectPtr object)
        {
            return *static_cast<TObjectType*>(object);
        }
    static const TObjectType& Get(TConstObjectPtr object)
        {
            return *static_cast<const TObjectType*>(object);
        }

    virtual size_t GetSize(void) const;
    virtual TObjectPtr Create(void) const;

protected:
    virtual TMemberIndex GetIndex(TConstObjectPtr object) const;
    virtual void SetIndex(TObjectPtr object, TMemberIndex index) const;
    virtual TObjectPtr x_GetData(TObjectPtr object) const;
};

class COctetStringTypeInfo : public CTypeInfoTmpl<bytestore*> {
    typedef CTypeInfoTmpl<bytestore*> CParent;
public:
    COctetStringTypeInfo(void);

    static TTypeInfo GetTypeInfo(void)
        {
            static TTypeInfo typeInfo = 0;
            if ( !typeInfo )
                typeInfo = new COctetStringTypeInfo();
            return typeInfo;
        }

    virtual bool IsDefault(TConstObjectPtr object) const;
    virtual bool Equals(TConstObjectPtr obj1, TConstObjectPtr obj2) const;
    virtual void SetDefault(TObjectPtr dst) const;
    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const;

protected:
    
    void CollectExternalObjects(COObjectList& list,
                                TConstObjectPtr object) const;

    void WriteData(CObjectOStream& out, TConstObjectPtr object) const;

    void ReadData(CObjectIStream& in, TObjectPtr object) const;
};

#ifdef HAVE_WINDOWS_H
# define ASNCALL __stdcall
#else
# define ASNCALL
#endif

class COldAsnTypeInfo : public CTypeInfoTmpl<void*>
{
    typedef CTypeInfoTmpl<void*> CParent;
public:
    typedef TObjectPtr (ASNCALL*TNewProc)(void);
    typedef TObjectPtr (ASNCALL*TFreeProc)(TObjectPtr);
    typedef TObjectPtr (ASNCALL*TReadProc)(asnio*, asntype*);
    typedef unsigned char (ASNCALL*TWriteProc)(TObjectPtr, asnio*, asntype*);

    COldAsnTypeInfo(const string& name,
                    TNewProc newProc, TFreeProc freeProc,
                    TReadProc readProc, TWriteProc writeProc);

    virtual bool IsDefault(TConstObjectPtr object) const;
    virtual bool Equals(TConstObjectPtr object1,
                        TConstObjectPtr object2) const;
    virtual void SetDefault(TObjectPtr dst) const;
    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const;

protected:
    
    void WriteData(CObjectOStream& out, TConstObjectPtr object) const;

    void ReadData(CObjectIStream& in, TObjectPtr object) const;

private:
    TNewProc m_NewProc;
    TFreeProc m_FreeProc;
    TReadProc m_ReadProc;
    TWriteProc m_WriteProc;

    static map<TNewProc, COldAsnTypeInfo*> m_Types;
};

//#include <serial/asntypes.inl>

END_NCBI_SCOPE

#endif  /* HAVE_NCBI_C */

#endif  /* ASNTYPES__HPP */
