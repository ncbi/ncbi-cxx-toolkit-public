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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Test suite for "ncbi_ifconf.[ch]"
 *
 */

#include <ncbiconf.h>
#include "../../ncbi_priv.h"
#include <connect/ext/ncbi_ifconf.h>
#include <connect/ncbi_socket.h>
#include <stdio.h>
#include <string.h>
/* This header must go last */
#include <common/test_assert.h>


int main(int argc, char* argv[])
{
    SNcbiIfConf c;
    char address[40];
    char netmask[40];
    char broadcast[40];

    CORE_SetLOGFormatFlags(fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE_Ex(stderr, eLOG_Trace, eLOG_Fatal, 0/*no auto-close*/);

    if ( !NcbiGetHostIfConf(&c) )
        CORE_LOG(eLOG_Fatal, "Unable to get host's net if conf");

    if (SOCK_ntoa(c.address, address, sizeof(address)) != 0)
        strcpy(address, "<unknown>");
    if (SOCK_ntoa(c.netmask, netmask, sizeof(netmask)) != 0)
        strcpy(netmask, "<unknown>");
    if (SOCK_ntoa(c.broadcast, broadcast, sizeof(broadcast)) != 0)
        strcpy(broadcast, "<unknown>");

    CORE_LOGF(eLOG_Note,
              ("# of ifs %d, # of skipped ifs %d", c.nifs, c.sifs));
    CORE_LOGF(eLOG_Note,
              ("address %s, netmask %s, broadcast %s, mtu %lu",
               address, netmask, broadcast, (unsigned long) c.mtu));
    return 0;
}
