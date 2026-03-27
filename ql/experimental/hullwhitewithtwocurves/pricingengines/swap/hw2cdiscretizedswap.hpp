/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2022, 2026 Ralf Konrad Eckel

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <https://www.quantlib.org/license.shtml>.

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
#include <ql/experimental/hullwhitewithtwocurves/pricingengines/hw2cdiscretizedasset.hpp>
#include <ql/instruments/vanillaswap.hpp>

namespace QuantLib {

    //! Discretized vanilla swap for two-curve Hull-White tree pricing.
    /*! This class discretizes a vanilla interest-rate swap for
        backward induction on a dual-lattice Hull-White tree.
        Fixed-leg coupons are discounted using the discount lattice,
        while floating-leg coupons are projected from the forward
        lattice and then discounted through the discount lattice.

        The class inherits from both DiscretizedAsset (lattice
        mechanics) and HW2CDiscretizedAsset (dual-lattice
        initialization contract).

        \ingroup swapengines
    */
    class HW2CDiscretizedSwap : public DiscretizedAsset, public HW2CDiscretizedAsset {
      public:
        /*! Constructs from swap arguments with default (pre-adjustment)
            coupon adjustment for all coupons. */
        HW2CDiscretizedSwap(const VanillaSwap::arguments& args,
                            const Date& referenceDate,
                            const DayCounter& dayCounter);

        /*! Constructs from swap arguments with explicit coupon
            adjustment vectors (pre or post) for swaption date-snapping
            support. */
        HW2CDiscretizedSwap(const VanillaSwap::arguments& args,
                            const Date& referenceDate,
                            const DayCounter& dayCounter,
                            std::vector<CouponAdjustment> fixedCouponAdjustments,
                            std::vector<CouponAdjustment> floatingCouponAdjustments);

        void reset(Size size) override;
        std::vector<Time> mandatoryTimes() const override;

        const ext::shared_ptr<Lattice>& discountMethod() const override { return method(); }
        const ext::shared_ptr<Lattice>& forwardMethod() const override { return forwardMethod_; }

        void initialize(const ext::shared_ptr<Lattice>& discountMethod,
                        const ext::shared_ptr<Lattice>& forwardMethod,
                        Time t) override;

      protected:
        void preAdjustValuesImpl() override;
        void postAdjustValuesImpl() override;

      private:
        VanillaSwap::arguments arguments_;

        std::vector<Time> fixedResetTimes_;
        std::vector<Time> fixedPayTimes_;
        std::vector<CouponAdjustment> fixedCouponAdjustments_;
        std::vector<bool> fixedResetTimeIsInPast_;

        std::vector<Time> floatingResetTimes_;
        std::vector<Time> floatingPayTimes_;
        std::vector<CouponAdjustment> floatingCouponAdjustments_;
        std::vector<bool> floatingResetTimeIsInPast_;

        std::vector<Time> indexStartTimes_;
        std::vector<Time> indexEndTimes_;
        std::vector<Time> fixingSpanningTimes_;

        ext::shared_ptr<Lattice> forwardMethod_;

        void addFixedCoupon(Size i);
        void addFloatingCoupon(Size i);
    };
}

#endif // quantlib_hw2c_discretized_swap_hpp
