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
* Authors: Sergiy Gotvyanskyy
*
* File Description:
*   Fast, indexed accessible five column feature table reader
*
*/

#ifndef _TABLE2ASN_ANNOT_MATCH_HPP_
#define _TABLE2ASN_ANNOT_MATCH_HPP_

#include <corelib/ncbistl.hpp>
#include <corelib/ncbiobj.hpp>
#include <shared_mutex>
#include <unordered_map>
#include "atomic_bitset.hpp"

BEGIN_NCBI_SCOPE

namespace objects
{
class CSeq_annot;
class CSeq_id;
class ILineErrorListener;

namespace edit
{
    class CHugeFile;
};

};
class IIndexedFeatureReader
{
public:
    using TFile = objects::edit::CHugeFile;
    using TLogger = objects::ILineErrorListener;

    virtual ~IIndexedFeatureReader(){};
    // the method must be implemented for safe multi-threaded use
    virtual std::list<CRef<objects::CSeq_annot>> GetAndUseAnnot(CRef<objects::CSeq_id> seqid) = 0;

    // Extract CSeq_id of the annotation
    static CRef<objects::CSeq_id> GetAnnotId(const objects::CSeq_annot& annot);
};

// Five column feature table reader
class CFast5colReader: public IIndexedFeatureReader
{
public:
    struct CMemBlockInfo
    {
        size_t index     = 0;
        size_t start_pos = 0;
        size_t size      = 0;
        size_t line_no   = 0;
        std::string_view seqid;
    };

    CFast5colReader();
    ~CFast5colReader() override;

    void Init(const std::string& genome_center_id, long reader_flags, TLogger* logger = nullptr);

    // Open file in huge mode and build index
    void Open(const std::string& filename);
    // Take ownership over file and build index
    void Open(std::unique_ptr<TFile> file);
    // Open any other memory image and build index, use it for testing or debugging
    void Open(std::string_view memory);

    // How many total annotations
    size_t size() const { return m_blocks.size(); }
    size_t index_size() const { return m_blocks_map.size(); }
    size_t unused_annots() const { return m_used_annots.size(); }

    // theses methods are non-blocking and thread-safe
    // construct annot and marks it as used
    std::list<CRef<objects::CSeq_annot>> GetAndUseAnnot(CRef<objects::CSeq_id> seqid) override;
    std::list<CRef<objects::CSeq_id>> FindAnnots(CRef<objects::CSeq_id> seqid) const;

private:
    using TContainer = std::unordered_multimap<std::string, const CMemBlockInfo*>;
    using TRange = std::pair<TContainer::const_iterator, TContainer::const_iterator>;

    std::unique_ptr<TFile>   m_hugefile;
    std::list<CMemBlockInfo> m_blocks;
    CAtomicBitSet            m_used_annots;
    TContainer               m_blocks_map;
    long                     m_reader_flags = 0;
    int                      m_seqid_parse_flags = 0;
    std::string              m_seqid_prefix;
    std::string_view         m_memory;
    TLogger*                 m_logger = nullptr;

    void x_IndexFile(std::string_view memory);
    TRange x_FindAnnots(CRef<objects::CSeq_id> id) const;
};


// Any other annotation container
class CWholeFileAnnotation: public IIndexedFeatureReader
{
public:
    CWholeFileAnnotation() = default;

    using TAnnots = list<CRef<objects::CSeq_annot>>;
    using TAnnotMap = map<string, list<CRef<objects::CSeq_annot>>>;

    void Init(const std::string& genome_center_id, long reader_flags, TLogger* logger = nullptr);
    void Open(std::unique_ptr<TFile> file);
    void AddAnnots(TAnnots& annots);
    void AddAnnot(CRef<objects::CSeq_annot> annot);

    // theses methods are atomic and non-blocking
    // construct annot and marks it as used. thread-safe.
    std::list<CRef<objects::CSeq_annot>> GetAndUseAnnot(CRef<objects::CSeq_id> seqid) override;

private:
    bool s_HasPrefixMatch(const string& idString, map<string, TAnnotMap::iterator>& matchMap);
    bool x_HasMatch(bool matchVersions, const string& idString, list<CRef<objects::CSeq_annot>>& annots);
    bool x_HasExactMatch(const string& idString, list<CRef<objects::CSeq_annot>>& annots);

    std::string m_genome_center_id;
    TAnnotMap   m_AnnotMap;
    set<string> m_MatchedAnnots;
    std::shared_mutex m_Mutex;
    std::string m_seqid_prefix;
    long        m_reader_flags = 0;
    TLogger*    m_logger = nullptr;

    void        x_IndexFile();
};



END_NCBI_SCOPE

#endif
