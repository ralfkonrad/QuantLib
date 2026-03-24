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
    HW2CModel::HW2CModel(const Handle<YieldTermStructure>& discountTermStructure,
                         const Handle<YieldTermStructure>& forwardTermStructure,
                         Real a,
                         Real sigma)
    : CalibratedModel(2), a_(arguments_[0]), sigma_(arguments_[1]),
      discountTermStructure_(discountTermStructure), forwardTermStructure_(forwardTermStructure) {
        QL_REQUIRE(discountTermStructure_->referenceDate() ==
                       forwardTermStructure_->referenceDate(),
                   "The reference date of discount and forward curve do not match.");
        QL_REQUIRE(discountTermStructure_->dayCounter() == forwardTermStructure_->dayCounter(),
                   "The day counter of discount and forward curve do not match.");

        a_ = ConstantParameter(a, PositiveConstraint());
        sigma_ = ConstantParameter(sigma, PositiveConstraint());

        discountModel_ = RelinkableHandle<HullWhite>(
            ext::make_shared<HullWhite>(discountTermStructure, this->a(), this->sigma()));
        forwardModel_ = RelinkableHandle<HullWhite>(
            ext::make_shared<HullWhite>(forwardTermStructure, this->a(), this->sigma()));

        registerWith(discountTermStructure_);
        registerWith(forwardTermStructure_);
    }

    ext::shared_ptr<Lattice> HW2CModel::discountTree(const TimeGrid& timeGrid) const {
        return discountModel()->tree(timeGrid);
    }

    ext::shared_ptr<Lattice> HW2CModel::forwardTree(const TimeGrid& timeGrid) const {
        return forwardModel()->tree(timeGrid);
    }

    void HW2CModel::generateArguments() {
        discountModel_.linkTo(
            ext::make_shared<HullWhite>(discountTermStructure_, this->a(), this->sigma()));
        forwardModel_.linkTo(
            ext::make_shared<HullWhite>(forwardTermStructure_, this->a(), this->sigma()));
    }
}
