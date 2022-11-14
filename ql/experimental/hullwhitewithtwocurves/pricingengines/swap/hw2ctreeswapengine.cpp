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
#include <ql/experimental/hullwhitewithtwocurves/pricingengines/swap/hw2ctreeswapengine.hpp>

namespace QuantLib {
    HW2CTreeSwapEngine::HW2CTreeSwapEngine(Handle<HW2CModel> model, Size timeSteps)
    : GenericModelEngine(model), timeSteps_(timeSteps) {}

    HW2CTreeSwapEngine::HW2CTreeSwapEngine(const ext::shared_ptr<HW2CModel>& model, Size timeSteps)
    : GenericModelEngine(model), timeSteps_(timeSteps) {}

    void HW2CTreeSwapEngine::calculate() const {
        QL_REQUIRE(!model_.empty(), "no model specified");

        auto referenceDate = model_->discountModel()->termStructure()->referenceDate();
        auto dayCounter = model_->discountModel()->termStructure()->dayCounter();
        auto forwardDayCounter = model_->forwardModel()->termStructure()->dayCounter();

        auto swap = HW2CDiscretizedSwap(arguments_, referenceDate, dayCounter, forwardDayCounter);
        auto times = swap.mandatoryTimes();
        auto maxTime = *std::max_element(times.begin(), times.end());

        auto timeGrid = TimeGrid(times.begin(), times.end(), timeSteps_);
        auto discountLattice = model_->discountTree(timeGrid);
        auto forwardLattice = model_->forwardTree(timeGrid);

        swap.initialize(discountLattice, forwardLattice, maxTime);
        swap.rollback(0.0);

        results_.value = swap.presentValue();
    }
}
