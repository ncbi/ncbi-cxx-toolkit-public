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
* Author:  Mati Shomrat, NCBI
*
* File Description:
*   Indicate a gap region on a bioseq
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objtools/format/formatter.hpp>
#include <objtools/format/items/gap_item.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CGapItem::CGapItem
(
  TSeqPos from, TSeqPos to, CBioseqContext& ctx, 
  const string &sFeatureName,
  const string &sType,
  const CGapItem::TEvidence &sEvidence,
  TSeqPos estimated_length ) :
    CFlatItem(&ctx), m_From(from + 1), m_To(to), 
        m_EstimatedLength(estimated_length),
        m_sFeatureName(sFeatureName),
        m_sType(sType), m_sEvidence(sEvidence)
        
{
}

void CGapItem::Format(IFormatter& formatter, IFlatTextOStream& text_os) const
{
    formatter.FormatGap(*this, text_os);
}


END_SCOPE(objects)
END_NCBI_SCOPE
