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

#include "toplevelfixture.hpp"
#include "utilities.hpp"
#include <ql/cashflows/coupon.hpp>
#include <ql/experimental/hullwhitewithtwocurves/model/hw2cmodel.hpp>
#include <ql/experimental/hullwhitewithtwocurves/pricingengines/swap/hw2ctreeswapengine.hpp>
#include <ql/experimental/hullwhitewithtwocurves/pricingengines/swaption/hw2ctreeswaptionengine.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/indexes/indexmanager.hpp>
#include <ql/instruments/makevanillaswap.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/models/shortrate/onefactormodels/hullwhite.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/pricingengines/swaption/treeswaptionengine.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(HullWhiteWithTwoCurvesTests)

namespace {
    struct CommonVars {
        SavedSettings backup;

        Date today = Date(15, Nov, 2022);
        Calendar calendar = TARGET();
        DayCounter dayCounter = Actual360();
        Real nominal = 10'000.0;
        Rate fixedRate = 0.04;

        RelinkableHandle<YieldTermStructure> discountCurve;
        RelinkableHandle<YieldTermStructure> forwardCurve;

        CommonVars() {
            Settings::instance().evaluationDate() = today;
            discountCurve.linkTo(flatRate(today, 0.03, dayCounter));
            forwardCurve.linkTo(flatRate(today, 0.05, dayCounter));
        }

        ~CommonVars() { IndexManager::instance().clearHistories(); }
    };

    ext::shared_ptr<VanillaSwap> makeSwap(const CommonVars& vars,
                                          const Handle<YieldTermStructure>& forwarding) {
        auto index = ext::make_shared<Euribor6M>(forwarding);
        return MakeVanillaSwap(5 * Years, index, vars.fixedRate).withNominal(vars.nominal);
    }

    ext::shared_ptr<Swaption> makeEuropeanSwaption(const CommonVars& vars,
                                                   const Handle<YieldTermStructure>& forwarding) {
        auto swap = makeSwap(vars, forwarding);
        Date exerciseDate = vars.calendar.advance(swap->startDate(), 2 * Years);
        auto exercise = ext::make_shared<EuropeanExercise>(exerciseDate);
        return ext::make_shared<Swaption>(swap, exercise);
    }

    ext::shared_ptr<Swaption> makeBermudanSwaption(const CommonVars& vars,
                                                   const Handle<YieldTermStructure>& forwarding) {
        auto swap = makeSwap(vars, forwarding);
        std::vector<Date> exerciseDates;
        for (const auto& cf : swap->fixedLeg()) {
            auto coupon = ext::dynamic_pointer_cast<Coupon>(cf);
            exerciseDates.push_back(coupon->accrualStartDate());
        }
        exerciseDates.erase(exerciseDates.begin());
        auto exercise = ext::make_shared<BermudanExercise>(exerciseDates);
        return ext::make_shared<Swaption>(swap, exercise);
    }
}

BOOST_AUTO_TEST_CASE(testSwapSingleCurveLimitAgainstDiscountingEngine) {
    BOOST_TEST_MESSAGE("Testing HW2C swap in single-curve limit against DiscountingSwapEngine...");

    CommonVars vars;

    auto swap = makeSwap(vars, vars.forwardCurve);
    swap->setPricingEngine(ext::make_shared<DiscountingSwapEngine>(vars.discountCurve));
    const Real discountingNpv = swap->NPV();

    auto hw2cModel = ext::make_shared<HW2CModel>(vars.discountCurve, vars.forwardCurve);
    swap->setPricingEngine(ext::make_shared<HW2CTreeSwapEngine>(hw2cModel, 80));
    const Real hw2cNpv = swap->NPV();

    const Real relTolerance = 0.000'5;
    if (std::fabs((hw2cNpv - discountingNpv) / discountingNpv) > relTolerance) {
        BOOST_ERROR("single-curve limit mismatch for swap:\n"
                    << "discounting NPV: " << discountingNpv << "\n"
                    << "HW2C tree NPV:   " << hw2cNpv << "\n"
                    << "rel. diff:       " << (hw2cNpv - discountingNpv) / discountingNpv << "\n"
                    << "rel. tolerance:  " << relTolerance);
    }
}

