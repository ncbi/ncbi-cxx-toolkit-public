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
* Revision 1.64  2001/05/11 20:41:11  grichenk
* Added support for non-blocking stream reading
*
* Revision 1.63  2001/01/05 20:10:35  vasilche
* CByteSource, CIStrBuffer, COStrBuffer, CLightString, CChecksum, CWeakMap
* were moved to util.
*
* Revision 1.62  2001/01/05 13:57:18  vasilche
* Fixed overloaded method ambiguity on Mac.
*
* Revision 1.61  2000/12/26 22:23:44  vasilche
* Fixed errors of compilation on Mac.
*
* Revision 1.60  2000/12/26 17:26:09  vasilche
* Added one more Read() interface method.
*
* Revision 1.59  2000/12/15 21:28:46  vasilche
* Moved some typedefs/enums from corelib/ncbistd.hpp.
* Added flags to CObjectIStream/CObjectOStream: eFlagAllowNonAsciiChars.
* TByte typedef replaced by Uint1.
*
* Revision 1.58  2000/12/15 15:37:59  vasilche
* Added support of Int8 and long double.
* Enum values now have type Int4 instead of long.
*
* Revision 1.57  2000/10/20 19:29:15  vasilche
* Adapted for MSVC which doesn't like explicit operator templates.
*
* Revision 1.56  2000/10/20 15:51:26  vasilche
* Fixed data error processing.
* Added interface for costructing container objects directly into output stream.
* object.hpp, object.inl and object.cpp were split to
* objectinfo.*, objecttype.*, objectiter.* and objectio.*.
*
* Revision 1.55  2000/10/17 18:45:24  vasilche
* Added possibility to turn off object cross reference detection in
* CObjectIStream and CObjectOStream.
*
* Revision 1.54  2000/10/13 20:59:12  vasilche
* Avoid using of ssize_t absent on some compilers.
*
* Revision 1.53  2000/10/13 20:22:46  vasilche
* Fixed warnings on 64 bit compilers.
* Fixed missing typename in templates.
*
* Revision 1.52  2000/10/13 16:28:31  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* Revision 1.51  2000/10/03 17:22:34  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.50  2000/09/29 16:18:13  vasilche
* Fixed binary format encoding/decoding on 64 bit compulers.
* Implemented CWeakMap<> for automatic cleaning map entries.
* Added cleaning local hooks via CWeakMap<>.
* Renamed ReadTypeName -> ReadFileHeader, ENoTypeName -> ENoFileHeader.
* Added some user interface methods to CObjectIStream, CObjectOStream and
* CObjectStreamCopier.
*
* Revision 1.49  2000/09/18 20:00:04  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.48  2000/09/01 13:16:00  vasilche
* Implemented class/container/choice iterators.
* Implemented CObjectStreamCopier for copying data without loading into memory.
*
* Revision 1.47  2000/08/15 19:44:40  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.46  2000/07/03 18:42:35  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.45  2000/06/16 20:01:20  vasilche
* Avoid use of unexpected_exception() which is unimplemented on Mac.
*
* Revision 1.44  2000/06/16 16:31:06  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.43  2000/06/07 19:45:42  vasilche
* Some code cleaning.
* Macros renaming in more clear way.
* BEGIN_NAMED_*_INFO, ADD_*_MEMBER, ADD_NAMED_*_MEMBER.
*
* Revision 1.42  2000/06/01 19:06:56  vasilche
* Added parsing of XML data.
*
* Revision 1.41  2000/05/24 20:08:13  vasilche
* Implemented XML dump.
*
* Revision 1.40  2000/05/09 16:38:33  vasilche
* CObject::GetTypeInfo now moved to CObjectGetTypeInfo::GetTypeInfo to reduce possible errors.
* Added write context to CObjectOStream.
* Inlined most of methods of helping class Member, Block, ByteBlock etc.
*
* Revision 1.39  2000/05/04 16:22:23  vasilche
* Cleaned and optimized blocks and members.
*
* Revision 1.38  2000/04/28 16:58:02  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
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
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbiutil.hpp>
#include <serial/serialdef.hpp>
#include <serial/typeinfo.hpp>
#include <util/strbuffer.hpp>
#include <serial/objstack.hpp>
#include <serial/objhook.hpp>
#include <serial/hookdatakey.hpp>

struct asnio;

BEGIN_NCBI_SCOPE

class CMemberId;
class CItemsInfo;
class CItemInfo;
class CMemberInfo;
class CVariantInfo;
class CDelayBuffer;
class CByteSource;
class CByteSourceReader;

class CObjectInfo;
class CObjectInfoMI;

class CClassTypeInfo;
class CChoiceTypeInfo;
class CContainerTypeInfo;
class CObjectStreamCopier;

