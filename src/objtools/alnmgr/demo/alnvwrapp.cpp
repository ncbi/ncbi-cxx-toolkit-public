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
*   Various alignment viewers. Demonstration of CAlnMap/CAlnVec usage.
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

#include <objtools/alnmgr/alnvec.hpp>
#include <objtools/alnmgr/alnpos_ci.hpp>


USING_SCOPE(ncbi);
USING_SCOPE(objects);

void LogTime(const string& s)
{

    static time_t prev_t;
    time_t        t = time(0);

    if (prev_t==0) {
        prev_t=t;
    }
    
    NcbiCout << s << " " << (int)(t-prev_t) << NcbiEndl;
}

class CAlnVwrApp : public CNcbiApplication
{
    virtual void     Init(void);
    virtual int      Run(void);
    void             LoadDenseg(void);
    void             View7();
    void             View8(int aln_pos);
    void             View9(int row0, int row1);
    void             View10(int row0, int row1);
    void             GetSeqPosFromAlnPosDemo();
    void             GetAlnPosFromSeqPosDemo();
private:
    CRef<CObjectManager> m_ObjMgr;
    CRef<CScope>  m_Scope;
    CRef<CAlnVec> m_AV;
};

void CAlnVwrApp::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "Alignment manager demo program");

    // Describe the expected command-line arguments
    arg_desc->AddDefaultKey
        ("in", "InputFile",
         "Name of file to read the Dense-seg from (standard input by default)",
         CArgDescriptions::eInputFile, "-", CArgDescriptions::fPreOpen);

    arg_desc->AddOptionalKey
        ("se_in", "SeqEntryInputFile",
         "An optional Seq-entry file to load a local top level seq entry from.",
         CArgDescriptions::eInputFile, CArgDescriptions::fPreOpen);

    arg_desc->AddOptionalKey
        ("log", "LogFile",
         "Name of log file to write to",
         CArgDescriptions::eOutputFile, CArgDescriptions::fPreOpen);

    arg_desc->AddOptionalKey
        ("a", "AnchorRow",
         "Anchor row (zero based)",
         CArgDescriptions::eInteger);

    arg_desc->AddKey
        ("v", "",
         "View format:\n"
         "1. CSV table\n"
         "2. Popset style using GetAlnSeqString\n"
         "   (memory efficient for large alns, but slower)\n"
         "3. Popset style using GetSeqString\n"
         "   (memory inefficient)\n"
         "4. Popset style using GetWholeAlnSeqString\n"
         "   (fastest, but memory inefficient)\n"
         "5. Print segments\n"
         "6. Print chunks\n"
         "7. Alternative ways to get sequence\n"
         "8. Demonstrate obtaining column vector in two alternative ways.\n"
         "   (Use numeric param n to choose alignment position)\n"
         "9. Print relative residue index mapping for two rows.\n"
         "   (Use row0 and row1 params to choose the rows)\n"
         "10. Iterate through alignment positions and show corresponding\n"
         "    native sequence positions for two chosen rows\n"
         "    (Use row0 and row1 params to choose the rows)\n"
         "11. Clustal style\n",
         CArgDescriptions::eInteger);

    arg_desc->AddDefaultKey
        ("w", "ScreenWidth",
         "Screen width for some of the viewers",
         CArgDescriptions::eInteger, "60");

    arg_desc->AddDefaultKey
        ("n", "Number",
         "Generic Numeric Parameter, used by some viewers",
         CArgDescriptions::eInteger, "0");

    arg_desc->AddDefaultKey
        ("row0", "Row0",
         "Generic Row Parameter, used by some viewers",
         CArgDescriptions::eInteger, "0");

    arg_desc->AddDefaultKey
        ("row1", "Row1",
         "Generic Row Parameter, used by some viewers",
         CArgDescriptions::eInteger, "1");

    arg_desc->AddDefaultKey
        ("cf", "GetChunkFlags",
         "Flags for GetChunks (CAlnMap::TGetChunkFlags)",
         CArgDescriptions::eInteger, "0");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


