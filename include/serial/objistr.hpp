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
* Revision 1.37  2000/04/13 14:50:17  vasilche
* Added CObjectIStream::Open() and CObjectOStream::Open() for easier use.
*
* Revision 1.36  2000/04/06 16:10:50  vasilche
* Fixed bug with iterators in choices.
* Removed unneeded calls to ReadExternalObject/WriteExternalObject.
* Added output buffering to text ASN.1 data.
*
* Revision 1.35  2000/03/14 14:43:29  vasilche
* Fixed error reporting.
*
* Revision 1.34  2000/03/07 14:05:30  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
*
* Revision 1.33  2000/01/10 20:04:07  vasilche
* Fixed duplicate name.
*
* Revision 1.32  2000/01/10 19:46:31  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.31  2000/01/05 19:43:44  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.30  1999/12/20 15:29:31  vasilche
* Fixed bug with old ASN structures.
*
* Revision 1.29  1999/12/17 19:04:53  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.28  1999/10/18 20:11:15  vasilche
* Enum values now have long type.
* Fixed template generation for enums.
*
* Revision 1.27  1999/10/04 16:22:08  vasilche
* Fixed bug with old ASN.1 structures.
*
* Revision 1.26  1999/09/27 14:17:59  vasilche
* Fixed bug with overloaded construtors of Block.
*
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
#include <serial/object.hpp>
#include <vector>

struct asnio;

BEGIN_NCBI_SCOPE

class CTypeMapper;
class CMemberId;
class CMembers;

class CObjectIStream
{
public:
    typedef unsigned TIndex;

    static CObjectIStream* Open(const string& fileName, unsigned flags);
    static CObjectIStream* Open(CNcbiIstream& inStream, unsigned flags,
                                bool deleteInStream = false);

    CObjectIStream(void);
    virtual ~CObjectIStream(void);

    // root reader
    void Read(TObjectPtr object, TTypeInfo type);
    void Read(TObjectPtr object, const CTypeRef& type);

    void Skip(TTypeInfo type);
    void Skip(const CTypeRef& type);

    CObjectInfo ReadObject(void);

    virtual string ReadTypeName(void);
    
    // try to read enum value name, "" if none
    virtual pair<long, bool> ReadEnum(const CEnumeratedTypeValues& values);

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
            ReadString(data);
        }
    virtual void ReadStringStore(string& s);

    // std C types readers
    void SkipStd(const bool &)
        {
            SkipBool();
        }
    void SkipStd(const char& )
        {
            SkipChar();
        }
    void SkipStd(const signed char& )
        {
            SkipSChar();
        }
    void SkipStd(const unsigned char& )
        {
            SkipUChar();
        }
    void SkipStd(const short& )
        {
            SkipShort();
        }
    void SkipStd(const unsigned short& )
        {
            SkipUShort();
        }
    void SkipStd(const int& )
        {
            SkipInt();
        }
    void SkipStd(const unsigned& )
        {
            SkipUInt();
        }
    void SkipStd(const long& )
        {
            SkipLong();
        }
    void SkipStd(const unsigned long& )
        {
            SkipULong();
        }
    void SkipStd(const float& )
        {
            SkipFloat();
        }
    void SkipStd(const double& )
        {
            SkipDouble();
        }
    void SkipStd(char* const& )
        {
            SkipCString();
        }
    void SkipStd(const char* const& )
        {
            SkipCString();
        }
    virtual void SkipString(void) = 0;
    virtual void SkipNull(void) = 0;
    virtual void SkipStringStore(void);
    virtual void SkipByteBlock(void) = 0;
    void SkipExternalObject(TTypeInfo typeInfo);

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

    void SkipPointer(TTypeInfo declaredType);
    void SkipObjectInfo(void);

    virtual void ReadNull(void) = 0;

    virtual void SkipValue(void);

    // low level readers:
    enum EFailFlags {
        eNoError = 0,
        eEOF = 1,
        eReadError = 2,
        eFormatError = 4,
        eOverflow = 8,
        eIllegalCall = 16,
        eFail = 32
    };
    bool fail(void) const
        {
            return m_Fail != 0;
        }
    unsigned GetFailFlags(void) const
        {
            return m_Fail;
        }
    unsigned SetFailFlags(unsigned flags);
    unsigned ClearFailFlags(unsigned flags)
        {
            unsigned old = m_Fail;
            m_Fail &= ~flags;
            return old;
        }
    virtual string GetPosition(void) const;

    void ThrowError1(EFailFlags fail, const char* message);
    void ThrowError1(EFailFlags fail, const string& message);
    void ThrowIOError1(CNcbiIstream& in);
    void CheckIOError1(CNcbiIstream& in)
        {
            if ( !in )
                ThrowIOError1(in);
        }

    void ThrowError1(const char* file, int line,
                     EFailFlags fail, const char* message);
    void ThrowError1(const char* file, int line,
                     EFailFlags fail, const string& message);
    void ThrowIOError1(const char* file, int line, CNcbiIstream& in);
    void CheckIOError1(const char* file, int line, CNcbiIstream& in)
        {
            if ( !in )
                ThrowIOError1(file, line, in);
        }

