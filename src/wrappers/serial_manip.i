
// Enable manipulator syntax for some stream manipulators
#ifdef SWIGPYTHON
%callback(1) MSerial_AsnText;
%callback(1) MSerial_AsnBinary;
%callback(1) MSerial_Xml;
%callback(1) MSerial_Json;
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
ios& MSerial_Json(ios& io);
ios& MSerial_VerifyDefault(ios& io);
ios& MSerial_VerifyNo(ios& io);
ios& MSerial_VerifyYes(ios& io);
ios& MSerial_VerifyDefValue(ios& io);
}
#endif

#ifdef SWIGPERL

// SWIG screws up on CNcbiIos typedef and callbacks,
// so provide SWIG with non-typedef versions of manipulators
namespace ncbi {
%constant ios& MSerial_AsnText(ios& io);
%constant ios& MSerial_AsnBinary(ios& io);
%constant ios& MSerial_Xml(ios& io);
%constant ios& MSerial_Json(ios& io);
%constant ios& MSerial_VerifyDefault(ios& io);
%constant ios& MSerial_VerifyNo(ios& io);
%constant ios& MSerial_VerifyYes(ios& io);
%constant ios& MSerial_VerifyDefValue(ios& io);
}

#endif

%ignore ncbi::MSerial_AsnText;
%ignore ncbi::MSerial_AsnBinary;
%ignore ncbi::MSerial_Xml;
%ignore ncbi::MSerial_Json;
%ignore ncbi::MSerial_VerifyYes;
%ignore ncbi::MSerial_VerifyNo;
%ignore ncbi::MSerial_VerifyDefault;
%ignore ncbi::MSerial_VerifyDefValue;

#ifdef SWIGPYTHON
// support operator>> and operator<< for manipulators with arguments
%extend ncbi::MSerial_Flags {
    std::istream& __rrshift__(std::istream& istr) {
        return istr >> *self;
    };
    // support opeator<< with streams
    std::ostream& __rlshift__(std::ostream& ostr) {
        return ostr << *self;
    };
}
#endif

#ifdef SWIGPERL
// support operator>> and operator<< for manipulators with arguments
%extend std::istream {
    std::istream& __rshift__(const ncbi::MSerial_Flags& manip) {
        return *self >> manip;
    }
}
%extend std::ostream {
    std::ostream& __lshift__(const ncbi::MSerial_Flags& manip) {
        return *self << manip;
    }
}
#endif
