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

#include "hullwhitewithtwocurves.hpp"
#include "utilities.hpp"
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/experimental/hullwhitewithtwocurves/model/hw2cmodel.hpp>
#include <ql/experimental/hullwhitewithtwocurves/pricingengines/swap/hw2ctreeswapengine.hpp>
#include <ql/experimental/hullwhitewithtwocurves/pricingengines/swaption/hw2ctreeswaptionengine.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/indexes/indexmanager.hpp>
#include <ql/indexes/swap/euriborswap.hpp>
#include <ql/instruments/makeswaption.hpp>
#include <ql/instruments/makevanillaswap.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/pricingengines/swaption/blackswaptionengine.hpp>
#include <ql/pricingengines/swaption/treeswaptionengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <iostream>
#include <utility>

using namespace QuantLib;
using namespace boost::unit_test_framework;

namespace hw2c_test {
    const Real atParCouponsTolerance = 1e-10;
    const Real indexedCouponsTolerance = 0.1;

    struct CommonVars {
        Date today;
        bool useAtParCoupons;

        Calendar calendar = TARGET();

        Real nominal = 10000.00;
        Rate fixedRate = 0.04;

        Volatility swaptionVola;
        VolatilityType volatilityType;

        Rate discountRate = 0.05;
        Rate forwardRate = 0.03;
        DayCounter dc = Actual360();

        Handle<YieldTermStructure> discountCurve;
        Handle<YieldTermStructure> forwardCurve;
        Handle<Quote> volatility;

        SavedSettings backup;

        CommonVars()
        : CommonVars(IborCoupon::Settings::instance().usingAtParCoupons(),
                     VolatilityType::ShiftedLognormal) {}

        CommonVars(bool atParCoupons)
        : CommonVars(atParCoupons, VolatilityType::ShiftedLognormal) {}

        CommonVars(bool atParCoupons, VolatilityType volatilityType) {
            // We need to set (and reset) the IborCoupon type here to create the same type within
            // the SwaptionHelpers as well.
            previousUseAtParCoupons_ = IborCoupon::Settings::instance().usingAtParCoupons();
            setIborCouponType(atParCoupons);

            today = Date(15, Nov, 2022);
            Settings::instance().evaluationDate() = today;

            this->volatilityType = volatilityType;
            swaptionVola = this->volatilityType == QuantLib::ShiftedLognormal ? 0.20 : 0.20 / 100.;

            discountCurve = Handle<YieldTermStructure>(flatRate(today, discountRate, dc));
            forwardCurve = Handle<YieldTermStructure>(flatRate(today, forwardRate, dc));
            volatility = Handle<Quote>(ext::make_shared<SimpleQuote>(swaptionVola));
        }

        ~CommonVars() {
            // reset the previous IborCoupun type.
            setIborCouponType(previousUseAtParCoupons_);
            IndexManager::instance().clearHistories();
        }

      private:
        bool previousUseAtParCoupons_;

        void setIborCouponType(bool atParCoupons) {
            useAtParCoupons = atParCoupons;
            if (useAtParCoupons) {
                IborCoupon::Settings::instance().createAtParCoupons();
            } else {
                IborCoupon::Settings::instance().createIndexedCoupons();
            }
        }
    };

    std::vector<ext::shared_ptr<IborIndex>> indices() {
        return {ext::make_shared<Euribor3M>(), ext::make_shared<Euribor6M>(),
                ext::make_shared<Euribor1Y>()};
    }

    std::vector<Period> swapTenors() {
        return {2 * Years, 5 * Years, 10 * Years};
    }

    std::vector<Period> swaptionTenors() {
        return {1 * Years, 2 * Years, 5 * Years};
    }

    void addFixings(ext::shared_ptr<IborIndex>& index, Date startDate, Rate fixing) {
        for (auto date = startDate - 1 * Months; date <= Settings::instance().evaluationDate();
             date++) {
            if (index->isValidFixingDate(date)) {
                index->addFixing(date, fixing);
            }
        }
    }

