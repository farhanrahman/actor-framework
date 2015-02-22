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

#ifndef CAF_DETAIL_BEHAVIOR_IMPL_HPP
#define CAF_DETAIL_BEHAVIOR_IMPL_HPP

#include <tuple>
#include <type_traits>

#include "caf/none.hpp"
#include "caf/variant.hpp"
#include "caf/optional.hpp"
#include "caf/match_case.hpp"
#include "caf/intrusive_ptr.hpp"

#include "caf/atom.hpp"
#include "caf/either.hpp"
#include "caf/message.hpp"
#include "caf/duration.hpp"
#include "caf/ref_counted.hpp"
#include "caf/skip_message.hpp"
#include "caf/response_promise.hpp"
#include "caf/timeout_definition.hpp"
#include "caf/typed_response_promise.hpp"

#include "caf/detail/int_list.hpp"
#include "caf/detail/apply_args.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/detail/optional_message_visitor.hpp"

namespace caf {

class message_handler;
using bhvr_invoke_result = optional<message>;

} // namespace caf

namespace caf {
namespace detail {

template <class... Ts>
struct has_skip_message {
  static constexpr bool value =
    disjunction<std::is_same<Ts, skip_message_t>::value...>::value;
};

class behavior_impl : public ref_counted {
 public:
  using pointer = intrusive_ptr<behavior_impl>;

  ~behavior_impl();

  behavior_impl(duration tout = duration{});

  virtual bhvr_invoke_result invoke(message&);

  inline bhvr_invoke_result invoke(message&& arg) {
    message tmp(std::move(arg));
    return invoke(tmp);
  }

  virtual void handle_timeout();

  inline const duration& timeout() const {
    return m_timeout;
  }

  virtual pointer copy(const generic_timeout_definition& tdef) const = 0;

  pointer or_else(const pointer& other);

 protected:
  duration m_timeout;
  size_t m_num_cases;
  match_case** m_cases;
};

struct dummy_match_expr {
  /*inline optional<message> operator()(message&) const {
    return none;
  }
  */
  using array_type = std::array<match_case*, 0>;
  array_type m_cases;
  inline array_type& cases_arr() {
    return m_cases;
  }
};

template <class MatchExpr, class F = std::function<void()>>
class default_behavior_impl : public behavior_impl {
 public:
  template <class Expr>
  default_behavior_impl(Expr&& expr, const timeout_definition<F>& d)
      : behavior_impl(d.timeout)
      , m_expr(std::forward<Expr>(expr))
      , m_fun(d.handler) {
    auto& arr = m_expr.cases_arr();
    m_num_cases = arr.size();
    m_cases = arr.data();
  }

  template <class Expr>
  default_behavior_impl(Expr&& expr, duration tout, F f)
      : behavior_impl(tout),
        m_expr(std::forward<Expr>(expr)),
        m_fun(f) {
    auto& arr = m_expr.cases_arr();
    m_num_cases = arr.size();
    m_cases = arr.data();
  }

  typename behavior_impl::pointer
  copy(const generic_timeout_definition& tdef) const override {
    return new default_behavior_impl<MatchExpr>(m_expr, tdef);
  }

  void handle_timeout() override {
    m_fun();
  }

 private:
  MatchExpr m_expr;
  F m_fun;
};

template <class MatchExpr, typename F>
default_behavior_impl<MatchExpr, F>*
new_default_behavior(const MatchExpr& mexpr, duration d, F f) {
  return new default_behavior_impl<MatchExpr, F>(mexpr, d, f);
}

template <class F>
behavior_impl* new_default_behavior(duration d, F f) {
  dummy_match_expr nop;
  return new default_behavior_impl<dummy_match_expr, F>(nop, d, std::move(f));
}

behavior_impl* new_default_behavior(duration d, std::function<void()> fun);

using behavior_impl_ptr = intrusive_ptr<behavior_impl>;

// implemented in message_handler.cpp
// message_handler combine(behavior_impl_ptr, behavior_impl_ptr);
// behavior_impl_ptr extract(const message_handler&);

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_BEHAVIOR_IMPL_HPP
