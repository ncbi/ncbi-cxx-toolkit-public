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
#include <corelib/ncbistre.hpp>
#include <serial/objistr.hpp>


BEGIN_NCBI_SCOPE

class CObjectIStreamAsn : public CObjectIStream
{
public:
    typedef unsigned char TByte;

    CObjectIStreamAsn(CNcbiIstream& in);

    int GetLine(void) const
        {
            return m_Line;
        }
    unsigned SetFailFlags(unsigned flags);

    virtual string ReadTypeName(void);
    virtual string ReadEnumName(void);

    virtual void ReadStd(bool& data);
    virtual void ReadStd(char& data);
    virtual void ReadStd(unsigned char& data);
    virtual void ReadStd(signed char& data);
    virtual void ReadStd(short& data);
    virtual void ReadStd(unsigned short& data);
    virtual void ReadStd(int& data);
    virtual void ReadStd(unsigned int& data);
    virtual void ReadStd(long& data);
    virtual void ReadStd(unsigned long& data);
    virtual void ReadStd(float& data);
    virtual void ReadStd(double& data);

    virtual void SkipValue(void);

    TByte ReadByte(void);
    void ReadBytes(TByte* bytes, unsigned size);
    void CheckError(void);
    TIndex ReadIndex(void);
    virtual string ReadString(void);
    string ReadId(void);

    virtual unsigned GetAsnFlags(void);
    virtual void AsnOpen(AsnIo& asn);
    virtual size_t AsnRead(AsnIo& asn, char* data, size_t length);

protected:
    virtual void VBegin(Block& block);
    virtual bool VNext(const Block& block);
    virtual void StartMember(Member& member);
	virtual void Begin(ByteBlock& block);
	virtual size_t ReadBytes(const ByteBlock& block, char* dst, size_t length);
	virtual void End(const ByteBlock& block);

private:
    virtual EPointerType ReadPointerType(void);
    virtual TIndex ReadObjectPointer(void);
    virtual string ReadOtherPointer(void);
    virtual bool HaveMemberSuffix(void);
    virtual string ReadMemberSuffix(void);

    void SkipObjectData(void);
    void SkipObjectPointer(void);
    void SkipBlock(void);

public:
    // low level methods
    char GetChar(void);
    char GetChar0(void); // get char after call to UngetChar
    void UngetChar(char c);

	// parse methods
    char GetChar(bool skipWhiteSpace);
private:
    bool GetChar(char c, bool skipWhiteSpace = false);
    void Expect(char c, bool skipWhiteSpace = false);
    bool Expect(char charTrue, char charFalse, bool skipWhiteSpace = false);
    void ExpectString(const char* s, bool skipWhiteSpace = false);

    static bool FirstIdChar(char c);
    static bool IdChar(char c);

    char SkipWhiteSpace(void);
    void SkipComments(void);
    void SkipString(void);
    void SkipBitString(void);

    CNcbiIstream& m_Input;
    int m_Line;
	int m_UngetChar;
    int m_UngetChar1;
};

//#include <objistrb.inl>

END_NCBI_SCOPE

#endif  /* OBJISTRB__HPP */
