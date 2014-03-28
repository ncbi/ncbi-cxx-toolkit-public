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
 *   Sample test application for BAM reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_system.hpp>
#include <sra/readers/bam/bamread.hpp>
#include <sra/readers/ncbi_traces_path.hpp>

#include <objects/general/general__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <objects/seqalign/seqalign__.hpp>

#include <objtools/readers/idmapper.hpp>
#include <objtools/simple/simple_om.hpp>

#include <klib/rc.h>
#include <klib/writer.h>
#include <vfs/path.h>
#include <align/align-access.h>

#include <connect/ncbi_conn_stream.hpp>
#include <serial/serial.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/objistrasnb.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
//  CBAMTestApp::


class CBAMTestApp : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test


#define BAM_DIR1 "/1000genomes/ftp/data"
#define BAM_DIR2 "/1000genomes/ftp/phase1/data"
#define BAM_DIR3 "/1kg_pilot_data/data"
#define BAM_DIR4 "/1000genomes2/ftp/data"
#define BAM_DIR5 "/1000genomes3/ftp/data"

#define BAM_FILE1 "NA19240.mapped.SOLID.bfast.YRI.exome.20111114.bam"
#define BAM_FILE2 "NA19240.chromMT.ILLUMINA.bwa.YRI.exon_targetted.20100311.bam"
#define BAM_FILE3 "NA19240.chromMT.SLX.SRP000032.2009_04.bam"
#define BAM_FILE4 "NA19240.chrom3.SLX.SRP000032.2009_04.bam"

void CBAMTestApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CArgDescriptions demo program");

    arg_desc->AddOptionalKey("dir", "Dir",
                             "BAM files files directory",
                             CArgDescriptions::eString);
    arg_desc->AddDefaultKey("file", "File",
                            "BAM file name",
                            CArgDescriptions::eString,
                            BAM_FILE1);
    arg_desc->AddFlag("no_index", "Access BAM file without index");
    arg_desc->AddFlag("header", "Dump BAM header text");
    arg_desc->AddFlag("refseq_table", "Dump RefSeq table");
    arg_desc->AddFlag("no_short", "Do not collect short ids");
    arg_desc->AddFlag("range_only", "Collect only the range on RefSeq");
    arg_desc->AddFlag("no_ref_size", "Assume zero ref_size");

    arg_desc->AddOptionalKey("mapfile", "MapFile",
                             "IdMapper config filename",
                             CArgDescriptions::eInputFile);
    arg_desc->AddDefaultKey("genome", "Genome",
                            "UCSC build number",
                            CArgDescriptions::eString, "");

    arg_desc->AddOptionalKey("refseq", "RefSeqId",
                             "RefSeq id",
                             CArgDescriptions::eString);
    arg_desc->AddDefaultKey("refpos", "RefSeqPos",
                            "RefSeq position",
                            CArgDescriptions::eInteger,
                            "0");
    arg_desc->AddOptionalKey("refposs", "RefSeqPoss",
                            "RefSeq positions",
                            CArgDescriptions::eString);
    arg_desc->AddDefaultKey("refwindow", "RefSeqWindow",
                            "RefSeq window",
                            CArgDescriptions::eInteger,
                            "0");
    arg_desc->AddDefaultKey("limit_count", "LimitCount",
                            "Number of BAM entries to read (0 - unlimited)",
                            CArgDescriptions::eInteger,
                            "100000");
    arg_desc->AddOptionalKey("check_id", "CheckId",
                             "Compare alignments with the specified sequence",
                             CArgDescriptions::eString);
    arg_desc->AddDefaultKey("min_quality", "MinQuality",
                            "Minimal quality of alignments to check",
                            CArgDescriptions::eInteger,
                            "1");
    arg_desc->AddFlag("verbose_weak_matches", "Print light mismatches");
    arg_desc->AddFlag("no_spot_id_detector",
                      "Do not use SpotId detector to resolve short id conflicts");
    arg_desc->AddFlag("make_seq_entry", "Generated Seq-entries");
    arg_desc->AddFlag("print_seq_entry", "Print generated Seq-entry");
    arg_desc->AddFlag("ignore_errors", "Ignore errors in individual entries");
    arg_desc->AddFlag("verbose", "Print BAM data");

    arg_desc->AddDefaultKey("o", "OutputFile",
                            "Output file of ASN.1",
                            CArgDescriptions::eOutputFile,
                            "-");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



