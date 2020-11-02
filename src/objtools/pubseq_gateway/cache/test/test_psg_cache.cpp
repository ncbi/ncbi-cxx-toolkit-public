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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *     Test utility for PSG LMDB cache manual testing.
 */

#include <ncbi_pch.hpp>

#include <climits>
#include <list>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objtools/pubseq_gateway/cache/psg_cache.hpp>

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;
USING_SCOPE(objects);

BEGIN_SCOPE()

using TJob = enum class EJob {
    jb_lookup_bi_primary,
    jb_lookup_bi_secondary,
    jb_lookup_primary_secondary,
    jb_lookup_blob_prop,
    jb_unpack_bi_key,
    jb_unpack_si_key,
    jb_unpack_bp_key,
    jb_last_si,
    jb_last_bi,
};

bool IsHex(char ch)
{
    return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
}

char HexToBin(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    } else if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    return -1;
}

string PrintableToHext(const string& str)
{
    stringstream ss;
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\\' && i + 2 < str.size() && IsHex(str[i + 1]) && IsHex(str[i + 2])) {
            char ch = HexToBin(str[i + 1]) * 16 + HexToBin(str[i + 2]);
            i += 2;
            ss << ch;
        } else {
            ss << str[i];
        }
    }
    return ss.str();
}

string GetListOfSeqIds(CBioseqInfoRecord::TSeqIds const& seq_ids)
{
    stringstream ss;
    bool empty = true;
    for (const auto& it : seq_ids) {
        if (!empty) {
            ss << ", ";
        }
        ss << "{" << get<0>(it) << ", " << get<1>(it) << "}";
        empty = false;
    }
    return ss.str();
}

END_SCOPE()

class CTestPsgCache
    : public CNcbiApplication
{
 public:
    CTestPsgCache()
        : m_job(TJob::jb_lookup_bi_primary)
        , m_force_version{}
        , m_force_seq_id_type{}
    {}
    virtual ~CTestPsgCache() = default;
    virtual void Init();
    virtual int Run();

 protected:
    void ParseArgs();

 private:
    bool ParsePrimarySeqId(const string& fasta_seqid, string& accession, int& version, int& seq_id_type);
    bool ParseSecondarySeqId(const string& fasta_seqid, string& seq_id_str, int& seq_id_type);

    void PrintBioseqInfo(CBioseqInfoRecord const& record) const;
    void PrintPrimaryId(CSI2CSIRecord const& record) const;
    void PrintBlobProp(CBlobRecord const& record) const;

    void LastSi2Csi(void);
    void LastBioseqInfo(void);
    void LookupBioseqInfoByPrimary(const string& fasta_seqid, int force_version, int force_seq_id_type);
    void LookupBioseqInfoByPrimaryAVT(const string& accession, int version, int seq_id_type);
    void LookupBioseqInfoBySecondary(const string& fasta_seqid, int force_seq_id_type);
    void LookupPrimaryBySecondary(const string& fasta_seqid, int force_seq_id_type);
    void LookupBlobProp(int sat, int sat_key, int64_t last_modified = -1);

    string m_BioseqInfoDbFile;
    string m_Si2csiDbFile;
    string m_BlobPropDbFile;
    unique_ptr<CPubseqGatewayCache> m_LookupCache;
    TJob m_job;
    string m_query;
    int m_force_version;
    int m_force_seq_id_type;
};

void CTestPsgCache::Init()
{
    unique_ptr<CArgDescriptions> argdesc(new CArgDescriptions());
    argdesc->SetUsageContext(GetArguments().GetProgramBasename(), "Test PSG cache");

    argdesc->AddDefaultKey("ini", "IniFile",
       "File with configuration information", CArgDescriptions::eString, "test_psg_cache.ini");
    argdesc->AddKey("j", "job", "Job type", CArgDescriptions::eString);
    argdesc->SetConstraint("j",
        &(*new CArgAllow_Strings, "bi_pri", "bi_sec", "si2csi", "blob_prop", "unp_bi", "unp_si", "unp_bp", "last_si", "last_bi"),
        CArgDescriptions::eConstraint
    );
    argdesc->AddOptionalKey("q", "query", "Query string (depends on job type)", CArgDescriptions::eString);
    argdesc->AddDefaultKey("v", "ver", "Force version", CArgDescriptions::eInteger, to_string(INT_MIN));
    argdesc->AddDefaultKey("t", "seqidtype", "Force seq_id_type", CArgDescriptions::eInteger, to_string(INT_MIN));
    SetupArgDescriptions(argdesc.release());
}

