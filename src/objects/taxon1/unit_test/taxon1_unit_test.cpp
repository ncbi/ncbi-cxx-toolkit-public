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
* Author: Michael Domrachev
*
* File Description:
*   Sample unit tests file for the TaxArch C to C++ wrapper
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbiapp.hpp>

#include <objects/taxon1/taxon1.hpp>

#include <serial/serial.hpp>
#include <serial/objostr.hpp>
#include <serial/objistr.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>


USING_NCBI_SCOPE;
USING_SCOPE(objects);

CTaxon1 tax1;
ESerialDataFormat format = eSerial_AsnBinary;

NCBITEST_AUTO_INIT()
{
    // Your application initialization code here (optional)
}


NCBITEST_INIT_CMDLINE(arg_desc)
{
    // Describe command line parameters that we are going to use
    arg_desc->AddDefaultKey("service",
                            "service_name",
                            "Taxonomy service name",
                            CArgDescriptions::eString,
                            "TaxService4");
}



NCBITEST_AUTO_FINI()
{
    // Your application finalization code here (optional)
    tax1.Fini();
}

BOOST_AUTO_TEST_SUITE(taxon1)

NCBITEST_INIT_TREE()
{
    // Here we can set some dependencies between tests (if one disabled or
    // failed other shouldn't execute) and hard-coded disablings. Note though
    // that more preferable way to make always disabled test is add to
    // ini-file in UNITTESTS_DISABLE section this line:
    // TestName = true

    NCBITEST_DEPENDS_ON(test_tax1_getbyid, test_tax1_init);
    NCBITEST_DEPENDS_ON(test_tax1m_getbyid, test_tax1_init);
    NCBITEST_DEPENDS_ON(test_tax1_getTaxIdByOrgRef, test_tax1_init);
    NCBITEST_DEPENDS_ON(test_tax1_getAllTaxIdByName, test_tax1_init);
    NCBITEST_DEPENDS_ON(test_tax1_findTaxIdByName, test_tax1_init);
    NCBITEST_DEPENDS_ON(test_tax1_getOrgRef, test_tax1_init);
    NCBITEST_DEPENDS_ON(test_tax1e_maxTaxId, test_tax1_init);
    NCBITEST_DEPENDS_ON(test_tax1_lookup, test_tax1_init);
    NCBITEST_DEPENDS_ON(test_tax1m_lookup, test_tax1_init);
    NCBITEST_DEPENDS_ON(test_tax1_getTaxIdByName, test_tax1_init);
    NCBITEST_DEPENDS_ON(test_tax1_getParent, test_tax1_init);
    NCBITEST_DEPENDS_ON(test_tax1_getGenus, test_tax1_init);
    NCBITEST_DEPENDS_ON(test_tax1_join, test_tax1_init);
    NCBITEST_DEPENDS_ON(test_tax1_getTaxId4GI, test_tax1_init);
    NCBITEST_DEPENDS_ON(test_tax1_getChildren, test_tax1_init);
    NCBITEST_DEPENDS_ON(test_tax1_getGCName, test_tax1_init);
    NCBITEST_DEPENDS_ON(test_tax1_getAllNames, test_tax1_init);
    NCBITEST_DEPENDS_ON(test_tax1m_getBlastName, test_tax1_init);
    NCBITEST_DEPENDS_ON(test_tax1e_needUpdate, test_tax1_init);
    NCBITEST_DEPENDS_ON(test_tax1m_getRankByName, test_tax1_init);
    NCBITEST_DEPENDS_ON(test_tax1m_getDivByName, test_tax1_init);

    //    NCBITEST_DISABLE(AlwaysDisabled);
}

BOOST_AUTO_TEST_CASE(test_tax1_init) {
    if( !CNcbiApplication::Instance()->GetArgs()["service"].AsString().empty() ) {
	CNcbiApplication::Instance()->SetEnvironment().Set("NI_SERVICE_NAME_TAXONOMY",
							   CNcbiApplication::Instance()->GetArgs()["service"].AsString());
    }

    bool b = tax1.Init();
    BOOST_REQUIRE_MESSAGE( b == true, "Unable to initialize taxon1" );
}

BOOST_AUTO_TEST_CASE(test_tax1e_maxTaxId) {
    Int4 b = tax1.GetMaxTaxId();
    BOOST_REQUIRE_MESSAGE( b > 0, "Unable to get max taxid" );
}

