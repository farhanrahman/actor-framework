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

#ifndef CAF_MATCH_EXPR_HPP
#define CAF_MATCH_EXPR_HPP

#include <vector>

#include "caf/none.hpp"
#include "caf/variant.hpp"

#include "caf/unit.hpp"
#include "caf/match_case.hpp"

#include "caf/detail/int_list.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/detail/purge_refs.hpp"
#include "caf/detail/apply_args.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/detail/left_or_right.hpp"

#include "caf/detail/try_match.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/detail/pseudo_tuple.hpp"
#include "caf/detail/behavior_impl.hpp"

namespace caf {
namespace detail {

template <class Expr, class Transformers, class Pattern>
struct get_case_ {
  using ctrait = typename detail::get_callable_trait<Expr>::type;

  using filtered_pattern =
    typename detail::tl_filter_not_type<
      Pattern,
      anything
    >::type;

  using padded_transformers =
    typename detail::tl_pad_right<
      Transformers,
      detail::tl_size<filtered_pattern>::value
    >::type;

  using base_signature =
    typename detail::tl_map<
      filtered_pattern,
      std::add_const,
      std::add_lvalue_reference
    >::type;

  using padded_expr_args =
    typename detail::tl_map_conditional<
      typename detail::tl_pad_left<
        typename ctrait::arg_types,
        detail::tl_size<filtered_pattern>::value
      >::type,
      std::is_lvalue_reference,
      false,
      std::add_const,
      std::add_lvalue_reference
    >::type;

  // override base signature with required argument types of Expr
  // and result types of transformation
  using partial_fun_signature =
    typename detail::tl_zip<
      typename detail::tl_map<
        padded_transformers,
        detail::map_to_result_type,
        detail::rm_optional,
        std::add_lvalue_reference
      >::type,
      typename detail::tl_zip<
        padded_expr_args,
        base_signature,
        detail::left_or_right
      >::type,
      detail::left_or_right
    >::type;

  // 'inherit' mutable references from partial_fun_signature
  // for arguments without transformation
  using projection_signature =
    typename detail::tl_zip<
      typename detail::tl_zip<
        padded_transformers,
        partial_fun_signature,
        detail::if_not_left
      >::type,
      base_signature,
      detail::deduce_ref_type
    >::type;

  using type =
    match_case_impl<
      Expr,
      Pattern,
      padded_transformers,
      projection_signature
    >;
};

template <bool Complete, class Expr, class Trans, class Pattern>
struct get_case {
  using type = typename get_case_<Expr, Trans, Pattern>::type;
};

template <class Expr, class Trans, class Pattern>
struct get_case<false, Expr, Trans, Pattern> {
  using lhs_pattern = typename detail::tl_pop_back<Pattern>::type;
  using rhs_pattern =
    typename detail::tl_map<
      typename detail::get_callable_trait<Expr>::arg_types,
      std::decay
    >::type;
  using type =
    typename get_case_<
      Expr,
      Trans,
      typename detail::tl_concat<
        lhs_pattern,
        rhs_pattern
      >::type
    >::type;
};

inline variant<none_t, unit_t> unroll_expr_result_unbox(bool value) {
  if (value) {
    return unit;
  }
  return none;
}

template <class T>
T& unroll_expr_result_unbox(optional<T>& opt) {
  return *opt;
}

template <class Result>
struct unroll_expr {
  Result operator()(message&) const {
    // end of recursion
    return none;
  }

