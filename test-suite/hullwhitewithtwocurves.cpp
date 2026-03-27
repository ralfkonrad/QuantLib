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

        Date today;
        Calendar calendar;
        DayCounter dayCounter;
        Real nominal;
        Rate fixedRate;

        RelinkableHandle<YieldTermStructure> discountCurve;
        RelinkableHandle<YieldTermStructure> forwardCurve;

        CommonVars()
        : today(15, Nov, 2022), calendar(TARGET()), dayCounter(Actual360()), nominal(10000.0),
          fixedRate(0.04) {
            Settings::instance().evaluationDate() = today;
            discountCurve.linkTo(flatRate(today, 0.03, dayCounter));
            forwardCurve.linkTo(flatRate(today, 0.03, dayCounter));
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

    const Real tolerance = 0.35;
    if (std::fabs(hw2cNpv - discountingNpv) > tolerance) {
        BOOST_ERROR("single-curve limit mismatch for swap:\n"
                    << "discounting NPV: " << discountingNpv << "\n"
                    << "HW2C tree NPV:  " << hw2cNpv << "\n"
                    << "diff:           " << (hw2cNpv - discountingNpv) << "\n"
                    << "tolerance:      " << tolerance);
    }
}

BOOST_AUTO_TEST_CASE(testEuropeanSwaptionSingleCurveLimitAgainstTreeSwaptionEngine) {
    BOOST_TEST_MESSAGE("Testing HW2C European swaption in single-curve limit against "
                       "TreeSwaptionEngine...");

    CommonVars vars;
    auto swaption = makeEuropeanSwaption(vars, vars.forwardCurve);

    auto oneCurveModel = ext::make_shared<HullWhite>(vars.discountCurve);
    swaption->setPricingEngine(ext::make_shared<TreeSwaptionEngine>(oneCurveModel, 80));
    const Real oneCurveNpv = swaption->NPV();

    auto hw2cModel = ext::make_shared<HW2CModel>(vars.discountCurve, vars.forwardCurve);
    swaption->setPricingEngine(ext::make_shared<HW2CTreeSwaptionEngine>(hw2cModel, 80));
    const Real hw2cNpv = swaption->NPV();

    const Real tolerance = 0.35;
    if (std::fabs(hw2cNpv - oneCurveNpv) > tolerance) {
        BOOST_ERROR("single-curve limit mismatch for European swaption:\n"
                    << "TreeSwaptionEngine NPV: " << oneCurveNpv << "\n"
                    << "HW2C tree NPV:          " << hw2cNpv << "\n"
                    << "diff:                   " << (hw2cNpv - oneCurveNpv) << "\n"
                    << "tolerance:              " << tolerance);
    }
}

BOOST_AUTO_TEST_CASE(testBermudanSwaptionReactsToForwardCurveRelink) {
    BOOST_TEST_MESSAGE("Testing HW2C Bermudan swaption observer/lazy recalculation on forward "
                       "curve relink...");

    CommonVars vars;
    auto swaption = makeBermudanSwaption(vars, vars.forwardCurve);

    auto hw2cModel = ext::make_shared<HW2CModel>(vars.discountCurve, vars.forwardCurve);
    swaption->setPricingEngine(ext::make_shared<HW2CTreeSwaptionEngine>(hw2cModel, 96));

    const Real baseNpv = swaption->NPV();

    vars.forwardCurve.linkTo(flatRate(vars.today, 0.015, vars.dayCounter));
    const Real relinkedNpv = swaption->NPV();

    if (std::fabs(relinkedNpv - baseNpv) < 1.0e-8) {
        BOOST_ERROR("HW2C Bermudan swaption NPV did not react to forward curve relink:\n"
                    << "base NPV:     " << baseNpv << "\n"
                    << "relinked NPV: " << relinkedNpv);
    }

    vars.forwardCurve.linkTo(vars.discountCurve.currentLink());
    const Real singleCurveNpv = swaption->NPV();

    auto oneCurveModel = ext::make_shared<HullWhite>(vars.discountCurve);
    swaption->setPricingEngine(ext::make_shared<TreeSwaptionEngine>(oneCurveModel, 96));
    const Real oneCurveNpv = swaption->NPV();

    const Real tolerance = 0.5;
    if (std::fabs(singleCurveNpv - oneCurveNpv) > tolerance) {
        BOOST_ERROR("single-curve reset mismatch for Bermudan swaption:\n"
                    << "HW2C tree NPV:          " << singleCurveNpv << "\n"
                    << "TreeSwaptionEngine NPV: " << oneCurveNpv << "\n"
                    << "diff:                   " << (singleCurveNpv - oneCurveNpv) << "\n"
                    << "tolerance:              " << tolerance);
    }
}

