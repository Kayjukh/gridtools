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

#include <array>
#include <utility>

#include <boost/mpl/int.hpp>
#include <boost/mpl/and.hpp>
#include <boost/mpl/max_element.hpp>
#include <boost/type_traits.hpp>

#include "alignment.hpp"
#include "definitions.hpp"
#include "halo.hpp"
#include "storage_info_metafunctions.hpp"
#include "../../common/array.hpp"
#include "../../common/variadic_pack_metafunctions.hpp"
#include "../../common/layout_map.hpp"

namespace gridtools {

    namespace {

        /*
         * @brief Internal helper function to check if two given storage infos contain the same information.
         * The function performs checks on all dimensions. This function is the base case.
         * @return true if the dimension, stride, size, initial_offset, etc. is equal, otherwise false
         */
        template < unsigned N, typename StorageInfo >
        GT_FUNCTION typename boost::enable_if_c< (N == 0), bool >::type equality_check(StorageInfo a, StorageInfo b) {
            return (a.template dim< N >() == b.template dim< N >()) &&
                   (a.template stride< N >() == b.template stride< N >()) &&
                   (a.template unaligned_dim< N >() == b.template unaligned_dim< N >()) &&
                   (a.template unaligned_stride< N >() == b.template unaligned_stride< N >()) &&
                   (a.length() == b.length()) && (a.total_length() == b.total_length()) &&
                   (a.padded_total_length() == b.padded_total_length()) &&
                   (a.get_initial_offset() == b.get_initial_offset());
        }

        /*
         * @brief Internal helper function to check if two given storage infos contain the same information.
         * The function performs checks on all dimensions. This function is the step case.
         * @return true if the dimension, stride, size, initial_offset, etc. is equal, otherwise false
         */
        template < unsigned N, typename StorageInfo >
        GT_FUNCTION typename boost::enable_if_c< (N > 0), bool >::type equality_check(StorageInfo a, StorageInfo b) {
            return (a.template dim< N >() == b.template dim< N >()) &&
                   (a.template stride< N >() == b.template stride< N >()) &&
                   (a.template unaligned_dim< N >() == b.template unaligned_dim< N >()) &&
                   (a.template unaligned_stride< N >() == b.template unaligned_stride< N >()) &&
                   equality_check< N - 1 >(a, b);
        }
    }

    /*
     * @brief The storage info interface. This class contains methods that should be implemented by all storage_info
     * implementations.
     * @tparam Id unique ID that should be shared among all storage infos with the same dimensionality.
     * @tparam Layout information about the memory layout
     * @tparam Halo information about the halo sizes (by default no halo is set)
     * @tparam Alignment information about the alignment
     */
    template < unsigned Id,
        typename Layout,
        typename Halo = zero_halo< Layout::masked_length >,
        typename Alignment = alignment< 1 > >
    struct storage_info_interface;

    template < unsigned Id, int... LayoutArgs, unsigned... Halos, typename Align >
    struct storage_info_interface< Id, layout_map< LayoutArgs... >, halo< Halos... >, Align > {
        using layout_t = layout_map< LayoutArgs... >;
        using halo_t = halo< Halos... >;
        using alignment_t = Align;
        static_assert(sizeof...(Halos) == layout_t::masked_length, "Halo size does not match number of dimensions");
        static_assert(is_alignment< Align >::value, "Given type is not an alignment type");

      private:
        typedef storage_info_interface< Id, layout_map< LayoutArgs... >, halo< Halos... >, Align > this_t;
        array< unsigned, layout_t::masked_length > m_dims;
        array< unsigned, layout_t::masked_length > m_strides;
        alignment_impl< alignment_t, layout_t, halo_t > m_alignment;

        /*
         * @brief private storage info interface constructor
         */
        GT_FUNCTION constexpr storage_info_interface() {}

        /*
         * @brief Helper function to check for out of bounds accesses (step case)
         * @param idx offsets that should be checked
         */
        template < unsigned N, typename... Ints >
        GT_FUNCTION constexpr
            typename boost::enable_if_c< (N <= layout_t::masked_length - 1), bool >::type check_bounds(
                Ints... idx) const {
            // check for out of bounds access; each index is checked if it does not exceed the unaligned dimension.
            // masked dimensions are skipped and a recursive call is performed until all indices are checked.
            return ((layout_t::template at< N >() == -1) || (get_value_from_pack(N, idx...) < unaligned_dim< N >())) &&
                   check_bounds< N + 1 >(idx...);
        }

