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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Access to SRA files
 *
 */

#include <ncbi_pch.hpp>
#include <sra/readers/sra/sraread.hpp>

#include <klib/rc.h>
#include <klib/writer.h>
#include <sra/types.h>
#include <sra/impl.h>
#include <sra/sradb-priv.h>
#include <vfs/manager.h>
#include <vfs/resolver.h>
#include <vfs/path.h>

#include <corelib/ncbimtx.hpp>
#include <corelib/ncbi_param.hpp>
#include <sra/readers/ncbi_traces_path.hpp>
#include <objects/general/general__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seqres/seqres__.hpp>
#include <sra/error_codes.hpp>

#include <cstring>

BEGIN_NCBI_SCOPE

#define NCBI_USE_ERRCODE_X   SRAReader
NCBI_DEFINE_ERR_SUBCODE_X(1);

BEGIN_SCOPE(objects)

class CSeq_entry;


CSraException::CSraException(void)
    : m_RC(0)
{
}


CSraException::CSraException(const CDiagCompileInfo& info,
                             const CException* prev_exc,
                             EErrCode err_code,
                             const string& message,
                             EDiagSev severity)
    : CException(info, prev_exc, CException::EErrCode(err_code), message),
      m_RC(0)
{
    this->x_Init(info, message, prev_exc, severity);
    this->x_InitErrCode(CException::EErrCode(err_code));
}


CSraException::CSraException(const CDiagCompileInfo& info,
                             const CException* prev_exc,
                             EErrCode err_code,
                             const string& message,
                             rc_t rc,
                             EDiagSev severity)
    : CException(info, prev_exc, CException::EErrCode(err_code), message),
      m_RC(rc)
{
    this->x_Init(info, message, prev_exc, severity);
    this->x_InitErrCode(CException::EErrCode(err_code));
}


CSraException::CSraException(const CDiagCompileInfo& info,
                             const CException* prev_exc,
                             EErrCode err_code,
                             const string& message,
                             rc_t rc,
                             const string& param,
                             EDiagSev severity)
    : CException(info, prev_exc, CException::EErrCode(err_code), message),
      m_RC(rc),
      m_Param(param)
{
    this->x_Init(info, message, prev_exc, severity);
    this->x_InitErrCode(CException::EErrCode(err_code));
}


CSraException::CSraException(const CDiagCompileInfo& info,
                             const CException* prev_exc,
                             EErrCode err_code,
                             const string& message,
                             rc_t rc,
                             uint64_t param,
                             EDiagSev severity)
    : CException(info, prev_exc, CException::EErrCode(err_code), message),
      m_RC(rc),
      m_Param(NStr::UInt8ToString(param))
{
    this->x_Init(info, message, prev_exc, severity);
    this->x_InitErrCode(CException::EErrCode(err_code));
}


CSraException::CSraException(const CSraException& other)
    : CException( other),
      m_RC(other.m_RC),
      m_Param(other.m_Param)
{
    x_Assign(other);
}


CSraException::~CSraException(void) throw()
{
}


const CException* CSraException::x_Clone(void) const
{
    return new CSraException(*this);
}


const char* CSraException::GetType(void) const
{
    return "CSraException";
}


CSraException::TErrCode CSraException::GetErrCode(void) const
{
    return typeid(*this) == typeid(CSraException) ?
        x_GetErrCode() : CException::GetErrCode();
}


const char* CSraException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eOtherError:   return "eOtherError";
    case eNullPtr:      return "eNullPtr";
    case eAddRefFailed: return "eAddRefFailed";
    case eInvalidArg:   return "eInvalidArg";
    case eInitFailed:   return "eInitFailed";
    case eNotFound:     return "eNotFound";
    case eInvalidState: return "eInvalidState";
    case eInvalidIndex: return "eInvalidIndex";
    case eNotFoundDb:   return "eNotFoundDb";
    case eNotFoundTable: return "eNotFoundTable";
    case eNotFoundColumn: return "eNotFoundColumn";
    case eNotFoundValue: return "eNotFoundValue";
    case eDataError:    return "eDataError";
    default:            return CException::GetErrCodeString();
    }
}


ostream& operator<<(ostream& out, const CSraRcFormatter& rc)
{
    char buffer[1024];
    size_t error_len;
    RCExplain(rc.GetRC(), buffer, sizeof(buffer), &error_len);
    out << "0x" << hex << rc.GetRC() << dec << ": " << buffer;
    return out;
}