BOOST_AUTO_TEST_CASE(testBermudanSwaptionSingleCurveLimitMatchesStandardHW) {
    BOOST_TEST_MESSAGE("Testing HW2C Bermudan swaption single-curve limit matches "
                       "standard HW tree pricing...");

    // This test guards against the day-counter mismatch bug in HW2CDiscretizedSwap
    // where indexEndTimes_ was computed as resetTime(Act365) + spanning(Act360)
    // instead of dayCounter.yearFraction(referenceDate, payDate).
    // Before the fix, HW2C overpriced Bermudans by ~18% (15.5 vs 13.1).

    CommonVars vars;

    // Actual365Fixed curve + Euribor(Actual360) = the exact day-counter
    // mismatch scenario that triggered the bug.
    vars.dayCounter = Actual360();
    auto curveDayCounter = Actual365Fixed();
    vars.discountCurve.linkTo(flatRate(vars.today, 0.04875825, curveDayCounter));
    vars.forwardCurve.linkTo(vars.discountCurve.currentLink());

    auto index = ext::make_shared<Euribor6M>(vars.forwardCurve);

    ext::shared_ptr<VanillaSwap> swap = MakeVanillaSwap(5 * Years, index, vars.fixedRate)
                                            .withNominal(vars.nominal)
                                            .withType(Swap::Payer);

    std::vector<Date> exerciseDates;
    for (const auto& cf : swap->fixedLeg()) {
        auto coupon = ext::dynamic_pointer_cast<Coupon>(cf);
        exerciseDates.push_back(coupon->accrualStartDate());
    }
    auto exercise = ext::make_shared<BermudanExercise>(exerciseDates);
    auto swaption = ext::make_shared<Swaption>(swap, exercise);

    // Use identical (a, sigma) to avoid calibration noise.
    Real a = 0.05, sigma = 0.006;
    Size timeSteps = 80;

    // Standard HW tree
    auto hwModel = ext::make_shared<HullWhite>(vars.discountCurve, a, sigma);
    swaption->setPricingEngine(ext::make_shared<TreeSwaptionEngine>(hwModel, timeSteps));
    const Real hwNpv = swaption->NPV();

    // HW2C tree (single-curve: same curve for discount and forward)
    auto hw2cModel = ext::make_shared<HW2CModel>(vars.discountCurve, vars.forwardCurve, a, sigma);
    swaption->setPricingEngine(ext::make_shared<HW2CTreeSwaptionEngine>(hw2cModel, timeSteps));
    const Real hw2cNpv = swaption->NPV();

    // Pre-fix difference was ~2.3; post-fix, engines agree within
    // discretization noise of explicit bond rollback vs telescoping 1-P identity.
    const Real tolerance = 0.15;
    if (std::fabs(hw2cNpv - hwNpv) > tolerance) {
        BOOST_ERROR("HW2C Bermudan swaption single-curve limit mismatch:\n"
                    << std::fixed << std::setprecision(6) << "standard HW tree NPV: " << hwNpv
                    << "\n"
                    << "HW2C tree NPV:        " << hw2cNpv << "\n"
                    << "diff:                 " << (hw2cNpv - hwNpv) << "\n"
                    << "tolerance:            " << tolerance);
    }
}

