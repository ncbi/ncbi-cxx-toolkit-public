#ifndef OBJOSTR__HPP
#define OBJOSTR__HPP

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
* Revision 1.1  1999/05/19 19:56:26  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/typeinfo.hpp>

BEGIN_NCBI_SCOPE

class CObjectList
{
public:
    typedef CTypeInfo::TConstObjectPtr TConstObjectPtr;
    typedef CTypeInfo::TTypeInfo TTypeInfo;

    // add object to object list
    // return true if object is new
    // return false if object was already added
    // may throw an exception if there is error in objects placements
    bool Add(TConstObjectPtr, TTypeInfo);
};

class CObjectOStream
{
public:
    typedef void* TObjectPtr;
    typedef const void* TConstObjectPtr;
    typedef const CTypeInfo* TTypeInfo;

    struct CObjectInfo
    {
        offset_t m_Id;
        TTypeInfo m_TypeInfo;
    };

    // root writer
    void Write(TConstObjectPtr object, TTypeInfo typeInfo);

    // std C types readers
#define PROCESS_STD_TYPE(TYPE) virtual void WriteStd(const TYPE& data) = 0;
    FOR_ALL_STD_TYPES
#undef PROCESS_STD_TYPE
    virtual void WriteStd(const string& data) = 0;

    // block interface
    class Block;
    virtual void Begin(Block& block, bool tmpl);
    virtual void Next(Block& block);
    virtual void End(Block& block);
    class Block
    {
    public:
        Block(CObjectOStream& out, unsigned count, bool tmpl = false)
            : m_Out(out), m_Count(count)
            {
                m_Out.Begin(*this, tmpl);
            }
        void Next(void)
            {
                if ( !m_Count )
                    throw runtime_error("2");
                m_Out.Next(*this);
                --m_Count;
            }
        ~Block(void)
            {
                if ( m_Count )
                    throw runtime_error("3");
                m_Out.End(*this);
            }
    private:
        CObjectOStream& m_Out;
        unsigned m_Count;
    };

protected:
    // object level writers
    // type info writers
    virtual void WriteTypeInfo(TTypeInfo typeInfo);
    virtual void WriteClassInfo(const string& name);
    virtual void WriteTemplateInfo(const string& name, TTypeInfo arg);
    virtual void WriteTemplateInfo(const string& name,
                                   TTypeInfo arg1, TTypeInfo arg2);

    // write object content
    void WriteObjectData(TConstObjectPtr object, TTypeInfo typeInfo);
    // write object reference
    void WriteObject(TConstObjectPtr object, TTypeInfo typeInfo);

    // low level writers
    virtual void WriteString(const string& str);
    virtual void WriteId(const string& id);
};

//#include <serial/objostr.inl>

END_NCBI_SCOPE

#endif  /* OBJOSTR__HPP */
