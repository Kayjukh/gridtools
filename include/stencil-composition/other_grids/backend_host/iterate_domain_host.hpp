#pragma once

#include "stencil-composition/iterate_domain_fwd.hpp"
#include "stencil-composition/iterate_domain.hpp"
#include "stencil-composition/iterate_domain_metafunctions.hpp"
#include "stencil-composition/iterate_domain_impl_metafunctions.hpp"

namespace gridtools {

    /**
 * @brief iterate domain class for the Host backend
 */
    template<typename DataPointerArray, typename StridesCached>
    class iterate_domain_host
    {
        DISALLOW_COPY_AND_ASSIGN(iterate_domain_host);
        GRIDTOOLS_STATIC_ASSERT((is_strides_cached<StridesCached>::value), "Internal error: wrong type");

    public:
        typedef boost::mpl::map0<> ij_caches_map_t;

        GT_FUNCTION
        explicit iterate_domain_host()
            : m_data_pointer(0), m_strides(0)
        {}

        void set_data_pointer_impl(DataPointerArray* RESTRICT data_pointer)
        {
            assert(data_pointer);
            m_data_pointer = data_pointer;
        }

        DataPointerArray& RESTRICT data_pointer_impl()
        {
            assert(m_data_pointer);
            return *m_data_pointer;
        }
        DataPointerArray const & RESTRICT data_pointer_impl() const
        {
            assert(m_data_pointer);
            return *m_data_pointer;
        }

        StridesCached& RESTRICT strides_impl()
        {
            assert(m_strides);
            return *m_strides;
        }

        StridesCached const & RESTRICT strides_impl() const
        {
            assert(m_strides);
            return *m_strides;
        }

        void set_strides_pointer_impl(StridesCached* RESTRICT strides)
        {
            assert(strides);
            m_strides = strides;
        }

        template <ushort_t Coordinate, typename Execution>
        GT_FUNCTION
        void increment_impl() {}

        template <ushort_t Coordinate>
        GT_FUNCTION
        void increment_impl(int_t steps) {}

        template <ushort_t Coordinate>
        GT_FUNCTION
        void initialize_impl() {}

    private:
        DataPointerArray* RESTRICT m_data_pointer;
        StridesCached* RESTRICT m_strides;
    };

//    template<
//            template<class> class IterateDomainBase, typename IterateDomainArguments>
//    struct is_iterate_domain<
//            iterate_domain_host<IterateDomainBase, IterateDomainArguments>
//            > : public boost::mpl::true_{};

//    template<
//            template<class> class IterateDomainBase,
//            typename IterateDomainArguments
//            >
//    struct is_positional_iterate_domain<iterate_domain_host<IterateDomainBase, IterateDomainArguments> > :
//            is_positional_iterate_domain<IterateDomainBase<iterate_domain_host<IterateDomainBase, IterateDomainArguments> > > {};

//    template<template<class> class IterateDomainBase, typename IterateDomainArguments>
//    struct iterate_domain_backend_id<iterate_domain_host<IterateDomainBase, IterateDomainArguments> >
//    {
//        typedef enumtype::enum_type< enumtype::backend, enumtype::Host > type;
//    };

}  //namespace gridtools