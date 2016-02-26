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
 * File Description: BAM file data loader
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/general/general__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqres/seqres__.hpp>

#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>
#include <objtools/readers/idmapper.hpp>

#include <sra/error_codes.hpp>
#include <sra/readers/ncbi_traces_path.hpp>
#include <sra/data_loaders/bam/bamloader.hpp>
#include <sra/data_loaders/bam/impl/bamloader_impl.hpp>

#include <util/sequtil/sequtil_manip.hpp>

#include <algorithm>
#include <cmath>

BEGIN_NCBI_SCOPE

#define NCBI_USE_ERRCODE_X   BAMLoader
NCBI_DEFINE_ERR_SUBCODE_X(10);

BEGIN_SCOPE(objects)

class CDataLoader;

static const int kTSEId = 1;
static const size_t kChunkSize = 500;
//static const size_t kGraphScale = 1000;
//static const size_t kGraphPoints = 20;
static const int kChunkIdMul = 4;
static const int kMainChunkId = -1;

enum EChunkIdType {
    eChunk_align,
    eChunk_short_seq,
    eChunk_pileup_graph
};

#define PILEUP_NAME_SUFFIX "pileup graphs"

NCBI_PARAM_DECL(int, BAM_LOADER, DEBUG);
NCBI_PARAM_DEF_EX(int, BAM_LOADER, DEBUG, 0,
                  eParam_NoThread, BAM_LOADER_DEBUG);

static int GetDebugLevel(void)
{
    static CSafeStatic<NCBI_PARAM_TYPE(BAM_LOADER, DEBUG)> s_Value;
    return s_Value->Get();
}


NCBI_PARAM_DECL(bool, BAM_LOADER, PILEUP_GRAPHS);
NCBI_PARAM_DEF_EX(bool, BAM_LOADER, PILEUP_GRAPHS, true,
                  eParam_NoThread, BAM_LOADER_PILEUP_GRAPHS);

bool CBAMDataLoader::GetPileupGraphsParamDefault(void)
{
    return NCBI_PARAM_TYPE(BAM_LOADER, PILEUP_GRAPHS)::GetDefault();
}


void CBAMDataLoader::SetPileupGraphsParamDefault(bool param)
{
    NCBI_PARAM_TYPE(BAM_LOADER, PILEUP_GRAPHS)::SetDefault(param);
}


static bool GetPileupGraphsParam(void)
{
    NCBI_PARAM_TYPE(BAM_LOADER, PILEUP_GRAPHS) s_Value;
    return s_Value.Get();
}


/////////////////////////////////////////////////////////////////////////////
// CBAMBlobId
/////////////////////////////////////////////////////////////////////////////

CBAMBlobId::CBAMBlobId(const CTempString& str)
{
    SIZE_TYPE div = str.rfind('/');
    m_BamName = str.substr(0, div);
    CSeq_id id(str.substr(div+1));
    m_SeqId = CSeq_id_Handle::GetHandle(id);
}


CBAMBlobId::CBAMBlobId(const string& bam_name, const CSeq_id_Handle& seq_id)
    : m_BamName(bam_name), m_SeqId(seq_id)
{
}


CBAMBlobId::~CBAMBlobId(void)
{
}


string CBAMBlobId::ToString(void) const
{
    return m_BamName+'/'+m_SeqId.AsString();
}


bool CBAMBlobId::operator<(const CBlobId& id) const
{
    const CBAMBlobId& bam2 = dynamic_cast<const CBAMBlobId&>(id);
    return m_SeqId < bam2.m_SeqId ||
        (m_SeqId == bam2.m_SeqId && m_BamName < bam2.m_BamName);
}


bool CBAMBlobId::operator==(const CBlobId& id) const
{
    const CBAMBlobId& bam2 = dynamic_cast<const CBAMBlobId&>(id);
    return m_SeqId == bam2.m_SeqId && m_BamName == bam2.m_BamName;
}


/////////////////////////////////////////////////////////////////////////////
// CBAMDataLoader_Impl
/////////////////////////////////////////////////////////////////////////////


CBAMDataLoader_Impl::CBAMDataLoader_Impl(
    const CBAMDataLoader::SLoaderParams& params)
    : m_IdMapper(params.m_IdMapper)
{
    CSrzPath srz_path;
    m_DirPath = srz_path.FindAccPathNoThrow(params.m_DirPath);
    if ( m_DirPath.empty() ) {
        m_DirPath = params.m_DirPath;
    }
    
    if ( !m_DirPath.empty() && *m_DirPath.rbegin() != '/' ) {
        m_DirPath += '/';
    }
    if ( params.m_BamFiles.empty() ||
         (params.m_BamFiles.size() == 1 &&
          params.m_BamFiles[0].m_BamName == SRZ_CONFIG_NAME) ) {
        AddSrzDef();
    }
    else {
        ITERATE (vector<CBAMDataLoader::SBamFileName>, it, params.m_BamFiles) {
            AddBamFile(*it);
        }
    }
}


CBAMDataLoader_Impl::~CBAMDataLoader_Impl(void)
{
}


void CBAMDataLoader_Impl::AddSrzDef(void)
{
    AutoPtr<CIdMapper> mapper(new CIdMapper);

    string def_name = m_DirPath+SRZ_CONFIG_NAME;
    CNcbiIfstream in(def_name.c_str());
    if ( !in ) {
        NCBI_THROW(CLoaderException, eNoData,
                   "CBAMDataLoader: no def file: "+def_name);
    }
    string line;
    vector<string> tokens;
    while ( getline(in, line) ) {
        tokens.clear();
        NStr::Tokenize(line, "\t", tokens);
        if ( tokens.size() < 4 ) {
            NCBI_THROW(CLoaderException, eNoData,
                       "CBAMDataLoader: bad def line: \""+line+"\"");
        }
        SDirSeqInfo info;
        info.m_BamSeqLabel = tokens[0];
        info.m_Label = tokens[1];
        if ( tokens[2].empty() ) {
            info.m_SeqId = CSeq_id_Handle::GetHandle(info.m_Label);
        }
        else {
            info.m_SeqId = CSeq_id_Handle::GetHandle(tokens[2]);
        }
        info.m_BamFileName = tokens[3];
        if ( tokens.size() >= 4 ) {
            info.m_CovFileName = tokens[4];
        }
        m_SeqInfos.push_back(info);
        CRef<CSeq_id> src_id;
        try {
            src_id = new CSeq_id(info.m_BamSeqLabel);
        }
        catch ( CException& /*ignored*/ ) {
            src_id = new CSeq_id(CSeq_id::e_Local, info.m_BamSeqLabel);
        }
        mapper->AddMapping(CSeq_id_Handle::GetHandle(*src_id), info.m_SeqId);
    }
    m_IdMapper.reset(mapper.release());

    ITERATE ( TSeqInfos, it, m_SeqInfos ) {
        const SDirSeqInfo& info = *it;
        
        CBAMDataLoader::SBamFileName bam(info.m_BamFileName);
        CRef<CBamFileInfo>& bam_info = m_BamFiles[info.m_BamFileName];
        if ( !bam_info ) {
            bam_info = new CBamFileInfo(*this, bam,
                                        info.m_BamSeqLabel, info.m_SeqId);
        }
        else {
            bam_info->AddRefSeq(info.m_BamSeqLabel, info.m_SeqId);
        }
        CBamRefSeqInfo* seq_info = bam_info->GetRefSeqInfo(info.m_SeqId);
        if ( !info.m_CovFileName.empty() ) {
            string file_name = m_DirPath + info.m_CovFileName;
            if ( !CFile(file_name).Exists() ) {
                ERR_POST_X(2, "CBAMDataLoader: "
                           "no cov file: \""+file_name+"\"");
            }
            else {
                seq_info->SetCovFileName(file_name);
            }
        }
    }
}


