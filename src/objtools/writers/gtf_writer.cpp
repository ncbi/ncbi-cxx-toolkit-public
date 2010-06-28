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

#include <objtools/writers/gff3_write_data.hpp>
#include <objtools/writers/gff_writer.hpp>
#include <objtools/writers/gtf_writer.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
CGtfWriter::CGtfWriter(
    CNcbiOstream& ostr ) :
//  ----------------------------------------------------------------------------
    CGffWriter( ostr )
{
};

//  ----------------------------------------------------------------------------
CGtfWriter::~CGtfWriter()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
bool CGtfWriter::x_WriteHeader()
//  ----------------------------------------------------------------------------
{
    m_Os << "#gtf-version 2.2" << endl;
    return true;
};

//  ----------------------------------------------------------------------------
string CGtfWriter::x_GffAttributes(
    const CGff3WriteRecord& record ) const
//  ----------------------------------------------------------------------------
{
    string strAttributes;
	strAttributes.reserve(256);
    CGff3WriteRecord::TAttributes attrs;
    attrs.insert( record.Attributes().begin(), record.Attributes().end() );
    CGff3WriteRecord::TAttrIt it;

    if ( ! record.GeneId().empty() ) {
        strAttributes += "gene_id \"";
		strAttributes += record.GeneId();
		strAttributes += "\"";
        strAttributes += "; ";
    }
    else {
        x_PriorityProcess( "gene_id", attrs, strAttributes );
    }

    if ( ! record.TranscriptId().empty() ) {
        strAttributes += "transcript_id \"";
		strAttributes += record.TranscriptId();
		strAttributes += "\"";
        strAttributes += "; ";
    }
    else {
        x_PriorityProcess( "transcript_id", attrs, strAttributes );
    }


    for ( it = attrs.begin(); it != attrs.end(); ++it ) {
        string strKey = it->first;
        if ( NStr::StartsWith( strKey, "gff_" ) ) {
            continue;
        }

        strAttributes += strKey;
        strAttributes += " ";
		
		bool quote = x_NeedsQuoting(it->second);
		if ( quote )
			strAttributes += '\"';		
		strAttributes += it->second;
		if ( quote )
			strAttributes += '\"';
		strAttributes += "; ";
    }
    return strAttributes;
}

//  ----------------------------------------------------------------------------
void CGtfWriter::x_PriorityProcess(
    const string& strKey,
    map<string, string >& attrs,
    string& strAttributes ) const
//  ----------------------------------------------------------------------------
{
    string strValue( "" );
    map< string, string >::iterator it = attrs.find( strKey );
    if ( it != attrs.end() ) {
        strValue = it->second;
    }

    strAttributes += strKey;
    strAttributes += " ";
   	bool quote = x_NeedsQuoting( strValue );
	if ( quote )
		strAttributes += '\"';		
	strAttributes += strValue;
    if ( it != attrs.end() ) {
        attrs.erase( it );
    }
	if ( quote )
		strAttributes += '\"';
	strAttributes += "; ";
}


END_NCBI_SCOPE
