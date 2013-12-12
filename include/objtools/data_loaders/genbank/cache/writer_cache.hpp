#ifndef WRITER_CACHE__HPP_INCLUDED
#define WRITER_CACHE__HPP_INCLUDED

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

#include <objtools/data_loaders/genbank/writer.hpp>
#include <objtools/data_loaders/genbank/cache/reader_cache.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

BEGIN_NCBI_SCOPE

class ICache;

BEGIN_SCOPE(objects)

class CLoadLockSeq_ids;

class NCBI_XREADER_CACHE_EXPORT CCacheWriter : public CWriter,
                                               public CCacheHolder,
                                               public SCacheInfo
{
public:
    CCacheWriter(void);

    virtual void SaveStringSeq_ids(CReaderRequestResult& result,
                                   const string& seq_id);
    virtual void SaveStringGi(CReaderRequestResult& result,
                              const string& seq_id);
    virtual void SaveSeq_idSeq_ids(CReaderRequestResult& result,
                                   const CSeq_id_Handle& seq_id);
    virtual void SaveSeq_idGi(CReaderRequestResult& result,
                              const CSeq_id_Handle& seq_id);
    virtual void SaveSeq_idAccVer(CReaderRequestResult& result,
                                  const CSeq_id_Handle& seq_id);
    virtual void SaveSeq_idLabel(CReaderRequestResult& result,
                                 const CSeq_id_Handle& seq_id);
    virtual void SaveSeq_idTaxId(CReaderRequestResult& result,
                                 const CSeq_id_Handle& seq_id);
    virtual void SaveSeq_idBlob_ids(CReaderRequestResult& result,
                                    const CSeq_id_Handle& seq_id,
                                    const SAnnotSelector* sel);
    virtual void SaveBlobVersion(CReaderRequestResult& result,
                                 const TBlobId& blob_id,
                                 TBlobVersion version);

    virtual CRef<CBlobStream> OpenBlobStream(CReaderRequestResult& result,
                                             const TBlobId& blob_id,
                                             TChunkId chunk_id,
                                             const CProcessor& processor);

    virtual bool CanWrite(EType type) const;

    void WriteSeq_ids(const string& key, const CLoadLockSeq_ids& ids);

    virtual void InitializeCache(CReaderCacheManager& cache_manager,
                                 const TPluginManagerParamTree* params);
    virtual void ResetCache(void);

protected:
    class CStoreBuffer {
    public:
        CStoreBuffer(void)
            : m_Buffer(m_Buffer0),
              m_End(m_Buffer0+sizeof(m_Buffer0)),
              m_Ptr(m_Buffer)
            {
            }
        ~CStoreBuffer(void)
            {
                x_FreeBuffer();
            }
        
        const char* data(void) const
            {
                return m_Buffer;
            }
        size_t size(void) const
            {
                return m_Ptr - m_Buffer;
            }
        void CheckSpace(size_t size);
        void StoreUint4(Uint4 v)
            {
                CheckSpace(4);
                x_StoreUint4(v);
            }
        void StoreInt4(Int4 v)
            {
                StoreUint4(v);
            }
        void StoreString(const string& s);

        static Uint4 ToUint4(size_t size)
            {
                Uint4 ret = Uint4(size);
                if ( ret != size ) {
                    NCBI_THROW(CLoaderException, eLoaderFailed,
                               "Uint4 overflow");
                }
                return ret;
            }
    protected:
        void x_FreeBuffer(void);
        void x_StoreUint4(Uint4 v)
            {
                m_Ptr[0] = v>>24;
                m_Ptr[1] = v>>16;
                m_Ptr[2] = v>>8;
                m_Ptr[3] = v;
                m_Ptr += 4;
            }

    private:
        CStoreBuffer(const CStoreBuffer&);
        void operator=(const CStoreBuffer&);

        char m_Buffer0[256];
        char* m_Buffer;
        char* m_End;
        char* m_Ptr;
    };

    friend class CStoreBuffer;

    void x_WriteId(const string& key,
                   const string& subkey,
                   const char* data,
                   size_t size);
    void x_WriteId(const string& key,
                   const string& subkey,
                   const string& str)
        {
            x_WriteId(key, subkey, str.data(), str.size());
        }
    void x_WriteId(const string& key,
                   const string& subkey,
                   const CStoreBuffer& str)
        {
            x_WriteId(key, subkey, str.data(), str.size());
        }
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // WRITER_CACHE__HPP_INCLUDED
