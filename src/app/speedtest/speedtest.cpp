// cppspeedtest.cpp

/*

(.bash_profile)

MyNewProject() {
  /netopt/ncbi_tools/c++.stable/scripts/new_project.sh $1 $2 \
    /netopt/ncbi_tools/c++.stable/GCC401-DebugMTUniv/build/
}
alias newproject='MyNewProject'

(terminal)

newproject cppspeedtest app/objects

modify Makefile.cppspeedtest_app by adding xobjutil xobjmgr to

LIB = xobjutil xobjmgr seqset $(SEQ_LIBS) pub medline biblio general xser xutil xncbi

make

*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbitime.hpp>

#include <serial/iterator.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/cleanup/cleanup.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);


/////////////////////////////////////////////////////////////////////////////


class CMytestApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
    void DoProcess (
        CNcbiIstream& ip,
        CNcbiOstream& op,
        CRef<CSeq_entry>& se
    );
    // data
    bool  m_bsec;
    bool  m_ssec;

    bool  m_fidx;

    bool  m_fasta;
    bool  m_nodef;
    bool  m_featfa;
    bool  m_transl;
    bool  m_svisit;
    bool  m_ostream;

    bool  m_fvisit;
    bool  m_goverlap;
    bool  m_gxref;
    bool  m_ooverlap;
};


/////////////////////////////////////////////////////////////////////////////


void CMytestApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "C++ speed test program");

    arg_desc->AddKey
        ("i", "InputFile",
         "Input File Name",
         CArgDescriptions::eInputFile);

    arg_desc->AddDefaultKey
        ("o", "OutputFile",
         "Output File Name",
         CArgDescriptions::eOutputFile, "-");

    arg_desc->AddDefaultKey
        ("a", "ASN1Type",
         "ASN.1 Type",
         CArgDescriptions::eString, "a");
    arg_desc->SetConstraint
        ("a", &(*new CArgAllow_Strings, "a", "e", "b", "s", "m", "t", "l"));

    arg_desc->AddDefaultKey
        ("X", "Repetitions",
         "Max Repeat Count",
         CArgDescriptions::eInteger, "1");

    arg_desc->AddOptionalKey
        ("K", "Cleanup",
         "b Basic, s Serious",
         CArgDescriptions::eString);

    arg_desc->AddOptionalKey
        ("I", "Indexing",
         "f Feature Indexing",
         CArgDescriptions::eString);

    arg_desc->AddOptionalKey
        ("S", "Sequence",
         "s FASTA, r No Defline, f By Feature, t Translation, v Visit, o Ostream",
         CArgDescriptions::eString);

    arg_desc->AddOptionalKey
        ("F", "Feature",
         "v Visit, g Gene by Overlap, x Xref, o Operon",
         CArgDescriptions::eString);

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



/////////////////////////////////////////////////////////////////////////////

void CMytestApplication::DoProcess (
    CNcbiIstream& ip,
    CNcbiOstream& op,
    CRef<CSeq_entry>& se
)

{
    if (m_bsec) {
        /*
        CCleanup Cleanup;
        Cleanup.BasicCleanup( se.GetObject() );
        */
    }
    if (m_ssec) {
        // need to implement
    }

    if (m_fidx) {
        // need to implement
    }

    if (m_fasta) {
        CFastaOstream fo (op);
        for (CTypeConstIterator<CBioseq> bit (*se); bit; ++bit) {
            fo.Write (*bit);
        }
    }
    if (m_nodef) {
        // need to implement
    }
    if (m_featfa) {
        // need to implement
    }
    if (m_transl) {
        // need to implement
    }
    if (m_svisit) {
        for (CTypeConstIterator<CBioseq> bit (*se); bit; ++bit) {
            if (m_ostream) {
                CFastaOstream fo (op);
            }
        }
    }
    if (m_ostream && (! m_svisit)) {
        CFastaOstream fo (op);
    }

    if (m_fvisit) {
        for (CTypeConstIterator<CBioseq> bit (*se); bit; ++bit) {
            // need to implement
        }
    }
    if (m_goverlap) {
        // need to implement
    }
    if (m_gxref) {
        // need to implement
    }
    if (m_ooverlap) {
        // need to implement
    }
}

