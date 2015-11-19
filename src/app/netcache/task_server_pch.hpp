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
 * Authors:  Pavel Ivanov
 *
 * File Description:
 *   File serving as precompiled header on Windows and that should be included
 *   first in all task_server sources. This file ensures the correct order of
 *   inclusion of core task_server headers and some corelib headers to avoid
 *   any conflicts and provide correct symbol definitions. Also it defines
 *   guards against inclusion of certain headers which should never be
 *   included in any source of task_server or application linking with it.
 */


#define CORELIB___NCBIDIAG__HPP
#define CORELIB___NCBIDBG__HPP
#define NCBIDBG_P__HPP
#define CORELIB___NCBIAPP__HPP
#define CORELIB___NCBITHR__HPP
#define CORELIB___NCBIMEMPOOL__HPP
#define CORELIB___NCBI_OS_MSWIN_P__HPP
#define CORELIB___NCBI_STACK__HPP
#include <common/ncbi_source_ver.h>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbitype.h>
#include <corelib/ncbistre.hpp>

#include <string>
#include <vector>
#include <map>
#include <list>
#include <memory>

#ifdef NCBI_OS_MSWIN
# include <windows.h>
#endif

#include "srv_diag.hpp"
#include "srv_lib.hpp"

// These should go after srv_lib.hpp
#ifdef _DEBUG
# undef _DEBUG
# include <corelib/ncbiobj.hpp>
# define _DEBUG
#else
# include <corelib/ncbiobj.hpp>
#endif
#include <corelib/ncbi_process.hpp>
#undef NCBI_THREAD_PID_WORKAROUND


#define BOOST_ENABLE_ASSERT_HANDLER
#include <boost/intrusive/slist.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/set.hpp>


#include "task_server.hpp"
#include "srv_sync.hpp"
#include "srv_time.hpp"
#include "srv_tasks.hpp"
#include "srv_sockets.hpp"
#include "srv_inlines.hpp"
