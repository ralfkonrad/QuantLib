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

#include <ql/experimental/hullwhitewithtwocurves/pricingengines/swap/hw2cdiscretizedswap.hpp>
#include <ql/experimental/hullwhitewithtwocurves/pricingengines/swaption/hw2cdiscretizedswaption.hpp>

namespace QuantLib {
    QuantLib::HW2CDiscretizedSwaption::HW2CDiscretizedSwaption(const Swaption::arguments& args,
                                                               const Date& referenceDate,
                                                               const DayCounter& dayCounter)
    : DiscretizedSwaption(args, referenceDate, dayCounter) {
        hw2cUnderlying_ = ext::make_shared<HW2CDiscretizedSwap>(args, referenceDate, dayCounter);
        underlying_ = hw2cUnderlying_;

        auto times = mandatoryTimes();
        lastPayment_ = *std::max_element(times.begin(), times.end());
    }

    void HW2CDiscretizedSwaption::reset(Size size) {
        hw2cUnderlying_->initialize(discountMethod(), forwardMethod(), lastPayment_);
        DiscretizedOption::reset(size); // NOLINT(bugprone-parent-virtual-call)
    }

    void HW2CDiscretizedSwaption::initialize(const ext::shared_ptr<Lattice>& discountMethod,
                                             const ext::shared_ptr<Lattice>& forwardMethod,
                                             Time t) {
        forwardMethod_ = forwardMethod;
        DiscretizedSwaption::initialize(discountMethod, t);
    }
}
