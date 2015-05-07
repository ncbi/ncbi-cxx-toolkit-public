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
* Author:  Kamen Todorov, NCBI
*
* File Description:
*   Sample alignment manager application
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

#include <connect/ncbi_core_cxx.hpp>

#include <serial/iterator.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>

#include <objtools/alnmgr/alnmix.hpp>
#include <objtools/alnmgr/alnvec.hpp>

USING_SCOPE(ncbi);
USING_SCOPE(objects);

class CSampleAlnmgrApplication : public CNcbiApplication
{
    virtual void Init(void);
    virtual int  Run(void);
};

void CSampleAlnmgrApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "Alignment manager demo program: Seq-align extractor/viewer");

    // Describe the expected command-line arguments
    arg_desc->AddDefaultKey
        ("in", "InputFile",
         "name of file to read from (standard input by default)",
         CArgDescriptions::eInputFile, "-", CArgDescriptions::fPreOpen);

    arg_desc->AddDefaultKey
        ("out", "OutputFile",
         "name of file to write to (standard output by default)",
         CArgDescriptions::eOutputFile, "-", CArgDescriptions::fPreOpen);

    arg_desc->AddDefaultKey
        ("asnout", "OutputFile",
         "name of asn file to write to (standard output by default)",
         CArgDescriptions::eOutputFile, "-", CArgDescriptions::fPreOpen);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CSampleAlnmgrApplication::Run(void)
{
    // Setup application registry, error log, and MT-lock for CONNECT library
    CONNECT_Init(&GetConfig());

    // Get arguments
    const CArgs& args = GetArgs();
    CSeq_align in_sa;
    CSeq_entry in_se;

    // Read the file
    {{
        auto_ptr<CObjectIStream> in
            (CObjectIStream::Open(eSerial_AsnText,
                                   args["in"].AsInputFile()));
        *in >> in_se;
    }}

    auto_ptr<CObjectOStream> asn_out 
        (CObjectOStream::Open(eSerial_AsnText, args["asnout"].AsOutputFile()));
    asn_out->SetSeparator("\n");

    CNcbiOstream& out = args["out"].AsOutputFile();

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*objmgr);
    CScope scope(*objmgr);
    scope.AddDefaults();

    CAlnMix mix(scope);
    for (CTypeIterator<CDense_seg> sa_it = Begin(in_se); sa_it; ++sa_it) {
        mix.Add(*sa_it);
    }
    mix.Merge(CAlnMix::fNegativeStrand | CAlnMix::fTruncateOverlaps);
    *asn_out << mix.GetDenseg();
    *asn_out << Separator;
    asn_out->Flush();

    CAlnVec av(mix.GetDenseg(), scope);

    av.SetAnchor(0);

    CAlnMap::TSignedRange range(5, 12);

    CRef<CAlnMap::CAlnChunkVec> chunk_vec = av.GetAlnChunks(1, range);
    
    for (int i = 0;  i < chunk_vec->size();  i++) {
        CConstRef<CAlnMap::CAlnChunk> chunk = (*chunk_vec)[i];
        if (!chunk->IsGap()) {
            out << chunk->GetRange().GetFrom() << "-"
                << chunk->GetRange().GetTo();
        } else {
            out << "gap";
        }

        if (chunk->GetType() & CAlnMap::fSeq) {
            cout << "(Seq)";
        }
        if (chunk->GetType() & CAlnMap::fNotAlignedToSeqOnAnchor) {
            cout << "(NotAlignedToSeqOnAnchor)";
        }
        if (chunk->GetType() & CAlnMap::fUnalignedOnRight) {
            cout << "(UnalignedOnRight)";
        }
        if (chunk->GetType() & CAlnMap::fUnalignedOnLeft) {
            cout << "(UnalignedOnLeft)";
        }
        if (chunk->GetType() & CAlnMap::fNoSeqOnRight) {
            cout << "(NoSeqOnRight)";
        }
        if (chunk->GetType() & CAlnMap::fNoSeqOnLeft) {
            cout << "(NoSeqOnLeft)";
        }
        if (chunk->GetType() & CAlnMap::fEndOnRight) {
            cout << "(EndOnRight)";
        }
        if (chunk->GetType() & CAlnMap::fEndOnLeft) {
            cout << "(EndOnLeft)";
        }
        cout << NcbiEndl;
    }

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CSampleAlnmgrApplication().AppMain(argc, argv);
}
