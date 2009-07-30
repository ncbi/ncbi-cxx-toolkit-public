#if !defined(MZXML_READER_HPP)
#define MZXML_READER_HPP

/* ===========================================================================
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
 * $Id$
 * Author:  Douglas Slotta and Dennis Troup
 * File Description: The interface to Xerces-C
 */

#include <xercesc/sax2/DefaultHandler.hpp>

#include <string>
#include <memory>
#include <stack>

#include <algo/ms/formats/mshdf5/MsHdf5.hpp>

XERCES_CPP_NAMESPACE_BEGIN

class MemBufFormatTarget;
class XMLFormatter;
class SAXParseException;
class InputSource;

XERCES_CPP_NAMESPACE_END

USING_SCOPE(ncbi);

class MzXmlReader : public xercesc::DefaultHandler {
    
public:
    
    explicit MzXmlReader(const char * encoding, CRef<CMsHdf5> msHdf5);
    virtual ~MzXmlReader();
    
    virtual void characters(const XMLCh * const chars,
                            const unsigned int  length);

    virtual void endElement(const XMLCh * const uri,
                            const XMLCh * const localname,
                            const XMLCh * const qname);

    virtual void startElement(const XMLCh * const         uri,
                              const XMLCh * const         localname,
                              const XMLCh * const         qname,
                              const xercesc::Attributes & attrs);
    
    virtual void warning(const xercesc::SAXParseException & exception);
    virtual void error(const xercesc::SAXParseException & exception);
    
    virtual xercesc::InputSource * resolveEntity(const XMLCh * const publicId,
                                                 const XMLCh * const systemId);
    
    static void initialize();    
    static void terminate();
    
private:
    
    MzXmlReader(const MzXmlReader &);
    MzXmlReader & operator=(const MzXmlReader &);
    
    void addAttributes(const xercesc::Attributes &attrs);
    void addScanAttributes(const xercesc::Attributes &attrs);
    
    void printName(const XMLCh * const uri,
                   const XMLCh * const localname,
                   const XMLCh * const qname,
                   const string & prefix);
    
    void recordError(const xercesc::SAXParseException & exception,
                     int                                severity);
    
    string findXMLDefinition(const string & publicLocation, 
                             const string & systemLocation) const;
    
    auto_ptr<xercesc::MemBufFormatTarget> m_buffer;
    auto_ptr<xercesc::XMLFormatter>       m_formatter;
    
    string toString(const XMLCh * const name);

    void convertPeaks(Uint4 peakCount, string &peaks, vector<float> &mz, vector<float> &it);    
    
    string m_elementText;
   
    CRef<CMsHdf5> m_msHdf5;
    stack<Uint4> m_scanStack;
    stack<Uint4> m_peaksCountStack;
    stack<string> m_peaksStack;
    stack<string> m_scanInfoStack;
    stack<Uint4> m_msLevelStack;
    bool m_inMsRun;
    string m_lastStartElement;
    string m_metadata;


    static unsigned int m_activeInstances;
    
};

#endif
