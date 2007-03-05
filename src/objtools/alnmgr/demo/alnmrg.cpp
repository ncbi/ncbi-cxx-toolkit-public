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
*   Alignment merger. Demonstration of CAlnMix usage.
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

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
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/submit/Seq_submit.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>

#include <objtools/alnmgr/alnmix.hpp>
#include <objtools/alnmgr/alnvec.hpp>
#include <objtools/alnmgr/aln_asn_reader.hpp>

#include <test/test_assert.h>  /* This header must go last */

USING_SCOPE(ncbi);
USING_SCOPE(objects);

class CAlnMrgApp : public CNcbiApplication
{
    virtual void     Init          (void);
    virtual int      Run           (void);
    CScope&          GetScope      (void)             const;
    void             SetOptions    (void);
    void             LoadInputAlns (void);
    void             PrintMergedAln(void);
    void             ViewMergedAln (void);
    bool             AddAlnToMix   (const CSeq_align* aln) {
        m_Mix->Add(*aln, m_AddFlags);
        return true;
    }

private:
    CAlnMix::TMergeFlags         m_MergeFlags;
    CAlnMix::TAddFlags           m_AddFlags;
    mutable CRef<CObjectManager> m_ObjMgr;
    mutable CRef<CScope>         m_Scope;
    CRef<CAlnMix>                m_Mix; // must appear AFTER m_ObjMgr!
};


class CAlnMrgTaskProgressCallback :
    public CObject,
    public ITaskProgressCallback
{
public:
    virtual void SetTaskName      (const string& name)
    {
        cerr << name << "..." << endl;
    };
    virtual void SetTaskCompleted (int completed)
    {
        cerr << completed << " out of " << m_Total << endl;
    }
    virtual void SetTaskTotal     (int total)
    {
        m_Total = total;
    }
    virtual bool InterruptTask    ()
    {
        return false;
    }
private:
    int m_Total;
};


void CAlnMrgApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "Alignment merger demo program");

    // Describe the expected command-line arguments
    arg_desc->AddDefaultKey
        ("in", "input_file_name",
         "Name of file to read from (standard input by default)",
         CArgDescriptions::eInputFile, "-");

    arg_desc->AddDefaultKey
        ("asnout", "asn_out_file_name",
         "Text ASN output",
         CArgDescriptions::eOutputFile, "-");

    arg_desc->AddOptionalKey
        ("asnoutb", "asn_out_file_name_b",
         "Text ASN output, to a file opened in binary mode (for MS-Win tests)",
         CArgDescriptions::eOutputFile, CArgDescriptions::fBinary);

    arg_desc->AddDefaultKey
        ("b", "bin_obj_type",
         "This forced the input file to be read in binary ASN.1 mode\n"
         "and specifies the type of the top-level ASN.1 object.\n",
         CArgDescriptions::eString, "");

    arg_desc->AddDefaultKey
        ("bout", "bool",
         "This forced the output file to be written in binary ASN.1 mode.\n",
         CArgDescriptions::eBoolean, "f");

    arg_desc->AddOptionalKey
        ("log", "log_file_name",
         "Name of log file to write to",
         CArgDescriptions::eOutputFile, CArgDescriptions::fPreOpen);

    arg_desc->AddDefaultKey
        ("dsout", "bool",
         "Output in Dense-seg format",
         CArgDescriptions::eBoolean, "f");

    arg_desc->AddDefaultKey
        ("gapjoin", "bool",
         "Consolidate segments of equal lens with a gap on the query sequence",
         CArgDescriptions::eBoolean, "f");

    arg_desc->AddDefaultKey
        ("mingap", "bool",
         "Consolidate all segments with a gap on the query sequence",
         CArgDescriptions::eBoolean, "f");

    arg_desc->AddDefaultKey
        ("rmleadtrailgaps", "bool",
         "Remove leading and trailing gaps",
         CArgDescriptions::eBoolean, "f");

    arg_desc->AddDefaultKey
        ("minusstrand", "bool",
         "Minus strand on the refseq when merging.",
         CArgDescriptions::eBoolean, "f");

    arg_desc->AddDefaultKey
        ("fillunaln", "bool",
         "Fill unaligned regions.",
         CArgDescriptions::eBoolean, "f");

    arg_desc->AddDefaultKey
        ("calcscore", "bool",
         "Calculate each aligned seq pair score and use it when merging."
         "(Don't stitch off ObjMgr for this).",
         CArgDescriptions::eBoolean, "f");

    arg_desc->AddDefaultKey
        ("sortseqsbyscore", "bool",
         "Sort sequences by score.",
         CArgDescriptions::eBoolean, "f");

    arg_desc->AddDefaultKey
        ("sortinputbyscore", "bool",
         "Sort input by score.",
         CArgDescriptions::eBoolean, "f");

    arg_desc->AddDefaultKey
        ("noobjmgr", "bool",
        // ObjMgr is used to identify sequences and obtain a bioseqhandle.
        // Also used to calc scores and determine the type of molecule
         "Skip ObjMgr in identifying sequences, calculating scores, etc.",
         CArgDescriptions::eBoolean, "f");

    arg_desc->AddDefaultKey
        ("queryseqmergeonly", "bool",
         "Merge the query seq only, keep subject seqs on separate rows "
         "(even if the same seq).",
         CArgDescriptions::eBoolean, "f");

    arg_desc->AddDefaultKey
        ("truncateoverlaps", "bool",
         "Truncate overlaps",
         CArgDescriptions::eBoolean, "f");

    arg_desc->AddDefaultKey
        ("allowtranslocation", "bool",
         "Allow translocation",
         CArgDescriptions::eBoolean, "f");

    arg_desc->AddDefaultKey
        ("forcetranslation", "bool",
         "Force translation of nucleotides",
         CArgDescriptions::eBoolean, "f");

    arg_desc->AddDefaultKey
        ("preserverows", "bool",
         "Preserve rows",
         CArgDescriptions::eBoolean, "f");


    // Viewing args:
    arg_desc->AddOptionalKey
        ("v", "",
         "View format:\n"
         " 1. CSV table\n"
         " 2. Print segments\n"
         " 3. Print chunks\n"
         " 4. Popset style using GetAlnSeqString\n"
         "    (memory efficient for large alns, but slower)\n"
         " 5. Popset style using GetSeqString\n"
         "   (memory inefficient)\n"
         " 6. Popset style using GetWholeAlnSeqString\n"
         "   (fastest, but memory inefficient)\n",
         CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey
        ("a", "AnchorRow",
         "Anchor row (zero based)",
         CArgDescriptions::eInteger);

    arg_desc->AddDefaultKey
        ("w", "ScreenWidth",
         "Screen width for some of the viewers",
         CArgDescriptions::eInteger, "60");

    arg_desc->AddDefaultKey
        ("cf", "GetChunkFlags",
         "Flags for GetChunks (CAlnMap::TGetChunkFlags)",
         CArgDescriptions::eInteger, "0");

    arg_desc->AddDefaultKey
        ("progress", "bool",
         "Show progress feedback on stderr",
         CArgDescriptions::eBoolean, "f");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


CScope& CAlnMrgApp::GetScope(void) const
{
    if (!m_Scope) {
        m_ObjMgr = CObjectManager::GetInstance();
        CGBDataLoader::RegisterInObjectManager(*m_ObjMgr);
        
        m_Scope = new CScope(*m_ObjMgr);
        m_Scope->AddDefaults();
    }
    return *m_Scope;
}


void CAlnMrgApp::SetOptions(void)
{
    const CArgs& args = GetArgs();

    if ( args["log"] ) {
        SetDiagStream( &args["log"].AsOutputFile() );
    } else {
        SetDiagStream(&NcbiCerr);
    }

    m_MergeFlags = 0;
    m_AddFlags = 0;

    if (args["gapjoin"]  &&  args["gapjoin"].AsBoolean()) {
        m_MergeFlags |= CAlnMix::fGapJoin;
    }

    if (args["mingap"]  &&  args["mingap"].AsBoolean()) {
        m_MergeFlags |= CAlnMix::fMinGap;
    }

    if (args["rmleadtrailgaps"]  &&  args["rmleadtrailgaps"].AsBoolean()) {
        m_MergeFlags |= CAlnMix::fRemoveLeadTrailGaps;
    }

    if (args["minusstrand"]  &&  args["minusstrand"].AsBoolean()) {
        m_MergeFlags |= CAlnMix::fNegativeStrand;
    }

    if (args["queryseqmergeonly"]  &&  args["queryseqmergeonly"].AsBoolean()) {
        m_MergeFlags |= CAlnMix::fQuerySeqMergeOnly;
    }

    if (args["fillunaln"]  &&  args["fillunaln"].AsBoolean()) {
        m_MergeFlags |= CAlnMix::fFillUnalignedRegions;
    }

    if (args["truncateoverlaps"]  &&  args["truncateoverlaps"].AsBoolean()) {
        m_MergeFlags |= CAlnMix::fTruncateOverlaps;
    }

    if (args["allowtranslocation"]  &&  args["allowtranslocation"].AsBoolean()) {
        m_MergeFlags |= CAlnMix::fAllowTranslocation;
    }

    if (args["forcetranslation"]  &&  args["forcetranslation"].AsBoolean()) {
        m_AddFlags |= CAlnMix::fForceTranslation;
    }

    if (args["preserverows"]  &&  args["preserverows"].AsBoolean()) {
        m_AddFlags |= CAlnMix::fPreserveRows;
    }

    if (args["calcscore"]  &&  args["calcscore"].AsBoolean()) {
        m_AddFlags |= CAlnMix::fCalcScore;
    }

    if (args["sortseqsbyscore"]  &&  args["sortseqsbyscore"].AsBoolean()) {
        m_MergeFlags |= CAlnMix::fSortSeqsByScore;
    }

    if (args["sortinputbyscore"]  &&  args["sortinputbyscore"].AsBoolean()) {
        m_MergeFlags |= CAlnMix::fSortInputByScore;
    }

    if ( !(args["noobjmgr"]  &&  args["noobjmgr"].AsBoolean()) ) {
        GetScope(); // first call creates the scope
    }
}

void CAlnMrgApp::PrintMergedAln(void)
{
    const CArgs& args = GetArgs();
    auto_ptr<CObjectOStream> asn_out 
        (CObjectOStream::Open
         (args["bout"] && args["bout"].AsBoolean() ? 
          eSerial_AsnBinary : eSerial_AsnText,
          args["asnoutb"] ?
          args["asnoutb"].AsOutputFile() : args["asnout"].AsOutputFile()));

    if (args["dsout"]  &&  args["dsout"].AsBoolean()) {
        *asn_out << m_Mix->GetDenseg();
    } else {
        *asn_out << m_Mix->GetSeqAlign();
    }
}


void CAlnMrgApp::ViewMergedAln(void)
{
    const CArgs& args = GetArgs();

    int screen_width = args["w"].AsInteger();

    CAlnVec aln_vec(m_Mix->GetDenseg(), GetScope());
    aln_vec.SetGapChar('-');
    aln_vec.SetEndChar('.');
    if (args["a"]) {
        aln_vec.SetAnchor(args["a"].AsInteger());
    }

    if (args["v"]) {
        switch (args["v"].AsInteger()) {
        case 1: 
            CAlnMapPrinter(aln_vec, NcbiCout).CsvTable();
            break;
        case 2:
            CAlnMapPrinter(aln_vec, NcbiCout).Segments();
            break;
        case 3:
            CAlnMapPrinter(aln_vec, NcbiCout).Chunks(args["cf"].AsInteger());
            break;
        case 4:
            CAlnVecPrinter(aln_vec, NcbiCout)
                .PopsetStyle(screen_width, CAlnVecPrinter::eUseAlnSeqString);
            break;
        case 5: 
            CAlnVecPrinter(aln_vec, NcbiCout)
                .PopsetStyle(screen_width, CAlnVecPrinter::eUseSeqString);
            break;
        case 6:
            CAlnVecPrinter(aln_vec, NcbiCout)
                .PopsetStyle(screen_width,
                             CAlnVecPrinter::eUseWholeAlnSeqString);
            break;
        default:
            NcbiCout << "Unknown view format." << NcbiEndl;
        }
    }
}


void CAlnMrgApp::LoadInputAlns(void)
{
    const CArgs& args = GetArgs();
    string sname = args["in"].AsString();
    
    // get the asn type of the top-level object
    string asn_type = args["b"].AsString();
    bool binary = !asn_type.empty();
    auto_ptr<CObjectIStream> in
        (CObjectIStream::Open(binary?eSerial_AsnBinary:eSerial_AsnText, sname));
    
    CAlnAsnReader reader(&GetScope());
    reader.Read(in.get(),
                bind1st(mem_fun(&CAlnMrgApp::AddAlnToMix), this),
                asn_type);
}


int CAlnMrgApp::Run(void)
{
    _TRACE("Run()");
    const CArgs& args = GetArgs();

    SetOptions();

    m_Mix = m_Scope ? new CAlnMix(GetScope()) : new CAlnMix();
    CRef<CAlnMrgTaskProgressCallback> progress_callback;
    if (args["progress"]  &&  args["progress"].AsBoolean()) {
        progress_callback.Reset(new CAlnMrgTaskProgressCallback);
    }
    m_Mix->SetTaskProgressCallback(progress_callback.GetPointerOrNull());
    LoadInputAlns();

    m_Mix->Merge(m_MergeFlags);

    PrintMergedAln();
    if ( args["v"] ) {
        ViewMergedAln();
    }
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CAlnMrgApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
