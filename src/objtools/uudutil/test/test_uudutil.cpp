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
* Author:  Liangshou Wu
*
* File Description:
*   Uunit tests file for CProjectStorage
*
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objtools/uudutil/project_storage.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_system.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/gbproj/GBProject_ver2.hpp>

#include <corelib/ncbifile.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>


USING_NCBI_SCOPE;
USING_SCOPE(objects);


static const int  kTTL = 60; // seconds
static const string kTestStr = "This is a very simple test string.";
static const CProjectStorage::ENC_Compression kDefComp = CProjectStorage::CProjectStorage::eNC_ZlibCompressed;

// global variables
static bool verbose = false;

NCBITEST_AUTO_INIT()
{
    // Your application initialization code here (optional)
    //
    // Useful function that can be used here:
    // if (some condition) {
    //     NcbiTestSetGlobalDisabled();
    //     return;
    // }
    const CArgs& args = CNcbiApplication::Instance()->GetArgs();
    verbose = args["verbose"];

    cout << "Initialization function executed" << endl;
}

NCBITEST_AUTO_FINI()
{
    // Your application finalization code here (optional)

    cout << "Finalization function executed" << endl;
}


NCBITEST_INIT_CMDLINE(arg_desc)
{
    // Describe command line parameters that we are going to use

    arg_desc->AddFlag(
        "verbose",
        "Verbose test prompts more testing status including some results");

    arg_desc->AddOptionalKey(
        "client",
        "nc_client",
        "NetCache client name. The default client name is NC_Test.",
        CArgDescriptions::eString);

    arg_desc->AddOptionalKey(
        "service",
        "nc_serivce",
        "NetCache service name. The default service name is "
        "NC_SV_UserData_TEST.",
        CArgDescriptions::eString);
}


NCBITEST_INIT_VARIABLES(conf_parser)
{
    // Initialize variables that will be used in conditions
    // in 'unit_test_alt_sample.ini'
    //const CArgs& args = CNcbiApplication::Instance()->GetArgs();

    // conf_parser->AddSymbol("enable_test_timeout",
    //                        !args["disable_TestTimeout"]);
}


NCBITEST_INIT_TREE()
{
    // Here we can set some dependencies between tests (if one disabled or
    // failed other shouldn't execute) and hard-coded disablings. Note though
    // that more preferable way to make always disabled test is add to
    // ini-file in UNITTESTS_DISABLE section this line:
    // TestName = true

    // NCBITEST_DEPENDS_ON(DependentOnArg, UsingArg);

    // NCBITEST_DISABLE(AlwaysDisabled);
}


static CProjectStorage s_GetPrjStorage(const string& password = "")
{
    const CArgs& args = CNcbiApplication::Instance()->GetArgs();

    string nc_service =  args["service"].HasValue() ?
        args["service"].AsString() : "NC_SV_UserData_TEST";
    if (nc_service[0] == '\'')
        nc_service = nc_service.substr(1, nc_service.length() - 1);
    if (nc_service[nc_service.length() - 1] == '\'')
        nc_service = nc_service.substr(0, nc_service.length() - 1);

    string nc_client = args["client"].HasValue() ?
        args["client"].AsString() : "NC_Test";

    return CProjectStorage(nc_client, nc_service, password);
}


/// Testing string blob
BOOST_AUTO_TEST_CASE(StringTest)
{
    CProjectStorage nc_tool = s_GetPrjStorage();

    if (verbose) {
        cout << "\n --- Testing string blob saving and retrieval --- " << endl;
    }

    string nc_key =
        nc_tool.SaveString(kTestStr, "", kDefComp, kTTL);

    // check if the saving succeeds
    BOOST_CHECK( !nc_key.empty() );
    if (verbose) {
        cout << "The saved NC key is: " << nc_key << endl;
    }

    // check if the nc key indeed exists
    BOOST_CHECK(nc_tool.Exists(nc_key));

    // test string blob retrieval
    string ret_str;
    nc_tool.GetString(nc_key, ret_str);
    BOOST_CHECK_EQUAL(kTestStr, ret_str);
    if (verbose) {
        cout << "The retrieved string is: " << ret_str << endl;
    }


    // test retrieve blob as a vector
    vector<char> ret_vec;
    nc_tool.GetVector(nc_key, ret_vec);
    NCBITEST_CHECK_EQUAL(ret_vec.size(), kTestStr.size());
    if (verbose) {
        //cout << "The retrieved vector<char> is: " << ret_vec.data() << endl;
    }

    // test nc blob removal
    BOOST_CHECK_NO_THROW(nc_tool.Delete(nc_key));

    // check if the nc key has been deleted
    BOOST_CHECK( !nc_tool.Exists(nc_key) );
}

