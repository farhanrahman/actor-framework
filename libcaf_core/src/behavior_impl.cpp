/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/detail/behavior_impl.hpp"

#include "caf/message_handler.hpp"

namespace caf {
namespace detail {

namespace {

class combinator final : public behavior_impl {
 public:
  bhvr_invoke_result invoke(message& arg) {
    auto res = first->invoke(arg);
    return res ? res : second->invoke(arg);
  }

  void handle_timeout() {
    // the second behavior overrides the timeout handling of
    // first behavior
    return second->handle_timeout();
  }

  pointer copy(const generic_timeout_definition& tdef) const {
    return new combinator(first, second->copy(tdef));
  }

  combinator(const pointer& p0, const pointer& p1)
      : behavior_impl(p1->timeout()),
        first(p0),
        second(p1) {
    // nop
  }

 protected:
  match_case** get_cases(size_t&) {
    // never called
    return nullptr;
  }

 private:
  pointer first;
  pointer second;
};

} // namespace <anonymous>

behavior_impl::~behavior_impl() {
  // nop
}

behavior_impl::behavior_impl(duration tout)
    : m_timeout(tout),
      m_num_cases(0),
      m_cases(nullptr) {
  // nop
}

bhvr_invoke_result behavior_impl::invoke(message& msg) {
  bhvr_invoke_result res;
  for (size_t i = 0; i < m_num_cases; ++i) {
    auto& mc = *m_cases[i];
    if (mc.has_wildcard() || mc.type_token() == msg.type_token()) {
      res = mc(msg);
      if (res) {
        return res;
      }
    }
  }
  return none;
}

behavior_impl::pointer behavior_impl::or_else(const pointer& other) {
  CAF_REQUIRE(other != nullptr);
  return new combinator(this, other);
}

behavior_impl* new_default_behavior(duration d, std::function<void()> fun) {
  dummy_match_expr nop;
  return new default_behavior_impl<dummy_match_expr>(nop, d, std::move(fun));
}

} // namespace detail
} // namespace caf
