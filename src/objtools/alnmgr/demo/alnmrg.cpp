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

#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>

#include <objtools/alnmgr/alnmix.hpp>

USING_SCOPE(ncbi);
USING_SCOPE(objects);

class CAlnMrgApp : public CNcbiApplication
{
    virtual void     Init                (void);
    virtual int      Run                 (void);
    void             SetOptions          (void);
    void             LoadInputAlignments (void);
    void             PrintMergedAlignment(void);
    void             View4               (int screen_width);

private:
    CRef<CAlnMix>        m_Mix;
    CAlnMix::TMergeFlags m_MergeFlags;
    CAlnMix::TAddFlags   m_AddFlags;
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
         CArgDescriptions::eInputFile, "-", CArgDescriptions::fPreOpen);

    arg_desc->AddDefaultKey
        ("log", "log_file_name",
         "Name of log file to write to",
         CArgDescriptions::eOutputFile, "-", CArgDescriptions::fPreOpen);

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

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


void CAlnMrgApp::LoadInputAlignments(void)
{
    CArgs args = GetArgs();

    CNcbiIstream& is = args["in"].AsInputFile();
    bool done = false;
    
    string asn_type;
    {{
        auto_ptr<CObjectIStream> in
            (CObjectIStream::Open(eSerial_AsnText, is));

        asn_type = in->ReadFileHeader();
        is.seekg(0);
    }}

    auto_ptr<CObjectIStream> in
        (CObjectIStream::Open(eSerial_AsnText, is));
    
    CTypesIterator i;
    CType<CSeq_align>::AddTo(i);

    if (asn_type == "Seq-entry") {
        CRef<CSeq_entry> se(new CSeq_entry);
        *in >> *se;
        m_Mix->GetScope().AddTopLevelSeqEntry(*se);
        for (i = Begin(*se); i; ++i) {
            if (CType<CSeq_align>::Match(i)) {
                m_Mix->Add(*(CType<CSeq_align>::Get(i)), m_AddFlags);
            }
        }
    } else if (asn_type == "Seq-submit") {
        CRef<CSeq_submit> ss(new CSeq_submit);
        *in >> *ss;
        CType<CSeq_entry>::AddTo(i);
        int tse_cnt = 0;
        for (i = Begin(*ss); i; ++i) {
            if (CType<CSeq_align>::Match(i)) {
                m_Mix->Add(*(CType<CSeq_align>::Get(i)), m_AddFlags);
            } else if (CType<CSeq_entry>::Match(i)) {
                if ( !(tse_cnt++) ) {
                    //m_Mix->GetScope().AddTopLevelSeqEntry
                        (*(CType<CSeq_entry>::Get(i)));
                }
            }
        }
    } else if (asn_type == "Seq-align") {
        CRef<CSeq_align> sa(new CSeq_align);
        *in >> *sa;
        for (i = Begin(*sa); i; ++i) {
            if (CType<CSeq_align>::Match(i)) {
                m_Mix->Add(*(CType<CSeq_align>::Get(i)), m_AddFlags);
            }
        }
    } else if (asn_type == "Seq-align-set") {
        CRef<CSeq_align_set> sas(new CSeq_align_set);
        *in >> *sas;
        for (i = Begin(*sas); i; ++i) {
            if (CType<CSeq_align>::Match(i)) {
                m_Mix->Add(*(CType<CSeq_align>::Get(i)), m_AddFlags);
            }
        }
    } else if (asn_type == "Seq-annot") {
        CRef<CSeq_annot> san(new CSeq_annot);
        *in >> *san;
        for (i = Begin(*san); i; ++i) {
            if (CType<CSeq_align>::Match(i)) {
                m_Mix->Add(*(CType<CSeq_align>::Get(i)), m_AddFlags);
            }
        }
    } else if (asn_type == "Dense-seg") {
        CRef<CDense_seg> ds(new CDense_seg);
        *in >> *ds;
        m_Mix->Add(*ds, m_AddFlags);
    } else {
        cerr << "Cannot read: " << asn_type;
        return;
    }
}


void CAlnMrgApp::SetOptions(void)
{
    CArgs args = GetArgs();

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

    if (args["calcscore"]  &&  args["calcscore"].AsBoolean()) {
        m_AddFlags |= CAlnMix::fCalcScore;
    }

    if (args["noobjmgr"]  &&  args["noobjmgr"].AsBoolean()) {
        m_AddFlags |= CAlnMix::fDontUseObjMgr;
    }

}

void CAlnMrgApp::PrintMergedAlignment(void)
{
    auto_ptr<CObjectOStream> asn_out 
        (CObjectOStream::Open(eSerial_AsnText, cout));

    *asn_out << m_Mix->GetDenseg();
}


void CAlnMrgApp::View4(int scrn_width)
{
    CAlnVec av(m_Mix->GetDenseg());
    CAlnMap::TNumrow row, nrows = av.GetNumRows();

    vector<string> buffer(nrows);
    vector<CAlnMap::TSeqPosList> insert_aln_starts(nrows);
    vector<CAlnMap::TSeqPosList> insert_starts(nrows);
    vector<CAlnMap::TSeqPosList> insert_lens(nrows);
    vector<CAlnMap::TSeqPosList> scrn_lefts(nrows);
    vector<CAlnMap::TSeqPosList> scrn_rights(nrows);
    
    // Fill in the vectors for each row
    for (row = 0; row < nrows; row++) {
        av.GetWholeAlnSeqString
            (row,
             buffer[row],
             &insert_aln_starts[row],
             &insert_starts[row],
             &insert_lens[row],
             scrn_width,
             &scrn_lefts[row],
             &scrn_rights[row]);
    }
        
    // Visualization
    TSeqPos pos = 0, aln_len = av.GetAlnStop() + 1;
    do {
        for (row = 0; row < nrows; row++) {
            cout << av.GetSeqId(row)
                 << "\t" 
                 << scrn_lefts[row].front()
                 << "\t"
                 << buffer[row].substr(pos, scrn_width)
                 << "\t"
                 << scrn_rights[row].front()
                 << endl;
            scrn_lefts[row].pop_front();
            scrn_rights[row].pop_front();
        }
        cout << endl;
        pos += scrn_width;
        if (pos + scrn_width > aln_len) {
            scrn_width = aln_len - pos;
        }
    } while (pos < aln_len);
}


int CAlnMrgApp::Run(void)
{
    SetOptions();

    m_Mix = new CAlnMix();
    LoadInputAlignments();
    m_Mix->Merge(m_MergeFlags);
    PrintMergedAlignment();
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
