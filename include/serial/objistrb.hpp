#ifndef OBJISTRB__HPP
#define OBJISTRB__HPP

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
* Revision 1.19  2000/01/05 19:43:45  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.18  1999/10/04 16:22:09  vasilche
* Fixed bug with old ASN.1 structures.
*
* Revision 1.17  1999/09/24 18:19:13  vasilche
* Removed dependency on NCBI toolkit.
*
* Revision 1.16  1999/09/23 18:56:53  vasilche
* Fixed bugs with overloaded methods in objistr*.hpp & objostr*.hpp
*
* Revision 1.15  1999/09/22 20:11:49  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.14  1999/08/13 15:53:44  vasilche
* C++ analog of asntool: datatool
*
* Revision 1.13  1999/07/26 18:31:30  vasilche
* Implemented skipping of unused values.
* Added more useful error report.
*
* Revision 1.12  1999/07/22 17:33:43  vasilche
* Unified reading/writing of objects in all three formats.
*
* Revision 1.11  1999/07/21 20:02:15  vasilche
* Added embedding of ASN.1 binary output from ToolKit to our binary format.
*
* Revision 1.10  1999/07/21 14:19:58  vasilche
* Added serialization of bool.
*
* Revision 1.9  1999/07/09 16:32:53  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.8  1999/07/07 21:15:03  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.7  1999/07/02 21:31:45  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.6  1999/06/30 16:04:29  vasilche
* Added support for old ASN.1 structures.
*
* Revision 1.5  1999/06/16 20:35:23  vasilche
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
#include <vector>

BEGIN_NCBI_SCOPE

class CObjectIStreamBinary : public CObjectIStream
{
public:
    typedef unsigned char TByte;

    CObjectIStreamBinary(CNcbiIstream& in);


    virtual void ReadNull(void);
    virtual void SkipValue(void);

    TByte ReadByte(void);
    void ReadBytes(char* buffer, size_t count);
    void SkipBytes(size_t count);

protected:
    TIndex ReadIndex(void);
    unsigned ReadSize(void);

    virtual bool ReadBool(void);
    virtual char ReadChar(void);
    virtual signed char ReadSChar(void);
    virtual unsigned char ReadUChar(void);
    virtual int ReadInt(void);
    virtual unsigned ReadUInt(void);
    virtual long ReadLong(void);
    virtual unsigned long ReadULong(void);
    virtual double ReadDouble(void);
    virtual string ReadString(void);

	const string& ReadStringValue(void);

#if HAVE_NCBI_C
    virtual unsigned GetAsnFlags(void);
    virtual void AsnOpen(AsnIo& asn);
    virtual size_t AsnRead(AsnIo& asn, char* data, size_t length);
    virtual void AsnClose(AsnIo& asn);
#endif

protected:
    virtual EPointerType ReadPointerType(void);
    virtual string ReadMemberPointer(void);
    virtual TIndex ReadObjectPointer(void);
    virtual string ReadOtherPointer(void);
    
    virtual void FBegin(Block& block);
    virtual void VBegin(Block& block);
    virtual bool VNext(const Block& block);
    virtual void StartMember(Member& member);
	virtual void Begin(ByteBlock& block);
	virtual size_t ReadBytes(const ByteBlock& block, char* dst, size_t length);
    

private:
    void SkipObjectData(void);
    void SkipObjectPointer(void);
    void SkipBlock(void);

    vector<string> m_Strings;

    CNcbiIstream& m_Input;
};

//#include <objistrb.inl>

END_NCBI_SCOPE

#endif  /* OBJISTRB__HPP */