void CAlnVwrApp::LoadDenseg(void)
{
    CArgs args = GetArgs();

    CNcbiIstream& is = args["in"].AsInputFile();
    
    string asn_type;
    {{
        auto_ptr<CObjectIStream> in
            (CObjectIStream::Open(eSerial_AsnText, is));

        asn_type = in->ReadFileHeader();
        in->Close();
        is.seekg(0);
    }}

    auto_ptr<CObjectIStream> in
        (CObjectIStream::Open(eSerial_AsnText, is));
    
    //create scope
    {{
        m_ObjMgr = CObjectManager::GetInstance();
        CGBDataLoader::RegisterInObjectManager(*m_ObjMgr);

        m_Scope = new CScope(*m_ObjMgr);
        m_Scope->AddDefaults();
    }}

    if (asn_type == "Dense-seg") {
        CRef<CDense_seg> ds(new CDense_seg);
        *in >> *ds;
        ds->Validate();
        m_AV = new CAlnVec(*ds, *m_Scope);
    } else if (asn_type == "Seq-align") {
        CRef<CSeq_align> sa(new CSeq_align);
        *in >> *sa;
        if ( !sa->GetSegs().IsDenseg() ) {
            NCBI_THROW(CAlnException, eMergeFailure,
                       "CAlnMix::LoadDenseg(): "
                       "Seq-align segs must be of type Dense-seg.");
        } 
        const CDense_seg& ds = sa->GetSegs().GetDenseg();
        ds.Validate();
        m_AV = new CAlnVec(ds, *m_Scope);
    } else if (asn_type == "Seq-submit") {
        CRef<CSeq_submit> ss(new CSeq_submit);
        *in >> *ss;
        CTypesIterator i;
        CType<CDense_seg>::AddTo(i);
        CType<CSeq_entry>::AddTo(i);
        int tse_cnt = 0;
        for (i = Begin(*ss); i; ++i) {
            if (CType<CDense_seg>::Match(i)) {
                m_AV = new CAlnVec(*(CType<CDense_seg>::Get(i)), *m_Scope);
            } else if (CType<CSeq_entry>::Match(i)) {
                if ( !(tse_cnt++) ) {
                    m_Scope->AddTopLevelSeqEntry
                        (*(CType<CSeq_entry>::Get(i)));
                }
            }
        }
    } else {
        cerr << "Cannot read: " << asn_type;
        return;
    }

    if ( args["se_in"] ) {
        CNcbiIstream& se_is = args["se_in"].AsInputFile();
    
        string se_asn_type;
        {{
            auto_ptr<CObjectIStream> se_in
                (CObjectIStream::Open(eSerial_AsnText, se_is));
            
            se_asn_type = se_in->ReadFileHeader();
            se_in->Close();
            se_is.seekg(0);
        }}
        
        auto_ptr<CObjectIStream> se_in
            (CObjectIStream::Open(eSerial_AsnText, se_is));
        
        if (se_asn_type == "Seq-entry") {
            CRef<CSeq_entry> se (new CSeq_entry);
            *se_in >> *se;
            m_Scope->AddTopLevelSeqEntry(*se);
        } else {
            cerr << "se_in only accepts a Seq-entry asn text file.";
            return;
        }
    }
}


// alternative ways to get the sequence

void CAlnVwrApp::View7()
{
    string buff;
    CAlnMap::TNumseg seg;
    CAlnMap::TNumrow row;

    m_AV->SetGapChar('-');
    m_AV->SetEndChar('.');
    for (seg=0; seg<m_AV->GetNumSegs(); seg++) {
        for (row=0; row<m_AV->GetNumRows(); row++) {
            NcbiCout << "row " << row << ", seg " << seg << " ";
            // if (m_AV->GetSegType(row, seg) & CAlnMap::fSeq) {
                NcbiCout << "["
                    << m_AV->GetStart(row, seg)
                    << "-"
                    << m_AV->GetStop(row, seg) 
                    << "]"
                    << NcbiEndl;
                for(TSeqPos i=0; i<m_AV->GetLen(seg); i++) {
                    NcbiCout << m_AV->GetResidue(row, m_AV->GetAlnStart(seg)+i);
                }
                NcbiCout << NcbiEndl;
                NcbiCout << m_AV->GetSeqString(buff, row,
                                           m_AV->GetStart(row, seg),
                                           m_AV->GetStop(row, seg)) << NcbiEndl;
                NcbiCout << m_AV->GetSegSeqString(buff, row, seg) 
                    << NcbiEndl;
                //            } else {
                //                NcbiCout << "-" << NcbiEndl;
                //            }
            NcbiCout << NcbiEndl;
        }
    }
}


