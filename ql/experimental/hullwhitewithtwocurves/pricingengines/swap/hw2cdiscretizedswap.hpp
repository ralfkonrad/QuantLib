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

/*! \file hw2cdiscretizedswap.hpp
    \brief
*/

#ifndef quantlib_hw2c_discretized_swap_hpp
#define quantlib_hw2c_discretized_swap_hpp

#include <ql/pricingengines/swap/discretizedswap.hpp>

namespace QuantLib {
    class HW2CDiscretizedSwap : public DiscretizedSwap {
      public:
        HW2CDiscretizedSwap(const VanillaSwap::arguments& args,
                            const Date& referenceDate,
                            const DayCounter& dayCounter);

        HW2CDiscretizedSwap(const VanillaSwap::arguments& args,
                            const Date& referenceDate,
                            const DayCounter& dayCounter,
                            std::vector<CouponAdjustment> fixedCouponAdjustments,
                            std::vector<CouponAdjustment> floatingCouponAdjustments);

        void initialize(const ext::shared_ptr<Lattice>& discountMethod,
                        const ext::shared_ptr<Lattice>& forwardMethod,
                        Time t);

        const ext::shared_ptr<Lattice>& discountMethod() const { return method(); }
        const ext::shared_ptr<Lattice>& forwardMethod() const { return forwardMethod_; }

      protected:
        void addFloatingCoupon(Size i) override;

      private:
        ext::shared_ptr<Lattice> forwardMethod_;
    };
}


#endif // quantlib_hw2c_discretized_swap_hpp
