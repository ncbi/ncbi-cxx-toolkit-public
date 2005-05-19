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
 * Authors:  Josh Cherry
 *
 * File Description:  Extensions to and for CSerialObject's
 *
 */


%extend ncbi::CSerialObject {
public:
    std::string Serialize(ncbi::ESerialDataFormat format) const {
        ncbi::CNcbiOstrstream buf;
        {{
            std::auto_ptr<ncbi::CObjectOStream>
                ostr(ncbi::CObjectOStream::Open(format, buf));
            ostr->Write(self, self->GetThisTypeInfo());
        }}
        return ncbi::CNcbiOstrstreamToString(buf);
    };
    std::string Asn(void) const {
        return ncbi_CSerialObject_Serialize(self, ncbi::eSerial_AsnText);
    };
    std::string AsnBin(void) const {
        return ncbi_CSerialObject_Serialize(self, ncbi::eSerial_AsnBinary);
    };
    std::string Xml(void) const {
        return ncbi_CSerialObject_Serialize(self, ncbi::eSerial_Xml);
    };

    void WriteAsn(ostream& ostr) const {
        ostr << MSerial_AsnText << *self;
    };
    void WriteAsnBin(ostream& ostr) const {
        ostr << MSerial_AsnBinary << *self;
    };
    void WriteXml(ostream& ostr) const {
        ostr << MSerial_Xml << *self;
    };

    void WriteAsn(const std::string& fname) const {
        ofstream ostr(fname.c_str());
        ostr << MSerial_AsnText << *self;
    };
    void WriteAsnBin(const std::string& fname) const {
        ofstream ostr(fname.c_str(), ios::binary|ios::out);
        ostr << MSerial_AsnBinary << *self;
    };
    void WriteXml(const std::string& fname) const {
        ofstream ostr(fname.c_str());
        ostr << MSerial_Xml << *self;
    };

    void Deserialize(const std::string& str, ncbi::ESerialDataFormat format) {
        ncbi::CNcbiIstrstream buf(str.data(), str.size());
        std::auto_ptr<ncbi::CObjectIStream>
            istr(ncbi::CObjectIStream::Open(format, buf));
        istr->Read(self, self->GetThisTypeInfo());
    };
    void FromAsn(const std::string& str) {
        ncbi_CSerialObject_Deserialize(self, str, ncbi::eSerial_AsnText);
    };
    void FromAsnBin(const std::string& str) {
        ncbi_CSerialObject_Deserialize(self, str, ncbi::eSerial_AsnBinary);
    };
    void FromXml(const std::string& str) {
        ncbi_CSerialObject_Deserialize(self, str, ncbi::eSerial_Xml);
    };

    void DeserializeFile(const std::string& fname,
                         ncbi::ESerialDataFormat format) {
        std::auto_ptr<ncbi::CObjectIStream>
            istr(ncbi::CObjectIStream::Open(format, fname));
        istr->Read(self, self->GetThisTypeInfo());
    };
    void FromAsnFile(const std::string& fname) {
        ncbi_CSerialObject_DeserializeFile(self, fname,
                                           ncbi::eSerial_AsnText);
    };
    void FromAsnBinFile(const std::string& fname) {
        ncbi_CSerialObject_DeserializeFile(self, fname,
                                           ncbi::eSerial_AsnBinary);
    };
    void FromXmlFile(const std::string& fname) {
        ncbi_CSerialObject_DeserializeFile(self, fname,
                                           ncbi::eSerial_Xml);
    };

    void FromAsnFile(std::istream& istr) {
        istr >> MSerial_AsnText >> *self;
    };
    void FromAsnBinFile(std::istream& istr) {
        istr >> MSerial_AsnBinary >> *self;
    };
    void FromXmlFile(std::istream& istr) {
        istr >> MSerial_Xml >> *self;
    };
#ifdef SWIGPYTHON
    // support operator>> with streams
    std::istream& __rrshift__(std::istream& istr) {
        return istr >> *self;
    };
    // support opeator<< with streams
    std::ostream& __rlshift__(std::ostream& ostr) {
        return ostr << *self;
    };
#endif
};


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2005/05/19 18:41:43  jcherry
 * Make sure that binary ASN.1 files are opened in binary mode for reading
 *
 * Revision 1.1  2005/05/11 21:27:35  jcherry
 * Initial version
 *
 * ===========================================================================
 */
