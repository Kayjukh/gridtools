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

#include <utility>

#include "../../common/functional.hpp"
#include "../backend_traits_fwd.hpp"
#include "../block_size.hpp"
#include "../empty_iterate_domain_cache.hpp"
#include "iterate_domain_mic.hpp"
#include "run_esf_functor_mic.hpp"
#include "strategy_mic.hpp"

#ifdef ENABLE_METERS
#include "timer_mic.hpp"
#else
#include "../timer_dummy.hpp"
#endif

/**@file
@brief type definitions and structures specific for the Mic backend
*/
namespace gridtools {
    /**Traits struct, containing the types which are specific for the mic backend*/
    template <>
    struct backend_traits_from_id< enumtype::Mic > {

        /** This is the function used to extract a pointer out of a given storage info.
            In the case of Host backend we have to return the CPU pointer.
        */
        using extract_storage_info_ptr_f = identity;

        /** This is the functor used to generate view instances. According to the given storage (data_store,
           data_store_field) an appropriate view is returned. When using the Host backend we return host view instances.
        */
        struct make_view_f {
            template < typename S, typename SI >
            auto operator()(data_store< S, SI > const &src) const GT_AUTO_RETURN(make_host_view(src));
            template < typename S, uint_t... N >
            auto operator()(data_store_field< S, N... > const &src) const GT_AUTO_RETURN(make_field_host_view(src));
        };

        /** This is the function used by the specific backend to inform the
            generic backend and the temporary storage allocator how to
            compute the number of threads in the i-direction, in a 2D
            grid of threads.
        */
        static uint_t n_i_pes(uint_t = 0) {
#ifdef STRUCTURED_GRIDS
            /* This function is not used in the MIC backend for structured grids. */
            assert(false);
#endif
            return omp_get_max_threads();
        }

        /** This is the function used by the specific backend to inform the
            generic backend and the temporary storage allocator how to
            compute the number of threads in the j-direction, in a 2D
            grid of threads.
        */
        static uint_t n_j_pes(uint_t = 0) {
#ifdef STRUCTURED_GRIDS
            /* This function is not used in the MIC backend for structured grids. */
            assert(false);
#endif
            return 1;
        }

        /** This is the function used by the specific backend
         *  that determines the i coordinate of a processing element.
         *  In the case of the host, a processing element is equivalent to an OpenMP core
         */
        static uint_t processing_element_i() {
#ifdef STRUCTURED_GRIDS
            /* This function is not used in the MIC backend for structured grids. */
            assert(false);
#endif
            return omp_get_thread_num();
        }

        /** This is the function used by the specific backend
         *  that determines the j coordinate of a processing element.
         *  In the case of the host, a processing element is equivalent to an OpenMP core
         */
        static uint_t processing_element_j() {
#ifdef STRUCTURED_GRIDS
            /* This function is not used in the MIC backend for structured grids. */
            assert(false);
#endif
            return 0;
        }

        template < uint_t Id, typename BlockSize >
        struct once_per_block {
            GRIDTOOLS_STATIC_ASSERT((is_block_size< BlockSize >::value), "Error: wrong type");

            template < typename Left, typename Right >
            GT_FUNCTION // inline
                static void
                assign(Left &l, Right const &r) {
                l = (Left)r;
            }
        };

        /**
           Static method in order to calculate the field offset. In the iterate domain we store one pointer per
           storage. In addition to this each OpenMP thread stores an integer that indicates the offset of this
           pointer. For temporaries we use an oversized storage in order to have private halo
           regions for each thread. This method calculates the offset for temporaries and takes the private halo and
           alignment information into account.
        */
        template < typename LocalDomain, typename PEBlockSize, typename Arg, typename GridTraits, typename StorageInfo >
        static typename boost::enable_if_c< Arg::is_temporary, int >::type fields_offset(StorageInfo const *sinfo) {
#ifdef STRUCTURED_GRIDS
            const int thread = omp_get_thread_num();
            const int total_threads = omp_get_max_threads();
            const int thread_offset = (sinfo->padded_total_length()) * thread / total_threads;
            return thread_offset;
#else
            using grid_traits_t = GridTraits;
            // get the thread ID
            const uint_t i = processing_element_i();
            // halo in I direction
            constexpr int halo_i = StorageInfo::halo_t::template at< grid_traits_t::dim_i_t::value >();
            // compute the blocksize
            constexpr int blocksize = 2 * halo_i + PEBlockSize::i_size_t::value;
            // return the field offset
            const int stride_i = sinfo->template stride< grid_traits_t::dim_i_t::value >();
            return stride_i * (i * blocksize + halo_i);
#endif
        }

        /**
           Static method in order to calculate the field offset. In the iterate domain we store one pointer per
           storage in the shared memory. In addition to this each OpenMP thread stores an integer that indicates
           the offset of this pointer. This function computes the field offset for non temporary storages.
        */
        template < typename LocalDomain, typename PEBlockSize, typename Arg, typename GridTraits, typename StorageInfo >
        static typename boost::enable_if_c< !Arg::is_temporary, int >::type fields_offset(StorageInfo const *sinfo) {
            return 0;
        }

