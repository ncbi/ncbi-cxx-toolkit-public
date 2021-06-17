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

class CINSDCSI2CSIFilter : public CNcbiApplication
{
    virtual int  Run(void);
    virtual void Init(void);

    string  IsINSDCLine(uint64_t  line_no, const string &  line);
};



void CINSDCSI2CSIFilter::Init(void)
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->AddPositional("dump_file", "Dump file", CArgDescriptions::eString);

    SetupArgDescriptions(arg_desc.release());
}


string
CINSDCSI2CSIFilter::IsINSDCLine(uint64_t  line_no, const string &  line)
{
    // Lines from the file are like that:
    // NW_012531840,1,10,925091040,"{(11, 'ASM:GCF_000233385.1|SCF_JCF1000382137_0_0'), (12, '925091040'), (19, 'GPS_009410525')}"
    // CEMG01059413,1,6,873045931,"{(12, '873045931')}"
    // {(11, 'WGS:NZ_LACC01|FCC4LTTACXX-WHAIPI007902-100_L5_1_(PAIRED)_CONTIG_72')}
    // {(11, 'WGS:JUYR01|2471_776_28687_36+,...,2155_'), (12, '875593872')}

    auto    comma_pos = line.rfind(",");
    string  sec_seq_id_type_str = line.substr(comma_pos + 1, line.size() - comma_pos);
    string  sec_seq_id = line.substr(0, comma_pos);

    if (sec_seq_id_type_str.empty() || sec_seq_id.empty()) {
        cerr << line_no << " Bad number of items; line: " << line << endl;
        return "";
    }

    int     seq_id_type = atoi(sec_seq_id_type_str.c_str());
    if (seq_id_type == CSeq_id_Base::e_Genbank ||
        seq_id_type == CSeq_id_Base::e_Embl ||
        seq_id_type == CSeq_id_Base::e_Ddbj ||
        seq_id_type == CSeq_id_Base::e_Tpg ||
        seq_id_type == CSeq_id_Base::e_Tpe ||
        seq_id_type == CSeq_id_Base::e_Tpd
       ) {
        if (sec_seq_id[0] == '"')
            return sec_seq_id.substr(1, sec_seq_id.size() - 2);

        return sec_seq_id;
    }

    return "";
}


int CINSDCSI2CSIFilter::Run(void)
{
    const CArgs &   args = GetArgs();
    string          dump_file_name = args["dump_file"].AsString();

    ifstream        file(dump_file_name);
    if (!file.is_open()) {
        cerr << "Cannot open file " << dump_file_name << endl;
        return 1;
    }

    string          line;
    uint64_t        lines = 0;

    while (getline(file, line)) {
        ++lines;

        line = NStr::TruncateSpaces(line);
        if (line.size() == 0)
            continue;

        string  accession = IsINSDCLine(lines, line);
        if (!accession.empty())
            cout << accession << endl;
    }
    file.close();
    return 0;
}


int NcbiSys_main(int argc, ncbi::TXChar* argv[])
{
    return CINSDCSI2CSIFilter().AppMain(argc, argv);
}
