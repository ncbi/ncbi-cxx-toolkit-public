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
* Revision 1.25  1999/09/24 18:19:13  vasilche
* Removed dependency on NCBI toolkit.
*
* Revision 1.24  1999/09/23 18:56:51  vasilche
* Fixed bugs with overloaded methods in objistr*.hpp & objostr*.hpp
*
* Revision 1.23  1999/09/22 20:11:48  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.22  1999/09/14 18:54:03  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.21  1999/08/16 16:07:43  vasilche
* Added ENUMERATED type.
*
* Revision 1.20  1999/08/13 15:53:43  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.19  1999/07/26 18:31:28  vasilche
* Implemented skipping of unused values.
* Added more useful error report.
*
* Revision 1.18  1999/07/22 20:36:37  vasilche
* Fixed 'using namespace' declaration for MSVC.
*
* Revision 1.17  1999/07/22 17:33:40  vasilche
* Unified reading/writing of objects in all three formats.
*
* Revision 1.16  1999/07/21 14:19:55  vasilche
* Added serialization of bool.
*
* Revision 1.15  1999/07/19 15:50:15  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
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
#include <serial/object.hpp>
#include <vector>

struct asnio;

BEGIN_NCBI_SCOPE

class CTypeMapper;

class CObjectIStream
{
public:
    typedef unsigned TIndex;

    CObjectIStream(void);
    virtual ~CObjectIStream(void);

    // root reader
    void Read(TObjectPtr object, TTypeInfo typeInfo);
    CObject ReadObject(void);

    virtual string ReadTypeName(void);
    
    // try to read enum value name, "" if none
    virtual string ReadEnumName(void);
    virtual int ReadEnumValue(void);

    virtual TTypeInfo MapType(const string& name);
    void SetTypeMapper(CTypeMapper* typeMapper);

    // std C types readers
    void ReadStd(bool& data)
        {
            data = ReadBool();
        }
    void ReadStd(char& data)
        {
            data = ReadChar();
        }
    void ReadStd(signed char& data)
        {
            data = ReadSChar();
        }
    void ReadStd(unsigned char& data)
        {
            data = ReadUChar();
        }
    void ReadStd(short& data)
        {
            data = ReadShort();
        }
    void ReadStd(unsigned short& data)
        {
            data = ReadUShort();
        }
    void ReadStd(int& data)
        {
            data = ReadInt();
        }
    void ReadStd(unsigned& data)
        {
            data = ReadUInt();
        }
    void ReadStd(long& data)
        {
            data = ReadLong();
        }
    void ReadStd(unsigned long& data)
        {
            data = ReadULong();
        }
    void ReadStd(float& data)
        {
            data = ReadFloat();
        }
    void ReadStd(double& data)
        {
            data = ReadDouble();
        }
    void ReadStd(char*& data)
        {
            data = ReadCString();
        }
    void ReadStd(const char*& data)
        {
            data = ReadCString();
        }
    void ReadStd(string& data)
        {
            data = ReadString();
        }

    // object level readers
    void ReadExternalObject(TObjectPtr object, TTypeInfo typeInfo);
    // reads type info
    virtual TObjectPtr ReadPointer(TTypeInfo declaredType);
    enum EPointerType {
        eNullPointer,
        eMemberPointer,
        eObjectPointer,
        eThisPointer,
        eOtherPointer
    };

    virtual void SkipValue(void);

    // low level readers:
    enum EFailFlags {
        eNoError = 0,
        eEOF = 1,
        eReadError = 2,
        eFormatError = 4,
        eOverflow = 8,
        eIllegalCall = 16
    };
    bool fail(void) const
        {
            return m_Fail != 0;
        }
    unsigned GetFailFlags(void) const
        {
            return m_Fail;
        }
    virtual unsigned SetFailFlags(unsigned flags);
    unsigned ClearFailFlags(unsigned flags)
        {
            unsigned old = m_Fail;
            m_Fail &= ~flags;
            return old;
        }
    void ThrowError(EFailFlags flags, const char* message);
    void ThrowError(EFailFlags flags, const string& message);
    void ThrowError(CNcbiIstream& in);
    void CheckError(CNcbiIstream& in)
        {
            if ( !in )
                ThrowError(in);
        }