        /*
         * @brief Helper function to check for out of bounds accesses (base case)
         * @param idx offsets that should be checked
         */
        template < unsigned N, typename... Ints >
        GT_FUNCTION constexpr
            typename boost::enable_if_c< (N == layout_t::masked_length), bool >::type check_bounds(Ints... idx) const {
            // base case out of bounds check
            return true;
        }

        /*
         * @brief Helper function to calculate the index with given offsets (step case)
         * @param first offsets
         * @param ints offsets
         * @return index
         */
        template < unsigned N, typename... Ints >
        GT_FUNCTION constexpr typename boost::enable_if_c< (N < layout_t::masked_length), int >::type index_part(
            int first, Ints... ints) const {
            return first * m_strides.template get< N >() + index_part< N + 1 >(ints..., first);
        }

        /*
         * @brief Helper function to calculate the index with given offsets (base case)
         * @param first offsets
         * @param ints offsets
         * @return index
         */
        template < unsigned N, typename... Ints >
        GT_FUNCTION constexpr typename boost::enable_if_c< (N == layout_t::masked_length), int >::type index_part(
            int first, Ints... ints) const {
            return 0;
        }

        /*
         * @brief helper function to retrieve an offset (or index) when given an array of offsets in I,J,K, etc (base
         * case).
         * @tparam Args pack of integers
         * @param idx given offset array
         * @param indices pack of offsets
         * @return index
         */
        template < typename... Args >
        GT_FUNCTION constexpr typename boost::enable_if_c< (sizeof...(Args) == layout_t::masked_length), int >::type
        index_part(std::array< int, layout_t::masked_length > const &idx, Args... indices) const {
            return index(indices...);
        }

        /*
         * @brief helper function to retrieve an offset (or index) when given an array of offsets in I,J,K, etc (step
         * case).
         * @tparam Args pack of integers
         * @param idx given offset array
         * @param indices pack of offsets
         * @return index
         */
        template < typename... Args >
        GT_FUNCTION constexpr typename boost::enable_if_c< (sizeof...(Args) < layout_t::masked_length), int >::type
        index_part(std::array< int, layout_t::masked_length > const &idx, Args... indices) const {
            return index_part(idx, indices..., idx[sizeof...(Args)]);
        }

        /*
         * @brief Helper function to calculate the storage size (step case)
         * @return storage size
         */
        template < bool HaloIncluded = true, bool AlignedDim = true, unsigned From = layout_t::masked_length - 1 >
        GT_FUNCTION constexpr typename boost::enable_if_c< (From > 0), unsigned >::type size_part() const {
            typedef boost::mpl::int_< (halo_t::template at< From >() * 2) > halo_val_t;
            typedef boost::mpl::bool_< (layout_t::template at< From >() == -1) > masked_dim_t;
            return ((int)(AlignedDim ? dim< From >() : unaligned_dim< From >()) -
                       (int)((HaloIncluded || masked_dim_t::value) ? 0 : halo_val_t::value)) *
                   size_part< HaloIncluded, AlignedDim, From - 1 >();
        }

        /*
         * @brief Helper function to calculate the storage size (base case)
         * @return storage size
         */
        template < bool HaloIncluded = true, bool AlignedDim = true, unsigned From = layout_t::masked_length - 1 >
        GT_FUNCTION constexpr typename boost::enable_if_c< (From == 0), unsigned >::type size_part() const {
            typedef boost::mpl::int_< (halo_t::template at< 0 >() * 2) > halo_val_t;
            typedef boost::mpl::bool_< (layout_t::template at< 0 >() == -1) > masked_dim_t;
            return ((int)(AlignedDim ? dim< 0 >() : unaligned_dim< 0 >()) -
                    (int)((HaloIncluded || masked_dim_t::value) ? 0 : halo_val_t::value));
        }

        /*
         * @brief Helper function to calculate the last usable index of the storage (base case)
         * @return index of last data point (either halo or non halo)
         */
        template < bool HaloIncluded, typename... Args >
        GT_FUNCTION constexpr
            typename boost::enable_if_c< (sizeof...(Args) == layout_t::masked_length), int >::type end_part(
                Args... indices) const {
            return index(indices...);
        }