int CMytestApplication::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();

    CNcbiIstream& ip = args["i"].AsInputFile();
    CNcbiOstream& op = args["o"].AsOutputFile();

    string tp = args["a"].AsString();

    int mx = args["X"].AsInteger();
    if (mx < 1) {
        mx = 1;
    }

    m_bsec = false;
    m_ssec = false;

    m_fidx = false;

    m_fasta = false;
    m_nodef = false;
    m_featfa = false;
    m_transl = false;
    m_svisit = false;
    m_ostream = false;

    m_fvisit = false;
    m_goverlap = false;
    m_gxref = false;
    m_ooverlap = false;

    if (args["K"]) {
        string km = args["K"].AsString();
        if (NStr::Find (km, "b") != NPOS) {
            m_bsec = true;
        }
        if (NStr::Find (km, "s") != NPOS) {
            m_ssec = true;
        }
    }

    if (args["I"]) {
        string im = args["I"].AsString();
        if (NStr::Find (im, "f") != NPOS) {
            m_fidx = true;
        }
    }

    if (args["S"]) {
        string sm = args["S"].AsString();
        if (NStr::Find (sm, "s") != NPOS) {
            m_fasta = true;
        }
        if (NStr::Find (sm, "r") != NPOS) {
            m_nodef = true;
        }
        if (NStr::Find (sm, "f") != NPOS) {
            m_featfa = true;
        }
        if (NStr::Find (sm, "t") != NPOS) {
            m_transl = true;
        }
        if (NStr::Find (sm, "v") != NPOS) {
            m_svisit = true;
        }
        if (NStr::Find (sm, "o") != NPOS) {
            m_ostream = true;
        }
    }

    if (args["F"]) {
        string fm = args["F"].AsString();
        if (NStr::Find (fm, "v") != NPOS) {
            m_fvisit = true;
        }
        if (NStr::Find (fm, "g") != NPOS) {
            m_goverlap = true;
        }
        if (NStr::Find (fm, "x") != NPOS) {
            m_gxref = true;
        }
        if (NStr::Find (fm, "o") != NPOS) {
            m_ooverlap = true;
        }
    }

    int ct;
    CTime t1, t2;
    TSeconds tx;

    // read line at a time if indicated
    if (NStr::Equal(tp, "l")) {
        string str;
        t1.SetCurrent();

        while (NcbiGetlineEOL (ip, str)) {
            if (! str.empty ()) {
                // op << str << endl;
            }
        }

        t2.SetCurrent();
        tx = t2.DiffSecond (t1);
        NcbiCout << "Read by line time is " << tx << " seconds" << endl;
        return 0;
    }

    t1.SetCurrent();

    // otherwise read ASN.1
    auto_ptr<CObjectIStream> is (CObjectIStream::Open (eSerial_AsnText, ip));
    CRef<CSeq_entry> se(new CSeq_entry);
    *is >> *se;

    t2.SetCurrent();
    tx = t2.DiffSecond (t1);
    NcbiCout << "ASN reading time is " << tx << " seconds" << endl;

    t1.SetCurrent();

    for (ct = 0; ct < mx; ct++) {
        DoProcess (ip, op, se);
    }

    t2.SetCurrent();
    tx = t2.DiffSecond (t1);
    NcbiCout << "Internal processing time is " << tx << " seconds" << endl;

    // write ASN.1
    /*
    if (NStr::Equal (fm, "a")) {
        auto_ptr<CObjectOStream> os (CObjectOStream::Open (eSerial_AsnText, op));
        *os << *se;
        t2.SetCurrent();
        tx = t2.DiffSecond (t1);
        NcbiCout << "elapsed time is " << tx << endl;
        return 0;
    }
    */

    return 0;
}


/////////////////////////////////////////////////////////////////////////////


void CMytestApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CMytestApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}

