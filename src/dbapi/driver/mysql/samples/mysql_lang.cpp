/* $Id$
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
 * Author:  Anton Butanayev
 *
 * This simple program illustrates how to use the language command
 *
 */

#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/mysql/interfaces.hpp>

USING_NCBI_SCOPE;

int main(int argc, char **argv)
{
  if(argc != 5)
  {
    cerr << "Usage: " << argv[0] << " host database user_name passwd" << endl;
    return 1;
  }

  try
  {
    CMySQLContext my_context;
    auto_ptr<CDB_Connection> con(my_context.Connect(argv[1], argv[3], argv[4], 0));

    // changing database
    {
      auto_ptr<CDB_LangCmd>
        lcmd(con->LangCmd(string("use ") + argv[2]));
      lcmd->Send();
      cout << "Database changed" << endl;
    }

    // creating table
    {
      auto_ptr<CDB_LangCmd>
        lcmd(con->LangCmd(
                          "create temporary table tmp_t1("
                          "a int,"
                          "b datetime,"
                          "c varchar(100),"
                          "d text,"
                          "e double)"
                         ));
      lcmd->Send();
      cout << "Table created" << endl;
    }

    // inserting data
    {
      auto_ptr<CDB_LangCmd>
        lcmd(con->LangCmd(
                          "insert into tmp_t1 values"
                          "(1, '2002-11-25 12:45:59', 'Hello, world', 'SOME TEXT', 3.1415)"
                         ));
      lcmd->Send();
      cout << "Data inserted" << endl;
    }

    // selecting data
    {
      auto_ptr<CDB_LangCmd> lcmd(con->LangCmd("select * from tmp_t1"));
      lcmd->Send();
      while (lcmd->HasMoreResults())
      {
        auto_ptr<CDB_Result> r(lcmd->Result());
        while (r->Fetch())
        {
          CDB_Int a;
          CDB_DateTime b;
          CDB_VarChar c;
          CDB_VarChar d;
          CDB_Double e;

          r->GetItem(&a);
          r->GetItem(&b);
          r->GetItem(&c);
          r->GetItem(&d);
          r->GetItem(&e);

          cout
            << "a=" << a.Value() << endl
            << "b=" << b.Value().AsString() << endl
            << "c=" << c.Value() << endl
            << "d=" << d.Value() << endl
            << "e=" << e.Value() << endl;
        }
      }
    }

    // selecting data as strings
    {
      auto_ptr<CDB_LangCmd> lcmd(con->LangCmd("select * from tmp_t1"));
      lcmd->Send();
      while (lcmd->HasMoreResults())
      {
        auto_ptr<CDB_Result> r(lcmd->Result());
        for(unsigned i = 0; i < r->NofItems(); ++i)
          cout << "[" << r->ItemName(i) << "]";
        cout << endl;

        while (r->Fetch())
        {
          for(unsigned i = 0; i < r->NofItems(); ++i)
          {
            CDB_VarChar field;
            r->GetItem(&field);
            if(! field.IsNULL())
              cout << field.Value() << endl;
            else
              cout << "NULL\n";

          }
        }
      }
    }
  }
  catch (CDB_Exception& e)
  {
    CDB_UserHandler_Stream myExHandler(&cerr);
    myExHandler.HandleIt(&e);
    return 1;
  }
  return 0;
}

/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2002/08/29 15:41:44  butanaev
 * Command line interface improved.
 *
 * Revision 1.3  2002/08/28 17:18:20  butanaev
 * Improved error handling, demo app.
 *
 * Revision 1.2  2002/08/13 20:30:24  butanaev
 * Username/password changed.
 *
 * Revision 1.1  2002/08/13 20:23:14  butanaev
 * The beginning.
 *
 * ===========================================================================
 */
