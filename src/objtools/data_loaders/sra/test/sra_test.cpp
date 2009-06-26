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
 * Authors:  Denis Vakatov, Vladimir Ivanov
 *
 * File Description:
 *   Sample for the command-line arguments' processing ("ncbiargs.[ch]pp"):
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <sra/sradb.h>
#include <sra/ncbi-sradb.h>
#include <vdb/types.h>

#include <objects/general/general__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seqres/seqres__.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

static const bool trace = 0;

/////////////////////////////////////////////////////////////////////////////
//  CFetchAnnotsApp::


class CFetchAnnotsApp : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CFetchAnnotsApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CArgDescriptions demo program");

    arg_desc->AddOptionalKey("sra", "SRA_Accession",
                            "Accession and spot to load",
                            CArgDescriptions::eString);

    arg_desc->AddDefaultKey("o", "OutputFile",
                            "Output file of ASN.1",
                            CArgDescriptions::eOutputFile,
                            "-");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



/////////////////////////////////////////////////////////////////////////////
//  Run test (printout arguments obtained from command-line)

class CSraException : EXCEPTION_VIRTUAL_BASE public CException
{
public:
    /// Error types that  corelib can generate.
    ///
    /// These generic error conditions can occur for corelib applications.
    enum EErrCode {
        eNullPtr,       ///< Null pointer error
        eAddRefFailed,  ///< AddRef failed
        eInvalidArg,    ///< Invalid argument error
        eOtherError
    };

    /// Translate from the error code value to its string representation.
    virtual const char* GetErrCodeString(void) const;

    // Standard exception boilerplate code.
    NCBI_EXCEPTION_DEFAULT(CSraException, CException);
};


const char* CSraException::GetErrCodeString(void) const
{
    switch (GetErrCode()) {
    case eNullPtr:    return "eNullPtr";
    case eAddRefFailed: return "eAddRefFailed";
    case eInvalidArg: return "eInvalidArg";
    case eOtherError: return "eOtherError";
    default:          return CException::GetErrCodeString();
    }
}


template<class Object,
         rc_t (*FRelease)(const Object*),
         rc_t (*FAddRef)(const Object*)>
class CSraRef
{
    typedef CSraRef<Object, FRelease, FAddRef> TSelf;
public:
    typedef Object TObject;
    void Log(const char* msg) const
        {
            if ( m_Object || msg[1] ) {
                const char* type = typeid(*this).name();
                const char* start = strstr(type, "SRA");
                const char* end = strstr(start, "XadL");
                static int count = 100000;
                LOG_POST(this<<" "<<++count<<" "<<string(start, end)<<" "<<msg<<" "<<m_Object);
            }
        }

    CSraRef(void)
        : m_Object(0)
        {
        }
    CSraRef(const TSelf& ref)
        : m_Object(s_AddRef(ref))
        {
            Log("+");
        }
    TSelf& operator=(const TSelf& ref)
        {
            if ( this != &ref ) {
                Release();
                m_Object = s_AddRef(ref);
                Log("+");
            }
            return *this;
        }
    ~CSraRef(void)
        {
            Release();
        }

    void Release(void)
        {
            if ( m_Object ) {
                Log("-");
                FRelease(m_Object);
                m_Object = 0;
            }
        }
    
    operator const TObject*(void) const
        {
            if ( !m_Object ) {
                NCBI_THROW_FMT(CSraException, eNullPtr,
                               "Null SRA pointer");
            }
            return m_Object;
        }

    DECLARE_OPERATOR_BOOL(m_Object);

protected:
    const TObject** x_InitPtr(void)
        {
            Release();
            Log("init");
            return &m_Object;
        }

protected:
    static const TObject* s_AddRef(const TSelf& ref)
        {
            const TObject* obj = ref.m_Object;
            if ( obj ) {
                if ( rc_t rc = FAddRef(obj) ) {
                    NCBI_THROW_FMT(CSraException, eAddRefFailed,
                                   "Cannot add ref: "<<rc);
                }
            }
            return obj;
        }

private:
    const TObject* m_Object;
};

class CSraMgr;
class CSraRun;
class CSraColumn;