void CTestPsgCache::ParseArgs()
{
    map<string, TJob> jm({
        { "bi_pri", TJob::jb_lookup_bi_primary },
        { "bi_sec", TJob::jb_lookup_bi_secondary },
        { "si2csi", TJob::jb_lookup_primary_secondary },
        { "blob_prop", TJob::jb_lookup_blob_prop },
        { "unp_bi", TJob::jb_unpack_bi_key    },
        { "unp_si", TJob::jb_unpack_si_key    },
        { "unp_bp", TJob::jb_unpack_bp_key    },
        { "last_si", TJob::jb_last_si },
        { "last_bi", TJob::jb_last_bi },
    });

    const CArgs & args = GetArgs();
    const CNcbiRegistry & registry = GetConfig();

    m_Si2csiDbFile = registry.GetString("LMDB_CACHE", "dbfile_si2csi", "");
    m_BioseqInfoDbFile = registry.GetString("LMDB_CACHE", "dbfile_bioseq_info", "");
    m_BlobPropDbFile = registry.GetString("LMDB_CACHE", "dbfile_blob_prop", "");

    if (args["j"]) {
        auto it = jm.find(args["j"].AsString());
        if (it == jm.end()) {
            NCBI_THROW(CException, eInvalid, "Unsupported job argument");
        }
        m_job = it->second;
    }
    if (args["q"].HasValue()) {
        m_query = args["q"].AsString();
    }
    m_force_version = args["v"].AsInteger();
    m_force_seq_id_type = args["t"].AsInteger();
}

int CTestPsgCache::Run()
{
    ParseArgs();
    m_LookupCache.reset(new CPubseqGatewayCache(m_BioseqInfoDbFile, m_Si2csiDbFile, m_BlobPropDbFile));
    m_LookupCache->Open({4});

    switch (m_job) {
        case TJob::jb_last_si: {
            LastSi2Csi();
            break;
        }
        case TJob::jb_last_bi: {
            LastBioseqInfo();
            break;
        }
        case TJob::jb_lookup_bi_primary: {
            LookupBioseqInfoByPrimary(m_query, m_force_version, m_force_seq_id_type);
            break;
        }
        case TJob::jb_lookup_bi_secondary: {
            LookupBioseqInfoBySecondary(m_query, m_force_seq_id_type);
            break;
        }
        case TJob::jb_lookup_primary_secondary: {
            LookupPrimaryBySecondary(m_query, m_force_seq_id_type);
            break;
        }
        case TJob::jb_lookup_blob_prop: {
            int sat = -1;
            int sat_key = -1;
            int64_t last_modified = -1;
            list<string> list;
            NStr::Split(m_query, ", :", list,
                NStr::ESplitFlags::fSplit_MergeDelimiters | NStr::ESplitFlags::fSplit_Tokenize);
            if (list.size() >= 2) {
                auto it = list.begin();
                sat = stoi(*it);
                ++it;
                sat_key = stoi(*it);
                if (list.size() >= 3) {
                    ++it;
                    last_modified = stoi(*it);
                }
                LookupBlobProp(sat, sat_key, last_modified);
            } else {
                ERR_POST(Error << "Query parameter expected: sat,sat_key(,last_modified) ");
            }

            break;
        }
        case TJob::jb_unpack_bi_key: {
            string s = PrintableToHext(m_query);
            int version = -1;
            int seq_id_type = -1;
            int64_t gi = -1;
            string accession;
            CPubseqGatewayCache::UnpackBioseqInfoKey(s.c_str(), s.size(), accession, version, seq_id_type, gi);
            cout << accession << "." << version << "/" << seq_id_type << ":" << gi << endl;
            break;
        }
        case TJob::jb_unpack_si_key: {
            string s = PrintableToHext(m_query);
            int seq_id_type = -1;
            string seq_id;
            CPubseqGatewayCache::UnpackSiKey(s.c_str(), s.size(), seq_id_type);
            cout << seq_id << "/" << seq_id_type << endl;
            break;
        }
        case TJob::jb_unpack_bp_key: {
            string s = PrintableToHext(m_query);
            int64_t last_modified = -1;
            int32_t sat_key = -1;
            CPubseqGatewayCache::UnpackBlobPropKey(s.c_str(), s.size(), last_modified, sat_key);
            cout << sat_key << "/" << last_modified << endl;
            break;
        }
    }
    return 0;
}