BOOST_AUTO_TEST_CASE(testBermudanSwaptionSingleCurveLimitMultipleStrikes) {
    BOOST_TEST_MESSAGE("Testing HW2C Bermudan swaption single-curve parity across strikes...");

    CommonVars vars;

    auto curveDayCounter = Actual365Fixed();
    vars.discountCurve.linkTo(flatRate(vars.today, 0.04875825, curveDayCounter));
    vars.forwardCurve.linkTo(vars.discountCurve.currentLink());

    auto index = ext::make_shared<Euribor6M>(vars.forwardCurve);

    ext::shared_ptr<VanillaSwap> parSwap =
        MakeVanillaSwap(5 * Years, index, 0.0).withNominal(vars.nominal);
    parSwap->setPricingEngine(ext::make_shared<DiscountingSwapEngine>(vars.discountCurve));
    Rate atmRate = parSwap->fairRate();

    Real a = 0.05, sigma = 0.006;
    Size timeSteps = 80;
    auto hwModel = ext::make_shared<HullWhite>(vars.discountCurve, a, sigma);
    auto hw2cModel = ext::make_shared<HW2CModel>(vars.discountCurve, vars.forwardCurve, a, sigma);

    std::vector<Real> strikeMultipliers = {0.8, 1.0, 1.2};
    const Real tolerance = 0.15;

    for (Real mult : strikeMultipliers) {
        Rate strike = mult * atmRate;
        ext::shared_ptr<VanillaSwap> swap = MakeVanillaSwap(5 * Years, index, strike)
                                                .withNominal(vars.nominal)
                                                .withType(Swap::Payer);

        std::vector<Date> exerciseDates;
        for (const auto& cf : swap->fixedLeg()) {
            auto coupon = ext::dynamic_pointer_cast<Coupon>(cf);
            exerciseDates.push_back(coupon->accrualStartDate());
        }
        auto exercise = ext::make_shared<BermudanExercise>(exerciseDates);
        auto swaption = ext::make_shared<Swaption>(swap, exercise);

        swaption->setPricingEngine(ext::make_shared<TreeSwaptionEngine>(hwModel, timeSteps));
        const Real hwNpv = swaption->NPV();

        swaption->setPricingEngine(ext::make_shared<HW2CTreeSwaptionEngine>(hw2cModel, timeSteps));
        const Real hw2cNpv = swaption->NPV();

        if (std::fabs(hw2cNpv - hwNpv) > tolerance) {
            BOOST_ERROR("HW2C single-curve mismatch at strike "
                        << std::fixed << std::setprecision(4) << strike * 100 << "%:\n"
                        << std::setprecision(6) << "standard HW tree NPV: " << hwNpv << "\n"
                        << "HW2C tree NPV:        " << hw2cNpv << "\n"
                        << "diff:                 " << (hw2cNpv - hwNpv) << "\n"
                        << "tolerance:            " << tolerance);
        }
    }
}

BOOST_AUTO_TEST_CASE(testModelAccessorsReflectConstructorInputs) {
    BOOST_TEST_MESSAGE("Testing HW2C model accessors reflect constructor inputs...");

    CommonVars vars;

    Real a = 0.07, sigma = 0.015;
    auto model = ext::make_shared<HW2CModel>(vars.discountCurve, vars.forwardCurve, a, sigma);

    if (std::fabs(model->a() - a) > 1.0e-15) {
        BOOST_ERROR("HW2CModel a() does not match constructor input:\n"
                    << "expected: " << a << "\n"
                    << "got:      " << model->a());
    }

    if (std::fabs(model->sigma() - sigma) > 1.0e-15) {
        BOOST_ERROR("HW2CModel sigma() does not match constructor input:\n"
                    << "expected: " << sigma << "\n"
                    << "got:      " << model->sigma());
    }

    BOOST_CHECK_EQUAL(model->discountTermStructure().currentLink(),
                      vars.discountCurve.currentLink());
    BOOST_CHECK_EQUAL(model->forwardTermStructure().currentLink(), vars.forwardCurve.currentLink());

    BOOST_CHECK(model->discountModel().currentLink() != nullptr);
    BOOST_CHECK(model->forwardModel().currentLink() != nullptr);

    if (std::fabs(model->discountModel()->params()[0] - a) > 1.0e-15) {
        BOOST_ERROR("discount HullWhite a does not match:\n"
                    << "expected: " << a << "\n"
                    << "got:      " << model->discountModel()->params()[0]);
    }
    if (std::fabs(model->forwardModel()->params()[1] - sigma) > 1.0e-15) {
        BOOST_ERROR("forward HullWhite sigma does not match:\n"
                    << "expected: " << sigma << "\n"
                    << "got:      " << model->forwardModel()->params()[1]);
    }
}