void CSraException::ReportExtra(ostream& out) const
{
    if ( m_RC ) {
        out << CSraRcFormatter(m_RC);
    }
    if ( !m_Param.empty() ) {
        if ( m_RC ) {
            out << ": ";
        }
        out << m_Param;
    }
}


void CSraException::ReportError(const char* msg, rc_t rc)
{
    ERR_POST_X(1, msg<<": "<<CSraRcFormatter(rc));
}


static const size_t kInitialPathSize = 256;

CSraPath::CSraPath(void)
{
    // AddRepPath(GetDefaultRepPath());
    // AddVolPath(GetDefaultVolPath());
}


CSraPath::CSraPath(const string& rep_path, const string& vol_path)
{
    AddRepPath(rep_path.empty()? GetDefaultRepPath(): rep_path);
    AddVolPath(vol_path.empty()? GetDefaultVolPath(): vol_path);
}


NCBI_PARAM_DECL(string, SRA, REP_PATH);
NCBI_PARAM_DEF_EX(string, SRA, REP_PATH, "",
                  eParam_NoThread, SRA_REP_PATH);


NCBI_PARAM_DECL(string, SRA, VOL_PATH);
NCBI_PARAM_DEF_EX(string, SRA, VOL_PATH, "",
                  eParam_NoThread, SRA_VOL_PATH);


string CSraPath::GetDefaultRepPath(void)
{
    return NCBI_PARAM_TYPE(SRA, REP_PATH)::GetDefault();
}


string CSraPath::GetDefaultVolPath(void)
{
    return NCBI_PARAM_TYPE(SRA, VOL_PATH)::GetDefault();
}


DEFINE_STATIC_FAST_MUTEX(sx_PathMutex);


void CSraPath::AddRepPath(const string& rep_path)
{
}


void CSraPath::AddVolPath(const string& vol_path)
{
}


string CSraPath::FindAccPath(const string& acc) const
{
    string ret;
    ret.resize(128);
    rc_t rc;
    {{
        CFastMutexGuard guard(sx_PathMutex);
        //rc = SRAPathFind(*this, acc.c_str(), &ret[0], ret.size());
        VFSManager* mgr;
        rc = VFSManagerMake(&mgr);
        if (rc == 0) {
            rc_t rc2;
            VResolver* res;
            rc = VFSManagerGetResolver(mgr, &res);
            if (rc == 0) {
                VPath* accPath;
                rc = VFSManagerMakePath(mgr, &accPath, acc.c_str());
                if (rc == 0)
                {
                    const VPath* resolvedPath;
                    rc = VResolverQuery(res, eProtocolHttp, accPath, &resolvedPath, NULL, NULL);
                    if (rc == 0)
                    {
                        for ( ;; ) {
                            String s;
                            StringInitCString(&s, ret.c_str());
                            VPathGetPath(resolvedPath, &s);
                            if ( GetRCState(rc) == rcInsufficient ) {
                                // buffer too small, realloc and repeat
                                ret.resize(ret.size()*2);
                            }
                            else {
                                break;
                            }
                        }
                        rc2 = VPathRelease(resolvedPath);
                        if (rc == 0)
                            rc = rc2;
                    }
                    rc2 = VPathRelease(accPath);
                    if (rc == 0)
                        rc = rc2;
                }
                rc2 = VResolverRelease(res);
                if (rc == 0)
                    rc = rc2;
            }
            rc2 = VFSManagerRelease(mgr);
            if (rc == 0)
                rc = rc2;
        }
    }}
    if ( rc ) {
        NCBI_THROW3(CSraException, eNotFound,
                    "Cannot find acc path", rc, acc);
    }
    SIZE_TYPE eol_pos = ret.find('\0');
    if ( eol_pos != NPOS ) {
        ret.resize(eol_pos);
    }
    return ret;
}


CSraMgr::CSraMgr(void)
    : m_Path(null),
      m_Trim(false)
{
    x_Init();
}


CSraMgr::CSraMgr(const string& rep_path, const string& vol_path,
                 ETrim trim)
    : m_Path(null),
      m_Trim(trim == eTrim)
{
    if ( !rep_path.empty() || !vol_path.empty() ) {
        m_Path = CSraPath(rep_path, vol_path);
    }
    x_Init();
}

void CSraMgr::RegisterFunctions(void)
{
}


void CSraMgr::x_Init(void)
{
    if ( rc_t rc = SRAMgrMakeRead(x_InitPtr()) ) {
        *x_InitPtr() = 0;
        NCBI_THROW2(CSraException, eInitFailed,
                    "Cannot open SRAMgr", rc);
    }
}


