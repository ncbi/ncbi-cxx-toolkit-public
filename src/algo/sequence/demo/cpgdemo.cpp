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
* Author: Philip Johnson
*
* File Description: cpgdemo -- demo program for cpg island finder
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <algo/sequence/cpg.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

#include <objmgr/object_manager.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

class CCpGDemoApp : public CNcbiApplication {
public:
    CCpGDemoApp(void) {DisableArgDescriptions();};
    virtual void Init(void);
    virtual int Run(void);
};

/*---------------------------------------------------------------------------*/

void CCpGDemoApp::Init(void)
{
    auto_ptr<CArgDescriptions> argDescr(new CArgDescriptions);
    argDescr->AddDefaultKey("gc", "gcContent", "calibrated organism %GC "
                            "content (ie. human: 0.5, rat: 0.48)",
                            CArgDescriptions::eDouble, "0.5");
    argDescr->AddDefaultKey("cpg", "obsexp",
                            "observed / expected CpG ratio",
                            CArgDescriptions::eDouble, "0.6");
    argDescr->AddDefaultKey("win", "window_size",
                            "width of sliding window",
                            CArgDescriptions::eInteger, "200");
    argDescr->AddDefaultKey("len", "min_length",
                            "minimum length of an island",
                            CArgDescriptions::eInteger, "500");
    argDescr->AddOptionalKey("m", "merge_isles",
                             "merge adjacent islands within the specified "
                             "distance of each other",
                             CArgDescriptions::eInteger);



    argDescr->AddOptionalKey("a", "accession",
                             "Single accession", CArgDescriptions::eString);
    argDescr->AddOptionalKey("i", "infile",
                             "List of accessions",
                             CArgDescriptions::eInputFile);
    argDescr->AddExtra(0,99, "fasta files", CArgDescriptions::eInputFile);


    argDescr->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Scans sequences for CpG islands; uses algorithm based upon Takai & Jones, 2002.  Output sent to stdout.\n", false);
    SetupArgDescriptions(argDescr.release());
}

//---------------------------------------------------------------------------
// PRE : open output stream, populated CpG island struct
// POST: <cpg start> <cpg stop> <%G + C> <obs/exp CpG>
CNcbiOstream& operator<< (CNcbiOstream& o, SCpGIsland i)
{
    unsigned int len = i.m_Stop - i.m_Start + 1;
    o << i.m_Start << "\t" << i.m_Stop << "\t" <<
        (double) (i.m_C + i.m_G) / len << "\t" <<
        (double) i.m_CG * len / (i.m_C * i.m_G);
    return o;
};

//---------------------------------------------------------------------------
// PRE : accession to scan, scope in which to resolve accession, CArgs
// containing cpg island-scanning parameters
// POST: 0 if successful, any islands found in the given accession according
// to the arguments have been sent to cout
int ScanForCpGs(const string& acc, CScope &scope, const CArgs& args)
{
    CSeq_id seq_id(acc);
    if (seq_id.Which() == CSeq_id::e_not_set) {
        cerr << "Invalid seq-id: '" << acc << "'" << endl;
        return 1;
    }

    CBioseq_Handle bioseq_handle = scope.GetBioseqHandle(seq_id);

    if (!bioseq_handle) {
        cerr << "Bioseq load FAILED." << endl;
        return 2;
    }

    CSeqVector sv =
        bioseq_handle.GetSeqVector(CBioseq_Handle::eCoding_Iupac);    
    string seqString;
    seqString.reserve(sv.size());
    sv.GetSeqData(0, sv.size(), seqString);
    
    CCpGIslands cpgIsles(seqString.data(), seqString.length(),
                         args["win"].AsInteger(), args["len"].AsInteger(),
                         args["gc"].AsDouble(), args["cpg"].AsDouble());
    if (args["m"]) {
        cpgIsles.MergeIslesWithin(args["m"].AsInteger());
    }
    
    ITERATE(CCpGIslands::TIsles, i, cpgIsles.GetIsles()) {
        cout << acc << "\t" << *i << endl;
    }

    return 0;
}