        /*
         * @brief Helper function to calculate the last usable index of the storage (step case)
         * @return index of last data point (either halo or non halo)
         */
        template < bool HaloIncluded, typename... Args >
        GT_FUNCTION constexpr
            typename boost::enable_if_c< (sizeof...(Args) < layout_t::masked_length), int >::type end_part(
                Args... indices) const {
            return HaloIncluded
                       ? end_part< HaloIncluded >(indices...,
                             (unaligned_dim< sizeof...(Args) >() - 1 - halo_t::template at< sizeof...(Args) >()))
                       : end_part< HaloIncluded >(indices..., (unaligned_dim< sizeof...(Args) >() - 1));
        }

      public:
        const static int id = Id;

        /*
         * @brief storage info constructor. Additionally to initializing the members the halo
         * region is added to the corresponding dimensions and the alignment is applied.
         */
        template < typename... Dims >
        GT_FUNCTION explicit constexpr storage_info_interface(Dims... dims_)
            : m_dims{align_dimensions< alignment_t, sizeof...(LayoutArgs), LayoutArgs >(
                  handle_masked_dims< LayoutArgs >::extend(dims_))...},
              m_strides(get_strides< layout_t >::get_stride_array(
                  align_dimensions< alignment_t, sizeof...(LayoutArgs), LayoutArgs >(
                      handle_masked_dims< LayoutArgs >::extend(dims_))...)),
              m_alignment(
                  array< unsigned, sizeof...(Dims) >{(unsigned)handle_masked_dims< LayoutArgs >::extend(dims_)...},
                  get_strides< layout_t >::get_stride_array(handle_masked_dims< LayoutArgs >::extend(dims_)...)) {
            static_assert(boost::mpl::and_< boost::mpl::bool_< (sizeof...(Dims) > 0) >,
                              typename is_all_integral< Dims... >::type >::value,
                "Dimensions have to be integral types.");
            static_assert((sizeof...(Dims) == layout_t::masked_length),
                "Number of passed dimensions do not match the layout map length.");
        }

        /*
         * @brief storage info copy constructor.
         */
        GT_FUNCTION constexpr storage_info_interface(storage_info_interface const &other) = default;

        /*
         * @brief member function to retrieve the total size (dimensions, halos, initial_offset, padding).
         * @return total size including dimensions, halos, initial_offset, padding, and initial_offset
         */
        GT_FUNCTION constexpr unsigned padded_total_length() const {
            return size_part< true, true >() + get_initial_offset();
        }

        /*
         * @brief member function to retrieve the number of domain elements
         * (dimensions, halos, no initial_offset, no padding).
         * @return number of domain elements
         */
        GT_FUNCTION constexpr unsigned total_length() const { return size_part< true, false >(); }

        /*
         * @brief member function to retrieve the number of inner domain elements
         * (dimensions, no halos, no initial_offset, no padding).
         * @return number of inner domain elements
         */
        GT_FUNCTION constexpr unsigned length() const { return size_part< false, false >(); }

        /*
         * @brief member function to retrieve the position of the first point.
         * This could also be a halo point.
         * @return position of first accessible point
         */
        GT_FUNCTION constexpr unsigned total_begin() const {
            return index({
                0,
            });
        }

        /*
         * @brief member function to retrieve the position of the first point.
         * This could also be a halo point.
         * @return position of last accessible point
         */
        GT_FUNCTION constexpr unsigned total_end() const { return end_part< false >(); }

        /*
         * @brief member function to retrieve the position of the first point.
         * This could also be a halo point.
         * @return position of first accessible point
         */
        GT_FUNCTION constexpr unsigned begin() const { return index(Halos...); }

        /*
         * @brief member function to retrieve the position of the first point.
         * This could also be a halo point.
         * @return position of last accessible point
         */
        GT_FUNCTION constexpr unsigned end() const {
            typedef typename boost::mpl::max_element< typename layout_t::static_layout_vector >::type iter;
            return end_part< true >();
        }

        /*
         * @brief member function to retrieve the (aligned) size of a dimension (e.g., I, J, or K)
         * If an alignment is set the "first" dimension is aligned to a given value (e.g., 32). For example
         * a storage info with layout_map<1,2,0> and dimensions 100x110x80 and an alignment of 32 will result
         * in a container with size 100x128x80 because the "innermost" dimension gets aligned.
         * @tparam Coord queried coordinate
         * @return size of dimension
         */
        template < int Coord >
        GT_FUNCTION constexpr int dim() const {
            static_assert((Coord < layout_t::masked_length), "Out of bounds access in storage info dimension call.");
            return m_dims.template get< Coord >();
        }

