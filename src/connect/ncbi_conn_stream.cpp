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
* Authors:  Anton Lavrentiev,  Denis Vakatov
*
* File Description:
*   CONN-based C++ stream
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 6.1  2001/01/09 23:35:25  vakatov
* Initial revision (draft, not tested in run-time)
*
* ===========================================================================
*/


#include <connect/ncbi_conn_stream.hpp>
#include "ncbi_conn_streambuf.hpp"


BEGIN_NCBI_SCOPE


CConn_IOStream::CConn_IOStream(CONNECTOR connector,
                               streamsize buf_size, bool do_tie)
    : iostream(new CConn_Streambuf(connector, buf_size))
{
    if ( do_tie ) {
        tie(this);
    }
}


CConn_IOStream::~CConn_IOStream(void)
{
    delete const_cast<streambuf*> (rdbuf());
}


END_NCBI_SCOPE
