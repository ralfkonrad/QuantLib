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

/*! \file hw2cdiscretizedswaption.hpp
    \brief Discretized swaption for two-curve Hull-White lattice pricing
*/

#ifndef quantlib_hw2c_discretized_swaption_hpp
#define quantlib_hw2c_discretized_swaption_hpp

#include <ql/discretizedasset.hpp>
#include <ql/instruments/swaption.hpp>

namespace QuantLib {
    class HW2CDiscretizedSwap;
    class HW2CModel;

    //! Discretized swaption for two-curve Hull-White tree pricing.
    /*! This class discretizes a swaption (European or Bermudan) for
        backward induction on a Hull-White trinomial tree.  It wraps
        an HW2CDiscretizedSwap as its underlying and delegates
        analytical coupon computation to it during reset.

        Exercise dates that fall within one week of a coupon reset
        date are snapped to the exercise date, with coupon adjustment
        flags (pre/post) set accordingly to maintain correct
        valuation.

        \ingroup swaptionengines
    */
    class HW2CDiscretizedSwaption : public DiscretizedOption {
      public:
        HW2CDiscretizedSwaption(const Swaption::arguments& args,
                                const Date& referenceDate,
                                const DayCounter& dayCounter,
                                ext::shared_ptr<HW2CModel> model);

        void reset(Size size) override;

        //! Returns the exercise times (as year fractions from reference date).
        const std::vector<Time>& exerciseTimes() const { return exerciseTimes_; }

      private:
        static void
        prepareSwaptionWithSnappedDates(const Swaption::arguments& args,
                                        Swaption::arguments& snappedArgs,
                                        std::vector<CouponAdjustment>& fixedCouponAdjustments,
                                        std::vector<CouponAdjustment>& floatingCouponAdjustments);

        Swaption::arguments arguments_;
        Time lastPayment_;
        ext::shared_ptr<HW2CDiscretizedSwap> hw2cUnderlying_;
    };
}

#endif // quantlib_hw2c_discretized_swaption_hpp
