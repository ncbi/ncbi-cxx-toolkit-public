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
 * File Description: WGS file data loader
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/general/general__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqres/seqres__.hpp>
#include <objects/seqsplit/ID2S_Split_Info.hpp>
#include <objects/seqsplit/ID2S_Chunk.hpp>

//#define USE_ID2_CLIENT
#ifdef USE_ID2_CLIENT
# include <objects/id2/id2__.hpp>
# include <objects/id2/id2_client.hpp>
#endif

#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/tse_loadlock.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/impl/split_parser.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>
#include <util/thread_nonstop.hpp>

#include <sra/error_codes.hpp>
#include <sra/readers/ncbi_traces_path.hpp>
#include <sra/readers/sra/wgsresolver.hpp>
#include <sra/data_loaders/wgs/wgsloader.hpp>
#include <sra/data_loaders/wgs/impl/wgsloader_impl.hpp>

#include <util/sequtil/sequtil_manip.hpp>

#include <algorithm>
#include <cmath>
#include <sra/error_codes.hpp>

BEGIN_NCBI_SCOPE

#define NCBI_USE_ERRCODE_X   WGSLoader
NCBI_DEFINE_ERR_SUBCODE_X(20);

BEGIN_SCOPE(objects)

class CDataLoader;

static const size_t kMaxWGSProteinAccLen = 3+7; // longest WGS protein accession
static const size_t kMinWGSPrefixLetters = 4;
static const size_t kMaxWGSPrefixLetters = 6;
static const size_t kWGSPrefixDigits = 2;

NCBI_PARAM_DECL(int, WGS_LOADER, DEBUG);
NCBI_PARAM_DEF_EX(int, WGS_LOADER, DEBUG, 0,
                  eParam_NoThread, WGS_LOADER_DEBUG);

static int GetDebugLevel(void)
{
    static NCBI_PARAM_TYPE(WGS_LOADER, DEBUG) s_Value;
    return s_Value.Get();
}


NCBI_PARAM_DECL(bool, WGS_LOADER, MASTER_DESCR);
NCBI_PARAM_DEF(bool, WGS_LOADER, MASTER_DESCR, true);

static bool GetMasterDescrParam(void)
{
    return NCBI_PARAM_TYPE(WGS_LOADER, MASTER_DESCR)::GetDefault();
}


NCBI_PARAM_DECL(size_t, WGS_LOADER, GC_SIZE);
NCBI_PARAM_DEF(size_t, WGS_LOADER, GC_SIZE, 100);

static size_t GetGCSize(void)
{
    return NCBI_PARAM_TYPE(WGS_LOADER, GC_SIZE)::GetDefault();
}


NCBI_PARAM_DECL(string, WGS_LOADER, VOL_PATH);
NCBI_PARAM_DEF(string, WGS_LOADER, VOL_PATH, "");

static string GetWGSVolPath(void)
{
    return NCBI_PARAM_TYPE(WGS_LOADER, VOL_PATH)::GetDefault();
}


NCBI_PARAM_DECL(bool, WGS_LOADER, RESOLVE_GIS);
NCBI_PARAM_DEF(bool, WGS_LOADER, RESOLVE_GIS, true);


static bool GetResolveGIsParam(void)
{
    return NCBI_PARAM_TYPE(WGS_LOADER, RESOLVE_GIS)::GetDefault();
}


NCBI_PARAM_DECL(bool, WGS_LOADER, RESOLVE_PROT_ACCS);
NCBI_PARAM_DEF(bool, WGS_LOADER, RESOLVE_PROT_ACCS, true);


static bool GetResolveProtAccsParam(void)
{
    static bool value =
        NCBI_PARAM_TYPE(WGS_LOADER, RESOLVE_PROT_ACCS)::GetDefault();
    return value;
}


NCBI_PARAM_DECL(bool, WGS_LOADER, SPLIT_QUALITY_GRAPH);
NCBI_PARAM_DEF(bool, WGS_LOADER, SPLIT_QUALITY_GRAPH, true);


static bool GetSplitQualityGraphParam(void)
{
    static bool value =
        NCBI_PARAM_TYPE(WGS_LOADER, SPLIT_QUALITY_GRAPH)::GetDefault();
    return value;
}


NCBI_PARAM_DECL(bool, WGS_LOADER, SPLIT_SEQUENCE);
NCBI_PARAM_DEF(bool, WGS_LOADER, SPLIT_SEQUENCE, true);


static bool GetSplitSequenceParam(void)
{
    static bool value =
        NCBI_PARAM_TYPE(WGS_LOADER, SPLIT_SEQUENCE)::GetDefault();
    return value;
}


NCBI_PARAM_DECL(bool, WGS_LOADER, SPLIT_FEATURES);
NCBI_PARAM_DEF(bool, WGS_LOADER, SPLIT_FEATURES, true);


static bool GetSplitFeaturesParam(void)
{
    static bool value =
        NCBI_PARAM_TYPE(WGS_LOADER, SPLIT_FEATURES)::GetDefault();
    return value;
}


NCBI_PARAM_DECL(bool, WGS_LOADER, KEEP_REPLACED);
NCBI_PARAM_DEF(bool, WGS_LOADER, KEEP_REPLACED, false);


static bool GetKeepReplacedParam(void)
{
    static bool value =
        NCBI_PARAM_TYPE(WGS_LOADER, KEEP_REPLACED)::GetDefault();
    return value;
}


NCBI_PARAM_DECL(unsigned, WGS_LOADER, INDEX_UPDATE_TIME);
NCBI_PARAM_DEF(unsigned, WGS_LOADER, INDEX_UPDATE_TIME, 600);


static unsigned GetUpdateTime(void)
{
    static unsigned value =
        NCBI_PARAM_TYPE(WGS_LOADER, INDEX_UPDATE_TIME)::GetDefault();
    return value;
}


/////////////////////////////////////////////////////////////////////////////
// CWGSBlobId
/////////////////////////////////////////////////////////////////////////////

CWGSBlobId::CWGSBlobId(CTempString str)
{
    FromString(str);
}