class CReadObjectHook;
class CReadClassMemberHook;
class CReadChoiceVariantHook;

class CReadObjectInfo;
class CReadObjectList;

class CObjectIStream : public CObjectStack
{
public:
    // typedefs
    typedef size_t TObjectIndex;

    // open methods
    virtual void Open(const CRef<CByteSourceReader>& reader);
    void Open(const CRef<CByteSource>& source);
    void Open(CNcbiIstream& inStream, bool deleteInStream = false);
    void Close(void);

    static CObjectIStream* Open(ESerialDataFormat format,
                                CNcbiIstream& inStream,
                                bool deleteInStream = false,
                                bool use_non_blocking_read = false);
    static CObjectIStream* Open(ESerialDataFormat format,
                                const string& fileName,
                                unsigned openFlags = 0);
    static CObjectIStream* Open(const string& fileName,
                                ESerialDataFormat format);

    bool DetectLoops(void) const;
    void DetectLoops(bool detectLoops);

    // constructors
protected:
    CObjectIStream(void);
    CObjectIStream(CNcbiIstream& in, bool deleteIn = false);

public:
    virtual ~CObjectIStream(void);

    // get data format
    virtual ESerialDataFormat GetDataFormat(void) const = 0;

    // USER INTERFACE
    // root reader
    void Read(const CObjectInfo& object);
    void Read(TObjectPtr object, TTypeInfo type);
    CObjectInfo Read(const CObjectTypeInfo& type);
    CObjectInfo Read(TTypeInfo type);
    void Skip(const CObjectTypeInfo& type);
    void Skip(TTypeInfo type);

    // file header readers
    virtual string ReadFileHeader(void);
    void SkipFileHeader(TTypeInfo typeInfo);

    enum ENoFileHeader {
        eNoFileHeader
    };
    void Read(const CObjectInfo& object, ENoFileHeader noFileHeader);
    void Read(TObjectPtr object, TTypeInfo type, ENoFileHeader noFileHeader);
    void Skip(TTypeInfo type, ENoFileHeader noFileHeader);

    // subtree reader
    void ReadObject(const CObjectInfo& object);
    void ReadObject(TObjectPtr object, TTypeInfo typeInfo);
    void SkipObject(const CObjectTypeInfo& objectType);
    void SkipObject(TTypeInfo typeInfo);

    // temporary reader
    void ReadSeparateObject(const CObjectInfo& object);

	// member
	void ReadClassMember(const CObjectInfoMI& member);

    // END OF USER INTERFACE

    // internal reader
    void ReadExternalObject(TObjectPtr object, TTypeInfo typeInfo);
    void SkipExternalObject(TTypeInfo typeInfo);

    CObjectInfo ReadObject(void);

    virtual void EndOfRead(void);
    
    // try to read enum value name, "" if none
    virtual TEnumValueType ReadEnum(const CEnumeratedTypeValues& values) = 0;

    // std C types readers
    // bool
    void ReadStd(bool& data);
    void SkipStd(const bool &);

    // char
    void ReadStd(char& data);
    void SkipStd(const char& );

    // integer numbers
    void ReadStd(signed char& data);
    void ReadStd(unsigned char& data);
    void SkipStd(const signed char& );
    void SkipStd(const unsigned char& );
    void ReadStd(short& data);
    void ReadStd(unsigned short& data);
    void SkipStd(const short& );
    void SkipStd(const unsigned short& );
    void ReadStd(int& data);
    void ReadStd(unsigned& data);
    void SkipStd(const int& );
    void SkipStd(const unsigned& );
#if SIZEOF_LONG == 4
    void ReadStd(long& data);
    void ReadStd(unsigned long& data);
    void SkipStd(const long& );
    void SkipStd(const unsigned long& );
#endif
    void ReadStd(Int8& data);
    void ReadStd(Uint8& data);
    void SkipStd(const Int8& );
    void SkipStd(const Uint8& );

    // float numbers
    void ReadStd(float& data);
    void ReadStd(double& data);
    void SkipStd(const float& );
    void SkipStd(const double& );
#if SIZEOF_LONG_DOUBLE != 0
    virtual void ReadStd(long double& data);
    virtual void SkipStd(const long double& );
#endif

    // string
    void ReadStd(string& data);
    void SkipStd(const string& );

    // C string
    void ReadStd(char* & data);
    void ReadStd(const char* & data);
    void SkipStd(char* const& );
    void SkipStd(const char* const& );

    // primitive readers
    // bool
    virtual bool ReadBool(void) = 0;
    virtual void SkipBool(void) = 0;

    // char
    virtual char ReadChar(void) = 0;
    virtual void SkipChar(void) = 0;

