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
#include <objtools/alnmgr/alnvwr.hpp>

USING_SCOPE(ncbi);
USING_SCOPE(objects);

class CAlnMrgApp : public CNcbiApplication
{
    virtual void     Init                (void);
    virtual int      Run                 (void);
    CScope&          GetScope            (void)             const;
    void             SetOptions          (void);
    void             LoadInputAlignments (void);
    void             PrintMergedAlignment(void);
    void             ViewMergedAlignment (void);

private:
    CAlnMix::TMergeFlags         m_MergeFlags;
    CAlnMix::TAddFlags           m_AddFlags;
    mutable CRef<CObjectManager> m_ObjMgr;
    mutable CRef<CScope>         m_Scope;
    CRef<CAlnMix>                m_Mix; // must appear AFTER m_ObjMgr!
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

    arg_desc->AddOptionalKey
        ("log", "log_file_name",
         "Name of log file to write to",
         CArgDescriptions::eOutputFile, CArgDescriptions::fPreOpen);

    arg_desc->AddDefaultKey
        ("dsout", "bool",
         "Output in Dense-seg format",
         CArgDescriptions::eBoolean, "f");

    arg_desc->AddDefaultKey
        ("gen2est", "bool",
         "Perform Gen2EST Merge",
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


void CAlnMrgApp::LoadInputAlignments(void)
{
    const CArgs& args = GetArgs();

    string sname = args["in"].AsString();
    
    // get the asn type of the top-level object
    string asn_type = args["b"].AsString();
    bool binary = asn_type.length();
    auto_ptr<CObjectIStream> in
        (CObjectIStream::Open(binary?eSerial_AsnBinary:eSerial_AsnText, sname));
    if ( !binary ) {
        // auto-detection is possible in ASN.1 text mode
        asn_type = in->ReadFileHeader();
    }
    
    CTypesIterator i;
    CType<CSeq_align>::AddTo(i);

    if (asn_type == "Seq-entry") {
        CRef<CSeq_entry> se(new CSeq_entry);
        in->Read(Begin(*se), CObjectIStream::eNoFileHeader);
        if (m_Scope) {
            GetScope().AddTopLevelSeqEntry(*se);
        }
        for (i = Begin(*se); i; ++i) {
            if (CType<CSeq_align>::Match(i)) {
                m_Mix->Add(*(CType<CSeq_align>::Get(i)), m_AddFlags);
            }
        }
    } else if (asn_type == "Seq-submit") {
        CRef<CSeq_submit> ss(new CSeq_submit);
        in->Read(Begin(*ss), CObjectIStream::eNoFileHeader);
        CType<CSeq_entry>::AddTo(i);
        int tse_cnt = 0;
        for (i = Begin(*ss); i; ++i) {
            if (CType<CSeq_align>::Match(i)) {
                m_Mix->Add(*(CType<CSeq_align>::Get(i)), m_AddFlags);
            } else if (CType<CSeq_entry>::Match(i)) {
                if ( !(tse_cnt++) ) {
                    //GetScope().AddTopLevelSeqEntry
                        (*(CType<CSeq_entry>::Get(i)));
                }
            }
        }
    } else if (asn_type == "Seq-align") {
        CRef<CSeq_align> sa(new CSeq_align);
        in->Read(Begin(*sa), CObjectIStream::eNoFileHeader);
        for (i = Begin(*sa); i; ++i) {
            if (CType<CSeq_align>::Match(i)) {
                m_Mix->Add(*(CType<CSeq_align>::Get(i)), m_AddFlags);
            }
        }
    } else if (asn_type == "Seq-align-set") {
        CRef<CSeq_align_set> sas(new CSeq_align_set);
        in->Read(Begin(*sas), CObjectIStream::eNoFileHeader);
        for (i = Begin(*sas); i; ++i) {
            if (CType<CSeq_align>::Match(i)) {
                m_Mix->Add(*(CType<CSeq_align>::Get(i)), m_AddFlags);
            }
        }
    } else if (asn_type == "Seq-annot") {
        CRef<CSeq_annot> san(new CSeq_annot);
        in->Read(Begin(*san), CObjectIStream::eNoFileHeader);
        for (i = Begin(*san); i; ++i) {
            if (CType<CSeq_align>::Match(i)) {
                m_Mix->Add(*(CType<CSeq_align>::Get(i)), m_AddFlags);
            }
        }
    } else if (asn_type == "Dense-seg") {
        CRef<CDense_seg> ds(new CDense_seg);
        in->Read(Begin(*ds), CObjectIStream::eNoFileHeader);
        m_Mix->Add(*ds, m_AddFlags);
        while (true) {
            try {
                CRef<CDense_seg> ds(new CDense_seg);
                *in >> *ds;
                m_Mix->Add(*ds, m_AddFlags);
            } catch(...) {
                break;
            }
        }
    } else {
        cerr << "Cannot read: " << asn_type;
        return;
    }
}


void CAlnMrgApp::SetOptions(void)
{
    const CArgs& args = GetArgs();

    if ( args["log"] ) {
        SetDiagStream( &args["log"].AsOutputFile() );
    }

    m_MergeFlags = 0;
    m_AddFlags = 0;

    if (args["gapjoin"]  &&  args["gapjoin"].AsBoolean()) {
        m_MergeFlags |= CAlnMix::fGapJoin;
    }

    if (args["mingap"]  &&  args["mingap"].AsBoolean()) {
        m_MergeFlags |= CAlnMix::fMinGap;
    }

    if (args["gen2est"]  &&  args["gen2est"].AsBoolean()) {
        m_MergeFlags |= CAlnMix::fGen2EST | CAlnMix::fTruncateOverlaps;
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

    if ( !(args["noobjmgr"]  &&  args["noobjmgr"].AsBoolean()) ) {
        GetScope(); // first call creates the scope
    }
}

void CAlnMrgApp::PrintMergedAlignment(void)
{
    const CArgs& args = GetArgs();
    auto_ptr<CObjectOStream> asn_out 
        (CObjectOStream::Open
         (eSerial_AsnText,
          args["asnoutb"] ?
          args["asnoutb"].AsOutputFile() : args["asnout"].AsOutputFile()));

    if (args["dsout"]  &&  args["dsout"].AsBoolean()) {
        *asn_out << m_Mix->GetDenseg();
    } else {
        *asn_out << m_Mix->GetSeqAlign();
    }
}


void CAlnMrgApp::ViewMergedAlignment(void)
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
            CAlnVwr::CsvTable(aln_vec, NcbiCout);
            break;
        case 2:
            CAlnVwr::Segments(aln_vec, NcbiCout);
            break;
        case 3:
            CAlnVwr::Chunks(aln_vec, NcbiCout, args["cf"].AsInteger());
            break;
        case 4:
            CAlnVwr::PopsetStyle(aln_vec, NcbiCout, screen_width,
                                 CAlnVwr::eUseAlnSeqString);
            break;
        case 5: 
            CAlnVwr::PopsetStyle(aln_vec, NcbiCout, screen_width,
                                 CAlnVwr::eUseSeqString);
            break;
        case 6:
            CAlnVwr::PopsetStyle(aln_vec, NcbiCout, screen_width,
                                 CAlnVwr::eUseWholeAlnSeqString);
            break;
        default:
            NcbiCout << "Unknown view format." << NcbiEndl;
        }
    }
}