CWGSBlobId::CWGSBlobId(const CWGSFileInfo::SAccFileInfo& info)
    : m_WGSPrefix(info.file->GetWGSPrefix()),
      m_SeqType(info.seq_type),
      m_RowId(info.row_id),
      m_Version(info.version)
{
}


CWGSBlobId::~CWGSBlobId(void)
{
}


string CWGSBlobId::ToString(void) const
{
    CNcbiOstrstream out;
    out << m_WGSPrefix << '/';
    if ( m_SeqType ) {
        out << m_SeqType;
    }
    out << m_RowId;
    if ( m_Version != -1 ) {
        out << '.' << m_Version;
    }
    return CNcbiOstrstreamToString(out);
}


void CWGSBlobId::FromString(CTempString str)
{
    SIZE_TYPE slash = str.rfind('/');
    if ( slash == NPOS ) {
        NCBI_THROW_FMT(CSraException, eOtherError,
                       "Bad CWGSBlobId: "<<str);
    }
    m_WGSPrefix = str.substr(0, slash);
    str = str.substr(slash+1);
    SIZE_TYPE pos = 0;
    if ( str[pos] == 'S' || str[pos] == 'P' ) {
        m_SeqType = str[pos++];
    }
    else {
        m_SeqType = '\0';
    }
    SIZE_TYPE dot = str.rfind('.');
    if ( dot == NPOS ) {
        m_Version = -1;
    }
    else {
        m_Version = NStr::StringToNumeric<int>(str.substr(dot+1));
        str = str.substr(0, dot);
    }
    m_RowId = NStr::StringToNumeric<Uint8>(str);
}


bool CWGSBlobId::operator<(const CBlobId& id) const
{
    const CWGSBlobId& wgs2 = dynamic_cast<const CWGSBlobId&>(id);
    if ( int diff = NStr::CompareNocase(m_WGSPrefix, wgs2.m_WGSPrefix) ) {
        return diff < 0;
    }
    if ( m_SeqType != wgs2.m_SeqType ) {
        return m_SeqType < wgs2.m_SeqType;
    }
    if ( m_Version != wgs2.m_Version ) {
        return m_Version < wgs2.m_Version;
    }
    return m_RowId < wgs2.m_RowId;
}


bool CWGSBlobId::operator==(const CBlobId& id) const
{
    const CWGSBlobId& wgs2 = dynamic_cast<const CWGSBlobId&>(id);
    return m_RowId == wgs2.m_RowId &&
        m_Version == wgs2.m_Version &&
        m_SeqType == wgs2.m_SeqType &&
        m_WGSPrefix == wgs2.m_WGSPrefix;
}


/////////////////////////////////////////////////////////////////////////////
// resolver update thread

BEGIN_LOCAL_NAMESPACE;


class CIndexUpdateThread : public CThreadNonStop
{
public:
    CIndexUpdateThread(unsigned update_delay,
                       CRef<CWGSResolver> resolver)
        : CThreadNonStop(update_delay),
          m_FirstRun(true),
          m_Resolver(resolver)
        {
        }

protected:
    virtual void DoJob(void) {
        if ( m_FirstRun ) {
            // CThreadNonStop runs first iteration immediately, ignore it
            m_FirstRun = false;
            return;
        }
        try {
            if ( m_Resolver->Update() ) {
                if ( GetDebugLevel() >= 1 ) {
                    LOG_POST_X(18, Info<<"CWGSDataLoader: updated WGS index");
                }
            }
        }
        catch ( CException& exc ) {
            if ( GetDebugLevel() >= 1 ) {
                ERR_POST_X(20, "ID2WGS: "
                           "Exception while updating WGS index: "<<exc);
            }
        }
        catch ( exception& exc ) {
            if ( GetDebugLevel() >= 1 ) {
                ERR_POST_X(20, "ID2WGS: "
                           "Exception while updating WGS index: "<<exc.what());
            }
        }
    }

private:
    bool m_FirstRun;
    CRef<CWGSResolver> m_Resolver;
};


END_LOCAL_NAMESPACE;


/////////////////////////////////////////////////////////////////////////////
// CWGSDataLoader_Impl
/////////////////////////////////////////////////////////////////////////////


CWGSDataLoader_Impl::CWGSDataLoader_Impl(
    const CWGSDataLoader::SLoaderParams& params)
    : m_WGSVolPath(params.m_WGSVolPath),
      m_FoundFiles(GetGCSize()),
      m_AddWGSMasterDescr(GetMasterDescrParam()),
      m_ResolveGIs(GetResolveGIsParam()),
      m_ResolveProtAccs(GetResolveProtAccsParam())
{
    if ( m_WGSVolPath.empty() && params.m_WGSFiles.empty() ) {
        m_WGSVolPath = GetWGSVolPath();
    }
    ITERATE (vector<string>, it, params.m_WGSFiles) {
        CRef<CWGSFileInfo> info(new CWGSFileInfo(*this, *it));
        if ( !m_FixedFiles.insert(TFixedFiles::value_type(info->GetWGSPrefix(),
                                                          info)).second ) {
            NCBI_THROW_FMT(CSraException, eOtherError,
                           "Duplicated fixed WGS prefix: "<<
                           info->GetWGSPrefix());
        }
    }
}


CWGSDataLoader_Impl::~CWGSDataLoader_Impl(void)
{
    if ( m_UpdateThread ) {
        m_UpdateThread->RequestStop();
        m_UpdateThread->Join();
    }
}


CWGSResolver& CWGSDataLoader_Impl::GetResolver(void)
{
    if ( !m_Resolver ) {
        CMutexGuard guard(m_Mutex);
        if ( !m_Resolver ) {
            m_Resolver = CWGSResolver::CreateResolver(m_Mgr);
        }
        if ( m_Resolver && !m_UpdateThread ) {
#ifdef NCBI_THREADS
            m_UpdateThread = new CIndexUpdateThread(GetUpdateTime(), m_Resolver);
            m_UpdateThread->Run();
#endif
        }
    }
    return *m_Resolver;
}


