#ifndef OBJISTR__HPP
#define OBJISTR__HPP

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
* Revision 1.1  1999/05/19 19:56:24  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/typeinfo.hpp>

BEGIN_NCBI_SCOPE

class CObjectIStream
{
public:
    typedef void* TObjectPtr;
    typedef const void* TConstObjectPtr;
    typedef const CTypeInfo* TTypeInfo;
    typedef unsigned TIndex;

    struct CObjectInfo
    {
        TObjectPtr m_Object;
        TTypeInfo m_TypeInfo;
    };

    // root reader
    void Read(TObjectPtr object, TTypeInfo typeInfo);

    // std C types readers
#define PROCESS_STD_TYPE(TYPE) virtual void ReadStd(TYPE& data) = 0;
    FOR_ALL_STD_TYPES
#undef PROCESS_STD_TYPE
    virtual void ReadStd(string& data) = 0;

    // block interface
    class Block;
    virtual void Begin(Block& block, bool tmpl) = 0;
    virtual bool Next(Block& block) = 0;
    virtual void End(Block& block) = 0;
    class Block
    {
    public:
        Block(CObjectIStream& in, bool tmpl = false)
            : m_In(in)
            {
                m_In.Begin(*this, tmpl);
            }
        bool Next(void)
            {
                return m_In.Next(*this);
            }
        ~Block(void)
            {
                m_In.End(*this);
            }
    private:
        CObjectIStream& m_In;
        unsigned m_Count;
    };

protected:
    // object level readers
    // reads type info
    virtual TTypeInfo ReadTypeInfo(void) = 0;

    // read object content
    void ReadObjectData(TObjectPtr object, TTypeInfo typeInfo);
    // read object reference
    TObjectPtr ReadObject(TTypeInfo declaredTypeInfo);

    // low level readers
    virtual const string& ReadString(void);
    virtual const string& ReadId(void);

    struct CObjectInfo
    {
        CObjectInfo(TObjectPtr object, TTypeInfo typeInfo)
            : m_Object(object), m_TypeInfo(typeInfo)
            {
            }
        TObjectPtr m_Object;
        TTypeInfo m_TypeInfo;
    };

    struct CClassInfo
    {
        TTypeInfo m_TypeInfo;
        vector<TTypeInfo> m_Members;
    };

    TTypeInfo GetClass(TIndex index) const;
    const CObjectInfo& GetObjectInfo(TIndex index) const;
    TIndex RegisterObject(TObjectPtr object, 
    
private:
    vector<CObjectInfo> m_Objects;
    vector<CClassInfo> m_Classes;
};

//#include <serial/objistr.inl>

END_NCBI_SCOPE

#endif  /* OBJISTR__HPP */
