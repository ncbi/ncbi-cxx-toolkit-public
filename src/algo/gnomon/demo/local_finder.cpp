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
 * Authors:  Alexandre Souvorov
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <algo/gnomon/gnomon.hpp>
#include <serial/serial.hpp>
#include <serial/objostr.hpp>
#include <objtools/readers/fasta.hpp>
#include <objmgr/object_manager.hpp>

USING_SCOPE(ncbi);
USING_SCOPE(ncbi::objects);
USING_SCOPE(ncbi::gnomon);

class CLocalFinderApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};



void CLocalFinderApp::Init(void)
{
    // Prepare command line descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddKey("input", "FastaFile",
                     "File containing FASTA-format sequence",
                     CArgDescriptions::eInputFile);

    arg_desc->AddDefaultKey("from", "From",
                            "From",
                            CArgDescriptions::eInteger,
                            "0");

    arg_desc->AddDefaultKey("to", "To",
                            "To",
                            CArgDescriptions::eInteger,
                            "1000000000");

    arg_desc->AddKey("model", "ModelData",
                     "Model Data",
                     CArgDescriptions::eInputFile);

    arg_desc->AddOptionalKey("align", "Alignments",
                            "Alignments",
                            CArgDescriptions::eInputFile);

    arg_desc->AddFlag("rep", "Repeats");


    // Pass argument descriptions to the application
    //

    SetupArgDescriptions(arg_desc.release());
}


int CLocalFinderApp::Run(void)
{
    CArgs myargs = GetArgs();

    int left            = myargs["from"].AsInteger();
    int right           = myargs["to"].AsInteger();
    bool repeats        = myargs["rep"];


    //
    // read our sequence data
    //
    CRef<CSeq_entry> se = ReadFasta(myargs["input"].AsInputFile(),fReadFasta_AssumeNuc|fReadFasta_ForceType|fReadFasta_OneSeq|fReadFasta_RequireID);

    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CScope scope(*objmgr);
    scope.AddTopLevelSeqEntry(*se);       

    CRef<CSeq_id> cntg(new CSeq_id);
    cntg->Assign(*se->GetSeq().GetFirstId());
    CSeq_loc loc;
    loc.SetWhole(*cntg);
    CSeqVector vec(loc, scope);
    vec.SetIupacCoding();

    CResidueVec seq;
    ITERATE(CSeqVector,i,vec)
        seq.push_back(*i);

    // read the alignment information
    TAlignList alignments;
    if(myargs["align"]) {
        CNcbiIstream& alignmentfile = myargs["align"].AsInputFile();
        string our_contig = cntg->GetSeqIdString(true);
        string cur_contig; 
        CGeneModel algn;
        
        while(alignmentfile >> algn >> getcontig(cur_contig)) {
            if (cur_contig==our_contig)
                alignments.push_back(algn);
        }
    }

    // create engine
    CRef<CHMMParameters> hmm_params(new CHMMParameters(myargs["model"].AsInputFile()));
    CGnomonEngine gnomon(hmm_params, seq, TSignedSeqRange(left, right));

    // run!
    gnomon.Run(alignments, repeats, true, true, false, false, 10.0);

    // dump the annotation
    CRef<CSeq_annot> annot = gnomon.GetAnnot(*cntg);
    auto_ptr<CObjectOStream> os(CObjectOStream::Open(eSerial_AsnText, cout));
    *os << *annot;

    return 0;

}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    return CLocalFinderApp().AppMain(argc, argv, 0, eDS_Default, 0);
}