BOOST_AUTO_TEST_CASE(testEuropeanSwaptionSingleCurveLimitAgainstTreeSwaptionEngine) {
    BOOST_TEST_MESSAGE("Testing HW2C European swaption in single-curve limit against "
                       "TreeSwaptionEngine...");

    CommonVars vars;
    auto swaption = makeEuropeanSwaption(vars, vars.discountCurve);

    auto oneCurveModel = ext::make_shared<HullWhite>(vars.discountCurve);
    swaption->setPricingEngine(ext::make_shared<TreeSwaptionEngine>(oneCurveModel, 80));
    const Real oneCurveNpv = swaption->NPV();

    auto hw2cModel = ext::make_shared<HW2CModel>(vars.discountCurve, vars.discountCurve);
    swaption->setPricingEngine(ext::make_shared<HW2CTreeSwaptionEngine>(hw2cModel, 80));
    const Real hw2cNpv = swaption->NPV();

    const Real relTolerance = 0.006;
    if (std::fabs((hw2cNpv - oneCurveNpv) / oneCurveNpv) > relTolerance) {
        BOOST_ERROR("single-curve limit mismatch for European swaption:\n"
                    << "TreeSwaptionEngine NPV: " << oneCurveNpv << "\n"
                    << "HW2C tree NPV:          " << hw2cNpv << "\n"
                    << "rel. diff:              " << (hw2cNpv - oneCurveNpv) / oneCurveNpv << "\n"
                    << "rel. tolerance:         " << relTolerance);
    }
}

BOOST_AUTO_TEST_CASE(testBermudanSwaptionSingleCurveLimitAgainstTreeSwaptionEngine) {
    BOOST_TEST_MESSAGE("Testing HW2C Bermudan swaption in single-curve limit against "
                       "TreeSwaptionEngine...");

    CommonVars vars;
    auto swaption = makeBermudanSwaption(vars, vars.discountCurve);

    auto oneCurveModel = ext::make_shared<HullWhite>(vars.discountCurve);
    swaption->setPricingEngine(ext::make_shared<TreeSwaptionEngine>(oneCurveModel, 80));
    const Real oneCurveNpv = swaption->NPV();

    auto hw2cModel = ext::make_shared<HW2CModel>(vars.discountCurve, vars.discountCurve);
    swaption->setPricingEngine(ext::make_shared<HW2CTreeSwaptionEngine>(hw2cModel, 80));
    const Real hw2cNpv = swaption->NPV();

    const Real relTolerance = 0.006;
    if (std::fabs((hw2cNpv - oneCurveNpv) / oneCurveNpv) > relTolerance) {
        BOOST_ERROR("single-curve limit mismatch for European swaption:\n"
                    << "TreeSwaptionEngine NPV: " << oneCurveNpv << "\n"
                    << "HW2C tree NPV:          " << hw2cNpv << "\n"
                    << "rel. diff:              " << (hw2cNpv - oneCurveNpv) / oneCurveNpv << "\n"
                    << "rel. tolerance:         " << relTolerance);
    }
}

BOOST_AUTO_TEST_CASE(testSwapPayerReceiverParity) {
    BOOST_TEST_MESSAGE("Testing HW2C swap payer/receiver parity...");

    CommonVars vars;

    auto hw2cModel = ext::make_shared<HW2CModel>(vars.discountCurve, vars.forwardCurve);
    auto index = ext::make_shared<Euribor6M>(vars.forwardCurve);

    ext::shared_ptr<VanillaSwap> payer = MakeVanillaSwap(5 * Years, index, vars.fixedRate)
                                             .withNominal(vars.nominal)
                                             .withType(Swap::Payer);
    ext::shared_ptr<VanillaSwap> receiver = MakeVanillaSwap(5 * Years, index, vars.fixedRate)
                                                .withNominal(vars.nominal)
                                                .withType(Swap::Receiver);

    payer->setPricingEngine(ext::make_shared<HW2CTreeSwapEngine>(hw2cModel, 80));
    receiver->setPricingEngine(ext::make_shared<HW2CTreeSwapEngine>(hw2cModel, 80));

    const Real payerNpv = payer->NPV();
    const Real receiverNpv = receiver->NPV();
    const Real sum = payerNpv + receiverNpv;

    const Real tolerance = 1e-6;
    if (std::fabs(sum) > tolerance) {
        BOOST_ERROR("HW2C payer + receiver swap NPV != 0:\n"
                    << "payer NPV:    " << payerNpv << "\n"
                    << "receiver NPV: " << receiverNpv << "\n"
                    << "sum:          " << sum << "\n"
                    << "tolerance:    " << tolerance);
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
