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
 * Author:  Douglas Slotta
 * File Description: SAX parser of mzXML via xmlwrapp
 */

// standard includes
#include <ncbi_pch.hpp>
#include <corelib/ncbistl.hpp>
#include <connect/ncbi_socket.h>

#include "MzXmlReader.hpp"

#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/stat.h>

//using namespace xercesc;
USING_SCOPE(ncbi);

MzXmlReader::MzXmlReader(CRef<CMsHdf5> msHdf5)
{
    m_msHdf5 = msHdf5;
    m_inMsRun = false;
    m_lastStartElement.clear();
    m_metadata.clear();
    m_parentScanOverride = -1;
}

MzXmlReader::~MzXmlReader()
 {
}

bool MzXmlReader::start_element(const string &name, const attrs_type &attrs)
{
    if (!m_msHdf5) return true; // Just testing

    if (name == "msRun") {
        m_inMsRun = true;
    }
    
    if (m_inMsRun) {

        if (name != "peaks") {
            string xmlText = "";
            
            if (!m_lastStartElement.empty() && m_lastStartElement != "scan") {
                xmlText += ">\n";
            }

            xmlText += "<" + name;
            ITERATE(attrs_type, iAttr, attrs) {
                xmlText += " " + iAttr->first + "=\"" + iAttr->second + "\"";
            }
            m_lastStartElement = name;

            if (name == "precursorMz") {
                attrs_type::const_iterator iA;
                iA = attrs.find("precursorScanNum");
                if (iA != attrs.end()) {
                    m_parentScanOverride = NStr::StringToUInt(iA->second);
                } else {
                    m_parentScanOverride = -1;
                }
            }

            if (name == "scan") {
                xmlText += ">\n";
                attrs_type::const_iterator iAt;
                iAt = attrs.find("num");
                Uint4 scanId = NStr::StringToUInt(iAt->second);
                m_scanStack.push(scanId);
                iAt = attrs.find("peaksCount");
                string peaksCount = iAt->second;
                m_peaksCountStack.push(NStr::StringToUInt(peaksCount));
                iAt = attrs.find("msLevel");
                string msLevelStr = iAt->second;
                Uint4 msLevel = NStr::StringToUInt(msLevelStr);
                //if (msLevel > 2) {
                //    cerr << "Contains msLevel = " + msLevelStr << endl;
                //}
                m_msLevelStack.push(msLevel);
                m_scanInfoStack.push(xmlText);
            } else if (m_scanStack.size() > 0) {
                m_scanInfoStack.top() += xmlText;
            } else {            
                m_metadata += xmlText;
            }
        }
        m_elementText.clear();
    }
    return true;    
}

bool MzXmlReader::end_element(const string &name)
{
    if (!m_msHdf5) return true; // Just testing

    if (m_inMsRun) {
        if (name == "peaks") {
            m_peaksStack.push(m_elementText);
        } else {
            string xmlText = "";
            
            if (m_lastStartElement == name && name != "scan" && m_elementText.empty()) {
                xmlText += "/>";
            } else {
                if (!m_elementText.empty()) {
                    xmlText += ">" + m_elementText;
                }
                xmlText += "</" + name + ">";
            }
            m_lastStartElement.clear();

            if (m_scanStack.size() > 0) {
                m_scanInfoStack.top() += xmlText;
            } else {            
                m_metadata += xmlText;
            }

            if (name == "scan") {
                int scanNum = m_scanStack.top();
                m_scanStack.pop();
                int parentScan = 0;
                if (m_scanStack.size() > 0) parentScan = m_scanStack.top();
                if (m_parentScanOverride > 0) {
                    parentScan = m_parentScanOverride;
                    m_parentScanOverride = -1;
                }
                vector<float> mz, it;
                convertPeaks(m_peaksCountStack.top(), m_peaksStack.top(), mz, it);
                m_msHdf5->addSpectrum(scanNum, m_msLevelStack.top(), parentScan,
                                      mz, it, m_scanInfoStack.top());
                m_peaksCountStack.pop();
                m_peaksStack.pop();
                m_scanInfoStack.pop();
                m_msLevelStack.pop();
            }
        
            if (name == "msRun") {
                m_inMsRun = false;
                m_msHdf5->addMetadata(m_metadata);
                m_msHdf5->writeSpectra();
            }
        }
        m_elementText.clear();
    }   
    return true;
}


bool MzXmlReader::text(const string &data)
{
    if (!m_msHdf5) return true; // Just testing

    if (m_inMsRun) {
        m_elementText += NStr::TruncateSpaces(data);
    }
    return true;
}

bool MzXmlReader::warning(const string &message)
{
    cerr << message << endl;
    return false;
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

