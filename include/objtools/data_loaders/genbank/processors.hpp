#ifndef PROCESSORS__HPP_INCLUDED
#define PROCESSORS__HPP_INCLUDED
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
*  File Description: various blob stream processors
*
*/

#include <objtools/data_loaders/genbank/processor.hpp>
#include <objtools/data_loaders/genbank/reader_snp.hpp>
#include <vector>
#include <list>

BEGIN_NCBI_SCOPE

class CByteSource;
class CByteSourceReader;
class CObjectInfo;
class CObjectIStream;
class CObjectOStream;

BEGIN_SCOPE(objects)

class CSeq_entry;
class CID1server_back;
class CLoadLockBlob;

class NCBI_XREADER_EXPORT CProcessor_ID1 : public CProcessor
{
public:
    CProcessor_ID1(CReadDispatcher& dispatcher);
    ~CProcessor_ID1(void);

    EType GetType(void) const;
    TMagic GetMagic(void) const;

    void ProcessObjStream(CReaderRequestResult& result,
                          const TBlobId& blob_id,
                          TChunkId chunk_id,
                          CObjectIStream& obj_stream) const;

    TBlobVersion GetVersion(const CID1server_back& reply) const;
    CRef<CSeq_entry> GetSeq_entry(CReaderRequestResult& result,
                                  const TBlobId& blob_id,
                                  CLoadLockBlob& blob,
                                  CID1server_back& reply) const;

    void SaveBlob(CReaderRequestResult& result,
                  const TBlobId& blob_id,
                  TChunkId chunk_id,
                  CWriter* writer,
                  CRef<CByteSource> byte_source) const;
    void SaveBlob(CReaderRequestResult& result,
                  const TBlobId& blob_id,
                  TChunkId chunk_id,
                  CWriter* writer,
                  const CID1server_back& reply) const;
};


class NCBI_XREADER_EXPORT CProcessor_ID1_SNP : public CProcessor_ID1
{
public:
    CProcessor_ID1_SNP(CReadDispatcher& dispatcher);
    ~CProcessor_ID1_SNP(void);

    EType GetType(void) const;
    TMagic GetMagic(void) const;

    void ProcessObjStream(CReaderRequestResult& result,
                          const TBlobId& blob_id,
                          TChunkId chunk_id,
                          CObjectIStream& obj_stream) const;
};


class NCBI_XREADER_EXPORT CProcessor_SE : public CProcessor
{
public:
    CProcessor_SE(CReadDispatcher& dispatcher);
    ~CProcessor_SE(void);

    EType GetType(void) const;
    TMagic GetMagic(void) const;

    void ProcessObjStream(CReaderRequestResult& result,
                          const TBlobId& blob_id,
                          TChunkId chunk_id,
                          CObjectIStream& obj_stream) const;
};


class NCBI_XREADER_EXPORT CProcessor_SE_SNP : public CProcessor_SE
{
public:
    CProcessor_SE_SNP(CReadDispatcher& dispatcher);
    ~CProcessor_SE_SNP(void);

    EType GetType(void) const;
    TMagic GetMagic(void) const;

    void ProcessObjStream(CReaderRequestResult& result,
                          const TBlobId& blob_id,
                          TChunkId chunk_id,
                          CObjectIStream& obj_stream) const;
};


class NCBI_XREADER_EXPORT CProcessor_St_SE : public CProcessor_SE
{
public:
    CProcessor_St_SE(CReadDispatcher& dispatcher);
    ~CProcessor_St_SE(void);

    EType GetType(void) const;
    TMagic GetMagic(void) const;

    void ProcessObjStream(CReaderRequestResult& result,
                          const TBlobId& blob_id,
                          TChunkId chunk_id,
                          CObjectIStream& obj_stream) const;

    TBlobState ReadBlobState(CNcbiIstream& stream) const;
    TBlobState ReadBlobState(CObjectIStream& obj_stream) const;
    void WriteBlobState(CNcbiOstream& stream,
                        TBlobState blob_state) const;
    void WriteBlobState(CObjectOStream& obj_stream,
                        TBlobState blob_state) const;

    void SaveBlob(CReaderRequestResult& result,
                  const TBlobId& blob_id,
                  TChunkId chunk_id,
                  const CLoadLockBlob& blob,
                  CWriter* writer,
                  CRef<CByteSource> byte_source) const;
    void SaveBlob(CReaderRequestResult& result,
                  const TBlobId& blob_id,
                  TChunkId chunk_id,
                  const CLoadLockBlob& blob,
                  CWriter* writer,
                  CRef<CByteSourceReader> reader) const;
    void SaveBlob(CReaderRequestResult& result,
                  const TBlobId& blob_id,
                  TChunkId chunk_id,
                  const CLoadLockBlob& blob,
                  CWriter* writer,
                  const CSeq_entry& seq_entry) const;
    typedef vector<char> TOctetString;
    typedef list<TOctetString*> TOctetStringSequence;
    void SaveBlob(CReaderRequestResult& result,
                  const TBlobId& blob_id,
                  TChunkId chunk_id,
                  const CLoadLockBlob& blob,
                  CWriter* writer,
                  const TOctetStringSequence& data) const;