void CBAMDataLoader_Impl::AddBamFile(const CBAMDataLoader::SBamFileName& bam)
{
    m_BamFiles[bam.m_BamName] = new CBamFileInfo(*this, bam);
}


CBamRefSeqInfo* CBAMDataLoader_Impl::GetRefSeqInfo(const CBAMBlobId& blob_id)
{
    TBamFiles::iterator bit = m_BamFiles.find(blob_id.m_BamName);
    if ( bit == m_BamFiles.end() ) {
        return 0;
    }
    CBamRefSeqInfo* info = bit->second->GetRefSeqInfo(blob_id.m_SeqId);
    return info;
}


void CBAMDataLoader_Impl::LoadBAMEntry(const CBAMBlobId& blob_id,
                                       CTSE_LoadLock& load_lock)
{
    GetRefSeqInfo(blob_id)->LoadMainSplit(load_lock);
}


void CBAMDataLoader_Impl::LoadChunk(const CBAMBlobId& blob_id,
                                    CTSE_Chunk_Info& chunk_info)
{
    GetRefSeqInfo(blob_id)->LoadChunk(chunk_info);
}


CRef<CBAMBlobId>
CBAMDataLoader_Impl::GetShortSeqBlobId(const CSeq_id_Handle& idh)
{
    CRef<CBAMBlobId> ret;
    ITERATE ( TBamFiles, it, m_BamFiles ) {
        it->second->GetShortSeqBlobId(ret, idh);
    }
    return ret;
}


CRef<CBAMBlobId>
CBAMDataLoader_Impl::GetRefSeqBlobId(const CSeq_id_Handle& idh)
{
    CRef<CBAMBlobId> ret;
    ITERATE ( TBamFiles, it, m_BamFiles ) {
        it->second->GetRefSeqBlobId(ret, idh);
    }
    return ret;
}


CBAMDataLoader::TAnnotNames CBAMDataLoader_Impl::GetPossibleAnnotNames(void) const
{
    CBAMDataLoader::TAnnotNames names;
    ITERATE ( TBamFiles, it, m_BamFiles ) {
        const string& name = it->second->GetAnnotName();
        if ( name.empty() ) {
            names.push_back(CAnnotName());
        }
        else {
            names.push_back(CAnnotName(name));
        }
    }
    sort(names.begin(), names.end());
    names.erase(unique(names.begin(), names.end()), names.end());
    return names;
}


void CBAMDataLoader_Impl::GetIds(const CSeq_id_Handle& idh, TIds& ids)
{
    if ( IsShortSeq(idh) ) {
        ids.push_back(idh);
    }
}


CDataSource::SAccVerFound
CBAMDataLoader_Impl::GetAccVer(const CSeq_id_Handle& idh)
{
    CDataSource::SAccVerFound ret;
    if ( !idh.IsGi() && idh.GetSeqId()->GetTextseq_Id() && IsShortSeq(idh) ) {
        ret.sequence_found = true;
        ret.acc_ver = idh;
    }
    return ret;
}


CDataSource::SGiFound
CBAMDataLoader_Impl::GetGi(const CSeq_id_Handle& idh)
{
    CDataSource::SGiFound ret;
    if ( idh.IsGi() && IsShortSeq(idh) ) {
        ret.sequence_found = true;
        ret.gi = idh.GetGi();
    }
    return ret;
}


string CBAMDataLoader_Impl::GetLabel(const CSeq_id_Handle& idh)
{
    if ( IsShortSeq(idh) ) {
        return objects::GetLabel(idh);
    }
    return string();
}


int CBAMDataLoader_Impl::GetTaxId(const CSeq_id_Handle& idh)
{
    if ( IsShortSeq(idh) ) {
        return 0;
    }
    return -1;
}


bool CBAMDataLoader_Impl::IsShortSeq(const CSeq_id_Handle& idh)
{
    return GetShortSeqBlobId(idh) != null;
}


/////////////////////////////////////////////////////////////////////////////
// CBamFileInfo
/////////////////////////////////////////////////////////////////////////////


CBamFileInfo::CBamFileInfo(const CBAMDataLoader_Impl& impl,
                           const CBAMDataLoader::SBamFileName& bam)
{
    CMutexGuard guard(GetMutex());
    x_Initialize(impl, bam);
    for ( CBamRefSeqIterator rit(m_BamDb); rit; ++rit ) {
        string refseq_label = rit.GetRefSeqId();
        CSeq_id_Handle seq_id = CSeq_id_Handle::GetHandle(*rit.GetRefSeq_id());
        AddRefSeq(refseq_label, seq_id);
    }
}


CBamFileInfo::CBamFileInfo(const CBAMDataLoader_Impl& impl,
                           const CBAMDataLoader::SBamFileName& bam,
                           const string& refseq_label,
                           const CSeq_id_Handle& seq_id)
{
    CMutexGuard guard(GetMutex());
    x_Initialize(impl, bam);
    AddRefSeq(refseq_label, seq_id);
}


void CBamFileInfo::x_Initialize(const CBAMDataLoader_Impl& impl,
                                const CBAMDataLoader::SBamFileName& bam)
{
    m_BamName = bam.m_BamName;
    m_AnnotName = CDirEntry(m_BamName).GetBase();
    m_BamDb = CBamDb(impl.m_Mgr,
                     impl.m_DirPath+bam.m_BamName,
                     impl.m_DirPath+(bam.m_IndexName.empty()?
                                     bam.m_BamName+".bai":
                                     bam.m_IndexName));
    if ( impl.m_IdMapper.get() ) {
        m_BamDb.SetIdMapper(impl.m_IdMapper.get(), eNoOwnership);
    }
}


void CBamFileInfo::AddRefSeq(const string& refseq_label,
                               const CSeq_id_Handle& refseq_id)
{
    if ( GetDebugLevel() >= 1 ) {
        LOG_POST_X(9, Info << "CBAMDataLoader(" << m_BamName << "): "
                   "Found "<<refseq_label<<" -> "<<refseq_id);
    }
    m_RefSeqs[refseq_id] = new CBamRefSeqInfo(this, refseq_label, refseq_id);
}