CRef<CWGSFileInfo> CWGSDataLoader_Impl::GetWGSFile(const string& prefix)
{
    if ( !m_FixedFiles.empty() ) {
        // no dynamic WGS accessions
        TFixedFiles::iterator it = m_FixedFiles.find(prefix);
        if ( it != m_FixedFiles.end() ) {
            return it->second;
        }
        return null;
    }
    CRef<CWGSFileInfo> info;
    //CMutexGuard guard2(m_Mutex);
    {{
        // lookup in cache
        CMutexGuard guard(m_Mutex);
        auto& slot = m_FoundFiles[prefix];
        if ( slot ) {
            // use cached info
            info = slot;
        }
        else {
            // create new info
            slot = info = new CWGSFileInfo();
        }
    }}
    // make sure the file is opened
    try {
        info->Open(*this, prefix);
    }
    catch ( CSraException& exc ) {
        if ( exc.GetErrCode() == exc.eNotFoundDb ||
             exc.GetErrCode() == exc.eProtectedDb ) {
            // no such WGS table
            return null;
        }
        else {
            // problem in VDB or WGS reader
            throw;
        }
    }
    if ( info->GetDb()->IsReplaced() && !GetKeepReplacedParam() ) {
        // replaced
        if ( GetDebugLevel() >= 2 ) {
            ERR_POST_X(11, "CWGSDataLoader: WGS Project "<<prefix<<" is replaced");
        }
        return null;
    }
    else {
        // found
        return info;
    }
}


CWGSFileInfo::SAccFileInfo
CWGSDataLoader_Impl::GetFileInfoByGi(TGi gi)
{
    CWGSFileInfo::SAccFileInfo ret;
    if ( !m_FixedFiles.empty() ) {
        for ( auto& slot : m_FixedFiles ) {
            if ( slot.second->FindGi(ret, gi) ) {
                if ( GetDebugLevel() >= 2 ) {
                    LOG_POST_X(3, Info<<"CWGSDataLoader: "
                               "Resolved gi "<<gi<<
                               " -> "<<ret.file->GetWGSPrefix());
                }
                return ret;
            }
        }
        if ( GetDebugLevel() >= 3 ) {
            ERR_POST_X(4, "CWGSDataLoader: "
                       "Failed to resolve gi "<<gi);
        }
        return ret;
    }
    if ( 0 ) {
        CMutexGuard guard(m_Mutex);
        for ( auto& slot : m_FoundFiles ) {
            slot.second->Open(*this, slot.first);
            if ( slot.second->GetDb()->IsReplaced() && !GetKeepReplacedParam() ) {
                // replaced
                continue;
            }
            if ( slot.second->FindGi(ret, gi) ) {
                if ( GetDebugLevel() >= 2 ) {
                    LOG_POST_X(5, Info<<"CWGSDataLoader: "
                               "Resolved gi "<<gi<<
                               " -> "<<ret.file->GetWGSPrefix());
                }
                return ret;
            }
        }
    }
    CWGSResolver& resolver = GetResolver();
    CWGSResolver::TWGSPrefixes prefixes = resolver.GetPrefixes(gi);
    ITERATE ( CWGSResolver::TWGSPrefixes, it, prefixes ) {
        if ( CRef<CWGSFileInfo> file = GetWGSFile(*it) ) {
            if ( GetDebugLevel() >= 2 ) {
                LOG_POST_X(6, Info<<"CWGSDataLoader: "
                           "Resolved gi "<<gi<<
                           " -> "<<file->GetWGSPrefix());
            }
            if ( file->FindGi(ret, gi) ) {
                resolver.SetWGSPrefix(gi, prefixes, *it);
                return ret;
            }
        }
    }
    if ( !prefixes.empty() ) {
        resolver.SetNonWGS(gi, prefixes);
    }
    return ret;
}


CWGSFileInfo::SAccFileInfo
CWGSDataLoader_Impl::GetFileInfoByProtAcc(const CTextseq_id& text_id)
{
    const string& acc = text_id.GetAccession();
    CWGSFileInfo::SAccFileInfo ret;
    if ( !m_FixedFiles.empty() ) {
        for ( auto& slot : m_FixedFiles ) {
            if ( slot.second->FindProtAcc(ret, text_id) ) {
                if ( GetDebugLevel() >= 2 ) {
                    LOG_POST_X(7, Info<<"CWGSDataLoader: "
                               "Resolved prot acc "<<acc<<
                               " -> "<<ret.file->GetWGSPrefix());
                }
                return ret;
            }
        }
        if ( GetDebugLevel() >= 3 ) {
            ERR_POST_X(8, "CWGSDataLoader: "
                       "Failed to resolve prot acc "<<acc);
        }
        return ret;
    }
    if ( 0 ) {
        CMutexGuard guard(m_Mutex);
        for ( auto& slot : m_FoundFiles ) {
            slot.second->Open(*this, slot.first);
            if ( slot.second->GetDb()->IsReplaced() && !GetKeepReplacedParam() ) {
                // replaced
                continue;
            }
            if ( slot.second->FindProtAcc(ret, text_id) ) {
                if ( GetDebugLevel() >= 2 ) {
                    LOG_POST_X(9, Info<<"CWGSDataLoader: "
                               "Resolved prot acc "<<acc<<
                               " -> "<<ret.file->GetWGSPrefix());
                }
                return ret;
            }
        }
    }
    CWGSResolver& resolver = GetResolver();
    CWGSResolver::TWGSPrefixes prefixes = resolver.GetPrefixes(acc);
    ITERATE ( CWGSResolver::TWGSPrefixes, it, prefixes ) {
        if ( CRef<CWGSFileInfo> file = GetWGSFile(*it) ) {
            if ( GetDebugLevel() >= 2 ) {
                LOG_POST_X(10, Info<<"CWGSDataLoader: "
                           "Resolved prot acc "<<acc<<
                           " -> "<<file->GetWGSPrefix());
            }
            if ( file->FindProtAcc(ret, text_id) ) {
                resolver.SetWGSPrefix(acc, prefixes, *it);
                return ret;
            }
        }
    }
    if ( !prefixes.empty() ) {
        resolver.SetNonWGS(acc, prefixes);
    }
    return ret;
}


