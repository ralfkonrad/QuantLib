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
#include <ql/settings.hpp>
#include <utility>

namespace QuantLib {

    namespace {
        inline bool isResetTimeInPast(const Time& resetTime,
                                      const Time& payTime,
                                      const bool& includeTodaysCashFlows) {
            return (resetTime < 0.0) &&
                   ((payTime > 0.0) || (includeTodaysCashFlows && (payTime == 0.0)));
        }
    }

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
    : arguments_(args), fixedCouponAdjustments_(std::move(fixedCouponAdjustments)),
      floatingCouponAdjustments_(std::move(floatingCouponAdjustments)) {
        QL_REQUIRE(fixedCouponAdjustments_.size() == arguments_.fixedPayDates.size(),
                   "The fixed coupon adjustments must have the same size as the number of fixed "
                   "coupons.");
        QL_REQUIRE(floatingCouponAdjustments_.size() == arguments_.floatingPayDates.size(),
                   "The floating coupon adjustments must have the same size as the number of "
                   "floating coupons.");

        const bool includeTodaysCashFlows = Settings::instance().includeTodaysCashFlows() &&
                                            *Settings::instance().includeTodaysCashFlows();

        const Size numberOfFixedCoupons = args.fixedResetDates.size();
        fixedResetTimes_.resize(numberOfFixedCoupons);
        fixedPayTimes_.resize(numberOfFixedCoupons);
        fixedResetTimeIsInPast_.resize(numberOfFixedCoupons);
        for (Size i = 0; i < numberOfFixedCoupons; ++i) {
            const Time resetTime = dayCounter.yearFraction(referenceDate, args.fixedResetDates[i]);
            const Time payTime = dayCounter.yearFraction(referenceDate, args.fixedPayDates[i]);
            const bool resetIsInPast =
                isResetTimeInPast(resetTime, payTime, includeTodaysCashFlows);

            fixedResetTimes_[i] = resetTime;
            fixedPayTimes_[i] = payTime;
            fixedResetTimeIsInPast_[i] = resetIsInPast;
            if (resetIsInPast)
                fixedCouponAdjustments_[i] = CouponAdjustment::post;
        }

        const Size numberOfFloatingCoupons = args.floatingResetDates.size();
        floatingResetTimes_.resize(numberOfFloatingCoupons);
        floatingPayTimes_.resize(numberOfFloatingCoupons);
        floatingResetTimeIsInPast_.resize(numberOfFloatingCoupons);
        indexStartTimes_.resize(numberOfFloatingCoupons);
        indexEndTimes_.resize(numberOfFloatingCoupons);
        fixingSpanningTimes_.resize(numberOfFloatingCoupons);

        for (Size i = 0; i < numberOfFloatingCoupons; ++i) {
            const Time resetTime =
                dayCounter.yearFraction(referenceDate, args.floatingResetDates[i]);
            const Time payTime = dayCounter.yearFraction(referenceDate, args.floatingPayDates[i]);
            const bool resetIsInPast =
                isResetTimeInPast(resetTime, payTime, includeTodaysCashFlows);

            floatingResetTimes_[i] = resetTime;
            floatingPayTimes_[i] = payTime;
            floatingResetTimeIsInPast_[i] = resetIsInPast;
            if (resetIsInPast)
                floatingCouponAdjustments_[i] = CouponAdjustment::post;

            const Time spanning = args.floatingAccrualTimes[i];
            QL_REQUIRE(spanning > 0.0, "non-positive floating accrual time");
            fixingSpanningTimes_[i] = spanning;

            indexStartTimes_[i] = resetTime;
            indexEndTimes_[i] = resetTime + spanning;
        }
    }

    void HW2CDiscretizedSwap::reset(Size size) {
        values_ = Array(size, 0.0);
        adjustValues();
    }

    std::vector<Time> HW2CDiscretizedSwap::mandatoryTimes() const {
        std::vector<Time> times;
        for (Real t : fixedResetTimes_) {
            if (t >= 0.0)
                times.push_back(t);
        }
        for (Real t : fixedPayTimes_) {
            if (t >= 0.0)
                times.push_back(t);
        }
        for (Real t : floatingResetTimes_) {
            if (t >= 0.0)
                times.push_back(t);
        }
        for (Real t : floatingPayTimes_) {
            if (t >= 0.0)
                times.push_back(t);
        }
        for (Real t : indexStartTimes_) {
            if (t >= 0.0)
                times.push_back(t);
        }
        for (Real t : indexEndTimes_) {
            if (t >= 0.0)
                times.push_back(t);
        }
        return times;
    }