class CSraMgr : public CSraRef<SRAMgr, SRAMgrRelease, SRAMgrAddRef>
{
public:
    CSraMgr(const string& rep_path, const string& vol_path);

    CRef<CSeq_entry> GetSpotEntry(const string& sra) const;
    CRef<CSeq_entry> GetSpotEntry(const string& sra,
                                  AutoPtr<CSraRun>& run) const;
};

class CSraColumn : public CSraRef<SRAColumn, SRAColumnRelease, SRAColumnAddRef>
{
public:
    CSraColumn(const CSraRun& run, const char* name, const char* type);
};


struct CSraRunColumns
{
    CSraRunColumns(const CSraRun& run);

    CSraColumn m_Name;
    CSraColumn m_Read;
    CSraColumn m_Qual;
    CSraColumn m_SDesc;
    CSraColumn m_RDesc;
};


class CSraRun : public CSraRef<SRATable, SRATableRelease, SRATableAddRef>
{
public:
    CSraRun(const CSraMgr& mgr, const string& acc);

    const string& GetAccession(void) const
        {
            return m_Acc;
        }

    CRef<CSeq_entry> GetSpotEntry(spotid_t spot_id) const;
 
private:
    string m_Acc;
    AutoPtr<CSraRunColumns> m_Columns;
};


class CSraValue
{
public:
    CSraValue(const CSraColumn& col, spotid_t id)
        : m_Error(0), m_Data(0), m_Bitoffset(0), m_Bitlen(0), m_Len(0)
        {
            m_Error = SRAColumnRead(col, id, &m_Data, &m_Bitoffset, &m_Bitlen);
            if ( m_Error ) {
                return;
            }
            if ( m_Bitoffset ) {
                m_Error = 1;
            }
            else {
                m_Len = (m_Bitlen+7)>>3;
            }
        }

    size_t GetLength(void) const
        {
            return m_Len;
        }

    DECLARE_OPERATOR_BOOL(!m_Error);

protected:
    rc_t m_Error;
    const void* m_Data;
    bitsz_t m_Bitoffset;
    bitsz_t m_Bitlen;
    size_t m_Len;
};

class CSraStringValue : public CSraValue
{
public:
    CSraStringValue(const CSraColumn& col, spotid_t id)
        : CSraValue(col, id)
        {
        }
    const char* data(void) const
        {
            return static_cast<const char*>(m_Data);
        }
    size_t size(void) const
        {
            return GetLength();
        }
    operator string(void) const
        {
            return string(data(), size());
        }
    string Value(void) const
        {
            return *this;
        }
};

template<class V>
class CSraValueFor : public CSraValue
{
public:
    typedef V TValue;
    CSraValueFor(const CSraColumn& col, spotid_t id)
        : CSraValue(col, id)
        {
        }
    const TValue& Value(void) const
        {
            return *static_cast<const TValue*>(m_Data);
        }
    const TValue* operator->(void) const
        {
            return &Value();
        }
    const TValue& operator[](size_t i) const
        {
            return static_cast<const TValue*>(m_Data)[i];
        }
};

typedef CSraValueFor<uint16_t> CSraUInt16Value;
typedef CSraValueFor<char> CSraBytesValue;


CSraMgr::CSraMgr(const string& rep_path, const string& vol_path)
{
    if ( rc_t rc = SRAMgrMakeRead(x_InitPtr()) ) {
        NCBI_THROW_FMT(CSraException, eOtherError,
                       "Cannot open SRAMgr: " << rc);
    }
    if ( !rep_path.empty() ) {
        if ( rc_t rc = SRAMgrAddRepPath(*this, rep_path.c_str()) ) {
            NCBI_THROW_FMT(CSraException, eOtherError,
                           "Cannot add SRAMgr rep path: " << rc
                           << ": " << rep_path);
        }
    }
    if ( !vol_path.empty() ) {
        if ( rc_t rc = SRAMgrAddVolPath(*this, vol_path.c_str()) ) {
            NCBI_THROW_FMT(CSraException, eOtherError,
                           "Cannot add SRAMgr vol path: " << rc
                           << ": " << vol_path);
        }
    }
}


