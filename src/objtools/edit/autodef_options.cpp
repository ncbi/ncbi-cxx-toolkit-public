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
* Author:  Colleen Bollin
*
* File Description:
*   Generate unique definition lines for a set of sequences using organism
*   descriptions and feature clauses.
*/

#include <ncbi_pch.hpp>
#include <objtools/edit/autodef_options.hpp>
#include <corelib/ncbimisc.hpp>

#include <objects/seqfeat/BioSource.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

            
CAutoDefOptions::CAutoDefOptions()
    : m_FeatureListType(eListAllFeatures),
      m_MiscFeatRule(eDelete),
      m_ProductFlag(CBioSource::eGenome_unknown),
      m_SpecifyNuclearProduct(false),
      m_AltSpliceFlag(false),
      m_SuppressLocusTags(false),
      m_GeneClusterOppStrand(false),
      m_SuppressFeatureAltSplice(false),
      m_SuppressMobileElementSubfeatures(false),
      m_KeepExons(false),
      m_KeepIntrons(false),
      m_KeepPromoters(false),
      m_UseFakePromoters(false),
      m_KeepLTRs(false),
      m_Keep3UTRs(false),
      m_Keep5UTRs(false),
      m_UseNcRNAComment(false)
{
    m_SuppressedFeatures.clear();
}


CRef<CUser_object> CAutoDefOptions::MakeUserObject()
{
    CRef<CUser_object> user(new CUser_object());


    return user;
}


void CAutoDefOptions::InitFromUserObject(const CUser_object& obj)
{
}



END_SCOPE(objects)
END_NCBI_SCOPE