  template <class F, class... Fs>
  Result operator()(message& msg, F& f, Fs&... fs) const {
    meta_elements<typename F::pattern> ms;
    typename F::intermediate_tuple targs;
    if ((F::has_wildcard || F::type_token == msg.type_token())
        && try_match(msg, ms.arr.data(), ms.arr.size(), targs.data)) {
      auto is = detail::get_indices(targs);
      auto res = detail::apply_args(f, is, msg);
      if (res) {
        return std::move(unroll_expr_result_unbox(res));
      }
    }
    return (*this)(msg, fs...);
  }
};

template <bool IsManipulator, typename T0, typename T1>
struct mexpr_fwd_ {
  using type = T1;
};

template <class T>
struct mexpr_fwd_<false, const T&, T> {
  using type = std::reference_wrapper<const T>;
};

template <class T>
struct mexpr_fwd_<true, T&, T> {
  using type = std::reference_wrapper<T>;
};

template <bool IsManipulator, typename T>
struct mexpr_fwd {
  using type =
    typename mexpr_fwd_<
      IsManipulator,
      T,
      typename detail::implicit_conversions<
        typename std::decay<T>::type
      >::type
    >::type;
};

inline void detach_if_needed(message& msg, std::true_type) {
  msg.force_detach();
}

inline void detach_if_needed(const message&, std::false_type) {
  // nop
}

template <class T>
struct is_manipulator_case {
  // static constexpr bool value = T::second_type::manipulates_args;
  using arg_types = typename T::arg_types;
  static constexpr bool value = tl_exists<arg_types, is_mutable_ref>::value;
};

template <class T>
struct get_case_result {
  // using type = typename T::second_type::result_type;
  using type = typename T::result_type;
};

} // namespace detail
} // namespace caf

namespace caf {

template <class List>
struct match_result_from_type_list;

template <class... Ts>
struct match_result_from_type_list<detail::type_list<Ts...>> {
  using type = variant<none_t, typename lift_void<Ts>::type...>;
};

template <size_t Pos, size_t Size>
struct match_expr_init_arr {
  template <class Array, class Tuple>
  static void init(Array& arr, Tuple& tup) {
    arr[Pos] = &std::get<Pos>(tup);
    match_expr_init_arr<Pos + 1, Size>::init(arr, tup);
  }
};

template <size_t Size>
struct match_expr_init_arr<Size, Size> {
  template <class Array, class Tuple>
  static void init(Array&, Tuple&) {
    // nop
  }
};

/**
 * A match expression encapsulating cases `Cs..., whereas
 * each case is a `match_case`.
 */
template <class... Cs>
class match_expr {
 public:
  using cases_list = detail::type_list<Cs...>;

  using result_type =
    typename match_result_from_type_list<
      typename detail::tl_distinct<
        typename detail::tl_map<
          cases_list,
          detail::get_case_result
        >::type
      >::type
    >::type;

  using array_type = std::array<match_case*, sizeof...(Cs)>;

  template <class T, class... Ts>
  match_expr(T v, Ts&&... vs) : m_cases(std::move(v), std::forward<Ts>(vs)...) {
    init();
  }

  match_expr(match_expr&& other) : m_cases(std::move(other.m_cases)) {
    init();
  }

  match_expr(const match_expr& other) : m_cases(other.m_cases) {
    init();
  }

  //result_type operator()(message& msg) {
  optional<message> operator()(message& msg) {
    /*
    auto indices = detail::get_indices(m_cases);
    detail::unroll_expr<result_type> f;
    return detail::apply_args_prefixed(f, indices, m_cases, msg);
    */
    for (auto fun : m_cases_arr) {
      CAF_REQUIRE(fun != nullptr);
      auto res = (*fun)(msg);
      if (res) {
        return res;
      }
    }
    return none;
  }

  array_type& cases_arr() {
    return m_cases_arr;
  }

  template <class... Ds>
  match_expr<Cs..., Ds...> or_else(const match_expr<Ds...>& other) const {
    return {tuple_cat(m_cases, other.cases())};
  }

  /** @cond PRIVATE */

  const std::tuple<Cs...>& cases() const {
    return m_cases;
  }

  intrusive_ptr<detail::behavior_impl> as_behavior_impl() const {
    // return new pfun_impl(*this);
    auto lvoid = [] {};
    using impl = detail::default_behavior_impl<match_expr, decltype(lvoid)>;
    return new impl(*this, duration{}, lvoid);
  }

  /** @endcond */

