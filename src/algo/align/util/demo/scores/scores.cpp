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
* Author:  Cliff Clausen, NCBI
*
* File Description:
*   Adds scores to alignments in seq-annots using CScoreBuilder. Adds:
*       bit score
*       e-value
*       identity count
*       percent identity 
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbienv.hpp>

#include <connect/ncbi_core_cxx.hpp>

#include <serial/iterator.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>
#include <serial/objostrasnb.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <algo/align/util/score_builder.hpp>

#include <objtools/alnmgr/alnmix.hpp>
#include <objtools/alnmgr/alnvec.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/general/Object_id.hpp>
#include <util/util_exception.hpp>

USING_SCOPE(ncbi);
USING_SCOPE(objects);

class CScoresApplication : public CNcbiApplication
{
    virtual void Init(void);
    virtual int  Run(void);
};

void CScoresApplication::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "Calculate scores for alignments");

    arg_desc->AddKey
        ("in", 
         "InputFile",
         "name of file to read from",
         CArgDescriptions::eString);

    arg_desc->AddKey
        ("out", 
         "OutputFile",
         "name of file to write to",
         CArgDescriptions::eString);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CScoresApplication::Run(void)
{    
    // Setup application registry, error log, and MT-lock for CONNECT library
    CONNECT_Init(&GetConfig());

    // Get arguments
    CArgs args = GetArgs();
    
    
    CRef<CSeq_annot> seq_annot(new CSeq_annot());
    CNcbiIfstream is(args["in"].AsString().c_str());   
    try {
        while(true) {
            CRef<CSeq_annot> tmp_annot(new CSeq_annot());
            is >> MSerial_AsnBinary >> *tmp_annot;
            CSeq_annot::TData::TAlign& alns = tmp_annot->SetData().SetAlign();
            NON_CONST_ITERATE(CSeq_annot::TData::TAlign, it, alns) {
                seq_annot->SetData().SetAlign().push_back(*it);
            }
        }
    } catch (CEofException& e) {
    }
    
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);
    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();
    CScoreBuilder score_builder(blast::eMegablast);

    const Int8 kEffectiveSearchSpace = 1050668186940LL;
    
    CSeq_annot::TData::TAlign& align_list = seq_annot->SetData().SetAlign();
    try {
        score_builder.SetEffectiveSearchSpace(kEffectiveSearchSpace);

        NON_CONST_ITERATE(CSeq_annot::TData::TAlign, it, align_list) {
            score_builder.AddScore(*scope, **it, CScoreBuilder::eScore_Blast_BitScore);
            score_builder.AddScore(*scope, **it, CScoreBuilder::eScore_Blast_EValue);
            score_builder.AddScore(*scope, **it, CScoreBuilder::eScore_IdentityCount);
            score_builder.AddScore(*scope, **it, CScoreBuilder::eScore_PercentIdentity);
            
            NON_CONST_ITERATE(CSeq_align::TScore, it2, (*it)->SetScore()) {
                if (!(*it2)->IsSetValue()) {
                    continue;
                }
                if ((*it2)->GetValue().IsReal() && 
                    (*it2)->GetValue().GetReal() == numeric_limits<double>::infinity())
                {
                    (*it2)->SetValue().SetReal() = numeric_limits<double>::max() / 10.0;
                }
            }
        }
    }
    catch (CException &e) {
        cerr << "Error using CScoreBuilder" << endl;
        throw e;
    }

    // Write output file
    // CNcbiOfstream os(args["out"].AsString().c_str());
    // os << MSerial_AsnBinary << *seq_annot;
    auto_ptr<CNcbiOfstream> os(new CNcbiOfstream(args["out"].AsString().c_str()));
    auto_ptr<CObjectOStreamAsnBinary> os_asn(new CObjectOStreamAsnBinary(*os, false));
    *os_asn << *seq_annot;
    
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CScoresApplication().AppMain(argc, argv);
}
