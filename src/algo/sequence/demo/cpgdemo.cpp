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

#include <algo/sequence/cpg.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/gbloader.hpp>
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
    argDescr->AddOptionalKey("a", "accession",
                             "Single accession", CArgDescriptions::eString);
    argDescr->AddOptionalKey("i", "infile",
                             "List of accessions",
                             CArgDescriptions::eInputFile);
    argDescr->AddKey("o", "outfile",
                     "Root name for output files ('.strict' and '.relaxed'"
                     " will be added)", CArgDescriptions::eOutputFile);

    argDescr->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Scans sequences for CpG islands\n", false);
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
// PRE : accession to scan, scope in which to resolve accession, two open
// output streams
// POST: 0 if successful, any islands found in the given accession according
// to the strict rules have been sent to strictFile & those meeting the
// relaxed rules to relaxedFile
int ScanForCpGs(const string& acc, CScope &scope, ostream& strictFile,
                ostream& relaxedFile)
{
    CSeq_id seq_id(acc);
    if (seq_id.Which() == CSeq_id::e_not_set) {
        cerr << "Invalid seq-id: '" << acc << "'" << endl;
        return 1;
    }

     CBioseq_Handle bioseq_handle = scope.GetBioseqHandle(seq_id);
    if (bioseq_handle) {
        CSeqVector sv =
            bioseq_handle.GetSeqVector(CBioseq_Handle::eCoding_Iupac);

        string seqString;
        seqString.reserve(sv.size());
        sv.GetSeqData(0, sv.size(), seqString);

        CCpGIslands cpgIsles(seqString.data(), seqString.length(),
                             200, 200, 0.5, 0.6);
        cpgIsles.MergeIslesWithin(100);

        ITERATE(CCpGIslands, i, cpgIsles) {
            relaxedFile << acc << "\t" << *i << endl;
        }
`
        cpgIsles.Calc(200, 500, 0.5, 0.6);
        ITERATE(CCpGIslands, i, cpgIsles) {
            strictFile << acc << "\t" << *i << endl;
        }

        return 0;
    } else {
        cerr << "Bioseq load FAILED." << endl;
        return 2;
    }
}

/*---------------------------------------------------------------------------*/

int CCpGDemoApp::Run(void)
{
    const CArgs& args = GetArgs();
    CRef<CObjectManager> objMgr(new CObjectManager);
    objMgr->RegisterDataLoader(*new CGBDataLoader("GENBANK"),
                               CObjectManager::eDefault);
    CScope scope(*objMgr);
    scope.AddDefaults();
    int retCode = 0;

    ofstream strictFile((args["o"].AsString() + ".strict").c_str());
    ofstream relaxedFile((args["o"].AsString() + ".relaxed").c_str());
    strictFile.precision(2);
    strictFile.setf(ios::fixed, ios::floatfield);
    relaxedFile.precision(2);
    relaxedFile.setf(ios::fixed, ios::floatfield);

    if (args["a"]) {
        retCode = 
            ScanForCpGs(args["a"].AsString(), scope, strictFile, relaxedFile);
    }

    if (args["i"]) {
        istream &infile = args["i"].AsInputFile();
        string acc;

        infile >> acc;
        while (infile.good()) {
            cout << "Processing " << acc << endl;
            if (ScanForCpGs(acc, scope, strictFile, relaxedFile) != 0) {
                retCode = 3;
            }
            infile >> acc;
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
 * Revision 1.1  2003/06/17 15:12:24  johnson
 * initial revision
 *
 * ===========================================================================
 */