    // integer numbers
    virtual Int1 ReadInt1(void);
    virtual Uint1 ReadUint1(void);
    virtual Int2 ReadInt2(void);
    virtual Uint2 ReadUint2(void);
    virtual Int4 ReadInt4(void);
    virtual Uint4 ReadUint4(void);
    virtual Int8 ReadInt8(void) = 0;
    virtual Uint8 ReadUint8(void) = 0;

    virtual void SkipInt1(void);
    virtual void SkipUint1(void);
    virtual void SkipInt2(void);
    virtual void SkipUint2(void);
    virtual void SkipInt4(void);
    virtual void SkipUint4(void);
    virtual void SkipInt8(void);
    virtual void SkipUint8(void);

    virtual void SkipSNumber(void) = 0;
    virtual void SkipUNumber(void) = 0;

    // float numbers
    virtual float ReadFloat(void);
    virtual double ReadDouble(void) = 0;
    virtual void SkipFloat(void);
    virtual void SkipDouble(void);
#if SIZEOF_LONG_DOUBLE != 0
    virtual long double ReadLDouble(void);
    virtual void SkipLDouble(void);
#endif
    virtual void SkipFNumber(void) = 0;

    // string
    virtual void ReadString(string& s) = 0;
    virtual void SkipString(void) = 0;

    // StringStore
    virtual void ReadStringStore(string& s);
    virtual void SkipStringStore(void);
    
    // C string
    virtual char* ReadCString(void);
    virtual void SkipCString(void);

    // null
    virtual void ReadNull(void) = 0;
    virtual void SkipNull(void) = 0;

    // octet string
    virtual void SkipByteBlock(void) = 0;

    // reads type info
    virtual pair<TObjectPtr, TTypeInfo> ReadPointer(TTypeInfo declaredType);
    enum EPointerType {
        eNullPointer,
        eObjectPointer,
        eThisPointer,
        eOtherPointer
    };

    void SkipPointer(TTypeInfo declaredType);
    void SkipObjectInfo(void);

    // low level readers:
    enum EFailFlags {
        eNoError       = 0,
        eEOF           = 1 << 0,
        eReadError     = 1 << 1,
        eFormatError   = 1 << 2,
        eOverflow      = 1 << 3,
        eInvalidData   = 1 << 4,
        eIllegalCall   = 1 << 5,
        eFail          = 1 << 6,
        eNotOpen       = 1 << 7
    };
    bool fail(void) const;
    unsigned GetFailFlags(void) const;
    unsigned SetFailFlags(unsigned flags, const char* message);
    unsigned ClearFailFlags(unsigned flags);
    bool InGoodState(void);
    virtual string GetStackTrace(void) const;
    virtual string GetPosition(void) const = 0;

    void ThrowError1(EFailFlags fail, const char* message);
    void ThrowError1(EFailFlags fail, const string& message);
    void ThrowError1(const char* file, int line,
                     EFailFlags fail, const char* message);
    void ThrowError1(const char* file, int line,
                     EFailFlags fail, const string& message);

#ifndef FILE_LINE
# ifdef _DEBUG
#  define FILE_LINE __FILE__, __LINE__, 
# else
#  define FILE_LINE
# endif
# define ThrowError(flag, mess) ThrowError1(FILE_LINE flag, mess)
# define ThrowIOError(in) ThrowIOError1(FILE_LINE in)
# define CheckIOError(in) CheckIOError1(FILE_LINE in)
#endif

    enum EFlags {
        eFlagNone                = 0,
        eFlagAllowNonAsciiChars  = 1 << 0
    };
    unsigned GetFlags(void) const;
    unsigned SetFlags(unsigned flags);
    unsigned ClearFlags(unsigned flags);

	class ByteBlock
    {
	public:
		ByteBlock(CObjectIStream& in);
		~ByteBlock(void);

        void End(void);

        CObjectIStream& GetStream(void) const;

		size_t Read(void* dst, size_t length, bool forceLength = false);

        bool KnownLength(void) const;
		size_t GetExpectedLength(void) const;

        void SetLength(size_t length);
        void EndOfBlock(void);
        
	private:
        CObjectIStream& m_Stream;
		bool m_KnownLength;
        bool m_Ended;
		size_t m_Length;

		friend class CObjectIStream;
	};

#if HAVE_NCBI_C
    // ASN.1 interface
    class AsnIo
    {
    public:
        AsnIo(CObjectIStream& in, const string& rootTypeName);
        ~AsnIo(void);

        void End(void);

        CObjectIStream& GetStream(void) const;

        size_t Read(char* data, size_t length);
        
        operator asnio*(void);
        asnio* operator->(void);
        const string& GetRootTypeName(void) const;

    private:
        CObjectIStream& m_Stream;
        bool m_Ended;
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
public:
#endif
    