bool CTestPsgCache::ParsePrimarySeqId(const string& fasta_seqid, string& accession, int& version, int& seq_id_type) {
    const CTextseq_id *tx_id = nullptr;
    try {
        CSeq_id seq_id(fasta_seqid);
        seq_id_type = seq_id.Which() == CSeq_id::e_not_set  ? -1 : static_cast<int>(seq_id.Which());
        tx_id = seq_id.GetTextseq_Id();
        if (seq_id_type != static_cast<int>(CSeq_id::e_Gi)) {
            if (tx_id) {
                if (tx_id->IsSetAccession()) {
                    accession = tx_id->GetAccession();
                    if (tx_id->IsSetVersion()) {
                        version = tx_id->GetVersion();
                    } else {
                        version = -1;
                    }
                } else if (tx_id->IsSetName()) {
                    accession = tx_id->GetName();
                }
            }
        }
    }
    catch (const exception& e) {
        ERR_POST(Error << "Failed to parse seqid: " << fasta_seqid
            << ", exception thrown: " << e.what());
        return false;
    }

    if (accession.empty()) {
        ERR_POST(Error << "Provided SeqId \"" << fasta_seqid
             << "\" is not recognized as primary. A primary would have accession[dot version]. "
             << "In order to resolve secondary identifier, use -j=bi_secondary");
        return false;
    }

    return true;
}

bool CTestPsgCache::ParseSecondarySeqId(const string& fasta_seqid, string& seq_id_str, int& seq_id_type)
{
    const CTextseq_id *tx_id = nullptr;
    try {
        CSeq_id seq_id(fasta_seqid);
        seq_id_type = seq_id.Which() == CSeq_id::e_not_set  ? -1 : static_cast<int>(seq_id.Which());
        if (seq_id_type != static_cast<int>(CSeq_id::e_Gi)) {
            tx_id = seq_id.GetTextseq_Id();
            if (tx_id) {
                if (tx_id->IsSetAccession()) {
                    seq_id_str = tx_id->GetAccession();
                    if (tx_id->IsSetVersion())
                        seq_id_str = seq_id_str + "." + to_string(tx_id->GetVersion());
                } else if (tx_id->IsSetName()) {
                    seq_id_str = tx_id->GetName();
                }
            }
        } else {
            seq_id_str = NStr::NumericToString(seq_id.GetGi());
        }
    }
    catch (const exception& e) {
        ERR_POST(Error << "Failed to parse seqid: " << fasta_seqid << ", exception thrown: " << e.what());
        return false;
    }

    if (seq_id_str.empty()) {
        ERR_POST(Error << "Provided SeqId '" << fasta_seqid
            << "' is not recognized as secondary. A secondary would be numeric GI or fasta name");
        return false;
    }
    return true;
}