    // member interface
    class Member {
    public:
        Member(CObjectIStream& in);
        Member(CObjectIStream& in, const CMemberId& id);
        ~Member(void);

        Member* GetPrevous(void) const
            {
                return m_Previous;
            }

        CMemberId& Id(void)
            {
                return m_Id;
            }
        const CMemberId& Id(void) const
            {
                return m_Id;
            }
        operator const CMemberId&(void) const
            {
                return m_Id;
            }

    private:
        CObjectIStream& m_In;
        CMemberId m_Id;
        Member* m_Previous;
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

#if HAVE_NCBI_C
    // ASN.1 interface
    class AsnIo {
    public:
        AsnIo(CObjectIStream& in);
        ~AsnIo(void);
        operator asnio*(void)
            {
                return m_AsnIo;
            }
        asnio* operator->(void)
            {
                return m_AsnIo;
            }
        size_t Read(char* data, size_t length)
            {
                return m_In.AsnRead(*this, data, length);
            }
    private:
        CObjectIStream& m_In;
        asnio* m_AsnIo;

    public:
        size_t m_Count;
    };
    friend class AsnIo;
protected:
    // ASN.1 interface
    virtual void AsnOpen(AsnIo& asn);
    virtual void AsnClose(AsnIo& asn);
    virtual unsigned GetAsnFlags(void);
    virtual size_t AsnRead(AsnIo& asn, char* data, size_t length);
#endif

protected:
    friend class Block;
    friend class Member;
	friend class ByteBlock;

    // member interface
    virtual void StartMember(Member& member) = 0;
    virtual void EndMember(const Member& member);

    // block interface
    static void SetNonFixed(Block& block, bool finished = false)
        {
            block.m_Fixed = false;
            block.m_Size = 0;
            block.m_Finished = finished;
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
	static void SetBlockLength(ByteBlock& block, size_t length)
		{
			block.m_Length = length;
			block.m_KnownLength = true;
		}
	virtual void Begin(ByteBlock& block);
	virtual size_t ReadBytes(const ByteBlock& block,
                             char* buffer, size_t count) = 0;
	virtual void End(const ByteBlock& block);

    // low level readers
    CObject ReadObjectInfo(void);
    virtual EPointerType ReadPointerType(void) = 0;
    virtual string ReadMemberPointer(void);
    virtual void ReadMemberPointerEnd(void);
    virtual TIndex ReadObjectPointer(void) = 0;
    virtual void ReadThisPointerEnd(void);
    virtual string ReadOtherPointer(void) = 0;
    virtual void ReadOtherPointerEnd(void);
    virtual bool HaveMemberSuffix(void);
    virtual string ReadMemberSuffix(void);

    virtual bool ReadBool(void) = 0;
    virtual char ReadChar(void) = 0;
    virtual signed char ReadSChar(void);
    virtual unsigned char ReadUChar(void);
    virtual short ReadShort(void);
    virtual unsigned short ReadUShort(void);
    virtual int ReadInt(void);
    virtual unsigned ReadUInt(void);
    virtual long ReadLong(void) = 0;
    virtual unsigned long ReadULong(void) = 0;
    virtual float ReadFloat(void);
    virtual double ReadDouble(void) = 0;
    virtual string ReadString(void) = 0;
    virtual char* ReadCString(void);

    const CObject& GetRegisteredObject(TIndex index) const;
    TIndex RegisterObject(TObjectPtr object, TTypeInfo typeInfo);
    TIndex RegisterInvalidObject(void)
        { return RegisterObject(0, 0); }

    void ReadData(TObjectPtr object, TTypeInfo typeInfo)
        {
            typeInfo->ReadData(*this, object);
        }
    
private:
    vector<CObject> m_Objects;

    unsigned m_Fail;
    Member* m_CurrentMember;

    CTypeMapper* m_TypeMapper;
};

//#include <serial/objistr.inl>

END_NCBI_SCOPE

#endif  /* OBJISTR__HPP */