#ifdef _DEBUG
# define FILE_LINE __FILE__, __LINE__, 
#else
# define FILE_LINE
#endif
#define ThrowError(flag, mess) ThrowError1(FILE_LINE flag, mess)
#define ThrowIOError(in) ThrowIOError1(FILE_LINE in)
#define CheckIOError(in) CheckIOError1(FILE_LINE in)

    string MemberStack(void) const;

    typedef CTypeInfo::TMemberIndex TMemberIndex;
    typedef int TTag;

    // member interface
    class LastMember
    {
    public:
        LastMember(const CMembers& members)
            : m_Members(members), m_Index(-1)
            {
            }

        const CMembers& GetMembers(void) const
            {
                return m_Members;
            }

        TMemberIndex GetIndex(void) const
            {
                return m_Index;
            }

    private:
        const CMembers& m_Members;
        TMemberIndex m_Index;

        friend class CObjectIStream;
    };

    class StackElement
    {
        enum ENameType {
            eNameEmpty,
            eNameCharPtr,
            eNameString,
            eNameId
        };
    public:
        void SetName(const char* str)
            {
                m_NameType = eNameCharPtr;
                m_NameCharPtr = str;
            }
        void SetName(const string& str)
            {
                m_NameType = eNameString;
                m_NameString = &str;
            }
        void SetName(const CMemberId& id)
            {
                m_NameType = eNameId;
                m_NameId = &id;
            }


        StackElement(CObjectIStream& s)
            : m_Stream(s), m_Previous(s.m_CurrentElement), m_Ended(false),
              m_NameType(eNameEmpty)
            {
                s.m_CurrentElement = this;
            }
        StackElement(CObjectIStream& s, const string& str)
            : m_Stream(s), m_Previous(s.m_CurrentElement), m_Ended(false)
            {
                SetName(str);
                s.m_CurrentElement = this;
            }
        StackElement(CObjectIStream& s, const char* str)
            : m_Stream(s), m_Previous(s.m_CurrentElement), m_Ended(false)
            {
                SetName(str);
                s.m_CurrentElement = this;
            }
        ~StackElement(void)
            {
                m_Stream.m_CurrentElement = m_Previous;
            }

        CObjectIStream& GetStream(void) const
            {
                return m_Stream;
            }

        const StackElement* GetPrevous(void) const
            {
                return m_Previous;
            }

        string ToString(void) const;

        bool Ended(void) const
            {
                return m_Ended;
            }
        void End(void)
            {
                _ASSERT(!Ended());
                m_Ended = true;
            }
        bool CanClose(void) const;

    private:
        CObjectIStream& m_Stream;
        friend class CObjectIStream;
        const StackElement* m_Previous;

    protected:
        union {
            const char* m_NameCharPtr;
            const string* m_NameString;
            const CMemberId* m_NameId;
        };
        bool m_Ended;
        char m_NameType;

    private:
        // to prevent allocation in heap
        void *operator new(size_t size);
        void *operator new(size_t size, size_t count);
        // to prevent copying
        StackElement(const StackElement&);
        StackElement& operator=(const StackElement&);
    };

    class Member : public StackElement
    {
    public:
        Member(CObjectIStream& in, const CMembers& members);
        Member(CObjectIStream& in, const CMemberId& member);
        Member(CObjectIStream& in, LastMember& lastMember);
        ~Member(void);

        TMemberIndex GetIndex(void) const
            {
                return m_Index;
            }

    private:
        TMemberIndex m_Index;

        friend class CObjectIStream;
    };

    // block interface
    enum EFixed {
        eFixed
    };
    class Block : public StackElement
    {
    public:
        Block(CObjectIStream& in);
        Block(EFixed eFixed, CObjectIStream& in);
        Block(CObjectIStream& in, bool randomOrder);
        Block(EFixed eFixed, CObjectIStream& in, bool randomOrder);
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

        size_t GetNextIndex(void) const
            {
                return m_NextIndex;
            }

        size_t GetIndex(void) const
            {
                return GetNextIndex() - 1;
            }

        bool First(void) const
            {
                return GetNextIndex() == 0;
            }

        size_t GetSize(void) const
            {
                return m_Size;
            }

    protected:
        void IncIndex(void)
            {
                ++m_NextIndex;
            }

    private:
        friend class CObjectIStream;

        bool m_Fixed;
        bool m_RandomOrder;
        bool m_Finished;
        size_t m_Size;
        size_t m_NextIndex;
    };
	class ByteBlock : public StackElement
    {
	public:
		ByteBlock(CObjectIStream& in);
		~ByteBlock(void);

        bool KnownLength(void) const
        {
            return m_KnownLength;
        }

		size_t GetExpectedLength(void) const
		{
			return m_Length;
		}

		size_t Read(void* dst, size_t length, bool forceLength = false);

	private:
		bool m_KnownLength;
		size_t m_Length;

		friend class CObjectIStream;
	};