void CBamFileInfo::GetShortSeqBlobId(CRef<CBAMBlobId>& ret,
                                     const CSeq_id_Handle& idh) const
{
    ITERATE ( TRefSeqs, it, m_RefSeqs ) {
        it->second->GetShortSeqBlobId(ret, idh);
    }
}


void CBamFileInfo::GetRefSeqBlobId(CRef<CBAMBlobId>& ret,
                                   const CSeq_id_Handle& idh) const
{
    const CBamRefSeqInfo* info = GetRefSeqInfo(idh);
    if ( info ) {
        info->SetBlobId(ret, idh);
    }
}


CBamRefSeqInfo* CBamFileInfo::GetRefSeqInfo(const CSeq_id_Handle& seq_id) const
{
    TRefSeqs::const_iterator it = m_RefSeqs.find(seq_id);
    if ( it == m_RefSeqs.end() ) {
        return 0;
    }
    return it->second.GetNCPointer();
}


/////////////////////////////////////////////////////////////////////////////
// CBamRefSeqInfo
/////////////////////////////////////////////////////////////////////////////


class CDefaultSpotIdDetector : public CObject,
                               public CBamAlignIterator::ISpotIdDetector
{
public:
    void AddSpotId(string& short_id, const CBamAlignIterator* iter) {
        string seq = iter->GetShortSequence();
        if ( iter->IsSetStrand() && IsReverse(iter->GetStrand()) ) {
            CSeqManip::ReverseComplement(seq, CSeqUtil::e_Iupacna, 0, TSeqPos(seq.size()));
        }
        SShortSeqInfo& info = m_ShortSeqs[short_id];
        if ( info.spot1.empty() ) {
            info.spot1 = seq;
            short_id += ".1";
            return;
        }
        if ( info.spot1 == seq ) {
            short_id += ".1";
            return;
        }
        if ( info.spot2.empty() ) {
            info.spot2 = seq;
            short_id += ".2";
            return;
        }
        if ( info.spot2 == seq ) {
            short_id += ".2";
            return;
        }
        short_id += ".?";
    }

private:
    struct SShortSeqInfo {
        string spot1, spot2;
    };
    map<string, SShortSeqInfo> m_ShortSeqs;
};

CBamRefSeqInfo::CBamRefSeqInfo(const CBamFileInfo* bam_file,
                               const string& refseqid,
                               const CSeq_id_Handle& seq_id)
    : m_File(bam_file),
      m_RefSeqId(refseqid),
      m_RefSeq_id(seq_id),
      m_MinMapQuality(1),
      m_LoadedRanges(false)
{
    m_SpotIdDetector = new CDefaultSpotIdDetector();
}


void CBamRefSeqInfo::SetBlobId(CRef<CBAMBlobId>& ret,
                               const CSeq_id_Handle& idh) const
{
    CRef<CBAMBlobId> id(new CBAMBlobId(m_File->GetBamName(), GetRefSeq_id()));
    if ( ret ) {
        ERR_POST_X(1, "CBAMDataLoader::GetBlobId: "
                   "Seq-id "<<idh<<" appears in two files: "
                   <<ret->ToString()<<" & "<<id->ToString());
    }
    else {
        ret = id;
    }
}


void CBamRefSeqInfo::GetShortSeqBlobId(CRef<CBAMBlobId>& ret,
                                       const CSeq_id_Handle& idh) const
{
    bool exists;
    {{
        CMutexGuard guard(m_File->GetMutex());
        exists = m_Seq2Chunk.find(idh) != m_Seq2Chunk.end();
    }}
    if ( exists ) {
        SetBlobId(ret, idh);
    }
}


namespace {
    struct SRefStat {
        SRefStat(void)
            : m_RefPosQuery(0),
              m_Count(0),
              m_RefPosFirst(0),
              m_RefPosLast(0),
              m_RefPosMax(0),
              m_RefLenMax(0)
            {
            }

        TSeqPos m_RefPosQuery;
        unsigned m_Count;
        TSeqPos m_RefPosFirst;
        TSeqPos m_RefPosLast;
        TSeqPos m_RefPosMax;
        TSeqPos m_RefLenMax;

        void Collect(const CBamDb& bam_db, const string& ref_id,
                     TSeqPos ref_pos, unsigned count, int min_quality);

        unsigned GetStatCount(void) const {
            return m_Count;
        }
        double GetStatLen(void) const {
            return m_RefPosLast - m_RefPosFirst + .5;
        }
    };
    
    
    void SRefStat::Collect(const CBamDb& bam_db, const string& ref_id,
                           TSeqPos ref_pos, unsigned count, int min_quality)
    {
        m_RefPosQuery = ref_pos;
        size_t skipped = 0;
        TSeqPos ref_len = bam_db.GetRefSeqLength(ref_id);
        CBamAlignIterator ait(bam_db, ref_id, ref_pos);
        for ( ; ait; ++ait ) {
            TSeqPos pos = ait.GetRefSeqPos();
            if ( pos < ref_pos ) {
                // the alignment starts before current range
                continue;
            }
            if ( min_quality > 0 && ait.GetMapQuality() < min_quality ) {
                ++skipped;
                continue;
            }
            TSeqPos len = ait.GetCIGARRefSize();
            TSeqPos max = pos + len;
            if ( max > ref_len ) {
                ++skipped;
                continue;
            }
            m_RefPosLast = pos;
            if ( len > m_RefLenMax ) {
                m_RefLenMax = len;
            }
            if ( max > m_RefPosMax ) {
                m_RefPosMax = max;
            }
            if ( m_Count == 0 ) {
                m_RefPosFirst = pos;
            }
            if ( ++m_Count == count ) {
                break;
            }
        }
        if ( GetDebugLevel() >= 2 ) {
            LOG_POST_X(4, Info << "CBAMDataLoader: "
                       "Stat @ "<<m_RefPosQuery<<": "<<m_Count<<" entries: "<<
                       m_RefPosFirst<<"-"<<m_RefPosLast<<
                       "(+"<<m_RefPosMax-m_RefPosLast<<")"<<
                       " max len: "<<m_RefLenMax<<
                       " skipped: "<<skipped);
        }
    }
};


void CBamRefSeqInfo::LoadRanges(void)
{
    if ( m_LoadedRanges ) {
        return;
    }
    CMutexGuard guard(m_File->GetMutex());
    if ( m_LoadedRanges ) {
        return;
    }

    _TRACE("Loading "<<GetRefSeqId()<<" -> "<<GetRefSeq_id());
    if ( !x_LoadRangesCov() ) {
        if ( 1 ) {
            x_LoadRangesStat();
        }
        else {
            x_LoadRangesScan();
        }
    }
    _TRACE("Loaded ranges on "<<GetRefSeqId());
    m_LoadedRanges = true;
}


