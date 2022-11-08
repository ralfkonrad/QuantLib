/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2022 Ralf Konrad Eckel

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

#include <ql/experimental/hullwhitewithtwocurves/model/hw2c.hpp>

namespace QuantLib {
    HW2C::HW2C(Handle<QuantLib::YieldTermStructure> discountTermStructure,
               Handle<QuantLib::YieldTermStructure> forwardTermStructure,
               QuantLib::Real a,
               QuantLib::Real sigma)
    : CalibratedModel(2), discountTermStructure_(std::move(discountTermStructure)),
      forwardTermStructure_(std::move(forwardTermStructure)), a_(a), sigma_(sigma) {
        discountModel_ = ext::make_shared<HullWhite>(discountTermStructure_, a_, sigma_);
        forwardModel_ = ext::make_shared<HullWhite>(forwardTermStructure_, a_, sigma_);
    }
}
