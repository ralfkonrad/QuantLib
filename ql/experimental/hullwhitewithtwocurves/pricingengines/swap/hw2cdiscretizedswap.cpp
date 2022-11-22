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
                      std::move(floatingCouponAdjustments)) {
        auto nrOfFloatingCoupons = args.floatingResetDates.size();
        indexStartTimes_.resize(nrOfFloatingCoupons);
        indexEndTimes_.resize(nrOfFloatingCoupons);
        for (Size i = 0; i < nrOfFloatingCoupons; ++i) {
            auto fixingValueTime = dayCounter.yearFraction(referenceDate, args.fixingValueDates[i]);
            auto fixingEndTime = dayCounter.yearFraction(referenceDate, args.fixingEndDates[i]);
            indexStartTimes_[i] = fixingValueTime;
            indexEndTimes_[i] = fixingEndTime;
        }
    }

    std::vector<Time> HW2CDiscretizedSwap::mandatoryTimes() const {
        auto times = DiscretizedSwap::mandatoryTimes();
        std::copy_if(indexStartTimes_.begin(), indexStartTimes_.end(), std::back_inserter(times),
                     [](auto time) { return time >= 0; });
        std::copy_if(indexEndTimes_.begin(), indexEndTimes_.end(), std::back_inserter(times),
                     [](auto time) { return time >= 0; });
        return times;
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

        DiscretizedDiscountBond indexBond;
        indexBond.initialize(forwardMethod(), indexEndTimes_[i]);
        indexBond.rollback(indexStartTimes_[i]);

        auto nominal = arguments_.nominal;
        auto T = arguments_.floatingAccrualTimes[i];
        auto fixingSpanningTime = arguments_.fixingSpanningTimes[i];
        auto spread = arguments_.floatingSpreads[i];
        auto accruedSpread = nominal * T * spread;
        for (Size j = 0; j < values_.size(); j++) {
            auto discountFactor = discountBond.values()[j];
            auto forwardRate = (1.0 / indexBond.values()[j] - 1.0) / fixingSpanningTime;
            auto accruedNominal = nominal * T * forwardRate;
            auto coupon = (accruedNominal + accruedSpread) * discountFactor;

            if (arguments_.type == Swap::Payer)
                values_[j] += coupon;
            else
                values_[j] -= coupon;
        }
    }
}