static const CUser_field& GetIdField(const CUser_field& field, int id)
{
    ITERATE ( CUser_field::TData::TFields, it, field.GetData().GetFields() ) {
        const CUser_field& field = **it;
        if ( field.IsSetLabel() &&
             field.GetLabel().IsId() &&
             field.GetLabel().GetId() == id ) {
            return field;
        }
    }
    NCBI_THROW_FMT(CLoaderException, eOtherError, "CBAMDataLoader: "
                   "outlier value not found: "<<id);
}


bool CBamRefSeqInfo::x_LoadRangesCov(void)
{
    if ( m_CovFileName.empty() ) {
        return false;
    }
    try {
        CRef<CSeq_entry> entry(new CSeq_entry);
        CRef<CSeq_annot> annot;
        CRef<CSeq_graph> graph;
        AutoPtr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnBinary,
                                                        m_CovFileName));
        *in >> *entry;
        const CBioseq::TAnnot& alist = entry->GetSeq().GetAnnot();
        if ( alist.size() != 1 ) {
            NCBI_THROW_FMT(CLoaderException, eOtherError, "CBAMDataLoader: "
                           "wrong number of annots in cov entry: "<<
                           alist.size());
        }
        annot = alist.front();
        const CSeq_annot::TData::TGraph& glist = annot->GetData().GetGraph();
        if ( glist.size() != 1 ) {
            NCBI_THROW_FMT(CLoaderException, eOtherError, "CBAMDataLoader: "
                           "wrong number of graphs in cov entry: "<<
                           glist.size());
        }            
        graph = glist.front();
        
        CConstRef<CUser_object> params;
        ITERATE ( CAnnot_descr::Tdata, it, annot->GetDesc().Get() ) {
            if ( (*it)->IsUser() &&
                 (*it)->GetUser().GetType().GetStr() == "BAM coverage" ) {
                params = &(*it)->GetUser();
                break;
            }
        }

        TSeqPos slot = graph->GetComp();
        TSeqPos cnt = graph->GetNumval();
        CConstRef<CUser_field> outliers = params->GetFieldRef("Outliers");
        double vmul = graph->GetA();
        double vadd = graph->GetB();

        size_t non_zero_count = 0;
        vector<double> cov(cnt);
        if ( graph->GetGraph().IsByte() ) {
            const CByte_graph& g = graph->GetGraph().GetByte();
            const CByte_graph::TValues& vv = g.GetValues();
            int vmin = g.GetMin();
            int vmax = g.GetMax();
            for ( TSeqPos i = 0; i < cnt; ++i ) {
                double v;
                int vg = Uint1(vv[i]);
                if ( vg < vmin ) {
                    continue;
                }
                if ( vg > vmax ) {
                    v = GetIdField(*outliers, i).GetData().GetReal();
                }
                else {
                    v = vmul*vg+vadd;
                }
                cov[i] = v;
                ++non_zero_count;
            }
        }
        else {
            const CInt_graph& g = graph->GetGraph().GetInt();
            const CInt_graph::TValues& vv = g.GetValues();
            int vmin = g.GetMin();
            int vmax = g.GetMax();
            for ( TSeqPos i = 0; i < cnt; ++i ) {
                double v;
                int vg = vv[i];
                if ( vg < vmin ) {
                    continue;
                }
                if ( vg > vmax ) {
                    v = GetIdField(*outliers, i).GetData().GetReal();
                }
                else {
                    v = vmul*vg+vadd;
                }
                cov[i] = v;
                ++non_zero_count;
            }
        }

        m_MinMapQuality = params->GetField("MinMapQuality").GetData().GetInt();
        int align_cnt = params->GetField("AlignCount").GetData().GetInt();
        double avg_cov = params->GetField("AvgCoverage").GetData().GetReal();
        double total_cov = avg_cov*non_zero_count*slot;
        double avg_align_len = total_cov/align_cnt;
        int max_align_len;
        CConstRef<CUser_field> len_field = params->GetFieldRef("MaxAlignSpan");
        if ( len_field ) {
            max_align_len = len_field->GetData().GetInt();
        }
        else {
            max_align_len = int(avg_align_len*2+50);
        }
        double cov_to_align_cnt = (align_cnt*slot)/total_cov;

        TSeqPos cur_first = kInvalidSeqPos, cur_last = kInvalidSeqPos;
        double cur_cnt = 0;
        
        for ( TSeqPos i = 0; i <= cnt; ++i ) {
            bool empty = i==cnt || !cov[i];
            double next_cnt = i==cnt? 0: cov[i] * cov_to_align_cnt;
            if ( cur_first != kInvalidSeqPos &&
                 (i == cnt ||
                  cur_cnt >= kChunkSize*1.5 ||
                  (next_cnt >= kChunkSize*2 && cur_cnt >= kChunkSize*.7) ||
                  (empty && cur_cnt >= kChunkSize)) ) {
                // flush collected slots
                if ( GetDebugLevel() >= 2 ) {
                    LOG_POST_X(8, Info << "CBAMDataLoader:"
                               " Chunk "<<m_Chunks.size()<<
                               " Slots "<<cur_first<<"-"<<cur_last<<
                               " exp: "<<cur_cnt);
                }
                CBamRefSeqChunkInfo chunk;
                chunk.m_AlignCount = int(cur_cnt+1);
                chunk.m_RefSeqRange.SetFrom(cur_first*slot);
                TSeqPos end = cur_last*slot+slot;
                chunk.m_RefSeqRange.SetToOpen(end+max_align_len);
                chunk.m_MaxRefSeqFrom = end-1;
                m_Chunks.push_back(chunk);
                cur_first = kInvalidSeqPos;
                cur_last = kInvalidSeqPos;
                cur_cnt = 0;
            }
            if ( empty ) continue;
            if ( cur_first == kInvalidSeqPos ) {
                cur_first = i;
            }
            cur_last = i;
            cur_cnt += max(next_cnt, 1e-9);
        }
        
        m_CovEntry = entry;
        return true;
    }
    catch ( CException& exc ) {
        ERR_POST_X(3, "CBAMDataLoader: "
                   "failed to load cov file: "<<m_CovFileName<<": "<<exc);
        return false;
    }
}


