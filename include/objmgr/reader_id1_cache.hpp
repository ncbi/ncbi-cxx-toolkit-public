#ifndef READER_ID1_CACHE__HPP_INCLUDED
#define READER_ID1_CACHE__HPP_INCLUDED

/*  $Id$
* ===========================================================================
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
* ===========================================================================
*
*  Author:  Eugene Vasilchenko, Anatoliy Kuznetsov
*
*  File Description: Cached extension of data reader from ID1
*
*/

#include <objmgr/reader_id1.hpp>

BEGIN_NCBI_SCOPE

class IBLOB_Cache;

BEGIN_SCOPE(objects)

class NCBI_XOBJMGR_EXPORT CCachedId1Reader : public CId1Reader
{
public:
    CCachedId1Reader(TConn noConn = 5);
    ~CCachedId1Reader();

protected:
    void x_ReadBlob(CID1server_back& id1_reply,
                    const CSeqref& seqref, TConn conn);
    void x_ReadBlob(CID1server_back& id1_reply,
                    CObjectIStream& in);

    CRef<CSeq_annot_SNP_Info> GetSNPAnnot(const CSeqref& seqref, TConn conn);

private:
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* $Log$
* Revision 1.1  2003/09/30 19:38:26  vasilche
* Added support for cached id1 reader.
*
*/

#endif // READER_ID1_CACHE__HPP_INCLUDED
