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
* Author:  Aaron Ucko
*
* File Description:
*   test code for CScope::GetTitle
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <objmgr/util/sequence.hpp>

// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
USING_SCOPE(sequence);

class CTitleTester : public CNcbiApplication
{
    virtual void Init(void);
    virtual int  Run(void);
};


void CTitleTester::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Show a sequence's title", false);

    arg_desc->AddKey("gi", "SeqEntryID", "GI id of the Seq-Entry to examine",
                     CArgDescriptions::eInteger);
    arg_desc->AddFlag("reconstruct", "Reconstruct title");
    arg_desc->AddFlag("accession", "Prepend accession");
    arg_desc->AddFlag("organism", "Append organism name");

    SetupArgDescriptions(arg_desc.release());
}


int CTitleTester::Run(void)
{
    const CArgs&   args = GetArgs();
    CObjectManager objmgr;
    CScope         scope(objmgr);
    CSeq_id        id;
    
    id.SetGi(args["gi"].AsInteger());

    objmgr.RegisterDataLoader(*(new CGBDataLoader), CObjectManager::eDefault);
    scope.AddDefaults();

    CBioseq_Handle handle = scope.GetBioseqHandle(id);
    TGetTitleFlags flags  = 0;
    if (args["reconstruct"]) {
        flags |= fGetTitle_Reconstruct;
    }
    if (args["organism"]) {
        flags |= fGetTitle_Organism;
    }
    NcbiCout << GetTitle(handle, flags) << NcbiEndl;
    return 0;
}


// (END_NCBI_SCOPE must be preceded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE

USING_NCBI_SCOPE;

int main(int argc, const char** argv)
{
    return CTitleTester().AppMain(argc, argv);
}

/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2004/05/21 21:42:56  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.1  2003/12/16 17:51:22  kuznets
* Code reorganization
*
* Revision 1.12  2003/06/02 16:06:39  dicuccio
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
* Revision 1.11  2002/11/15 17:53:28  ucko
* Only print the title once.
* Move CVS log to end.
*
* Revision 1.10  2002/06/07 16:13:34  ucko
* GetTitle() is now in sequence::.
*
* Revision 1.9  2002/06/07 14:34:33  ucko
* More changes for GetTitle(): pull in <objects/util/sequence.hpp>, drop
* CBioseq_Handle:: from flag-related identifiers.
*
* Revision 1.8  2002/06/06 19:47:38  clausen
* Removed usage of fGetTitle_Accession
*
* Revision 1.7  2002/05/06 03:28:53  vakatov
* OM/OM1 renaming
*
* Revision 1.6  2002/03/27 22:08:46  ucko
* Use high-level GB loader instead of low-level ID1 reader.
*
* Revision 1.5  2002/02/21 19:27:08  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.4  2002/01/29 18:21:07  grichenk
* CScope::GetTitle() -> CBioseq_Handle::GetTitle()
*
* Revision 1.3  2002/01/23 21:59:34  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.2  2002/01/15 21:36:41  grichenk
* Fixed to work with the new OM and scopes
*
* Revision 1.1  2002/01/11 19:06:28  gouriano
* restructured objmgr
*
* Revision 1.4  2001/12/07 19:39:19  butanaev
* Minor code improvements.
*
* Revision 1.3  2001/12/07 16:43:35  butanaev
* Switching to new reader interfaces.
*
* Revision 1.2  2001/10/17 20:51:54  ucko
* Pass flags to GetTitle.
*
* Revision 1.1  2001/10/12 21:20:37  ucko
* Add test for CScope::GetTitle
*
* ===========================================================================
*/
