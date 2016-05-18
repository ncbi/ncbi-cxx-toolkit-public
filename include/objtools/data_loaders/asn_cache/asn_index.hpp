#ifndef ___ASN_INDEX__HPP
#define ___ASN_INDEX__HPP

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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <corelib/ncbistd.hpp>
#include <objtools/data_loaders/asn_cache/asn_cache_export.h>
#include <db/bdb/bdb_file.hpp>

class CAsnCacheApplication;

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
class CAsnCache_DataLoader;
class CBioseq;
END_SCOPE(objects)

/////////////////////////////////////////////////////////////////////////////
///
/// This is a simple BDB structure holding information about a given accession
/// and its indexed location
///

class CAsnIndex : public CBDB_File
{
public:
    typedef string TSeqId;
    typedef Uint4  TVersion;
    typedef Uint8  TGi;
    typedef Uint4  TTimestamp;
    typedef Uint4  TChunkId;
    typedef Uint8  TOffset;
    typedef Uint4  TSize;
    typedef Uint4  TSeqLength;
    typedef Uint4  TTaxId;

    enum E_index_type { e_main, e_seq_id };

    explicit CAsnIndex(E_index_type type);

    E_index_type GetIndexType() const { return m_type; }

    string CurrentLocationAsString () const
    {
        return GetSeqId() + " | " +
            NStr::IntToString(GetVersion()) + " | " +
            NStr::UInt8ToString(GetGi()) + " | " +
            NStr::IntToString(GetTimestamp());
    }

    /// accessors
    TSeqId      GetSeqId() const;
    TVersion    GetVersion() const;
    TGi         GetGi() const;
    TTimestamp  GetTimestamp() const;
    TChunkId    GetChunkId() const;
    TOffset     GetOffset() const;
    TSize       GetSize() const;
    TSeqLength  GetSeqLength() const;
    TTaxId      GetTaxId() const;

    void SetSeqId(TSeqId val);
    void SetGi(TGi val);
    void SetVersion(TVersion val);
    void SetTimestamp(TTimestamp val);
    void SetChunkId(TChunkId val);
    void SetOffset(TOffset val);
    void SetSize(TSize val);
    void SetSeqLength(TSeqLength val);
    void SetTaxId(TTaxId val);

private:

    /// Flag indicating which type of index this is.
    E_index_type m_type;

    /// @name key
    /// @{

    /// flattened, normalized seq-id representation
    SBDB_TypeTraits<TSeqId>::TFieldType     m_SeqId;

    /// version (or 0 if not applicable)
    SBDB_TypeTraits<TVersion>::TFieldType   m_Version;

    /// associated gi
    SBDB_TypeTraits<TGi>::TFieldType        m_Gi;

    /// timestamp of update; this permist multiple gis and query by date
    SBDB_TypeTraits<TTimestamp>::TFieldType m_Timestamp;

    /// @}

    /// @name data
    /// @{

    /// file chunk and offset
    SBDB_TypeTraits<TChunkId>::TFieldType m_ChunkId;
    SBDB_TypeTraits<TOffset>::TFieldType  m_Offset;
    SBDB_TypeTraits<TSize>::TFieldType    m_Size;

    /// sequence-specific data
    SBDB_TypeTraits<TSeqLength>::TFieldType m_SeqLength;
    SBDB_TypeTraits<TTaxId>::TFieldType     m_TaxId;

    /// @}

    struct SIndexInfo {
        TSeqId      seq_id;
        TVersion    version;
        TGi         gi;
        TTimestamp  timestamp;
        TChunkId    chunk;
        TOffset     offs;
        TSize       size;
        TSeqLength  sequence_length;
        TTaxId      taxonomy_id;

        SIndexInfo(const CAsnIndex &index)
        : seq_id(index.GetSeqId())
        , version(index.GetVersion())
        , gi(index.GetGi())
        , timestamp(index.GetTimestamp())
        , chunk(index.GetChunkId())
        , offs(index.GetOffset())
        , size(index.GetSize())
        , sequence_length(index.GetSeqLength())
        , taxonomy_id(index.GetTaxId())
        {}

        SIndexInfo()
        : version(0)
        , gi(0)
        , timestamp(0)
        , chunk(0)
        , offs(0)
        , size(0)
        , sequence_length(0)
        , taxonomy_id(0)
        {}
    };

    friend CNcbiOstream &operator<<(CNcbiOstream &ostr, const CAsnIndex::SIndexInfo &info);

    friend class CAsnCache;
    friend class ::CAsnCacheApplication;
    friend class objects::CAsnCache_DataLoader;
};


size_t  IndexABioseq( const objects::CBioseq&   bioseq,
                        CAsnIndex &             index,
                        CAsnIndex::TTimestamp   timestamp,
                        CAsnIndex::TChunkId     chunk_id,
                        CAsnIndex::TOffset      offset,
                        CAsnIndex::TSize        size
                    );

void     BioseqIndexData( const objects::CBioseq&   bioseq,
                          CAsnIndex::TGi&           gi,
                          CAsnIndex::TSeqLength&    seq_length,
                          CAsnIndex::TTaxId&    taxid);

END_NCBI_SCOPE


#endif  // ___ASN_INDEX__HPP