CRef<CSeq_entry> CSraMgr::GetSpotEntry(const string& sra) const
{
    if ( trace ) {
        NcbiCout << "Generating Seq-entry for spot "<<sra<<NcbiEndl;
    }
    SIZE_TYPE dot = sra.find('.');
    string acc;
    spotid_t spot_id = 0;
    if ( dot == NPOS ) {
        ERR_POST("No spot id defined");
        return null;
    }
    else {
        acc = sra.substr(0, dot);
        spot_id = NStr::StringToUInt(sra.substr(dot+1));
    }
    CSraRun run(*this, acc);
    return run.GetSpotEntry(spot_id);
}


CRef<CSeq_entry> CSraMgr::GetSpotEntry(const string& sra,
                                       AutoPtr<CSraRun>& run) const
{
    if ( trace ) {
        NcbiCout << "Generating Seq-entry for spot "<<sra<<NcbiEndl;
    }
    SIZE_TYPE dot = sra.find('.');
    string acc;
    spotid_t spot_id = 0;
    if ( dot == NPOS ) {
        ERR_POST("No spot id defined");
        return null;
    }
    else {
        acc = sra.substr(0, dot);
        spot_id = NStr::StringToUInt(sra.substr(dot+1));
    }
    if ( 1 || !run || run->GetAccession() != acc ) {
        run.reset(new CSraRun(*this, acc));
    }
    return run->GetSpotEntry(spot_id);
}


CSraRun::CSraRun(const CSraMgr& mgr, const string& acc)
    : m_Acc(acc)
{
    if ( rc_t rc = SRAMgrOpenRunRead(mgr, x_InitPtr(), acc.c_str()) ) {
        NCBI_THROW_FMT(CSraException, eOtherError,
                       "Cannot open run read: " << rc
                       << ": " << acc);
    }
    /*
    if ( 0 ) {
        uint64_t num_bases;
        if ( rc_t rc = SRATableBaseCount(*this, &num_bases) )
            ERR_POST("Cannot get base count: "<<rc);
        
        if ( trace ) {
            LOG_POST("Base count: "<<num_bases);
        }
    }
    uint64_t spot_count = 0;
    if ( 0 ) {
        if ( rc_t rc = SRATableSpotCount(*this, &spot_count) )
            ERR_POST("Cannot get spot count: "<<rc);
        if ( trace ) {
            LOG_POST("Spot count: "<<spot_count);
        }
    }
    spotid_t max_id = 0;
    if ( 0 ) {
        if ( rc_t rc = SRATableMaxSpotId(*this, &max_id) )
            ERR_POST("Cannot get max spot id: "<<rc);
        if ( trace ) {
            LOG_POST("Max spot id: "<<max_id);
        }
    }

    if ( 0 ) {
        SRANamelist* names = 0;
        rc_t rc = SRATableListCol(*this, &names);
        if ( rc ) ERR_POST(Fatal << "Cannot get list of columns");
        uint32_t count;
        rc = SRANamelistCount(names, &count);
        if ( rc ) ERR_POST(Fatal << "Cannot get name count");
        LOG_POST("Names: "<<count);
        const char* name = 0;
        for ( uint32_t idx = 0; idx < count; ++idx ) {
            rc = SRANamelistGet(names, idx, &name);
            LOG_POST("Name["<<idx<<"]: "<<name);
        }

        SRANamelistRelease(names);
    }
    */
    m_Columns.reset(new CSraRunColumns(*this));
}


CSraColumn::CSraColumn(const CSraRun& run, const char* name, const char* type)
{
    if ( rc_t rc = SRATableOpenColumnRead(run, x_InitPtr(),
                                          name, type) ) {
        NCBI_THROW_FMT(CSraException, eOtherError,
                       "Cannot get SRA column: "<<rc
                       <<": "<<name);
    }
}


CSraRunColumns::CSraRunColumns(const CSraRun& run)
    : m_Name(run, "NAME", vdb_ascii_t),
      m_Read(run, "READ", insdc_fasta_t),
      m_Qual(run, "QUALITY", ncbi_qual1_t),
      m_SDesc(run, "SPOT_DESC", sra_spot_desc_t),
      m_RDesc(run, "READ_DESC", sra_read_desc_t)
{
}


