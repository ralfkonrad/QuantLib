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
#include <ql/indexes/swap/euriborswap.hpp>
#include <ql/instruments/makeswaption.hpp>
#include <ql/instruments/makevanillaswap.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/pricingengines/swaption/blackswaptionengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <iostream>
#include <utility>

using namespace QuantLib;
using namespace boost::unit_test_framework;

namespace hw2c_test {
    struct CommonVars {
        Date today;
        bool useAtParCoupons;

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
        return {Period(2, Years), Period(5, Years), Period(10, Years)};
    }

    std::vector<Period> swaptionTenors() {
        return {Period(1, Years), Period(2, Years), Period(5, Years)};
    }

    ext::shared_ptr<VanillaSwap> makeSwap(const CommonVars& vars,
                                          const ext::shared_ptr<IborIndex>& index,
                                          const Period& swapTenor) {
        auto iborIndex = index->clone(vars.forwardCurve);
        return MakeVanillaSwap(swapTenor, iborIndex, vars.fixedRate).withNominal(vars.nominal);
    }

    std::pair<ext::shared_ptr<Swaption>, std::vector<ext::shared_ptr<SwaptionHelper>>>
    makeSwaptionWithHelpers(const CommonVars& vars,
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

    void testSwapPricingAgainstDiscountingEngine(bool atParCoupons, Real relativeTolerance) {
        hw2c_test::CommonVars vars(atParCoupons);

        for (const auto& index : hw2c_test::indices()) {
            for (const auto& swapTenor : hw2c_test::swapTenors()) {
                auto swap = hw2c_test::makeSwap(vars, index, swapTenor);

                auto discountingEngine =
                    ext::make_shared<DiscountingSwapEngine>(vars.discountCurve);
                swap->setPricingEngine(discountingEngine);
                auto discountingNpv = swap->NPV();

                auto hw2cModel = ext::make_shared<HW2CModel>(vars.discountCurve, vars.forwardCurve);
                auto treeEngine = ext::make_shared<HW2CTreeSwapEngine>(hw2cModel, 40);
                swap->setPricingEngine(treeEngine);
                auto treeNpv = swap->NPV();

                auto diff = (discountingNpv - treeNpv) / vars.nominal;
                if (std::abs(diff) > relativeTolerance) {
                    BOOST_ERROR("" << std::setprecision(2) << std::fixed << std::boolalpha
                                   << "The npvs with index='" << index->name() << "', maturity="
                                   << swapTenor << ", atParCoupons=" << vars.useAtParCoupons
                                   << " do not match.\n"
                                   << "       discounting engine: " << discountingNpv << "\n"
                                   << "    HW2CModel tree engine: " << treeNpv << "\n"
                                   << std::setprecision(2) << std::defaultfloat
                                   << " diff relative to nominal: " << diff << "\n"
                                   << "                tolerance: " << relativeTolerance);
                }
            }
        }
    }

    void testEuropeanSwaptionAgainstAnalyticalEngine(VolatilityType volatilityType,
                                                     bool atParCoupons,
                                                     Real relativeTolerance) {
        hw2c_test::CommonVars vars(atParCoupons, volatilityType);

        for (const auto& swapTenor : hw2c_test::swapTenors()) {
            for (const auto& swaptionTenor : hw2c_test::swaptionTenors()) {
                auto swaptionWithHelpers =
                    hw2c_test::makeSwaptionWithHelpers(vars, swaptionTenor, swapTenor);
                auto swaption = swaptionWithHelpers.first;
                auto swaptionHelpers = swaptionWithHelpers.second;
                auto timeSteps = hw2c_test::calculateTimeSteps(vars, swaption, 4);

                auto engine =
                    vars.volatilityType == VolatilityType::ShiftedLognormal ?
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

                auto relativeDiff = (analyticalNpv - treeNpv) / vars.nominal;
                if (std::abs(relativeDiff) > relativeTolerance) {
                    BOOST_ERROR("" << std::setprecision(2) << std::fixed << std::boolalpha
                                   << "The npvs for " << swaptionTenor << "*" << swapTenor
                                   << " swaption, "
                                   << "atParCoupons=" << vars.useAtParCoupons << " do not match.\n"
                                   << "   NPV analytical SwaptionEngine: " << analyticalNpv << "\n"
                                   << "     NPV  HW2CTreeSwaptionEngine: " << treeNpv << "\n"
                                   << std::setprecision(2) << std::defaultfloat
                                   << "        diff relative to nominal: " << relativeDiff << "\n"
                                   << "                       tolerance: " << relativeTolerance);
                }
            }
        }
    }
}

void HullWhiteWithTwoCurves::testSwapPricingAgainstDiscountingEngineWithAtParCoupons() {
    BOOST_TEST_MESSAGE("Testing HullWhiteWithTwoCurves swap with at par coupons...");
    hw2c_test::testSwapPricingAgainstDiscountingEngine(true, 1e-14);
}

void HullWhiteWithTwoCurves::testSwapPricingAgainstDiscountingEngineWithIndexedCoupons() {
    BOOST_TEST_MESSAGE("Testing HullWhiteWithTwoCurves swap with indexed coupons...");
    hw2c_test::testSwapPricingAgainstDiscountingEngine(false, 1e-5);
}

void HullWhiteWithTwoCurves::testEuropeanSwaptionPricingAgainstBlackEngineWithAtParCoupons() {
    BOOST_TEST_MESSAGE("Testing HullWhiteWithTwoCurves european swaption against black swaption "
                       "engine with at par coupons...");
    hw2c_test::testEuropeanSwaptionAgainstAnalyticalEngine(VolatilityType::ShiftedLognormal, true,
                                                           1e-14);
}

void HullWhiteWithTwoCurves::testEuropeanSwaptionPricingAgainstBlackEngineWithIndexedCoupons() {
    BOOST_TEST_MESSAGE("Testing HullWhiteWithTwoCurves european swaption against black swaption "
                       "engine with at par coupons...");
    hw2c_test::testEuropeanSwaptionAgainstAnalyticalEngine(VolatilityType::ShiftedLognormal, false,
                                                           1e-5);
}

void HullWhiteWithTwoCurves::testEuropeanSwaptionPricingAgainstBachelierEngineWithAtParCoupons() {
    BOOST_TEST_MESSAGE("Testing HullWhiteWithTwoCurves european swaption against bachelier "
                       "swaption engine with at par coupons...");
    hw2c_test::testEuropeanSwaptionAgainstAnalyticalEngine(VolatilityType::Normal, true, 1e-14);
}

void HullWhiteWithTwoCurves::testEuropeanSwaptionPricingAgainstBachelierEngineWithIndexedCoupons() {
    BOOST_TEST_MESSAGE("Testing HullWhiteWithTwoCurves european swaption against bachelier "
                       "swaption engine with at par coupons...");
    hw2c_test::testEuropeanSwaptionAgainstAnalyticalEngine(VolatilityType::Normal, false, 1e-5);
}

test_suite* HullWhiteWithTwoCurves::suite() {
    auto* suite = BOOST_TEST_SUITE("HullWhiteWithTwoCurves tests");

    suite->add(QUANTLIB_TEST_CASE(
        &HullWhiteWithTwoCurves::testSwapPricingAgainstDiscountingEngineWithAtParCoupons));
    suite->add(QUANTLIB_TEST_CASE(
        &HullWhiteWithTwoCurves::testSwapPricingAgainstDiscountingEngineWithIndexedCoupons));

    suite->add(QUANTLIB_TEST_CASE(
        &HullWhiteWithTwoCurves::testEuropeanSwaptionPricingAgainstBlackEngineWithAtParCoupons));
    suite->add(QUANTLIB_TEST_CASE(
        &HullWhiteWithTwoCurves::testEuropeanSwaptionPricingAgainstBlackEngineWithIndexedCoupons));

    suite->add(
        QUANTLIB_TEST_CASE(&HullWhiteWithTwoCurves::
                               testEuropeanSwaptionPricingAgainstBachelierEngineWithAtParCoupons));
    suite->add(QUANTLIB_TEST_CASE(
        &HullWhiteWithTwoCurves::
            testEuropeanSwaptionPricingAgainstBachelierEngineWithIndexedCoupons));

    return suite;
}