/// Testing blob cloning
BOOST_AUTO_TEST_CASE(CloneTest)
{
    if (verbose) {
        cout << "\n --- Testing string blob cloning --- " << endl;
    }

    CProjectStorage nc_tool = s_GetPrjStorage();
    string nc_key =
        nc_tool.SaveString(kTestStr, "", kDefComp, kTTL);
    if (verbose) {
        cout << "The saved NC key is: " << nc_key << endl;
    }

    // check if the saving succeeds
    BOOST_CHECK( !nc_key.empty() );
    
    string dup_nc_key = nc_tool.Clone(nc_key, kTTL);
    if (verbose) {
        cout << "The cloned NC key is: " << dup_nc_key << endl;
    }

    // check if the cloning succeeds
    BOOST_CHECK( !dup_nc_key.empty() );

    // test string blob retrieval
    string ret_str;
    nc_tool.GetString(dup_nc_key, ret_str);
    if (verbose) {
        cout << "The retrieved cloned string is: " << ret_str << endl;
    }

    BOOST_CHECK_EQUAL(kTestStr, ret_str);

    // test nc blob removal
    BOOST_CHECK_NO_THROW(nc_tool.Delete(nc_key));

    // test nc blob removal
    BOOST_CHECK_NO_THROW(nc_tool.Delete(dup_nc_key));
}


/// Testing password protection
BOOST_AUTO_TEST_CASE(PasswordProtectionTest)
{
    CProjectStorage nc_tool = s_GetPrjStorage("my password");

    if (verbose) {
        cout << "\n --- Testing password-protected blob --- "
             << endl;
    }

    string nc_key =
        nc_tool.SaveString(kTestStr, "", kDefComp, kTTL);

    // check if the saving succeeds
    BOOST_CHECK( !nc_key.empty() );
    if (verbose) {
        cout << "The saved NC key is: " << nc_key << endl;
    }

    // check if the nc key indeed exists
    BOOST_CHECK(nc_tool.Exists(nc_key));

    string ret_str;

    // test failure on checking password-protected blob with no password.
    CProjectStorage failed_nc_tool1 = s_GetPrjStorage();
    BOOST_CHECK( !failed_nc_tool1.Exists(nc_key) );
    BOOST_CHECK_THROW(
        failed_nc_tool1.GetString(nc_key, ret_str), CPrjStorageException);

    // test failure on checking password-protected blob with a wrong password.
    CProjectStorage failed_nc_tool2 = s_GetPrjStorage("wrong password");
    BOOST_CHECK( !failed_nc_tool2.Exists(nc_key) );
    BOOST_CHECK_THROW(
        failed_nc_tool2.GetString(nc_key, ret_str), CPrjStorageException);

    // test string blob retrieval
    nc_tool.GetString(nc_key, ret_str);
    BOOST_CHECK_EQUAL(kTestStr, ret_str);
    if (verbose) {
        cout << "The retrieved string is: " << ret_str << endl;
    }

    // test nc blob removal
    BOOST_CHECK_NO_THROW(nc_tool.Delete(nc_key));

    // check if the nc key has been deleted
    BOOST_CHECK( !nc_tool.Exists(nc_key) );
}


/// Testing raw data blob
BOOST_AUTO_TEST_CASE(RawDataTest)
{
    if (verbose) {
        cout << "\n --- Testing raw data blob saving and retrieval --- "
             << endl;
    }

    CProjectStorage nc_tool = s_GetPrjStorage();
    string nc_key =
        nc_tool.SaveRawData(kTestStr.data(), kTestStr.size(), "", kTTL);
    if (verbose) {
        cout << "The saved NC key is: " << nc_key << endl;
    }

    // check if the saving succeeds
    BOOST_CHECK( !nc_key.empty() );

    // check if the nc key indeed exists
    BOOST_CHECK(nc_tool.Exists(nc_key));
    
    // test string blob retrieval
    string ret_str;
    auto_ptr<CNcbiIstream> istr = nc_tool.GetIstream(nc_key, true);
    string line;
    while (!istr->eof()) {
        getline(*istr, line);
        ret_str += line;
    }

    if (verbose) {
        cout << "The retrieved raw string is: " << ret_str << endl;
    }

    // test if the retrieved string is identical to the original one
    BOOST_CHECK_EQUAL(kTestStr, ret_str);

    // test nc blob removal
    BOOST_CHECK_NO_THROW(nc_tool.Delete(nc_key));
}


static const string kSeqAnnotAsnFile = "test_align_annot.asn";