void CBamRefSeqInfo::x_LoadRangesStat(void)
{
    const unsigned kNumStat = 10;
    const unsigned kStatCount = 1000;
    vector<SRefStat> stat(kNumStat);
    TSeqPos ref_begin = 0, ref_end_min = 0, ref_end = 0, max_len = 0;
    double stat_len = 0, stat_cnt = 0;
    const unsigned scan_first = 1;
    if ( scan_first ) {
        stat[0].Collect(*m_File, GetRefSeqId(), 0,
                        kStatCount, m_MinMapQuality);
        if ( stat[0].m_Count != kStatCount ) {
            // single chunk
            if ( stat[0].m_Count > 0 ) {
                CBamRefSeqChunkInfo chunk;
                chunk.m_AlignCount = stat[0].m_Count;
                chunk.m_RefSeqRange.SetFrom(stat[0].m_RefPosFirst);
                chunk.m_RefSeqRange.SetToOpen(stat[0].m_RefPosMax);
                chunk.m_MaxRefSeqFrom = stat[0].m_RefPosLast;
                m_Chunks.push_back(chunk);
            }
            m_LoadedRanges = true;
            return;
        }
        ref_begin = stat[0].m_RefPosFirst;
        ref_end_min = stat[0].m_RefPosLast;
        max_len = stat[0].m_RefLenMax;
        stat_len = stat[0].GetStatLen();
        stat_cnt = stat[0].GetStatCount();
    }
    ref_end = m_File->GetRefSeqLength(GetRefSeqId());
    if ( ref_end == kInvalidSeqPos ) {
        TSeqPos min = ref_end_min;
        TSeqPos max = kInvalidSeqPos;
        while ( max > min+max_len+1 ) {
            TSeqPos mid = min + (max - min)/2;
            _TRACE("binary: "<<min<<"-"<<max<<" -> "<<mid);
            if ( CBamAlignIterator(*m_File, GetRefSeqId(), mid) ) {
                min = mid;
            }
            else {
                max = mid;
            }
        }
        ref_end = max;
        _TRACE("binary: end: "<<max);
    }
    for ( unsigned k = scan_first; k < kNumStat; ++k ) {
        TSeqPos ref_pos = ref_begin +
            TSeqPos(double(ref_end - ref_begin)*k/kNumStat);
        if ( k && ref_pos < stat[k-1].m_RefPosLast ) {
            ref_pos = stat[k-1].m_RefPosLast;
        }
        _TRACE("stat["<<k<<"] @ "<<ref_pos);
        stat[k].Collect(*m_File, GetRefSeqId(), ref_pos,
                        kStatCount, m_MinMapQuality);
        stat_len += stat[k].GetStatLen();
        stat_cnt += stat[k].GetStatCount();
        if ( stat[k].m_RefLenMax > max_len ) {
            max_len = stat[k].m_RefLenMax;
        }
    }
    double density = stat_cnt / stat_len;
    double exp_count = (ref_end-ref_begin)*density;
    unsigned chunks = unsigned(exp_count/kChunkSize+1);
    chunks = min(chunks, unsigned(sqrt(exp_count)+1));
    max_len *= 2;
    if ( GetDebugLevel() >= 1 ) {
        LOG_POST_X(5, Info << "CBAMDataLoader: "
                   "Total range: "<<ref_begin<<"-"<<ref_end-1<<
                   " exp count: "<<exp_count<<" chunks: "<<chunks);
    }
    vector<TSeqPos> pp(chunks+1);
    for ( unsigned k = 1; k < chunks; ++k ) {
        TSeqPos pos = ref_begin +
            TSeqPos(double(ref_end-ref_begin)*k/chunks);
        pp[k] = pos;
    }
    pp[chunks] = ref_end;
    for ( unsigned k = 0; k < chunks; ++k ) {
        CBamRefSeqChunkInfo chunk;
        chunk.m_AlignCount = 1;
        TSeqPos pos = pp[k];
        TSeqPos end = pp[k+1];
        chunk.m_RefSeqRange.SetFrom(pos);
        TSeqPos end2 = min(end+max_len, ref_end);
        if ( k+1 < chunks ) {
            end2 = min(end2, pp[k+2]);
        }
        chunk.m_RefSeqRange.SetToOpen(end2);
        chunk.m_MaxRefSeqFrom = end-1;
        m_Chunks.push_back(chunk);
    }
}


void CBamRefSeqInfo::x_LoadRangesScan(void)
{
    typedef CBamRefSeqInfo::TRange TRange;
    vector<TRange> rr;
    int min_quality = m_MinMapQuality;
    TSeqPos ref_len = m_File->GetRefSeqLength(GetRefSeqId());
    for ( CBamAlignIterator ait(*m_File, GetRefSeqId(), 0); ait; ++ait ) {
        if ( min_quality > 0 && ait.GetMapQuality() < min_quality ) {
            continue;
        }
        TSeqPos ref_pos = ait.GetRefSeqPos();
        TSeqPos ref_end = ref_pos + ait.GetCIGARRefSize() - 1;
        if ( ref_end > ref_len ) {
            continue;
        }
        rr.push_back(TRange(ref_pos, ref_end));
    }
    if ( !rr.empty() ) {
        sort(rr.begin(), rr.end());
        for ( size_t p = 0; p < rr.size(); ) {
            size_t e = min(p+kChunkSize, rr.size())-1;
            TSeqPos min_from = rr[p].GetFrom();
            TSeqPos max_from = rr[e++].GetFrom();
            while ( e < rr.size() && rr[e].GetFrom() == max_from ) {
                ++e;
            }
            TSeqPos max_to_open = max_from;
            for ( size_t i = p; i < e; ++i ) {
                max_to_open = max(max_to_open, rr[i].GetToOpen());
            }
            CBamRefSeqChunkInfo chunk;
            chunk.AddRefSeqRange(TRange(min_from, max_to_open-1));
            chunk.AddRefSeqRange(TRange(max_from, max_to_open-1));
            _TRACE("Chunk "<<m_Chunks.size()<<" count: "<<e-p<<
                   " "<<TRange(min_from, max_from)<<" "<<max_to_open-1);
            m_Chunks.push_back(chunk);
            p = e;
        }
        if ( GetDebugLevel() >= 1 ) {
            LOG_POST_X(6, Info<<"CBAMDataLoader: "
                       "Total range: "<<
                       rr[0].GetFrom()<<"-"<<rr.back().GetTo()<<
                       " count: "<<rr.size()<<" chunks: "<<m_Chunks.size());
        }
    }
}


void CBamRefSeqInfo::LoadMainEntry(CTSE_LoadLock& load_lock)
{
    CMutexGuard guard(m_File->GetMutex());
    LoadRanges();
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSet().SetId().SetId(kTSEId);
    if ( m_CovEntry ) {
        entry->SetSet().SetAnnot() = m_CovEntry->GetAnnot();
    }
    load_lock->SetSeq_entry(*entry);
    CTSE_Split_Info& split_info = load_lock->GetSplitInfo();
    int chunk_count = int(m_Chunks.size());
    bool has_pileup = GetPileupGraphsParam();
    CAnnotName name, pileup_name;
    if ( !m_File->GetAnnotName().empty() ) {
        string base = m_File->GetAnnotName();
        name = CAnnotName(base);
        if ( has_pileup ) {
            pileup_name = CAnnotName(base + ' ' + PILEUP_NAME_SUFFIX);
        }
    }

    // create chunk info for alignments
    for ( int range_id = 0; range_id < chunk_count; ++range_id ) {
        CRef<CTSE_Chunk_Info> chunk;
        int base_id = range_id*kChunkIdMul;
        chunk = new CTSE_Chunk_Info(base_id+eChunk_align);
        chunk->x_AddAnnotType(name,
                              SAnnotTypeSelector(CSeq_annot::C_Data::e_Align),
                              GetRefSeq_id(),
                              m_Chunks[range_id].GetRefSeqRange());
        split_info.AddChunk(*chunk);

        if ( has_pileup ) {
            CRange<TSeqPos> range = m_Chunks[range_id].GetRefSeqRange();
            if ( range_id+1 < chunk_count ) {
                range.SetToOpen(m_Chunks[range_id+1].GetRefSeqRange().GetFrom());
            }
            chunk = new CTSE_Chunk_Info(base_id+eChunk_pileup_graph);
            chunk->x_AddAnnotType(pileup_name,
                                  CSeq_annot::C_Data::e_Graph,
                                  GetRefSeq_id(),
                                  range);
            split_info.AddChunk(*chunk);
        }
    }
}