BOOST_AUTO_TEST_CASE(test_tax1_getbyid)
{
    int taxid  = 9606;

    CRef< CTaxon2_data > p = tax1.GetById( taxid );

    BOOST_REQUIRE_MESSAGE( p != NULL, "No human taxid found" );
    BOOST_REQUIRE( p->IsSetOrg() != false );
    BOOST_REQUIRE( p->GetOrg().IsSetTaxname() != false );
    BOOST_REQUIRE_MESSAGE( strcmp(p->GetOrg().GetTaxname().c_str(),"Homo sapiens") == 0, "Human taxonomy name is not Homo sapiens" );

    // If this check fails, test will continue its execution
//     BOOST_CHECK_EQUAL(i,  1);
//     // If this check fails, test will stop its execution at this point
//     BOOST_REQUIRE_EQUAL(d,  0.123);
//     // If something during checking can throw an exception and you don't want
//     // it to do that you can use different macros:
//     NCBITEST_CHECK_EQUAL(s1, s2);
//     // ... or
//     NCBITEST_CHECK(s1 == s2);    
//     // ... or
//     NCBITEST_CHECK_MESSAGE(s1 == s2, "Object s1 not equal object s2");

//     // Never use it this way, because it will not compile on WorkShop:
//     //    BOOST_CHECK_EQUAL(s1, "qwerty");
//     // Instead, use it this way:
//     BOOST_CHECK(s1 == "qwerty");
//     // ...or this way:
//     BOOST_CHECK_EQUAL(s1, string("qwerty"));
}

BOOST_AUTO_TEST_CASE(test_tax1m_getbyid)
{
    int taxid  = 9606;
    string sName;

    bool p = tax1.GetScientificName( taxid, sName );

    BOOST_REQUIRE_MESSAGE( p == true, "No human taxid found" );
    BOOST_REQUIRE_MESSAGE( sName == "Homo sapiens", "Human scientific name is not Homo sapiens" );

}

static CRef< COrg_ref >
s_String2OrgRef( const string& orgref_in )
{
    unique_ptr< CObjectIStream > is( CObjectIStream::CreateFromBuffer( eSerial_AsnText, orgref_in.data(), orgref_in.size() ) );
    //try {
    CRef< COrg_ref > orp( new COrg_ref );
    *is >> *orp;
	//} catch(...)
    return orp;
}

static string // empty if unable to write orgref
s_OrgRef2String( const COrg_ref& orgref )
{
    stringstream ss;
    unique_ptr< CObjectOStream > os( CObjectOStream::Open( eSerial_AsnText, ss ) );
    os->SetUseEol(false); // make it a single line
    os->SetSeparator(""); // ... without endline character
    *os << orgref;
    return ss.str();
}


BOOST_AUTO_TEST_CASE(test_tax1_getTaxIdByOrgRef)
{
    CRef< COrg_ref > orp = s_String2OrgRef( "Org-ref ::= { taxname \"Homo sapiens\" }" );
    BOOST_REQUIRE( orp != NULL );

    int taxid  = tax1.GetTaxIdByOrgRef( *orp );

    BOOST_REQUIRE( taxid == 9606 );

    
    // test not found
    orp = s_String2OrgRef( "Org-ref ::= { taxname \"Hdfjkhkda jdkfkjd\" }" );
    taxid  = tax1.GetTaxIdByOrgRef( *orp );

    //    BOOST_CHECK_MESSAGE( taxid < 0, "Retrieve error" );
    BOOST_REQUIRE( taxid == 0 );

    
}

/*----------------------------------------------
 * Get tax_id by organism name
 * Returns: tax_id - if organism found
 *          0 - no organism found
 *          -tax_id - if multiple nodes found (where tax_id is id of one of the nodes)
 * NOTE:
 * orgname can be a regular expression.
 */

BOOST_AUTO_TEST_CASE(test_tax1_getTaxIdByName)
{
    string name = ("Homo sapiens");
    Int4 taxid = tax1.GetTaxIdByName( name );
    BOOST_REQUIRE( taxid == 9606 );
    

    name = ("Hlknadflk asjkfh");
    taxid = tax1.GetTaxIdByName( name );
    BOOST_REQUIRE( taxid == 0 );
    

    name = ("bacteria");
    taxid = tax1.GetTaxIdByName( name );
    BOOST_REQUIRE( taxid < 0 );
    
}

