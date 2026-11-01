// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#pragma once
// Contract checks: GLW_EXPECTS (a precondition), GLW_ENSURES (a postcondition), GLW_ASSERT (anything else). A failed check is a
// bug of the caller (or of GLWidgets): it reports the expression and its place and aborts. They are on when NDEBUG is not
// defined; GLW_ENABLE_ASSERTS (0 / 1) decides otherwise. Off, the expression is not evaluated.
//
// They stand for the contract assertions of C++26 (pre / post / contract_assert): when the compilers have them, GLW_EXPECTS
// and GLW_ENSURES move to the declarations and GLW_ASSERT becomes contract_assert. Keep the conditions free of side effects so
// that the move changes nothing.
#include <GLWidgets/Export.hpp>

namespace glw::detail {

[[noreturn]] GLW_EXPORT void assertionFailed(const char* kind, const char* expression, const char* file, int line);

} // namespace glw::detail

#ifndef GLW_ENABLE_ASSERTS
#  ifdef NDEBUG
#    define GLW_ENABLE_ASSERTS 0
#  else
#    define GLW_ENABLE_ASSERTS 1
#  endif
#endif

#if GLW_ENABLE_ASSERTS
#  define GLW_CHECK_(kind, cond) ((cond) ? (void)0 : ::glw::detail::assertionFailed(kind, #cond, __FILE__, __LINE__))
#else
#  define GLW_CHECK_(kind, cond) ((void)sizeof((cond) ? 1 : 0))
#endif

#define GLW_EXPECTS(cond) GLW_CHECK_("precondition", cond)
#define GLW_ENSURES(cond) GLW_CHECK_("postcondition", cond)
#define GLW_ASSERT(cond)  GLW_CHECK_("assertion", cond)
