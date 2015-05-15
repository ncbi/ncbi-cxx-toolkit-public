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

#include <objtools/alnmgr/aln_asn_reader.hpp>

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
    void             View10();
    bool             AddAlnToMix   (const CSeq_align* aln) {
        if (aln->GetSegs().IsDenseg()) {
            aln->GetSegs().GetDenseg().Validate(true);
            m_AV = new CAlnVec(aln->GetSegs().GetDenseg(), *m_Scope);
            return true;
        } else {
            return false;
        }
    }

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

    arg_desc->AddDefaultKey
        ("b", "bin_obj_type",
         "This forced the input file to be read in binary ASN.1 mode\n"
         "and specifies the type of the top-level ASN.1 object.\n",
         CArgDescriptions::eString, "");

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
         "10. Iterate forward and backwards through alignment positions\n"
         "    and show corresponding native sequence positions for each row.\n"
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
    //create scope
    {{
        m_ObjMgr = CObjectManager::GetInstance();
        CGBDataLoader::RegisterInObjectManager(*m_ObjMgr);

        m_Scope = new CScope(*m_ObjMgr);
        m_Scope->AddDefaults();
    }}

    const CArgs& args = GetArgs();
    string sname = args["in"].AsString();
    
    // get the asn type of the top-level object
    string asn_type = args["b"].AsString();
    bool binary = !asn_type.empty();
    auto_ptr<CObjectIStream> in
        (CObjectIStream::Open(binary?eSerial_AsnBinary:eSerial_AsnText, sname));
    
    CAlnAsnReader reader(m_Scope);
    reader.Read(in.get(),
                bind1st(mem_fun(&CAlnVwrApp::AddAlnToMix), this),
                asn_type);


    // read the seq-entry if provided
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


void CAlnVwrApp::View10()
{
    CAlnMap::TNumrow dim = m_AV->GetNumRows();
    vector<TSignedSeqPos> last_seq_pos(dim, -1);

    for (int reverse = 0; reverse < 2; ++reverse) {
        CAlnPos_CI it(*m_AV, reverse ? m_AV->GetAlnStop() : m_AV->GetAlnStart());
        IAlnExplorer::ESearchDirection search_dir = reverse ? IAlnExplorer::eRight : IAlnExplorer::eLeft;
        do {
            NcbiCout << it.GetAlnPos() << "\t";
            for (CAlnMap::TNumrow row = 0; row < dim; ++row) {
                NcbiCout << it.GetSeqPos(row) << "\t";
#ifdef _DEBUG
                if (it.GetSeqPos(row) >= 0) {
                    _ASSERT((TSignedSeqPos)it.GetAlnPos() == m_AV->GetAlnPosFromSeqPos(row, it.GetSeqPos(row)));
                    _ASSERT(m_AV->GetSeqPosFromAlnPos(row, it.GetAlnPos()) == it.GetSeqPos(row));
                    last_seq_pos[row] = it.GetSeqPos(row);
                } else if (last_seq_pos[row] >= 0) {
                    _ASSERT(m_AV->GetSeqPosFromAlnPos(row, it.GetAlnPos(), search_dir) == last_seq_pos[row]);
                }
                for (CAlnMap::TNumrow row2 = 0; row2 <= row; ++row2) {
                    if (it.GetSeqPos(row) >= 0  &&  last_seq_pos[row2] >= 0) {
                        _ASSERT(m_AV->GetSeqPosFromSeqPos(row2, row, it.GetSeqPos(row), search_dir) == last_seq_pos[row2]);
                    }
                    if (it.GetSeqPos(row2) >= 0  &&  last_seq_pos[row] >= 0) {
                        _ASSERT(m_AV->GetSeqPosFromSeqPos(row, row2, it.GetSeqPos(row2), search_dir) == last_seq_pos[row]);
                    }
                }
#endif
            }
            NcbiCout << NcbiEndl;
        } while (reverse ? --it : ++it);
        NcbiCout << NcbiEndl;
    }
}


int CAlnVwrApp::Run(void)
{
    const CArgs& args = GetArgs();

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
            View10();
            break;
        case 11:
            printer.ClustalStyle(screen_width,
                                 CAlnVecPrinter::eUseAlnSeqString);
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
