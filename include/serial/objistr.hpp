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
* Revision 1.14  1999/07/15 16:54:42  vasilche
* Implemented vector<X> & vector<char> as special case.
*
* Revision 1.13  1999/07/09 16:32:53  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.12  1999/07/07 21:15:03  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.11  1999/07/02 21:31:43  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.10  1999/07/01 17:55:19  vasilche
* Implemented ASN.1 binary write.
*
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

    virtual void ReadStr(string& data);
    virtual void ReadStr(char*& data);
    virtual void ReadStr(const char*& data);

    // object level readers
    void ReadExternalObject(TObjectPtr object, TTypeInfo typeInfo);
    // reads type info
    virtual TObjectPtr ReadPointer(TTypeInfo declaredType) = 0;

    virtual void SkipValue(void);

    class Member : public CMemberId {
    public:
        Member(CObjectIStream& in)
            : m_In(in)
            {
                in.StartMember(*this);
            }
        ~Member(void)
            {
                m_In.EndMember(*this);
            }
    private:
        CObjectIStream& m_In;
    };
    // block interface
    enum EFixed {
        eFixed
    };
    class Block {
    public:
        Block(CObjectIStream& in);
        Block(CObjectIStream& in, EFixed eFixed);
        Block(CObjectIStream& in, bool randomOrder);
        Block(CObjectIStream& in, bool randomOrder, EFixed eFixed);
        ~Block(void);

        bool Next(void);

        bool Fixed(void) const
            {
                return m_Fixed;
            }
        bool RandomOrder(void) const
            {
                return m_RandomOrder;
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
        bool m_RandomOrder;
        bool m_Finished;
        unsigned m_Size;
        unsigned m_NextIndex;
    };
	class ByteBlock {
	public:
		ByteBlock(CObjectIStream& in)
			: m_In(in), m_KnownLength(false), m_EndOfBlock(false), m_Length(0)
		{
			in.Begin(*this);
		}

        bool KnownLength(void) const
        {
            return m_KnownLength;
        }

		size_t GetExpectedLength(void) const
		{
			return m_Length;
		}

		~ByteBlock(void)
		{
            if ( KnownLength()? m_Length != 0: !m_EndOfBlock )
				THROW1_TRACE(runtime_error, "not all bytes read");
			m_In.End(*this);
		}

		size_t Read(void* dst, size_t length)
		{
			if ( KnownLength() )
                length = min(length, m_Length);
            
			if ( m_EndOfBlock || length == 0 )
                return 0;

            length = m_In.ReadBytes(*this, static_cast<char*>(dst), length);
            m_Length -= length;
            m_EndOfBlock = length == 0;
            return length;
		}

	private:
		CObjectIStream& m_In;
		bool m_KnownLength;
        bool m_EndOfBlock;
		size_t m_Length;

		friend class CObjectIStream;
	};

protected:
    // block interface
    friend class Block;
    friend class Member;
	friend class ByteBlock;
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
    virtual void StartMember(Member& member) = 0;
    virtual void EndMember(const Member& member);
	static void SetBlockLength(ByteBlock& block, size_t length)
		{
			block.m_Length = length;
			block.m_KnownLength = true;
		}
	virtual void Begin(ByteBlock& block);
	virtual size_t ReadBytes(const ByteBlock& block, char* dst, size_t length) = 0;
	virtual void End(const ByteBlock& block);

protected:
    // low level readers
    virtual string ReadString(void) = 0;
    virtual char* ReadCString(void);

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