//----------------------------------------------
// Get tax_id by organism "unique" name
// Returns: tax_id - if organism found
//               0 - no organism found
//              -1 - error during processing occured
//         -tax_id - if multiple nodes found
//                   (where tax_id > 1 is id of one of the nodes)
///

BOOST_AUTO_TEST_CASE(test_tax1_findTaxIdByName)
{
    string name = ("Mus musculus");
    Int4 taxid = tax1.FindTaxIdByName( name );
    BOOST_REQUIRE( taxid == 10090 );
    

    name = ("Hlknadflk asjkfh");
    taxid = tax1.FindTaxIdByName( name );
    BOOST_REQUIRE( taxid == 0 );
    

    name = ("Bacillus");
    taxid = tax1.FindTaxIdByName( name );
    BOOST_REQUIRE( taxid < 0 );
    
}

/*----------------------------------------------
 * Get ALL tax_id by organism name
 * Returns: number of organism found
 * NOTE:
 * 1. orgname can be a regular expression.
 * 2. Ids consists of tax ids. Caller is responsible to free this memory
 */

BOOST_AUTO_TEST_CASE(test_tax1_getAllTaxIdByName)
{
    string name = ("Drosophila");
    CTaxon1::TTaxIdList p;
    Int4 nof = tax1.GetAllTaxIdByName( name, p );
    BOOST_CHECK( nof >= 2 );
    BOOST_REQUIRE( p.empty() != true );
    BOOST_REQUIRE( find(p.begin(), p.end(), 7215) != p.end() );

    // Check not found too
    p.clear();
    name = ("Hlknadflk asjkfh");
    nof = tax1.GetAllTaxIdByName( name, p );
    BOOST_REQUIRE( nof == 0 );
}

/*----------------------------------------------
 * Get organism by tax_id
 * Returns: pointer to OrgRef structure if organism found
 *          NULL - if no such organism in taxonomy database
 * NOTE:
 * This function does not make a copy of OrgRef structure, so, caller can not use
 * OrgRefFree function for returned pointer.
 */
BOOST_AUTO_TEST_CASE(test_tax1_getOrgRef)
{
    bool is_species = false;
    bool is_uncultured = false;
    bool is_specified = false;
    string blast_name;
    
    CConstRef< COrg_ref > orp = tax1.GetOrgRef(9606, is_species, is_uncultured, blast_name, &is_specified);
    BOOST_REQUIRE( orp != NULL );
    BOOST_CHECK( is_species != false );
    BOOST_CHECK( is_uncultured == false );
    BOOST_CHECK( is_specified != false );
    cout << blast_name << endl;
    BOOST_CHECK( blast_name == "primates" );

    string s = s_OrgRef2String( *orp );
    BOOST_CHECK( s.find("taxname \"Homo sapiens\"") != string::npos );
    
    // test not found
    orp = tax1.GetOrgRef(999999999, is_species, is_uncultured, blast_name, &is_specified);
    BOOST_REQUIRE( orp == NULL );
}

/*----------------------------------------------
 * Get organism by OrgRef
 * Returns: pointer to Taxon1Data if organism exists
 *          NULL - if no such organism in taxonomy database
 *
 * NOTE:
 * 1. This functions uses the following data from inp_orgRef to find organism
 *    in taxonomy database. It uses taxname first. If no organism was found (or multiple nodes found)
 *    then it tryes to find organism using common name. If nothing found, then it tryes to find
 *    organism using synonyms. tax1_lookup never uses tax_id to find organism.
 * 2. If merge == 0 this function makes the new copy of OrgRef structure and puts into it data
 *    from taxonomy database.
 *    If merge != 0 this function updates inp_orgRef structure.
 */