    std::pair<ext::shared_ptr<Swaption>, std::vector<ext::shared_ptr<SwaptionHelper>>>
    makeEuropeanSwaptionWithHelpers(const CommonVars& vars,
                                    const Period& swaptionTenor,
                                    const Period& swapTenor) {
        auto swapIndex =
            ext::make_shared<EuriborSwapIsdaFixA>(swapTenor, vars.forwardCurve, vars.discountCurve);
        ext::shared_ptr<Swaption> swaption =
            MakeSwaption(swapIndex, swaptionTenor).withNominal(vars.nominal);
        const auto& underlyingSwap = swaption->underlyingSwap();

        auto fixedLegDayCounter =
            (ext::dynamic_pointer_cast<FixedRateCoupon>)(underlyingSwap->fixedLeg()[0])
                ->dayCounter();
        auto floatingLegDayCounter =
            (ext::dynamic_pointer_cast<FloatingRateCoupon>)(underlyingSwap->floatingLeg()[0])
                ->dayCounter();

        auto swaptionHelper = ext::make_shared<SwaptionHelper>(
            swaptionTenor, swapTenor, vars.volatility, underlyingSwap->iborIndex(),
            swapIndex->fixedLegTenor(), fixedLegDayCounter, floatingLegDayCounter,
            vars.discountCurve, BlackCalibrationHelper::RelativePriceError, Null<Real>(),
            vars.nominal, vars.volatilityType);

        return {swaption, {swaptionHelper}};
    }

    std::pair<ext::shared_ptr<Swaption>, std::vector<ext::shared_ptr<SwaptionHelper>>>
    makeBermudanSwaptionWithHelpers(const CommonVars& vars,
                                    const Date& effectiveDate,
                                    ext::shared_ptr<IborIndex> index,
                                    Rate fixing,
                                    const Period& swapTenor) {
        addFixings(index, effectiveDate, fixing);
        ext::shared_ptr<VanillaSwap> swap = MakeVanillaSwap(swapTenor, index, vars.fixedRate)
                                                .withEffectiveDate(effectiveDate)
                                                .withNominal(vars.nominal);

        std::vector<Date> exerciseDates;
        for (const auto& cf : swap->fixedLeg()) {
            auto fixedRateCoupon = ext::dynamic_pointer_cast<FixedRateCoupon>(cf);
            exerciseDates.push_back(fixedRateCoupon->accrualStartDate());
        }
        auto exercise = ext::make_shared<BermudanExercise>(exerciseDates);

        auto swaption = ext::make_shared<Swaption>(swap, exercise);

        return {swaption, {}};
    }

    Size calculateTimeSteps(const CommonVars& vars,
                            const ext::shared_ptr<Swaption>& swaption,
                            Size minTimeStepsPerYear) {
        auto maturityDate = swaption->underlyingSwap()->maturityDate();
        auto timeToMaturiy =
            swaption->underlyingSwap()->fixedDayCount().yearFraction(vars.today, maturityDate);
        return timeToMaturiy * minTimeStepsPerYear;
    }

    void calibrateModel(ext::shared_ptr<HW2CModel>& model,
                        std::vector<ext::shared_ptr<SwaptionHelper>> helpers,
                        Size timeSteps) {
        if (helpers.empty()) {
            return;
        }

        for (const auto& helper : helpers) {
            auto treeEngine = ext::make_shared<HW2CTreeSwaptionEngine>(model, timeSteps);
            helper->setPricingEngine(treeEngine);
        }

        std::vector<ext::shared_ptr<CalibrationHelper>> calibrationHelper(helpers.begin(),
                                                                          helpers.end());
        LevenbergMarquardt om;
        EndCriteria endCriteria(400, 100, 1.0e-8, 1.0e-8, 1.0e-8);
        auto constraint = Constraint();
        auto weights = std::vector<Real>();
        auto fixParameters =
            helpers.size() == 1 ? std::vector<bool>{true, false} : std::vector<bool>{false, false};
        model->calibrate(calibrationHelper, om, endCriteria, constraint, weights, fixParameters);
    }
}

