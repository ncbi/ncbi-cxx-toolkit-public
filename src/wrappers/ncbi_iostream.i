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
 * File Description:  SWIG interface file for std streams
 *
 */


%{
#include <iostream>
#include <fstream>
#include <sstream>
%}


namespace std {

    class ios_base
    {
    public:
        typedef int openmode;
        typedef int iostate;
        typedef int fmtflags;

        static const fmtflags adjustfield;
        static const fmtflags basefield;
        static const fmtflags boolalpha;
        static const fmtflags dec;
        static const fmtflags fixed;
        static const fmtflags floatfield;
        static const fmtflags hex;
        static const fmtflags internal;
        static const fmtflags left;
        static const fmtflags oct;
        static const fmtflags right;
        static const fmtflags scientific;
        static const fmtflags showbase;
        static const fmtflags showpoint;
        static const fmtflags showpos;
        static const fmtflags skipws;
        static const fmtflags unitbuf;
        static const fmtflags uppercase;

        static const iostate badbit;
        static const iostate eofbit;
        static const iostate failbit;
        static const iostate goodbit;

        static const openmode app;
        static const openmode ate;
        static const openmode binary;
#ifdef SWIGPYTHON
        // "in" is a Python keyword; rename it to "in_"
        %rename(in_) in;
#endif
        static const openmode in;
        static const openmode out;
        static const openmode trunc;

        fmtflags flags(void) const;
        fmtflags setf(fmtflags flags);
        fmtflags setf(fmtflags flags, fmtflags mask);
        void unsetf(fmtflags mask);

        // "int" should really be "streamsize"
        int precision(void) const;
        int precision(int n);
        int width(void) const;
        int width(int n);
        bool sync_with_stdio(bool sync = true);
        // locale imbue(const locale& loc);
        // locale getloc(void) const;

        static int xalloc();
        long& iword(int idx);
        void*& pword(int idx);

    protected:
        ios_base(void);
    private:
        ios_base(const ios_base& other);
        ios_base& operatore=(const ios_base& rhs);
        
    };
    
    class ostream;

    class ios : public ios_base
    {
    public:
        bool fail(void);
        bool good(void);
        bool bad(void);
        bool eof(void);
        void clear(void);
        void clear(iostate state);
        void setstate(iostate state);
        iostate rdstate(void);
        iostate exceptions(void) const;
        void exceptions(iostate state);

        // hmm...thinking about this
        //bool operator!(void) const;

        static const ios::seekdir beg;
        static const ios::seekdir end;
        static const ios::seekdir cur;

        ostream* tie(void) const;
        ostream* tie(ostream* os);

        // rdbuf()

        ios& copyfmt(const ios& other);
        
        char fill(void) const;
        char fill(char ch);

        // locale stuff

    protected:
        // there is a public constructor too, but let's not deal with it
        ios(void);
    };


    class istream : public ios
    {
    public:
        typedef long pos_type;
        typedef long off_type;

        int      get(void);
        istream& get(char& c);
        istream& get(char* str, int count);
        istream& get(char* str, int count, char delim);
        istream& getline(char* str, int count);
        istream& getline(char* str, int count, char delim);
        istream& read(char* str, int count);
        int      readsome(char* str, int count);
        int      gcount(void) const;
        istream& ignore(void);
        istream& ignore(int count);
        istream& ignore(int count, char delim);
        int      peek(void);
        istream& unget(void);
        istream& putback(char c);
        istream& seekg(pos_type pos);
        istream& seekg(off_type ot, ios::seekdir dir);
        pos_type tellg(void);

        istream& operator>>(istream& (*manip)(istream&));
        istream& operator>>(ios& (*manip)(ios&));
        istream& operator>>(ios_base& (*manip)(ios_base&));

    protected:
        istream(void);
    };

    %extend istream {
    public:
        std::string getline(void) {
            std::string rv;
            std::getline(*self, rv);
            return rv;
        }
        std::string getline(char delim) {
            std::string rv;
            std::getline(*self, rv, delim);
            return rv;
        }
        std::string read(int count) {
            char *buf = new char[count];
            self->read(buf, count);
            std::string rv(buf, self->gcount());
            delete buf;
            return rv;
        }
        std::string readsome(int count) {
            char *buf = new char[count];
            self->readsome(buf, count);
            std::string rv(buf, self->gcount());
            delete buf;
            return rv;
        }
    }


    istream& getline(istream& istr, std::string& s);
    istream& getline(istream& istr, std::string& s, char delim);


    class ifstream : public istream
    {
    public:
        ifstream(void);
        ifstream(const char *file_name);
        ifstream(const char *file_name, openmode flags);
        virtual ~ifstream();
        bool is_open(void);
        void open(const char *file_name);
        void open(const char *file_name, openmode flags);
        void close(void);
    };


    %immutable cin;
    istream cin;


    class ostream : public ios
    {
    public:
        typedef long pos_type;
        typedef long off_type;

        virtual  ~ostream();

        ostream& operator<<(ostream& (*manip)(ostream&));
        ostream& operator<<(ios& (*manip)(ios&));
        ostream& operator<<(ios_base& (*manip)(ios_base&));

        //ostream& operator<<(bool b);
        ostream& operator<< (long n);
        ostream& operator<<(double x);
        //ostream& operator<<(const void *ptr);
        //ostream& operator<<(const char *str);
        ostream& put(char ch);
        ostream& write(const char *str, int count);
        ostream& flush(void);
        ostream& seekp(pos_type pos);
        ostream& seekp(off_type ot, ios::seekdir dir);
        pos_type tellp(void);
    protected:
        // to fake out swig
        ostream(void);
    };
    
    %extend ostream {
    public:
        ostream& operator<< (const std::string& str) {*self << str; return *self;};
        ostream& write(const string& str) {
            self->write(str.c_str(), str.size());
            return *self;
        }
    }


    class ofstream : public ostream
    {
    public:
        ofstream(void);
        ofstream(const char *file_name);
        ofstream(const char *file_name, openmode flags);
        virtual ~ofstream();
        bool is_open(void);
        void open(const char *file_name);
        void open(const char *file_name, openmode flags);
        void close(void);
    };


    %immutable cout;
    ostream cout;
    %immutable cerr;
    ostream cerr;
    %immutable clog;
    ostream clog;


    class iostream : public istream, public ostream
    {
    protected:
        iostream(void);
    };


    // string streams, omitting rdbuf()

    class ostringstream : public ostream
    {
    public:
        // Simplified constructors
        ostringstream(void);
        ostringstream(const string& str);
        
        // Get/set the string
        string str(void) const;
        void str(const string& buf);
    };

#ifdef SWIGPYTHON
%callback(1) endl;
%callback(1) flush;
#endif
    ostream& endl(ostream& os);
    ostream& flush(ostream& os);

    class istringstream : public istream
    {
    public:
        // Simplified constructors
        istringstream(void);
        istringstream(const string& str);
        
        // Get/set the string
        string str(void) const;
        void str(const string& buf);
    };
    

    class stringstream : public iostream
    {
    public:
        // Simplified constructors
        stringstream(void);
        stringstream(const string& str);
        
        // Get/set the string
        string str(void) const;
        void str(const string& buf);
    };
    
}

#if 0
%rename(endl) my_endl;
%{
    std::ostream& my_endl(std::ostream& ostr) {return std::endl(ostr);}
%}
%constant std::ostream& my_endl(std::ostream& ostr);
#endif

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/05/11 21:27:35  jcherry
 * Initial version
 *
 * ===========================================================================
 */