void CTestPsgCache::PrintBioseqInfo(const CBioseqInfoRecord& record) const
{
    cout << "result: bioseq_info cache hit" << endl
         << "accession: " << record.GetAccession() << endl
         << "version: " << record.GetVersion() << endl
         << "seq_id_type: " << record.GetSeqIdType() << endl
         << "gi: " << record.GetGI() << endl
         << "name: " << record.GetName() << endl
         << "sat: " << record.GetSat() << endl
         << "sat_key: " << record.GetSatKey() << endl
         << "state: " << static_cast<int32_t>(record.GetState()) << endl
         << "seq_state: " << static_cast<int32_t>(record.GetSeqState()) << endl
         << "mol: " << record.GetMol() << endl
         << "hash: " << record.GetHash() << endl
         << "length: " << record.GetLength() << endl
         << "date_changed: " << record.GetDateChanged() << endl
         << "tax_id: " << record.GetTaxId() << endl
         << "seq_ids: {" << GetListOfSeqIds(record.GetSeqIds()) << "}" << endl;
}

void CTestPsgCache::PrintPrimaryId(CSI2CSIRecord const& record) const
{
    cout << "result: si2csi cache hit" << endl
         << "sec_seq_id: " << record.GetSecSeqId() << endl
         << "sec_seq_id_type: " << record.GetSecSeqIdType() << endl
         << "accession: " << record.GetAccession() << endl
         << "version: " << record.GetVersion() << endl
         << "seq_id_type: " << record.GetSeqIdType() << endl
         << "gi: " << record.GetGI() << endl;
}

void CTestPsgCache::PrintBlobProp(CBlobRecord const& record) const
{
    cout << "result: blob_prop cache hit" << endl
         << "sat_key: " << record.GetKey() << endl
         << "last_modified: " << record.GetModified() << endl
         << "class: " << record.GetClass() << endl
         << "date_asn1: " << record.GetDateAsn1() << endl
         << "div: " << record.GetDiv() << endl
         << "flags: " << record.GetFlags() << endl
         << "hup_date: " << record.GetHupDate() << endl
         << "id2_info: " << record.GetId2Info() << endl
         << "n_chunks: " << record.GetNChunks() << endl
         << "owner: " << record.GetOwner() << endl
         << "size: " << record.GetSize() << endl
         << "size_unpacked: " << record.GetSizeUnpacked() << endl
         << "username: " << record.GetUserName() << endl;
}

void CTestPsgCache::LookupBioseqInfoByPrimary(const string& fasta_seqid, int force_version, int force_seq_id_type)
{
    int version = -1;
    int seq_id_type = -1;
    string accession;

    bool res = ParsePrimarySeqId(fasta_seqid, accession, version, seq_id_type);
    cout << "Accession: '" << accession << "' , version: " << version << ", seq_id_type: " << seq_id_type << endl;
    if (!res) {
        return;
    }

    if (force_version != INT_MIN) {
        version = force_version;
    }

    if (force_seq_id_type != INT_MIN) {
        seq_id_type = force_seq_id_type;
    }

    LookupBioseqInfoByPrimaryAVT(accession, version, seq_id_type);
}

void CTestPsgCache::LastSi2Csi(void)
{
    auto response = m_LookupCache->FetchSi2CsiLast();
    if (response.empty()) {
        cout << "result: si2csi should be empty" << endl;
    } else {
        for (auto & item : response) {
            PrintPrimaryId(item);
        }
    }
}

void CTestPsgCache::LastBioseqInfo(void)
{
    auto response = m_LookupCache->FetchBioseqInfoLast();
    if (response.empty()) {
        cout << "result: bioseq_info should be empty" << endl;
    } else {
        for (auto & item : response) {
            PrintBioseqInfo(item);
        }
    }
}

void CTestPsgCache::LookupBioseqInfoByPrimaryAVT(const string& accession, int version, int seq_id_type)
{
    CPubseqGatewayCache::TBioseqInfoRequest request;
    request.SetAccession(accession);
    if (version >= 0) {
        request.SetVersion(version);
    }
    if (seq_id_type >= 0) {
        request.SetSeqIdType(seq_id_type);
    }
    auto response = m_LookupCache->FetchBioseqInfo(request);
    if (response.empty()) {
        cout << "result: bioseq_info cache miss" << endl;
    }
    for (auto & item : response) {
        PrintBioseqInfo(item);
    }
}

