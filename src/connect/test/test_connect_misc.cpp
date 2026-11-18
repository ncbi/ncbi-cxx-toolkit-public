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

#include <connect/impl/connect_misc.hpp>
#include <connect/ncbi_socket.hpp>
#include <corelib/test_boost.hpp>

#include <iterator>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

BOOST_AUTO_TEST_SUITE(SocketAddress)

BOOST_AUTO_TEST_CASE(ConstructsSocketAddressFromBinaryIPv6)
{
    const TNCBI_IPv6Addr native = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}};
    const SSocketAddress address(native, 443);
    BOOST_CHECK(address == SSocketAddress(string("::1"), 443));
    BOOST_CHECK(static_cast<bool>(address.host));

    const TNCBI_IPv6Addr mapped = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 127, 0, 0, 1}};
    const SSocketAddress ipv4(mapped, 443);
    BOOST_CHECK(ipv4 == SSocketAddress(string("127.0.0.1"), 443));
    BOOST_CHECK(ipv4 == SSocketAddress(static_cast<unsigned>(ipv4.host), 443));

    const SSocketAddress empty(TNCBI_IPv6Addr{}, 443);
    BOOST_CHECK(!empty);
    BOOST_CHECK(empty.host.IsEmpty());
}

BOOST_AUTO_TEST_CASE(ConstructsIPv6SocketAddress)
{
    const SSocketAddress address(string("2001:db8::1"), 443);
    const unsigned char expected[16] = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};

    BOOST_CHECK(static_cast<bool>(address));
    BOOST_CHECK(static_cast<bool>(address.host));
    BOOST_CHECK_EQUAL(address.port, 443);
    BOOST_CHECK_EQUAL_COLLECTIONS(begin(address.host.GetAddr().octet), end(address.host.GetAddr().octet), begin(expected), end(expected));
    BOOST_CHECK_EQUAL(CSocketAPI::ntoa(address.host), "2001:db8::1");
}

BOOST_AUTO_TEST_CASE(DistinguishesIPv6SocketAddresses)
{
    const SSocketAddress first(string("2001:db8::1"), 443);
    const SSocketAddress second(string("2001:db8::2"), 443);
    const SSocketAddress same(string("2001:db8:0:0:0:0:0:1"), 443);

    BOOST_CHECK(!(first == second));
    BOOST_CHECK(first < second);
    BOOST_CHECK(!(second < first));
    BOOST_CHECK(first == same);
    BOOST_CHECK(!(first < same));
    BOOST_CHECK(!(same < first));
}

BOOST_AUTO_TEST_CASE(PreservesIPv4SocketAddress)
{
    const auto address = SSocketAddress::Parse("127.0.0.1:443", SSocketAddress::SHost::EName::eOriginal);
    BOOST_CHECK(NcbiIsIPv4(&address.host.GetAddr()));
    BOOST_CHECK_EQUAL(address.AsString(), "127.0.0.1:443");
    BOOST_CHECK(address == SSocketAddress(address.host, address.port));

    const SSocketAddress empty(0, 0);
    BOOST_CHECK(!empty);
    BOOST_CHECK(NcbiIsEmptyIPv6(&empty.host.GetAddr()));
}

BOOST_AUTO_TEST_CASE(ComparesPortsForTheSameHost)
{
    const auto host = CSocketAPI::gethostbyname("2001:db8::1");
    const SSocketAddress first(host, 80);
    const SSocketAddress second(host, 443);
    BOOST_CHECK(!(first == second));
    BOOST_CHECK(first < second);
    BOOST_CHECK(!(second < first));
    BOOST_CHECK(!SSocketAddress(host, 0));
    BOOST_CHECK(!SSocketAddress(CNCBI_IPAddr(), 443));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(ServiceDiscovery)

BOOST_AUTO_TEST_CASE(DiscoversIPv6Endpoint)
{
    CServiceDiscovery discovery("[2001:db8::1]:443");
    BOOST_REQUIRE(discovery.IsSingleServer());
    const auto servers = discovery();
    BOOST_REQUIRE_EQUAL(servers.size(), 1u);
    BOOST_CHECK_EQUAL(servers.front().first.GetHostName(), "2001:db8::1");
    BOOST_CHECK_EQUAL(servers.front().first.AsString(), "[2001:db8::1]:443");
    BOOST_CHECK(servers.front().first == SSocketAddress(string("2001:db8::1"), 443));
}

BOOST_AUTO_TEST_CASE(DiscoversIPv4Endpoint)
{
    CServiceDiscovery discovery("127.0.0.1:443");
    BOOST_REQUIRE(discovery.IsSingleServer());
    const auto servers = discovery();
    BOOST_REQUIRE_EQUAL(servers.size(), 1u);
    const auto& address = servers.front().first;
    BOOST_CHECK_EQUAL(address.AsString(), "127.0.0.1:443");
    BOOST_CHECK(address == SSocketAddress(string("127.0.0.1"), 443));
}

BOOST_AUTO_TEST_SUITE_END()