CWGSFileInfo::SAccFileInfo
CWGSDataLoader_Impl::GetFileInfoByAcc(const CTextseq_id& text_id)
{
    const string& acc = text_id.GetAccession();
    CWGSFileInfo::SAccFileInfo ret;
    CSeq_id::EAccessionInfo type = CSeq_id::IdentifyAccession(acc);
    switch ( type & CSeq_id::eAcc_division_mask ) {
        // accepted accession types
    case CSeq_id::eAcc_wgs:
    case CSeq_id::eAcc_wgs_intermed:
    case CSeq_id::eAcc_tsa:
    case CSeq_id::eAcc_targeted:
        break;
    case CSeq_id::eAcc_other:
        if ( type == CSeq_id::eAcc_embl_prot ||
             (type == CSeq_id::eAcc_gb_prot && acc.size() == 10) ) { // TODO: remove
            // Some EMBL WGS accession aren't identified as WGS, so we'll try lookup anyway
            break;
        }
        return ret;
    default:
        return ret;
    }

    if ( (type & CSeq_id::fAcc_prot) && acc.size() <= kMaxWGSProteinAccLen ) {
        if ( m_ResolveProtAccs ) {
            ret = GetFileInfoByProtAcc(text_id);
        }
        return ret;
    }

    // WGS accession
    // AAAA010000001 - prefix AAAA01 or AAAAAA01
    // optional 'S' or 'P' symbol after prefix for scaffolds and proteins
    
    // first find number of letters in prefix
    if ( acc.size() <= kMinWGSPrefixLetters+kWGSPrefixDigits ) {
        return ret;
    }
    // determine actual number of letters
    SIZE_TYPE prefix_letters = 0;
    for ( ; prefix_letters < kMaxWGSPrefixLetters; ++prefix_letters ) {
        if ( !isalpha(Uchar(acc[prefix_letters])) ) {
            if ( prefix_letters < kMinWGSPrefixLetters ) {
                return ret;
            }
            else {
                break;
            }
        }
    }
    // check prefix digits
    for ( SIZE_TYPE i = 0; i < kWGSPrefixDigits; ++i ) {
        if ( !isdigit(Uchar(acc[prefix_letters+i])) ) {
            return ret;
        }
    }
    SIZE_TYPE prefix_len = prefix_letters+kWGSPrefixDigits;
    // prefix is valid
    
    string prefix = acc.substr(0, prefix_len);
    SIZE_TYPE row_pos = prefix_len;
    if ( acc[row_pos] == 'S' || acc[row_pos] == 'P' ) {
        ret.seq_type = acc[row_pos++];
    }
    if ( acc.size() <= row_pos ) {
        return ret;
    }
    ret.row_id = NStr::StringToNumeric<Uint8>(acc.substr(row_pos),
                                              NStr::fConvErr_NoThrow);
    if ( !ret.row_id ) {
        return ret;
    }
    NStr::ToUpper(prefix);
    if ( CRef<CWGSFileInfo> info = GetWGSFile(prefix) ) {
        SIZE_TYPE row_digits = acc.size() - row_pos;
        if ( info->m_WGSDb->GetIdRowDigits() == row_digits ) {
            ret.file = info;
            if ( !ret.ValidateAcc(text_id) ) {
                ret.file = 0;
                return ret;
            }
        }
    }
    return ret;
}


CWGSFileInfo::SAccFileInfo
CWGSDataLoader_Impl::GetFileInfoByGeneral(const CDbtag& dbtag)
{
    SAccFileInfo ret;
    const CObject_id& object_id = dbtag.GetTag();
    const string& db = dbtag.GetDb();
    if ( db.size() != 8 /* WGS:AAAA */ &&
         db.size() != 10 /* WGS:AAAA01 or WGS:AAAAAA */ &&
         db.size() != 12 /* WGS:AAAAAA01 */ ) {
        return SAccFileInfo();
    }
    bool is_tsa = false;
    if ( NStr::StartsWith(db, "WGS:", NStr::eNocase) ) {
    }
    else if ( NStr::StartsWith(db, "TSA:", NStr::eNocase) ) {
        is_tsa = true;
    }
    else {
        return ret;
    }
    string wgs_acc = db.substr(4); // remove "WGS:" or "TSA:"

    NStr::ToUpper(wgs_acc);
    if ( isalpha(wgs_acc.back()&0xff) ) {
        wgs_acc += "01"; // add default version digits
    }
    CRef<CWGSFileInfo> info = GetWGSFile(wgs_acc);
    if ( !info ) {
        return ret;
    }
    const CWGSDb& wgs_db = info->GetDb();
    if ( wgs_db->IsTSA() != is_tsa ) {
        // TSA or WGS type must match
        return ret;
    }
    string tag;
    if ( object_id.IsStr() ) {
        tag = object_id.GetStr();
        NStr::ToUpper(tag);
    }
    else {
        tag = NStr::NumericToString(object_id.GetId());
    }
    if ( TVDBRowId row = wgs_db.GetContigNameRowId(tag) ) {
        ret.row_id = row;
    }
    else if ( TVDBRowId row = wgs_db.GetScaffoldNameRowId(tag) ) {
        ret.seq_type = 'S';
        ret.row_id = row;
    }
    else if ( TVDBRowId row = wgs_db.GetProteinNameRowId(tag) ) {
        ret.seq_type = 'P';
        ret.row_id = row;
    }
    if ( ret.row_id ) {
        ret.file = info;
    }
    return ret;
}