CSraPath& CSraMgr::GetPath(void) const
{
    if ( !m_Path ) {
        CFastMutexGuard guard(sx_PathMutex);
        if ( !m_Path ) {
            m_Path = CSraPath();
        }
    }
    return m_Path;
}


string CSraMgr::FindAccPath(const string& acc) const
{
    if ( !m_Path ) {
        return acc;
    }
    return GetPath().FindAccPath(acc);
}


spotid_t CSraMgr::GetSpotInfo(const string& sra, CSraRun& run)
{
    SIZE_TYPE dot = sra.find('.');
    string acc;
    spotid_t spot_id = 0;
    if ( dot == NPOS ) {
        NCBI_THROW3(CSraException, eInvalidArg,
                    "No spot id specified",
                    RC(rcApp, rcFunctParam, rcDecoding, rcParam, rcIncomplete),
                    sra);
    }
    else {
        acc = sra.substr(0, dot);
        spot_id = NStr::StringToUInt(sra.substr(dot+1));
    }
    if ( !run || run.GetAccession() != acc ) {
        run = CSraRun(*this, acc);
    }
    return spot_id;
}


CRef<CSeq_entry> CSraMgr::GetSpotEntry(const string& sra,
                                       CSraRun& run)
{
    return run.GetSpotEntry(GetSpotInfo(sra, run));
}


CRef<CSeq_entry> CSraMgr::GetSpotEntry(const string& sra)
{
    CSraRun run;
    return GetSpotEntry(sra, run);
}


CSraValue::CSraValue(const CSraColumn& col, spotid_t id, ECheckRc check_rc)
    : m_Error(0), m_Data(0), m_Bitoffset(0), m_Bitlen(0), m_Len(0)
{
    m_Error = SRAColumnRead(col, id, &m_Data, &m_Bitoffset, &m_Bitlen);
    if ( !m_Error ) {
        if ( m_Bitoffset ) {
            m_Error = RC(rcApp, rcColumn, rcDecoding, rcOffset, rcUnsupported);
        }
        else {
            m_Len = (m_Bitlen+7)>>3;
        }
    }
    if ( m_Error && check_rc == eCheckRc ) {
        NCBI_THROW3(CSraException, eNotFoundValue, "Cannot read value",
                    m_Error, NStr::UIntToString(id));
    }
}


void CSraRun::Init(CSraMgr& mgr, const string& acc)
{
    m_Acc = acc;
    m_Trim = mgr.GetTrim();
    x_DoInit(mgr, acc);
}


void CSraRun::x_DoInit(CSraMgr& mgr, const string& acc)
{
    if ( rc_t rc = SRAMgrOpenTableRead(mgr, x_InitPtr(),
                                       mgr.FindAccPath(acc).c_str()) ) {
        *x_InitPtr() = 0;
        NCBI_THROW3(CSraException, eNotFoundDb,
                    "Cannot open run read", rc, acc);
    }
    m_Name.Init(*this, "NAME", vdb_ascii_t);
    m_Read.Init(*this, "READ", insdc_fasta_t);
    m_Qual.Init(*this, "QUALITY", ncbi_qual1_t);
    m_SDesc.Init(*this, "SPOT_DESC", sra_spot_desc_t);
    m_RDesc.Init(*this, "READ_DESC", sra_read_desc_t);
    m_TrimStart.TryInitRc(*this, "TRIM_START", "INSDC:coord:zero");
}


rc_t CSraColumn::TryInitRc(const CSraRun& run,
                           const char* name, const char* type)
{
    return SRATableOpenColumnRead(run, x_InitPtr(), name, type);
}


void CSraColumn::Init(const CSraRun& run,
                      const char* name, const char* type)
{
    if ( rc_t rc = TryInitRc(run, name, type) ) {
        *x_InitPtr() = 0;
        NCBI_THROW3(CSraException, eInitFailed,
                    "Cannot get SRA column", rc, name);
    }
}


