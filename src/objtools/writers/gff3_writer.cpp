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
 * File Description:  Write gff file
 *
 */

#include <ncbi_pch.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/feature.hpp>

#include <objtools/writers/gff3_write_data.hpp>
#include <objtools/writers/gff3_writer.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
CGff3Writer::CGff3Writer(
    CScope& scope,
    CNcbiOstream& ostr,
    TFlags uFlags ) :
//  ----------------------------------------------------------------------------
    CGff2Writer( scope, ostr, uFlags )
{
};

//  ----------------------------------------------------------------------------
CGff3Writer::~CGff3Writer()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
bool CGff3Writer::x_WriteHeader()
//  ----------------------------------------------------------------------------
{
    m_Os << "##gff-version 3" << endl;
    return true;
}

//  ============================================================================
CGff2WriteRecord* CGff3Writer::x_CreateRecord(
    feature::CFeatTree& feat_tree )
//  ============================================================================
{
    return new CGff3WriteRecord( feat_tree );
}

END_NCBI_SCOPE