CWGSFileInfo::SAccFileInfo
CWGSDataLoader_Impl::GetFileInfo(const CSeq_id_Handle& idh)
{
    if ( m_ResolveGIs && idh.IsGi() ) {
        return GetFileInfoByGi(idh.GetGi());
    }
    switch ( idh.Which() ) { // shortcut
    case CSeq_id::e_not_set:
    case CSeq_id::e_Local:
    case CSeq_id::e_Gi:
    case CSeq_id::e_Gibbsq:
    case CSeq_id::e_Gibbmt:
    case CSeq_id::e_Giim:
    case CSeq_id::e_Patent:
    case CSeq_id::e_Pdb:
        return CWGSFileInfo::SAccFileInfo();
    case CSeq_id::e_General:
        return GetFileInfoByGeneral(idh.GetSeqId()->GetGeneral());
    default:
        break;
    }
    CConstRef<CSeq_id> id = idh.GetSeqId();
    const CTextseq_id* text_id = id->GetTextseq_Id();
    if ( !text_id ) {
        return CWGSFileInfo::SAccFileInfo();
    }
    CWGSFileInfo::SAccFileInfo ret;
    if ( text_id->IsSetAccession() ) {
        ret = GetFileInfoByAcc(*text_id);
    }
    if ( !ret ) {
        return ret;
    }
    return ret;
}


CRef<CWGSFileInfo> CWGSDataLoader_Impl::GetFileInfo(const CWGSBlobId& blob_id)
{
    return GetWGSFile(blob_id.m_WGSPrefix);
}


CWGSFileInfo::SAccFileInfo
CWGSFileInfo::SAccFileInfo::GetRootFileInfo(void) const
{
    SAccFileInfo root_info;
    if ( seq_type != 'P' ) {
        return root_info;
    }
    
    // may belong to a contig prot-set
    // proteins can be located in nuc-prot set
    TVDBRowId cds_row_id = GetProteinIterator().GetBestProductFeatRowId();
    if ( !cds_row_id ) {
        return root_info;
    }

    CWGSFeatureIterator cds_it(file->GetDb(), cds_row_id);
    if ( !cds_it ) {
        return root_info;
    }

    switch ( cds_it.GetLocSeqType() ) {
    case NCBI_WGS_seqtype_contig:
    {
        // switch to contig
        root_info.file = file;
        root_info.row_id = cds_it.GetLocRowId();
        root_info.seq_type = '\0';
        break;
    }
    case NCBI_WGS_seqtype_scaffold:
    {
        // switch to scaffold
        root_info.file = file;
        root_info.row_id = cds_it.GetLocRowId();
        root_info.seq_type = 'S';
        break;
    }
    default:
        break;
    }
    return root_info;
}


CRef<CWGSBlobId> CWGSDataLoader_Impl::GetBlobId(const CSeq_id_Handle& idh)
{
    // return blob-id of blob with sequence
    if ( CWGSFileInfo::SAccFileInfo info = GetFileInfo(idh) ) {
        if ( CWGSFileInfo::SAccFileInfo root_info = info.GetRootFileInfo() ) {
            info = root_info;
        }
        return Ref(new CWGSBlobId(info));
    }
    return null;
}


CWGSSeqIterator
CWGSFileInfo::SAccFileInfo::GetContigIterator(void) const
{
    _ASSERT(IsContig() && row_id != 0);
    CWGSSeqIterator iter(file->GetDb(), row_id,
                         CWGSSeqIterator::fIncludeAll);
    iter.SelectAccVersion(version);
    return iter;
}


CWGSScaffoldIterator
CWGSFileInfo::SAccFileInfo::GetScaffoldIterator(void) const
{
    _ASSERT(IsScaffold() && row_id != 0);
    return CWGSScaffoldIterator(file->GetDb(), row_id);
}


CWGSProteinIterator
CWGSFileInfo::SAccFileInfo::GetProteinIterator(void) const
{
    _ASSERT(IsProtein() && row_id != 0);
    return CWGSProteinIterator(file->GetDb(), row_id);
}


CWGSSeqIterator
CWGSFileInfo::GetContigIterator(const CWGSBlobId& blob_id) const
{
    _ASSERT(blob_id.m_SeqType == '\0');
    _ASSERT(blob_id.m_RowId);
    CWGSSeqIterator iter(GetDb(), blob_id.m_RowId,
                         CWGSSeqIterator::fIncludeAll);
    iter.SelectAccVersion(blob_id.m_Version);
    return iter;
}


CWGSScaffoldIterator
CWGSFileInfo::GetScaffoldIterator(const CWGSBlobId& blob_id) const
{
    _ASSERT(blob_id.m_SeqType == 'S');
    _ASSERT(blob_id.m_RowId);
    return CWGSScaffoldIterator(GetDb(), blob_id.m_RowId);
}


CWGSProteinIterator
CWGSFileInfo::GetProteinIterator(const CWGSBlobId& blob_id) const
{
    _ASSERT(blob_id.m_SeqType == 'P');
    _ASSERT(blob_id.m_RowId);
    return CWGSProteinIterator(GetDb(), blob_id.m_RowId);
}


CTSE_LoadLock CWGSDataLoader_Impl::GetBlobById(CDataSource* data_source,
                                               const CWGSBlobId& blob_id)
{
    CDataLoader::TBlobId loader_blob_id(&blob_id);
    CTSE_LoadLock load_lock = data_source->GetTSE_LoadLock(loader_blob_id);
    if ( !load_lock.IsLoaded() ) {
        LoadBlob(blob_id, load_lock);
        load_lock.SetLoaded();
    }
    return load_lock;
}


CDataLoader::TTSE_LockSet
CWGSDataLoader_Impl::GetRecords(CDataSource* data_source,
                                const CSeq_id_Handle& idh,
                                CDataLoader::EChoice choice)
{
    CDataLoader::TTSE_LockSet locks;
    if ( choice == CDataLoader::eExtAnnot ||
         choice == CDataLoader::eExtFeatures ||
         choice == CDataLoader::eExtAlign ||
         choice == CDataLoader::eExtGraph ||
         choice == CDataLoader::eOrphanAnnot ) {
        // WGS loader doesn't provide external annotations
        return locks;
    }
    // return blob-id of blob with annotations and possibly with sequence

    if ( CRef<CWGSBlobId> blob_id = GetBlobId(idh) ) {
        CDataLoader::TTSE_Lock lock = GetBlobById(data_source, *blob_id);
        if ( (lock->GetBlobState() & CBioseq_Handle::fState_no_data) &&
             (lock->GetBlobState() != CBioseq_Handle::fState_no_data) ) {
            NCBI_THROW2(CBlobStateException, eBlobStateError,
                        "blob state error for "+idh.AsString(),
                        lock->GetBlobState());
        }
        locks.insert(lock);
    }

    return locks;
}


