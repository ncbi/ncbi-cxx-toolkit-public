#ifndef CLEANUP___CLEANUP__HPP
#define CLEANUP___CLEANUP__HPP

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
 * Author:  Robert Smith
 *
 * File Description:
 *   Basic Cleanup of CSeq_entries.
 *   .......
 *
 */
#include <corelib/ncbistd.hpp>
#include <corelib/ncbidiag.hpp>
#include <serial/objectinfo.hpp>
#include <serial/serialbase.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;

class validator::CValidError;

class NCBI_CLEANUP_EXPORT CCleanup : public CObject 
{
public:

    enum EValidOptions {};

    // Construtor / Destructor
    CCleanup();
    ~CCleanup();

    // Validate Seq-entry. 
    // If provding a scope the Seq-entry must be a 
    // top-level Seq-entry in that scope.
    CConstRef<CValidError> Cleanup(CSeq_entry& se, Uint4 options = 0);

private:
    // Prohibit copy constructor & assignment operator
    CCleanup(const CCleanup&);
    CCleanup& operator= (const CCleanup&);

};


// Inline Functions:


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2005/10/17 12:21:07  rsmith
* initial checkin
*
*
* ===========================================================================
*/

#endif  /* CLEANUP___CLEANUP__HPP */
