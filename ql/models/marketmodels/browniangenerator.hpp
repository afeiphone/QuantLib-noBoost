/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2006 Mark Joshi

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/


#ifndef quantlib_brownian_generator_hpp
#define quantlib_brownian_generator_hpp

#include <ql/types.hpp>
#include <memory>
#include <algorithm>
#include <vector>

namespace QuantLib {

    class BrownianGenerator {
      public:

        BrownianGenerator() = default;
        virtual ~BrownianGenerator() = default;
        BrownianGenerator(const BrownianGenerator&) = default;
        BrownianGenerator(BrownianGenerator&&) = default;
        BrownianGenerator& operator=(const BrownianGenerator&) = default;
        BrownianGenerator& operator=(BrownianGenerator&&) = default;


        virtual Real nextStep(std::vector<Real>&) = 0;
        virtual Real nextPath() = 0;

        virtual Size numberOfFactors() const = 0;
        virtual Size numberOfSteps() const = 0;
    };

    class BrownianGeneratorFactory {
      public:
        virtual ~BrownianGeneratorFactory() {}

        virtual std::shared_ptr<BrownianGenerator> create(Size factors,
                                                            Size steps) const = 0;
    };

}

#endif