    void SaveNoBlob(CReaderRequestResult& result,
                    const TBlobId& blob_id,
                    TChunkId chunk_id,
                    const CLoadLockBlob& blob,
                    CWriter* writer) const;
};


class NCBI_XREADER_EXPORT CProcessor_St_SE_SNPT : public CProcessor_St_SE
{
public:
    CProcessor_St_SE_SNPT(CReadDispatcher& dispatcher);
    ~CProcessor_St_SE_SNPT(void);

    EType GetType(void) const;
    TMagic GetMagic(void) const;

    void ProcessStream(CReaderRequestResult& result,
                       const TBlobId& blob_id,
                       TChunkId chunk_id,
                       CNcbiIstream& stream) const;

    typedef CSeq_annot_SNP_Info_Reader::TSNP_InfoMap TSNP_InfoMap;

    void SaveSNPBlob(CReaderRequestResult& result,
                     const TBlobId& blob_id,
                     TChunkId chunk_id,
                     const CLoadLockBlob& blob,
                     CWriter* writer,
                     const CSeq_entry& seq_entry,
                     const TSNP_InfoMap& snps) const;
};


class NCBI_XREADER_EXPORT CProcessor_ID2 : public CProcessor
{
public:
    CProcessor_ID2(CReadDispatcher& dispatcher);
    ~CProcessor_ID2(void);

    EType GetType(void) const;
    TMagic GetMagic(void) const;

    typedef int TSplitVersion;

    void ProcessObjStream(CReaderRequestResult& result,
                          const TBlobId& blob_id,
                          TChunkId chunk_id,
                          CObjectIStream& obj_stream) const;

    void ProcessData(CReaderRequestResult& result,
                     const TBlobId& blob_id,
                     TChunkId chunk_id,
                     const CID2_Reply_Data& data,
                     TSplitVersion split_version = 0,
                     const CID2_Reply_Data* skel = 0) const;
    
    void SaveData(CReaderRequestResult& result,
                  const TBlobId& blob_id,
                  TChunkId chunk_id,
                  CWriter* writer,
                  const CID2_Reply_Data& data) const;

    static void x_FixDataFormat(const CID2_Reply_Data& data);
    static CObjectIStream* x_OpenDataStream(const CID2_Reply_Data& data);
    static void x_ReadData(const CID2_Reply_Data& data,
                           const CObjectInfo& object);
};


class NCBI_XREADER_EXPORT CProcessor_ID2AndSkel : public CProcessor_ID2
{
public:
    CProcessor_ID2AndSkel(CReadDispatcher& dispatcher);
    ~CProcessor_ID2AndSkel(void);

    EType GetType(void) const;
    TMagic GetMagic(void) const;

    void ProcessObjStream(CReaderRequestResult& result,
                          const TBlobId& blob_id,
                          TChunkId chunk_id,
                          CObjectIStream& obj_stream) const;

    void SaveDataAndSkel(CReaderRequestResult& result,
                         const TBlobId& blob_id,
                         TChunkId chunk_id,
                         CWriter* writer,
                         TSplitVersion split_version,
                         const CID2_Reply_Data& split_data,
                         const CID2_Reply_Data& skel_data) const;
    void SaveDataAndSkel(CObjectOStream& obj_stream,
                         TSplitVersion split_version,
                         const CID2_Reply_Data& split_data,
                         const CID2_Reply_Data& skel_data) const;
};


class NCBI_XREADER_EXPORT CProcessor_ExtAnnot : public CProcessor
{
public:
    CProcessor_ExtAnnot(CReadDispatcher& dispatcher);
    ~CProcessor_ExtAnnot(void);

    EType GetType(void) const;
    TMagic GetMagic(void) const;

    void ProcessStream(CReaderRequestResult& result,
                       const TBlobId& blob_id,
                       TChunkId chunk_id,
                       CNcbiIstream& stream) const;

    enum {
        eSat_ANNOT = 26
    };
    enum {
        eSubSat_SNP         = 1<<0,
        eSubSat_SNP_graph   = 1<<2,
        eSubSat_CDD         = 1<<3,
        eSubSat_MGC         = 1<<4
    };

    static bool IsExtAnnot(const TBlobId& blob_id);
    static bool IsExtAnnot(const TBlobId& blob_id, TChunkId chunk_id);
    static bool IsExtAnnot(const TBlobId& blob_id, CLoadLockBlob& blob);

    void Process(CReaderRequestResult& result,
                 const TBlobId& blob_id,
                 TChunkId chunk_id) const;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//PROCESSORS__HPP_INCLUDED