void CWGSDataLoader_Impl::LoadBlob(const CWGSBlobId& blob_id,
                                   CTSE_LoadLock& load_lock)
{
    GetFileInfo(blob_id)->LoadBlob(blob_id, load_lock);
}


void CWGSDataLoader_Impl::LoadChunk(const CWGSBlobId& blob_id,
                                    CTSE_Chunk_Info& chunk_info)
{
    GetFileInfo(blob_id)->LoadChunk(blob_id, chunk_info);
}


void CWGSDataLoader_Impl::GetIds(const CSeq_id_Handle& idh, TIds& ids)
{
    CBioseq::TId ids2;
    if ( CWGSFileInfo::SAccFileInfo info = GetFileInfo(idh) ) {
        switch ( info.seq_type ) {
        case 'S':
            info.GetScaffoldIterator().GetIds(ids2);
            break;
        case 'P':
            info.GetProteinIterator().GetIds(ids2);
            break;
        default:
            info.GetContigIterator().GetIds(ids2);
            break;
        }
    }
    ITERATE ( CBioseq::TId, it2, ids2 ) {
        ids.push_back(CSeq_id_Handle::GetHandle(**it2));
    }
}


CDataLoader::SAccVerFound
CWGSDataLoader_Impl::GetAccVer(const CSeq_id_Handle& idh)
{
    CDataLoader::SAccVerFound ret;
    if ( CWGSFileInfo::SAccFileInfo info = GetFileInfo(idh) ) {
        ret.sequence_found = true;
        CRef<CSeq_id> acc_id;
        switch ( info.seq_type ) {
        case 'S':
            if ( CWGSScaffoldIterator it = info.GetScaffoldIterator() ) {
                acc_id = it.GetAccSeq_id();
            }
            break;
        case 'P':
            if ( CWGSProteinIterator it = info.GetProteinIterator() ) {
                acc_id = it.GetAccSeq_id();
            }
            break;
        default:
            if ( CWGSSeqIterator it = info.GetContigIterator() ) {
                acc_id = it.GetAccSeq_id();
            }
            break;
        }
        if ( acc_id ) {
            ret.acc_ver = CSeq_id_Handle::GetHandle(*acc_id);
        }
    }
    return ret;
}


CDataLoader::SGiFound
CWGSDataLoader_Impl::GetGi(const CSeq_id_Handle& idh)
{
    CDataLoader::SGiFound ret;
    if ( CWGSFileInfo::SAccFileInfo info = GetFileInfo(idh) ) {
        ret.sequence_found = true;
        if ( info.IsContig() ) {
            if ( CWGSSeqIterator it = info.GetContigIterator() ) {
                if ( it.HasGi() ) {
                    ret.gi = it.GetGi();
                }
            }
        }
        else if ( info.IsProtein() ) {
            if ( CWGSProteinIterator it = info.GetProteinIterator() ) {
                if ( it.HasGi() ) {
                    ret.gi = it.GetGi();
                }
            }
        }
    }
    return ret;
}


TTaxId CWGSDataLoader_Impl::GetTaxId(const CSeq_id_Handle& idh)
{
    if ( CWGSFileInfo::SAccFileInfo info = GetFileInfo(idh) ) {
        if ( info.IsContig() ) {
            if ( CWGSSeqIterator it = info.GetContigIterator() ) {
                if ( it.HasTaxId() ) {
                    return it.GetTaxId();
                }
            }
        }
        return ZERO_TAX_ID; // taxid is not defined
    }
    return INVALID_TAX_ID; // sequence is unknown
}


TSeqPos CWGSDataLoader_Impl::GetSequenceLength(const CSeq_id_Handle& idh)
{
    if ( CWGSFileInfo::SAccFileInfo info = GetFileInfo(idh) ) {
        switch ( info.seq_type ) {
        case 'S':
            if ( CWGSScaffoldIterator it = info.GetScaffoldIterator() ) {
                return it.GetSeqLength();
            }
            break;
        case 'P':
            if ( CWGSProteinIterator it = info.GetProteinIterator() ) {
                return it.GetSeqLength();
            }
            break;
        default:
            if ( CWGSSeqIterator it = info.GetContigIterator() ) {
                return it.GetSeqLength();
            }
            break;
        }
    }
    return kInvalidSeqPos;
}


CDataLoader::SHashFound
CWGSDataLoader_Impl::GetSequenceHash(const CSeq_id_Handle& idh)
{
    CDataLoader::SHashFound ret;
    if ( CWGSFileInfo::SAccFileInfo info = GetFileInfo(idh) ) {
        ret.sequence_found = true;
        switch ( info.seq_type ) {
        case 'S': // scaffold
            /*
            if ( CWGSScaffoldIterator it = info.GetScaffoldIterator() ) {
                return it.GetSeqHash();
            }
            */
            break;
        case 'P': // protein
            /*
            if ( CWGSProteinIterator it = info.GetProteinIterator() ) {
                return it.GetSeqHash();
            }
            */
            break;
        default:
            if ( CWGSSeqIterator it = info.GetContigIterator() ) {
                if ( it.HasSeqHash() ) {
                    ret.hash = it.GetSeqHash();
                    ret.hash_known = true;
                }
            }
            break;
        }
    }
    return ret;
}


CDataLoader::STypeFound
CWGSDataLoader_Impl::GetSequenceType(const CSeq_id_Handle& idh)
{
    CDataLoader::STypeFound ret;
    if ( CWGSFileInfo::SAccFileInfo info = GetFileInfo(idh) ) {
        ret.sequence_found = true;
        switch ( info.seq_type ) {
        case 'S':
            ret.type = info.file->GetDb()->GetScaffoldMolType();
            break;
        case 'P':
            ret.type = info.file->GetDb()->GetProteinMolType();
            break;
        default:
            ret.type = info.file->GetDb()->GetContigMolType();
            break;
        }
    }
    return ret;
}


