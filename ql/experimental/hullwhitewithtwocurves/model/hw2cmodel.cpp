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

#include <ql/experimental/hullwhitewithtwocurves/model/hw2cmodel.hpp>

namespace QuantLib {
    HW2CModel::HW2CModel(Handle<QuantLib::YieldTermStructure> discountTermStructure,
                         Handle<QuantLib::YieldTermStructure> forwardTermStructure,
                         QuantLib::Real a,
                         QuantLib::Real sigma)
    : CalibratedModel(2), a_(a), sigma_(sigma) {
        QL_REQUIRE(discountTermStructure->referenceDate() == forwardTermStructure->referenceDate(),
                   "The reference date of discount and forward curve do not match.");
        QL_REQUIRE(discountTermStructure->dayCounter() == forwardTermStructure->dayCounter(),
                   "The day counter of discount and forward curve do not match.");
        discountModel_ = ext::make_shared<HullWhite>(discountTermStructure, a_, sigma_);
        forwardModel_ = ext::make_shared<HullWhite>(forwardTermStructure, a_, sigma_);
    }

    ext::shared_ptr<Lattice> HW2CModel::discountTree(const TimeGrid& timeGrid) const {
        return discountModel()->tree(timeGrid);
    }

    ext::shared_ptr<Lattice> HW2CModel::forwardTree(const TimeGrid& timeGrid) const {
        return forwardModel()->tree(timeGrid);
    }
}
