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
* Author: Sema Kachalo
*
* File Description:
*   Getting text description for CSeq_feat / CSeqdesc / CBioseq / CBioseq_set objects
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
//#include <objtools/edit/seqid_guesser.hpp>
//#include <objects/seqfeat/RNA_ref.hpp>
#include <objtools/edit/text_object_description.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

string GetTextObjectDescription(const objects::CSeq_feat& sf, objects::CScope& scope)
{
    return "Coming soon";
}


string GetTextObjectDescription(const objects::CSeqdesc& sd, objects::CScope& scope)
{
    return "Coming soon";
}


string GetTextObjectDescription(const objects::CBioseq& bs, objects::CScope& scope)
{
    return "Coming soon";
}


string GetTextObjectDescription(const objects::CBioseq_set& bs, objects::CScope& scope)
{
    return "Coming soon";
}


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

