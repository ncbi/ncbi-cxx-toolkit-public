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
*   Demo of extracting a pairwise alignment from a file with Seq-align(s).
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <connect/ncbi_core_cxx.hpp>

#include <test/test_assert.h>

#include <serial/objistr.hpp>
#include <serial/iterator.hpp>

#include <objtools/alnmgr/aln_asn_reader.hpp>
#include <objtools/alnmgr/pairwise_aln.hpp>


using namespace ncbi;
using namespace objects;


ostream& operator<<(ostream& out, const CPairwiseAln& coll)
{
    char s[32];
    sprintf(s, "%X", (unsigned int) coll.GetFlags());
    out << "CPairwiseAln  Flags = " << s << ", : ";

    ITERATE (CPairwiseAln, it, coll) {
        const CPairwiseAln::TAlnRng& r = *it;
        out << "[" << r.GetFirstFrom() << ", " << r.GetSecondFrom() << ", "
            << r.GetLength() << ", " << (r.IsDirect() ? "true" : "false") << "]";
    }
    return out;
}


class CPairwiseAlnApp : public CNcbiApplication
{
public:
    virtual void Init         (void);
    virtual int  Run          (void);
    void         LoadInputAlns(void);
    bool         InsertAln    (const CSeq_align* aln);

private:
    int m_QueryRow;
    int m_SubjectRow;
};


void CPairwiseAlnApp::Init(void)
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

    arg_desc->AddDefaultKey
        ("q", "QueryRow",
         "Query (anchor) row (zero-based)",
         CArgDescriptions::eInteger, "0");

    arg_desc->AddDefaultKey
        ("s", "SubjectRow",
         "Subject row (zero-based)",
         CArgDescriptions::eInteger, "1");

    // Program description
    string prog_description = "Alignment build application.\n";
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              prog_description, false);

    SetupArgDescriptions(arg_desc.release());
}


void CPairwiseAlnApp::LoadInputAlns(void)
{
    const CArgs& args = GetArgs();
    string sname = args["in"].AsString();
    
    /// get the asn type of the top-level object
    string asn_type = args["b"].AsString();
    bool binary = !asn_type.empty();
    auto_ptr<CObjectIStream> in
        (CObjectIStream::Open(binary?eSerial_AsnBinary:eSerial_AsnText, sname));
    
    CAlnAsnReader reader;
    reader.Read(in.get(),
                bind1st(mem_fun(&CPairwiseAlnApp::InsertAln), this),
                asn_type);
}


bool
CPairwiseAlnApp::InsertAln(const CSeq_align* sa) 
{
    sa->Validate(true);
    CPairwiseAln pairwise_aln(*sa, m_QueryRow, m_SubjectRow);
    cout << pairwise_aln << endl;
    return true;
}


int CPairwiseAlnApp::Run(void)
{
    // Setup application registry, error log, and MT-lock for CONNECT library
    CONNECT_Init(&GetConfig());

    m_QueryRow = GetArgs()["q"].AsInteger();
    m_SubjectRow = GetArgs()["s"].AsInteger();
    LoadInputAlns();
    return 0;
}


int main(int argc, const char* argv[])
{
    return CPairwiseAlnApp().AppMain(argc, argv, 0, eDS_Default, 0);
}


/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2006/10/19 20:35:47  todorov
* Initial revision
*
* ===========================================================================
*/
