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
#include <utility>

namespace QuantLib {
    HW2CDiscretizedSwap::HW2CDiscretizedSwap(const VanillaSwap::arguments& args,
                                             const Date& referenceDate,
                                             const DayCounter& dayCounter)
    : HW2CDiscretizedSwap(
          args,
          referenceDate,
          dayCounter,
          std::vector<CouponAdjustment>(args.fixedPayDates.size(), CouponAdjustment::pre),
          std::vector<CouponAdjustment>(args.floatingPayDates.size(), CouponAdjustment::pre)) {}

    HW2CDiscretizedSwap::HW2CDiscretizedSwap(
        const VanillaSwap::arguments& args,
        const Date& referenceDate,
        const DayCounter& dayCounter,
        std::vector<CouponAdjustment> fixedCouponAdjustments,
        std::vector<CouponAdjustment> floatingCouponAdjustments)
    : DiscretizedSwap(args,
                      referenceDate,
                      dayCounter,
                      std::move(fixedCouponAdjustments),
                      std::move(floatingCouponAdjustments)) {}

    std::vector<Time> HW2CDiscretizedSwap::mandatoryTimes() const {
        return DiscretizedSwap::mandatoryTimes();
    }

    void HW2CDiscretizedSwap::initialize(const ext::shared_ptr<Lattice>& discountMethod,
                                         const ext::shared_ptr<Lattice>& forwardMethod,
                                         Time t) {
        DiscretizedAsset::initialize(discountMethod, t);
        forwardMethod_ = forwardMethod;
    }
}