static CRef<CSeq_annot> s_ReadTestSeqAnnot()
{
    auto_ptr<CObjectIStream>
        istr(CObjectIStream::Open(kSeqAnnotAsnFile, eSerial_AsnText));
    CRef<CSeq_annot> annot(new CSeq_annot);
    if (istr.get()  &&  istr->InGoodState()) {
        *istr >> *annot;
    } else {
        NCBI_THROW(CException, eUnknown, "Can't open the seq-annot asn file");
    }
    
    return annot;
}


static void s_TestSeqAnnotAsn(ESerialDataFormat serial_fmt,
                              CProjectStorage::ENC_Compression compression_fmt)
{
    CProjectStorage nc_tool = s_GetPrjStorage();
    CRef<CSeq_annot> annot;
    BOOST_CHECK_NO_THROW(annot = s_ReadTestSeqAnnot());

    string nc_key = nc_tool.SaveObject(*annot, "",
                                       compression_fmt,
                                       serial_fmt,
                                       kTTL);

    // check if the saving succeeds
    BOOST_CHECK( !nc_key.empty() );
    if (verbose) {
        cout << "The saved NC key is: " << nc_key << endl;
    }

    // check if the nc key indeed exists
    BOOST_CHECK(nc_tool.Exists(nc_key));

    // test ASN blob retrieval
    CRef<CSerialObject> object = nc_tool.GetObject(nc_key);
    BOOST_CHECK(object.NotNull());

    annot.Reset(dynamic_cast<CSeq_annot*>(object.GetPointer()));
    BOOST_CHECK(annot.NotNull());

    // save the seq-annot to a temp file and compare
    CTmpFile tmp_file;

    if (verbose) {
        cout << MSerial_AsnText << *annot << endl;
    }

    auto_ptr<CObjectOStream> ostr(
        CObjectOStream::Open(eSerial_AsnText,
                             tmp_file.AsOutputFile(
                                 CTmpFile::eIfExists_ReturnCurrent)));
    *ostr << *annot;
    ostr->Close();

    CFile orig_file(kSeqAnnotAsnFile);
    BOOST_CHECK(orig_file.CompareTextContents(tmp_file.GetFileName(), CFile::eIgnoreEol));

    // test nc blob removal
    BOOST_CHECK_NO_THROW(nc_tool.Delete(nc_key));

    // check if the nc key has been deleted
    BOOST_CHECK( !nc_tool.Exists(nc_key) );
}


/// Testing ASN text object (seq-annot) blob
BOOST_AUTO_TEST_CASE(AsnTextObjectTest)
{
    if (verbose) {
        cout << "\n --- Testing ASN Text blob saving and retrieval --- "
             << endl;
    }
    s_TestSeqAnnotAsn(eSerial_AsnText, CProjectStorage::eNC_Uncompressed);
}


/// Testing ASN binary object (seq-annot) blob
BOOST_AUTO_TEST_CASE(AsnBinaryObjectTest)
{
    if (verbose) {
        cout << "\n --- Testing ASN binary blob saving and retrieval --- "
             << endl;
    }
    s_TestSeqAnnotAsn(eSerial_AsnBinary, CProjectStorage::eNC_Uncompressed);
}


/// Testing data compression (gzip)
BOOST_AUTO_TEST_CASE(AsnBinaryGZipTest)
{
    if (verbose) {
        cout << "\n --- Testing ASN binary blob (gzip) --- "
             << endl;
    }
    s_TestSeqAnnotAsn(eSerial_AsnBinary, kDefComp);
}

/// Testing data compression (bzip2)
BOOST_AUTO_TEST_CASE(AsnBinaryBzip2Test)
{
    if (verbose) {
        cout << "\n --- Testing ASN binary blob (bzip2) --- " << endl;
    }
    s_TestSeqAnnotAsn(eSerial_AsnBinary, CProjectStorage::eNC_Bzip2Compressed);
}

/// Testing data compression (lzo)
BOOST_AUTO_TEST_CASE(AsnBinaryLzoTest)
{
    if (verbose) {
        cout << "\n --- Testing ASN binary blob (lzo) --- " << endl;
    }
#if defined(HAVE_LIBLZO)
    s_TestSeqAnnotAsn(eSerial_AsnBinary, CProjectStorage::eNC_LzoCompressed);
#endif //HAVE_LIBLZO
}

