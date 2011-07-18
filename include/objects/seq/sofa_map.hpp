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
 * File Description:  Sequence Ontology Type Mapping
 *
 */

#ifndef OBJECTS___SOFAMAP__HPP
#define OBJECTS___SOFAMAP__HPP

#include <objects/seqfeat/SeqFeatData.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  ============================================================================
class NCBI_SEQ_EXPORT CSofaMap
//  ============================================================================
{
public:
    CSofaMap() {
        x_Init();
    };

    string DefaultName() 
    {
        return m_default.m_name;
    };

    string MappedName(
        CSeqFeatData::E_Choice type,
        CSeqFeatData::ESubtype subtype )
    {
        map< CFeatListItem, SofaType >::const_iterator cit = m_Map.find(
            CFeatListItem( type, subtype, "", "" ) );
        if ( cit != m_Map.end() ) {
            return cit->second.m_name;
        }
        return m_default.m_name;
    }

protected:
    void x_Init();

    map< CFeatListItem, SofaType > m_Map;
    SofaType m_default;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJECTS___SOFAMAP__HPP
