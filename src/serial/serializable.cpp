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
* Author: Eugene Vasilchenko
*
* File Description:
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2001/04/12 17:01:11  kholodov
* General serializable interface for different output formats
*
*
* ===========================================================================
*/

#include <serial/serializable.hpp>

BEGIN_NCBI_SCOPE

ostream& operator << (ostream& out, ISerializable& src) 
{
  switch( src.GetOutputType() ) {
  case ISerializable::eAsFasta:
    src.WriteAsFasta(out);
    break;
  case ISerializable::eAsAsnText:
    src.WriteAsAsnText(out);
    break;
  case ISerializable::eAsAsnBinary:
    src.WriteAsAsnBinary(out);
    break;
  case ISerializable::eAsXML:
    src.WriteAsXML(out);
    break;
  default:
    throw runtime_error("operator << (ostream&, ISerializable&): wrong output type");
  }

  return out;
};

END_NCBI_SCOPE