/////////////////////////////////////////////////////////////////////////////
// CWGSFileInfo
/////////////////////////////////////////////////////////////////////////////


CWGSFileInfo::CWGSFileInfo()
{
}


CWGSFileInfo::CWGSFileInfo(const CWGSDataLoader_Impl& impl,
                           CTempString prefix)
{
    Open(impl, prefix);
}


void CWGSFileInfo::Open(const CWGSDataLoader_Impl& impl,
                        CTempString prefix)
{
    if ( m_WGSDb ) {
        return;
    }
    CMutexGuard guard(m_Mutex);
    if ( m_WGSDb ) {
        return;
    }
    try {
        x_Initialize(impl, prefix);
    }
    catch ( CSraException& exc ) {
        if ( GetDebugLevel() >= 1 ) {
            ERR_POST_X(1, "CWGSDataLoader: "
                       "Exception while opening WGS DB "<<prefix<<": "<<exc);
        }
        if ( exc.GetParam().find(prefix) == NPOS ) {
            exc.SetParam(exc.GetParam()+" acc="+string(prefix));
        }
        throw exc;
    }
    catch ( CException& exc ) {
        if ( GetDebugLevel() >= 1 ) {
            ERR_POST_X(1, "CWGSDataLoader: "
                       "Exception while opening WGS DB "<<prefix<<": "<<exc);
        }
        NCBI_RETHROW_FMT(exc, CSraException, eOtherError,
                         "CWGSDataLoader: exception while opening WGS DB "<<prefix);
    }
}


void CWGSFileInfo::x_Initialize(const CWGSDataLoader_Impl& impl,
                                CTempString prefix)
{
    auto mgr = impl.m_Mgr;
    m_WGSDb = CWGSDb(mgr, prefix, impl.m_WGSVolPath);
    m_WGSPrefix = m_WGSDb->GetIdPrefixWithVersion();
    if ( GetDebugLevel() >= 1 ) {
        LOG_POST_X(2, Info<<"CWGSDataLoader: "
                   "Opened WGS DB "<<prefix<<" -> "<<
                   GetWGSPrefix()<<" "<<m_WGSDb.GetWGSPath());
    }
    if ( impl.GetAddWGSMasterDescr() ) {
        x_InitMasterDescr();
    }
}


void CWGSFileInfo::x_InitMasterDescr(void)
{
    if ( m_WGSDb.LoadMasterDescr() ) {
        // loaded descriptors from metadata
        return;
    }
    CRef<CSeq_id> id = m_WGSDb->GetMasterSeq_id();
    if ( !id ) {
        // no master sequence id
        return;
    }
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*id);
    CDataLoader* gb_loader =
        CObjectManager::GetInstance()->FindDataLoader("GBLOADER");
    if ( !gb_loader ) {
        // no GenBank loader found -> no way to load master record
        return;
    }
    CDataLoader::TTSE_LockSet locks =
        gb_loader->GetRecordsNoBlobState(idh, CDataLoader::eBioseqCore);
    ITERATE ( CDataLoader::TTSE_LockSet, it, locks ) {
        CConstRef<CBioseq_Info> bs_info = (*it)->FindMatchingBioseq(idh);
        if ( !bs_info ) {
            continue;
        }
        if ( bs_info->IsSetDescr() ) {
            m_WGSDb.SetMasterDescr(bs_info->GetDescr().Get());
        }
        break;
    }
}


static bool s_ValidateAcc(const CConstRef<CSeq_id>& real_seq_id, const CTextseq_id& asked_text_id)
{
    if ( !real_seq_id ) {
        return false;
    }
    if ( auto real_text_id = real_seq_id->GetTextseq_Id() ) {
        if ( !NStr::EqualNocase(real_text_id->GetAccession(), asked_text_id.GetAccession()) ||
             !real_text_id->IsSetVersion() ) {
            return false;
        }
        
        if ( asked_text_id.IsSetVersion() ) {
            return real_text_id->GetVersion() == asked_text_id.GetVersion();
        }
        else {
            return true;
        }
    }
    return false;
}


bool CWGSFileInfo::SAccFileInfo::ValidateAcc(const CTextseq_id& text_id)
{
    _ASSERT(version == -1);
    if ( row_id == 0 ) {
        return false;
    }
    if ( IsScaffold() ) {
        if ( auto iter = GetScaffoldIterator() ) {
            return s_ValidateAcc(iter.GetAccSeq_id(), text_id);
        }
    }
    else if ( IsProtein() ) {
        if ( auto iter = GetProteinIterator() ) {
            if ( !GetKeepReplacedParam() &&
                 iter.GetGBState() == NCBI_gb_state_eWGSGenBankMigrated ) {
                // replaced individual protein
                if ( GetDebugLevel() >= 2 ) {
                    ERR_POST_X(11, "CWGSDataLoader: WGS protein "<<text_id.GetAccession()<<" is replaced");
                }
                return false;
            }
            return s_ValidateAcc(iter.GetAccSeq_id(), text_id);
        }
    }
    else {
        if ( auto iter = GetContigIterator() ) {
            if ( text_id.IsSetVersion() ) {
                // select requested version
                version = text_id.GetVersion();
                if ( !iter.HasAccVersion(version) ) {
                    return false;
                }
                iter.SelectAccVersion(version);
            }
            else {
                // select latest version
                version = iter.GetLatestAccVersion();
            }
            return s_ValidateAcc(iter.GetAccSeq_id(), text_id);
        }
    }
    return false;
}


