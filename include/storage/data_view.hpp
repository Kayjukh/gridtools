/*
  GridTools Libraries

  Copyright (c) 2017, ETH Zurich and MeteoSwiss
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:

  1. Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  3. Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  For information: http://eth-cscs.github.io/gridtools/
*/

#pragma once

#include <assert.h>

#include <boost/mpl/bool.hpp>
#include <boost/mpl/if.hpp>
#include <boost/mpl/and.hpp>
#include <boost/type_traits.hpp>

#include "common/definitions.hpp"
#include "common/storage_info_interface.hpp"

namespace gridtools {

    /**
     * @brief data_view implementation. This struct provides means to modify contents of
     * gridtools data_store containers on arbitrary locations (host, device, etc.).
     * @tparam DataStore data store type
     * @tparam AccessMode access mode (default is read-write)
     */
    template < typename DataStore, access_mode AccessMode = access_mode::ReadWrite >
    struct data_view {
        static_assert(is_data_store< DataStore >::value, "Passed type is no data_store type");
        typedef typename DataStore::data_t data_t;
        typedef typename DataStore::state_machine_t state_machine_t;
        typedef typename DataStore::storage_info_t storage_info_t;
        const static access_mode mode = AccessMode;
        const static unsigned num_of_storages = 1;

        data_t *m_raw_ptrs[1];
        state_machine_t *m_state_machine_ptr;
        storage_info_t const *m_storage_info;
        bool m_device_view;

        /**
         * @brief data_view constructor
         */
        GT_FUNCTION data_view() {}

        /**
         * @brief data_view constructor. This constructor is normally not called by the user because it is more
         * convenient to use the provided make functions.
         * @param data_ptr pointer to the data
         * @param info_ptr pointer to the storage_info
         * @param state_ptr pointer to the state machine
         * @param device_view true if device view, false otherwise
         */
        GT_FUNCTION data_view(
            data_t *data_ptr, storage_info_t const *info_ptr, state_machine_t *state_ptr, bool device_view)
            : m_raw_ptrs{data_ptr}, m_state_machine_ptr(state_ptr), m_storage_info(info_ptr),
              m_device_view(device_view) {
            assert(data_ptr && "Cannot create data_view with invalid data pointer");
            assert(info_ptr && "Cannot create data_view with invalid storage info pointer");
        }

        /**
         * @brief operator() is used to access elements. E.g., view(0,0,2) will return the third element.
         * @param c given indices
         * @return reference to the queried value
         */
        template < typename... Coords >
        typename boost::mpl::if_c< (AccessMode == access_mode::ReadOnly), data_t const &, data_t & >::type GT_FUNCTION
        operator()(Coords... c) const {
            static_assert(boost::mpl::and_< boost::mpl::bool_< (sizeof...(Coords) > 0) >,
                              typename is_all_integral< Coords... >::type >::value,
                "Index arguments have to be integral types.");
            return m_raw_ptrs[0][m_storage_info->index(c...)];
        }

        /**
         * @brief operator() is used to access elements. E.g., view({0,0,2}) will return the third element.
         * @param arr array of indices
         * @return reference to the queried value
         */
        template < typename T, unsigned N >
        typename boost::mpl::if_c< (AccessMode == access_mode::ReadOnly), data_t const &, data_t & >::type GT_FUNCTION
        operator()(std::array< T, N > const &arr) const {
            static_assert(boost::mpl::and_< boost::mpl::bool_< (N > 0) >, typename is_all_integral< T >::type >::value,
                "Index arguments have to be integral types.");
            return m_raw_ptrs[0][m_storage_info->index(arr)];
        }

        /**
         * @brief Check if view contains valid pointers, and simple state machine checks.
         * Be aware that this is not a full check. In order to check if a view is in a
         * consistent state use check_consistency function.
         * @return true if pointers and state is correct, otherwise false
         */
        bool valid() const {
            // ptrs invalid -> view invalid
            if (!m_raw_ptrs[0] || !m_storage_info)
                return false;
            // when used in combination with a host storage the view is always valid as long as the ptrs are
            if (!m_state_machine_ptr)
                return true;
            // read only -> simple check
            if (AccessMode == access_mode::ReadOnly)
                return m_device_view ? !m_state_machine_ptr->m_dnu : !m_state_machine_ptr->m_hnu;
            // check state machine ptrs
            return m_device_view ? ((m_state_machine_ptr->m_hnu) && !(m_state_machine_ptr->m_dnu))
                                 : (!(m_state_machine_ptr->m_hnu) && (m_state_machine_ptr->m_dnu));
        }

        /*
         * @brief function to retrieve the (aligned) size of a dimension (e.g., I, J, or K).
         * @tparam Coord queried coordinate
         * @return size of dimension
         */
        template < int Coord >
        GT_FUNCTION constexpr int dim() const {
            return m_storage_info->template dim< Coord >();
        }

        /*
         * @brief member function to retrieve the total size (dimensions, halos, padding, initial_offset).
         * @return total size
         */
        GT_FUNCTION constexpr int padded_total_length() const { return m_storage_info->padded_total_length(); }

        /*
         * @brief member function to retrieve the inner domain size + halo (dimensions, halos, no initial_offset).
         * @return inner domain size + halo
         */
        GT_FUNCTION constexpr int total_length() const { return m_storage_info->total_length(); }

        /*
         * @brief member function to retrieve the inner domain size (dimensions, no halos, no initial_offset).
         * @return inner domain size
         */
        GT_FUNCTION constexpr int length() const { return m_storage_info->length(); }
    };

    template < typename T >
    struct is_data_view : boost::mpl::false_ {};

    template < typename Storage, access_mode AccessMode >
    struct is_data_view< data_view< Storage, AccessMode > > : boost::mpl::true_ {};
}