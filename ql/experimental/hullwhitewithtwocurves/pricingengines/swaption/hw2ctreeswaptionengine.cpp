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

#include <ql/experimental/hullwhitewithtwocurves/pricingengines/swaption/hw2cdiscretizedswaption.hpp>
#include <ql/experimental/hullwhitewithtwocurves/pricingengines/swaption/hw2ctreeswaptionengine.hpp>
#include <algorithm>
#include <utility>

namespace QuantLib {
    HW2CTreeSwaptionEngine::HW2CTreeSwaptionEngine(Handle<HW2CModel> model, Size timeSteps)
    : GenericModelEngine(std::move(model)), timeSteps_(timeSteps) {}

    HW2CTreeSwaptionEngine::HW2CTreeSwaptionEngine(const ext::shared_ptr<HW2CModel>& model,
                                                   Size timeSteps)
    : GenericModelEngine(model), timeSteps_(timeSteps) {}

    void HW2CTreeSwaptionEngine::calculate() const {
        QL_REQUIRE(arguments_.settlementMethod != Settlement::ParYieldCurve,
                   "cash settled (ParYieldCurve) swaptions not priced with "
                   "HW2CTreeSwaptionEngine");
        QL_REQUIRE(!model_.empty(), "no model specified");

        const Date referenceDate = model_->discountModel()->termStructure()->referenceDate();
        const DayCounter dayCounter = model_->discountModel()->termStructure()->dayCounter();

        HW2CDiscretizedSwaption swaption(arguments_, referenceDate, dayCounter);

        auto mandatoryTimes = swaption.mandatoryTimes();
        TimeGrid timeGrid(mandatoryTimes.begin(), mandatoryTimes.end(), timeSteps_);
        const ext::shared_ptr<Lattice> discountLattice = model_->discountTree(timeGrid);
        const ext::shared_ptr<Lattice> forwardLattice = model_->forwardTree(timeGrid);

        std::vector<Time> stoppingTimes(arguments_.exercise->dates().size());
        for (Size i = 0; i < stoppingTimes.size(); ++i)
            stoppingTimes[i] = dayCounter.yearFraction(referenceDate, arguments_.exercise->date(i));

        const Time lastTime = stoppingTimes.empty() ? 0.0 : stoppingTimes.back();

        swaption.initialize(discountLattice, forwardLattice, lastTime);
        const Time nextExercise = *std::find_if(stoppingTimes.begin(), stoppingTimes.end(),
                                                [](Time t) { return t >= 0.0; });
        swaption.rollback(nextExercise);

        results_.value = swaption.presentValue();
    }
}
