
// Enable manipulator syntax for some stream manipulators
#ifdef SWIGPYTHON
%callback(1) MSerial_AsnText;
%callback(1) MSerial_AsnBinary;
%callback(1) MSerial_Xml;
%callback(1) MSerial_VerifyYes;
%callback(1) MSerial_VerifyNo;
%callback(1) MSerial_VerifyDefault;
%callback(1) MSerial_VerifyDefValue;

// SWIG screws up on CNcbiIos typedef and callbacks,
// so provide SWIG with non-typedef versions of manipulators
namespace ncbi {
ios& MSerial_AsnText(ios& io);
ios& MSerial_AsnBinary(ios& io);
ios& MSerial_Xml(ios& io);
ios& MSerial_VerifyDefault(ios& io);
ios& MSerial_VerifyNo(ios& io);
ios& MSerial_VerifyYes(ios& io);
ios& MSerial_VerifyDefValue(ios& io);
}

%ignore ncbi::MSerial_AsnText;
%ignore ncbi::MSerial_AsnBinary;
%ignore ncbi::MSerial_Xml;
%ignore ncbi::MSerial_VerifyYes;
%ignore ncbi::MSerial_VerifyNo;
%ignore ncbi::MSerial_VerifyDefault;
%ignore ncbi::MSerial_VerifyDefValue;

#endif
