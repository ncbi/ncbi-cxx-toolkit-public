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
* Author: Clifford Clausen
*
* File Description: Demo for CRegexp to CSeq_loc class (CRegexp_loc)
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <algo/sequence/regexp_loc.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

#include <objmgr/object_manager.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>

#include <serial/iterator.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

class CRegexpLocApp : public CNcbiApplication {
public:
    CRegexpLocApp(void) {DisableArgDescriptions();};
    virtual void Init(void);
    virtual int Run(void);
};

void CRegexpLocApp::Init(void)
{
    auto_ptr<CArgDescriptions> argDescr(new CArgDescriptions);
    argDescr->AddKey("a", "accession",
                     "GENBANK accession", CArgDescriptions::eString);

    argDescr->AddKey("r", "PCRE",
                     "Perl Compatible Regular Expression",
                     CArgDescriptions::eString);

    argDescr->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Displays CSeq_loc for CRegexp\n", false);
    SetupArgDescriptions(argDescr.release());
}

// Display CSeq_loc
CNcbiOstream& operator<< (CNcbiOstream& os, const CSeq_loc &loc)
{
    auto_ptr<CObjectOStream> los(CObjectOStream::Open(eSerial_AsnText, os));
    *los << loc;
    return os;
};

// Gets raw sequence for input accession, matches pattern to it creating 
// a CSeq_loc, then displays the CSeq_loc
int GetLoc(const string& acc, const string &pat, CSeq_loc &loc, CScope &scope)
{
    CRef<CSeq_id> seq_id(new CSeq_id(acc));
    if (seq_id->Which() == CSeq_id::e_not_set) {
        cerr << "Invalid seq-id: '" << acc << "'" << endl;
        return 1;
    }
    
     CBioseq_Handle bioseq_handle = scope.GetBioseqHandle(*seq_id);
    if (bioseq_handle) {
        CSeqVector sv =
            bioseq_handle.GetSeqVector(CBioseq_Handle::eCoding_Iupac);

        // Get the raw sequence data and display it
        string seq;
        seq.reserve(sv.size());
        sv.GetSeqData(0, sv.size(), seq);
        cout << "seq=" << seq << endl;
        
        // Set pattern
        CRegexp_loc rl(pat);
        
        // Find matches
        TSeqPos offset = 0;
        do {
            // Get match
            offset = rl.GetLoc(seq.c_str(), &loc, offset) + 1;
            cout << offset << endl;
            // Add seq_id to loc so it will display OK 
            for (CTypeIterator<CSeq_interval> it(Begin(loc)); it; ++it) {
                it->SetId(*seq_id);
            }
            // Display loc           
            cout << loc << endl;
        } while (offset != 0 && offset < seq.size());
        
        return 0;
    } else {
        cerr << "Bioseq load FAILED." << endl;
        return 2;
    }
}

int CRegexpLocApp::Run(void)
{
    const CArgs& args = GetArgs();
    CRef<CObjectManager> objMgr(new CObjectManager);
    objMgr->RegisterDataLoader(*new CGBDataLoader("GENBANK"),
                               CObjectManager::eDefault);
    CScope scope(*objMgr);
    scope.AddDefaults();
    int retCode = 0;

    CSeq_loc loc;
    retCode = GetLoc(args["a"].AsString(),
                     args["r"].AsString(),
                     loc,
                     scope);    
    return retCode;
}

int main(int argc, char** argv)
{
    CRegexpLocApp theApp;
    return theApp.AppMain(argc, argv, NULL, eDS_Default, 0);
}

/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2004/05/21 21:41:04  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.2  2004/01/07 17:39:28  vasilche
 * Fixed include path to genbank loader.
 *
 * Revision 1.1  2003/07/16 19:22:00  clausen
 * Initial version
 *
 *
 * ===========================================================================
 */
