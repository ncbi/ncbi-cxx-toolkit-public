#ifndef _DiscRep_AUTOFIX_HPP
#define _DiscRep_AUTOFIX_HPP

/*  $Id$
 * ==========================================================================
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
 * ==========================================================================
 *
 * Author:  Jie Chen
 *
 * File Description:
 *   header file of autofix
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <connect/ncbi_core_cxx.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/seq_feat_handle.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objects/submit/Submit_block.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(DiscRepNmSpc)
USING_SCOPE(objects);

typedef void (*FAutofix)(vector <CRef <CObject> >& obj, vector <string>& msgs, CScope* scope);

NCBI_DISCREPANCY_REPORT_EXPORT void MarkOverlappingCDSs(vector <CRef <CObject> >& obj, vector <string>& msgs, CScope* scope);

END_SCOPE(DiscRepNmSpc)
END_NCBI_SCOPE

#endif // AUTOFIX_HPP