#if HAVE_NCBI_C
    // ASN.1 interface
    class AsnIo : public StackElement
    {
    public:
        AsnIo(CObjectIStream& in, const string& rootTypeName);
        ~AsnIo(void);
        operator asnio*(void)
            {
                return m_AsnIo;
            }
        asnio* operator->(void)
            {
                return m_AsnIo;
            }
        const string& GetRootTypeName(void) const
            {
                return m_RootTypeName;
            }
        size_t Read(char* data, size_t length)
            {
                return GetStream().AsnRead(*this, data, length);
            }
    private:
        string m_RootTypeName;
        asnio* m_AsnIo;

    public:
        size_t m_Count;
    };
    friend class AsnIo;
protected:
    // ASN.1 interface
    virtual unsigned GetAsnFlags(void);
    virtual void AsnOpen(AsnIo& asn);
    virtual size_t AsnRead(AsnIo& asn, char* data, size_t length);
    virtual void AsnClose(AsnIo& asn);
#endif

protected:
    friend class StackElement;
    friend class Block;
    friend class Member;
	friend class ByteBlock;

    // member interface
    virtual void StartMember(Member& member, const CMembers& members) = 0;
    virtual void StartMember(Member& member, const CMemberId& id) = 0;
    virtual void StartMember(Member& member, LastMember& lastMember) = 0;
    virtual void EndMember(const Member& member);
    static void SetIndex(LastMember& lastMember, TMemberIndex index)
        {
            lastMember.m_Index = index;
        }
    static void SetIndex(Member& member,
                         TMemberIndex index, const CMemberId& id)
        {
            member.m_Index = index;
            member.SetName(id);
        }

    // block interface
    static void SetNonFixed(Block& block, bool finished = false)
        {
            block.m_Fixed = false;
            block.m_Size = 0;
            block.m_Finished = finished;
        }
    static void SetFixed(Block& block, size_t size)
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
    CObjectInfo ReadObjectInfo(void);
    virtual EPointerType ReadPointerType(void) = 0;
    virtual TIndex ReadObjectPointer(void) = 0;
    virtual void ReadThisPointerEnd(void);
    virtual string ReadOtherPointer(void) = 0;
    virtual void ReadOtherPointerEnd(void);
    virtual bool HaveMemberSuffix(void);
    virtual TMemberIndex ReadMemberSuffix(const CMembers& members) = 0;
    void SelectMember(CObjectInfo& object);

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
    virtual void ReadString(string& s) = 0;
    virtual char* ReadCString(void);

    virtual void SkipBool(void) = 0;
    virtual void SkipChar(void) = 0;
    virtual void SkipSNumber(void) = 0;
    virtual void SkipUNumber(void);
    virtual void SkipSChar(void);
    virtual void SkipUChar(void);
    virtual void SkipShort(void);
    virtual void SkipUShort(void);
    virtual void SkipInt(void);
    virtual void SkipUInt(void);
    virtual void SkipLong(void);
    virtual void SkipULong(void);
    virtual void SkipFNumber(void) = 0;
    virtual void SkipFloat(void);
    virtual void SkipDouble(void);
    virtual void SkipCString(void);

    const CObjectInfo& GetRegisteredObject(TIndex index) const;
    void RegisterObject(TObjectPtr object, TTypeInfo typeInfo);

    void ReadData(TObjectPtr object, TTypeInfo typeInfo)
        {
            typeInfo->ReadData(*this, object);
        }

    void SkipData(TTypeInfo typeInfo)
        {
            typeInfo->SkipData(*this);
        }

private:
    vector<CObjectInfo> m_Objects;

    unsigned m_Fail;
    const StackElement* m_CurrentElement;

    CTypeMapper* m_TypeMapper;
};

//#include <serial/objistr.inl>

END_NCBI_SCOPE

#endif  /* OBJISTR__HPP */
