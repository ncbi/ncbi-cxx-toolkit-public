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
* Revision 1.27  2000/06/07 19:45:41  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.26  2000/03/31 21:38:17  vasilche
* Renamed First() -> FirstNode(), Next() -> NextNode() to avoid name conflict.
*
* Revision 1.25  2000/03/07 14:05:27  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
*
* Revision 1.24  2000/02/17 20:02:26  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.23  2000/02/01 21:44:33  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Added buffering to CObjectIStreamAsn.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
*
* Revision 1.22  1999/12/28 18:55:39  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.21  1999/12/17 19:04:51  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.20  1999/11/22 21:04:32  vasilche
* Cleaned main interface headers. Now generated files should include serial/serialimpl.hpp and user code should include serial/serial.hpp which became might lighter.
*
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
#include <serial/choice.hpp>
#include <serial/serialasn.hpp>

struct valnode;
struct bytestore;
struct asnio;
struct asntype;

BEGIN_NCBI_SCOPE

class CSequenceOfTypeInfo : public CTypeInfo {
    typedef CTypeInfo CParent;
    typedef CType<void*> TType;
public:
    CSequenceOfTypeInfo(TTypeInfo type);
	CSequenceOfTypeInfo(const string& name, TTypeInfo type);
	CSequenceOfTypeInfo(const char* name, TTypeInfo type);
    ~CSequenceOfTypeInfo(void);

    size_t GetNextOffset(void) const
        {
            return m_NextOffset;
        }
    size_t GetDataOffset(void) const
        {
            return m_DataOffset;
        }
    
    TTypeInfo GetDataTypeInfo(void) const
        {
            return m_DataType;
        }

    TObjectPtr CreateData(void) const;

    static TObjectPtr& FirstNode(TObjectPtr object)
        {
            return TType::Get(object);
        }
    static TConstObjectPtr FirstNode(TConstObjectPtr object)
        {
            return TType::Get(object);
        }
    TObjectPtr& NextNode(TObjectPtr object) const
        {
            return TType::Get(Add(object, m_NextOffset));
        }
    TConstObjectPtr NextNode(TConstObjectPtr object) const
        {
            return TType::Get(Add(object, m_NextOffset));
        }
    TObjectPtr Data(TObjectPtr object) const
        {
            return Add(object, m_DataOffset);
        }
    TConstObjectPtr Data(TConstObjectPtr object) const
        {
            return Add(object, m_DataOffset);
        }

    size_t GetSize(void) const;

    virtual bool RandomOrder(void) const;
    
    static TTypeInfo GetTypeInfo(TTypeInfo base);

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

protected:

    virtual void WriteData(CObjectOStream& out,
                           TConstObjectPtr obejct) const;

    virtual void ReadData(CObjectIStream& in,
                          TObjectPtr object) const;

    virtual void SkipData(CObjectIStream& in) const;

private:
    TTypeInfo m_DataType;
    size_t m_NextOffset;  // offset in struct of pointer to next object (def 0)
    size_t m_DataOffset;  // offset in struct of data struct (def 0)
};

class CSetOfTypeInfo : public CSequenceOfTypeInfo {
    typedef CSequenceOfTypeInfo CParent;
public:
    CSetOfTypeInfo(TTypeInfo type);
    CSetOfTypeInfo(const string& name, TTypeInfo type);
    CSetOfTypeInfo(const char* name, TTypeInfo type);
    ~CSetOfTypeInfo(void);

    static TTypeInfo GetTypeInfo(TTypeInfo base);

    virtual bool RandomOrder(void) const;

};

class CChoiceTypeInfo : public CChoiceTypeInfoBase {
    typedef CChoiceTypeInfoBase CParent;
public:
    typedef valnode TObjectType;
    typedef CType<valnode> TType;

    CChoiceTypeInfo(const string& name);
    CChoiceTypeInfo(const char* name);
    ~CChoiceTypeInfo(void);

    // object getters:
    static TObjectType& Get(TObjectPtr object)
        {
            return TType::Get(object);
        }
    static const TObjectType& Get(TConstObjectPtr object)
        {
            return TType::Get(object);
        }

    virtual size_t GetSize(void) const;
    virtual TObjectPtr Create(void) const;

protected:
    virtual TMemberIndex GetIndex(TConstObjectPtr object) const;
    virtual void SetIndex(TObjectPtr object, TMemberIndex index) const;
    virtual TObjectPtr x_GetData(TObjectPtr object, TMemberIndex index) const;
};

class COctetStringTypeInfo : public CTypeInfo {
    typedef CTypeInfo CParent;
    typedef bytestore* TObjectType;
    typedef CType<TObjectType> TType;
public:
    COctetStringTypeInfo(void);
    ~COctetStringTypeInfo(void);

    static TObjectType& Get(TObjectPtr object)
        {
            return TType::Get(object);
        }
    static const TObjectType& Get(TConstObjectPtr object)
        {
            return TType::Get(object);
        }
    size_t GetSize(void) const;

    static TTypeInfo GetTypeInfo(void);

    virtual bool IsDefault(TConstObjectPtr object) const;
    virtual bool Equals(TConstObjectPtr obj1, TConstObjectPtr obj2) const;
    virtual void SetDefault(TObjectPtr dst) const;
    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const;

protected:
    
    void WriteData(CObjectOStream& out, TConstObjectPtr object) const;

    void ReadData(CObjectIStream& in, TObjectPtr object) const;

    void SkipData(CObjectIStream& in) const;
};

class COldAsnTypeInfo : public CTypeInfo
{
    typedef CTypeInfo CParent;
    typedef void* TObjectType;
    typedef CType<TObjectType> TType;
public:
    typedef TObjectPtr (ASNCALL*TNewProc)(void);
    typedef TObjectPtr (ASNCALL*TFreeProc)(TObjectPtr);
    typedef TObjectPtr (ASNCALL*TReadProc)(asnio*, asntype*);
    typedef unsigned char (ASNCALL*TWriteProc)(TObjectPtr, asnio*, asntype*);

    COldAsnTypeInfo(const string& name,
                    TNewProc newProc, TFreeProc freeProc,
                    TReadProc readProc, TWriteProc writeProc);
    COldAsnTypeInfo(const char* name,
                    TNewProc newProc, TFreeProc freeProc,
                    TReadProc readProc, TWriteProc writeProc);
    ~COldAsnTypeInfo(void);

    static TObjectType& Get(TObjectPtr object)
        {
            return TType::Get(object);
        }
    static const TObjectType& Get(TConstObjectPtr object)
        {
            return TType::Get(object);
        }
    size_t GetSize(void) const;

    virtual bool IsDefault(TConstObjectPtr object) const;
    virtual bool Equals(TConstObjectPtr object1,
                        TConstObjectPtr object2) const;
    virtual void SetDefault(TObjectPtr dst) const;
    virtual void Assign(TObjectPtr dst, TConstObjectPtr src) const;

protected:
    
    void WriteData(CObjectOStream& out, TConstObjectPtr object) const;

    void ReadData(CObjectIStream& in, TObjectPtr object) const;

    void SkipData(CObjectIStream& in) const;

private:
    TNewProc m_NewProc;
    TFreeProc m_FreeProc;
    TReadProc m_ReadProc;
    TWriteProc m_WriteProc;
};

//#include <serial/asntypes.inl>

END_NCBI_SCOPE

#endif  /* HAVE_NCBI_C */

#endif  /* ASNTYPES__HPP */
