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
 * Authors: Rafael Sadyrov
 *
 */

#include <ncbi_pch.hpp>

#include <common/test_data_path.h>
#include <corelib/test_boost.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_sftp.hpp>

#include <optional>
#include <unordered_set>
#include <variant>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


// A copy-paste from test_ncbi_conn_stream.cpp
static bool s_GetFtpCreds(string& user, string& pass)
{
    user.clear();
    pass.clear();
    string path = NCBI_GetTestDataPath();
    path += "/ftp/test_ncbi_ftp_upload";
    ifstream ifs(path.c_str());
    if (ifs) {
        string src;
        ifs >> src;
        ifs.close();
        string dst(src.size(), '\0');
        size_t n_read, n_written;
        BASE64_Decode(src.c_str(), src.size(), &n_read,
                      &dst[0], dst.size(), &n_written);
        dst.resize(n_written);
        NStr::SplitInTwo(dst, ":", user, pass);
    }
    return !user.empty()  &&  !pass.empty();
}


struct SGlobalFixture
{
    void setup()
    {
        auto& suite = boost::unit_test::framework::master_test_suite();

        string user, password;
        s_GetFtpCreds(user, password);

        if ((suite.argc == 2) && (string_view(suite.argv[1]) == "--ftp"sv)) {
            type = FTP;
            session.emplace<FTP>(user, password);
        } else {
            type = SFTP;
            session.emplace<SFTP>("sftp-private.ncbi.nlm.nih.gov", user, password);
        }
    }

    void teardown()
    {
        session.emplace<None>();
    }

    static bool IsSFTP() { return type == SFTP; }

    static unique_ptr<iostream> CreateStream(const string& path)
    {
        if (auto sftp_session = get_if<SFTP>(&session)) {
            return make_unique<CSFTP_Stream>(*sftp_session, path);

        } else if (auto cred = get_if<FTP>(&session)) {
            return make_unique<CConn_FtpStream>("ftp-private.ncbi.nlm.nih.gov", cred->first, cred->second, path);

        } else {
            BOOST_FAIL("Unknown stream type requested");
            return {};
        }
    }

private:
    inline static enum EVariantType : size_t { None, SFTP, FTP } type = None;
    inline static variant<monostate, CSFTP_Session, pair<string, string>> session;
};

struct SDefaultDirFixture
{
    const string default_path;

    unique_ptr<iostream> stream_holder;
    iostream& stream;
    string line;

    SDefaultDirFixture(const string dp = "/test_ncbi_sftp"s) :
        default_path(dp),
        stream_holder(SGlobalFixture::CreateStream(default_path)),
        stream(*stream_holder)
    {
    }

    template <class... TArgs>
    void Test(string_view command, TArgs&&... args)
    {
        BOOST_TEST_INFO_SCOPE(command);
        stream << command << endl;
        x_Test(std::forward<TArgs>(args)...);
        stream.clear();
    }

private:
    void x_Test()
    {
        BOOST_CHECK(!getline(stream, line));
        BOOST_CHECK_EQUAL(stream.rdstate(), ios_base::failbit | ios_base::eofbit);
    }

    void x_Test(string_view expected)
    {
        BOOST_TEST_INFO_SCOPE("With expected=" << expected);
        BOOST_CHECK(getline(stream, line));
        BOOST_CHECK_EQUAL(line, expected);
        BOOST_CHECK(!getline(stream, line));
        BOOST_CHECK_EQUAL(stream.rdstate(), ios_base::failbit | ios_base::eofbit);
    }

    void x_Test(set<string> expected)
    {
        set<string> actual;

        while (getline(stream, line)) {
            if (line.back() == '\r') {
                line.replace(line.size() - 1, 1, "\\r"sv);
            }

            actual.insert(std::move(line));
        }

        BOOST_CHECK_EQUAL_COLLECTIONS(actual.begin(), actual.end(), expected.begin(), expected.end());
        BOOST_CHECK_EQUAL(stream.rdstate(), ios_base::failbit | ios_base::eofbit);
    }

    template <class TExpected>
    void x_Test(array<TExpected, 2> expected)
    {
        x_Test(expected[SGlobalFixture::IsSFTP() ? 0 : 1]);
    }

