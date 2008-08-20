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
                                     CNcbiStreampos     stream_pos);

    // Event function alled after top object deserialization
    virtual void OnTopObjectFoundPost(const CObjectInfo& object);

    // Overload from CObjectsSniffer
    virtual void OnObjectFoundPre(const CObjectInfo& object, 
                                  CNcbiStreampos     stream_pos);

    // Overload from CObjectsSniffer
    virtual void OnObjectFoundPost(const CObjectInfo& object);

    // Overload from CObjectsSniffer
    virtual void Reset();

public:

    struct SObjectDetails
    {
        CObjectInfo    info;
        CNcbiStreampos offset;
        CNcbiStreampos parent_offset;
        CNcbiStreampos top_level_offset;
        bool           is_top_level;
        int            ext_id;  // Database id or any other external id

        SObjectDetails(const  CObjectInfo& object_info,
                       CNcbiStreampos stream_offset,
                       CNcbiStreampos p_offset,
                       CNcbiStreampos top_offset,
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
    typedef map<CNcbiStreampos, SObjectDetails*> TObjectIndex;

    // Find object information based on the stream offset.
    // Return NULL if not found.
    // This function works on the fact that only one object can 
    // be found in one particular offset, and offset udentifies any
    // object unqiely in its file.
    SObjectDetails* FindObjectInfo(CNcbiStreampos pos);

    TObjectVector& GetObjectsVector() { return m_Objects; }

    void ClearObjectsVector();

protected:

    struct SObjectParseDescr
    {
        const CObjectInfo*  object_info;
        CNcbiStreampos      stream_pos;

        SObjectParseDescr(const CObjectInfo* oi,
                          CNcbiStreampos     pos)
        : object_info(oi),
          stream_pos(pos)
        {}
        SObjectParseDescr() 
        : object_info(0),
          stream_pos(0)
        {}
    };

    typedef stack<SObjectParseDescr>  TParseStack;

private:
    TParseStack         m_Stack;
    SObjectParseDescr   m_TopDescr;
    TObjectVector       m_Objects;
    TObjectIndex        m_ObjectIndex;
};



END_SCOPE(objects)
END_NCBI_SCOPE

#endif