// Demonstrate obtaining column vector in two alternative ways.
// (Use numeric param n to choose alignment position)
void CAlnVwrApp::View8(int aln_pos)
{
    CAlnMap::TSignedRange rng;
    rng.Set(aln_pos, aln_pos); // range covers only a single position
    
    string buffer;
    
    // obtain all individual residues
    for (CAlnMap::TNumrow row=0; row<m_AV->GetNumRows(); row++) {
        NcbiCout << m_AV->GetAlnSeqString(buffer, row, rng);
    }
    NcbiCout << NcbiEndl;
    
    // get the column at once
    string column;
    column.resize(m_AV->GetNumRows());
    
    NcbiCout << m_AV->GetColumnVector(column, aln_pos) << NcbiEndl;
    
    // %ID
    NcbiCout << m_AV->CalculatePercentIdentity(aln_pos) << NcbiEndl;
}


void CAlnVwrApp::View9(int row0, int row1)
{
    vector<TSignedSeqPos> result;
    CAlnMap::TRange aln_rng(0, m_AV->GetAlnStop()), rng0, rng1;

    m_AV->GetResidueIndexMap(row0, row1, aln_rng, result, rng0, rng1);

    size_t size = result.size();
    NcbiCout << "(" << rng0.GetFrom() << "-" << rng0.GetTo() << ")" << NcbiEndl;
    NcbiCout << "(" << rng1.GetFrom() << "-" << rng1.GetTo() << ")" << NcbiEndl;
    for (size_t i = 0; i < size; i++) {
        NcbiCout << result[i] << " ";
    }
    NcbiCout << NcbiEndl;
}


void CAlnVwrApp::View10(int row0, int row1)
{
    CAlnPos_CI it(*m_AV);
    do {
        NcbiCout << it.GetAlnPos() << "\t"
                 << it.GetSeqPos(row0) << "\t"
                 << it.GetSeqPos(row1) << NcbiEndl;
    } while (++it);
    NcbiCout << NcbiEndl;
}


//////
// GetSeqPosFromAlnPos
void CAlnVwrApp::GetSeqPosFromAlnPosDemo()
{
    NcbiCout << "["
        << m_AV->GetSeqPosFromAlnPos(0, 707, CAlnMap::eForward, false)
        << "-" 
        << m_AV->GetSeqPosFromAlnPos(0, 708, (CAlnMap::ESearchDirection)7, false)
        << "]"
        << NcbiEndl;
}


void CAlnVwrApp::GetAlnPosFromSeqPosDemo()
{
    NcbiCout << "["
        << m_AV->GetAlnPosFromSeqPos(0, 707, CAlnMap::eLeft, false)
        << "-" 
        << m_AV->GetAlnPosFromSeqPos(0, 708, CAlnMap::eRight, false)
        << "]"
        << NcbiEndl;
}


int CAlnVwrApp::Run(void)
{
    CArgs args = GetArgs();

    if ( args["log"] ) {
        SetDiagStream( &args["log"].AsOutputFile() );
    }

    LoadDenseg();


    if (args["a"]) {
        m_AV->SetAnchor(args["a"].AsInteger());
    }

    int screen_width = args["w"].AsInteger();
    int number       = args["n"].AsInteger();
    int row0         = args["row0"].AsInteger();
    int row1         = args["row1"].AsInteger();
    m_AV->SetGapChar('-');
    m_AV->SetEndChar('.');

    CAlnVecPrinter printer(*m_AV, NcbiCout);

    if (args["v"]) {
        switch (args["v"].AsInteger()) {
        case 1: 
            printer.CsvTable();
            break;
        case 2:
            printer.PopsetStyle(screen_width,
                                CAlnVecPrinter::eUseAlnSeqString);
            break;
        case 3: 
            printer.PopsetStyle(screen_width,
                                CAlnVecPrinter::eUseSeqString);
            break;
        case 4:
            printer.PopsetStyle(screen_width,
                                CAlnVecPrinter::eUseWholeAlnSeqString);
            break;
        case 5:
            printer.Segments();
            break;
        case 6:
            printer.Chunks(args["cf"].AsInteger());
            break;
        case 7:
            View7();
            break;
        case 8:
            View8(number);
            break;
        case 9:
            View9(row0, row1);
            break;
        case 10:
            View10(row0, row1);
            break;
        case 11:
            printer.ClustalStyle(screen_width,
                                 CAlnVecPrinter::eUseWholeAlnSeqString);
            break;
        default:
            NcbiCout << "Unknown view format." << NcbiEndl;
        }
    }
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CAlnVwrApp().AppMain(argc, argv, 0, eDS_Default, 0);
}

