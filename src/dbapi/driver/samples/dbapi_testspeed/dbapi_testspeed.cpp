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
* File Description:
*   Run a series of insert/update/select statement,
*   measure the time required for their execution.
*
*============================================================================
*/

#include <ncbi_pch.hpp>
#include "dbapi_testspeed.hpp"
#include <corelib/ncbitime.hpp>
#include <corelib/ncbifile.hpp>

USING_NCBI_SCOPE;

const char usage[] =
  "Run a series of BCP/insert/select commands,\n"
  "measure the time required for their execution. \n"
  "\n"
  "USAGE:   dbapi_testspeed -parameters [file]\n"
  "REQUIRED PARAMETERS:\n"
  "  -S server\n"
  "  -d driver (e.g. ctlib dblib ftds odbc gateway)\n"
  "  -g sss_server:port for gateway database driver\n"
  "     (only one of -d/-g is required to be present)\n"
  "OPTIONAL PARAMETERS:\n"
  "  -r row_count (default is 1)\n"
  "  -b text_blob_size in kilobytes (default is 1 kb)\n"
  "  -c column_count  1..5 (int_val fl_val date_val str_val txt_val)\n"
  "  -t table_name (default is 'TestSpeed')\n"
  "  -m mode_character:\n"
  "     r  write and read as usual,\n"
  "        use CDB_Result->ReadItem() instead of GetItem() whenever possible\n"
  "     w  write to the table, do not read, do not delete it\n"
  "     R  read using ReadItem() (can be done many times after '-m w' )\n"
  "     G  read using GetItem()  (can be done many times after '-m w' )\n"
  "FILE (optional):\n"
  "  Upload the specified file as a blob into the table,\n"
  "  then download it to \"./testspeed.out\".\n"
  "  \"diff\" can then be used to verify the data.\n"
  "  -r -b -c parameterss are ignored.\n"
  "\n"
  ;

int Usage()
{
  cout << usage;
  return 1;
}

// Create a table with 5 columns, fill it using BCP or insert commands(1),
// fetch results(2), delete the table. Execution time is measured for steps 1, 2.

