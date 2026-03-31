/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2022, 2026 Ralf Konrad Eckel

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

/*! \file hw2cmodel.cpp
    \brief Two-curve Hull-White model implementation
*/

#include <ql/experimental/hullwhitewithtwocurves/model/hw2cmodel.hpp>

using std::exp;
using std::sqrt;

namespace QuantLib {

    HW2CModel::HW2CModel(const Handle<YieldTermStructure>& discountTermStructure,
                         const Handle<YieldTermStructure>& forwardTermStructure,
                         Real a,
                         Real sigma)
    : OneFactorAffineModel(2), TermStructureConsistentModel(discountTermStructure),
      a_(arguments_[0]), sigma_(arguments_[1]), discountTermStructure_(discountTermStructure),
      forwardTermStructure_(forwardTermStructure) {
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

        HW2CModel::generateArguments();

        registerWith(discountTermStructure_);
        registerWith(forwardTermStructure_);
    }

    HW2CModel::HW2CModel(const Handle<YieldTermStructure>& termStructure, Real a, Real sigma)
    : HW2CModel(termStructure, termStructure, a, sigma) {}

    ext::shared_ptr<Lattice> HW2CModel::tree(const TimeGrid& grid) const {
        return discountModel()->tree(grid);
    }

    Real HW2CModel::A(Time t, Time T) const {
        DiscountFactor discount1 = discountTermStructure()->discount(t);
        DiscountFactor discount2 = discountTermStructure()->discount(T);
        Rate forward = discountTermStructure()->forwardRate(t, t, Continuous, NoFrequency);
        Real temp = sigma() * B(t, T);
        Real value = B(t, T) * forward - 0.25 * temp * temp * B(0.0, 2.0 * t);
        return exp(value) * discount2 / discount1;
    }

    Real HW2CModel::B(Time t, Time T) const {
        Real _a = a();
        if (_a < sqrt(QL_EPSILON))
            return (T - t);
        return (1.0 - exp(-_a * (T - t))) / _a;
    }

    Real HW2CModel::discountBondOption(Option::Type type,
                                       Real strike,
                                       Time maturity,
                                       Time bondMaturity) const {
        return discountModel()->discountBondOption(type, strike, maturity, bondMaturity);
    }

    Real HW2CModel::discountBondOption(
        Option::Type type, Real strike, Time maturity, Time bondStart, Time bondMaturity) const {
        return discountModel()->discountBondOption(type, strike, maturity, bondStart, bondMaturity);
    }

    Real HW2CModel::A_fwd(Time t, Time T) const {
        DiscountFactor discount1 = forwardTermStructure()->discount(t);
        DiscountFactor discount2 = forwardTermStructure()->discount(T);
        Rate forward = forwardTermStructure()->forwardRate(t, t, Continuous, NoFrequency);
        Real temp = sigma() * B(t, T);
        Real value = B(t, T) * forward - 0.25 * temp * temp * B(0.0, 2.0 * t);
        return exp(value) * discount2 / discount1;
    }

    Real HW2CModel::forwardDiscountBond(Time t, Time T, Real x) const {
        Rate r_fwd = x + phi_fwd_(t);
        return A_fwd(t, T) * exp(-B(t, T) * r_fwd);
    }

    void HW2CModel::generateArguments() {
        phi_ = HullWhite::FittingParameter(discountTermStructure(), a(), sigma());
        phi_fwd_ = HullWhite::FittingParameter(forwardTermStructure(), a(), sigma());

        discountModel_.linkTo(
            ext::make_shared<HullWhite>(discountTermStructure_, this->a(), this->sigma()));
        forwardModel_.linkTo(
            ext::make_shared<HullWhite>(forwardTermStructure_, this->a(), this->sigma()));
    }
}