//extern "C" Taxon1DataPtr tax1.lookup(CRef< COrg_ref > inp_orgRef, int merge)
BOOST_AUTO_TEST_CASE(test_tax1_lookup)
{
    // Simple lookup, no modifier forwarding
    CRef< COrg_ref > orp = s_String2OrgRef( "Org-ref ::= { taxname \"Homo sapiens\" }" );
    BOOST_REQUIRE( orp != NULL );

    {
    CRef< CTaxon2_data > t1p = tax1.Lookup( *orp );
    BOOST_REQUIRE( t1p != NULL );
    BOOST_CHECK( t1p->IsSetOrg() != false );
    BOOST_CHECK( t1p->GetOrg().IsSetOrgname() != false );
    string s = s_OrgRef2String( t1p->GetOrg() );
    cout << s << endl;
    BOOST_CHECK( NStr::MatchesMask(s,"*taxname \"Homo sapiens\"*db \"taxon\"*,*tag* id 9606}*") );
    
    // Simple lookup, test not found
    orp.Reset( s_String2OrgRef( "Org-ref ::= { taxname \"Hkdsglkg sdlgldks\" }" ) );
    t1p.Reset( tax1.Lookup( *orp ) );
    BOOST_REQUIRE( t1p == NULL );
  
    // Simple lookup merge, no modifier forwarding
    //orp = s_String2OrgRef( "Org-ref ::= { taxname \"Homo sapiens\", orgname { mod {{subtype strain, subname \"denisovan\"}} } }" );
    orp.Reset( s_String2OrgRef( "Org-ref ::= {taxname \"Eptatretus lophehliae\" ,  db {              { db \"taxon\" ,  tag id 1314881 } } , orgname { name binomial { genus \"Eptatretus\" , species \"lophehliae\" } , mod { { subtype specimen-voucher , subname \"NRM 60920-t7566\" } ,  { subtype synonym , subname \"Eptatretus lopheliae\" } , { subtype old-name , subname \"Rubicundus lopheliae\" } } , lineage \"Eukaryota; Metazoa; Chordata; Craniata; Hyperotreti; Myxiniformes; Myxinidae; Eptatretinae; Eptatretus\" , gcode 1 , mgcode 2 , div \"VRT\" } }" ) );
    BOOST_REQUIRE( orp != NULL );
    }

    CConstRef< CTaxon2_data > t1p = tax1.LookupMerge( *orp );
    BOOST_REQUIRE( t1p != NULL );
    BOOST_CHECK( t1p->IsSetOrg() != false );
    BOOST_CHECK( t1p->GetOrg().IsSetOrgname() != false );
//     BOOST_CHECK( t1p->is_species_level != 0 );
//     BOOST_CHECK( strcmp( t1p->div, "PRI" ) == 0 );
//     BOOST_CHECK( strcmp( t1p->embl_code, "HS" ) == 0 );
    string s = s_OrgRef2String( *orp );

    //    BOOST_CHECK( s.find("subname \"denisovan\"") != string::npos );

    // -- no need, a part of Taxon1Data
}

/*----------------------------------------------
 * Get organism by OrgRef
 * Returns: pointer to Taxon1Data if organism exists
 *          NULL - if no such organism in taxonomy database
 *
 * NOTE:
 * 1. This functions uses the following data from inp_orgRef to find organism
 *    in taxonomy database. It uses taxname first. If no organism was found (or multiple nodes found)
 *    then it tryes to find organism using common name. If nothing found, then it tryes to find
 *    organism using synonyms. tax1_lookup never uses tax_id to find organism.
 * 2. If merge == 0 this function makes the new copy of OrgRef structure and puts into it data
 *    from taxonomy database.
 *    If merge != 0 this function updates inp_orgRef structure.
 */

BOOST_AUTO_TEST_CASE(test_tax1m_lookup_tm)
{
    CRef< COrg_ref > orp( s_String2OrgRef( "Org-ref ::= { taxname \"Aeromonas ichthiosmia\",   db { { db \"taxlookup%version\" , tag id 2 } } , orgname { mod { {subtype strain, subname \"MTCC 3249\"}, {subtype old-name, subname \"Aeromonas hybridization group 10 (HG10)\"} } } }" ) );
    BOOST_REQUIRE( orp != NULL );

    CConstRef< CTaxon2_data > t2p = tax1.LookupMerge( *orp );
    BOOST_REQUIRE( t2p != NULL );
    BOOST_CHECK( t2p->IsSetOrg() != false );
    BOOST_CHECK( t2p->GetOrg().IsSetOrgname() != false );
    BOOST_CHECK( t2p->GetIs_species_level() != 0 );
    BOOST_CHECK( t2p->GetIs_uncultured() == 0 );
    BOOST_CHECK( t2p->GetBlast_name().empty() != true );
    string s = s_OrgRef2String( t2p->GetOrg() );
    cout << s << endl;
    BOOST_CHECK( NStr::MatchesMask(s,"*mod*{*subtype type-material,*subname \"type strain of Aeromonas culicicola\"*}*}*") );

    orp.Reset( s_String2OrgRef( "Org-ref ::= { taxname \"Bipolaris maydis\",   db { { db \"taxlookup%version\" , tag id 2 } } , orgname { mod { {subtype isolate, subname \"CBS137271\"} } } }" ) );
    BOOST_REQUIRE( orp != NULL );

    t2p = tax1.LookupMerge( *orp );
    BOOST_REQUIRE( t2p != NULL );
    BOOST_CHECK( t2p->IsSetOrg() != false );
    BOOST_CHECK( t2p->GetOrg().IsSetOrgname() != false );
    BOOST_CHECK( t2p->GetIs_species_level() != 0 );
    BOOST_CHECK( t2p->GetIs_uncultured() == 0 );
    BOOST_CHECK( t2p->GetBlast_name().empty() != true );
    s = s_OrgRef2String( t2p->GetOrg() );
    cout << s << endl;
    //BOOST_CHECK( NStr::MatchesMask(s,"*taxname \"Homo sapiens neanderthalensis\"*db \"taxon\" ,\n      tag\n        id 63221 }*") );

}

