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
 * Authors:  Richa Agarwala
 *
 * File Description:
 *   Program to read nucleotide FASTA and produce DUST'd version.
 */

/// @file dust_app.cpp
/// This is the top level implementation file of dust_app, a utility
/// to find low complexity NA regions in FASTA format sequence data.

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbistr.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/iterator.hpp>

// Objects includes
#include <corelib/ncbidbg.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <algo/blast/core/blast_dust.h>
#include <algo/blast/core/blast_filter.h>
#include <objtools/readers/fasta.hpp> 

#define FASTA_LEN 60

USING_NCBI_SCOPE;
using namespace objects;


/////////////////////////////////////////////////////////////////////////////
//  CDustApplication::


class CDustApplication : public CNcbiApplication
{
public:
    static const char * const USAGE_LINE;
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
};

/////////////////////////////////////////////////////////////////////////////
//  Usage description
const char * const
CDustApplication::USAGE_LINE = "Dust nucleotide sequence";


/////////////////////////////////////////////////////////////////////////////
//  Get arguments


void CDustApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              USAGE_LINE);

    // Describe the expected command-line arguments
    arg_desc->AddKey("input", "Inputfile",
                        "The input file", CArgDescriptions::eInputFile);

    arg_desc->AddKey("output", "Outputfile",
                        "The output file", CArgDescriptions::eOutputFile);

    arg_desc->AddDefaultKey( "window", "window_size", "window size",
                             CArgDescriptions::eInteger, "64" );

    arg_desc->AddDefaultKey( "level", "level", "minimum level",
                             CArgDescriptions::eInteger, "20" );

    arg_desc->AddDefaultKey( "linker", "linker", "link windows apart by <linker> bp",
                             CArgDescriptions::eInteger, "1" );

    arg_desc->AddDefaultKey("segments", "segments", "Print segments instead of FASTA", CArgDescriptions::eBoolean, "F");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}

static string GetFastaDefline(const CBioseq & bioseq)
{
    string seq_id_description = 
           CSeq_id::GetStringDescr(bioseq, CSeq_id::eFormat_FastA);

    if ((seq_id_description.size() < 5) ||
        (NStr::CompareCase(seq_id_description, 0, 4, "lcl|") != 0))
    {
        NcbiCerr << "Input Bioseq was not generated as a local entry" << endl;
        exit(0);
    }
    // Take "lcl|" out of id and prefix it with ">"
    string defline = ">"+ seq_id_description.substr(4,seq_id_description.size()-4);

    // Add title if present
    if (bioseq.CanGetDescr())
    {
        const CSeq_descr & title_seq = bioseq.GetDescr();
        if (title_seq.CanGet())
        {
            const CSeq_descr_Base::Tdata & title = title_seq.Get();
            if (!title.empty())
            {
                CSeq_descr_Base::Tdata::const_iterator it = title.begin();
                CRef<CSeqdesc> ref_desc = *it;
                if (it != title.end())
                {
                    defline.append(1, ' '); // space between id and title
                    ref_desc->GetLabel(&defline, CSeqdesc::eContent);
                }
            }
        }
    }

    return defline;
}

