#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>

USING_SCOPE(ncbi);
USING_SCOPE(objects);

class CFastaParsableApp : public CNcbiApplication
{
    virtual int  Run(void);
    virtual void Init(void);

    vector<pair<int, string>>   ParseLine(uint64_t  line_no, const string &  line);

    int         errors;
};



void CFastaParsableApp::Init(void)
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->AddPositional("dump_file", "Dump file", CArgDescriptions::eString);
    arg_desc->AddFlag("check_minus_one", "Include checking Which() for seq_id_type -1. Default: false", true);

    SetupArgDescriptions(arg_desc.release());
}


vector<pair<int, string>>
CFastaParsableApp::ParseLine(uint64_t  line_no, const string &  line)
{
    // Lines from the file are like that:
    // NW_012531840,1,10,925091040,"{(11, 'ASM:GCF_000233385.1|SCF_JCF1000382137_0_0'), (12, '925091040'), (19, 'GPS_009410525')}"
    // CEMG01059413,1,6,873045931,"{(12, '873045931')}"
    // {(11, 'WGS:NZ_LACC01|FCC4LTTACXX-WHAIPI007902-100_L5_1_(PAIRED)_CONTIG_72')}
    // {(11, 'WGS:JUYR01|2471_776_28687_36+,...,2155_'), (12, '875593872')}
    vector<pair<int, string>>       result;

    auto    begin_pos = line.find("(");
    auto    end_pos = line.rfind(")");
    if (begin_pos == string::npos || end_pos == string::npos) {
        cerr << line_no << " Empty seq_ids list in line " << line << endl;
        return result;
    }

    // NOTE: seq_ids_list would not have the trailing ')'
    string      seq_ids_list = line.substr(begin_pos, end_pos - begin_pos);

    list<string>        item_parts;
    list<string>        parts;
    string              pair_item;
    int                 seq_id_type;
    string              fasta_content;
    NStr::Split(seq_ids_list, "), ", parts, NStr::fSplit_ByPattern);
    for (const auto &  part : parts) {
        begin_pos = part.find("(");
        if (begin_pos == string::npos) {
            cerr << line_no << " Bad seq_ids list format (unbalanced parantheses): " << line << endl;
            ++errors;
            return result;
        }

        pair_item = part.substr(begin_pos + 1, part.size() - begin_pos);
        item_parts.clear();
        NStr::Split(pair_item, ", '", item_parts, NStr::fSplit_ByPattern);
        if (item_parts.size() != 2) {
            cerr << line_no << " Bad seq_ids list format (expected two comma separated items): " << line << endl;
            ++errors;
            return result;
        }

        seq_id_type = atoi(item_parts.front().c_str());
        fasta_content = NStr::TruncateSpaces(item_parts.back());
        fasta_content = fasta_content.substr(0, fasta_content.size() - 1);  // strip ' character at the end

        result.push_back(make_pair(seq_id_type, fasta_content));
    }

    return result;
}


int CFastaParsableApp::Run(void)
{
    const CArgs &   args = GetArgs();
    string          dump_file_name = args["dump_file"].AsString();
    bool            check_minus_one = args["check_minus_one"];

    ifstream        file(dump_file_name);
    if (!file.is_open()) {
        cerr << "Cannot open file " << dump_file_name << endl;
        return 1;
    }

    errors = 0;

    string          line;
    CSeq_id         seq_id;
    uint64_t        lines = 0;

    string                      fasta_content;
    vector<pair<int, string>>   seq_ids;

    while (getline(file, line)) {
        ++lines;

        line = NStr::TruncateSpaces(line);
        if (line.size() == 0)
            continue;

        try {
            seq_ids = ParseLine(lines, line);
        } catch (const exception &  exc) {
            cerr << lines << " Line: " << line << " Cannot parse exception: " << exc.what() << endl;
            ++errors;
            continue;
        } catch (...) {
            cerr << lines << " Line: " << line << " Cannot parse unknown exception" << endl;
            ++errors;
            continue;
        }

        for (const auto &  item : seq_ids) {
            try {
                if (item.first == -1 && !check_minus_one)
                    continue;

                seq_id.Set(CSeq_id::eFasta_AsTypeAndContent,
                           (CSeq_id::E_Choice)item.first, item.second);

                // Check the type and label
                if (seq_id.Which() != (CSeq_id::E_Choice)item.first) {
                    cerr << lines << " Line: " << line
                         << " Which() inconsistency. Which() reports " << seq_id.Which()
                         << ", expected " << item.first
                         << " for fasta content " << item.second << endl;
                    ++errors;
                }

                fasta_content.clear();
                seq_id.GetLabel(&fasta_content, CSeq_id::eFastaContent);
                if (fasta_content != item.second) {
                    // May be it will match with '|' at the end...
                    if (fasta_content != item.second + "|") {
                        cerr << lines << " Line: " << line
                             << " Fasta content inconsistency. CSeq_id reports " << fasta_content
                             << ", expected " << item.second << endl;
                        ++errors;
                    }
                }
            } catch (const exception &  exc) {
                cerr << lines << " Line: " << line << " Exception: " << exc.what() << endl;
                ++errors;
            } catch (...) {
                cerr << lines << " Line: " << line << " Unknown exception" << endl;
                ++errors;
            }
        }
    }
    file.close();

    cout << "Processed " << lines << " lines" << endl;

    if (errors == 0)
        return 0;
    return 1;
}


int NcbiSys_main(int argc, ncbi::TXChar* argv[])
{
    return CFastaParsableApp().AppMain(argc, argv);
}