BOOST_AUTO_TEST_CASE(test_tax1m_lookup)
{
    // Old-name driven lookup, no modifier forwarding
    CRef< COrg_ref > orp( s_String2OrgRef( "Org-ref ::= { taxname \"Homo sapiens\", orgname { mod { {subtype sub-species, subname \"neanderthalensis\"}, {subtype old-name, subname \"Homo sapiens neanderthalensis\"} } } }" ) );
    BOOST_REQUIRE( orp != NULL );

    CRef< CTaxon2_data > ct2p = tax1.Lookup( *orp );
    BOOST_REQUIRE( ct2p != NULL );
    BOOST_CHECK( ct2p->IsSetOrg() != false );
    BOOST_CHECK( ct2p->GetOrg().IsSetOrgname() != false );
    BOOST_CHECK( ct2p->GetIs_species_level() != 0 );
    BOOST_CHECK( ct2p->GetIs_uncultured() == 0 );
    BOOST_CHECK( ct2p->GetBlast_name().empty() != true );
    string s = s_OrgRef2String( ct2p->GetOrg() );
    cout << s << endl;
    BOOST_CHECK( NStr::MatchesMask(s,"*taxname \"Homo sapiens neanderthalensis\"*db \"taxon\"*,*tag* id 63221}*") );
    
    // Simple lookup, test not found
    orp.Reset( s_String2OrgRef( "Org-ref ::= { taxname \"Hkdsglkg sdlgldks\" }" ) );
    ct2p = tax1.Lookup( *orp );
    BOOST_REQUIRE( ct2p == NULL );
  
    // Simple lookup merge, no modifier forwarding
    orp = s_String2OrgRef( "Org-ref ::= { taxname \"Homo sapiens\", orgname { mod { {subtype strain, subname \"denisovan\"} } } }" );
    BOOST_REQUIRE( orp != NULL );

    CConstRef< CTaxon2_data > t2p = tax1.LookupMerge( *orp );
    BOOST_REQUIRE( t2p != NULL );
    BOOST_CHECK( t2p->IsSetOrg() != false );
    BOOST_CHECK( t2p->GetOrg().IsSetOrgname() != false );
    BOOST_CHECK( t2p->GetIs_species_level() != 0 );
    BOOST_CHECK( t2p->GetIs_uncultured() == 0 );
    BOOST_CHECK( t2p->GetBlast_name().empty() != true );
    s = s_OrgRef2String( *orp );

    BOOST_CHECK( s.find("subname \"denisovan\"") != string::npos );
}


BOOST_AUTO_TEST_CASE(test_tax1_getParent)
{
    Int4 taxid = tax1.GetParent( 9606 );
    BOOST_REQUIRE( taxid == 9605 );

    taxid = tax1.GetParent( 999999999 );
    BOOST_REQUIRE( taxid == 0 );

    taxid = tax1.GetParent( 1 );
    BOOST_REQUIRE( taxid == 0 );
}

BOOST_AUTO_TEST_CASE(test_tax1_getGenus)
{
    Int4 taxid = tax1.GetGenus( 9606 );
    BOOST_REQUIRE( taxid == 9605 );

    taxid = tax1.GetGenus( 999999999 );
    BOOST_REQUIRE( taxid <= 0 );

    taxid = tax1.GetGenus( 1 );
    BOOST_REQUIRE( taxid <= 0 );
}

