#ifndef WGSMASTER__HPP_INCLUDED
#define WGSMASTER__HPP_INCLUDED
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
*  Author:  Eugene Vasilchenko
*
*  File Description:
*    Helper classes to propagate master WGS descriptors
*
*/

#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataLoader;
class CTSE_LoadLock;
class CTSE_Chunk_Info;

// API for WGS descriptors propagation

class NCBI_XREADER_EXPORT CWGSMasterSupport
{
public:

    // The AddWGSMaster() method should be called on loaded entry that is suspected
    // to be a WGS entry - by means of its blob id or something like that.
    // The library will check if Seq-ids of sequences in the entry match WGS pattern
    // and attach artificial split chunk with special id kMasterWGS_ChunkId (== kMax_Int-1).
    // The TSE should be assigned with Seq-entry but not marked in OM as loaded yet.
    static void AddWGSMaster(CTSE_LoadLock& lock);

    // The LoadWGSMaster() method should be called when OM requests to load
    // the artificial chunk with id kMasterWGS_ChunkId.
    // The loader argument will be used to retrieve master WGS entry using method
    // GetRecordsNoBlobState() (default CDataLoader implementation calls GetRecords()).
    static void LoadWGSMaster(CDataLoader* loader,
                              CRef<CTSE_Chunk_Info> chunk);

};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//WGSMASTER__HPP_INCLUDED
