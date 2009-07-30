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
 * Authors:  Douglas Slotta and Dennis Troup
 * File Description: The interface to Xerces-C
 */

#include "MzXmlReader.hpp"

#include <xercesc/sax2/Attributes.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/framework/XMLFormatter.hpp>
#include <xercesc/framework/MemBufFormatTarget.hpp>
#include <xercesc/framework/LocalFileInputSource.hpp>
#include <xercesc/sax/EntityResolver.hpp>

#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/stat.h>

// standard includes
#include <corelib/ncbistl.hpp>
#include <connect/ncbi_socket.h>

using namespace xercesc;
USING_SCOPE(ncbi);

MzXmlReader::MzXmlReader(const char * encoding, CRef<CMsHdf5> msHdf5) :
    DefaultHandler()
{
    m_msHdf5 = msHdf5;
    m_inMsRun = false;
    
    m_buffer.reset(new MemBufFormatTarget);
    m_formatter.reset(new XMLFormatter(encoding, 0, m_buffer.get(),
                                      XMLFormatter::NoEscapes,
                                      XMLFormatter::UnRep_CharRef));
    m_lastStartElement.clear();
    m_metadata.clear();
    initialize();
}

MzXmlReader::~MzXmlReader()
{
    terminate();
}

void MzXmlReader::characters(const XMLCh * const chars, const unsigned int  length)
{
    if (m_inMsRun) {
        m_formatter->formatBuf(chars, length);
        string value(reinterpret_cast<const char *>(m_buffer->getRawBuffer()));
        m_elementText += NStr::TruncateSpaces(value);
        m_buffer->reset();
    }
}

void MzXmlReader::printName(const XMLCh * const uri,
                            const XMLCh * const localname,
                            const XMLCh * const qname,
                            const string &      prefix)
{
    cout << prefix << toString(qname) << endl;
}

void MzXmlReader::endElement(const XMLCh * const uri,
                                 const XMLCh * const localname,
                                 const XMLCh * const qname)
{
    //printName(uri, localname, qname, "stop: ");
    string qnameStr = toString(qname);

    if (m_inMsRun) {
        if (qnameStr == "peaks") {
            m_peaksStack.push(m_elementText);
        } else {
            string xmlText = "";
            
            if (m_lastStartElement == qnameStr && qnameStr != "scan" && m_elementText.empty()) {
                xmlText += "/>\n";
            } else {
                if (!m_elementText.empty()) {
                    xmlText += ">\n" + m_elementText + "\n";
                }
                xmlText += "</" + qnameStr + ">\n";
            }
            m_lastStartElement.clear();

            if (m_scanStack.size() > 0) {
                m_scanInfoStack.top() += xmlText;
            } else {            
                m_metadata += xmlText;
            }

            if (qnameStr == "scan") {
                int scanNum = m_scanStack.top();
                m_scanStack.pop();
                int parentScan = 0;
                if (m_scanStack.size() > 0) parentScan = m_scanStack.top();
                vector<float> mz, it;
                convertPeaks(m_peaksCountStack.top(), m_peaksStack.top(), mz, it);
                m_msHdf5->addSpectrum(scanNum,
                                      m_msLevelStack.top(),
                                      parentScan,
                                      mz, it,
                                      m_scanInfoStack.top());
                m_peaksCountStack.pop();
                m_peaksStack.pop();
                m_scanInfoStack.pop();
                m_msLevelStack.pop();
            }
        
            if (qnameStr == "msRun") {
                m_inMsRun = false;
                m_msHdf5->addMetadata(m_metadata);
                m_msHdf5->writeSpectra();
            }
        }
        m_elementText.clear();
    }   
}

void MzXmlReader::startElement(const XMLCh * const uri,
                               const XMLCh * const localname,
                               const XMLCh * const qname,
                               const Attributes &  attrs)
{
    string qnameStr = toString(qname);
    
    if (qnameStr == "msRun") {
        m_inMsRun = true;
    }
    
    if (m_inMsRun) {

        if (qnameStr != "peaks") {
            string xmlText = "";
            
            if (!m_lastStartElement.empty() && m_lastStartElement != "scan") {
                xmlText += ">\n";
            }

            xmlText += "<" + qnameStr;
            for (uint i = 0; i < attrs.getLength(); i++) {
                string name = toString(attrs.getQName(i));
                string value = toString(attrs.getValue(i));
                xmlText += " " + name + "=\"" + value + "\"";
            }
            m_lastStartElement = qnameStr;

            if (qnameStr == "scan") {
                xmlText += ">\n";
                string scanIdStr = toString(attrs.getValue(XMLString::transcode("num")));
                Uint4 scanId = NStr::StringToUInt(scanIdStr);
                m_scanStack.push(scanId);
                string peaksCount = toString(attrs.getValue(XMLString::transcode("peaksCount")));
                m_peaksCountStack.push(NStr::StringToUInt(peaksCount));
                string msLevel = toString(attrs.getValue(XMLString::transcode("msLevel")));
                m_msLevelStack.push(NStr::StringToUInt(msLevel));
                m_scanInfoStack.push(xmlText);
            } else if (m_scanStack.size() > 0) {
                m_scanInfoStack.top() += xmlText;
            } else {            
                m_metadata += xmlText;
            }
        }
        m_elementText.clear();
    }
    
}

void MzXmlReader::addAttributes(const Attributes &attrs)
{
    for (uint i = 0; i < attrs.getLength(); i++) {
        string name = toString(attrs.getQName(i));
        string value = toString(attrs.getValue(i));
        m_msHdf5->addAttribute(name, value);
    }
}