//---------------------------------------------------------------------------
// PRE : fasta file to scan, scope in which to resolve accession, CArgs
// containing cpg island-scanning parameters
// POST: 0 if successful, any islands found in the given accession according
// to the arguments have been sent to cout
int ScanForCpGs(istream &infile, const CArgs &args)
{
    string localID;

    infile >> localID;
    //skip rest of line
    infile.ignore(numeric_limits<int>::max(), '\n');

    while (infile) {
        if (localID[0] != '>') {
            ERR_POST(Critical << "FASTA file garbled around '" <<localID<<"'");
            return 1;
        }
        
        //read in nucleotides
        string seqString, lineBuff;
        while (!infile.eof() && infile.peek() != '>') {
            getline(infile, lineBuff);
            if (seqString.size() + lineBuff.size() > seqString.capacity())
                seqString.reserve(seqString.capacity() * 2);
            seqString += lineBuff;
        }

        //scan
        CCpGIslands cpgIsles(seqString.data(), seqString.length(),
                             args["win"].AsInteger(), args["len"].AsInteger(),
                             args["gc"].AsDouble(), args["cpg"].AsDouble());
        if (args["m"]) {
            cpgIsles.MergeIslesWithin(args["m"].AsInteger());
        }

        //output
        ITERATE(CCpGIslands::TIsles, i, cpgIsles.GetIsles()) {
            cout << localID << "\t" << *i << endl;
        }

        infile >> localID;
        infile.ignore(numeric_limits<int>::max(), '\n');
    }

    return 0;
}


/*---------------------------------------------------------------------------*/

int CCpGDemoApp::Run(void)
{
    const CArgs& args = GetArgs();
    CRef<CObjectManager> objMgr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*objMgr);

    CScope scope(*objMgr);
    scope.AddDefaults();
    int retCode = 0;

    cout.precision(2);
    cout.setf(ios::fixed, ios::floatfield);
    cout << "# CpG islands.  Win:" << args["win"].AsInteger()
         << "; Min len:" << args["len"].AsInteger() << "; Min %GC:"
         <<  args["gc"].AsDouble() << "; Min obs/exp CpG: "
         << args["cpg"].AsDouble();
    if (args["m"]) {
        cout << "; Merge islands within: " << args["m"].AsInteger();
    }
    cout << endl;
    cout << "# label\tisle_start\tisle_stop\t%GC\tobs/exp CpG" << endl;

    if (args["a"]) {
        retCode = ScanForCpGs(args["a"].AsString(), scope, args);
    }

    if (args["i"]) {
        istream &infile = args["i"].AsInputFile();
        string acc;

        infile >> acc;
        while (infile.good()) {
            cerr << "Processing " << acc << endl;
            if (ScanForCpGs(acc, scope, args) != 0) {
                retCode = 3;
            }
            infile >> acc;
        }
    }

    for (unsigned int i = 1; i <= args.GetNExtra(); ++i) {
        if (!ScanForCpGs(args[i].AsInputFile(), args)) {
            retCode = 3;
        }
    }

    return retCode;
}

//---------------------------------------------------------------------------
int main(int argc, char** argv)
{
    CCpGDemoApp theApp;
    return theApp.AppMain(argc, argv, NULL, eDS_Default, 0);
}

/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2004/07/21 15:51:24  grichenk
 * CObjectManager made singleton, GetInstance() added.
 * CXXXXDataLoader constructors made private, added
 * static RegisterInObjectManager() and GetLoaderNameFromArgs()
 * methods.
 *
 * Revision 1.5  2004/05/21 21:41:04  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.4  2004/01/07 17:39:28  vasilche
 * Fixed include path to genbank loader.
 *
 * Revision 1.3  2003/12/12 20:06:34  johnson
 * accommodate MSVC 7 refactoring; also made more features accessible from
 * command line
 *
 * Revision 1.2  2003/06/17 15:35:12  johnson
 * remove stray char
 *
 * Revision 1.1  2003/06/17 15:12:24  johnson
 * initial revision
 *
 * ===========================================================================
 */
