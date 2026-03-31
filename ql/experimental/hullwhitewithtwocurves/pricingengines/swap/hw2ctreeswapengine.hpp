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

/*! \file hw2ctreeswapengine.hpp
    \brief Two-curve Hull-White tree engine for vanilla swaps
*/

#ifndef quantlib_hw2c_tree_swap_engine_hpp
#define quantlib_hw2c_tree_swap_engine_hpp

#include <ql/experimental/hullwhitewithtwocurves/model/hw2cmodel.hpp>
#include <ql/instruments/vanillaswap.hpp>
#include <ql/pricingengines/genericmodelengine.hpp>

namespace QuantLib {

    //! Two-curve Hull-White tree pricing engine for vanilla swaps.
    /*! Prices a VanillaSwap by building a single trinomial tree from
        an HW2CModel and performing backward induction through an
        HW2CDiscretizedSwap.  Forward-rate projection uses analytical
        forward discount bonds while the tree drives present-value
        discounting.

        \ingroup swapengines
    */
    class HW2CTreeSwapEngine
    : public GenericModelEngine<HW2CModel, VanillaSwap::arguments, VanillaSwap::results> {

      public:
        HW2CTreeSwapEngine(Handle<HW2CModel> model, Size timeSteps);
        HW2CTreeSwapEngine(const ext::shared_ptr<HW2CModel>& model, Size timeSteps);

        void calculate() const override;

      private:
        Size timeSteps_;
    };
}

#endif // quantlib_hw2c_tree_swap_engine_hpp