BOOST_AUTO_TEST_CASE(test_tax1_getChildren)
{
    CTaxon1::TTaxIdList children;
    int nof = tax1.GetChildren( 9606, children );
    BOOST_REQUIRE( children.empty() != true );
    BOOST_REQUIRE( nof >= 2 );
    BOOST_REQUIRE( find(children.begin(),children.end(),63221) != children.end() );
    BOOST_REQUIRE( find(children.begin(),children.end(),741158) != children.end() );
    
    children.clear();
    nof = tax1.GetChildren( 999999999, children );
    BOOST_REQUIRE( nof <= 0 );
}

BOOST_AUTO_TEST_CASE(test_tax1_getGCName)
{
    string gc;
    bool b = tax1.GetGCName( 1, gc );
    BOOST_REQUIRE( b != false );
    BOOST_REQUIRE( gc != "" );
    cout << '"' << gc << '"' << endl;
    BOOST_REQUIRE( gc == "Standard" );
    
    b = tax1.GetGCName( 9999, gc );
    BOOST_REQUIRE( b == false );
}

BOOST_AUTO_TEST_CASE(test_tax1_join)
{
    Int4 taxid = tax1.Join( 9606, 10090 );
    BOOST_REQUIRE( taxid == 314146 );

    taxid = tax1.Join( 9606, 11676 );
    BOOST_REQUIRE( taxid == 1 );
}

BOOST_AUTO_TEST_CASE(test_tax1_getAllNames)
{
    CTaxon1::TNameList nl;
    Int2 nof = tax1.GetAllNames( 9606, nl, false );
    BOOST_REQUIRE( nl.empty() != true );
    BOOST_REQUIRE( find(nl.begin(),nl.end(),"Homo sapiens") != nl.end() );

    nl.clear();
    Int2 nof1 = tax1.GetAllNames( 9606, nl, true );
    BOOST_REQUIRE( nl.empty() != true );
    BOOST_CHECK( nof ==  nof1 );
    BOOST_REQUIRE( find(nl.begin(),nl.end(),"Homo sapiens") != nl.end() );

    nl.clear();
    nof = tax1.GetAllNames( 999999999, nl, true );
    BOOST_REQUIRE( nof <= 0 );
}

BOOST_AUTO_TEST_CASE(test_tax1_getTaxId4GI)
{
    Int4 taxid = 0;
    bool b = tax1.GetTaxId4GI( 1000, taxid );
    BOOST_REQUIRE( b == true );
    BOOST_REQUIRE( taxid == 9749 );
    // Not found
    b = tax1.GetTaxId4GI( 0, taxid );
    BOOST_REQUIRE( b == true );
    BOOST_REQUIRE( taxid == 0 );

    b = tax1.GetTaxId4GI( 0x100000000LL + 1000, taxid );
    BOOST_REQUIRE( b == true );
    if( ncbi::NStr::EndsWith( CNcbiApplicationAPI::Instance()->GetArgs()["service"].AsString(), "dev", ncbi::NStr::eNocase ) ) {
        BOOST_REQUIRE( taxid == 9749 );
    } else {
        BOOST_REQUIRE( taxid == 0 );
    }
}

/***************************************************
 * Get pointer to "blast" name
 * Returns: the pointer on first blast name at or above this node in the lineage
 * NOTE:
 * This function does not make a copy of "blast" name, so, caller can not use
 * MemFree function for returned pointer.
 */
//extern "C" string tax1.getBlastName(Int4 tax_id)
BOOST_AUTO_TEST_CASE(test_tax1m_getBlastName)
{
    string name;
    bool b = tax1.GetBlastName(9606, name);
    BOOST_REQUIRE( b != false );
    BOOST_REQUIRE( name == "primates" );
    
}

