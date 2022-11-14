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
                                             const DayCounter& dayCounter,
                                             const DayCounter& forwardDayCounter)
    : HW2CDiscretizedSwap(
          args,
          referenceDate,
          dayCounter,
          forwardDayCounter,
          std::vector<CouponAdjustment>(args.fixedPayDates.size(), CouponAdjustment::pre),
          std::vector<CouponAdjustment>(args.floatingPayDates.size(), CouponAdjustment::pre)) {}

    HW2CDiscretizedSwap::HW2CDiscretizedSwap(
        const VanillaSwap::arguments& args,
        const Date& referenceDate,
        const DayCounter& dayCounter,
        const DayCounter& forwardDayCounter,
        std::vector<CouponAdjustment> fixedCouponAdjustments,
        std::vector<CouponAdjustment> floatingCouponAdjustments)
    : DiscretizedSwap(args,
                      referenceDate,
                      dayCounter,
                      std::move(fixedCouponAdjustments),
                      std::move(floatingCouponAdjustments)) {
        auto nrOfFloatingCoupons = args.floatingResetDates.size();
        forwardStartTimes_.resize(nrOfFloatingCoupons);
        forwardEndTimes_.resize(nrOfFloatingCoupons);
        for (Size i = 0; i < nrOfFloatingCoupons; ++i) {
            auto forwardStartTime =
                forwardDayCounter.yearFraction(referenceDate, args.floatingResetDates[i]);
            auto forwardEndTime =
                forwardDayCounter.yearFraction(referenceDate, args.floatingPayDates[i]);
            forwardStartTimes_[i] = forwardStartTime;
            forwardEndTimes_[i] = forwardEndTime;
        }
    }

    void HW2CDiscretizedSwap::initialize(const ext::shared_ptr<Lattice>& discountMethod,
                                         const ext::shared_ptr<Lattice>& forwardMethod,
                                         Time t) {
        DiscretizedAsset::initialize(discountMethod, t);
        forwardMethod_ = forwardMethod;
        forwardMethod_->initialize(*this, t);
    }

    void HW2CDiscretizedSwap::addFloatingCoupon(Size i) {
        DiscretizedDiscountBond discountBond;
        discountBond.initialize(discountMethod(), floatingPayTimes_[i]);
        discountBond.rollback(time_);

        DiscretizedDiscountBond forwardBond;
        forwardBond.initialize(forwardMethod(), floatingPayTimes_[i]);
        forwardBond.rollback(time_);

        Real nominal = arguments_.nominal;
        Time T = arguments_.floatingAccrualTimes[i];
        Spread spread = arguments_.floatingSpreads[i];
        Real accruedSpread = nominal * T * spread;
        for (Size j = 0; j < values_.size(); j++) {
            auto discountFactor = discountBond.values()[j];
            auto unaccruedForwardRate = (1 / forwardBond.values()[j] - 1.0);
            auto coupon = (nominal * unaccruedForwardRate + accruedSpread) * discountFactor;
            if (arguments_.type == Swap::Payer)
                values_[j] += coupon;
            else
                values_[j] -= coupon;
        }
    }
}