        /*
         * @brief member function to retrieve the (aligned) stride (e.g., I, J, or K)
         * @tparam Coord queried coordinate
         * @return aligned stride size
         */
        template < int Coord >
        GT_FUNCTION constexpr int stride() const {
            static_assert((Coord < layout_t::masked_length), "Out of bounds access in storage info stride call.");
            return m_strides.template get< Coord >();
        }

        /*
         * @brief member function to retrieve the (unaligned) size of a dimension (e.g., I, J, or K).
         * If an alignment is set the "first" dimension is aligned to a given value (e.g., 32). For example
         * a storage info with layout_map<1,2,0> and dimensions 100x110x80 and an alignment of 32 will result
         * in a container with size 100x128x80 because the "innermost" dimension gets aligned. Still though the
         * unaligned size will be 100x110x80.
         * @tparam Coord queried coordinate
         * @return size of dimension
         */
        template < int Coord >
        GT_FUNCTION constexpr int unaligned_dim() const {
            static_assert(
                (Coord < layout_t::masked_length), "Out of bounds access in storage info unaligned dimension call.");
            return m_alignment.template unaligned_dim< Coord >() ? m_alignment.template unaligned_dim< Coord >()
                                                                 : dim< Coord >();
        }

        /*
         * @brief member function to retrieve the (unaligned) stride (e.g., I, J, or K)
         * @tparam Coord queried coordinate
         * @return unaligned stride size
         */
        template < int Coord >
        GT_FUNCTION constexpr int unaligned_stride() const {
            static_assert(
                (Coord < layout_t::masked_length), "Out of bounds access in storage info unaligned stride call.");
            return m_alignment.template unaligned_stride< Coord >() ? m_alignment.template unaligned_stride< Coord >()
                                                                    : stride< Coord >();
        }

        /*
         * @brief member function to retrieve an offset (or index) when given offsets in I,J,K, etc.
         * E.g., index(1,2,3) --> 1*strideI + 2*strideJ + 3*strideK + initial_offset
         * @param idx given offsets
         * @return index
         */
        template < typename... Ints >
        GT_FUNCTION constexpr
            typename boost::enable_if< typename is_all_integral< Ints... >::type, int >::type index(Ints... idx) const {
            static_assert(boost::mpl::and_< boost::mpl::bool_< (sizeof...(Ints) > 0) >,
                              typename is_all_integral< Ints... >::type >::value,
                "Dimensions have to be integral types.");
            static_assert(
                sizeof...(Ints) == layout_t::masked_length, "Index function called with wrong number of arguments.");
#ifdef NDEBUG
            return index_part< 0 >(idx...) + get_initial_offset();
#else
            return error_or_return(check_bounds< 0 >(idx...),
                index_part< 0 >(idx...) + get_initial_offset(),
                "Storage out of bounds access");
#endif
        }

        /*
         * @brief member function to retrieve an offset (or index) when given an array of offsets in I,J,K, etc.
         * E.g., index(1,2,3) --> 1*strideI + 2*strideJ + 3*strideK + initial_offset
         * @param offsets given offset array
         * @return index
         */
        GT_FUNCTION constexpr int index(std::array< int, layout_t::masked_length > const &offsets) const {
            return index_part(offsets);
        }

        /*
         * @brief function that returns the initial offset. The initial offset
         * has to be added if we use alignment in combination with a halo
         * in the aligned dimension. We want to have the first non halo point
         * aligned. Therefore we have to introduce an initial offset.
         * @return initial offset
         */
        GT_FUNCTION static constexpr unsigned get_initial_offset() {
            return alignment_impl< alignment_t, layout_t, halo_t >::InitialOffset;
        }

        /*
         * @brief function to check for equality of two given storage_infos
         * @param rhs right hand side storage info instance
         * @return true if the storage infos are equal, false otherwise
         */
        GT_FUNCTION
        bool operator==(this_t const &rhs) const { return equality_check< layout_t::masked_length - 1 >(*this, rhs); }
    };

    template < typename T >
    struct is_storage_info
        : boost::is_base_of<
              storage_info_interface< T::id, typename T::layout_t, typename T::halo_t, typename T::alignment_t >,
              T > {};
}