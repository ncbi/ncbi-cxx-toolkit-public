// containers for common types

%template(vector_int) std::vector<int>;
%template(vector_long) std::vector<long>;
//%template(vector_long_long) std::vector<long long>;
%template(vector_unsigned_int) std::vector<unsigned int>;
%template(vector_unsigned_long) std::vector<unsigned long>;
//%template(vector_unsigned_long_long) std::vector<unsigned long long>;
%template(vector_size_t) std::vector<size_t>;
%template(vector_double) std::vector<double>;

// Special treatment of vector<char> to make its data() method
// return a pointer representation rather than a string
%typemap(out) orig_const_char_ptr = char *;
%typemap(out) char * = SWIGTYPE *;
%template(vector_char) std::vector<char>;
%typemap(out) char * = orig_const_char_ptr;

%template(vector_vector_char) std::vector<std::vector<char> >;
%template(vector_signed_char) std::vector<signed char>;
%template(vector_unsigned_char) std::vector<unsigned char>;
%template(vector_bool) std::vector<bool>;
%template(vector_string) std::vector<std::string>;

%template(list_int) std::list<int>;
%template(list_long) std::list<long>;
%template(list_unsigned_int) std::list<unsigned int>;
%template(list_unsigned_long) std::list<unsigned long>;
%template(list_size_t) std::list<size_t>;
%template(list_double) std::list<double>;
%template(list_char) std::list<char>;
%template(list_signed_char) std::list<signed char>;
%template(list_unsigned_char) std::list<unsigned char>;
%template(list_bool) std::list<bool>;
%template(list_string) std::list<std::string>;

#ifdef SWIGPYTHON
%template(set_int) std::set<int>;
%template(set_long) std::set<long>;
// unsigned int leads to compile error
//%template(set_unsigned_int) std::set<unsigned int>;
%template(set_unsigned_long) std::set<unsigned long>;
%template(set_size_t) std::set<size_t>;
%template(set_double) std::set<double>;
%template(set_char) std::set<char>;
%template(set_unsigned_char) std::set<unsigned char>;
%template(set_bool) std::set<bool>;

// Need this hack because we use SWIG's std::set but our std::string
%{
// only prototype this, because it's defined later in the generated .cpp
SWIGINTERNINLINE PyObject *
SWIG_FromCharArray(const char* carray, size_t size);

SWIGINTERNINLINE PyObject*
SWIG_From_std_string(const std::string& s)
{
    return SWIG_FromCharArray(s.data(), s.size());
}

SWIGINTERN int
SWIG_AsPtr_std_string(PyObject* obj, std::string **val)
{
    static swig_type_info* string_info = SWIG_TypeQuery("std::string *");
    std::string *vptr;    
    if (SWIG_ConvertPtr(obj, (void**)&vptr, string_info, 0) != -1) {
        if (val) *val = vptr;
        return SWIG_OLDOBJ;
    } else {
        PyErr_Clear();
        char* buf = 0 ; size_t size = 0;
        if (SWIG_AsCharPtrAndSize(obj, &buf, &size)) {
            if (buf) {
                if (val) *val = new std::string(buf, size - 1);
                return SWIG_NEWOBJ;
            }
        } else {
            PyErr_Clear();
        }  
        if (val) {
            PyErr_SetString(PyExc_TypeError,"a string is expected");
        }
        return 0;
    }
}

SWIGINTERN int
SWIG_AsVal_std_string(PyObject* obj, std::string *val)
{
    std::string* s;
    int res = SWIG_AsPtr_std_string(obj, &s);
    if ((res != 0) && s) {
        if (val) *val = *s;
        if (res == SWIG_NEWOBJ) delete s;
        return res;
    }
    if (val) {
        PyErr_SetString(PyExc_TypeError,"a string is expected");
    }
    return 0;
}
%}
%template(set_string) std::set<std::string>;
#endif

#ifdef SWIGPERL
namespace std {
specialize_std_map_on_both(int, foo, foo, foo,
                           int, foo, foo, foo)
}
#endif
%template(map_int_int) std::map<int, int>;
