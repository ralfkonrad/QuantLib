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

/*! \file hw2cdiscretizedswaption.hpp
    \brief
*/

#ifndef quantlib_hw2c_discretized_swaption_hpp
#define quantlib_hw2c_discretized_swaption_hpp

#include <ql/experimental/hullwhitewithtwocurves/pricingengines/hw2cdiscretizedasset.hpp>
#include <ql/pricingengines/swaption/discretizedswaption.hpp>

namespace QuantLib {
    class HW2CDiscretizedSwap;

    class HW2CDiscretizedSwaption : public DiscretizedSwaption, public HW2CDiscretizedAsset {
      public:
        HW2CDiscretizedSwaption(const Swaption::arguments& args,
                                const Date& referenceDate,
                                const DayCounter& dayCounter);

        void reset(Size size) override;

        const ext::shared_ptr<Lattice>& discountMethod() const override { return method(); }
        const ext::shared_ptr<Lattice>& forwardMethod() const override { return forwardMethod_; }

        void initialize(const ext::shared_ptr<Lattice>& discountMethod,
                        const ext::shared_ptr<Lattice>& forwardMethod,
                        Time t) override;

      private:
        ext::shared_ptr<HW2CDiscretizedSwap> hw2cUnderlying_;
        ext::shared_ptr<Lattice> forwardMethod_;
    };
}

#endif // quantlib_hw2c_discretized_swaption_hpp