void HullWhiteWithTwoCurves::testSwapPricing() {
    BOOST_TEST_MESSAGE("Testing HullWhiteWithTwoCurves swap against discounting engine...");

    auto testSwapPricing = [](const ext::shared_ptr<IborIndex>& index, const Period& swapTenor,
                              bool atParCoupons) {
        hw2c_test::CommonVars vars(atParCoupons);

        auto clonedIndex = index->clone(vars.forwardCurve);
        VanillaSwap swap =
            MakeVanillaSwap(swapTenor, clonedIndex, vars.fixedRate).withNominal(vars.nominal);

        auto discountingEngine = ext::make_shared<DiscountingSwapEngine>(vars.discountCurve);
        swap.setPricingEngine(discountingEngine);
        auto discountingNpv = swap.NPV();

        auto hw2cModel = ext::make_shared<HW2CModel>(vars.discountCurve, vars.forwardCurve);
        auto treeEngine = ext::make_shared<HW2CTreeSwapEngine>(hw2cModel, 40);
        swap.setPricingEngine(treeEngine);
        auto treeNpv = swap.NPV();

        auto diff = discountingNpv - treeNpv;
        auto tolerance = vars.useAtParCoupons ? hw2c_test::atParCouponsTolerance :
                                                hw2c_test::indexedCouponsTolerance;
        if (std::abs(diff) > tolerance) {
            BOOST_ERROR(std::setprecision(2)
                        << std::fixed << std::boolalpha << "The npvs do not match.\n"
                        << "                    index: " << index->name() << "\n"
                        << "               swap tenor: " << swapTenor << "\n"
                        << "             atParCoupons: " << vars.useAtParCoupons << "\n"
                        << "    DiscountingSwapEngine: " << discountingNpv << "\n"
                        << "       HW2CTreeSwapEngine: " << treeNpv << "\n"
                        << std::setprecision(2) << std::defaultfloat
                        << "                     diff: " << diff << "\n"
                        << "                tolerance: " << tolerance);
        }
    };

    for (const auto& index : hw2c_test::indices()) {
        for (const auto& swapTenor : hw2c_test::swapTenors()) {
            for (auto atParCoupons : {true, false}) {
                testSwapPricing(index, swapTenor, atParCoupons);
            }
        }
    }
}

void HullWhiteWithTwoCurves::testEuropeanSwaptionPricing() {
    BOOST_TEST_MESSAGE("Testing HullWhiteWithTwoCurves european swaption against black swaption "
                       "engine with at par coupons...");

    auto testEuropeanSwaptionPricing = [](const Period& swaptionTenor, const Period& swapTenor,
                                          VolatilityType volatilityType, bool atParCoupons) {
        hw2c_test::CommonVars vars(atParCoupons, volatilityType);

        auto swaptionWithHelpers =
            hw2c_test::makeEuropeanSwaptionWithHelpers(vars, swaptionTenor, swapTenor);
        auto swaption = swaptionWithHelpers.first;
        auto swaptionHelpers = swaptionWithHelpers.second;
        auto timeSteps = hw2c_test::calculateTimeSteps(vars, swaption, 4);

        auto engine = vars.volatilityType == VolatilityType::ShiftedLognormal ?
                          ext::shared_ptr<PricingEngine>(
                              new BlackSwaptionEngine(vars.discountCurve, vars.swaptionVola)) :
                          ext::shared_ptr<PricingEngine>(
                              new BachelierSwaptionEngine(vars.discountCurve, vars.swaptionVola));
        swaption->setPricingEngine(engine);
        auto analyticalNpv = swaption->NPV();

        auto hw2cModel = ext::make_shared<HW2CModel>(vars.discountCurve, vars.forwardCurve);
        hw2c_test::calibrateModel(hw2cModel, swaptionHelpers, timeSteps);

        auto treeEngine = ext::make_shared<HW2CTreeSwaptionEngine>(hw2cModel, timeSteps);
        swaption->setPricingEngine(treeEngine);
        auto treeNpv = swaption->NPV();

        auto diff = analyticalNpv - treeNpv;
        auto tolerance = vars.useAtParCoupons ? hw2c_test::atParCouponsTolerance :
                                                hw2c_test::indexedCouponsTolerance;
        if (std::abs(diff) > tolerance) {
            BOOST_ERROR(std::setprecision(2)
                        << std::fixed << std::boolalpha << "The npvs do not match.\n"
                        << "                    swaption: " << swaptionTenor << "*" << swapTenor
                        << "\n"
                        << "              volatilityType: " << volatilityType << "\n"
                        << "                atParCoupons: " << vars.useAtParCoupons << "\n"
                        << "    BlackStyleSwaptionEngine: " << analyticalNpv << "\n"
                        << "      HW2CTreeSwaptionEngine: " << treeNpv << "\n"
                        << std::setprecision(2) << std::defaultfloat
                        << "                        diff: " << diff << "\n"
                        << "                   tolerance: " << tolerance);
        }
    };

    for (const auto& swaptionTenor : hw2c_test::swaptionTenors()) {
        for (const auto& swapTenor : hw2c_test::swapTenors()) {
            for (auto volatilityType : {VolatilityType::ShiftedLognormal, VolatilityType::Normal}) {
                for (auto atParCoupons : {true, false}) {
                    testEuropeanSwaptionPricing(swaptionTenor, swapTenor, volatilityType,
                                                atParCoupons);
                }
            }
        }
    }
}

