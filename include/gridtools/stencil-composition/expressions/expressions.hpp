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

#include "../../common/defs.hpp"
#include "../../common/dimension.hpp"

/**@file
   @brief Expression templates definition.
   The expression templates are a method to parse at compile time the mathematical expression given
   by the user, recognizing the structure and building a syntax tree by recursively nesting
   templates.
*/

#include "./expr_divide.hpp"
#include "./expr_minus.hpp"
#include "./expr_plus.hpp"
#include "./expr_pow.hpp"
#include "./expr_times.hpp"

namespace gridtools {

    /** \ingroup stencil-composition
        @{
    */

    /** \defgroup expressions Expressions
        @{
    */

    /** Namespace containing all the compomnents to enable using expressions in stencil operators
     */
    namespace expressions {

        /** Expressions defining the interface for specifiyng a given offset for a specified dimension in plus-direction
           \tparam Coordinate: direction in which to apply the offset
           \param d1 Coordinate id
           \param offset: the offset to be applied in the Coordinate direction
        */
        template <ushort_t Coordinate>
        GT_FUNCTION constexpr dimension<Coordinate> operator+(dimension<Coordinate> d1, int const &offset) {
            return dimension<Coordinate>(offset);
        }

        /** Expressions defining the interface for specifiyng a given offset for a specified dimension in
           minus-direction
           \tparam Coordinate: direction in which to apply the offset
           \param d1 Coordinate id
           \param offset: the offset to be applied in the Coordinate direction
        */
        template <ushort_t Coordinate>
        GT_FUNCTION constexpr dimension<Coordinate> operator-(dimension<Coordinate> d1, int const &offset) {
            return dimension<Coordinate>(-offset);
        }
    } // namespace expressions

    /** @} */
    /** @} */

} // namespace gridtools