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
 * File Description:
 *    Driver for MySQL server
 *
 */

#include <dbapi/driver/mysql/interfaces.hpp>


BEGIN_NCBI_SCOPE


CMySQLContext::CMySQLContext()
{
}


CMySQLContext::~CMySQLContext()
{
}


bool CMySQLContext::IsAbleTo(ECapability /*cpb*/) const
{
  return false;
}


bool CMySQLContext::SetLoginTimeout(unsigned int /*nof_secs*/)
{
  return false;
}


bool CMySQLContext::SetTimeout(unsigned int /*nof_secs*/)
{
  return false;
}


bool CMySQLContext::SetMaxTextImageSize(size_t /*nof_bytes*/)
{
  return false;
}


CDB_Connection* CMySQLContext::Connect(const string&   srv_name,
                                       const string&   user_name,
                                       const string&   passwd,
                                       TConnectionMode /*mode*/,
                                       bool            /*reusable*/,
                                       const string&   /*pool_name*/)
{
  return Create_Connection
      (*new CMySQL_Connection(this, srv_name, user_name, passwd));
}


unsigned int CMySQLContext::NofConnections(const string& /*srv_name*/) const
{
  return 0;
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2003/01/06 20:30:26  vakatov
 * Get rid of some redundant header(s).
 * Formally reformatted to closer meet C++ Toolkit/DBAPI style.
 *
 * Revision 1.1  2002/08/13 20:23:14  butanaev
 * The beginning.
 *
 * ===========================================================================
 */
