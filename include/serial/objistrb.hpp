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

    virtual TObjectPtr ReadPointer(TTypeInfo declaredType);

    virtual void SkipValue(void);

    TByte ReadByte(void);
    void ReadBytes(char* bytes, size_t size);
    TIndex ReadIndex(void);
    unsigned ReadSize(void);
    virtual string ReadString(void);
	const string& ReadStringValue(void);

protected:
    virtual void FBegin(Block& block);
    virtual bool VNext(const Block& block);
    virtual void StartMember(Member& member);
	virtual void Begin(ByteBlock& block);
	virtual size_t ReadBytes(const ByteBlock& block, char* dst, size_t length);

private:
    CIObjectInfo ReadObjectPointer(void);

    void SkipObjectData(void);
    void SkipObjectPointer(void);
    void SkipBlock(void);

    vector<string> m_Strings;

    CNcbiIstream& m_Input;
};

//#include <objistrb.inl>

END_NCBI_SCOPE

#endif  /* OBJISTRB__HPP */