/*
* ===========================================================================
*
* $Log$
* Revision 1.7  2005/03/15 17:45:10  todorov
* Use the new printer class
*
* Revision 1.6  2005/03/01 17:23:10  todorov
* + void GetAlnPosFromSeqPosDemo()
*
* Revision 1.5  2004/10/04 16:12:25  todorov
* Added Seq-align as input type. It's segs must be of type Dense-seg
*
* Revision 1.4  2004/09/20 15:03:23  todorov
* Invclude a demo view using the new CAlnPos_CI class
*
* Revision 1.3  2004/09/16 18:25:41  todorov
* CAlnVwr::PopsetStyle flags change
*
* Revision 1.2  2004/08/30 12:32:42  todorov
* Made CNcbiOstream& a required param for the methods in CAlnVwr class. Changed the order of their params.
*
* Revision 1.1  2004/08/27 20:58:30  todorov
* alnvwrapp.cpp
*
* Revision 1.23  2004/07/21 15:51:26  grichenk
* CObjectManager made singleton, GetInstance() added.
* CXXXXDataLoader constructors made private, added
* static RegisterInObjectManager() and GetLoaderNameFromArgs()
* methods.
*
* Revision 1.22  2004/05/21 21:42:51  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.21  2004/03/03 19:41:59  todorov
* +GetResidueIndexMap
*
* Revision 1.20  2004/02/12 22:51:27  todorov
* +optinal seq-entry input file to read a local seq
*
* Revision 1.19  2004/02/03 19:52:25  todorov
* m_AV declared after m_OM so that its ref to scope is distroyed first
*
* Revision 1.18  2004/01/16 22:11:48  ucko
* Explicitly call DumpAsFasta() on Seq-ids intended to appear in FASTA format.
*
* Revision 1.17  2004/01/07 17:37:36  vasilche
* Fixed include path to genbank loader.
* Moved split_cache application.
*
* Revision 1.16  2003/12/19 19:37:26  todorov
* +comments
*
* Revision 1.15  2003/12/18 20:08:53  todorov
* Demo GetColumnVector & CalculatePercentIdentity
*
* Revision 1.14  2003/12/11 00:43:47  ucko
* Fix typo in previous revision: call Close on the CObjectIStream rather
* than the auto_ptr.
*
* Revision 1.13  2003/12/10 23:58:07  todorov
* Added CObjectIStream::Close before IStream::seekg
*
* Revision 1.12  2003/12/09 16:13:34  todorov
* code cleanup
*
* Revision 1.11  2003/12/08 21:28:04  todorov
* Forced Translation of Nucleotide Sequences
*
* Revision 1.10  2003/09/26 15:30:07  todorov
* +Print segments
*
* Revision 1.9  2003/07/23 21:01:08  ucko
* Revert use of (uncommitted) BLAST DB data loader.
*
* Revision 1.8  2003/07/23 20:52:07  todorov
* +width, +aln_starts for the inserts in GetWhole..
*
* Revision 1.7  2003/07/17 22:48:17  todorov
* View4 implemented in CAlnVec::GetWholeAlnSeqString
*
* Revision 1.6  2003/07/17 21:06:44  todorov
* -v is now required param
*
* Revision 1.5  2003/07/14 20:25:18  todorov
* Added another, even faster viewer
*
* Revision 1.4  2003/07/08 19:27:46  todorov
* Added an speed-optimized viewer
*
* Revision 1.3  2003/06/04 18:20:40  todorov
* read seq-submit
*
* Revision 1.2  2003/06/02 16:06:41  dicuccio
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
* Revision 1.1  2003/04/03 21:17:48  todorov
* Adding demo projects
*
*
* ===========================================================================
*/