/////////////////////////////////////////////////////////////////////////////
//  Run test
/////////////////////////////////////////////////////////////////////////////

inline bool Matches(char ac, char rc)
{
    static int bits[26] = {
        0x1, 0xE, 0x2, 0xD,
        0x0, 0x0, 0x4, 0xB,
        0x0, 0x0, 0xC, 0x0,
        0x3, 0xF, 0x0, 0x0,
        0x0, 0x5, 0x6, 0x8,
        0x0, 0x7, 0x9, 0x0,
        0xA, 0x0
    };
    
    //if ( ac == rc || ac == 'N' || rc == 'N' ) return true;
    if ( ac < 'A' || ac > 'Z' || rc < 'A' || rc > 'Z' ) return false;
    if ( bits[ac-'A'] & bits[rc-'A'] ) return true;
    return false;
}

string GetShortSeqData(const CBamAlignIterator& it)
{
    string ret;
    const CBamString& src = it.GetShortSequence();
    //bool minus = it.IsSetStrand() && IsReverse(it.GetStrand());
    TSeqPos src_pos = it.GetCIGARPos();

    const CBamString& CIGAR = it.GetCIGAR();
    const char* ptr = CIGAR.data();
    const char* end = ptr + CIGAR.size();
    char type;
    TSeqPos len;
    while ( ptr != end ) {
        type = *ptr;
        for ( len = 0; ++ptr != end; ) {
            char c = *ptr;
            if ( c >= '0' && c <= '9' ) {
                len = len*10+(c-'0');
            }
            else {
                break;
            }
        }
        if ( type == 'M' || type == '=' || type == 'X' ) {
            // match
            for ( TSeqPos i = 0; i < len; ++i ) {
                ret += src[src_pos++];
            }
        }
        else if ( type == 'I' || type == 'S' ) {
            // insert
            src_pos += len;
        }
        else if ( type == 'D' || type == 'N' ) {
            ret.append(len, 'N');
        }
    }
    return ret;
}


char Complement(char c)
{
    switch ( c ) {
    case 'N': return 'N';
    case 'A': return 'T';
    case 'T': return 'A';
    case 'C': return 'G';
    case 'G': return 'C';
    case 'B': return 'V';
    case 'V': return 'B';
    case 'D': return 'H';
    case 'H': return 'D';
    case 'K': return 'M';
    case 'M': return 'K';
    case 'R': return 'Y';
    case 'Y': return 'R';
    case 'S': return 'S';
    case 'W': return 'W';
    default: Abort();
    }
}


string Reverse(const string& s)
{
    size_t size = s.size();
    string r(size, ' ');
    for ( size_t i = 0; i < size; ++i ) {
        r[i] = Complement(s[size-1-i]);
    }
    return r;
}


void CheckRc(rc_t rc)
{
    if ( rc ) {
        char buffer[1024];
        size_t error_len;
        RCExplain(rc, buffer, sizeof(buffer), &error_len);
        cerr << "SDK Error: 0x" << hex << rc << dec << ": " << buffer << endl;
        exit(1);
    }
}

