#ifndef LDS_COREOBJREADER_HPP__
#define LDS_COREOBJREADER_HPP__
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
 * Author: Anatoliy Kuznetsov
 *
 * File Description: Core bio objects reader.
 *
 */


#include <objmgr/util/obj_sniff.hpp>
#include <stack>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

//////////////////////////////////////////////////////////////////
//
// Try and fail parser, used for discovery of files structure.
//

class NCBI_LDS_EXPORT CLDS_CoreObjectsReader : public CObjectsSniffer
{
public:
    CLDS_CoreObjectsReader();

    // Event function called when parser finds a top level object
    virtual void OnTopObjectFoundPre(const CObjectInfo& object, 
                                     CNcbiStreamoff stream_offset);

    // Event function alled after top object deserialization
    virtual void OnTopObjectFoundPost(const CObjectInfo& object);

    // Overload from CObjectsSniffer
    virtual void OnObjectFoundPre(const CObjectInfo& object, 
                                  CNcbiStreamoff stream_offset);

    // Overload from CObjectsSniffer
    virtual void OnObjectFoundPost(const CObjectInfo& object);

    // Overload from CObjectsSniffer
    virtual void Reset();

public:

    struct SObjectDetails
    {
        CObjectInfo    info;
        CNcbiStreamoff offset;
        CNcbiStreamoff parent_offset;
        CNcbiStreamoff top_level_offset;
        bool           is_top_level;
        int            ext_id;  // Database id or any other external id

        SObjectDetails(const  CObjectInfo& object_info,
                       CNcbiStreamoff stream_offset,
                       CNcbiStreamoff p_offset,
                       CNcbiStreamoff top_offset,
                       bool   is_top)
        : info(object_info),
          offset(stream_offset),
          parent_offset(p_offset),
          top_level_offset(top_offset),
          is_top_level(is_top),
          ext_id(0)
        {}
    };

    typedef vector<SObjectDetails>    TObjectVector;

    // Find object information based on the stream offset.
    // Return NULL if not found.
    SObjectDetails* FindObjectInfo(CNcbiStreamoff stream_offset);

    TObjectVector& GetObjectsVector() { return m_Objects; }

    void ClearObjectsVector() { m_Objects.clear(); }

protected:

    struct SObjectParseDescr
    {
        const CObjectInfo*  object_info;
        CNcbiStreamoff      stream_offset;

        SObjectParseDescr(const CObjectInfo* oi,
                          CNcbiStreamoff offset)
        : object_info(oi),
          stream_offset(offset)
        {}
        SObjectParseDescr() 
        : object_info(0),
          stream_offset(0)
        {}
    };

    typedef stack<SObjectParseDescr>  TParseStack;

protected:
    // Find object in the objects vector (m_Objects) by the stream 
    // offset. Returns objects' index in vector, -1 if "not found".
    // This function works on the fact that only one object can 
    // be found in one particular offset, and offset udentifies any
    // object unqiely in its file.
    int FindObject(CNcbiStreamoff stream_offset);

private:
    TParseStack         m_Stack;
    SObjectParseDescr   m_TopDescr;
    TObjectVector       m_Objects;

};



END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.9  2005/03/29 19:21:21  jcherry
 * Added export specifier
 *
 * Revision 1.8  2004/08/30 18:16:28  gouriano
 * Use CNcbiStreamoff instead of size_t for stream offset operations
 *
 * Revision 1.7  2003/10/09 16:43:05  kuznets
 * +ClearObjectsVector()
 *
 * Revision 1.6  2003/10/07 20:44:44  kuznets
 * + Reset() method
 *
 * Revision 1.5  2003/06/03 14:07:46  kuznets
 * Include paths changed to reflect the new directory structure
 *
 * Revision 1.4  2003/05/30 20:31:51  kuznets
 * Added SObjectDetails::ext_id (database id)
 *
 * Revision 1.2  2003/05/23 20:33:33  kuznets
 * Bulk changes in lds library, code reorganizations, implemented top level
 * objects read, metainformation persistance implemented for top level objects...
 *
 * Revision 1.1  2003/05/22 18:57:17  kuznets
 * Work in progress
 *
 * ===========================================================================
 */

#endif