BOOST_AUTO_TEST_CASE(testSingleCurveConstructorSharesCurves) {
    BOOST_TEST_MESSAGE("Testing HW2C single-curve constructor shares curves...");

    CommonVars vars;

    auto model = ext::make_shared<HW2CModel>(vars.discountCurve, 0.05, 0.01);

    BOOST_CHECK_EQUAL(model->discountTermStructure().currentLink(),
                      model->forwardTermStructure().currentLink());
}

BOOST_AUTO_TEST_CASE(testModelRejectsMismatchedReferenceDates) {
    BOOST_TEST_MESSAGE("Testing HW2C model rejects mismatched reference dates...");

    CommonVars vars;

    Handle<YieldTermStructure> differentDateCurve(flatRate(vars.today + 1, 0.03, vars.dayCounter));

    BOOST_CHECK_THROW(HW2CModel(vars.discountCurve, differentDateCurve), Error);
}

BOOST_AUTO_TEST_CASE(testModelRejectsMismatchedDayCounters) {
    BOOST_TEST_MESSAGE("Testing HW2C model rejects mismatched day counters...");

    CommonVars vars;

    // vars.dayCounter is Actual360; use Actual365Fixed for mismatch
    Handle<YieldTermStructure> differentDcCurve(flatRate(vars.today, 0.03, Actual365Fixed()));

    BOOST_CHECK_THROW(HW2CModel(vars.discountCurve, differentDcCurve), Error);
}

BOOST_AUTO_TEST_CASE(testParYieldCurveSettlementRejected) {
    BOOST_TEST_MESSAGE("Testing HW2C swaption engine rejects ParYieldCurve settlement...");

    CommonVars vars;

    auto swap = makeSwap(vars, vars.forwardCurve);
    Date exerciseDate = vars.calendar.advance(swap->startDate(), 2 * Years);
    auto exercise = ext::make_shared<EuropeanExercise>(exerciseDate);
    auto swaption =
        ext::make_shared<Swaption>(swap, exercise, Settlement::Cash, Settlement::ParYieldCurve);

    auto hw2cModel = ext::make_shared<HW2CModel>(vars.discountCurve, vars.forwardCurve);
    swaption->setPricingEngine(ext::make_shared<HW2CTreeSwaptionEngine>(hw2cModel, 40));

    BOOST_CHECK_THROW(swaption->NPV(), Error);
}

BOOST_AUTO_TEST_CASE(testSwapDualCurveSpreadImpact) {
    BOOST_TEST_MESSAGE("Testing HW2C swap NPV changes when forward curve differs from "
                       "discount curve...");

    CommonVars vars;

    auto hw2cModelSame = ext::make_shared<HW2CModel>(vars.discountCurve, vars.forwardCurve);

    auto swap = makeSwap(vars, vars.forwardCurve);
    swap->setPricingEngine(ext::make_shared<HW2CTreeSwapEngine>(hw2cModelSame, 80));
    const Real singleCurveNpv = swap->NPV();

    // Higher forward curve produces different floating-leg projection
    Handle<YieldTermStructure> higherForward(flatRate(vars.today, 0.05, vars.dayCounter));
    auto hw2cModelDual = ext::make_shared<HW2CModel>(vars.discountCurve, higherForward);

    auto index = ext::make_shared<Euribor6M>(higherForward);
    ext::shared_ptr<VanillaSwap> swapDual =
        MakeVanillaSwap(5 * Years, index, vars.fixedRate).withNominal(vars.nominal);
    swapDual->setPricingEngine(ext::make_shared<HW2CTreeSwapEngine>(hw2cModelDual, 80));
    const Real dualCurveNpv = swapDual->NPV();

    if (std::fabs(dualCurveNpv - singleCurveNpv) < 1.0e-8) {
        BOOST_ERROR("HW2C swap NPV did not change when forward curve differs from "
                    "discount curve:\n"
                    << "single-curve NPV: " << singleCurveNpv << "\n"
                    << "dual-curve NPV:   " << dualCurveNpv);
    }
}

