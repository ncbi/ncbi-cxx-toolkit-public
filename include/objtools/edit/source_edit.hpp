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
 *  and reliability of the software and data,  the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties,  express or implied,  including
 *  warranties of performance,  merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Colleen Bollin, Andrea Asztalos
 */


#ifndef _EDIT_SOURCE_EDIT__HPP_
#define _EDIT_SOURCE_EDIT__HPP_

#include <corelib/ncbistd.hpp>
#include <objmgr/scope.hpp>
#include <objects/seqfeat/BioSource.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

/// Function removes old-name modifier and the tax-id, 
/// resets the common name and removes the synonyms for taxname
NCBI_XOBJEDIT_EXPORT bool CleanupForTaxnameChange( objects::CBioSource& src );
NCBI_XOBJEDIT_EXPORT bool RemoveOldName( objects::CBioSource& src );
NCBI_XOBJEDIT_EXPORT bool RemoveTaxId( objects::CBioSource& src );
NCBI_XOBJEDIT_EXPORT CRef<CBioSource> MakeCommonBioSource(const objects::CBioSource& src1, const objects::CBioSource& src2);

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* _EDIT_SOURCE_EDIT__HPP_ */