class CSimpleSpotIdDetector : public CObject,
                              public CBamAlignIterator::ISpotIdDetector
{
public:
    CSimpleSpotIdDetector(void) : m_ResolveCount(0) {}

    size_t m_ResolveCount;

    void AddSpotId(string& short_id, const CBamAlignIterator* iter) {
        string seq = iter->GetShortSequence();
        if ( iter->IsSetStrand() && IsReverse(iter->GetStrand()) ) {
            seq = Reverse(seq);
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
            ++m_ResolveCount;
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

int CBAMTestApp::Run(void)
{
    if ( 0 ) {
        const AlignAccessMgr* mgr = 0;
        CheckRc(AlignAccessMgrMake(&mgr));
        VPath* bam_path = 0;
        CheckRc(VPathMakeSysPath(&bam_path, "/panfs/traces03.be-md.ncbi.nlm.nih.gov/1kg_pilot_data/data/NA10851/alignment/NA10851.SLX.maq.SRP000031.2009_08.bam"));
        VPath* bai_path = 0;
        CheckRc(VPathMakeSysPath(&bai_path, "/panfs/traces03.be-md.ncbi.nlm.nih.gov/1kg_pilot_data/data/NA10851/alignment/NA10851.SLX.maq.SRP000031.2009_08.bam.bai"));
        
        for ( int count = 0; count < 2; ++count ) {
            cout << "Step: " << count+1 << endl;
            const AlignAccessDB* bam = 0;
            CheckRc(AlignAccessMgrMakeIndexBAMDB(mgr, &bam, bam_path, bai_path));

            CheckRc(AlignAccessDBRelease(bam));
        }
        
        CheckRc(VPathRelease(bam_path));
        CheckRc(VPathRelease(bai_path));
        CheckRc(AlignAccessMgrRelease(mgr));
        cout << "Success." << endl;
        return 0;
    }


    // Get arguments
    const CArgs& args = GetArgs();

    string path;

    vector<string> dirs;
    if ( args["dir"] ) {
        dirs.push_back(args["dir"].AsString());
    }
    else {
        vector<string> reps;
        NStr::Tokenize(NCBI_TEST_BAM_FILE_PATH, ":", reps);
        ITERATE ( vector<string>, it, reps ) {
            dirs.push_back(CFile::MakePath(*it, BAM_DIR1));
            dirs.push_back(CFile::MakePath(*it, BAM_DIR2));
            dirs.push_back(CFile::MakePath(*it, BAM_DIR3));
            dirs.push_back(CFile::MakePath(*it, BAM_DIR4));
            dirs.push_back(CFile::MakePath(*it, BAM_DIR5));
        }
    }

    ITERATE ( vector<string>, it, dirs ) {
        string dir = *it;
        if ( !dir.empty() && !NStr::EndsWith(dir, "/") ) {
            dir += '/';
        }
        string file = args["file"].AsString();
        path = file;
        if ( CFile(path).Exists() ) {
            break;
        }
        path = dir + file;
        if ( CFile(path).Exists() ) {
            break;
        }
        SIZE_TYPE p1 = file.rfind('/');
        if ( p1 == NPOS ) {
            p1 = 0;
        }
        else {
            p1 += 1;
        }
        SIZE_TYPE p2 = file.find('.', p1);
        if ( p2 != NPOS ) {
            path = dir + file.substr(p1, p2-p1) + "/alignment/" + file;
            if ( CFile(path).Exists() ) {
                break;
            }
            path = dir + file.substr(p1, p2-p1) + "/exome_alignment/" + file;
            if ( CFile(path).Exists() ) {
                break;
            }
        }
        path.clear();
    }
#if 0
    if ( !CFile(path).Exists() ) {
        NcbiCerr << "\nNCBI_UNITTEST_SKIPPED "
                 << "Data+file+"<<args["file"].AsString()<<"+not+found."
                 << NcbiEndl;
        return 1;
    }
#endif
    if ( !CFile(path).Exists() ) {
        ERR_POST("Data file "<<args["file"].AsString()<<" not found.");
    }

    bool verbose_weak_matches = args["verbose_weak_matches"];
    CSeqVector sv;
    if ( args["check_id"] ) {
        sv = CSimpleOM::GetSeqVector(args["check_id"].AsString());
        if ( sv.empty() ) {
            ERR_POST(Fatal<<"Cannot get check sequence: "<<args["check_id"].AsString());
        }
        sv.SetCoding(CSeq_data::e_Iupacna);
    }

    CNcbiOstream& out = args["o"].AsOutputFile();

    for ( int cycle = 0; cycle < 1; ++cycle ) {
        if ( cycle ) path = "/panfs/traces03.be-md.ncbi.nlm.nih.gov/1kg_pilot_data/data/NA10851/alignment/NA10851.SLX.maq.SRP000031.2009_08.bam";
        out << "File: " << path << NcbiEndl;
        CBamMgr mgr;
        CBamDb bam_db;
        if ( args["no_index"] ) {
            bam_db = CBamDb(mgr, path);
        }
        else {
            bam_db = CBamDb(mgr, path, path+".bai");
        }

        if ( args["mapfile"] ) {
            bam_db.SetIdMapper(new CIdMapperConfig(args["mapfile"].AsInputFile(),
                                                   args["genome"].AsString(),
                                                   false),
                               eTakeOwnership);
        }
        else if ( !args["genome"].AsString().empty() ) {
            LOG_POST("Genome: "<<args["genome"].AsString());
            bam_db.SetIdMapper(new CIdMapperBuiltin(args["genome"].AsString(),
                                                    false),
                               eTakeOwnership);
        }

        if ( args["header"] ) {
            out << bam_db.GetHeaderText() << '\n';
        }
        if ( args["refseq_table"] ) {
            out << "RefSeq table:\n";
            for ( CBamRefSeqIterator it(bam_db); it; ++it ) {
                out << "RefSeq: " << it.GetRefSeqId() << " " << it.GetLength()
                    << '\n';
            }
        }
        int limit_count = args["limit_count"].AsInteger();
        bool ignore_errors = args["ignore_errors"];
        bool verbose = args["verbose"];
        bool make_seq_entry = args["make_seq_entry"];
        bool print_seq_entry = args["print_seq_entry"];
        int min_quality = args["min_quality"].AsInteger();
        vector<CRef<CSeq_entry> > entries;
        typedef map<string, int> TSeqDupMap;
        typedef map<string, TSeqDupMap> TConflicts;
        TConflicts conflicts;
        
        string seqdata_str;
        string conflict_asnb;
        CConn_MemoryStream conflict_memstr;
        CObjectOStreamAsnBinary conflict_objstr(conflict_memstr);

        vector<string> refseqs;
        if ( args["refseq"] ) {
            NStr::Tokenize(args["refseq"].AsString(), ",", refseqs);
        }
        else {
            refseqs.push_back(string());
        }

        ITERATE ( vector<string>, rit, refseqs ) {
            string refseq = *rit;
            int count = 0;
            CBamAlignIterator it;
            bool collect_short = false;
            bool range_only = args["range_only"];
            bool no_ref_size = args["no_ref_size"];
            if ( !refseq.empty() ) {
                collect_short = !args["no_short"];
                if ( args["refposs"] ) {
                    string str = args["refposs"].AsString();
                    CNcbiIstrstream in(str.data(), str.size());
                    TSeqPos pos;
                    while ( in >> pos ) {
                        NcbiCout << "Trying "<<pos<<NcbiFlush;
                        CBamAlignIterator ait(bam_db, refseq, pos);
                        if ( ait ) {
                            NcbiCout << " -> " << ait.GetRefSeqPos() << NcbiEndl;
                        }
                        else {
                            NcbiCout << " none" << NcbiEndl;
                        }
                    }
                }
                it = CBamAlignIterator(bam_db, refseq,
                                       args["refpos"].AsInteger(),
                                       args["refwindow"].AsInteger());
            }
            else {
                it = CBamAlignIterator(bam_db);
            }
            CRef<CSimpleSpotIdDetector> detector(new CSimpleSpotIdDetector);
            if ( !args["no_spot_id_detector"] ) {
                it.SetSpotIdDetector(detector);
            }
            string p_ref_seq_id, p_short_seq_id, p_short_seq_acc, p_data, p_cigar;
            TSeqPos p_ref_pos = 0, p_ref_size = 0;
            TSeqPos p_short_pos = 0, p_short_size = 0;
            Uint1 p_qual = 0;
            int p_strand = -1;
            typedef map<string, vector<COpenRange<TSeqPos> > > TRefIds;
            typedef vector<pair<string, TSeqPos> > TShortIds;
            TRefIds ref_ids;
            TSeqPos ref_min = 0, ref_max = 0;
            if ( range_only ) {
                ref_min = it.GetRefSeqPos();
            }
            TShortIds short_ids;

            size_t skipped_by_quality_count = 0;
            size_t perfect_match_count = 0, weak_match_count = 0;
            size_t mismatch_unmapped_count = 0, mismatch_mapped_count = 0;
            for ( ; it; ++it ) {
                if ( limit_count > 0 && count == limit_count ) break;
                try {
                    ++count;
                    TSeqPos ref_pos;
                    try {
                        ref_pos = it.GetRefSeqPos();
                        _ASSERT(ref_pos != kInvalidSeqPos);
                    }
                    catch ( CBamException& exc ) {
                        if ( exc.GetErrCode() != exc.eNoData ) {
                            throw;
                        }
                        ref_pos = kInvalidSeqPos;
                    }
                    if ( range_only ) {
                        TSeqPos ref_end = ref_pos;
                        if ( !no_ref_size ) {
                            ref_end += it.GetCIGARRefSize();
                        }
                        if ( ref_end > ref_max ) {
                            ref_max = ref_end;
                        }
                        if ( count == limit_count ) {
                            break;
                        }
                        continue;
                    }
                    TSeqPos ref_size = it.GetCIGARRefSize();
                    int qual = it.GetMapQuality();
                    string ref_seq_id;
                    if ( ref_pos == kInvalidSeqPos ) {
                        _ASSERT(ref_size == 0);
                        _ASSERT(qual == 0);
                        if ( verbose ) {
                            out << count << ": Unaligned\n";
                        }
                    }
                    else {
                        ref_seq_id = it.GetRefSeqId();
                        if ( verbose ) {
                            out << count << ": Ref: " << ref_seq_id
                                << " at [" << ref_pos
                                << " - " << (ref_pos+ref_size-1)
                                << "] = " << ref_size
                                << " Qual = " << qual
                                << '\n';
                        }
                    }
                    string short_seq_id = it.GetShortSeqId();
                    string short_seq_acc = it.GetShortSeqAcc();
                    TSeqPos short_pos = it.GetCIGARPos();
                    TSeqPos short_size = it.GetCIGARShortSize();
                    int strand = -1;
                    const char* strand_str = "";
                    bool minus = false;
                    if ( it.IsSetStrand() ) {
                        strand = it.GetStrand();
                        if ( strand == eNa_strand_plus ) {
                            strand_str = " plus";
                        }
                        else if ( strand == eNa_strand_minus ) {
                            strand_str = " minus";
                            //minus = true;
                        }
                        else {
                            strand_str = " unknown";
                        }
                    }
                    Uint2 flags = it.GetFlags();
                    bool is_paired = it.IsPaired();
                    bool is_first = it.IsFirstInPair();
                    bool is_second = it.IsSecondInPair();
                    if ( verbose ) {
                        out << "Seq: " << short_seq_id << " " << short_seq_acc
                            << " at [" << short_pos
                            << " - " << (short_pos+short_size-1)
                            << "] = " << short_size
                            << " 0x" << hex << flags << dec
                            << strand_str << " "
                            << (is_paired? "P": "")
                            << (is_first? "1": "")
                            << (is_second? "2": "")
                            << '\n';
                    }
                    if ( !(ref_seq_id != p_ref_seq_id || 
                           ref_pos != p_ref_pos ||
                           ref_size != p_ref_size ||
                           short_seq_id != p_short_seq_id ||
                           short_pos != p_short_pos ||
                           short_size != p_short_size ||
                           strand != p_strand ||
                           qual != p_qual ) ) {
                        if ( verbose ) {
                            out << "Error: match info is the same"
                                << endl;
                        }
                    }
                    if ( verbose || print_seq_entry || make_seq_entry ) {
                        string data = it.GetShortSequence();
                        if ( verbose ) {
                            out << "Sequence[" << data.size() << "]"
                                << ": " << data
                                << '\n';
                        }
                        string cigar = it.GetCIGAR();
                        if ( verbose ) {
                            out << "CIGAR: " << cigar
                                << '\n';
                        }
                        if ( cigar.empty() ) {
                            out << "Error: Empty CIGAR"
                                << endl;
                        }
                        else if ( !cigar[cigar.size()-1] ) {
                            out << "Error: Bad CIGAR: "
                                << NStr::PrintableString(cigar)
                                << endl;
                        }
                        if ( print_seq_entry || make_seq_entry ) {
                            CRef<CSeq_entry> entry = it.GetMatchEntry();
                            if ( make_seq_entry ) {
                                entries.push_back(entry);
                            }
                            if ( print_seq_entry ) {
                                out << MSerial_AsnText << *entry << '\n';
                            }
                        }
                        if ( ref_pos == p_ref_pos && strand == p_strand &&
                             cigar == p_cigar ) {
                            if ( data != p_data ) {
                                out << "Error: match data at the same place "
                                    << "is not the same"
                                    << endl;
                            }
                        }
                        else {
                            if ( !(data != p_data ||
                                   cigar != p_cigar) ) {
                                out << "Error: match data is the same"
                                    << endl;
                            }
                        }
                        p_data = data;
                        p_cigar = cigar;
                    }
                    if ( !sv.empty() && qual < min_quality ) {
                        skipped_by_quality_count += 1;
                    }
                    if ( !sv.empty() && qual >= min_quality ) {
                        const string& s = GetShortSeqData(it);
                        string av = minus? Reverse(s): s;
                        size_t match = 0, error = 0;
                        for ( TSeqPos i = 0; i < s.size(); ++i ) {
                            char ac = av[i];
                            char sc = sv[ref_pos+i];
                            if ( Matches(ac, sc) ) {
                                ++match;
                            }
                            else if ( ac != 'N' ) {
                                ++error;
                            }
                        }
                        bool print_error = false;
                        if ( error ) {
                            if ( error > match ) {
                                if ( qual == 0 ) {
                                    mismatch_unmapped_count += 1;
                                    if ( verbose ) {
                                        print_error = true;
                                        out << "Strand:" << strand_str << " ";
                                        out << "Mismatch: bad" << endl;
                                    }
                                }
                                else {
                                    mismatch_mapped_count += 1;
                                    print_error = true;
                                    out << "Strand:" << strand_str << " ";
                                    out << "Mismatch: bad, qual: " << qual
                                        << endl;
                                }
                            }
                            else {
                                weak_match_count += 1;
                                if ( verbose_weak_matches ) {
                                    print_error = true;
                                    out << "Strand:" << strand_str << " ";
                                    out << "Mismatch: weak" << endl;
                                }
                            }
                        }
                        else {
                            perfect_match_count += 1;
                            if ( verbose ) {
                                print_error = true;
                                out << "Strand:" << strand_str << " ";
                                out << "Matches perfectly" << endl;
                            }
                        }
                        if ( print_error ) {
                            if ( !verbose ) {
                                out << count << ": Ref: " << ref_seq_id
                                    << " at [" << ref_pos
                                    << " - " << (ref_pos+ref_size-1)
                                    << "] = " << ref_size
                                    << " Qual = " << qual
                                    << '\n';
                                out << "Seq: " << short_seq_id << " " << short_seq_acc
                                    << " at [" << short_pos
                                    << " - " << (short_pos+short_size-1)
                                    << "] = " << short_size
                                    << " 0x" << hex << flags << dec
                                    << strand_str << " "
                                    << (is_paired? "P": "")
                                    << (is_first? "1": "")
                                    << (is_second? "2": "")
                                    << endl;
                            }
                            out << "Matched: "<<match<<" error: "<<error<<endl;
                            sv.GetSeqData(ref_pos, ref_pos+ref_size,
                                          seqdata_str);
                            out << "Short data: " << s << endl;
                            if ( minus )
                                out << "Align data: " << av << endl;
                            out << "Check data: " << seqdata_str << endl;
                        }
                    }
                    out << flush;
                    if ( qual >= min_quality ) {
                        conflict_objstr << *it.GetShortBioseq();
                        conflict_objstr.FlushBuffer();
                        conflict_memstr.ToString(&conflict_asnb);
                        conflicts[it.GetShortSeq_id()->AsFastaString()]
                            [conflict_asnb] += 1;
                    }
                    p_ref_seq_id = ref_seq_id;
                    p_ref_pos = ref_pos;
                    p_ref_size = ref_size;
                    p_short_seq_id = short_seq_id;
                    p_short_pos = short_pos;
                    p_short_size = short_size;
                    p_qual = qual;
                    p_strand = strand;
                    ref_ids[ref_seq_id]
                        .push_back(COpenRange<TSeqPos>(ref_pos, ref_pos+ref_size));
                    if ( collect_short ) {
                        short_ids.push_back(make_pair(short_seq_id, ref_pos));
                    }
                }
                catch ( CException& exc ) {
                    ERR_POST("Error: "<<exc.what());
                    if ( !ignore_errors ) {
                        throw;
                    }
                }
                if ( limit_count > 0 && count == limit_count ) break;
            }
            out << "Loaded: " << count << " alignments." << NcbiEndl;
            if ( range_only ) {
                out << "Range: " << ref_min << "-" << ref_max-1 << NcbiEndl;
            }
            else {
                NON_CONST_ITERATE ( TRefIds, it, ref_ids ) {
                    out << it->first << ": " << it->second.size() << NcbiFlush;
                    sort(it->second.begin(), it->second.end());
                    out << "    " << it->second[0].GetFrom() << "-"
                        << it->second.back().GetToOpen()-1 << NcbiEndl;
                }
                if ( collect_short && !short_ids.empty() ) {
                    sort(short_ids.begin(), short_ids.end());
                    out << "Short: " << short_ids[0].first << " - "
                        << short_ids.back().first << NcbiEndl;
                }
                out << "Sorted." << NcbiEndl;
            }
            if ( !sv.empty() ) {
                if ( skipped_by_quality_count ) {
                    out << "Skipped low quality: " << skipped_by_quality_count
                        << endl;
                }
                if ( perfect_match_count ) {
                    out << "Perfect matches: " << perfect_match_count << endl;
                }
                if ( weak_match_count ) {
                    out << "Weak matches: " << weak_match_count << endl;
                }
                if ( mismatch_unmapped_count ) {
                    out << "Unmapped mismatches: " << mismatch_unmapped_count
                        << endl;
                }
                if ( mismatch_mapped_count ) {
                    out << "Mapped mismatches: " << mismatch_mapped_count
                        << endl;
                }
            }
            //SleepMilliSec(3000);
            int conflict_count = 0, dup_count = 0;
            ITERATE ( TConflicts, it, conflicts ) {
                if ( it->second.size() > 1 ) {
                    out << "Conflict id: " << it->first << NcbiEndl;
                    ++conflict_count;
                    if ( verbose ) {
                        ITERATE ( TSeqDupMap, it2, it->second ) {
                            CObjectIStreamAsnBinary str(it2->first.data(),
                                                        it2->first.size());
                            CRef<CBioseq> seq(new CBioseq);
                            str >> *seq;
                            NcbiCout << MSerial_AsnText << *seq << NcbiEndl;
                        }
                    }
                }
                ITERATE ( TSeqDupMap, it2, it->second ) {
                    dup_count += it2->second - 1;
                }
            }
            if ( conflict_count ) {
                out << "Conflicts: " << conflict_count << NcbiEndl;
            }
            if ( dup_count ) {
                out << "Short seq dups: " << dup_count << NcbiEndl;
            }
            if ( detector->m_ResolveCount ) {
                out << "SpotId detector resolve count: " <<
                    detector->m_ResolveCount << NcbiEndl;
            }
        }
    }
    out << "Exiting" << NcbiEndl;
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CBAMTestApp::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CBAMTestApp().AppMain(argc, argv);
}