int CDustApplication::Run(void)
{
    CRef<CSeq_entry> seq_entry;
    TSeqPos i, start, end;
    Uint1 *sequence;                // Sequnce in BLASTNA format
    BlastSeqLoc* dust_loc = NULL;   // Regions that get DUST'd
    BlastSeqLoc* temp_loc = NULL;   

    // Get arguments
    CArgs args = GetArgs();
    Int2 window = args["window"].AsInteger();
    Int2 level = args["level"].AsInteger();
    Int2 linker = args["linker"].AsInteger();
    Boolean PrintSegments = args["segments"].AsBoolean();

    // Set flags for reading data
    TReadFastaFlags flags = 0;
    flags = fReadFasta_ForceType;
    flags |= fReadFasta_AssumeNuc;
    flags |= fReadFasta_OneSeq;
    flags |= fReadFasta_NoParseID;

    // Open files
    CNcbiIfstream input_stream(args["input"].AsString().c_str(), IOS_BASE::in);
    CNcbiOfstream output_stream(args["output"].AsString().c_str(), IOS_BASE::out);

    // Read data
    int counter = 0;
    while (!input_stream.eof())
    {
        // Get one sequence from Fasta file
        vector<CConstRef<CSeq_loc> > lcase_mask;
        seq_entry = ReadFasta(input_stream, flags, &counter, &lcase_mask);
        if (seq_entry->IsSeq() && seq_entry->GetSeq().IsNa())
        {
            const CBioseq & bioseq = seq_entry->GetSeq();
            if (bioseq.CanGetInst() &&
                bioseq.GetInst().CanGetLength() &&
                bioseq.GetInst().CanGetSeq_data() )
            {
                // Get sequence id
                string seq_id_description = 
                       CSeq_id::GetStringDescr(bioseq, CSeq_id::eFormat_FastA);

                // Get defline
                string defline = GetFastaDefline(bioseq);
    
                // Get sequence length
                TSeqPos seq_length = bioseq.GetInst().GetLength();
    
                // Get nucleotide data for sequence
                const CSeq_data & seqdata = bioseq.GetInst().GetSeq_data();

                // Convert to Iupacna so don't have to worry about packing
                auto_ptr< CSeq_data > dest( new CSeq_data );
                CSeqportUtil::Convert(seqdata, dest.get(), CSeq_data::e_Iupacna,
                                      0, seq_length);
                const string & data = dest->GetIupacna().Get();

                // Convert nucleotides to BLASTNA format
                sequence = (Uint1*) malloc(sizeof(Uint1)*seq_length);
                if (sequence == NULL)
                {
                    NcbiCerr << "No space for storing sequence" << endl;
                    exit(0);
                }
                for (i = 0; i < seq_length; i++)
                    switch (data[i]) {
                        case 'a': case 'A': sequence[i] = 0; break;
                        case 'c': case 'C': sequence[i] = 1; break;
                        case 'g': case 'G': sequence[i] = 2; break;
                        case 't': case 'T': sequence[i] = 3; break;
                        case 'r': case 'R': sequence[i] = 4; break;
                        case 'y': case 'Y': sequence[i] = 5; break;
                        case 'm': case 'M': sequence[i] = 6; break;
                        case 'k': case 'K': sequence[i] = 7; break;
                        case 'w': case 'W': sequence[i] = 8; break;
                        case 's': case 'S': sequence[i] = 9; break;
                        case 'b': case 'B': sequence[i] = 10; break;
                        case 'd': case 'D': sequence[i] = 11; break;
                        case 'h': case 'H': sequence[i] = 12; break;
                        case 'v': case 'V': sequence[i] = 13; break;
                        case 'n': case 'N': sequence[i] = 14; break;
                        default: sequence[i] = 14;
                                 NcbiCerr << "Warning: Position " << i+1 ;
                                 NcbiCerr << " of sequence " << seq_id_description;
                                 NcbiCerr << " is and invalid residue; treating it as N" << endl;
                    }

                // Do dusting
                dust_loc = NULL;
                SeqBufferDust(sequence, seq_length, 0, level, window,
                               linker, &dust_loc);

                if (PrintSegments)
                {
                    for (temp_loc = dust_loc; temp_loc; temp_loc = temp_loc->next)
                    {
                        start = (TSeqPos) temp_loc->ssr->left;
                        end = (TSeqPos) temp_loc->ssr->right;
                        output_stream << seq_id_description << "\t";
                        output_stream << start+1 << "\t" << end+1 << endl;
                    }
                }
                else
                {
                    output_stream << defline << endl;

                    // Get copy of (const) data and mask DUST locations found
                    string copy_data = data;
                    for (temp_loc = dust_loc; temp_loc; temp_loc = temp_loc->next)
                    {
                        start = (TSeqPos) temp_loc->ssr->left;
                        end = (TSeqPos) temp_loc->ssr->right;
                        for (; start <= end; start++)
                            copy_data[start] = tolower(copy_data[start]);
                    }

                    // Get lowercase positions
                    if (lcase_mask[0].NotEmpty())
                    {
                        CConstRef<CSeq_loc> lowercase = lcase_mask[0];
                        if (lowercase->IsPacked_int())
                            ITERATE(list< CRef<CSeq_interval> >, itr, 
                                    lowercase->GetPacked_int().Get())
                            {
                                start = (TSeqPos) (*itr)->GetFrom();
                                end = (TSeqPos) (*itr)->GetTo();
                                for (; start < end; start++)
                                    copy_data[start] = tolower(copy_data[start]);
                            }
                    }

                    // Print result
                    start = 0;
                    TSeqPos copy_size = (TSeqPos) copy_data.size();
                    while (start < copy_size)
                    {
                        end = ((start+FASTA_LEN) >= copy_size) ? copy_size : start+FASTA_LEN;
                        output_stream << copy_data.substr(start, end-start) << endl;
                        start = end;
                    }
                }
    
                free(sequence);
                BlastSeqLocFree(dust_loc);
            }
        }
    }

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CDustApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CDustApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