    void x_Test(string_view data, string_view expected)
    {
        BOOST_TEST_INFO_SCOPE("With data=" << data);
        stream << data;
        x_Test(expected);
    }
};

struct SModelDirFixture : SDefaultDirFixture
{
    SModelDirFixture() : SDefaultDirFixture("/test_ncbi_sftp/model/dir"s) {}
};

NCBITEST_AUTO_INIT()
{
    boost::unit_test::framework::master_test_suite().p_name->assign("SFTP API Tests");
}

BOOST_TEST_GLOBAL_FIXTURE(SGlobalFixture);

BOOST_FIXTURE_TEST_SUITE(DefaultDir, SDefaultDirFixture)

BOOST_AUTO_TEST_CASE(CurrentPath)
{
    Test("CWD model//", "250");
    Test("PWD", default_path + "/model");
    Test("CDUP", "200");
    Test("CWD model/", "250");
    Test("PWD", default_path + "/model");
    Test("CDUP", "200");
    Test("CWD model", "250");
    Test("PWD", default_path + "/model");
    Test("CWD " + default_path + "/model//", "250");
    Test("PWD", default_path + "/model");
    Test("CWD " + default_path + "/model/", "250");
    Test("PWD", default_path + "/model");
    Test("CWD " + default_path + "/model", "250");
    Test("PWD", default_path + "/model");
    Test("CDUP", "200");
    Test("PWD", default_path);
    Test("CWD non-existent/");
    Test("PWD", default_path);
    Test("CWD " + default_path + "/non-existent/");
    Test("PWD", default_path);
    Test("CDUP", "200");
    Test("PWD", "/");
    Test("CDUP", "200");
    Test("PWD", "/");
}