void CBamRefSeqInfo::LoadMainSplit(CTSE_LoadLock& load_lock)
{
    CMutexGuard guard(m_File->GetMutex());
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSet().SetId().SetId(kTSEId);
    load_lock->SetSeq_entry(*entry);
    CTSE_Split_Info& split_info = load_lock->GetSplitInfo();
    bool has_pileup = GetPileupGraphsParam();
    CAnnotName name, pileup_name;
    if ( !m_File->GetAnnotName().empty() ) {
        string base = m_File->GetAnnotName();
        name = CAnnotName(base);
        if ( has_pileup ) {
            pileup_name = CAnnotName(base + ' ' + PILEUP_NAME_SUFFIX);
        }
    }

    CRef<CTSE_Chunk_Info> chunk(new CTSE_Chunk_Info(kMainChunkId));
    CRange<TSeqPos> whole_range = CRange<TSeqPos>::GetWhole();
    if ( m_CovEntry || !m_CovFileName.empty() ) {
        chunk->x_AddAnnotType(name,
                              SAnnotTypeSelector(CSeq_annot::C_Data::e_Graph),
                              GetRefSeq_id(),
                              whole_range);
    }
    if ( has_pileup ) {
        chunk->x_AddAnnotType(pileup_name,
                              SAnnotTypeSelector(CSeq_annot::C_Data::e_Graph),
                              GetRefSeq_id(),
                              whole_range);
    }
    chunk->x_AddAnnotType(name,
                          SAnnotTypeSelector(CSeq_annot::C_Data::e_Align),
                          GetRefSeq_id(),
                          whole_range);
    split_info.AddChunk(*chunk);
}


void CBamRefSeqInfo::LoadMainChunk(CTSE_Chunk_Info& chunk_info)
{
    CMutexGuard guard(m_File->GetMutex());
    LoadRanges();
    CTSE_Split_Info& split_info =
        const_cast<CTSE_Split_Info&>(chunk_info.GetSplitInfo());
    int chunk_count = int(m_Chunks.size());
    bool has_pileup = GetPileupGraphsParam();
    CAnnotName name, pileup_name;
    if ( !m_File->GetAnnotName().empty() ) {
        string base = m_File->GetAnnotName();
        name = CAnnotName(base);
        if ( has_pileup ) {
            pileup_name = CAnnotName(base + ' ' + PILEUP_NAME_SUFFIX);
        }
    }
    CTSE_Chunk_Info::TPlace place(CSeq_id_Handle(), kTSEId);

    if ( m_CovEntry ) {
        ITERATE ( CBioseq::TAnnot, it, m_CovEntry->GetAnnot() ) {
            chunk_info.x_LoadAnnot(place, **it);
        }
    }

    // create chunk info for alignments
    for ( int range_id = 0; range_id < chunk_count; ++range_id ) {
        CRef<CTSE_Chunk_Info> chunk;
        int base_id = range_id*kChunkIdMul;
        chunk = new CTSE_Chunk_Info(base_id+eChunk_align);
        chunk->x_AddAnnotType(name,
                              SAnnotTypeSelector(CSeq_annot::C_Data::e_Align),
                              GetRefSeq_id(),
                              m_Chunks[range_id].GetRefSeqRange());
        split_info.AddChunk(*chunk);

        if ( has_pileup ) {
            CRange<TSeqPos> range = m_Chunks[range_id].GetRefSeqRange();
            if ( range_id+1 < chunk_count ) {
                range.SetToOpen(m_Chunks[range_id+1].GetRefSeqRange().GetFrom());
            }
            chunk = new CTSE_Chunk_Info(base_id+eChunk_pileup_graph);
            chunk->x_AddAnnotType(pileup_name,
                                  CSeq_annot::C_Data::e_Graph,
                                  GetRefSeq_id(),
                                  range);
            split_info.AddChunk(*chunk);
        }
    }
    chunk_info.SetLoaded();
}


void CBamRefSeqInfo::LoadChunk(CTSE_Chunk_Info& chunk_info)
{
    if ( chunk_info.GetChunkId() == kMainChunkId ) {
        LoadMainChunk(chunk_info);
        return;
    }
    switch ( chunk_info.GetChunkId() % kChunkIdMul ) {
    case eChunk_align:
        LoadAlignChunk(chunk_info);
        break;
    case eChunk_short_seq:
        LoadSeqChunk(chunk_info);
        break;
    case eChunk_pileup_graph:
        LoadPileupChunk(chunk_info);
        break;
    }
}


