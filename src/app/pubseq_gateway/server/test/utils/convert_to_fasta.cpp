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

class CSampleObjectsApplication : public CNcbiApplication
{
    virtual int  Run(void);
    virtual void Init(void);
};



void CSampleObjectsApplication::Init(void)
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->AddPositional("sec_seq_id", "Secondary seq_id", CArgDescriptions::eString);
    arg_desc->AddPositional("sec_seq_id_type", "Secondary seq_id_type", CArgDescriptions::eInteger);

    SetupArgDescriptions(arg_desc.release());
}


int CSampleObjectsApplication::Run(void)
{
    const CArgs &   args = GetArgs();
    string          sec_seq_id = args["sec_seq_id"].AsString();
    int             sec_seq_id_type = args["sec_seq_id_type"].AsInteger();

    CSeq_id         seq_id;
    seq_id.Set(CSeq_id::eFasta_AsTypeAndContent,
               (CSeq_id::E_Choice) sec_seq_id_type, sec_seq_id);

    string          fasta = seq_id.AsFastaString();

    string          fasta_type;
    seq_id.GetLabel(&fasta_type, CSeq_id::eType);

    string          fasta_content;
    seq_id.GetLabel(&fasta_content, CSeq_id::eFastaContent);

    bool    fasta_content_parsable = false;
    try {
        CSeq_id         seq_id(fasta_content);
        fasta_content_parsable = true;
    } catch (...)
    {}


    // Must work against PSG, and the resolution result returned by PSG
    // must match the last 3 columns the SI2CSI too:
    //  'seq_id=<fasta>'
    //  'seq_id=<fasta>&seq_id_type=<seq_id.Which()>'
    //  'seq_id=<fasta_content>&seq_id_type=<seq_id.Which()>'
    // Also, this should work for most (if not all -- depends on whether
    // there are any different sec_seq_id with the same sec_seq_id_type):
    //  'seq_id=<fasta_content>'
    cout << "fasta: " << fasta << endl
         << "fasta_content: " << fasta_content << endl
         << "which: " << (int) seq_id.Which() << endl
         << "fasta_type: " << fasta_type << endl
         << "fasta_content_parsable: " << fasta_content_parsable;

    return 0;
}


int NcbiSys_main(int argc, ncbi::TXChar* argv[])
{
    return CSampleObjectsApplication().AppMain(argc, argv);
}