BOOST_AUTO_TEST_CASE(FilesAndDirs)
{
    Test("REST 0", "350");
    Test("STOR file1", "00000000000000000000", "20");
    Test("RETR file1", "00000000000000000000");
    Test("STOR file1", "111111111", "9");
    Test("RETR file1", "111111111");
    Test("REST 4", "350");
    Test("STOR file1", "2222", "4");
    Test("RETR file1", "111122221");
    Test("REST 8", "350");
    Test("STOR file1", "33", "2");
    Test("RETR file1", "1111222233");

    Test("REN file1 file2", "250");

    Test("REST 0", "350");
    Test("APPE file2", "444", "3");
    Test("RETR file2", "1111222233444");
    Test("APPE file2", "5", "1");
    Test("RETR file2", "11112222334445");
    Test("REST 5", "350");
    Test("APPE file2", "66", "2");
    Test("RETR file2", "11112662334445");
    Test("REST 9", "350");
    Test("APPE file2", "77", "2");
    Test("RETR file2", "11112662377445");

    Test("DELE file2", "250");

    // coremake does not have permissions for these at ftp-private
    if (SGlobalFixture::IsSFTP()) {
        Test("MKD dir1", default_path + "/dir1");
        Test("RMD dir1", "250");
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(ModelDir, SModelDirFixture)

BOOST_AUTO_TEST_CASE(MDTM)
{
    Test("MDTM non-existent");
    Test("MDTM file", "1743652800");
    Test("MDTM .hidden_file", "1743652800");
    Test("MDTM link", "1743652800");
    Test("MDTM .hidden_link", "1743652800");
    Test("MDTM dir/non-existent");
    Test("MDTM dir/another_file", "1743652800");
    Test("MDTM dir/.another_hidden_file", "1743652800");
    Test("MDTM dir/another_link", "1743652800");
    Test("MDTM dir/.another_hidden_link", "1743652800");
}

BOOST_AUTO_TEST_CASE(SIZE)
{
    Test("SIZE non-existent");
    Test("SIZE file", "4");
    Test("SIZE .hidden_file", "11");
    Test("SIZE link", "4");
    Test("SIZE .hidden_link", "11");
    Test("SIZE dir/non-existent");
    Test("SIZE dir/another_file", "12");
    Test("SIZE dir/.another_hidden_file", "19");
    Test("SIZE dir/another_link", "12");
    Test("SIZE dir/.another_hidden_link", "19");
}

BOOST_AUTO_TEST_CASE(NLST)
{
    set<string> current_dir{
            "fifo\\r",
            "dir\\r",
            "file\\r",
            "link\\r",
        },
        dir{
            "dir/another_fifo\\r",
            "dir/another_dir\\r",
            "dir/another_file\\r",
            "dir/another_link\\r",
        },
        alt_dir{
            "./dir/another_fifo\\r",
            "./dir/another_dir\\r",
            "./dir/another_file\\r",
            "./dir/another_link\\r",
        };

    Test("NLST", current_dir);
    Test("NLST .", current_dir);
    Test("NLST ./", current_dir);
    Test("NLST .//", current_dir);
    Test("NLST dir", dir);
    Test("NLST dir/", dir);
    Test("NLST dir//", dir);
    Test("NLST ./dir", alt_dir);
    Test("NLST ./dir/", alt_dir);
    Test("NLST ./dir//", alt_dir);
    Test("NLST .hidden_dir");
    Test("NLST dir/another_dir");
    Test("NLST dir/.another_hidden_dir");
}

BOOST_AUTO_TEST_CASE(LIST)
{
    array<set<string>, 2> current_dir{
            set<string>{
                "pr--r--r--    1 coremake coreprog        0 Apr  3 04:00 fifo\\r",
                "dr-xr-xr-x    4 coremake coreprog     4096 Apr  3 04:00 dir\\r",
                "-r--r--r--    1 coremake coreprog        4 Apr  3 04:00 file\\r",
                "lrwxrwxrwx    1 coremake coreprog        4 Apr  3 04:00 link\\r",
            },
            set<string>{
                "dr-xr-xr-x   4 coremake coreprog     4096 Apr  3 04:00 dir\\r",
                "pr--r--r--   1 coremake coreprog        0 Apr  3 04:00 fifo\\r",
                "-r--r--r--   1 coremake coreprog        4 Apr  3 04:00 file\\r",
                "lrwxrwxrwx   1 coremake coreprog        4 Apr  3 04:00 link -> file\\r",
            },
        },
        dir{
            set<string>{
                "-r--r--r--    1 coremake cppcore        12 Apr  3 04:00 another_file\\r",
                "dr-xr-xr-x    2 coremake coreprog     4096 Apr  3 04:00 another_dir\\r",
                "lrwxrwxrwx    1 coremake cppcore        12 Apr  3 04:00 another_link\\r",
                "pr--r--r--    1 coremake coreprog        0 Apr  3 04:00 another_fifo\\r",
            },
            set<string>{
                "dr-xr-xr-x   2 coremake coreprog     4096 Apr  3 04:00 another_dir\\r",
                "pr--r--r--   1 coremake coreprog        0 Apr  3 04:00 another_fifo\\r",
                "-r--r--r--   1 coremake cppcore        12 Apr  3 04:00 another_file\\r",
                "lrwxrwxrwx   1 coremake cppcore        12 Apr  3 04:00 another_link -> another_file\\r",
            },
        };

    Test("LIST", current_dir);
    Test("LIST .", current_dir);
    Test("LIST ./", current_dir);
    Test("LIST .//", current_dir);
    Test("LIST dir", dir);
    Test("LIST dir/", dir);
    Test("LIST dir//", dir);
    Test("LIST ./dir", dir);
    Test("LIST ./dir/", dir);
    Test("LIST ./dir//", dir);
    Test("LIST .hidden_dir");
    Test("LIST dir/another_dir");
    Test("LIST dir/.another_hidden_dir");
}

BOOST_AUTO_TEST_CASE(MLSD)
{
    array<set<string>, 2> current_dir{
            set<string>{
                "modify=20250403040000;size=4096;type=cdir;UNIX.group=511;UNIX.groupname=;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=; .\\r",
                "modify=20250403040000;size=4096;type=pdir;UNIX.group=511;UNIX.groupname=;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=; ..\\r",
                "modify=20250403040000;size=4096;type=dir;UNIX.group=511;UNIX.groupname=;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=; .hidden_dir\\r",
                "modify=20250403040000;size=0;type=special;UNIX.group=511;UNIX.groupname=;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=; fifo\\r",
                "modify=20250403040000;size=4096;type=dir;UNIX.group=511;UNIX.groupname=;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=; dir\\r",
                "modify=20250403040000;size=4;type=file;UNIX.group=511;UNIX.groupname=;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=; file\\r",
                "modify=20250403040000;size=11;type=file;UNIX.group=511;UNIX.groupname=;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=; .hidden_file\\r",
                "modify=20250403040000;size=4;type=OS.unix=symlink;UNIX.group=511;UNIX.groupname=;UNIX.mode=0777;UNIX.owner=3755;UNIX.ownername=; link\\r",
                "modify=20250403040000;size=12;type=OS.unix=symlink;UNIX.group=511;UNIX.groupname=;UNIX.mode=0777;UNIX.owner=3755;UNIX.ownername=; .hidden_link\\r",
                "modify=20250403040000;size=0;type=special;UNIX.group=511;UNIX.groupname=;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=; .hidden_fifo\\r",
            },
            set<string>{
                "modify=20250403040000;perm=fle;type=cdir;unique=2AU5A454149;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=coremake; .\\r",
                "modify=20250403040000;perm=fle;type=pdir;unique=2AU5A54B6D0;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=02555;UNIX.owner=3755;UNIX.ownername=coremake; ..\\r",
                "modify=20250403040000;perm=fle;type=dir;unique=2AU5A454162;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=coremake; .hidden_dir\\r",
                "modify=20250403040000;perm=adfr;size=0;type=file;unique=2AU5A454163;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=coremake; fifo\\r",
                "modify=20250403040000;perm=fle;type=dir;unique=2AU5A454165;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=coremake; dir\\r",
                "modify=20250403040000;perm=adfr;size=4;type=file;unique=2AU5A454166;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=coremake; file\\r",
                "modify=20250403040000;perm=adfr;size=11;type=file;unique=2AU5A45418E;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=coremake; .hidden_file\\r",
                "modify=20250403040000;perm=adfr;size=4;type=OS.unix=symlink;unique=2AU5A454166;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0777;UNIX.owner=3755;UNIX.ownername=coremake; link\\r",
                "modify=20250403040000;perm=adfr;size=12;type=OS.unix=symlink;unique=2AU5A45418E;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0777;UNIX.owner=3755;UNIX.ownername=coremake; .hidden_link\\r",
                "modify=20250403040000;perm=adfr;size=0;type=file;unique=2AU5A5476E6;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=coremake; .hidden_fifo\\r",
            },
        },
        dir{
            set<string>{
                "modify=20250403040000;size=4096;type=cdir;UNIX.group=511;UNIX.groupname=;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=; .\\r",
                "modify=20250403040000;size=4096;type=pdir;UNIX.group=511;UNIX.groupname=;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=; ..\\r",
                "modify=20250403040000;size=12;type=file;UNIX.group=5333;UNIX.groupname=;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=; another_file\\r",
                "modify=20250403040000;size=19;type=file;UNIX.group=5333;UNIX.groupname=;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=; .another_hidden_file\\r",
                "modify=20250403040000;size=4096;type=dir;UNIX.group=511;UNIX.groupname=;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=; another_dir\\r",
                "modify=20250403040000;size=4096;type=dir;UNIX.group=511;UNIX.groupname=;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=; .another_hidden_dir\\r",
                "modify=20250403040000;size=20;type=OS.unix=symlink;UNIX.group=5333;UNIX.groupname=;UNIX.mode=0777;UNIX.owner=3755;UNIX.ownername=; .another_hidden_link\\r",
                "modify=20250403040000;size=12;type=OS.unix=symlink;UNIX.group=5333;UNIX.groupname=;UNIX.mode=0777;UNIX.owner=3755;UNIX.ownername=; another_link\\r",
                "modify=20250403040000;size=0;type=special;UNIX.group=511;UNIX.groupname=;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=; .another_hidden_fifo\\r",
                "modify=20250403040000;size=0;type=special;UNIX.group=511;UNIX.groupname=;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=; another_fifo\\r",
            },
            set<string>{
                "modify=20250403040000;perm=fle;type=cdir;unique=2AU5A454165;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=coremake; .\\r",
                "modify=20250403040000;perm=fle;type=pdir;unique=2AU5A454149;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=coremake; ..\\r",
                "modify=20250403040000;perm=adfr;size=12;type=file;unique=2AU5A457A5B;UNIX.group=5333;UNIX.groupname=ftpguest;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=coremake; another_file\\r",
                "modify=20250403040000;perm=adfr;size=19;type=file;unique=2AU5A457AB6;UNIX.group=5333;UNIX.groupname=ftpguest;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=coremake; .another_hidden_file\\r",
                "modify=20250403040000;perm=fle;type=dir;unique=2AU5A547E8B;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=coremake; another_dir\\r",
                "modify=20250403040000;perm=fle;type=dir;unique=2AU5A10A7FD;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=coremake; .another_hidden_dir\\r",
                "modify=20250403040000;perm=adfr;size=20;type=OS.unix=symlink;unique=2AU5A457AB6;UNIX.group=5333;UNIX.groupname=ftpguest;UNIX.mode=0777;UNIX.owner=3755;UNIX.ownername=coremake; .another_hidden_link\\r",
                "modify=20250403040000;perm=adfr;size=12;type=OS.unix=symlink;unique=2AU5A457A5B;UNIX.group=5333;UNIX.groupname=ftpguest;UNIX.mode=0777;UNIX.owner=3755;UNIX.ownername=coremake; another_link\\r",
                "modify=20250403040000;perm=adfr;size=0;type=file;unique=2AU5A0C62C9;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=coremake; .another_hidden_fifo\\r",
                "modify=20250403040000;perm=adfr;size=0;type=file;unique=2AU5A1097AA;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=coremake; another_fifo\\r",
            },
        },
        hidden_dir{
            set<string>{
                "modify=20250403040000;size=4096;type=cdir;UNIX.group=511;UNIX.groupname=;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=; .\\r",
                "modify=20250403040000;size=4096;type=pdir;UNIX.group=511;UNIX.groupname=;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=; ..\\r",
            },
            set<string>{
                "modify=20250403040000;perm=fle;type=cdir;unique=2AU5A454162;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=coremake; .\\r",
                "modify=20250403040000;perm=fle;type=pdir;unique=2AU5A454149;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=coremake; ..\\r",
            },
        },
        another_dir{
            set<string>{
                "modify=20250403040000;size=4096;type=cdir;UNIX.group=511;UNIX.groupname=;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=; .\\r",
                "modify=20250403040000;size=4096;type=pdir;UNIX.group=511;UNIX.groupname=;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=; ..\\r",
            },
            set<string>{
                "modify=20250403040000;perm=fle;type=cdir;unique=2AU5A547E8B;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=coremake; .\\r",
                "modify=20250403040000;perm=fle;type=pdir;unique=2AU5A454165;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=coremake; ..\\r",
            },
        },
        another_hidden_dir{
            set<string>{
                "modify=20250403040000;size=4096;type=cdir;UNIX.group=511;UNIX.groupname=;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=; .\\r",
                "modify=20250403040000;size=4096;type=pdir;UNIX.group=511;UNIX.groupname=;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=; ..\\r",
            },
            set<string>{
                "modify=20250403040000;perm=fle;type=cdir;unique=2AU5A10A7FD;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=coremake; .\\r",
                "modify=20250403040000;perm=fle;type=pdir;unique=2AU5A454165;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=coremake; ..\\r",
            },
        };

    Test("MLSD", current_dir);
    Test("MLSD .", current_dir);
    Test("MLSD ./", current_dir);
    Test("MLSD .//", current_dir);
    Test("MLSD dir", dir);
    Test("MLSD dir/", dir);
    Test("MLSD dir//", dir);
    Test("MLSD ./dir", dir);
    Test("MLSD ./dir/", dir);
    Test("MLSD ./dir//", dir);
    Test("MLSD .hidden_dir", hidden_dir);
    Test("MLSD .hidden_dir/", hidden_dir);
    Test("MLSD .hidden_dir//", hidden_dir);
    Test("MLSD dir/another_dir", another_dir);
    Test("MLSD dir/another_dir/", another_dir);
    Test("MLSD dir/another_dir//", another_dir);
    Test("MLSD dir/.another_hidden_dir", another_hidden_dir);
    Test("MLSD dir/.another_hidden_dir/", another_hidden_dir);
    Test("MLSD dir/.another_hidden_dir//", another_hidden_dir);
}

BOOST_AUTO_TEST_CASE(MLST)
{
    array<string_view, 2> current_dir{
            "modify=20250403040000;size=4096;type=dir;UNIX.group=511;UNIX.groupname=;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=; /test_ncbi_sftp/model/dir",
            "modify=20250403040000;perm=fle;type=dir;unique=2AU5A454149;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=coremake; /test_ncbi_sftp/model/dir",
        },
        dir{
            "modify=20250403040000;size=4096;type=dir;UNIX.group=511;UNIX.groupname=;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=; /test_ncbi_sftp/model/dir/dir",
            "modify=20250403040000;perm=fle;type=dir;unique=2AU5A454165;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=coremake; /test_ncbi_sftp/model/dir/dir",
        },
        hidden_dir{
            "modify=20250403040000;size=4096;type=dir;UNIX.group=511;UNIX.groupname=;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=; /test_ncbi_sftp/model/dir/.hidden_dir",
            "modify=20250403040000;perm=fle;type=dir;unique=2AU5A454162;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=coremake; /test_ncbi_sftp/model/dir/.hidden_dir",
        },
        another_dir{
            "modify=20250403040000;size=4096;type=dir;UNIX.group=511;UNIX.groupname=;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=; /test_ncbi_sftp/model/dir/dir/another_dir",
            "modify=20250403040000;perm=fle;type=dir;unique=2AU5A547E8B;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=coremake; /test_ncbi_sftp/model/dir/dir/another_dir",
        },
        another_hidden_dir{
            "modify=20250403040000;size=4096;type=dir;UNIX.group=511;UNIX.groupname=;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=; /test_ncbi_sftp/model/dir/dir/.another_hidden_dir",
            "modify=20250403040000;perm=fle;type=dir;unique=2AU5A10A7FD;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0555;UNIX.owner=3755;UNIX.ownername=coremake; /test_ncbi_sftp/model/dir/dir/.another_hidden_dir",
        };

    Test("MLST", current_dir);
    Test("MLST .", current_dir);
    Test("MLST ./", current_dir);
    Test("MLST dir/..", current_dir);
    Test("MLST dir/../", current_dir);
    Test("MLST dir", dir);
    Test("MLST dir/", dir);
    Test("MLST ./dir", dir);
    Test("MLST ./dir/", dir);

    Test("MLST fifo", to_array({
                "modify=20250403040000;size=0;type=special;UNIX.group=511;UNIX.groupname=;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=; /test_ncbi_sftp/model/dir/fifo",
                "modify=20250403040000;perm=adfr;size=0;type=file;unique=2AU5A454163;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=coremake; /test_ncbi_sftp/model/dir/fifo",
                }));

    Test("MLST file", to_array({
                "modify=20250403040000;size=4;type=file;UNIX.group=511;UNIX.groupname=;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=; /test_ncbi_sftp/model/dir/file",
                "modify=20250403040000;perm=adfr;size=4;type=file;unique=2AU5A454166;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=coremake; /test_ncbi_sftp/model/dir/file",
                }));

    Test("MLST link", to_array({
                "modify=20250403040000;size=4;type=OS.unix=symlink;UNIX.group=511;UNIX.groupname=;UNIX.mode=0777;UNIX.owner=3755;UNIX.ownername=; /test_ncbi_sftp/model/dir/link",
                "modify=20250403040000;perm=adfr;size=4;type=file;unique=2AU5A454166;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0777;UNIX.owner=3755;UNIX.ownername=coremake; /test_ncbi_sftp/model/dir/link",
                }));

    Test("MLST .hidden_dir", hidden_dir);
    Test("MLST .hidden_dir/", hidden_dir);

    Test("MLST .hidden_fifo", to_array({
                "modify=20250403040000;size=0;type=special;UNIX.group=511;UNIX.groupname=;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=; /test_ncbi_sftp/model/dir/.hidden_fifo",
                "modify=20250403040000;perm=adfr;size=0;type=file;unique=2AU5A5476E6;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=coremake; /test_ncbi_sftp/model/dir/.hidden_fifo",
                }));

    Test("MLST .hidden_file", to_array({
                "modify=20250403040000;size=11;type=file;UNIX.group=511;UNIX.groupname=;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=; /test_ncbi_sftp/model/dir/.hidden_file",
                "modify=20250403040000;perm=adfr;size=11;type=file;unique=2AU5A45418E;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=coremake; /test_ncbi_sftp/model/dir/.hidden_file",
                }));

    Test("MLST .hidden_link", to_array({
                "modify=20250403040000;size=12;type=OS.unix=symlink;UNIX.group=511;UNIX.groupname=;UNIX.mode=0777;UNIX.owner=3755;UNIX.ownername=; /test_ncbi_sftp/model/dir/.hidden_link",
                "modify=20250403040000;perm=adfr;size=12;type=file;unique=2AU5A45418E;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0777;UNIX.owner=3755;UNIX.ownername=coremake; /test_ncbi_sftp/model/dir/.hidden_link",
                }));

    Test("MLST dir/another_dir", another_dir);
    Test("MLST dir/another_dir/", another_dir);

    Test("MLST dir/another_fifo", to_array({
                "modify=20250403040000;size=0;type=special;UNIX.group=511;UNIX.groupname=;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=; /test_ncbi_sftp/model/dir/dir/another_fifo",
                "modify=20250403040000;perm=adfr;size=0;type=file;unique=2AU5A1097AA;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=coremake; /test_ncbi_sftp/model/dir/dir/another_fifo",
                }));

    Test("MLST dir/another_file", to_array({
                "modify=20250403040000;size=12;type=file;UNIX.group=5333;UNIX.groupname=;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=; /test_ncbi_sftp/model/dir/dir/another_file",
                "modify=20250403040000;perm=adfr;size=12;type=file;unique=2AU5A457A5B;UNIX.group=5333;UNIX.groupname=ftpguest;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=coremake; /test_ncbi_sftp/model/dir/dir/another_file",
                }));

    Test("MLST dir/another_link", to_array({
                "modify=20250403040000;size=12;type=OS.unix=symlink;UNIX.group=5333;UNIX.groupname=;UNIX.mode=0777;UNIX.owner=3755;UNIX.ownername=; /test_ncbi_sftp/model/dir/dir/another_link",
                "modify=20250403040000;perm=adfr;size=12;type=file;unique=2AU5A457A5B;UNIX.group=5333;UNIX.groupname=ftpguest;UNIX.mode=0777;UNIX.owner=3755;UNIX.ownername=coremake; /test_ncbi_sftp/model/dir/dir/another_link",
                }));

    Test("MLST dir/.another_hidden_dir", another_hidden_dir);
    Test("MLST dir/.another_hidden_dir/", another_hidden_dir);

    Test("MLST dir/.another_hidden_fifo", to_array({
                "modify=20250403040000;size=0;type=special;UNIX.group=511;UNIX.groupname=;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=; /test_ncbi_sftp/model/dir/dir/.another_hidden_fifo",
                "modify=20250403040000;perm=adfr;size=0;type=file;unique=2AU5A0C62C9;UNIX.group=511;UNIX.groupname=ftpguest;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=coremake; /test_ncbi_sftp/model/dir/dir/.another_hidden_fifo",
                }));

    Test("MLST dir/.another_hidden_file", to_array({
                "modify=20250403040000;size=19;type=file;UNIX.group=5333;UNIX.groupname=;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=; /test_ncbi_sftp/model/dir/dir/.another_hidden_file",
                "modify=20250403040000;perm=adfr;size=19;type=file;unique=2AU5A457AB6;UNIX.group=5333;UNIX.groupname=ftpguest;UNIX.mode=0444;UNIX.owner=3755;UNIX.ownername=coremake; /test_ncbi_sftp/model/dir/dir/.another_hidden_file",
                }));

    Test("MLST dir/.another_hidden_link", to_array({
                "modify=20250403040000;size=20;type=OS.unix=symlink;UNIX.group=5333;UNIX.groupname=;UNIX.mode=0777;UNIX.owner=3755;UNIX.ownername=; /test_ncbi_sftp/model/dir/dir/.another_hidden_link",
                "modify=20250403040000;perm=adfr;size=20;type=file;unique=2AU5A457AB6;UNIX.group=5333;UNIX.groupname=ftpguest;UNIX.mode=0777;UNIX.owner=3755;UNIX.ownername=coremake; /test_ncbi_sftp/model/dir/dir/.another_hidden_link",
                }));
}

BOOST_AUTO_TEST_SUITE_END()