 private:
  void init() {
    match_expr_init_arr<0, sizeof...(Cs)>::init(m_cases_arr, m_cases);
  }

  // structure: std::tuple<match_case<...>,
  //                       match_case<...>,
  //                       ...>
  std::tuple<Cs...> m_cases;
  array_type m_cases_arr;
};

template <class T>
struct is_match_expr : std::false_type { };

template <class... Cs>
struct is_match_expr<match_expr<Cs...>> : std::true_type { };

template <class... Lhs, class... Rhs>
match_expr<Lhs..., Rhs...> operator,(const match_expr<Lhs...>& lhs,
                                     const match_expr<Rhs...>& rhs) {
  return lhs.or_else(rhs);
}

template <class... Cs>
match_expr<Cs...>& match_expr_collect(match_expr<Cs...>& arg) {
  return arg;
}

template <class... Cs>
match_expr<Cs...>&& match_expr_collect(match_expr<Cs...>&& arg) {
  return std::move(arg);
}

template <class... Cs>
const match_expr<Cs...>& match_expr_collect(const match_expr<Cs...>& arg) {
  return arg;
}

template <class T, class... Ts>
typename detail::tl_apply<
  typename detail::tl_concat<
    typename T::cases_list,
    typename Ts::cases_list...
  >::type,
  match_expr
>::type
match_expr_collect(const T& arg, const Ts&... args) {
  return {std::tuple_cat(arg.cases(), args.cases()...)};
}

namespace detail {

// implemented in message_handler.cpp
message_handler combine(behavior_impl_ptr, behavior_impl_ptr);
behavior_impl_ptr extract(const message_handler&);

template <class... Cs>
behavior_impl_ptr extract(const match_expr<Cs...>& arg) {
  return arg.as_behavior_impl();
}

template <class... As, class... Bs>
match_expr<As..., Bs...> combine(const match_expr<As...>& lhs,
                                 const match_expr<Bs...>& rhs) {
  return lhs.or_else(rhs);
}

// forwards match_expr as match_expr as long as combining two match_expr,
// otherwise turns everything into behavior_impl_ptr
template <class... As, class... Bs>
const match_expr<As...>& combine_fwd(const match_expr<As...>& lhs,
                                     const match_expr<Bs...>&) {
  return lhs;
}

template <class T, typename U>
behavior_impl_ptr combine_fwd(T& lhs, U&) {
  return extract(lhs);
}

template <class T>
behavior_impl_ptr match_expr_concat(const T& arg) {
  return arg.as_behavior_impl();
}

template <class F>
behavior_impl_ptr match_expr_concat(const message_handler& arg0,
                                    const timeout_definition<F>& arg) {
  return extract(arg0)->copy(arg);
}

template <class... Cs, typename F>
behavior_impl_ptr match_expr_concat(const match_expr<Cs...>& arg0,
                                    const timeout_definition<F>& arg) {
  return new default_behavior_impl<match_expr<Cs...>, F>{arg0, arg};
}

template <class T0, typename T1, class... Ts>
behavior_impl_ptr match_expr_concat(const T0& arg0, const T1& arg1,
                                    const Ts&... args) {
  return match_expr_concat(combine(combine_fwd(arg0, arg1),
                                   combine_fwd(arg1, arg0)),
                           args...);
}

// some more convenience functions

template <class T,
          class E = typename std::enable_if<
                      is_callable<T>::value && !is_match_expr<T>::value
                    >::type>
match_expr<typename get_case<false, T, type_list<>, type_list<>>::type>
lift_to_match_expr(T arg) {
  using result_type =
    typename get_case<
      false,
      T,
      detail::empty_type_list,
      detail::empty_type_list
    >::type;
  return result_type{std::move(arg)};
}

template <class T,
          class E = typename std::enable_if<
                      !is_callable<T>::value || is_match_expr<T>::value
                    >::type>
T lift_to_match_expr(T arg) {
  return arg;
}

} // namespace detail

} // namespace caf

#endif // CAF_MATCH_EXPR_HPP
