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
* Revision 1.2  1999/06/04 20:51:35  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:26  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/typeinfo.hpp>
#include <serial/objlist.hpp>
#include <map>
#include <vector>

BEGIN_NCBI_SCOPE

class CClassInfoTmpl;

class COClassInfo
{
public:
    typedef unsigned TIndex;
    typedef CTypeInfo::TTypeInfo TTypeInfo;

    COClassInfo(void)
        : m_Index(-1), m_TypeInfo(0), m_StreamDefinition(0)
        { }

    bool IsWritten(void) const
        { return m_StreamDefinition != 0; }
    TIndex GetIndex(void) const
        { return m_Index; }
    TTypeInfo GetTypeInfo(void) const
        { return m_TypeInfo; }
private:
    TIndex m_Index;
    TTypeInfo m_TypeInfo;
    void* m_StreamDefinition;
};

class CObjectOStream
{
public:
    typedef void* TObjectPtr;
    typedef const void* TConstObjectPtr;
    typedef const CTypeInfo* TTypeInfo;
    typedef unsigned TIndex;

    virtual ~CObjectOStream(void);

    // root writer
    void Write(TConstObjectPtr object, TTypeInfo typeInfo);

    // std C types readers
    virtual void WriteStd(const char& data) = 0;
    virtual void WriteStd(const unsigned char& data) = 0;
    virtual void WriteStd(const signed char& data) = 0;
    virtual void WriteStd(const short& data) = 0;
    virtual void WriteStd(const unsigned short& data) = 0;
    virtual void WriteStd(const int& data) = 0;
    virtual void WriteStd(const unsigned int& data) = 0;
    virtual void WriteStd(const long& data) = 0;
    virtual void WriteStd(const unsigned long& data) = 0;
    virtual void WriteStd(const float& data) = 0;
    virtual void WriteStd(const double& data) = 0;
    virtual void WriteStd(const string& data) = 0;
    virtual void WriteStd(char* const& data) = 0;
    virtual void WriteStd(const char* const& data) = 0;

    // object level writers
    // type info writers
    virtual void WritePointer(TConstObjectPtr object, TTypeInfo typeInfo) = 0;
    // write object content
    virtual void WriteObject(TConstObjectPtr object,
                             const CClassInfoTmpl* typeInfo) = 0;

    virtual void WriteTypeInfo(TTypeInfo typeInfo) = 0;
    virtual void WriteClassInfo(TTypeInfo typeInfo) = 0;
    virtual void WriteTemplateInfo(const string& name, TTypeInfo tmpl,
                                   TTypeInfo arg) = 0;
    virtual void WriteTemplateInfo(const string& name, TTypeInfo tmpl,
                                   TTypeInfo arg1, TTypeInfo arg2) = 0;
    virtual void WriteTemplateInfo(const string& name, TTypeInfo tmpl,
                                   const vector<TTypeInfo>& args) = 0;

    // block interface
    class Block;
    virtual void Begin(Block& block, unsigned count, bool tmpl);
    virtual void Next(Block& block);
    virtual void End(Block& block);
    class Block
    {
    public:
        Block(CObjectOStream& out, unsigned count, bool tmpl = false)
            : m_Out(out), m_Count(count)
            {
                m_Out.Begin(*this, count, tmpl);
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

        unsigned GetCount(void) const
            {
                return m_Count;
            }

    private:
        CObjectOStream& m_Out;
        unsigned m_Count;
    };
    
    COClassInfo* GetRegisteredClass(TTypeInfo typeInfo) const;
    virtual void RegisterClass(TTypeInfo typeInfo);

protected:
    // low level writers
    virtual void WriteString(const string& str) = 0;
    virtual void WriteId(const string& id);

    COObjectList m_Objects;
    map<TTypeInfo, COClassInfo> m_Classes;
};

//#include <serial/objostr.inl>

END_NCBI_SCOPE

#endif  /* OBJOSTR__HPP */
