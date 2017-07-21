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
/**
@file implementation of a metafunction to generate a vector, which is then to be used to construct a \ref
gridtools::meta_array
*/
namespace gridtools {

    template < typename Mss1, typename Mss2, typename Cond >
    struct condition;

    template < typename Vector, typename... Mss >
    struct meta_array_generator;

    /**@brief recursion anchor*/
    template < typename Vector >
    struct meta_array_generator< Vector > {
        typedef Vector type;
    };

    /**
       @brief metafunction to construct a vector of multi-stage stencils and conditions
     */
    template < typename Vector, typename First, typename... Mss >
    struct meta_array_generator< Vector, First, Mss... > {
        typedef
            typename meta_array_generator< typename boost::mpl::push_back< Vector, First >::type, Mss... >::type type;
    };

    /**
       @brief metafunction to construct a vector of multi-stage stencils and conditions

       specialization for conditions.
     */
    template < typename Vector, typename Mss1, typename Mss2, typename Cond, typename... Mss >
    struct meta_array_generator< Vector, condition< Mss1, Mss2, Cond >, Mss... > {
        typedef condition< typename meta_array_generator< Vector, Mss1, Mss... >::type,
            typename meta_array_generator< Vector, Mss2, Mss... >::type,
            Cond > type;
    };

} // namespace gridtools