void MzXmlReader::addScanAttributes(const Attributes &attrs)
{
    for (uint i = 0; i < attrs.getLength(); i++) {
        string name = toString(attrs.getQName(i));
        string value = toString(attrs.getValue(i));
        if (!NStr::EqualCase(name, "num") && 
            !NStr::EqualCase(name, "peaksCount") &&
            !NStr::EqualCase(name, "lowMz") && 
            !NStr::EqualCase(name, "highMz")) {
            m_msHdf5->addAttribute(name, value);
        }
    }
}

void MzXmlReader::warning(const SAXParseException & exception)
{
    recordError(exception, 0);
}

void MzXmlReader::error(const SAXParseException & exception)
{
    recordError(exception, 1);
}

void MzXmlReader::recordError(const SAXParseException & exception,
                                  int                       severity)
{
    cerr << (severity == 0 ? "Warning" : "Error") << ": ";
    
    int lineNumber(exception.getLineNumber());
    if (lineNumber != -1) {
        cerr << "At line #" << lineNumber << ", ";
    }
    
    int columnNumber(exception.getColumnNumber());
    if (columnNumber != -1) {
        cerr << "column #" << columnNumber << ", ";
    }
    
    char * message(XMLString::transcode(exception.getMessage()));
    cerr << message << "\n";
    XMLString::release(&message);
}

InputSource * MzXmlReader::resolveEntity(const XMLCh * const publicId,
                                             const XMLCh * const systemId)
{
    string publicLocation(toString(publicId));
    string systemLocation(toString(systemId));
    
    string localFile(findXMLDefinition(publicLocation, systemLocation));
    
    InputSource * newSource(0);
    
    if (!localFile.empty()) {
        XMLCh * xmlFile = XMLString::transcode(localFile.c_str());
        newSource = new LocalFileInputSource(xmlFile);
        XMLString::release(&xmlFile);
    }
    
    return newSource;
}

string MzXmlReader::findXMLDefinition(const string & publicLocation,
                                          const string & systemLocation) const
{
    // Prefer the locally defined location over the schema defined location.
    string localFile;
    if (systemLocation.find("mzXML_2.0") == string::npos) {
        if (systemLocation.find("mzXML_idx") == string::npos) {
            //localFile = "/panfs/pan1/geo_dev/bin/config/mzXML_2.1.xsd";
            localFile = "/home/slottad/NCBI/schema/mzXML/mzXML_2.1.xsd";
        }
        else {
            //localFile = "/panfs/pan1/geo_dev/bin/config/mzXML_idx_2.1.xsd";
            localFile = "/home/slottad/NCBI/schema/mzXML/mzXML_idx_2.1.xsd";
        }
    }
    else {
        if (systemLocation.find("mzXML_idx") == string::npos) {
            //localFile = "/panfs/pan1/geo_dev/bin/config/mzXML_2.0.xsd";
            localFile = "/home/slottad/NCBI/schema/mzXML/mzXML_2.0.xsd";
        }
        else {
            //localFile = "/panfs/pan1/geo_dev/bin/config/mzXML_idx_2.0.xsd";
            localFile = "/home/slottad/NCBI/schema/mzXML_idx_2.0.xsd";
        }
    }
    
    struct stat info;
    if (localFile.empty() ||
        (stat(localFile.c_str(), &info) || !S_ISREG(info.st_mode))) {
        localFile.clear();
    }
	
    return localFile;
}

void MzXmlReader::initialize()
{
    if (++m_activeInstances == 1) {
        try {
            XMLPlatformUtils::Initialize();
        }
        catch (const XMLException & toCatch) {
            char * message(XMLString::transcode(toCatch.getMessage()));
            cerr << "Error: " << message << "\n";
            XMLString::release(&message);
        }
    }
}

void MzXmlReader::terminate()
{
    if (--m_activeInstances == 0) {
        XMLPlatformUtils::Terminate();
    }
}

string MzXmlReader::toString(const XMLCh * const name)
{
    string value;
    if (name) {
        char * char_value(XMLString::transcode(name));
        value = char_value;
        XMLString::release(&char_value);
    }
  
    return value;
}

void MzXmlReader::convertPeaks(Uint4 peakCount, string &peaks, vector<float> &mz, vector<float> &it) 
{
    if (peakCount >= 1) {
        int nFloats = (peakCount+1) * 2;
        Uint4 *buf = new Uint4[nFloats];
        
        size_t bRead, bWritten;
        BASE64_Decode(peaks.c_str(), peaks.size()+1,
                      &bRead, buf, sizeof(Uint4)*nFloats, &bWritten);
        
        //float* data = new float[ peakCount * 2 ];
        float* data = new float[2];
        mz.reserve(peakCount);
        it.reserve(peakCount);
        for (Uint4 n=0; n<(peakCount*2); n+=2) {
            // Get M/Z
            Uint4 val1 =  SOCK_NetToHostLong(buf[n]);
            ((Uint4 *)data)[0] = val1;
            mz.push_back(data[0]);
            
            // Get Intensity (or abundance as OMSSA calls it)
            Uint4 val2 =  SOCK_NetToHostLong(buf[n+1]);
            ((Uint4 *)data)[1] = val2;
            it.push_back(data[1]);
        }
        delete [] data;
        delete [] buf;
    }

}

unsigned int MzXmlReader::m_activeInstances(0);