void CBamRefSeqInfo::LoadAlignChunk(CTSE_Chunk_Info& chunk_info)
{
    CMutexGuard guard(m_File->GetMutex());
    CTSE_Split_Info& split_info =
        const_cast<CTSE_Split_Info&>(chunk_info.GetSplitInfo());
    int range_id = chunk_info.GetChunkId()/kChunkIdMul;
    const CBamRefSeqChunkInfo& chunk = m_Chunks[range_id];
    TSeqPos pos = chunk.GetRefSeqRange().GetFrom();
    TSeqPos len = chunk.GetMaxRefSeqFrom() - pos + 1;
    CTSE_Chunk_Info::TPlace place(CSeq_id_Handle(), kTSEId);
    int min_quality = m_MinMapQuality;
    _TRACE("Loading aligns "<<GetRefSeqId()<<" @ "<<chunk.GetRefSeqRange());
    size_t skipped = 0, count = 0, repl_count = 0, fail_count = 0;
    vector<CSeq_id_Handle> short_ids;
    
    CRef<CSeq_annot> annot;
    CSeq_annot::TData::TAlign* align_list = 0;
    
    TSeqPos ref_len = m_File->GetRefSeqLength(GetRefSeqId());
    CBamAlignIterator ait(*m_File, GetRefSeqId(), pos, len);
    if ( m_SpotIdDetector ) {
        ait.SetSpotIdDetector(m_SpotIdDetector.GetNCPointer());
    }
    for( ; ait; ++ait ) {
        TSeqPos align_pos = ait.GetRefSeqPos();
        if ( align_pos < pos ) {
            // the alignments starts before current chunk range
            ++skipped;
            continue;
        }
        if ( min_quality > 0 && ait.GetMapQuality() < min_quality ) {
            ++skipped;
            continue;
        }
        TSeqPos align_end = align_pos + ait.GetCIGARRefSize();
        if ( align_end > ref_len ) {
            ++skipped;
            continue;
        }
        ++count;

        if ( !align_list ) {
            annot = ait.GetSeq_annot(m_File->GetAnnotName());
            align_list = &annot->SetData().SetAlign();
        }
        align_list->push_back(ait.GetMatchAlign());
        short_ids.push_back(CSeq_id_Handle::GetHandle(*ait.GetShortSeq_id()));
    }
    if ( annot ) {
        chunk_info.x_LoadAnnot(place, *annot);
    }
    if ( GetDebugLevel() >= 2 ) {
        LOG_POST_X(7, Info<<"CBAMDataLoader: "
                   "Loaded "<<GetRefSeqId()<<" @ "<<
                   chunk.GetRefSeqRange()<<": "<<
                   count<<" repl: "<<repl_count<<" fail: "<<fail_count<<
                   " skipped: "<<skipped);
    }
    int seq_chunk_id = range_id*kChunkIdMul+eChunk_short_seq;
    CRef<CTSE_Chunk_Info> seq_chunk;
    ITERATE ( vector<CSeq_id_Handle>, it, short_ids ) {
        if ( !m_Seq2Chunk.insert(TSeq2Chunk::value_type(*it, seq_chunk_id)).second ) {
            continue;
        }
        if ( !seq_chunk ) {
            seq_chunk = new CTSE_Chunk_Info(seq_chunk_id);
        }
        seq_chunk->x_AddBioseqId(*it);
    }
    if ( seq_chunk ) {
        seq_chunk->x_AddBioseqPlace(kTSEId);
        split_info.AddChunk(*seq_chunk);
    }
    chunk_info.SetLoaded();
}


void CBamRefSeqInfo::LoadSeqChunk(CTSE_Chunk_Info& chunk_info)
{
    CMutexGuard guard(m_File->GetMutex());
    int chunk_id = chunk_info.GetChunkId();
    int range_id = chunk_id/kChunkIdMul;
    const CBamRefSeqChunkInfo& chunk = m_Chunks[range_id];
    TSeqPos pos = chunk.GetRefSeqRange().GetFrom();
    TSeqPos len = chunk.GetMaxRefSeqFrom() - pos + 1;
    CTSE_Chunk_Info::TPlace place(CSeq_id_Handle(), kTSEId);
    int min_quality = m_MinMapQuality;
    _TRACE("Loading seqs "<<GetRefSeqId()<<" @ "<<chunk.GetRefSeqRange());
    size_t count = 0, skipped = 0, dups = 0;
    set<CSeq_id_Handle> loaded;
    
    TSeqPos ref_len = m_File->GetRefSeqLength(GetRefSeqId());
    CBamAlignIterator ait(*m_File, GetRefSeqId(), pos, len);
    if ( m_SpotIdDetector ) {
        ait.SetSpotIdDetector(m_SpotIdDetector.GetNCPointer());
    }
    for( ; ait; ++ait ){
        TSeqPos align_pos = ait.GetRefSeqPos();
        if ( align_pos < pos ) {
            // the alignments starts before current chunk range
            ++skipped;
            continue;
        }
        if ( min_quality > 0 && ait.GetMapQuality() < min_quality ) {
            ++skipped;
            continue;
        }
        TSeqPos align_end = align_pos + ait.GetCIGARRefSize();
        if ( align_end > ref_len ) {
            ++skipped;
            continue;
        }

        CSeq_id_Handle seq_id =
            CSeq_id_Handle::GetHandle(*ait.GetShortSeq_id());
        if ( m_Seq2Chunk[seq_id] != chunk_id ) {
            ++skipped;
            continue;
        }

        if ( !loaded.insert(seq_id).second ) {
            ++dups;
            continue;
        }
        chunk_info.x_LoadBioseq(place, *ait.GetShortBioseq());
        ++count;
    }

    if ( GetDebugLevel() >= 2 ) {
        LOG_POST_X(10, Info<<"CBAMDataLoader: "
                   "Loaded seqs "<<GetRefSeqId()<<" @ "<<
                   chunk.GetRefSeqRange()<<": "<<
                   count<<" skipped: "<<skipped<<" dups: "<<dups);
    }

    chunk_info.SetLoaded();
}


BEGIN_LOCAL_NAMESPACE;

struct SBaseStat
{
    typedef unsigned TCount;
    enum {
        kStat_A = 0,
        kStat_C = 1,
        kStat_G = 2,
        kStat_T = 3,
        kStat_Insert = 4,
        kStat_Gap = kStat_Insert,
        kNumStat = 5
    };
    SBaseStat(void) {
        for ( int i = 0; i < kNumStat; ++i ) {
            cnts[i] = 0;
        }
    }

    TCount total() const {
        TCount ret = 0;
        for ( int i = 0; i < kNumStat; ++i ) {
            ret += cnts[i];
        }
        return ret;
    }


    void add_base(char b) {
        switch ( b ) {
        case 'A': cnts[kStat_A] += 1; break;
        case 'C': cnts[kStat_C] += 1; break;
        case 'G': cnts[kStat_G] += 1; break;
        case 'T': cnts[kStat_T] += 1; break;
        //case 'N': cnts[kStat_Insert] += 1; break;
        //case '=': cnts[kStat_Match] += 1; break;
        }
    }
    void add_match() {
        //cnts[kStat_Match] += 1;
    }
    void add_gap() {
        cnts[kStat_Gap] += 1;
    }
    void add_gaps(TCount count) {
        cnts[kStat_Gap] += count;
    }

    TCount cnts[kNumStat];
};

END_LOCAL_NAMESPACE;


