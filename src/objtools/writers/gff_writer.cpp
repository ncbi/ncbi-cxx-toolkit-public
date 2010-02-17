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
#include <objects/seqfeat/Seq_feat.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>

#include <objtools/writers/gff_record.hpp>
#include <objtools/writers/gff_writer.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
CGffWriter::CGffWriter(
    CNcbiOstream& ostr ) :
//  ----------------------------------------------------------------------------
    m_Os( ostr )
{
};

//  ----------------------------------------------------------------------------
CGffWriter::~CGffWriter()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
bool CGffWriter::WriteAnnot( CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    if ( pAnnot->IsFtable() ) {
        return WriteAnnotFTable( pAnnot );
    }
    if ( pAnnot->IsAlign() ) {
        return WriteAnnotAlign( pAnnot );
    }
    cerr << "Unexpected!" << endl;
    return false;
}

//  ----------------------------------------------------------------------------
bool CGffWriter::WriteAnnotFTable( CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
    if ( ! pAnnot->IsFtable() ) {
        return false;
    }
    const list< CRef< CSeq_feat > >& table = pAnnot->GetData().GetFtable();
    list< CRef< CSeq_feat > >::const_iterator it = table.begin();
    while ( it != table.end() ) {
        CGffRecord record;
        record.SetRecord( pAnnot, *it );
        record.DumpRecord( m_Os );
        it++;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGffWriter::WriteAnnotAlign( CRef< CSeq_annot > pAnnot )
//  ----------------------------------------------------------------------------
{
//    cerr << "To be done." << endl;
    return false;
}

END_NCBI_SCOPE