/*-------------------------------------
  Check if OrgRef needs to be updated
*/
BOOST_AUTO_TEST_CASE(test_tax1e_needUpdate)
{
    string orgref = "Org-ref ::= {\
    taxname \"Homo sapiens\",\
    common \"human\",\
    db {\
      {\
        db \"taxon\",\
        tag id 9606\
      }\
    },\
    syn {\
      \"man\"\
    },\
    orgname {\
      name binomial {\
        genus \"Homo\",\
        species \"sapiens\"\
      },\
      attrib \"specified\",\
      lineage \"Eukaryota; Metazoa; Chordata; Craniata; Vertebrata;\
 Euteleostomi; Mammalia; Eutheria; Euarchontoglires; Primates; Haplorrhini;\
 Catarrhini; Hominidae; Homo\",\
      gcode 1,\
      mgcode 2,\
      div \"PRI\"\
    }\
}";
    // Wrong taxid
    CTaxon1::TOrgRefStatus status;
    CRef< COrg_ref > orp = s_String2OrgRef( orgref.c_str() );
    BOOST_REQUIRE( orp != NULL );

    bool b = tax1.CheckOrgRef( *orp, status );
    cout << status << endl;
    BOOST_REQUIRE( b == true );
    BOOST_REQUIRE( status == 0 );
    
    orp = s_String2OrgRef( NStr::Replace( orgref, "PRI", "" ) );
    b = tax1.CheckOrgRef( *orp, status );
    cout << status << endl;
    BOOST_REQUIRE( b == true );
    BOOST_REQUIRE( (status&CTaxon1::eStatus_WrongDivision) != 0 );

    orp = s_String2OrgRef( NStr::Replace( orgref, 
"      name binomial {\
        genus \"Homo\",\
        species \"sapiens\"\
      }", 
"      name partial {\
        {\
          fixed-level other,\
          level \"species\",\
          name \"Homo sapiens\"\
        } }" ) );
    b = tax1.CheckOrgRef( *orp, status );
    cout << status << endl;
    BOOST_REQUIRE( b == true );
    BOOST_REQUIRE( (status&CTaxon1::eStatus_WrongOrgname) != 0 );

    status = 0;
    CConstRef< CTaxon2_data > orpb = tax1.LookupMerge( *orp, 0, &status );
    cout << status << endl;
    BOOST_REQUIRE( orpb.GetPointer() != NULL );
    BOOST_REQUIRE( (status&CTaxon1::eStatus_WrongOrgname) != 0 );

    orp = s_String2OrgRef( NStr::Replace( orgref, "human", "dog" ) );
    b = tax1.CheckOrgRef( *orp, status );
    cout << status << endl;
    BOOST_REQUIRE( b == true );
    BOOST_REQUIRE( (status&CTaxon1::eStatus_WrongCommonName) != 0 );
    
    orp = s_String2OrgRef( NStr::Replace( orgref, "Eukaryota", "Root" ) );
    b = tax1.CheckOrgRef( *orp, status );
    cout << status << endl;
    BOOST_REQUIRE( b == true );
    BOOST_REQUIRE( (status&CTaxon1::eStatus_WrongLineage) != 0 );
    
}
    //---------------------------------------------
    // Get taxonomic rank id by rank name
    // Returns: rank id
    //          -2 - in case of error
    ///
BOOST_AUTO_TEST_CASE(test_tax1m_getRankByName)
{
    string name = "species";
    string genus = "genus";

    TTaxRank r1 = tax1.GetRankIdByName(name);
    BOOST_REQUIRE( r1 > 0 );
    TTaxRank r2 = tax1.GetRankIdByName(genus);
    BOOST_REQUIRE( r2 > 0 );

    BOOST_REQUIRE( r2 < r1 );

    TTaxRank re = tax1.GetRankIdByName("ksdflglksg");
    BOOST_REQUIRE( re == -2 );
}
    //---------------------------------------------
    // Get taxonomic division id by division name (or code)
    // Returns: rank id
    //          -1 - in case of error
    ///
    //---------------------------------------------
    // Get taxonomic division name by division id
    ///

BOOST_AUTO_TEST_CASE(test_tax1m_getDivByName)
{
    string name = "Mammals";
    TTaxDivision d = tax1.GetDivisionIdByName( name );
    BOOST_REQUIRE( d > 0 );
    TTaxDivision dd = tax1.GetDivisionIdByName( "MAM" );
    BOOST_REQUIRE( dd > 0 );
    BOOST_REQUIRE( d == dd );
    string div_out;
    bool b = tax1.GetDivisionName( d, div_out );
    BOOST_REQUIRE( b );
    BOOST_REQUIRE( div_out == name );

    TTaxDivision de = tax1.GetDivisionIdByName( "kldjsjklshg" );
    BOOST_REQUIRE( de < 0 );
}

BOOST_AUTO_TEST_SUITE_END();
