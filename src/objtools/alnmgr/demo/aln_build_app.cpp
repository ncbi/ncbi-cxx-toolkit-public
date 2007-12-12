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
* Author:  Kamen Todorov
*
* File Description:
*   Demo of alignment building.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <connect/ncbi_core_cxx.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <common/test_assert.h>

#include <serial/iterator.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>

#include <objtools/alnmgr/aln_asn_reader.hpp>
#include <objtools/alnmgr/aln_container.hpp>
#include <objtools/alnmgr/aln_tests.hpp>
#include <objtools/alnmgr/aln_stats.hpp>
#include <objtools/alnmgr/pairwise_aln.hpp>
#include <objtools/alnmgr/aln_converters.hpp>
#include <objtools/alnmgr/aln_generators.hpp>
#include <objtools/alnmgr/sparse_aln.hpp>
#include <objtools/alnmgr/aln_builders.hpp>
#include <objtools/alnmgr/aln_user_options.hpp>
#include <objtools/alnmgr/seqids_extractor.hpp>

using namespace ncbi;
using namespace objects;


class CAlnBuildApp : public CNcbiApplication
{
public:
    virtual void Init         (void);
    virtual int  Run          (void);
    CScope&      GetScope     (void) const;
    void         LoadInputAlns(void);
    bool         InsertAln    (const CSeq_align* aln) {
        m_AlnContainer.insert(*aln);
        aln->Validate(true);
        return true;
    }
    void ReportTime(const string& msg);

private:
    mutable CRef<CObjectManager> m_ObjMgr;
    mutable CRef<CScope>         m_Scope;
    CAlnContainer                m_AlnContainer;
    CStopWatch                   m_StopWatch;
};


void CAlnBuildApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddDefaultKey
        ("in", "input_file_name",
         "Name of file to read from (standard input by default)",
         CArgDescriptions::eInputFile, "-");

    arg_desc->AddDefaultKey
        ("b", "bin_obj_type",
         "This forces the input file to be read in binary ASN.1 mode\n"
         "and specifies the type of the top-level ASN.1 object.\n",
         CArgDescriptions::eString, "");

    arg_desc->AddDefaultKey
        ("noobjmgr", "bool",
        // ObjMgr is used to identify sequences and obtain a bioseqhandle.
        // Also used to calc scores and determine the type of molecule
         "Skip ObjMgr in identifying sequences, calculating scores, etc.",
         CArgDescriptions::eBoolean, "f");

    arg_desc->AddDefaultKey
        ("print", "bool",
         "Print the sequence strings",
         CArgDescriptions::eBoolean, "f");


    arg_desc->AddDefaultKey
        ("asnout", "asn_out_file_name",
         "Text ASN output",
         CArgDescriptions::eOutputFile, "-");


    // Conversion option
    arg_desc->AddDefaultKey
        ("dir", "filter_direction",
         "eBothDirections = 0, ///< No filtering: use both direct and reverse sequences.\n"
         "eDirect         = 1, ///< Use only sequences whose strand is the same as that of the anchor\n"
         "eReverse        = 2  ///< Use only sequences whose strand is opposite to that of the anchor\n",
         CArgDescriptions::eInteger, "0");
    arg_desc->SetConstraint("dir", new CArgAllow_Integers(0,2));


    // Merge option
    arg_desc->AddDefaultKey
        ("algo", "merge_algo",
         "eMergeAllSeqs      = 0, ///< Merge all sequences\n"
         "eQuerySeqMergeOnly = 1, ///< Only put the query seq on same row, \n"
         "ePreserveRows      = 2, ///< Preserve all rows as they were in the input (e.g. self-align a sequence)\n"
         "eDefaultMergeAlgo  = eMergeAllSeqs",
         CArgDescriptions::eInteger, "0");
    arg_desc->SetConstraint("algo", new CArgAllow_Integers(0,2));


    // Program description
    string prog_description = "Alignment build application.\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    SetupArgDescriptions(arg_desc.release());

    // Start the timer
    m_StopWatch.Start();
}


void CAlnBuildApp::LoadInputAlns(void)
{
    const CArgs& args = GetArgs();
    
    /// get the asn type of the top-level object
    string asn_type = args["b"].AsString();
    bool binary = !asn_type.empty();
    auto_ptr<CObjectIStream> in
        (CObjectIStream::Open(binary?eSerial_AsnBinary:eSerial_AsnText,
                              args["in"].AsInputFile()));
    
    CAlnAsnReader reader(&GetScope());
    reader.Read(in.get(),
                bind1st(mem_fun(&CAlnBuildApp::InsertAln), this),
                asn_type);
}


