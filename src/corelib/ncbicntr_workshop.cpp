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
* Author:  Aaron Ucko
*
* File Description:
*   Trivial file to avoid requiring all users of the toolkit to
*   compile with ncbicntr_workshop.il.
*
* =========================================================================== */

#define NCBI_COUNTER_IMPLEMENTATION
#include <ncbi_pch.hpp>
#include <corelib/ncbiatomic.hpp>

/*
* ===========================================================================
*
* $Log$
* Revision 1.4  2004/05/14 13:59:27  gorelenk
* Added include of ncbi_pch.hpp
*
* Revision 1.3  2003/10/01 12:02:51  ucko
* Fix header for SwapPointers
*
* Revision 1.2  2003/06/27 17:28:08  ucko
* +SwapPointers
*
* Revision 1.1  2002/05/23 22:24:22  ucko
* Use low-level atomic operations for reference counts
*
*
* ===========================================================================
*/


