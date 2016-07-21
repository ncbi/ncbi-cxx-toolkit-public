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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *   C++ replacement for standard 'idfetch' tool
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/util/sequence.hpp>
#include <misc/data_loaders_util/data_loaders_util.hpp>

#include <serial/serial.hpp>
#include <serial/objostr.hpp>

#include <misc/eutils_client/eutils_client.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

/////////////////////////////////////////////////////////////////////////////
//  CIdfetchApplication::


class IOutputFormatter
{
public:
    IOutputFormatter(CNcbiOstream& ostr)
        : m_Ostr(ostr)
    {
    }

    virtual ~IOutputFormatter()
    {
    }

    virtual void Format(const CBioseq_Handle&) = 0;

protected:
    CNcbiOstream& m_Ostr;
};


class COutput_SerialFormat : public IOutputFormatter
{
public:
    COutput_SerialFormat(CNcbiOstream& ostr,
                         ESerialDataFormat fmt)
        : IOutputFormatter(ostr)
    {
        m_Os.reset(CObjectOStream::Open(fmt, m_Ostr));
    }

    virtual void Format(const CBioseq_Handle& bsh)
    {
        CSeq_entry_Handle tse = bsh.GetTopLevelEntry();
        *m_Os << *tse.GetCompleteSeq_entry();
    }

private:
    auto_ptr<CObjectOStream> m_Os;

};


class COutput_Fasta : public IOutputFormatter
{
public:
    COutput_Fasta(CNcbiOstream& ostr)
        : IOutputFormatter(ostr)
    {
        m_Os.reset(new CFastaOstream(ostr));
    }

    virtual void Format(const CBioseq_Handle& bsh)
    {
        CSeq_entry_Handle tse = bsh.GetTopLevelEntry();
        m_Os->Write(tse);
    }

private:
    auto_ptr<CFastaOstream> m_Os;

};


/////////////////////////////////////////////////////////////////////////////
///


class CIdfetchApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

    // this must match the output formats in '-t'
    enum EOutputFormat {
        eAsnText = 1,
        eAsnBinary = 2,
        eGenbank = 3,
        eGenpept = 4,
        eFASTA = 5,
        eQualityScores = 6,
        eDocsums = 7,
        eFASTAReverse = 8
    };

    static IOutputFormatter* x_GetFormatter(CNcbiOstream& ostr,
                                            const CArgs& args);
};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CIdfetchApplication::Init(void)
{
    // // Set error posting and tracing on maximum
    // SetDiagTrace(eDT_Enable);
    // SetDiagPostFlag(eDPF_All);
    // SetDiagPostLevel(eDiag_Info);

    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Retrieve sequences in a varietu of formats");

    arg_desc->AddDefaultKey("o", "Output",
                            "Filename for output",
                            CArgDescriptions::eOutputFile,
                            "-");

    arg_desc->AddDefaultKey("t", "Type",
                            "Output type.  Options are:\n"
                            "1 = text ASN.1\n"
                            "2 = binary ASN.1\n"
                            "3 = Genbank\n"
                            "4 = genpept\n"
                            "5 = FASTA\n"
                            "6 = quality scores\n"
                            "7 = Entrez DocSums\n"
                            "8 = FASTA reverse complement\n",
                            CArgDescriptions::eInteger,
                            "1");
    arg_desc->SetConstraint("t",
                            *(new CArgAllow_Integers(1, 8)));

    arg_desc->AddKey("g", "GI",
                     "GI to for single entry to process",
                     CArgDescriptions::eInteger);
    arg_desc->AddOptionalKey("s", "SeqId",
                             "Fasta style SeqId to process",
                             CArgDescriptions::eString);
    arg_desc->SetDependency("s",
                            CArgDescriptions::eExcludes,
                            "g");

    arg_desc->AddOptionalKey("q", "Query",
                             "Use an entrez query to generate the input list",
                             CArgDescriptions::eString);
    arg_desc->SetDependency("q",
                            CArgDescriptions::eExcludes,
                            "g");

    arg_desc->AddFlag("dp", "Query is for protein database");
    arg_desc->AddFlag("dn", "Query is for nucleotide database");


    // Add standard data loader support
    CDataLoadersUtil::AddArgumentDescriptions(*arg_desc);


    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


IOutputFormatter* CIdfetchApplication::x_GetFormatter(CNcbiOstream& ostr,
                                                      const CArgs& args)
{
    int type = args["t"].AsInteger();
    switch (type) {
    case eAsnText:
        return new COutput_SerialFormat(ostr, eSerial_AsnText);
    case eAsnBinary:
        return new COutput_SerialFormat(ostr, eSerial_AsnBinary);
    case eFASTA:
        return new COutput_Fasta(ostr);
    }

    return NULL;
}


/////////////////////////////////////////////////////////////////////////////
//  Run test (printout arguments obtained from command-line)


int CIdfetchApplication::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();

    CNcbiOstream& ostr = args["o"].AsOutputFile();

    //
    // object manager setup
    CRef<CObjectManager> om(CObjectManager::GetInstance());
    CDataLoadersUtil::SetupObjectManager(args, *om);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    // generate a formatter class
    auto_ptr<IOutputFormatter> formatter(x_GetFormatter(ostr, args));


    if (args["g"]  ||  args["s"]) {
        if ( !formatter.get() ) {
            NCBI_THROW(CException, eUnknown,
                       "formatter type not understood or not handled");
        }

        //
        // processing single sequence
        CSeq_id_Handle idh;
        if (args["g"]) {
            idh = CSeq_id_Handle::GetHandle(args["g"].AsInt8());
        }
        else if (args["s"]) {
            idh = CSeq_id_Handle::GetHandle(args["s"].AsString());
        }

        CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
        if ( !bsh ) {
            NCBI_THROW(CException, eUnknown,
                       "failed to retrieve sequence: " + idh.AsString());
        }

        formatter->Format(bsh);
    }
    else if (args["q"]) {
        if ( !formatter.get() ) {
            NCBI_THROW(CException, eUnknown,
                       "formatter type not understood or not handled");
        }

        string query = args["q"].AsString();

        vector<TGi> uids;
        CEutilsClient cli;
        if (args["dp"]) {
            cli.Search("protein", query, uids);
        }
        else if (args["dn"]) {
            cli.Search("nuccore", query, uids);
        }
        else {
            NCBI_THROW(CException, eUnknown,
                       "one of -dp or -dn must be provided to "
                       "indicate the database of interest");
        }

        ITERATE (vector<TGi>, it, uids) {
            CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*it);
            CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
            if ( !bsh ) {
                NCBI_THROW(CException, eUnknown,
                           "failed to retrieve sequence: " + idh.AsString());
            }

            formatter->Format(bsh);
        }
    }
    else {
        //
        // not yet supported
        //
        NCBI_THROW(CException, eUnknown,
                   "input options not yet supported");
    }


    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CIdfetchApplication::Exit(void)
{
    // Do your after-Run() cleanup here
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function; change argument list to
    // (argc, argv, 0, eDS_Default, 0) if there's no point in trying
    // to look for an application-specific configuration file.
    return CIdfetchApplication().AppMain(argc, argv);
}
