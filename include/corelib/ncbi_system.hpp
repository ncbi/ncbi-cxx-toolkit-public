#ifndef NCBI_SYSTEM__HPP
#define NCBI_SYSTEM__HPP

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
* Author:  Vladimir Ivanov
*
* File Description:
*
*   System functions:
*      SetHeapLimit()
*      SetCpuTimeLimit()
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2001/07/02 21:33:05  vakatov
* Fixed against SIGXCPU during the signal handling.
* Increase the amount of reserved memory for the memory limit handler
* to 10K (to fix for the 64-bit WorkShop compiler).
* Use standard C++ arg.processing (ncbiargs) in the test suite.
* Cleaned up the code. Get rid of the "Ncbi_" prefix.
*
* Revision 1.1  2001/07/02 16:47:25  ivanov
* Initialization
*
* ===========================================================================
*/


BEGIN_NCBI_SCOPE


/* [UNIX only] 
 * Set the limit for the size of dynamic memory(heap) allocated by the process.
 *
 * If, during the program execution, the heap size exceeds "max_heap_size"
 * in any `operator new' (but not malloc, etc.!), then:
 *  1) dump info about current program state to log-stream;
 *  2) terminate the program.
 *
 * NOTE:  "max_heap_size" == 0 will lift off the heap restrictions.
 */
extern bool SetHeapLimit(size_t max_heap_size);


/* [UNIX only] 
 * Set the limit for the CPU time that can be consumed by this process.
 *
 * If current process consumes more than "max_cpu_time" seconds of CPU time:
 *  1) dump info about current program state to log-stream;
 *  2) terminate the program.
 *
 * NOTE:  "max_cpu_time" == 0 will lift off the CPU time restrictions.
 */
extern bool SetCpuTimeLimit(size_t max_cpu_time);


END_NCBI_SCOPE

#endif  /* NCBI_SYSTEM__HPP */

