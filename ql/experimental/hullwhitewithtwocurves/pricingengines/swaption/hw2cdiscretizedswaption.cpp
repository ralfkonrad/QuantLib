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

/*! \file hw2cdiscretizedswaption.cpp
    \brief Discretized swaption for two-curve Hull-White lattice pricing
*/

#include <ql/experimental/hullwhitewithtwocurves/model/hw2cmodel.hpp>
#include <ql/experimental/hullwhitewithtwocurves/pricingengines/swap/hw2cdiscretizedswap.hpp>
#include <ql/experimental/hullwhitewithtwocurves/pricingengines/swaption/hw2cdiscretizedswaption.hpp>

namespace QuantLib {

    namespace {
        bool withinPreviousWeek(const Date& d1, const Date& d2) {
            return d2 >= d1 - 7 && d2 <= d1;
        }

        bool withinNextWeek(const Date& d1, const Date& d2) {
            return d2 >= d1 && d2 <= d1 + 7;
        }

        bool withinOneWeek(const Date& d1, const Date& d2) {
            return withinPreviousWeek(d1, d2) || withinNextWeek(d1, d2);
        }
    }

    HW2CDiscretizedSwaption::HW2CDiscretizedSwaption(const Swaption::arguments& args,
                                                     const Date& referenceDate,
                                                     const DayCounter& dayCounter,
                                                     ext::shared_ptr<HW2CModel> model)
    : DiscretizedOption(
          ext::shared_ptr<DiscretizedAsset>(), args.exercise->type(), std::vector<Time>()),
      arguments_(args), lastPayment_(0.0) {
        Swaption::arguments snappedArgs;
        std::vector<CouponAdjustment> fixedCouponAdjustments;
        std::vector<CouponAdjustment> floatingCouponAdjustments;

        prepareSwaptionWithSnappedDates(arguments_, snappedArgs, fixedCouponAdjustments,
                                        floatingCouponAdjustments);

        exerciseTimes_.resize(snappedArgs.exercise->dates().size());
        for (Size i = 0; i < exerciseTimes_.size(); ++i) {
            exerciseTimes_[i] =
                dayCounter.yearFraction(referenceDate, snappedArgs.exercise->date(i));
        }

        const Time lastFixedPayment =
            dayCounter.yearFraction(referenceDate, snappedArgs.fixedPayDates.back());
        const Time lastFloatingPayment =
            dayCounter.yearFraction(referenceDate, snappedArgs.floatingPayDates.back());
        lastPayment_ = std::max(lastFixedPayment, lastFloatingPayment);

        hw2cUnderlying_ = ext::make_shared<HW2CDiscretizedSwap>(
            snappedArgs, referenceDate, dayCounter, std::move(model),
            std::move(fixedCouponAdjustments), std::move(floatingCouponAdjustments));
        underlying_ = hw2cUnderlying_;
    }

    void HW2CDiscretizedSwaption::reset(Size size) {
        hw2cUnderlying_->initialize(method(), lastPayment_);
        DiscretizedOption::reset(size);
    }

    void HW2CDiscretizedSwaption::prepareSwaptionWithSnappedDates(
        const Swaption::arguments& args,
        Swaption::arguments& snappedArgs,
        std::vector<CouponAdjustment>& fixedCouponAdjustments,
        std::vector<CouponAdjustment>& floatingCouponAdjustments) {
        std::vector<Date> fixedDates = args.swap->fixedSchedule().dates();
        std::vector<Date> floatDates = args.swap->floatingSchedule().dates();

        fixedCouponAdjustments.resize(args.swap->fixedLeg().size(), CouponAdjustment::pre);
        floatingCouponAdjustments.resize(args.swap->floatingLeg().size(), CouponAdjustment::pre);

        for (const auto& exerciseDate : args.exercise->dates()) {
            for (Size j = 0; j < fixedDates.size() - 1; j++) {
                const Date unadjustedDate = fixedDates[j];
                if (exerciseDate != unadjustedDate && withinOneWeek(exerciseDate, unadjustedDate)) {
                    fixedDates[j] = exerciseDate;
                    if (withinPreviousWeek(exerciseDate, unadjustedDate))
                        fixedCouponAdjustments[j] = CouponAdjustment::post;
                }
            }

            for (Size j = 0; j < floatDates.size() - 1; j++) {
                const Date unadjustedDate = floatDates[j];
                if (exerciseDate != unadjustedDate && withinOneWeek(exerciseDate, unadjustedDate)) {
                    floatDates[j] = exerciseDate;
                    if (withinPreviousWeek(exerciseDate, unadjustedDate))
                        floatingCouponAdjustments[j] = CouponAdjustment::post;
                }
            }
        }

        const Schedule snappedFixedSchedule(fixedDates);
        const Schedule snappedFloatSchedule(floatDates);

        const ext::shared_ptr<VanillaSwap> snappedSwap = ext::make_shared<VanillaSwap>(
            args.swap->type(), args.swap->nominal(), snappedFixedSchedule, args.swap->fixedRate(),
            args.swap->fixedDayCount(), snappedFloatSchedule, args.swap->iborIndex(),
            args.swap->spread(), args.swap->floatingDayCount(), args.swap->paymentConvention());

        const Swaption snappedSwaption(snappedSwap, args.exercise, args.settlementType,
                                       args.settlementMethod);

        snappedSwaption.setupArguments(&snappedArgs);
    }
}