    void HW2CDiscretizedSwap::initialize(const ext::shared_ptr<Lattice>& discountMethod,
                                         const ext::shared_ptr<Lattice>& forwardMethod,
                                         Time t) {
        DiscretizedAsset::initialize(discountMethod, t);
        forwardMethod_ = forwardMethod;
        QL_REQUIRE(forwardMethod_ != nullptr, "forward lattice not provided");
        forwardMethod_->initialize(*this, t);
    }

    void HW2CDiscretizedSwap::preAdjustValuesImpl() {
        for (Size i = 0; i < floatingResetTimes_.size(); i++) {
            const Time t = floatingResetTimes_[i];
            if (floatingCouponAdjustments_[i] == CouponAdjustment::pre && t >= 0.0 && isOnTime(t)) {
                addFloatingCoupon(i);
            }
        }
        for (Size i = 0; i < fixedResetTimes_.size(); i++) {
            const Time t = fixedResetTimes_[i];
            if (fixedCouponAdjustments_[i] == CouponAdjustment::pre && t >= 0.0 && isOnTime(t)) {
                addFixedCoupon(i);
            }
        }
    }

    void HW2CDiscretizedSwap::postAdjustValuesImpl() {
        for (Size i = 0; i < floatingResetTimes_.size(); i++) {
            const Time t = floatingResetTimes_[i];
            if (floatingCouponAdjustments_[i] == CouponAdjustment::post && t >= 0.0 &&
                isOnTime(t)) {
                addFloatingCoupon(i);
            }
        }
        for (Size i = 0; i < fixedResetTimes_.size(); i++) {
            const Time t = fixedResetTimes_[i];
            if (fixedCouponAdjustments_[i] == CouponAdjustment::post && t >= 0.0 && isOnTime(t)) {
                addFixedCoupon(i);
            }
        }

        for (Size i = 0; i < fixedPayTimes_.size(); i++) {
            const Time t = fixedPayTimes_[i];
            if (fixedResetTimeIsInPast_[i] && isOnTime(t)) {
                const Real fixedCoupon = arguments_.fixedCoupons[i];
                if (arguments_.type == Swap::Payer)
                    values_ -= fixedCoupon;
                else
                    values_ += fixedCoupon;
            }
        }

        for (Size i = 0; i < floatingPayTimes_.size(); i++) {
            const Time t = floatingPayTimes_[i];
            if (floatingResetTimeIsInPast_[i] && isOnTime(t)) {
                const Real currentFloatingCoupon = arguments_.floatingCoupons[i];
                QL_REQUIRE(currentFloatingCoupon != Null<Real>(),
                           "current floating coupon not given");
                if (arguments_.type == Swap::Payer)
                    values_ += currentFloatingCoupon;
                else
                    values_ -= currentFloatingCoupon;
            }
        }
    }

    void HW2CDiscretizedSwap::addFixedCoupon(Size i) {
        DiscretizedDiscountBond discountBond;
        discountBond.initialize(discountMethod(), fixedPayTimes_[i]);
        discountBond.rollback(time_);

        const Real fixedCoupon = arguments_.fixedCoupons[i];
        for (Size j = 0; j < values_.size(); j++) {
            const Real coupon = fixedCoupon * discountBond.values()[j];
            if (arguments_.type == Swap::Payer)
                values_[j] -= coupon;
            else
                values_[j] += coupon;
        }
    }

    void HW2CDiscretizedSwap::addFloatingCoupon(Size i) {
        DiscretizedDiscountBond discountBond;
        discountBond.initialize(discountMethod(), floatingPayTimes_[i]);
        discountBond.rollback(time_);

        DiscretizedDiscountBond forwardBond;
        forwardBond.initialize(forwardMethod(), indexEndTimes_[i]);
        forwardBond.rollback(indexStartTimes_[i]);

        QL_REQUIRE(arguments_.nominal != Null<Real>(),
                   "non-constant nominals are not supported yet");

        const Real nominal = arguments_.nominal;
        const Time accrualTime = arguments_.floatingAccrualTimes[i];
        const Time spanningTime = fixingSpanningTimes_[i];
        const Spread spread = arguments_.floatingSpreads[i];

        for (Size j = 0; j < values_.size(); j++) {
            const Real projectionDiscount = forwardBond.values()[j];
            const Real forwardRate = (1.0 / projectionDiscount - 1.0) / spanningTime;
            const Real couponAmount = nominal * accrualTime * (forwardRate + spread);
            const Real discountedCoupon = couponAmount * discountBond.values()[j];

            if (arguments_.type == Swap::Payer)
                values_[j] += discountedCoupon;
            else
                values_[j] -= discountedCoupon;
        }
    }
}
