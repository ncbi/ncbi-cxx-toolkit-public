#ifndef DISPATCHER__HPP_INCLUDED
#define DISPATCHER__HPP_INCLUDED
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
*  Author:  Eugene Vasilchenko
*
*  File Description: Dispatcher of readers/writers
*
*/

#include <corelib/ncbiobj.hpp>
#include <corelib/plugin_manager.hpp>
#include <objtools/data_loaders/genbank/reader.hpp>
#include <objtools/data_loaders/genbank/writer.hpp>
#include <objtools/data_loaders/genbank/processor.hpp>
#include <objtools/data_loaders/genbank/reader_snp.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CBlob_id;
class CSeq_id_Handle;
class CTSE_Info;
class CTSE_Chunk_Info;
class CSeq_annot_SNP_Info;

class CReaderRequestResult;
class CLoadLockBlob_ids;
class CLoadLockBlob;
class CReadDispatcherCommand;

class NCBI_XREADER_EXPORT CReadDispatcher : public CObject
{
public:
    typedef  CReader::TContentsMask             TContentsMask;
    typedef  CReader::TBlobState                TBlobState;
    typedef  CReader::TBlobVersion              TBlobVersion;
    typedef  CReader::TBlobId                   TBlobId;
    typedef  CReader::TChunkId                  TChunkId;
    typedef  int                                TLevel;

    CReadDispatcher(void);
    ~CReadDispatcher(void);
    
    // insert reader and set its m_Dispatcher
    void InsertReader   (TLevel       level,  CRef<CReader>    reader);
    void InsertWriter   (TLevel       level,  CRef<CWriter>    writer);
    void InsertProcessor(CRef<CProcessor> processor);

    CWriter* GetWriter(const CReaderRequestResult& result,
                       CWriter::EType type) const;
    const CProcessor& GetProcessor(CProcessor::EType type) const;

    void LoadStringSeq_ids(CReaderRequestResult& result,
                           const string& seq_id);
    void LoadSeq_idBlob_ids(CReaderRequestResult& result,
                            const CSeq_id_Handle& seq_id);
    void LoadSeq_idSeq_ids(CReaderRequestResult& result,
                           const CSeq_id_Handle& seq_id);
    void LoadSeq_idGi(CReaderRequestResult& result,
                      const CSeq_id_Handle& seq_id);
    void LoadBlobVersion(CReaderRequestResult& result,
                         const TBlobId& blob_id);
    /*
    void LoadBlobs(CReaderRequestResult& result,
                   const string& seq_id,
                   TContentsMask mask);
    */
    void LoadBlobs(CReaderRequestResult& result,
                   const CSeq_id_Handle& seq_id,
                   TContentsMask mask);
    void LoadBlobs(CReaderRequestResult& result,
                   CLoadLockBlob_ids blobs,
                   TContentsMask mask);
    void LoadBlob(CReaderRequestResult& result,
                  const CBlob_id& blob_id);
    void LoadChunk(CReaderRequestResult& result,
                   const TBlobId& blob_id, TChunkId chunk_id);

    void SetAndSaveBlobState(CReaderRequestResult& result,
                             const TBlobId& blob_id,
                             TBlobState state) const;
    void SetAndSaveBlobState(CReaderRequestResult& result,
                             const TBlobId& blob_id,
                             CLoadLockBlob& blob,
                             TBlobState state) const;
    void SetAndSaveBlobVersion(CReaderRequestResult& result,
                               const TBlobId& blob_id,
                               TBlobVersion version) const;
    void SetAndSaveBlobVersion(CReaderRequestResult& result,
                               const TBlobId& blob_id,
                               CLoadLockBlob& blob,
                               TBlobVersion version) const;

    void CheckReaders(void) const;
    void Process(CReadDispatcherCommand& command);
    
private:
    typedef map< TLevel,       CRef<CReader> >    TReaders;
    typedef map< TLevel,       CRef<CWriter> >    TWriters;
    typedef map< CProcessor::EType, CRef<CProcessor> > TProcessors;

    TReaders    m_Readers;
    TWriters    m_Writers;
    TProcessors m_Processors;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//DISPATCHER__HPP_INCLUDED
