/* $Id$
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
 * Author:  Yuri Kapustin
 *
 * File Description:  HitFilter application
 *                   
*/

#include <ncbi_pch.hpp>

#include "hitfilter_app.hpp"

#include <algo/align/util/blast_tabular.hpp>
#include <objects/seqloc/Seq_id.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

void CAppHitFilter::Init()
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideVersion);

    auto_ptr<CArgDescriptions> argdescr(new CArgDescriptions);
    argdescr->SetUsageContext(GetArguments().GetProgramName(),
                              "HitFilter v.2.0.0");

    SetupArgDescriptions(argdescr.release());
}


int CAppHitFilter::Run()
{ 
    //typedef CConstRef<CSeq_id>     TId;
    typedef Uint4     TId;
    typedef CBlastTabular<TId>     THit;
    typedef vector<THit>           THits;
    
    // read hits from stdin
    THits hits;
    while(cin) {

        char line [1024];
        cin.getline(line, sizeof line, '\n');
        string s (NStr::TruncateSpaces(line));
        if(s.size()) {

            THit hit (s.c_str());
            hits.push_back(hit);
        }
    }

    // do something
    // ... ... ...

    // dump the result
    ITERATE(THits, ii, hits) {
         cout << *ii << endl;
    }

    return 0;
}


void CAppHitFilter::Exit()
{
    return;
}


END_NCBI_SCOPE


USING_NCBI_SCOPE;

int main(int argc, const char* argv[]) 
{
    return CAppHitFilter().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2005/04/18 15:24:48  kapustin
 * Split CAlignShadow into core and blast tabular representation
 *
 * Revision 1.3  2004/12/22 21:26:18  kapustin
 * Move friend template definition to the header. Declare explicit 
 * specialization.
 *
 * Revision 1.2  2004/12/21 22:45:19  kapustin
 * Temporarily comment out the code
 *
 * Revision 1.1  2004/12/21 20:07:47  kapustin
 * Initial revision
 *
 * ===========================================================================
 */