void CTestPsgCache::LookupBioseqInfoBySecondary(const string& fasta_seqid, int force_seq_id_type)
{
    CPubseqGatewayCache::TSi2CsiRequest request;
    int seq_id_type = -1;
    string seq_id;
    bool res = ParseSecondarySeqId(fasta_seqid, seq_id, seq_id_type);
    if (!res) {
        // fallback to direct lookup
        seq_id = fasta_seqid;
        seq_id_type = -1;
    }

    if (force_seq_id_type != INT_MIN) {
        seq_id_type = force_seq_id_type;
    }

    request.SetSecSeqId(fasta_seqid);
    if (seq_id_type >= 0) {
        request.SetSecSeqIdType(seq_id_type);
    }
    auto response = m_LookupCache->FetchSi2Csi(request);
    if (response.empty()) {
        cout << "result: si2csi cache miss or si2csi data corrupted" << endl;
    } else {
        LookupBioseqInfoByPrimaryAVT(response[0].GetAccession(), response[0].GetVersion(), response[0].GetSeqIdType());
    }
}

void CTestPsgCache::LookupPrimaryBySecondary(const string& fasta_seqid, int force_seq_id_type)
{
    CPubseqGatewayCache::TSi2CsiRequest request;
    int seq_id_type = -1;
    string seq_id;
    bool res = ParseSecondarySeqId(fasta_seqid, seq_id, seq_id_type);
    if (!res) {
        // fallback to direct lookup
        seq_id = fasta_seqid;
        seq_id_type = -1;
    }
    if (force_seq_id_type != INT_MIN) {
        seq_id_type = force_seq_id_type;
    }
    request.SetSecSeqId(fasta_seqid);
    if (seq_id_type >= 0) {
        request.SetSecSeqIdType(seq_id_type);
    }
    auto response = m_LookupCache->FetchSi2Csi(request);
    if (response.empty()) {
        cout << "result: si2csi cache miss" << endl;
    } else {
        PrintPrimaryId(response[0]);
    }
}

void CTestPsgCache::LookupBlobProp(int sat, int sat_key, int64_t last_modified)
{
    CPubseqGatewayCache::TBlobPropRequest request;
    request.SetSat(sat).SetSatKey(sat_key);
    if (last_modified > 0) {
        request.SetLastModified(last_modified);
    }
    auto response = m_LookupCache->FetchBlobProp(request);
    if (response.empty()) {
        cout << "result: blob_prop cache miss" << endl;
    }
    for (auto & item : response) {
        PrintBlobProp(item);
    }
}

int main(int argc, const char* argv[])
{
    return CTestPsgCache().AppMain(argc, argv);
}

