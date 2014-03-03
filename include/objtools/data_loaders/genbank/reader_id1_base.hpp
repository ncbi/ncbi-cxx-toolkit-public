#ifndef READER_ID1_BASE__HPP_INCLUDED
#define READER_ID1_BASE__HPP_INCLUDED
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
*  File Description: Base class for ID1 and PubSeqOS readers
*
*/

#include <corelib/ncbiobj.hpp>
#include <objtools/data_loaders/genbank/reader.hpp>

//#define GENBANK_USE_SNP_SATELLITE_15 1

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CTSE_Info;
class CTSE_Chunk_Info;
class CSeq_annot_SNP_Info;
class CLoadLockBlob_ids;

class NCBI_XREADER_EXPORT CId1ReaderBase : public CReader
{
public:
    CId1ReaderBase(void);
    ~CId1ReaderBase(void);

    bool LoadStringSeq_ids(CReaderRequestResult& result,
                           const string& seq_id);
    /*
    bool LoadSeq_idSeq_ids(CReaderRequestResult& result,
                           const CSeq_id_Handle& seq_id);
    bool LoadSeq_idBlob_ids(CReaderRequestResult& result,
                            const CSeq_id_Handle& seq_id,
                            const SAnnotSelector* sel);
    */
    bool LoadBlob(CReaderRequestResult& result,
                  const TBlobId& blob_id);
    bool LoadBlobState(CReaderRequestResult& result,
                       const TBlobId& blob_id);
    bool LoadBlobVersion(CReaderRequestResult& result,
                         const TBlobId& blob_id);
    bool LoadChunk(CReaderRequestResult& result,
                   const TBlobId& blob_id, TChunkId chunk_id);

    virtual void GetBlobState(CReaderRequestResult& result,
                              const CBlob_id& blob_id) = 0;
    virtual void GetBlobVersion(CReaderRequestResult& result,
                                const CBlob_id& blob_id) = 0;

    virtual void GetBlob(CReaderRequestResult& result,
                         const TBlobId& blob_id,
                         TChunkId chunk_id) = 0;

    enum ESat {
        eSat_ANNOT_CDD  = 10,
        eSat_ANNOT      = 26,
        eSat_TRACE      = 28,
        eSat_TRACE_ASSM = 29,
        eSat_TR_ASSM_CH = 30,
        eSat_TRACE_CHGR = 31
    };

    enum ESubSat {
        eSubSat_main =    0,
        eSubSat_SNP  = 1<<0,
        eSubSat_SNP_graph = 1<<2,
        eSubSat_CDD  = 1<<3,
        eSubSat_MGC  = 1<<4,
        eSubSat_HPRD = 1<<5,
        eSubSat_STS  = 1<<6,
        eSubSat_tRNA = 1<<7,
        eSubSat_microRNA = 1<<8,
        eSubSat_Exon = 1<<9
    };

    static bool IsAnnotSat(int sat);
    static ESat GetAnnotSat(int subsat);
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//READER_ID1_BASE__HPP_INCLUDED