int main (int argc, char* argv[])
{
  int count=0;
  I_DriverContext* my_context;
  CDB_Connection* con;
  CDB_BCPInCmd* bcp=NULL;
  CDB_LangCmd* ins_cmd=NULL;
  CStopWatch timer;

  // Command line args
  string server_name, driver_name;
  int row_count;
  string table_name = "TestSpeed";
  int blob_size=1;
  int col_count=5;

  const char* p = NULL;
  bool readItems=false;
  bool writeOnly=false;
  bool selectOnly=false;
  double timeElapsed;

  // Read required args
  p= getParam('S', argc, argv);
  if(p) { server_name = p; } else return Usage();

  p= getParam('d', argc, argv);
  if(p) { driver_name = p; };

  string sss_host, sss_port;
  p= getParam('g', argc, argv);
  if(p){
    sss_host=p;
    int i=NStr::Find(sss_host, ":");
    if(i>0) {
      sss_port = sss_host.substr(i+1);
      sss_host.resize(i);
    }
    if( driver_name.size()==0 ) {
      driver_name="gateway";
    }
  }

  // one fo "-d" or "-g" must be present
  if( driver_name.size()==0 ) return Usage();

  // Read optional args
  p= getParam('r', argc, argv);
  if(p){
    row_count = atoi(p);
    if(row_count<1 || row_count>0x100000) {
      cerr << "Error -- invalid row count; valid values are 1 .. 1 Meg.\n";
      return Usage();
    }
  }
  //else return Usage();
  else row_count=1;

  p= getParam('c', argc, argv);
  if(p){
    col_count = atoi(p);
    if(col_count<1 || col_count>5) {
      cerr << "Error -- invalid column count = " << col_count << "; valid values are: 1..5\n";
      return Usage();
    }
  }

  p= getParam('b', argc, argv);
  if(p){
    blob_size = atoi(p);
    if(blob_size<1 || blob_size>1024000 ) {
      cerr << "Error -- invalid blob size; valid values are 1 (kb) to 1000 (kb)\n";
      return Usage();
    }
    if(col_count<5) {
      cerr << "Error -- blob size makes sense for '-c 5' only.\n";
      return 1;
    }
  }

  p= getParam('t', argc, argv);
  if(p) { table_name = p; };

  p= getParam('m', argc, argv);
  if(p) {
    switch(*p) {
      case 'r': readItems  = true; break;
      case 'w': writeOnly  = true; break;
      case 'G': selectOnly = true; break;
      case 'R': selectOnly = true; readItems = true; break;
      default:
        cerr << "Error -- invalid mode character '" << p[0] << "'\n";
        return Usage();
    }
  };


  char *fileParam=getParam(0, argc, argv);

  /*
  char *param;
  while( param=getParam(0, argc, argv) ) {
    cout << ">" << param << "<\n";
  }
  return 0;
  */


  // Load the database driver
  C_DriverMgr drv_mgr;
  string err_msg;

  map<string, string> mapDrvAttrib;
  if(driver_name == "dblib") {
    // Needed to work with BCP when using dblib Sybase 12.5
    mapDrvAttrib.insert(
      map<string,string>::value_type( string("version"), string("100") )
    );
  }
  else{
    if( sss_port.size() ) {
      mapDrvAttrib.insert(
        map<string,string>::value_type( string("port"), sss_port )
      );
    }
    if( sss_host.size() ) {
      mapDrvAttrib.insert(
        map<string,string>::value_type( string("host"), sss_host )
      );
    }
  }
  if( mapDrvAttrib.size() ) {
    my_context = drv_mgr.GetDriverContext(driver_name, &err_msg, &mapDrvAttrib);
  }
  else{
    my_context = drv_mgr.GetDriverContext( driver_name, &err_msg );
  }

  if(!my_context) {
    cerr << "Can not load a driver " << driver_name
         << " [" << err_msg << "]\n";
    return 1;
  }

  try {
    // Prepare the connection
    con = my_context->Connect(
        server_name, "anyone", "allowed", I_DriverContext::fBcpIn);

    CDB_LangCmd* set_cmd= con->LangCmd("use DBAPI_Sample");
    set_cmd->Send();
    CDB_Result* r;
    while(set_cmd->HasMoreResults()) {
      r= set_cmd->Result();
      if(r) delete r;
    }
    delete set_cmd;

    set_cmd= con->LangCmd("set textsize 1024000");
    set_cmd->Send();
    while(set_cmd->HasMoreResults()) {
      r= set_cmd->Result();
      if(r) delete r;
    }
    delete set_cmd;


    //cout<< "driver " << driver_name;

    // Create table, insert data
    if(!selectOnly) {
      CreateTable(con, table_name); // Deletes the pre-existing table, if present
      cout<< ", rows " << row_count
          << ", cols " << col_count
          << ", blob size " << blob_size << "\n";

      if(col_count>4) {
        // "Bulk copy in" command
        bcp = con->BCPIn(table_name, col_count);
      }
      else {
        //cerr << "-c option not supported yet\n";
        //return 1;
        string s  = "insert into ";
        s+= table_name;
        s+= " (int_val";
        string sv = "@i";

        if(col_count>1) { s+= ", fl_val"  ; sv+=", @f"; }
        if(col_count>2) { s+= ", date_val"; sv+=", @d"; }
        if(col_count>3) { s+= ", str_val" ; sv+=", @s"; }

        s+= ") values (";
        s+= sv;
        s+= ")";

        ins_cmd = con->LangCmd(s);
      }

      CDB_Int int_val;
      CDB_Float fl_val;
      CDB_DateTime date_val(CTime::eCurrent);
      CDB_VarChar str_val;
      CDB_Text pTxt;
      int i;

      if(fileParam) {
        CNcbiIfstream f(fileParam, IOS_BASE::in|IOS_BASE::binary);
        if(!f.is_open()) {
          cerr << "Error -- cannot read '" << fileParam << "'\n";
          return 1;
        }
        char buf[10240];
        int sz;
        while( f.good() && !f.eof() ) {
          f.read( buf, sizeof(buf) );
          sz = f.gcount();
          // cout << sz << "\n";
          if( sz <= 0 ) break;
          pTxt.Append(buf, sz);
          if( sz != sizeof(buf) ) break;
        }
        f.close();
        // cout << "pTxt.Size()=" << pTxt.Size() << "\n";

        col_count=5;
        row_count=1;
      }
      else if(col_count>4) {
        for(i=0; i<blob_size; i++) {
          // Add 1024 chars
          for( int j=0; j<32; j++ ) {
            pTxt.Append("If you want to know who we are--");
          }
        }
      }

      timer.Start();

      // Bind program variables as data source
      if(bcp) {
        bcp->Bind(0, &int_val);
        if(col_count>1) bcp->Bind(1, &fl_val  );
        if(col_count>2) bcp->Bind(2, &date_val);
        if(col_count>3) bcp->Bind(3, &str_val );
        if(col_count>4) bcp->Bind(4, &pTxt    );
      }
      else{
        if( !ins_cmd->BindParam("@i", &int_val) ) {
          cerr << "Error in BindParam()\n";
          DeleteTable(con, table_name);
          return 1;
        }

        if(col_count>1) ins_cmd->BindParam("@f", &fl_val  );
        if(col_count>2) ins_cmd->BindParam("@d", &date_val);
        if(col_count>3) ins_cmd->BindParam("@s", &str_val );
      }

      for(i= 0; i<row_count; i++) {
        int_val  = i;
        fl_val   = i + 0.999;
        if(fileParam) {
          CDirEntry fileEntry(fileParam);
          CTime fileTime;
          fileEntry.GetTime(&fileTime);

          date_val = fileTime;
          str_val = fileParam;
        }
        else {
          date_val = date_val.Value();
          str_val  = string("Franz Joseph Haydn symphony # ")+NStr::IntToString(i);
        }
        pTxt.MoveTo(0);

        if( bcp ) {
          bcp->SendRow();

          if (count == 2) {
            bcp->CompleteBatch();
            count = 0;
          }
          count++;
        }
        else {
          ins_cmd->Send();
          CDB_Result* r;
          while(ins_cmd->HasMoreResults()) {
            r= ins_cmd->Result();
            if(r) delete r;
          }
        }
      }
      if( bcp ) {
        bcp->CompleteBCP();
        delete bcp;
      }
      if( ins_cmd ) {
        delete ins_cmd;
      }
      timeElapsed = timer.Elapsed();
      cout << "inserting timeElapsed=" << NStr::DoubleToString(timeElapsed, 2) << "\n";
    }
    else{
      cout << "\n";
    }

    if(!writeOnly) {
      // Read data
      timer.Start();
      if(fileParam) {
        FetchFile(con, table_name, readItems);
      }
      else {
        FetchResults(con, table_name, readItems);
      }
      timeElapsed = timer.Elapsed();
      cout << "fetching timeElapsed=" << NStr::DoubleToString(timeElapsed,2) << "\n";
      cout << "\n";
    }

    if(!(selectOnly || writeOnly)) {
      DeleteTable(con, table_name);
    }
    delete con;
  }
  catch (CDB_Exception& e) {
    HandleIt(&e);
    //DeleteTable(con, table_name);
    return 1;
  }

  return 0;
}









