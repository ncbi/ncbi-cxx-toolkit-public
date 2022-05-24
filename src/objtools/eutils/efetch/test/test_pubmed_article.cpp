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
*           Aleksey Grichenko
*
* File Description:
*           Test CPubmedArticle converter.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <serial/serial.hpp>
#include <objtools/eutils/efetch/efetch__.hpp>


USING_NCBI_SCOPE;
USING_SCOPE(objects);


/////////////////////////////////////////////////////////////////////////////
//
//  CTestApplication::
//


class CTestApplication : public CNcbiApplication
{
public:
    void TestApp_Args(CArgDescriptions& args);

    virtual void Init(void);
    virtual int Run(void);
};


void CTestApplication::Init(void)
{
    unique_ptr<CArgDescriptions> args(new CArgDescriptions);
    args->AddOptionalKey("f", "file", "Input file name", CArgDescriptions::eInputFile);
    
    args->AddDefaultKey("ifmt", "inputformat", "Input format", CArgDescriptions::eString, "xml");
    args->SetConstraint("ifmt", &(*new CArgAllow_Strings, "xml", "asn", "asnb", "json"));

    args->AddDefaultKey("ofmt", "outputformat", "Output format", CArgDescriptions::eString, "asn");
    args->SetConstraint("ofmt", &(*new CArgAllow_Strings, "xml", "asn", "asnb", "json"));

    args->AddFlag("set", "Read CPubmedArticleSet");
    args->AddFlag("book", "Read CPubmedBookArticle objects");
    args->SetDependency("set", CArgDescriptions::eExcludes, "book");

    args->SetUsageContext(GetArguments().GetProgramBasename(), "test_pubmed_article", false);

    // Let test application add its own arguments
    SetupArgDescriptions(args.release());
}


template<class TA>
void ProcessArticle(const TA& article, const MSerial_Format& ofmt)
{
    auto entry = article.ToPubmed_entry();
    cout << ofmt << *entry;
}


MSerial_Format* GetSerialFormat(const string& fmt)
{
    if (fmt == "xml") return new MSerial_Format_Xml();
    if (fmt == "asn") return new MSerial_Format_AsnText();
    if (fmt == "asnb") return new MSerial_Format_AsnBinary();
    if (fmt == "json") return new MSerial_Format_Json();
    return nullptr;
}


int CTestApplication::Run(void)
{
    const auto& args = GetArgs();
    unique_ptr<MSerial_Format> ifmt(GetSerialFormat(args["ifmt"].AsString()));
    unique_ptr<MSerial_Format> ofmt(GetSerialFormat(args["ofmt"].AsString()));

    CNcbiIstream& in = args["f"].AsInputFile();
    if (args["set"]) {
        eutils::CPubmedArticleSet articles;
        in >> *ifmt >> articles;
        for (auto& it : articles.GetPP().GetPP()) {
            if (it->IsPubmedArticle()) {
                ProcessArticle<eutils::CPubmedArticle>(it->GetPubmedArticle(), *ofmt);
            }
            else if (it->IsPubmedBookArticle()) {
                ProcessArticle(it->GetPubmedBookArticle(), *ofmt);
            }
        }
    }
    else {
        if (args["book"].AsBoolean()) {
            eutils::CPubmedBookArticle book_article;
            in >> *ifmt >> book_article;
            ProcessArticle(book_article, *ofmt);
        }
        else {
            eutils::CPubmedArticle article;
            in >> *ifmt >> article;
            ProcessArticle(article, *ofmt);
        }
    }
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//
//  MAIN
//


int main(int argc, const char* argv[])
{
    return CTestApplication().AppMain(argc, argv);
}