bool CWGSFileInfo::SAccFileInfo::ValidateGi(TGi gi)
{
    _ASSERT(version == -1);
    if ( row_id == 0 ) {
        return false;
    }
    if ( IsScaffold() ) {
        // scaffolds cannot have GI
        return false;
    }
    else if ( IsProtein() ) {
        if ( auto iter = GetProteinIterator() ) {
            if ( !GetKeepReplacedParam() &&
                 iter.GetGBState() == NCBI_gb_state_eWGSGenBankMigrated ) {
                // replaced individual protein
                if ( GetDebugLevel() >= 2 ) {
                    ERR_POST_X(11, "CWGSDataLoader: WGS protein "<<gi<<" is replaced");
                }
                return false;
            }
            return iter.GetGi() == gi;
        }
    }
    else {
        if ( auto iter = GetContigIterator() ) {
            if ( iter.GetGi() == gi )  {
                // select latest version
                version = iter.GetLatestAccVersion();
                return true;
            }
        }
    }
    return false;
}


bool CWGSFileInfo::FindGi(SAccFileInfo& info, TGi gi)
{
    CWGSGiIterator it(m_WGSDb, gi);
    if ( it ) {
        info.file = this;
        info.row_id = it.GetRowId();
        info.seq_type = it.GetSeqType() == it.eProt? 'P': '\0';
        info.version = -1;
        if ( !info.ValidateGi(gi) ) {
            info.file = 0;
        }
        return info;
    }
    return false;
}


bool CWGSFileInfo::FindProtAcc(SAccFileInfo& info, const CTextseq_id& text_id)
{
    int ask_version = text_id.IsSetVersion()? text_id.GetVersion(): -1;
    if ( TVDBRowId row_id = m_WGSDb.GetProtAccRowId(text_id.GetAccession(), ask_version) ) {
        info.file = this;
        info.row_id = row_id;
        info.seq_type = 'P';
        info.version = -1;
        if ( !info.ValidateAcc(text_id) ) {
            info.file = 0;
        }
        return info;
    }
    return false;
}


static CBioseq_Handle::TBioseqStateFlags s_GBStateToOM(NCBI_gb_state gb_state)
{
    CBioseq_Handle::TBioseqStateFlags state = 0;
    switch ( gb_state ) {
    case NCBI_gb_state_eWGSGenBankSuppressed:
        state |= CBioseq_Handle::fState_suppress_perm;
        break;
    case NCBI_gb_state_eWGSGenBankReplaced:
    case NCBI_gb_state_eWGSGenBankMigrated:
        state |= CBioseq_Handle::fState_dead;
        break;
    case NCBI_gb_state_eWGSGenBankWithdrawn:
        state |= CBioseq_Handle::fState_withdrawn;
        break;
    default:
        break;
    }
    return state;
}


void CWGSFileInfo::LoadBlob(const CWGSBlobId& blob_id,
                            CTSE_LoadLock& load_lock) const
{
    if ( !load_lock.IsLoaded() ) {
        CBioseq_Handle::TBioseqStateFlags project_state = s_GBStateToOM(GetDb()->GetProjectGBState());
        CBioseq_Handle::TBioseqStateFlags state = project_state;
        CRef<CSeq_entry> entry;
        CRef<CID2S_Split_Info> split;
        if ( blob_id.m_SeqType == 'S' ) {
            if ( CWGSScaffoldIterator it = GetScaffoldIterator(blob_id) ) {
                entry = it.GetSeq_entry();
            }
        }
        else if ( blob_id.m_SeqType == 'P' ) {
            if ( CWGSProteinIterator it = GetProteinIterator(blob_id) ) {
                state = project_state | s_GBStateToOM(it.GetGBState());
                if ( !(state & CBioseq_Handle::fState_withdrawn) ) {
                    entry = it.GetSeq_entry();
                }
            }
        }
        else {
            if ( CWGSSeqIterator it = GetContigIterator(blob_id) ) {
                state = project_state | s_GBStateToOM(it.GetGBState());
                if ( !(state & CBioseq_Handle::fState_withdrawn) ) {
                    CWGSSeqIterator::TFlags flags = it.fDefaultFlags;
                    if ( !GetSplitQualityGraphParam() ) {
                        flags &= ~it.fSplitQualityGraph;
                    }
                    if ( !GetSplitSequenceParam() ) {
                        flags &= ~it.fSplitSeqData;
                    }
                    if ( !GetSplitFeaturesParam() ) {
                        flags &= ~it.fSplitFeatures;
                    }
                    split = it.GetSplitInfo(flags);
                    if ( !split ) {
                        entry = it.GetSeq_entry(flags);
                    }
                }
            }
        }
        if ( !entry && !split ) {
            if ( GetDebugLevel() >= 2 ) {
                ERR_POST_X(12, "CWGSDataLoader: blob "<<blob_id.ToString()<<
                           " not loaded");
            }
            state |= CBioseq_Handle::fState_no_data;
        }
        if ( entry ) {
            if ( GetDebugLevel() >=8 ) {
                LOG_POST_X(13, Info<<"CWGSDataLoader: blob "<<blob_id.ToString()<<
                           " "<<MSerial_AsnText<<*entry);
            }
        }
        if ( split ) {
            if ( GetDebugLevel() >=8 ) {
                LOG_POST_X(14, Info<<"CWGSDataLoader: blob "<<blob_id.ToString()<<
                           " "<<MSerial_AsnText<<*split);
            }
        }
        if ( state ) {
            load_lock->SetBlobState(state);
        }
        if ( split ) {
            _ASSERT(!entry);
            CSplitParser::Attach(*load_lock, *split);
        }
        else if ( entry ) {
            load_lock->SetSeq_entry(*entry);
        }
    }
}


void CWGSFileInfo::LoadChunk(const CWGSBlobId& blob_id,
                             CTSE_Chunk_Info& chunk_info) const
{
    if ( blob_id.m_SeqType == '\0' ) {
        CWGSSeqIterator it = GetContigIterator(blob_id);
        CRef<CID2S_Chunk> chunk = it.GetChunk(chunk_info.GetChunkId());
        if ( GetDebugLevel() >=8 ) {
            LOG_POST_X(15, Info<<"CWGSDataLoader: chunk "<<blob_id.ToString()<<
                       "."<<chunk_info.GetChunkId()<<
                       " "<<MSerial_AsnText<<*chunk);
        }
        CSplitParser::Load(chunk_info, *chunk);
        chunk_info.SetLoaded();
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
