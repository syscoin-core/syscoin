//
// Copyright (C) 2011-2019 Ben Key
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_DETAIL_EXECUTABLE_PATH_INTERNALS_HPP_
#define BOOST_DETAIL_EXECUTABLE_PATH_INTERNALS_HPP_

#pragma once

#include <locale>
#include <string>
#include <vector>

#include <boost/filesystem/path.hpp>

namespace boost {
namespace detail {
boost::filesystem::path executable_path_worker();
std::string executable_path_fallback(const char* argv0);
std::wstring executable_path_fallback(const wchar_t* argv0);
} // namespace detail
} // namespace boost

#endif // BOOST_DETAIL_EXECUTABLE_PATH_INTERNALS_HPP_
