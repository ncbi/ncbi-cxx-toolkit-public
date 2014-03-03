#ifndef WRITER__HPP_INCLUDED
#define WRITER__HPP_INCLUDED
/* */

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
*  Author:  Anton Butanaev, Eugene Vasilchenko
*
*  File Description: Base data reader interface
*
*/

#include <corelib/ncbiobj.hpp>
#include <corelib/plugin_manager.hpp>
#include <objtools/data_loaders/genbank/request_result.hpp>
#include <util/cache/icache.hpp>
#include <vector>
#include <list>

BEGIN_NCBI_SCOPE

class CByteSource;
class CByteSourceReader;

BEGIN_SCOPE(objects)

class CBlob_id;
class CSeq_id_Handle;
class CReaderRequestResult;
class CProcessor;
class CReadDispatcher;
class CReaderCacheManager;

class NCBI_XREADER_EXPORT CWriter : public CObject
{
public:
    typedef CBlob_id                                    TBlobId;
    typedef int                                         TChunkId;
    typedef int                                         TBlobState;
    typedef int                                         TBlobVersion;
    typedef int                                         TProcessorTag;

    enum EType {
        eBlobWriter,
        eIdWriter
    };

    virtual ~CWriter(void);

    virtual void SaveStringSeq_ids(CReaderRequestResult& result,
                                   const string& seq_id) = 0;
    virtual void SaveStringGi(CReaderRequestResult& result,
                              const string& seq_id) = 0;
    virtual void SaveSeq_idSeq_ids(CReaderRequestResult& result,
                                   const CSeq_id_Handle& seq_id) = 0;
    virtual void SaveSeq_idGi(CReaderRequestResult& result,
                              const CSeq_id_Handle& seq_id) = 0;
    virtual void SaveSeq_idAccVer(CReaderRequestResult& result,
                                  const CSeq_id_Handle& seq_id) = 0;
    virtual void SaveSeq_idLabel(CReaderRequestResult& result,
                                 const CSeq_id_Handle& seq_id) = 0;
    virtual void SaveSeq_idTaxId(CReaderRequestResult& result,
                                 const CSeq_id_Handle& seq_id) = 0;
    virtual void SaveSeq_idBlob_ids(CReaderRequestResult& result,
                                    const CSeq_id_Handle& seq_id,
                                    const SAnnotSelector* sel) = 0;
    virtual void SaveBlobState(CReaderRequestResult& result,
                               const TBlobId& blob_id,
                               TBlobState blob_state) = 0;
    virtual void SaveBlobVersion(CReaderRequestResult& result,
                                 const TBlobId& blob_id,
                                 TBlobVersion version) = 0;

    class NCBI_XREADER_EXPORT CBlobStream : public CObject {
    public:
        virtual ~CBlobStream(void);
        virtual bool CanWrite(void) const = 0;
        virtual CNcbiOstream& operator*(void) = 0;
        virtual void Close(void) = 0;
        virtual void Abort(void) = 0;
    };

    virtual CRef<CBlobStream> OpenBlobStream(CReaderRequestResult& result,
                                             const TBlobId& blob_id,
                                             TChunkId chunk_id,
                                             const CProcessor& processor) = 0;

    virtual bool CanWrite(EType type) const = 0;

    // helper writers
    static void WriteInt(CNcbiOstream& stream, int value);
    static void WriteBytes(CNcbiOstream& stream, CRef<CByteSource> bs);
    static void WriteBytes(CNcbiOstream& stream, CRef<CByteSourceReader> rdr);
    typedef vector<char> TOctetString;
    typedef list<TOctetString*> TOctetStringSequence;
    static void WriteBytes(CNcbiOstream& stream,
                           const TOctetString& data);
    static void WriteBytes(CNcbiOstream& stream,
                           const TOctetStringSequence& data);
    static void WriteProcessorTag(CNcbiOstream& stream,
                                  const CProcessor& processor);

    virtual void InitializeCache(CReaderCacheManager& cache_manager,
                                 const TPluginManagerParamTree* params);
    virtual void ResetCache(void);
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // WRITER__HPP_INCLUDED