BOOST_AUTO_TEST_CASE(testSwapConvergesWithTimeSteps) {
    BOOST_TEST_MESSAGE("Testing HW2C tree swap NPV converges with increasing time steps...");

    CommonVars vars;

    auto hw2cModel = ext::make_shared<HW2CModel>(vars.discountCurve, vars.forwardCurve);
    auto swap = makeSwap(vars, vars.forwardCurve);

    swap->setPricingEngine(ext::make_shared<DiscountingSwapEngine>(vars.discountCurve));
    const Real reference = swap->NPV();

    std::vector<Size> stepCounts = {20, 40, 80, 160};
    Real previousError = QL_MAX_REAL;

    for (Size steps : stepCounts) {
        swap->setPricingEngine(ext::make_shared<HW2CTreeSwapEngine>(hw2cModel, steps));
        const Real treeNpv = swap->NPV();
        const Real error = std::fabs(treeNpv - reference);

        if (error > previousError + 0.05) {
            BOOST_ERROR("HW2C tree swap NPV does not converge with increasing time steps:\n"
                        << "steps: " << steps << "  NPV: " << treeNpv << "  error: " << error
                        << "\n"
                        << "previous error: " << previousError);
        }
        previousError = error;
    }

    const Real finalTolerance = 1.0;
    if (previousError > finalTolerance) {
        BOOST_ERROR("HW2C tree swap NPV at " << stepCounts.back()
                                             << " steps is too far from reference:\n"
                                             << "error:     " << previousError << "\n"
                                             << "tolerance: " << finalTolerance);
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

    const Real tolerance = 0.5;
    if (std::fabs(sum) > tolerance) {
        BOOST_ERROR("HW2C payer + receiver swap NPV != 0:\n"
                    << "payer NPV:    " << payerNpv << "\n"
                    << "receiver NPV: " << receiverNpv << "\n"
                    << "sum:          " << sum << "\n"
                    << "tolerance:    " << tolerance);
    }
}

BOOST_AUTO_TEST_CASE(testEuropeanSwaptionDualCurveVsDiscounting) {
    BOOST_TEST_MESSAGE("Testing HW2C European swaption NPV changes with dual-curve spread...");

    CommonVars vars;

    auto swaption = makeEuropeanSwaption(vars, vars.forwardCurve);
    auto hw2cSingle = ext::make_shared<HW2CModel>(vars.discountCurve, vars.forwardCurve);
    swaption->setPricingEngine(ext::make_shared<HW2CTreeSwaptionEngine>(hw2cSingle, 80));
    const Real singleCurveNpv = swaption->NPV();

    // Higher forward rate changes the underlying swap value
    Handle<YieldTermStructure> higherForward(flatRate(vars.today, 0.05, vars.dayCounter));

    auto swaptionDual = makeEuropeanSwaption(vars, higherForward);
    auto hw2cDual = ext::make_shared<HW2CModel>(vars.discountCurve, higherForward);
    swaptionDual->setPricingEngine(ext::make_shared<HW2CTreeSwaptionEngine>(hw2cDual, 80));
    const Real dualCurveNpv = swaptionDual->NPV();

    if (std::fabs(dualCurveNpv - singleCurveNpv) < 1.0e-6) {
        BOOST_ERROR("HW2C European swaption NPV did not change with dual-curve spread:\n"
                    << "single-curve NPV: " << singleCurveNpv << "\n"
                    << "dual-curve NPV:   " << dualCurveNpv);
    }
}

BOOST_AUTO_TEST_CASE(testBermudanSwaptionMonotonicInVolatility) {
    BOOST_TEST_MESSAGE("Testing HW2C Bermudan swaption NPV is monotonic in volatility...");

    CommonVars vars;

    auto swaption = makeBermudanSwaption(vars, vars.forwardCurve);

    Real a = 0.05;
    std::vector<Real> sigmas = {0.002, 0.006, 0.012, 0.025};

    Real previousNpv = -1.0;
    for (Real sigma : sigmas) {
        auto hw2cModel =
            ext::make_shared<HW2CModel>(vars.discountCurve, vars.forwardCurve, a, sigma);
        swaption->setPricingEngine(ext::make_shared<HW2CTreeSwaptionEngine>(hw2cModel, 80));
        const Real npv = swaption->NPV();

        if (npv < previousNpv - 0.01) {
            BOOST_ERROR("HW2C Bermudan swaption NPV not monotonic in sigma:\n"
                        << "sigma: " << sigma << "  NPV: " << npv << "\n"
                        << "previous NPV: " << previousNpv);
        }
        previousNpv = npv;
    }
}

BOOST_AUTO_TEST_CASE(testSwapReactsToDiscountCurveRelink) {
    BOOST_TEST_MESSAGE("Testing HW2C swap observer/lazy recalculation on discount curve "
                       "relink...");

    CommonVars vars;

    auto swap = makeSwap(vars, vars.forwardCurve);
    auto hw2cModel = ext::make_shared<HW2CModel>(vars.discountCurve, vars.forwardCurve);
    swap->setPricingEngine(ext::make_shared<HW2CTreeSwapEngine>(hw2cModel, 80));

    const Real baseNpv = swap->NPV();

    vars.discountCurve.linkTo(flatRate(vars.today, 0.06, vars.dayCounter));
    const Real relinkedNpv = swap->NPV();

    if (std::fabs(relinkedNpv - baseNpv) < 1.0e-8) {
        BOOST_ERROR("HW2C swap NPV did not react to discount curve relink:\n"
                    << "base NPV:     " << baseNpv << "\n"
                    << "relinked NPV: " << relinkedNpv);
    }
}

BOOST_AUTO_TEST_CASE(testModelGenerateArgumentsRebuildsInternalModels) {
    BOOST_TEST_MESSAGE("Testing HW2C model internal HullWhite instances update when parameters "
                       "change...");

    CommonVars vars;

    Real a = 0.05, sigma = 0.01;
    auto model = ext::make_shared<HW2CModel>(vars.discountCurve, vars.forwardCurve, a, sigma);

    const Real origDiscountA = model->discountModel()->params()[0];
    const Real origForwardA = model->forwardModel()->params()[0];

    // setParams triggers generateArguments() which rebuilds internal HW models
    Array params = model->params();
    params[0] = 0.15;
    model->setParams(params);

    const Real newDiscountA = model->discountModel()->params()[0];
    const Real newForwardA = model->forwardModel()->params()[0];

    if (std::fabs(newDiscountA - 0.15) > 1.0e-15) {
        BOOST_ERROR("discount HW model a not updated after setParams:\n"
                    << "expected: 0.15\n"
                    << "got:      " << newDiscountA);
    }

    if (std::fabs(newForwardA - 0.15) > 1.0e-15) {
        BOOST_ERROR("forward HW model a not updated after setParams:\n"
                    << "expected: 0.15\n"
                    << "got:      " << newForwardA);
    }

    if (std::fabs(origDiscountA - 0.15) < 1.0e-15) {
        BOOST_ERROR("original discount a was already 0.15 — test is not meaningful");
    }

    if (std::fabs(origForwardA - 0.15) < 1.0e-15) {
        BOOST_ERROR("original forward a was already 0.15 — test is not meaningful");
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
