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

/*! \file hw2ctreeswapengine.cpp
    \brief Two-curve Hull-White tree engine for vanilla swaps
*/

#include <ql/experimental/hullwhitewithtwocurves/pricingengines/swap/hw2cdiscretizedswap.hpp>
#include <ql/experimental/hullwhitewithtwocurves/pricingengines/swap/hw2ctreeswapengine.hpp>
#include <algorithm>
#include <utility>

namespace QuantLib {
    HW2CTreeSwapEngine::HW2CTreeSwapEngine(Handle<HW2CModel> model, Size timeSteps)
    : GenericModelEngine(std::move(model)), timeSteps_(timeSteps) {}

    HW2CTreeSwapEngine::HW2CTreeSwapEngine(const ext::shared_ptr<HW2CModel>& model, Size timeSteps)
    : GenericModelEngine(model), timeSteps_(timeSteps) {}

    void HW2CTreeSwapEngine::calculate() const {
        QL_REQUIRE(!model_.empty(), "no model specified");

        const Date referenceDate = model_->termStructure()->referenceDate();
        const DayCounter dayCounter = model_->termStructure()->dayCounter();

        HW2CDiscretizedSwap swap(arguments_, referenceDate, dayCounter, *model_);
        auto times = swap.mandatoryTimes();
        QL_REQUIRE(!times.empty(), "no mandatory times in swap");
        const Time maxTime = times.empty() ? 0.0 : *std::max_element(times.begin(), times.end());

        TimeGrid timeGrid(times.begin(), times.end(), timeSteps_);
        const ext::shared_ptr<Lattice> lattice = model_->tree(timeGrid);

        swap.initialize(lattice, maxTime);
        swap.rollback(0.0);

        results_.value = swap.presentValue();
    }
}
