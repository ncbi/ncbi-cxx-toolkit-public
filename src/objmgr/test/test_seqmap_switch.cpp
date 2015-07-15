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
* Author:
*           Eugene Vasilchenko
*
* File Description:
*           Set for CSeqMap switch logic
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_map_switch.hpp>

#include <common/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE
using namespace objects;


//===========================================================================
// CTestSeqMapSwitch

class CTestSeqMapSwitch : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int Run(void);
};


void CTestSeqMapSwitch::Init(void)
{
    // Prepare command line descriptions
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);


    arg_desc->AddDefaultKey("id", "id",
                            "Seq-id of sequence to process",
                            CArgDescriptions::eString, "lcl|main");
    arg_desc->AddDefaultKey("file", "file",
                            "read Seq-entry from the ASN.1 file",
                            CArgDescriptions::eInputFile, "entry.asn");
    arg_desc->AddOptionalKey("pos", "pos",
                             "process switch at this position",
                             CArgDescriptions::eInteger);

    string prog_description = "test_seqmap_switch";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    SetupArgDescriptions(arg_desc.release());
}


int CTestSeqMapSwitch::Run()
{
    const CArgs& args = GetArgs();
    
    CScope scope(*CObjectManager::GetInstance());

    if ( args["file"] ) {
        CRef<CSeq_entry> entry(new CSeq_entry);
        auto_ptr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnText,
                                                         args["file"].AsInputFile()));
        *in >> *entry;
        scope.AddTopLevelSeqEntry(*entry);
    }

    CSeq_id_Handle id;
    if ( args["id"] ) {
        CSeq_id seq_id(args["id"].AsString());
        id = CSeq_id_Handle::GetHandle(seq_id);
    }

    CBioseq_Handle bh = scope.GetBioseqHandle(id);
    if ( !bh ) {
        ERR_POST(Fatal << "no bioseq found");
    }
    
    
    TSeqMapSwitchPoints pp = GetAllSwitchPoints(bh);
    ITERATE ( TSeqMapSwitchPoints, it, pp ) {
        const CSeqMapSwitchPoint& p = **it;
        NcbiCout << "Switch @ " << p.m_MasterPos
                 << " " << p.m_LeftId.AsString()
                 << " -> " << p.m_RightId.AsString() << NcbiEndl;
        NcbiCout << "    range: " << p.m_MasterRange.GetFrom()
                 << ".." << p.m_MasterRange.GetTo() << NcbiEndl;
        NcbiCout << "    exact: " << p.m_ExactMasterRange.GetFrom()
                 << ".." << p.m_ExactMasterRange.GetTo() << NcbiEndl;
        TSeqPos pos, add;
        int diff;

        pos = p.m_MasterRange.GetFrom();

        add = p.GetInsert(pos);
        diff = p.GetLengthDifference(pos, add);
        NcbiCout << " if switched @ " << pos << " diff="<<diff << NcbiEndl;

        pos = p.m_MasterRange.GetTo();

        add = p.GetInsert(pos);
        diff = p.GetLengthDifference(pos, add);
        NcbiCout << " if switched @ " << pos << " diff="<<diff << NcbiEndl;
    }
    if ( args["pos"] ) {
        TSeqPos pos = args["pos"].AsInteger();
        NON_CONST_ITERATE ( TSeqMapSwitchPoints, it, pp ) {
            CSeqMapSwitchPoint& p = **it;
            if ( pos >= p.m_MasterRange.GetFrom() &&
                 pos <= p.m_MasterRange.GetTo() ) {
                NcbiCout << "Switching to " << pos << NcbiEndl;
                NcbiCout << "Before: " <<
                    MSerial_AsnText << *bh.GetCompleteObject() << NcbiEndl;
                p.ChangeSwitchPoint(pos, 0);
                NcbiCout << "After: " << 
                    MSerial_AsnText << *bh.GetCompleteObject() << NcbiEndl;
                break;
            }
        }
    }
    return 0;
}

END_NCBI_SCOPE

USING_NCBI_SCOPE;

//===========================================================================
// entry point

int main( int argc, const char* argv[])
{
    return CTestSeqMapSwitch().AppMain(argc, argv);
}
