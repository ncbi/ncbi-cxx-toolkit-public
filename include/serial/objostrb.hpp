#ifndef OBJOSTRB__HPP
#define OBJOSTRB__HPP

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
* Revision 1.10  1999/07/22 17:33:46  vasilche
* Unified reading/writing of objects in all three formats.
*
* Revision 1.9  1999/07/21 20:02:16  vasilche
* Added embedding of ASN.1 binary output from ToolKit to our binary format.
*
* Revision 1.8  1999/07/21 14:20:00  vasilche
* Added serialization of bool.
*
* Revision 1.7  1999/07/09 16:32:54  vasilche
* Added OCTET STRING write/read.
*
* Revision 1.6  1999/07/07 21:15:03  vasilche
* Cleaned processing of string types (string, char*, const char*).
*
* Revision 1.5  1999/07/02 21:31:48  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.4  1999/06/16 20:35:25  vasilche
* Cleaned processing of blocks of data.
* Added input from ASN.1 text format.
*
* Revision 1.3  1999/06/15 16:20:04  vasilche
* Added ASN.1 object output stream.
*
* Revision 1.2  1999/06/10 21:06:40  vasilche
* Working binary output and almost working binary input.
*
* Revision 1.1  1999/06/07 19:30:18  vasilche
* More bug fixes
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/objostr.hpp>
#include <map>

BEGIN_NCBI_SCOPE

class CObjectOStreamBinary : public CObjectOStream
{
public:
    typedef unsigned char TByte;
    typedef map<string, TIndex> TStrings;

    CObjectOStreamBinary(CNcbiOstream& out);
    virtual ~CObjectOStreamBinary(void);

    virtual void WriteStd(const bool& data);
    virtual void WriteStd(const char& data);
    virtual void WriteStd(const unsigned char& data);
    virtual void WriteStd(const signed char& data);
    virtual void WriteStd(const short& data);
    virtual void WriteStd(const unsigned short& data);
    virtual void WriteStd(const int& data);
    virtual void WriteStd(const unsigned int& data);
    virtual void WriteStd(const long& data);
    virtual void WriteStd(const unsigned long& data);
    virtual void WriteStd(const float& data);
    virtual void WriteStd(const double& data);

    void WriteNull(void);
    void WriteByte(TByte byte);
    void WriteBytes(const char* bytes, size_t size);
    void WriteIndex(TIndex index);
    void WriteSize(unsigned size);
    virtual void WriteString(const string& str);
    virtual void WriteCString(const char* str);

    virtual unsigned GetAsnFlags(void);
    virtual void AsnWrite(AsnIo& asn, const char* data, size_t length);

protected:

    virtual void WriteMemberPrefix(const CMemberId& id);
    virtual void WriteNullPointer(void);
    virtual void WriteObjectReference(TIndex index);
    virtual void WriteThis(TConstObjectPtr object, TTypeInfo typeInfo);
    virtual void WriteOther(TConstObjectPtr object, TTypeInfo typeInfo);

    virtual void FBegin(Block& block);
    virtual void VNext(const Block& block);
    virtual void VEnd(const Block& block);
    virtual void StartMember(Member& member, const CMemberId& id);
	virtual void Begin(const ByteBlock& block);
	virtual void WriteBytes(const ByteBlock& block, const char* bytes, size_t length);

private:
    TStrings m_Strings;

	void WriteStringValue(const string& str);

    CNcbiOstream& m_Output;
};

//#include <serial/objostrb.inl>

END_NCBI_SCOPE

#endif  /* OBJOSTRB__HPP */