/// Testing saving seq-annot as raw string data
/// and retrieve it using the GetAnnots()
BOOST_AUTO_TEST_CASE(SeqAnnotRawDataTest)
{
    if (verbose) {
        cout << "\n --- Testing raw seq-annot blob saving and retrieval --- "
             << endl;
    }

    CProjectStorage nc_tool = s_GetPrjStorage();

    // read seq-annot into memory
    CRef<CSeq_annot> annot;
    BOOST_CHECK_NO_THROW(annot = s_ReadTestSeqAnnot());

    // convert it as a (ASN binary) string
    std::ostringstream str_str;
    str_str << MSerial_AsnBinary << *annot;
    string annot_str = str_str.str();

    // save it as raw data (no header)
    string nc_key = nc_tool.SaveRawData(annot_str.data(),
                                        annot_str.size(),
                                        "",
                                        kTTL);

    // check if the saving succeeds
    BOOST_CHECK( !nc_key.empty() );
    if (verbose) {
        cout << "The saved NC key is: " << nc_key << endl;
    }

    // check if the nc key indeed exists
    BOOST_CHECK(nc_tool.Exists(nc_key));

    // test ASN blob retrieval using GetAnnots()
    CProjectStorage::TAnnots annots = nc_tool.GetAnnots(nc_key);
    BOOST_CHECK(annots.size() == 1);
    BOOST_CHECK(annots.front().NotNull());

    // save the seq-annot to a temp file and compare

    const CSeq_annot& ret_annot = *annots.front();
    CTmpFile tmp_file;
    if (verbose) {
        cout << MSerial_AsnText << ret_annot << endl;
    }

    auto_ptr<CObjectOStream> ostr(
        CObjectOStream::Open(eSerial_AsnText,
                             tmp_file.AsOutputFile(
                                 CTmpFile::eIfExists_ReturnCurrent)));
    *ostr << ret_annot;
    ostr->Close();

    CFile orig_file(kSeqAnnotAsnFile);
    BOOST_CHECK(orig_file.CompareTextContents(tmp_file.GetFileName(), CFile::eIgnoreEol));

    // test nc blob removal
    BOOST_CHECK_NO_THROW(nc_tool.Delete(nc_key));

    // check if the nc key has been deleted
    BOOST_CHECK( !nc_tool.Exists(nc_key) );
}


static const string kGBProjectFile = "test_gbproject.gbp";

static CRef<CGBProject_ver2> s_ReadGBProject()
{
    auto_ptr<CObjectIStream>
        istr(CObjectIStream::Open(kGBProjectFile, eSerial_AsnBinary));
    CRef<CGBProject_ver2> project(new CGBProject_ver2);
    if (istr.get()  &&  istr->InGoodState()) {
        *istr >> *project;
    } else {
        NCBI_THROW(CException, eUnknown, "Can't open the GBProject file");
    }

    return project;
}


BOOST_AUTO_TEST_CASE(GBProjectTest)
{
    if (verbose) {
        cout << "\n --- Testing GB Project saving and retrieval --- " << endl;
    }

    CProjectStorage nc_tool = s_GetPrjStorage();
    CRef<CGBProject_ver2> gb_project = s_ReadGBProject();

    if (verbose) {
        cout << MSerial_AsnText << *gb_project << endl;
    }

    string nc_key;
    BOOST_CHECK_NO_THROW(
        nc_key = nc_tool.SaveProject(*gb_project, "",
                                     kDefComp,
                                     eSerial_AsnBinary,
                                     kTTL));

    // check if the saving succeeds
    BOOST_CHECK( !nc_key.empty() );
    if (verbose) {
        cout << "The saved NC key is: " << nc_key << endl;
    }

    // check if the nc key indeed exists
    BOOST_CHECK(nc_tool.Exists(nc_key));

    // test ASN blob retrieval
    CRef<CSerialObject> object = nc_tool.GetObject(nc_key);
    BOOST_CHECK(object.NotNull());

    gb_project.Reset(dynamic_cast<CGBProject_ver2*>(object.GetPointer()));
    BOOST_CHECK(gb_project.NotNull());

    // save the seq-annot to a temp file and compare
    CTmpFile tmp_file;

    if (verbose) {
        cout << MSerial_AsnText << *gb_project << endl;
    }

    CNcbiOstream& out_str = tmp_file.AsOutputFile(CTmpFile::eIfExists_ReturnCurrent);
    out_str << MSerial_AsnBinary << *gb_project;

    CFile orig_file(kGBProjectFile);
    BOOST_CHECK(orig_file.Compare(tmp_file.GetFileName()));

    // test GetAnnots() method
    CProjectStorage::TAnnots annots;
    BOOST_CHECK_NO_THROW(annots = nc_tool.GetAnnots(nc_key));
    BOOST_CHECK_EQUAL(annots.size(), (size_t)2);
    if (verbose) {
        ITERATE (CProjectStorage::TAnnots, iter, annots) {
            cout << MSerial_AsnText << **iter << endl;
        }
    }

    // test nc blob removal
    BOOST_CHECK_NO_THROW(nc_tool.Delete(nc_key));

    // check if the nc key has been deleted
    BOOST_CHECK( !nc_tool.Exists(nc_key) );
    
}
