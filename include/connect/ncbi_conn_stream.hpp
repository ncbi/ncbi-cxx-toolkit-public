#ifndef NCBI_CONN_STREAM__HPP
#define NCBI_CONN_STREAM__HPP

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
* Author:  Denis Vakatov, Anton Lavrentiev
*
* File Description:
*   CONN-based C++ stream
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 6.1  2001/01/09 23:35:24  vakatov
* Initial revision (draft, not tested in run-time)
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <connect/ncbi_connector.h>


BEGIN_NCBI_SCOPE


class CConn_IOStream : public iostream
{
public:
    CConn_IOStream(CONNECTOR connector,
                   streamsize buf_size = 4096, bool do_tie = true);
    virtual ~CConn_IOStream(void);
};


END_NCBI_SCOPE

#endif  /* NCBI_CONN_STREAM__HPP */