/*
-j=bi_pri -q=NC_000852
    result: bioseq_info cache hit
    accession: NC_000852
    version: -1
    seq_id_type: 10
    sat: 4
    sat_key: 79895203
    state: 10
    mol: 1
    hash: -1714995068
    length: 330611
    date_changed: 1345755420000
    tax_id: 10506
    seq_ids: {{11, 14116}, {12, 340025671}}


-j=bi_pri -q=NC_000852.3
    result: bioseq_info cache hit
    accession: NC_000852
    version: 3
    seq_id_type: 10
    sat: 4
    sat_key: 13131352
    state: 0
    mol: 1
    hash: -69310498
    length: 330743
    date_changed: 1176933360000
    tax_id: 10506
    seq_ids: {{12, 52353967}}

-j=bi_pri -q=NC_000852.4
    result: bioseq_info cache hit
    accession: NC_000852
    version: 4
    seq_id_type: 10
    sat: 4
    sat_key: 47961402
    state: 0
    mol: 1
    hash: -1254382679
    length: 330743
    date_changed: 1310747580000
    tax_id: 10506
    seq_ids: {{12, 145309287}}

-j=bi_pri -q=NC_000852.5
    result: bioseq_info cache hit
    accession: NC_000852
    version: 5
    seq_id_type: 10
    sat: 4
    sat_key: 79895203
    state: 10
    mol: 1
    hash: -1714995068
    length: 330611
    date_changed: 1345755420000
    tax_id: 10506
    seq_ids: {{12, 340025671}, {11, 14116}}

-j=bi_pri -q="ref|NC_000852.4"
    result: bioseq_info cache hit
    accession: NC_000852
    version: 4
    seq_id_type: 10
    sat: 4
    sat_key: 47961402
    state: 0
    mol: 1
    hash: -1254382679
    length: 330743
    date_changed: 1310747580000
    tax_id: 10506
    seq_ids: {{11, NCBI_GENOMES|14116}, {12, 145309287}}


    CPsgSi2CsiCache si_cache("/data/cassandra/NEWRETRIEVAL/si2csi.db");
    rv = si_cache.LookupBySeqId("1.METASSY.1|METASSY_00010", id_type, data);
    cout << "LookupBySeqId: 1.METASSY.1|METASSY_00010: " << id_type << " rv=" << rv << ", data.size=" << data.size() << endl;
    if (rv) PrintSi2CsiData(data);

    rv = si_cache.LookupBySeqId("14116", id_type, data);
    cout << "LookupBySeqId: 14116: " << id_type << " rv=" << rv << ", data.size=" << data.size() << endl;
    if (rv) PrintSi2CsiData(data);

    rv = si_cache.LookupBySeqIdIdType("11693124", 12, data);
    cout << "LookupBySeqIdIdType: 11693124-12: " << " rv=" << rv << ", data.size=" << data.size() << endl;
    if (rv) PrintSi2CsiData(data);

    rv = si_cache.LookupBySeqIdIdType("145309287", 12, data);
    cout << "LookupBySeqIdIdType: 145309287-12: " << " rv=" << rv << ", data.size=" << data.size() << endl;
    if (rv) PrintSi2CsiData(data);

    rv = si_cache.LookupBySeqIdIdType("52353967", 12, data);
    cout << "LookupBySeqIdIdType: 52353967-12: " << " rv=" << rv << ", data.size=" << data.size() << endl;
    if (rv) PrintSi2CsiData(data);

    cout << endl << "===bp_cache===" << endl;

    int64_t last_modified;
    CPsgBlobPropCache bp_cache("/data/cassandra/NEWRETRIEVAL/blob_prop_sat_ncbi.db");

    rv = bp_cache.LookupBySatKey(142824422, last_modified, data);
    cout << "LookupBySatKey: 142824422: " << time2str(last_modified / 1000) << " rv=" << rv << ", data.size=" << data.size() << endl;
    if (rv) PrintBlobPropData(data);

    rv = bp_cache.LookupBySatKey(189880732, last_modified, data);
    cout << "LookupBySatKey: 189880732: " << time2str(last_modified / 1000) << " rv=" << rv << ", data.size=" << data.size() << endl;
    if (rv) PrintBlobPropData(data);

    rv = bp_cache.LookupBySatKey(24084221, last_modified, data);
    cout << "LookupBySatKey: 24084221: " << time2str(last_modified / 1000) << " rv=" << rv << ", data.size=" << data.size() << endl;
    if (rv) PrintBlobPropData(data);

    rv = bp_cache.LookupBySatKeyLastModified(24084221, 1530420864303, data);
    cout << "LookupBySatKeyLastModified: 24084221 " << time2str(1530420864303 / 1000) << ": rv=" << rv << ", data.size=" << data.size() << endl;
    if (rv) PrintBlobPropData(data);

    rv = bp_cache.LookupBySatKeyLastModified(24084221, 1436069133170, data);
    cout << "LookupBySatKeyLastModified: 24084221 " << time2str(1436069133170 / 1000) << ": rv=" << rv << ", data.size=" << data.size() << endl;
    if (rv) PrintBlobPropData(data);
*/