CRef<CSeq_entry> CSraRun::GetSpotEntry(spotid_t spot_id) const
{
    CRef<CSeq_entry> entry;
    
    CSraStringValue name(m_Name, spot_id);

    entry = new CSeq_entry();
    CBioseq_set& seqset = entry->SetSet();
    seqset.SetLevel(0);
    seqset.SetClass(seqset.eClass_other);

    CSraValueFor<SRASpotDesc> sdesc(m_SDesc, spot_id);
    TSeqPos trim_start = m_Trim && m_TrimStart?
        CSraValueFor<INSDC_coord_zero>(m_TrimStart, spot_id).Value(): 0;
    TSeqPos trim_end = sdesc->clip_qual_right;

    CSraValueFor<SRAReadDesc> rdesc(m_RDesc, spot_id);
    CSraStringValue read(m_Read, spot_id);
    CSraBytesValue qual(m_Qual, spot_id);
    int seq_count = 0;
    string id_start = GetAccession()+'.'+NStr::UIntToString(spot_id)+'.';
    for ( int r = 0; r < sdesc->num_reads; ++r ) {
        if ( rdesc[r].type != SRA_READ_TYPE_BIOLOGICAL ) {
            continue;
        }
        TSeqPos len = rdesc[r].seg.len;
        if ( len == 0 ) {
            continue;
        }
        TSeqPos start = rdesc[r].seg.start;
        TSeqPos end = start + len;
        if ( m_Trim ) {
            start = max(start, trim_start);
            end = min(end, trim_end);
            if ( start >= end ) {
                continue;
            }
            len = end - start;
        }

        CRef<CSeq_entry> seq_entry(new CSeq_entry);
        CBioseq& seq = seq_entry->SetSeq();
        
        CRef<CSeq_id> id(new CSeq_id);
        id->SetGeneral().SetDb("SRA");
        id->SetGeneral().SetTag().SetStr(id_start+NStr::UIntToString(r+1));
        seq.SetId().push_back(id);

        {{
            CRef<CSeqdesc> desc(new CSeqdesc);
            desc->SetTitle(name.Value());
            seq.SetDescr().Set().push_back(desc);
        }}
        {{
            CSeq_inst& inst = seq.SetInst();
            inst.SetRepr(inst.eRepr_raw);
            inst.SetMol(inst.eMol_na);
            inst.SetLength(len);
            inst.SetSeq_data().SetIupacna().Set()
                .assign(read.data()+start, len);
        }}
        {{
            CRef<CSeq_annot> annot(new CSeq_annot);
            CRef<CSeq_graph> graph(new CSeq_graph);
            annot->SetData().SetGraph().push_back(graph);
            graph->SetTitle("Phred Quality");
            graph->SetLoc().SetWhole(*id);
            graph->SetNumval(len);
            CByte_graph& bytes = graph->SetGraph().SetByte();
            bytes.SetAxis(0);
            CByte_graph::TValues& values = bytes.SetValues();
            values.reserve(len);
            int min = kMax_Int;
            int max = kMin_Int;
            for ( size_t i = 0; i < len; ++i ) {
                int v = qual[start+i];
                values.push_back(v);
                if ( v < min ) {
                    min = v;
                }
                if ( v > max ) {
                    max = v;
                }
            }
            bytes.SetMin(min);
            bytes.SetMax(max);

            seq.SetAnnot().push_back(annot);
        }}

        seqset.SetSeq_set().push_back(seq_entry);
        ++seq_count;
    }
    switch ( seq_count ) {
    case 0:
        entry.Reset();
        break;
    case 1:
        entry = seqset.GetSeq_set().front();
        break;
    }
    return entry;
}


CSeq_inst::TMol CSraRun::GetSequenceType(spotid_t spot_id,
                                         uint8_t read_id) const
{
    CSraValueFor<SRASpotDesc> sdesc(m_SDesc, spot_id);
    if ( read_id == 0 || read_id > sdesc->num_reads ) {
        return CSeq_inst::eMol_not_set;
    }
    return CSeq_inst::eMol_na;
}


TSeqPos CSraRun::GetSequenceLength(spotid_t spot_id,
                                   uint8_t read_id) const
{
    CSraValueFor<SRASpotDesc> sdesc(m_SDesc, spot_id);
    if ( read_id == 0 || read_id > sdesc->num_reads ) {
        return kInvalidSeqPos;
    }
    TSeqPos trim_start = m_Trim && m_TrimStart?
        CSraValueFor<INSDC_coord_zero>(m_TrimStart, spot_id).Value(): 0;
    TSeqPos trim_end = sdesc->clip_qual_right;
    CSraValueFor<SRAReadDesc> rdesc(m_RDesc, spot_id);
    size_t r = read_id-1;
    TSeqPos len = rdesc[r].seg.len;
    if ( len == 0 ) {
        return kInvalidSeqPos;
    }
    TSeqPos start = rdesc[r].seg.start;
    TSeqPos end = start + len;
    if ( m_Trim ) {
        start = max(start, trim_start);
        end = min(end, trim_end);
        if ( start >= end ) {
            return kInvalidSeqPos;
        }
        len = end - start;
    }
    return len;
}


END_SCOPE(objects)
END_NCBI_SCOPE
