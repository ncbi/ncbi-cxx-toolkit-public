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
* Author:  Kamen Todorov
*
* File Description:
*   Demo of alignment building.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <connect/ncbi_core_cxx.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <test/test_assert.h>

#include <serial/objistr.hpp>
#include <serial/iterator.hpp>

#include <objtools/alnmgr/aln_asn_reader.hpp>
#include <objtools/alnmgr/aln_container.hpp>
#include <objtools/alnmgr/aln_tests.hpp>
#include <objtools/alnmgr/aln_hints.hpp>


using namespace ncbi;
using namespace objects;


class CAlnBuildApp : public CNcbiApplication
{
public:
    virtual void Init         (void);
    virtual int  Run          (void);
    CScope&      GetScope     (void) const;
    void         LoadInputAlns(void);
    bool         InsertAln    (const CSeq_align* aln) {
        m_AlnContainer.insert(*aln);
        return true;
    }

private:
    mutable CRef<CObjectManager> m_ObjMgr;
    mutable CRef<CScope>         m_Scope;
    CAlnContainer                m_AlnContainer;
};


void CAlnBuildApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddDefaultKey
        ("in", "InputFileName",
         "Name of file to read from (standard input by default)",
         CArgDescriptions::eInputFile, "-");

    arg_desc->AddDefaultKey
        ("b", "bin_obj_type",
         "This forces the input file to be read in binary ASN.1 mode\n"
         "and specifies the type of the top-level ASN.1 object.\n",
         CArgDescriptions::eString, "");

    // Program description
    string prog_description = "Alignment build application.\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    SetupArgDescriptions(arg_desc.release());
}


void CAlnBuildApp::LoadInputAlns(void)
{
    const CArgs& args = GetArgs();
    string sname = args["in"].AsString();
    
    /// get the asn type of the top-level object
    string asn_type = args["b"].AsString();
    bool binary = !asn_type.empty();
    auto_ptr<CObjectIStream> in
        (CObjectIStream::Open(binary?eSerial_AsnBinary:eSerial_AsnText, sname));
    
    CAlnAsnReader reader(&GetScope());
    reader.Read(in.get(),
                bind1st(mem_fun(&CAlnBuildApp::InsertAln), this),
                asn_type);
}


CScope& CAlnBuildApp::GetScope(void) const
{
    if (!m_Scope) {
        m_ObjMgr = CObjectManager::GetInstance();
        CGBDataLoader::RegisterInObjectManager(*m_ObjMgr);
        
        m_Scope = new CScope(*m_ObjMgr);
        m_Scope->AddDefaults();
    }
    return *m_Scope;
}


int CAlnBuildApp::Run(void)
{
    // Setup application registry, error log, and MT-lock for CONNECT library
    CONNECT_Init(&GetConfig());

    LoadInputAlns();
    typedef vector<const CSeq_align*> TAlnVector;
    TAlnVector aln_vector(m_AlnContainer.size());
    aln_vector.assign(m_AlnContainer.begin(), m_AlnContainer.end());

    typedef SCompareOrdered<const CSeq_id*> TComp;
    TComp comp;

    typedef CAlnSeqIdVector<TAlnVector, TComp> TAlnSeqIdVector;
    TAlnSeqIdVector aln_id_vector(aln_vector, comp);

    CSeqIdAlnBitmap<TAlnSeqIdVector>
        id_aln_bitmap(aln_id_vector, GetScope());

    CAlnHints aln_hints;
    aln_hints.m_AnchorHandle = id_aln_bitmap.GetAnchorHandle();

    cout << "QueryAnchoredTest:" 
         << id_aln_bitmap.IsQueryAnchored() << endl;

    cout << "Number of alignments: " << id_aln_bitmap.AlnCount() << endl;
    cout << "Number of alignments containing nuc seqs:" << id_aln_bitmap.AlnWithNucCount() << endl;
    cout << "Number of alignments containing prot seqs:" << id_aln_bitmap.AlnWithProtCount() << endl;
    cout << "Number of alignments containing nuc seqs only:" << id_aln_bitmap.NucOnlyAlnCount() << endl;
    cout << "Number of alignments containing prot seqs only:" << id_aln_bitmap.ProtOnlyAlnCount() << endl;
    cout << "Number of alignments containing both nuc and prot seqs:" << id_aln_bitmap.TranslatedAlnCount() << endl;
    cout << "Number of sequences:" << id_aln_bitmap.SeqCount() << endl;
    cout << "Number of self-aligned sequences:" << id_aln_bitmap.SelfAlignedSeqCount() << endl;

    return 0;
}


int main(int argc, const char* argv[])
{
    return CAlnBuildApp().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2006/10/19 17:12:41  todorov
* Initial revision.
*
* ===========================================================================
*/
