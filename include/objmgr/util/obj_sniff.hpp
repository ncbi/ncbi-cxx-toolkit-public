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
#include <util/format_guess.hpp>

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
        CNcbiStreamoff   stream_offset;   // Offset in file

        SObjectDescription(const CObjectTypeInfo& object_info,
                           CNcbiStreamoff offset)
        : info(object_info),
          stream_offset(offset)
        {}
    };

    // Specifies how the OnObjectFoundPre and OnObjectFoundPost events will
    // be delivered
    enum EEventCallBackMode
    {
        eCallAlways,  // Default mode of operation
        eDoNotCall,   // Object deserialized, but OnObjectFound not called
        eSkipObject   // Object is skipped from deserialization
    };

    struct SCandidateInfo
    {
        CObjectTypeInfo     type_info;
        EEventCallBackMode  event_mode;

        SCandidateInfo(CObjectTypeInfo tinfo, EEventCallBackMode emode)
        : type_info(tinfo),
          event_mode(emode)
        {}
    };

    typedef vector<SObjectDescription>  TTopLevelMapVector;
    typedef vector<SCandidateInfo>      TCandidates;

    // List of objects, reflects objects serialization hierarchy
    typedef list<const CObjectInfo*> TObjectStack;



public:
    CObjectsSniffer() : m_DiscardCurrObj(false) {}
    virtual ~CObjectsSniffer() {}

    // Add new possible type to the recognition list.
    void AddCandidate(CObjectTypeInfo ti, 
                      EEventCallBackMode emode=eCallAlways);

    // Return reference on the internal vector of object candidates.
    const TCandidates& GetCandidates() const { return m_Candidates; }

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
    CNcbiStreamoff GetStreamOffset() const { return m_StreamOffset; }

    // Event handling virtual function, called when candidate is found but 
    // before deserialization. This function can be overloaded in child
    // classes to implement some custom actions. This function is called before
    // deserialization.
    virtual void OnObjectFoundPre(const CObjectInfo& object, 
                                  CNcbiStreamoff stream_offset);
    
    // Event handling virtual function, called when candidate is found
    // and deserialized.
    virtual void OnObjectFoundPost(const CObjectInfo& object);

    // Event indicates that sniffer objects needs to reset it's status and
    // get ready for the next probing.
    virtual void Reset() {}

    // Set the discard flag. If set TRUE current deserialized object is not
    // deserialized. 
    // The mechanizm is based on CObjectIStream::SetDiscardCurrObject
    void SetDiscardCurrObject(bool discard=true) { m_DiscardCurrObj = discard; }
    bool GetDiscardCurrObject() const { return m_DiscardCurrObj; }

protected:
    void ProbeASN1_Text(CObjectIStream& input);
    void ProbeASN1_Bin(CObjectIStream& input);

protected:
    TObjectStack         m_CallStack;
    
    friend class COffsetReadHook;
private:
    // Possible candidates for type probing
    TCandidates         m_Candidates;    
    // Vector of level object descriptions
    TTopLevelMapVector  m_TopLevelMap;   
    // Stream offset of the top level object
    CNcbiStreamoff      m_StreamOffset;  
    // Flag indicates that current object should be discarded
    bool                m_DiscardCurrObj;
};


// Return serialization format corresponding to CFormatGuess
inline ESerialDataFormat FormatGuess2Serial(CFormatGuess::EFormat fmt)
{
    switch (fmt) 
    {
    case CFormatGuess::eBinaryASN:
        return eSerial_AsnBinary;
    case CFormatGuess::eTextASN:
        return eSerial_AsnText;
    case CFormatGuess::eXml:
        return eSerial_Xml;
    case CFormatGuess::eFasta:
        return eSerial_None;       
    default:
        return eSerial_None;
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.16  2004/08/30 18:15:09  gouriano
 * use CNcbiStreamoff instead of size_t for stream offset operations
 *
 * Revision 1.15  2003/10/07 20:42:04  kuznets
 * + virtual Reset() function.
 * Called when object scan fails because of the format error and the
 * whole state machine should be reset.
 *
 * Revision 1.14  2003/09/09 20:22:40  kuznets
 * Fixed a bug in CObjectsSniffer::SetDiscardCurrObject()
 *
 * Revision 1.13  2003/08/28 16:15:23  kuznets
 * + SetDiscardCurrObject() method
 *
 * Revision 1.12  2003/08/25 14:27:24  kuznets
 * Added stack reflecting current serialization hooks call hierachy.
 *
 * Revision 1.11  2003/08/05 21:11:48  kuznets
 * +eSkipObject deserialization callback mode
 *
 * Revision 1.10  2003/08/05 14:31:06  kuznets
 * Implemented background "do not call" candidates for recognition.
 *
 * Revision 1.9  2003/07/14 19:25:20  kuznets
 * Cosmetic fix.
 *
 * Revision 1.8  2003/06/06 20:26:56  kuznets
 * Added inline function FormatGuess2Serial
 *
 * Revision 1.7  2003/06/02 16:01:38  dicuccio
 * Rearranged include/objects/ subtree.  This includes the following shifts:
 *     - include/objects/alnmgr --> include/objtools/alnmgr
 *     - include/objects/cddalignview --> include/objtools/cddalignview
 *     - include/objects/flat --> include/objtools/flat
 *     - include/objects/objmgr/ --> include/objmgr/
 *     - include/objects/util/ --> include/objmgr/util/
 *     - include/objects/validator --> include/objtools/validator
 *
 * Revision 1.4  2003/05/22 16:46:46  kuznets
 * ObjectFound methods renamed OnObjectFound, added GetCandidates() method
 *
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