void HullWhiteWithTwoCurves::testBermudanSwaptionPricing() {
    BOOST_TEST_MESSAGE(
        "Testing HullWhiteWithTwoCurves bermudan swaption against HullWhite TreeSwaptionEngine...");

    auto testBermudanSwaptionPricing = [](const ext::shared_ptr<IborIndex>& index,
                                          const Period& swapTenor, const Period& shift,
                                          bool atParCoupons) {
        hw2c_test::CommonVars vars(atParCoupons);

        auto indexWithForwardCurve = index->clone(vars.discountCurve);
        auto effectiveDate = vars.calendar.adjust(vars.today + shift);

        auto swaptionWithHelpers = hw2c_test::makeBermudanSwaptionWithHelpers(
            vars, effectiveDate, indexWithForwardCurve, vars.discountRate, swapTenor);
        auto swaption = swaptionWithHelpers.first;
        auto helpers = swaptionWithHelpers.second;
        auto timeSteps = hw2c_test::calculateTimeSteps(vars, swaption, 4);

        auto hw2cModel = ext::make_shared<HW2CModel>(vars.discountCurve, vars.discountCurve);
        auto hw2cTreeEngine = ext::make_shared<HW2CTreeSwaptionEngine>(hw2cModel, timeSteps);
        swaption->setPricingEngine(hw2cTreeEngine);
        auto hw2cNPV = swaption->NPV();

        auto hwModel = ext::make_shared<HullWhite>(vars.discountCurve);
        auto hwTreeEngine = ext::make_shared<TreeSwaptionEngine>(hwModel, timeSteps);
        swaption->setPricingEngine(hwTreeEngine);
        auto hwNPV = swaption->NPV();

        auto diff = hw2cNPV - hwNPV;
        auto tolerance = vars.useAtParCoupons ? hw2c_test::atParCouponsTolerance :
                                                2 * hw2c_test::indexedCouponsTolerance;

        if (std::abs(diff) > tolerance) {
            BOOST_ERROR(std::setprecision(2)
                        << std::fixed << std::boolalpha << "The npvs do not match.\n"
                        << "           swap start date: " << io::iso_date(effectiveDate) << "\n"
                        << "                 swapTenor: " << swapTenor << "\n"
                        << "              atParCoupons: " << vars.useAtParCoupons << "\n"
                        << "    HW2CTreeSwaptionEngine: " << hw2cNPV << "\n"
                        << "        TreeSwaptionEngine: " << hwNPV << "\n"
                        << std::setprecision(2) << std::defaultfloat
                        << "                      diff: " << diff << "\n"
                        << "                 tolerance: " << tolerance);
        }
    };

    for (const auto& index : hw2c_test::indices()) {
        for (const auto& swapTenor : hw2c_test::swapTenors()) {
            for (const auto& shift : {-16 * Months, 0 * Months, 5 * Months}) {
                for (auto atParCoupons : {true, false}) {
                    testBermudanSwaptionPricing(index, swapTenor, shift, atParCoupons);
                }
            }
        }
    }
}

test_suite* HullWhiteWithTwoCurves::suite() {
    auto* suite = BOOST_TEST_SUITE("HullWhiteWithTwoCurves tests");

    suite->add(QUANTLIB_TEST_CASE(&HullWhiteWithTwoCurves::testSwapPricing));
    suite->add(QUANTLIB_TEST_CASE(&HullWhiteWithTwoCurves::testEuropeanSwaptionPricing));
    suite->add(QUANTLIB_TEST_CASE(&HullWhiteWithTwoCurves::testBermudanSwaptionPricing));

    return suite;
}