CScope& CAlnBuildApp::GetScope(void) const
{
    if (!m_Scope) {
        m_ObjMgr = CObjectManager::GetInstance();
        CGBDataLoader::RegisterInObjectManager(*m_ObjMgr);
        
        m_Scope = new CScope(*m_ObjMgr);
        m_Scope->AddDefaults();
    }
    return *m_Scope;
}


void CAlnBuildApp::ReportTime(const string& msg)
{
    cerr << "time:\t" 
         << m_StopWatch.AsSmartString(CTimeSpan::eSSP_Millisecond) 
         << "\t" << msg << endl;
    m_StopWatch.Restart();
}


int CAlnBuildApp::Run(void)
{
    // Setup application registry, error log, and MT-lock for CONNECT library
    CONNECT_Init(&GetConfig());
    
    LoadInputAlns();
    ReportTime("LoadInputAlns");
    

    /// Create a vector of seq-ids per seq-align
    TIdExtract id_extract;
    TAlnIdMap aln_id_map(id_extract, m_AlnContainer.size());
    ITERATE(CAlnContainer, aln_it, m_AlnContainer) {
        try {
            aln_id_map.push_back(**aln_it);
        } catch (CAlnException e) {
            cerr << "Skipping this alignment: " << e.what() << endl;;
        }
    }
    ReportTime("TAlnIdMap");


    /// Crete align statistics object
    TAlnStats aln_stats(aln_id_map);
    ReportTime("TAlnStats");
    {
        aln_stats.Dump(cout);
        m_StopWatch.Restart();
    }


    /// Show which seq-ids are aligned to the first one
    {
        const TAlnStats::TIdVec& aligned_ids = aln_stats.GetAlignedIds(aln_stats.GetIdVec()[0]);
        ReportTime("GetAlignedIds");
        cerr << aln_stats.GetIdVec()[0]->AsString()
             << " is aligned to:" << endl;
        ITERATE(TAlnStats::TIdVec, id_it, aligned_ids) {
            cerr << (*id_it)->AsString() << endl;
        }
        cerr << endl;
        m_StopWatch.Restart();
    }
        

    /// Create user options
    CAlnUserOptions aln_user_options;


    /// Optionally, choose to filter a direction
    aln_user_options.m_Direction = 
        (CAlnUserOptions::EDirection) GetArgs()["dir"].AsInteger();


    /// Construct a vector of anchored alignments
    typedef vector<CRef<CAnchoredAln> > TAnchoredAlnVec;
    TAnchoredAlnVec anchored_aln_vec;
    CreateAnchoredAlnVec(aln_stats, anchored_aln_vec, aln_user_options);
    ReportTime("TAnchoredAlnVec");
    {
        ITERATE(TAnchoredAlnVec, aln_vec_it, anchored_aln_vec) {
            (*aln_vec_it)->Dump(cout);
        }
        m_StopWatch.Restart();
    }


    /// Choose merging algorithm
    aln_user_options.m_MergeAlgo = 
        (CAlnUserOptions::EMergeAlgo) GetArgs()["algo"].AsInteger();


    /// Build a single anchored aln
    CAnchoredAln out_anchored_aln;
    BuildAln(anchored_aln_vec,
             out_anchored_aln,
             aln_user_options);
    ReportTime("BuildAln");
    {
        out_anchored_aln.Dump(cout);
        m_StopWatch.Restart();
    }


    /// Get sequence:
    CSparseAln sparse_aln(out_anchored_aln, GetScope());
    ReportTime("CSparseAln");
    if (GetArgs()["print"].AsBoolean()) {
        for (CSparseAln::TDim row = 0;  row < sparse_aln.GetDim();  ++row) {
            string sequence;
            sparse_aln.GetAlnSeqString
                (row, 
                 sequence, 
                 sparse_aln.GetSeqAlnRange(row));
            cout << sparse_aln.GetSeqId(row).AsFastaString() << "\t"
                 << sequence << endl;
        }
        ReportTime("GetAlnSeqString");
    }



    /// Create a Seq-align
    CRef<CSeq_align> sa = 
        CreateSeqAlignFromAnchoredAln(out_anchored_aln,
                                      CSeq_align::TSegs::e_Denseg);
    ReportTime("CreateSeqAlignFromAnchoredAln");
    auto_ptr<CObjectOStream> asn_out
        (CObjectOStream::Open(eSerial_AsnText, 
                              GetArgs()["asnout"].AsString()));
    *asn_out << *sa;


    /// Create individual Dense-segs (one per CPairwiseAln)
    ITERATE(CAnchoredAln::TPairwiseAlnVector,
            pairwise_aln_i, 
            out_anchored_aln.GetPairwiseAlns()) {

        CRef<CDense_seg> ds = 
            CreateDensegFromPairwiseAln(**pairwise_aln_i);
        *asn_out << *ds;
    }


    return 0;
}


int main(int argc, const char* argv[])
{
    return CAlnBuildApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
