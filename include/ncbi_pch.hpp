#if defined(NCBI_USE_PCH)

#  ifndef NCBI_PCH__HPP
#  define NCBI_PCH__HPP
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
 */

/** @file ncbi_pch.hpp
 ** Header file to be pre-compiled and speed up build of NCBI C++ Toolkit
 **/


//TODO - define file content
#include <corelib/ddumpable.hpp>
#include <corelib/metareg.hpp>
///#include <corelib/ncbiapp.hpp>
///#include <corelib/ncbiargs.hpp>
#include <corelib/ncbiatomic.hpp>
#include <corelib/ncbicntr.hpp>
#include <corelib/ncbidbg.hpp>
#include <corelib/ncbidiag.hpp>
//#include <corelib/ncbidll.hpp>
///#include <corelib/ncbienv.hpp>
//#include <corelib/ncbiexec.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbiobj.hpp>
///#include <corelib/ncbireg.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbithr.hpp>
#include <corelib/ncbithr_conf.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbiutil.hpp>
//#include <corelib/ncbi_bswap.hpp>
#include <corelib/ncbi_limits.hpp>
//#include <corelib/ncbi_process.hpp>
//#include <corelib/ncbi_safe_static.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_tree.hpp>
//#include <corelib/plugin_manager.hpp>
//#include <corelib/plugin_manager_impl.hpp>
//#include <corelib/test_mt.hpp>
//#include <corelib/version.hpp>
/*
 * ===========================================================================
 * $Log$
 * Revision 1.4  2004/05/24 16:39:33  gorelenk
 * Removed ncbi_safe_static.hpp - causes problems with progs not using xncbi
 *
 * Revision 1.3  2004/05/14 16:46:02  gorelenk
 * Comment some rarely used includes
 *
 * Revision 1.2  2004/05/14 14:14:14  gorelenk
 * Changed ifdef
 *
 * Revision 1.1  2004/05/14 13:58:11  gorelenk
 * Initial revision
 *
 * ===========================================================================
 */

#  endif /* NCBI_PCH__HPP */

#endif /* NCBI_USE_PCH */