int CAlnMrgApp::Run(void)
{
    SetOptions();

    m_Mix = m_Scope ? new CAlnMix(GetScope()) : new CAlnMix();
    LoadInputAlignments();
    m_Mix->Merge(m_MergeFlags);

    const CArgs& args = GetArgs();

    PrintMergedAlignment();
    if ( args["v"] ) {
        ViewMergedAlignment();
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


/*
* ===========================================================================
*
* $Log$
* Revision 1.31  2005/03/01 17:19:27  todorov
* 1) Added a sortseqbyscore flag
* 2) Extended to reading multiple dense-segs.
*
* Revision 1.30  2004/10/19 18:54:16  todorov
* Default output now is Seq-align, Dense-seg can be chosen by the dsout param.
*
* Revision 1.29  2004/10/18 15:07:53  todorov
* Do not construct CAlnMix with a scope if noobjmgr was requested.
*
* Revision 1.28  2004/09/28 00:53:29  vakatov
* Added -asnoutb key to be able to use it in the place of -asnout when we
* want to open output file in "binary" mode. This is to work around our
* Win-specific testsuite glitch -- it checks out the test data files on UNIX,
* so with UNIX EOLs.
*
* Revision 1.27  2004/09/22 21:21:42  todorov
* - CArgDescriptions::fPreOpen
*
* Revision 1.26  2004/09/22 19:11:24  todorov
* open in file depending on the mode
*
* Revision 1.25  2004/09/22 17:00:53  todorov
* +CAlnMix::fPreserveRows
*
* Revision 1.24  2004/09/20 15:51:17  todorov
* Made log an optional parameter
*
* Revision 1.23  2004/09/16 19:04:55  todorov
* rm binout
*
* Revision 1.22  2004/09/16 18:44:46  todorov
* +Viewers, mostly to be able to view translated Dense_segs (DS + m_Widths)
*
* Revision 1.21  2004/07/21 15:51:26  grichenk
* CObjectManager made singleton, GetInstance() added.
* CXXXXDataLoader constructors made private, added
* static RegisterInObjectManager() and GetLoaderNameFromArgs()
* methods.
*
* Revision 1.20  2004/05/21 21:42:51  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.19  2004/01/07 17:37:35  vasilche
* Fixed include path to genbank loader.
* Moved split_cache application.
*
* Revision 1.18  2003/12/22 20:28:11  ucko
* Added missing call to GetScope.
*
* Revision 1.17  2003/12/22 18:33:48  ucko
* Simplify format autodetection behavior by means of Read(..., eNoFileHeader);
* fixes problems observed with GCC 2.95.
*
* Revision 1.16  2003/12/20 03:39:12  ucko
* Reorder data members of CAlnMrgApp: m_Mix should follow m_ObjMgr so
* it's destroyed first by default, so that the scope will no longer be
* live by the time m_ObjMgr is destroyed.
*
* Revision 1.15  2003/12/19 19:38:34  todorov
* Demon creation of scope outside alnmix
*
* Revision 1.14  2003/12/08 21:28:04  todorov
* Forced Translation of Nucleotide Sequences
*
* Revision 1.13  2003/11/20 23:58:07  todorov
* Added CObjectIStream::Close before IStream::seekg
*
* Revision 1.12  2003/09/08 20:41:42  todorov
* binint fixed. binout added
*
* Revision 1.11  2003/09/08 19:33:12  todorov
* - unused var
*
* Revision 1.10  2003/09/08 17:05:57  todorov
* ability to read binary files
*
* Revision 1.9  2003/09/04 19:05:46  todorov
* Removed view. Should be optional
*
* Revision 1.8  2003/08/20 14:43:01  todorov
* Support for NA2AA Densegs
*
* Revision 1.7  2003/07/24 16:26:09  ucko
* Undouble CAlnMix:: prefix in one place.
*
* Revision 1.6  2003/07/23 16:14:18  todorov
* +trunacteoverlaps
*
* Revision 1.5  2003/06/26 22:00:25  todorov
* + fFillUnalignedRegions
*
* Revision 1.4  2003/06/24 19:23:37  todorov
* objmgr usage optional
*
* Revision 1.3  2003/06/02 16:06:41  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.2  2003/05/15 19:09:18  todorov
* Optional mixing of the query sequence only
*
* Revision 1.1  2003/04/03 21:17:48  todorov
* Adding demo projects
*
*
* ===========================================================================
*/
