/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011, 2012                                                   *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation, either version 3 of the License                  *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


#ifndef CPPA_WEAK_INTRUSIVE_PTR_HPP
#define CPPA_WEAK_INTRUSIVE_PTR_HPP

#include "cppa/ref_counted.hpp"
#include "cppa/intrusive_ptr.hpp"

namespace cppa {

template<typename T>
class weak_intrusive_ptr {

    typedef T::anchor anchor_type;

 public:

    weak_intrusive_ptr(const intrusive_ptr<T>& from) {
        if (from) m_anchor = from->get_weak_ptr_anchor();
    }

    weak_intrusive_ptr() = default;
    weak_intrusive_ptr(const weak_intrusive_ptr&) = default;
    weak_intrusive_ptr& operator=(const weak_intrusive_ptr&) = default;

    /**
     * @brief Promotes this weak pointer to an intrusive_ptr.
     * @warning Returns @p nullptr if expired.
     */
    intrusive_ptr<T> promote() {
        return (m_anchor) ? m_anchor->get() : nullptr;
    }

    /**
     * @brief Queries whether the object was already deleted.
     */
    bool expired() const {
        return (m_anchor) ? m_anchor->expired() : true;
    }

 private:

    intrusive_ptr<anchor_type> m_anchor;

};

} // namespace cppa

#endif // CPPA_WEAK_INTRUSIVE_PTR_HPP