#ifndef OBJ_SNIFF__HPP
#define OBJ_SNIFF__HPP

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
 * File Description: Methods of objects deserialization when the input 
 *                   format is uncertain.
 *
 */

#include <corelib/ncbistd.hpp>

#include <serial/objectinfo.hpp>
#include <serial/objistr.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)



//////////////////////////////////////////////////////////////////
//
// Serialized objects sniffer.
// Binary ASN format does not include any information on what 
// objects are encoded in any particular file. This class uses
// try and fail method to identify if the input stream contains
// any objects.
//
// Use AddCandidate function to tune the sniffer for recognition, 
// then call Probe to interrogate the stream.
// 
// NOTE: This method is not 100% accurate. Small probablitity 
// present that serialization code will be able to read the
// wrong type.
//

class NCBI_XOBJUTIL_EXPORT CObjectsSniffer
{
public:

    struct SObjectDescription 
    {
        CObjectTypeInfo  info;            // Type information class
        size_t           stream_offset;   // Offset in file

        SObjectDescription(const CObjectTypeInfo& object_info,
                           size_t offset)
        : info(object_info),
          stream_offset(offset)
        {}
    };

    typedef vector<SObjectDescription>  TTopLevelMapVector;

public:
    CObjectsSniffer() {}

    // Add new possible type to the recognition list.
    void AddCandidate(CObjectTypeInfo ti) { m_Candidates.push_back(ti); }

    // The main worker function. Tryes to identify if the input stream contains
    // any candidate objects. Function reads the stream up until it ends of
    // deserializer cannot recognize the input file format.
    void Probe(CObjectIStream& input);

    // Get map of all top level objects
    const TTopLevelMapVector& GetTopLevelMap() const { return m_TopLevelMap; }

    // Return TRUE if Probe found at least one top level objects
    bool IsTopObjectFound() const { return m_TopLevelMap.size() != 0; }

    // Return stream offset of the most recently found top object.
    // Note: If the top object has not been found return value is undefined.
    size_t GetStreamOffset() const { return m_StreamOffset; }

    // Event handling virtual function, called when candidate is found but 
    // before deserialization. This function can be overloaded in child
    // classes to implement some custom actions. This function is called before
    // deserialization.
    virtual void ObjectFoundPre(const CObjectInfo& object, 
                                size_t stream_offset);
    
    // Event handling virtual function, called when candidate is found
    // and deserialized.
    virtual void ObjectFoundPost(const CObjectInfo& object);

protected:
    void ProbeASN1_Text(CObjectIStream& input);
    void ProbeASN1_Bin(CObjectIStream& input);


private:
    typedef vector<CObjectTypeInfo> TCandidates;

    TCandidates         m_Candidates;  // Possible candidates for type probing
    TTopLevelMapVector  m_TopLevelMap; // Vector of level object descriptions
    size_t              m_StreamOffset;// Stream offset of the
};


END_SCOPE(objects)
END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2003/05/21 14:27:49  kuznets
 * Added methods ObjectFoundPre, ObjectFoundPost
 *
 * Revision 1.2  2003/05/19 16:38:37  kuznets
 * Added support for ASN text
 *
 * Revision 1.1  2003/05/16 19:34:32  kuznets
 * Initial revision.
 *
 * ===========================================================================
 */

#endif /* OBJ_SNIFF__HPP */

