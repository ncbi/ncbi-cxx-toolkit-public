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
* Revision 1.9  1999/06/30 16:04:27  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.8  1999/06/24 14:44:39  vasilche
* Added binary ASN.1 output.
*
* Revision 1.7  1999/06/17 20:42:00  vasilche
* Fixed storing/loading of pointers.
*
* Revision 1.6  1999/06/16 20:35:21  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.5  1999/06/15 16:20:01  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.4  1999/06/10 21:06:38  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.3  1999/06/07 19:30:15  vasilche
* More bug fixes
*
* Revision 1.2  1999/06/04 20:51:32  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:24  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/serialdef.hpp>
#include <serial/typeinfo.hpp>
#include <serial/memberid.hpp>
#include <vector>

BEGIN_NCBI_SCOPE

class CIObjectInfo
{
public:
    CIObjectInfo(void)
        : m_Object(0), m_TypeInfo(0)
        {
        }
    CIObjectInfo(TObjectPtr object, TTypeInfo typeInfo)
        : m_Object(object), m_TypeInfo(typeInfo)
        {
        }

    TObjectPtr GetObject(void) const
        { return m_Object; }

    TTypeInfo GetTypeInfo(void) const
        { return m_TypeInfo; }

private:
    TObjectPtr m_Object;
    TTypeInfo m_TypeInfo;
};

class CObjectIStream
{
public:
    typedef unsigned TIndex;

    // root reader
    virtual void Read(TObjectPtr object, TTypeInfo typeInfo);

    // std C types readers
    virtual void ReadStd(char& data) = 0;
    virtual void ReadStd(unsigned char& data) = 0;
    virtual void ReadStd(signed char& data) = 0;
    virtual void ReadStd(short& data) = 0;
    virtual void ReadStd(unsigned short& data) = 0;
    virtual void ReadStd(int& data) = 0;
    virtual void ReadStd(unsigned int& data) = 0;
    virtual void ReadStd(long& data) = 0;
    virtual void ReadStd(unsigned long& data) = 0;
    virtual void ReadStd(float& data) = 0;
    virtual void ReadStd(double& data) = 0;
    virtual void ReadStd(string& data) = 0;
    virtual void ReadStd(char*& data) = 0;
    virtual void ReadStd(const char*& data);

    // object level readers
    void ReadExternalObject(TObjectPtr object, TTypeInfo typeInfo);
    // reads type info
    virtual TObjectPtr ReadPointer(TTypeInfo declaredType) = 0;

    virtual void SkipValue(void);

    virtual CMemberId ReadMember(void);

    // block interface
    enum EFixed {
        eFixed
    };
    enum ESequence {
        eSequence
    };
    class Block
    {
    public:
        Block(CObjectIStream& in);
        Block(CObjectIStream& in, EFixed eFixed);
        Block(CObjectIStream& in, ESequence sequence);
        Block(CObjectIStream& in, ESequence sequence, EFixed eFixed);
        ~Block(void);

        bool Next(void);

        bool Fixed(void) const
            {
                return m_Fixed;
            }
        bool Sequence(void) const
            {
                return m_Sequence;
            }

        bool Finished(void) const
            {
                return m_Finished;
            }

        unsigned GetNextIndex(void) const
            {
                return m_NextIndex;
            }

        unsigned GetIndex(void) const
            {
                return GetNextIndex() - 1;
            }

        bool First(void) const
            {
                return GetNextIndex() == 0;
            }

        unsigned GetSize(void) const
            {
                return m_Size;
            }

    protected:
        CObjectIStream& m_In;

        void IncIndex(void)
            {
                ++m_NextIndex;
            }

    private:
        // to prevent copying
        Block(const Block&);
        Block& operator=(const Block&);
        // to prevent allocation in heap
        void* operator new(size_t size);
        friend class CObjectIStream;

        bool m_Fixed;
        bool m_Sequence;
        bool m_Finished;
        unsigned m_Size;
        unsigned m_NextIndex;
    };

protected:
    // block interface
    friend class Block;
    static void SetNonFixed(Block& block)
        {
            block.m_Fixed = false;
            block.m_Size = 0;
        }
    static void SetFixed(Block& block, unsigned size)
        {
            block.m_Fixed = true;
            block.m_Size = size;
        }
    virtual void FBegin(Block& block);
    virtual void VBegin(Block& block);
    virtual void FNext(const Block& block);
    virtual bool VNext(const Block& block);
    virtual void FEnd(const Block& block);
    virtual void VEnd(const Block& block);

protected:
    // low level readers
    virtual string ReadString(void) = 0;
    virtual string ReadId(void);

    const CIObjectInfo& GetRegisteredObject(TIndex index) const;
    TIndex RegisterObject(TObjectPtr object, TTypeInfo typeInfo);
    TIndex RegisterInvalidObject(void)
        { return RegisterObject(0, 0); }

    void ReadData(TObjectPtr object, TTypeInfo typeInfo)
        {
            typeInfo->ReadData(*this, object);
        }
    
private:
    vector<CIObjectInfo> m_Objects;
};

//#include <serial/objistr.inl>

END_NCBI_SCOPE

#endif  /* OBJISTR__HPP */
