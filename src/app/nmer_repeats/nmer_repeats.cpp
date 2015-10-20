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
 * Authors:  Josh Cherry
 *
 * File Description:
 *   Command-line tool for n-mer nucleotide repeat searching
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <objects/seq/Seq_data.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/readers/fasta.hpp>
#include <serial/iterator.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/IUPACna.hpp>

#include <algo/sequence/find_pattern.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
//  CNmer_repeatsApplication::


class CNmer_repeatsApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CNmer_repeatsApplication::Init(void)
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideVersion);

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "n-mer nucleotide repeat finding program");

    // Describe the expected command-line arguments

    arg_desc->AddPositional
        ("fasta_file",
         "fasta format file to read (\"-\" for stdin)",
         CArgDescriptions::eInputFile);

    arg_desc->AddDefaultKey
        ("2", "dinuc_min",
         "Minimum number of repeat units for a dinucleotide repeat "
         "(0 to suppress dinucleotide search)",
         CArgDescriptions::eInteger, "5");

    arg_desc->AddDefaultKey
        ("3", "trinuc_min",
         "Minimum number of repeat units for a trinucleotide repeat "
         "(0 to suppress trinucleotide search)",
         CArgDescriptions::eInteger, "5");

    arg_desc->AddDefaultKey
        ("4", "tetranuc_min",
         "Minimum number of repeat units for a tetranucleotide repeat "
         "(0 to suppress tetranucleotide search)",
         CArgDescriptions::eInteger, "5");


    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



/////////////////////////////////////////////////////////////////////////////
//  Run


int CNmer_repeatsApplication::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();

    vector<int> minima(5);
    minima[2] = args["2"].AsInteger();
    minima[3] = args["3"].AsInteger();
    minima[4] = args["4"].AsInteger();

    CNcbiIstream& is = args["fasta_file"].AsInputFile();

    // load sequences
    CStreamLineReader line_rdr(is);
    CFastaReader rdr(line_rdr, CFastaReader::fAssumeNuc);
    CRef<CSeq_entry> entry = rdr.ReadSet();

    // print the header line
    cout << "id\trepeat_unit\tnumber_of_repeats\tfrom\tto" << endl;

    // Do search on each sequence
    for (CTypeConstIterator<CBioseq> itr(ConstBegin(*entry));
         itr; ++itr) {
        
        vector<TSeqPos> starts, ends;

        CSeq_data seq_data;
        string seq;
        CSeqportUtil::Convert(itr->GetInst().GetSeq_data(),
                              &seq_data, CSeq_data::e_Iupacna);
        seq = seq_data.GetIupacna();

        const string id = itr->GetFirstId()->AsFastaString();

        for (unsigned int n = 2;  n <= 4;  ++n) {
            if (minima[n] == 0) {
                // 0 means don't do this search
                continue;
            }
            starts.clear();
            ends.clear();
            CFindPattern::FindNucNmerRepeats(seq, n, minima[n],
                                             starts, ends);
            for(unsigned int k = 0;  k < starts.size();  k++) {
                string rep_unit = seq.substr(starts[k], n);
                
                if (rep_unit.find_first_not_of(rep_unit.substr(0, 1), 1)
                    == string::npos) {
                    // skip it if it's a monomer repeat
                    continue;
                }
                
                if (n == 4) {
                    // check whether it's a dimer repeat too
                    if (rep_unit[2] == rep_unit[0] &&
                        rep_unit[3] == rep_unit[1]) {
                        // if so, skip it
                        continue;
                    }
                }
                
                int rep_num = (ends[k] - starts[k] + 1) / n;

                // ouput for this repeat
                cout << id << '\t' << rep_unit << '\t' << rep_num << '\t'
                     << starts[k] + 1 << '\t' << ends[k] + 1 << endl;
            }
        }
    }
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CNmer_repeatsApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CNmer_repeatsApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