    // mid level I/O
    // named type
    MLIOVIR void ReadNamedType(TTypeInfo namedTypeInfo,
                               TTypeInfo typeInfo, TObjectPtr object);
    MLIOVIR void SkipNamedType(TTypeInfo namedTypeInfo,
                               TTypeInfo typeInfo);

    // container
    MLIOVIR void ReadContainer(const CContainerTypeInfo* containerType,
                               TObjectPtr containerPtr);
    MLIOVIR void SkipContainer(const CContainerTypeInfo* containerType);
    
    // class
    MLIOVIR void ReadClassSequential(const CClassTypeInfo* classType,
                                     TObjectPtr classPtr);
    MLIOVIR void ReadClassRandom(const CClassTypeInfo* classType,
                                 TObjectPtr classPtr);
    MLIOVIR void SkipClassSequential(const CClassTypeInfo* classType);
    MLIOVIR void SkipClassRandom(const CClassTypeInfo* classType);

    // choice
    MLIOVIR void ReadChoice(const CChoiceTypeInfo* choiceType,
                            TObjectPtr choicePtr);
    MLIOVIR void SkipChoice(const CChoiceTypeInfo* choiceType);

    // low level I/O
    // named type (alias)
    virtual void BeginNamedType(TTypeInfo namedTypeInfo);
    virtual void EndNamedType(void);

    // container
    virtual void BeginContainer(const CContainerTypeInfo* containerType) = 0;
    virtual void EndContainer(void) = 0;
    virtual bool BeginContainerElement(TTypeInfo elementType) = 0;
    virtual void EndContainerElement(void);

    // class
    virtual void BeginClass(const CClassTypeInfo* classInfo) = 0;
    virtual void EndClass(void);

    virtual TMemberIndex BeginClassMember(const CClassTypeInfo* classType) = 0;
    virtual TMemberIndex BeginClassMember(const CClassTypeInfo* classType,
                                          TMemberIndex pos) = 0;
    virtual void EndClassMember(void);

    // choice
    virtual TMemberIndex BeginChoiceVariant(const CChoiceTypeInfo* choiceType) = 0;
    virtual void EndChoiceVariant(void);

    // byte block
	virtual void BeginBytes(ByteBlock& block) = 0;
	virtual size_t ReadBytes(ByteBlock& block, char* buffer, size_t count) = 0;
	virtual void EndBytes(const ByteBlock& block);

    // report error about unended block
    void Unended(const string& msg);
    // report error about unended object stack frame
    virtual void UnendedFrame(void);

    // report error about class members
    void DuplicatedMember(const CMemberInfo* memberInfo);
    void ExpectedMember(const CMemberInfo* memberInfo);

    void StartDelayBuffer(void);
    void EndDelayBuffer(CDelayBuffer& buffer,
                        const CItemInfo* itemInfo, TObjectPtr objectPtr);

protected:
    friend class CObjectStreamCopier;

    // low level readers
    pair<TObjectPtr, TTypeInfo> ReadObjectInfo(void);
    virtual EPointerType ReadPointerType(void) = 0;
    virtual TObjectIndex ReadObjectPointer(void) = 0;
    virtual string ReadOtherPointer(void) = 0;
    virtual void ReadOtherPointerEnd(void);

    void RegisterObject(TTypeInfo typeInfo);
    void RegisterObject(TObjectPtr object, TTypeInfo typeInfo);
    const CReadObjectInfo& GetRegisteredObject(TObjectIndex index);

public:
    // open helpers
    static CObjectIStream* Create(ESerialDataFormat format);
    static CObjectIStream* Create(ESerialDataFormat format,
                                  const CRef<CByteSource>& source);
private:
    static CRef<CByteSource> GetSource(ESerialDataFormat format,
                                       const string& fileName,
                                       unsigned openFlags = 0);
    static CRef<CByteSource> GetSource(CNcbiIstream& inStream,
                                       bool deleteInStream = false,
                                       bool use_non_blocking_read = false);

    static CObjectIStream* CreateObjectIStreamAsn(void);
    static CObjectIStream* CreateObjectIStreamAsnBinary(void);
    static CObjectIStream* CreateObjectIStreamXml(void);

protected:
    CIStreamBuffer m_Input;
    
private:
    AutoPtr<CReadObjectList> m_Objects;

    unsigned m_Fail;
    unsigned m_Flags;

public:
    // hook support
    CHookDataKey<CReadObjectHook> m_ObjectHookKey;
    CHookDataKey<CReadClassMemberHook> m_ClassMemberHookKey;
    CHookDataKey<CReadChoiceVariantHook> m_ChoiceVariantHookKey;
};

#include <serial/objistr.inl>

END_NCBI_SCOPE

#endif  /* OBJISTR__HPP */
