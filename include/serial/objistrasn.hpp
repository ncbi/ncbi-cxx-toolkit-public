#ifndef OBJISTRASN__HPP
#define OBJISTRASN__HPP

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
* Revision 1.25  2000/02/17 20:02:28  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.24  2000/02/11 17:10:19  vasilche
* Optimized text parsing.
*
* Revision 1.23  2000/02/01 21:44:35  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Added buffering to CObjectIStreamAsn.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
*
* Revision 1.22  2000/01/10 20:12:37  vasilche
* Fixed duplicate argument names.
* Fixed conflict between template and variable name.
*
* Revision 1.21  2000/01/10 19:46:31  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.20  2000/01/05 19:43:44  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.19  1999/09/24 18:19:13  vasilche
* Removed dependency on NCBI toolkit.
*
* Revision 1.18  1999/09/23 18:56:52  vasilche
* Fixed bugs with overloaded methods in objistr*.hpp & objostr*.hpp
*
* Revision 1.17  1999/09/22 20:11:49  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.16  1999/08/17 15:13:03  vasilche
* Comments are allowed in ASN.1 text files.
* String values now parsed in accordance with ASN.1 specification.
*
* Revision 1.15  1999/08/13 15:53:43  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.14  1999/07/26 18:31:29  vasilche
* Implemented skipping of unused values.
* Added more useful error report.
*
* Revision 1.13  1999/07/22 17:33:41  vasilche
* Unified reading/writing of objects in all three formats.
*
* Revision 1.12  1999/07/21 14:19:57  vasilche
* Added serialization of bool.
*
* Revision 1.11  1999/07/19 15:50:16  vasilche
* Added interface to old ASN.1 routines.
* Added naming of key/value in STL map.
*
* Revision 1.10  1999/07/09 16:32:53  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.9  1999/07/07 21:15:03  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.8  1999/07/07 19:58:45  vasilche
* Reduced amount of data allocated on heap
* Cleaned ASN.1 structures info
*
* Revision 1.7  1999/07/07 18:18:32  vasilche
* Fixed some bugs found by MS VC++
*
* Revision 1.6  1999/07/02 21:31:44  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.5  1999/06/30 16:04:28  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.4  1999/06/18 16:26:49  vasilche
* Fixed bug with unget() in MSVS
*
* Revision 1.3  1999/06/17 21:08:49  vasilche
* Fixed bug with unget()
*
* Revision 1.2  1999/06/17 20:42:01  vasilche
* Fixed storing/loading of pointers.
*
* Revision 1.1  1999/06/16 20:35:22  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.4  1999/06/15 16:20:02  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.3  1999/06/10 21:06:38  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.2  1999/06/04 20:51:32  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:25  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objistr.hpp>
#include <serial/strbuffer.hpp>

BEGIN_NCBI_SCOPE

class CObjectIStreamAsn : public CObjectIStream
{
public:
    typedef unsigned char TByte;

    CObjectIStreamAsn(CNcbiIstream& in);

    virtual unsigned SetFailFlags(unsigned flags);

    virtual string ReadTypeName(void);
    virtual pair<long, bool> ReadEnum(const CEnumeratedTypeValues& values);

    virtual void ReadNull(void);

    virtual void SkipValue(void);

    TByte ReadByte(void);
    void ReadBytes(TByte* bytes, unsigned size);

protected:
    TIndex ReadIndex(void);

    // action: read ID into local buffer
    // return: ID pointer and length
    // note: it is not zero ended
    pair<const char*, size_t> ReadId(void);

    virtual bool ReadBool(void);
    virtual char ReadChar(void);
    virtual int ReadInt(void);
    virtual unsigned ReadUInt(void);
    virtual long ReadLong(void);
    virtual unsigned long ReadULong(void);
    virtual double ReadDouble(void);
    virtual void ReadString(string& s);

#if HAVE_NCBI_C
    virtual unsigned GetAsnFlags(void);
    virtual void AsnOpen(AsnIo& asn);
    virtual size_t AsnRead(AsnIo& asn, char* data, size_t length);
#endif

protected:
    virtual void VBegin(Block& block);
    virtual bool VNext(const Block& block);
    virtual void StartMember(Member& member, const CMembers& members);
    virtual void StartMember(Member& member, const CMemberId& id);
    virtual void StartMember(Member& member, LastMember& lastMember);
	virtual void Begin(ByteBlock& block);
    int GetHexChar(void);
	virtual size_t ReadBytes(const ByteBlock& block, char* dst, size_t length);
	virtual void End(const ByteBlock& block);

private:
    virtual EPointerType ReadPointerType(void);
    virtual TIndex ReadObjectPointer(void);
    virtual string ReadOtherPointer(void);
    virtual bool HaveMemberSuffix(void);
    virtual TMemberIndex ReadMemberSuffix(const CMembers& members);

    void SkipObjectData(void);
    void SkipObjectPointer(void);
    void SkipBlock(void);

public:
    // low level methods
    char GetChar(void);
    char PeekChar(void);
    void SkipChar(void);

	// parse methods
    char GetChar(bool skipWhiteSpace);
    char PeekChar(bool skipWhiteSpace);
private:
    bool GetChar(char c, bool skipWhiteSpace = false);
    void Expect(char c, bool skipWhiteSpace = false);
    bool Expect(char charTrue, char charFalse, bool skipWhiteSpace = false);
    void ExpectString(const char* s, bool skipWhiteSpace = false);

    static bool FirstIdChar(char c);
    static bool IdChar(char c);

    void SkipEndOfLine(char c);
    char SkipWhiteSpace(void);
    char SkipWhiteSpaceAndGetChar(void);
    void SkipComments(void);
    void SkipString(void);
    void SkipBitString(void);

    CStreamBuffer m_Input;
};

//#include <objistrb.inl>

END_NCBI_SCOPE

#endif  /* OBJISTRB__HPP */
