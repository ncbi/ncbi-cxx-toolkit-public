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
 * Authors:  Frank Ludwig
 *
 * File Description:  Common writer functions
 *
 */

#ifndef OBJTOOLS_READERS___WRITE_UTIL__HPP
#define OBJTOOLS_READERS___WRITE_UTIL__HPP

#include <corelib/ncbistd.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/feature.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Trna_ext.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  ============================================================================
class NCBI_XOBJWRITE_EXPORT CWriteUtil
//  ============================================================================
{
public:
    static CRef<CUser_object> GetDescriptor(
        const CSeq_annot& annot,
        const string& );

    static bool GetGenomeString( 
        const CBioSource&,
        string& );

    static bool GetOrgModSubType(
        const COrgMod&,
        string&,
        string& );

    static bool GetSubSourceSubType(
        const CSubSource&,
        string&,
        string& );

    static bool GetBiomol(
        CBioseq_Handle,
        string& );

    static bool GetIdType(
        CBioseq_Handle,
        string& );

    static bool GetIdType(
        const CSeq_id&,
        string& );

    static bool GetAaName(
        const CCode_break&,
        string& );

    static bool GetCodeBreak(
        const CCode_break&,
        string& );

    static bool GetGeneRefGene(
        const CGene_ref&,
        string& );

    static bool GetTrnaProductName(
        const CTrna_ext&,
        string& );

    static bool GetBestId(
        CSeq_id_Handle,
        CScope&, 
        string& );

    static bool GetBestId(
        CMappedFeat,
        string& );

    static bool GetTrnaCodons(
        const CTrna_ext&,
        string& );

    static bool GetTrnaAntiCodon(
        const CTrna_ext&,
        string& );

    static bool GetDbTag(
        const CDbtag&,
        string& );

    static bool IsLocationOrdered(
        const CSeq_loc& );

    static bool IsSequenceCircular(
        CBioseq_Handle );

    static bool IsNucProtSet(
        CSeq_entry_Handle );

    static string UrlEncode(
        const string& );

    static bool NeedsQuoting(
        const string& );

    static void ChangeToPackedInt(
        CSeq_loc& loc);

    static bool GetQualifier(
        CMappedFeat mf,
        const string& key,
        string& value);

};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___WRITE_UTIL__HPP