CRef<CSeq_entry> CSraRun::GetSpotEntry(spotid_t spot_id) const
{
    CRef<CSeq_entry> entry;
    
    if ( trace ) {
        NcbiCout << "Id: " << spot_id << NcbiEndl;
    }
    
    CSraStringValue name(m_Columns->m_Name, spot_id);
    if ( !name ) {
        ERR_POST("Cannot read spot name");
        return entry;
    }

    entry = new CSeq_entry();
    CBioseq_set& seqset = entry->SetSet();
    seqset.SetLevel(0);
    seqset.SetClass(seqset.eClass_other);

    if ( trace ) {
        NcbiCout << "Name: " << name.Value() << NcbiEndl;
    }
    
    CSraValueFor<SRASpotDesc> sdesc(m_Columns->m_SDesc, spot_id);
    if ( trace ) {
        NcbiCout << "Spot desc: \n"
                 << "    spot len: " << sdesc->spot_len << "\n"
                 << "   fixed len: " << sdesc->fixed_len << "\n"
                 << "  signal len: " << sdesc->signal_len << "\n"
                 << "  clip right: " << sdesc->clip_qual_right << "\n"
                 << "   num reads: " << int(sdesc->num_reads)
                 << NcbiEndl;
    }

    CSraValueFor<SRAReadDesc> rdesc(m_Columns->m_RDesc, spot_id);
    if ( trace ) {
        for ( int r = 0; r < sdesc->num_reads; ++r ) {
            const SRAReadDesc& rd = rdesc[r];
            NcbiCout << "Read desc["<<r<<"]: \n"
                     << "    start: " << rd.seg.start << "\n"
                     << "      len: " << rd.seg.len
                     << NcbiEndl;
        }
    }

    CSraStringValue read(m_Columns->m_Read, spot_id);
    if ( trace ) {
        NcbiCout << "Read: " << read.Value() << NcbiEndl;
    }

    CSraBytesValue qual(m_Columns->m_Qual, spot_id);
    if ( trace ) {
        NcbiCout << "Qual: ";
        for ( size_t i = 0; i < 10 && i < qual.GetLength(); ++i ) {
            NcbiCout << " " << int(qual[i]);
        }
        NcbiCout << "... of " << qual.GetLength() << NcbiEndl;
    }

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
        if ( start >= sdesc->clip_qual_right ) {
            //continue;
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


int CFetchAnnotsApp::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();

    CSraMgr mgr("/panfs/traces01", "sra0:sra1");
    
    if ( args["sra"] ) {
        CRef<CSeq_entry> entry = mgr.GetSpotEntry(args["sra"].AsString());
        args["o"].AsOutputFile() << MSerial_AsnText << *entry << NcbiFlush;
    }
    else {
        NcbiCout << "Loading data." << NcbiEndl;
        CRef<CSeq_entry> all_entries(new CSeq_entry);
        CNcbiIfstream in("/home/vasilche/data/SRR000010.asn");
        in >> MSerial_AsnText >> *all_entries;
        NcbiCout << "Scanning data." << NcbiEndl;
        size_t count = 0;
        AutoPtr<CSraRun> run;
        ITERATE(CBioseq_set::TSeq_set, it, all_entries->GetSet().GetSeq_set()){
            const CSeq_entry& e1 = **it;
            const CBioseq& seq = e1.IsSeq()? e1.GetSeq():
                e1.GetSet().GetSeq_set().front()->GetSeq();
            string tag = seq.GetId().front()->GetGeneral().GetTag().GetStr();
            string sar = tag.substr(0, tag.rfind('.'));
            if ( trace ) {
                NcbiCout << "Reading "<<sar<<NcbiEndl;
            }
            CRef<CSeq_entry> e2 = mgr.GetSpotEntry(sar, run);
            if ( !e2->Equals(e1) ) {
                NcbiCout << "Reference: "<<MSerial_AsnText<<e1;
                NcbiCout << "Generated: "<<MSerial_AsnText<<*e2;
                ERR_POST(Fatal<<"Different!");
            }
            ++count;
            if ( count >= 10 ) break;
        }
        NcbiCout << "Equal records: " << count << NcbiEndl;
    }

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CFetchAnnotsApp::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CFetchAnnotsApp().AppMain(argc, argv);
}