void CBamRefSeqInfo::LoadPileupChunk(CTSE_Chunk_Info& chunk_info)
{
    CMutexGuard guard(m_File->GetMutex());
    size_t range_id = chunk_info.GetChunkId()/kChunkIdMul;
    const CBamRefSeqChunkInfo& chunk = m_Chunks[range_id];
    TSeqPos chunk_pos = chunk.GetRefSeqRange().GetFrom();
    TSeqPos chunk_len;
    if ( range_id+1 < m_Chunks.size() ) {
        chunk_len = m_Chunks[range_id+1].GetRefSeqRange().GetFrom()-chunk_pos;
    }
    else {
        chunk_len = chunk.GetMaxRefSeqFrom() - chunk_pos + 1;
    }
    CTSE_Chunk_Info::TPlace place(CSeq_id_Handle(), kTSEId);
    int min_quality = m_MinMapQuality;
    _TRACE("Loading pileup "<<GetRefSeqId()<<" @ "<<chunk.GetRefSeqRange());
    size_t count = 0, skipped = 0;

    vector<SBaseStat> ss(chunk_len);
    SBaseStat::TCount whole_chunk_gap_count = 0;
    
    TSeqPos ref_len = m_File->GetRefSeqLength(GetRefSeqId());
    CBamAlignIterator ait(*m_File, GetRefSeqId(), chunk_pos, chunk_len);
    if ( m_SpotIdDetector ) {
        ait.SetSpotIdDetector(m_SpotIdDetector.GetNCPointer());
    }
    for( ; ait; ++ait ){
        TSeqPos align_pos = ait.GetRefSeqPos();
        TSeqPos align_end = align_pos + ait.GetCIGARRefSize();
        if ( align_end > ref_len ) {
            ++skipped;
            continue;
        }
        if ( min_quality > 0 && ait.GetMapQuality() < min_quality ) {
            ++skipped;
            continue;
        }
        ++count;

        TSeqPos ss_pos = align_pos - chunk_pos;
        TSeqPos read_pos = ait.GetCIGARPos();
        const CBamString& read = ait.GetShortSequence();
        const CBamString& cigar = ait.GetCIGAR();
        const char* ptr = cigar.data();
        const char* end = ptr + cigar.size();
        while ( ptr != end ) {
            char type = *ptr;
            TSeqPos seglen = 0;
            for ( ; ++ptr != end; ) {
                char c = *ptr;
                if ( c >= '0' && c <= '9' ) {
                    seglen = seglen*10+(c-'0');
                }
                else {
                    break;
                }
            }
            if ( seglen == 0 ) {
                ERR_POST_X(4, "Bad CIGAR length: "<<type<<"0 in "<<cigar);
                break;
            }
            if ( type == '=' ) {
                // match
                ss_pos += seglen;
                read_pos += seglen;
            }
            else if ( type == 'M' || type == 'X' ) {
                // mismatch
                for ( TSeqPos i = 0; i < seglen; ++i ) {
                    if ( ss_pos < chunk_len ) {
                        ss[ss_pos].add_base(read[read_pos]);
                    }
                    ++ss_pos;
                    ++read_pos;
                }
            }
            else if ( type == 'I' || type == 'S' ) {
                if ( type == 'S' ) {
                    // soft clipping already accounted in seqpos
                    continue;
                }
                read_pos += seglen;
            }
            else if ( type == 'D' || type == 'N' ) {
                // gap
                TSeqPos ref_pos = ss_pos + chunk_pos; // temporary
                if ( ref_pos < chunk_pos ) {
                    // skip part of gap before chunk range
                    TSeqPos before = min(chunk_pos - ref_pos, seglen);
                    ss_pos += before;
                    seglen -= before;
                }
                if ( ss_pos == 0 && seglen >= chunk_len ) {
                    ++whole_chunk_gap_count;
                    ss_pos += chunk_len;
                    seglen -= chunk_len;
                }
                else if ( ss_pos < chunk_len ) {
                    TSeqPos inside = min(chunk_len - ss_pos, seglen);
                    for ( TSeqPos i = 0; i < inside; ++i ) {
                        ss[ss_pos].add_gap();
                        ++ss_pos;
                    }
                    seglen -= inside;
                }
                if ( seglen > 0 ) {
                    // align continues beyond chunk end
                    break;
                }
            }
            else if ( type != 'P' ) {
                ERR_POST_X(4, "Bad CIGAR char: "<<type<<" in "<<cigar);
                break;
            }
        }
    }
    if ( whole_chunk_gap_count ) {
        for ( TSeqPos i = 0; i < chunk_len; ++i ) {
            ss[i].add_gaps(whole_chunk_gap_count);
        }
    }

    if ( GetDebugLevel() >= 2 ) {
        LOG_POST_X(10, Info<<"CBAMDataLoader: "
                   "Loaded pileup "<<GetRefSeqId()<<" @ "<<
                   chunk.GetRefSeqRange()<<": "<<
                   count<<" skipped: "<<skipped);
    }

    int c_min[SBaseStat::kNumStat], c_max[SBaseStat::kNumStat];
    for ( int i = 0; i < SBaseStat::kNumStat; ++i ) {
        c_min[i] = kMax_Int;
        c_max[i] = 0;
    }
    for ( size_t j = 0; j < chunk_len; ++j ) {
        const SBaseStat& s = ss[j];
        for ( int i = 0; i < SBaseStat::kNumStat; ++i ) {
            int c = s.cnts[i];
            c_min[i] = min(c_min[i], c);
            c_max[i] = max(c_max[i], c);
        }
    }

    CRef<CSeq_annot> annot(new CSeq_annot);
    {
        string name = m_File->GetAnnotName();
        name += ' ';
        name += PILEUP_NAME_SUFFIX;
        CRef<CAnnotdesc> desc(new CAnnotdesc);
        desc->SetName(name);
        annot->SetDesc().Set().push_back(desc);
    }
    CRef<CSeq_id> ref_id(SerialClone(*GetRefSeq_id().GetSeqId()));
    for ( int i = 0; i < SBaseStat::kNumStat; ++i ) {
        CRef<CSeq_graph> graph(new CSeq_graph);
        static const char* const titles[SBaseStat::kNumStat] = {
            "Number of A bases",
            "Number of C bases",
            "Number of G bases",
            "Number of T bases",
            "Number of inserts"
        };
        graph->SetTitle(titles[i]);
        CSeq_interval& loc = graph->SetLoc().SetInt();
        loc.SetId(*ref_id);
        loc.SetFrom(chunk_pos);
        loc.SetTo(chunk_pos+chunk_len-1);
        graph->SetNumval(chunk_len);

        if ( c_max[i] < 256 ) {
            CByte_graph& data = graph->SetGraph().SetByte();
            CByte_graph::TValues& vv = data.SetValues();
            vv.reserve(chunk_len);
            for ( size_t j = 0; j < chunk_len; ++j ) {
                vv.push_back(ss[j].cnts[i]);
            }
            data.SetMin(c_min[i]);
            data.SetMax(c_max[i]);
            data.SetAxis(0);
        }
        else {
            CInt_graph& data = graph->SetGraph().SetInt();
            CInt_graph::TValues& vv = data.SetValues();
            vv.reserve(chunk_len);
            for ( size_t j = 0; j < chunk_len; ++j ) {
                vv.push_back(ss[j].cnts[i]);
            }
            data.SetMin(c_min[i]);
            data.SetMax(c_max[i]);
            data.SetAxis(0);
        }
        annot->SetData().SetGraph().push_back(graph);
    }
    chunk_info.x_LoadAnnot(place, *annot);

    chunk_info.SetLoaded();
}


/////////////////////////////////////////////////////////////////////////////
// CBamRefSeqChunkInfo
/////////////////////////////////////////////////////////////////////////////


void CBamRefSeqChunkInfo::AddRefSeqRange(const TRange& range)
{
    ++m_AlignCount;
    m_RefSeqRange += range;
    m_MaxRefSeqFrom = max(m_MaxRefSeqFrom, range.GetFrom());
}


END_SCOPE(objects)
END_NCBI_SCOPE
