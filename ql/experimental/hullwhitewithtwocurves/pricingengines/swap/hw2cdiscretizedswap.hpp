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

/*! \file hw2cdiscretizedswap.hpp
    \brief Discretized vanilla swap for two-curve Hull-White lattice pricing
*/

#ifndef quantlib_hw2c_discretized_swap_hpp
#define quantlib_hw2c_discretized_swap_hpp

#include <ql/discretizedasset.hpp>
#include <ql/instruments/vanillaswap.hpp>
#include <ql/models/shortrate/onefactormodel.hpp>
#include <ql/shared_ptr.hpp>

namespace QuantLib {

    class HW2CModel;

    //! Discretized vanilla swap for two-curve Hull-White tree pricing.
    /*! This class discretizes a vanilla interest-rate swap for
        backward induction on a Hull-White trinomial tree.
        Fixed-leg and floating-leg coupons are computed analytically
        at each tree node using the affine bond pricing formulas of
        the HW2CModel, eliminating the need for a separate forward
        tree.

        \ingroup swapengines
    */
    class HW2CDiscretizedSwap : public DiscretizedAsset {
      public:
        /*! Constructs from swap arguments with default (pre-adjustment)
            coupon adjustment for all coupons. */
        HW2CDiscretizedSwap(const VanillaSwap::arguments& args,
                            const Date& referenceDate,
                            const DayCounter& dayCounter,
                            ext::shared_ptr<HW2CModel> model);

        /*! Constructs from swap arguments with explicit coupon
            adjustment vectors (pre or post) for swaption date-snapping
            support. */
        HW2CDiscretizedSwap(const VanillaSwap::arguments& args,
                            const Date& referenceDate,
                            const DayCounter& dayCounter,
                            ext::shared_ptr<HW2CModel> model,
                            std::vector<CouponAdjustment> fixedCouponAdjustments,
                            std::vector<CouponAdjustment> floatingCouponAdjustments);

        void reset(Size size) override;
        std::vector<Time> mandatoryTimes() const override;

      protected:
        void preAdjustValuesImpl() override;
        void postAdjustValuesImpl() override;

      private:
        ext::shared_ptr<HW2CModel> model_;

        VanillaSwap::arguments arguments_;

        std::vector<Time> fixedResetTimes_;
        std::vector<Time> fixedPayTimes_;
        std::vector<CouponAdjustment> fixedCouponAdjustments_;
        std::vector<bool> fixedResetTimeIsInPast_;

        std::vector<Time> floatingResetTimes_;
        std::vector<Time> floatingPayTimes_;
        std::vector<CouponAdjustment> floatingCouponAdjustments_;
        std::vector<bool> floatingResetTimeIsInPast_;

        std::vector<Time> indexEndTimes_;

        ext::shared_ptr<OneFactorModel::ShortRateTree> shortRateTree_;

        void addFixedCoupon(Size i);
        void addFloatingCoupon(Size i);
    };
}

#endif // quantlib_hw2c_discretized_swap_hpp
