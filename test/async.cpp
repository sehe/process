// Copyright (c) 2006, 2007 Julio M. Merino Vidal
// Copyright (c) 2008 Ilya Sokolov, Boris Schaeling
// Copyright (c) 2009 Boris Schaeling
// Copyright (c) 2010 Felipe Tanus, Boris Schaeling
// Copyright (c) 2011, 2012 Jeff Flinn, Boris Schaeling
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MAIN
#define BOOST_TEST_IGNORE_SIGCHLD
#include <boost/test/included/unit_test.hpp>

#include <boost/process/error.hpp>
#include <boost/process/exe_args.hpp>
#include <boost/process/async.hpp>
#include <boost/process/io.hpp>
#include <boost/process/execute.hpp>

#include <boost/thread.hpp>
#include <future>

#include <boost/system/error_code.hpp>

#include <boost/algorithm/string/predicate.hpp>

#include <iostream>

using namespace std;

namespace bp = boost::process;

BOOST_AUTO_TEST_CASE(async_wait)
{
    using boost::unit_test::framework::master_test_suite;
    using namespace boost::asio;

    boost::asio::io_service io_service;

    bool exit_called = false;
    int exit_code = 0;
    std::error_code ec;
    bp::child c = bp::execute(
        master_test_suite().argv[1],
        "test", "--exit-code", "123",
        ec,
        io_service,
        bp::on_exit([&](int exit, const std::error_code& ec_in){exit_code = exit; exit_called=true;BOOST_REQUIRE(!ec_in);})
    );
    BOOST_REQUIRE(!ec);

    io_service.run();
    BOOST_CHECK(exit_called);
    BOOST_CHECK_EQUAL(exit_code, 123);
}

BOOST_AUTO_TEST_CASE(async_out_stream)
{
    using boost::unit_test::framework::master_test_suite;

    boost::asio::io_service io_service;


    std::error_code ec;

    boost::asio::streambuf buf;

    bp::execute(
        master_test_suite().argv[1],
        "test", "--echo-stdout", "abc",
        bp::std_out > buf,
        io_service,
        ec
    );
    BOOST_REQUIRE(!ec);

    io_service.run();
    std::istream istr(&buf);

    std::string line;
    std::getline(istr, line);
    BOOST_CHECK(boost::algorithm::starts_with(line, "abc"));
}

BOOST_AUTO_TEST_CASE(async_out_callback)
{
    using boost::unit_test::framework::master_test_suite;

    boost::asio::io_service io_service;

    std::error_code ec;

    std::string result;

    bp::execute(
        master_test_suite().argv[1],
        "test", "--echo-stdout", "abc",
        bp::std_out>[&](const std::string & val){result = val;},
        io_service,
        ec
    );
    BOOST_REQUIRE(!ec);

    io_service.run();

    BOOST_CHECK(boost::algorithm::starts_with(result, "abc"));
}

BOOST_AUTO_TEST_CASE(async_out_callback_sep)
{
    using boost::unit_test::framework::master_test_suite;

    boost::asio::io_service io_service;

    std::error_code ec;

    std::vector<std::string> res;

    bp::execute(
        master_test_suite().argv[1],
        "test", "--echo-stdout", "abc\ndef",
        bp::std_out ([&](const std::string & val){res.push_back(val);}, '\n'),
        io_service,
        ec
    );
    BOOST_REQUIRE(!ec);

    io_service.run();

    BOOST_REQUIRE(res.size() > 0);
}