        /**
         * @brief main execution of a mss. Defines the IJ loop bounds of this particular block
         * and sequentially executes all the functors in the mss
         * @tparam RunFunctorArgs run functor arguments
         */
        template < typename RunFunctorArgs >
        struct mss_loop {
            typedef typename RunFunctorArgs::backend_ids_t backend_ids_t;

            GRIDTOOLS_STATIC_ASSERT((is_run_functor_arguments< RunFunctorArgs >::value), GT_INTERNAL_ERROR);
            template < typename LocalDomain, typename Grid, typename ReductionData, typename ExecutionInfo >
            static void run(LocalDomain &local_domain,
                const Grid &grid,
                ReductionData &reduction_data,
                const ExecutionInfo &execution_info) {
                GRIDTOOLS_STATIC_ASSERT((is_local_domain< LocalDomain >::value), GT_INTERNAL_ERROR);
                GRIDTOOLS_STATIC_ASSERT((is_grid< Grid >::value), GT_INTERNAL_ERROR);
                GRIDTOOLS_STATIC_ASSERT((is_reduction_data< ReductionData >::value), GT_INTERNAL_ERROR);

#ifdef STRUCTURED_GRIDS
                using grid_traits_t = grid_traits_from_id< backend_ids_t::s_grid_type_id >;
                using arch_grid_traits_t =
                    typename grid_traits_t::template with_arch< backend_ids_t::s_backend_id >::type;
                using kernel_functor_executor_t =
                    typename arch_grid_traits_t::template kernel_functor_executor< RunFunctorArgs >::type;

                kernel_functor_executor_t(local_domain, grid, reduction_data)(execution_info);
#else
                // each strategy executes a different high level loop for a mss
                strategy_from_id_mic< backend_ids_t::s_strategy_id >::template mss_loop< RunFunctorArgs >::template run(
                    local_domain, grid, reduction_data, execution_info);
#endif
            }
        };

/**
 * @brief determines whether ESFs should be fused in one single kernel execution or not for this backend.
 */
#ifdef STRUCTURED_GRIDS
        using mss_fuse_esfs_strategy = std::true_type;
#else
        using mss_fuse_esfs_strategy = std::false_type;
#endif

        // high level metafunction that contains the run_esf_functor corresponding to this backend
        typedef boost::mpl::quote2< run_esf_functor_mic > run_esf_functor_h_t;

        // metafunction that contains the strategy from id metafunction corresponding to this backend
        template < typename BackendIds >
        struct select_strategy {
            GRIDTOOLS_STATIC_ASSERT((is_backend_ids< BackendIds >::value), GT_INTERNAL_ERROR);
            typedef strategy_from_id_mic< BackendIds::s_strategy_id > type;
        };

        template < enumtype::strategy StrategyId >
        struct get_block_size {
            typedef typename strategy_from_id_mic< StrategyId >::block_size_t type;
        };

        /**
         * @brief metafunction that returns the right iterate domain
         * (depending on whether the local domain is positional or not)
         * @param IterateDomainArguments the iterate domain arguments
         * @return the iterate domain type for this backend
         */
        template < typename IterateDomainArguments >
        struct select_iterate_domain {
            GRIDTOOLS_STATIC_ASSERT((is_iterate_domain_arguments< IterateDomainArguments >::value), GT_INTERNAL_ERROR);
// indirection in order to avoid instantiation of both types of the eval_if
#ifdef STRUCTURED_GRIDS
            template < typename _IterateDomainArguments >
            struct select_positional_iterate_domain {
                typedef iterate_domain_mic< _IterateDomainArguments > type;
            };

            template < typename _IterateDomainArguments >
            struct select_basic_iterate_domain {
                typedef iterate_domain_mic< _IterateDomainArguments > type;
            };
#else
            template < typename _IterateDomainArguments >
            struct select_basic_iterate_domain {
                typedef iterate_domain_mic< iterate_domain, _IterateDomainArguments > type;
            };
#endif

            typedef typename boost::mpl::eval_if<
                local_domain_is_stateful< typename IterateDomainArguments::local_domain_t >,
#ifdef STRUCTURED_GRIDS
                select_positional_iterate_domain< IterateDomainArguments >,
#else
                select_basic_iterate_domain< IterateDomainArguments >,
#endif
                select_basic_iterate_domain< IterateDomainArguments > >::type type;
        };

#ifndef STRUCTURED_GRIDS
        template < typename IterateDomainArguments >
        struct select_iterate_domain_cache {
            typedef empty_iterate_domain_cache type;
        };
#endif

#ifdef ENABLE_METERS
        typedef timer_mic performance_meter_t;
#else
        typedef timer_dummy performance_meter_t;
#endif
    };

} // namespace gridtools