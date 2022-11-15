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
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/pricingengines/swaption/blackswaptionengine.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <iostream>

using namespace QuantLib;
using namespace boost::unit_test_framework;

namespace hw2c_test {
    struct CommonVars {
        Date today;

        Real nominal = 10000.00;
        Rate fixedRate = 0.04;

        Volatility swaptionVola = 0.20;

        Rate discountRate = 0.05;
        Rate forwardRate = 0.03;
        DayCounter dc = Actual360();

        Handle<YieldTermStructure> discountCurve;
        Handle<YieldTermStructure> forwardCurve;

        SavedSettings backup;

        CommonVars() {
            today = Date(15, Nov, 2022);
            Settings::instance().evaluationDate() = today;

            discountCurve = Handle<YieldTermStructure>(flatRate(today, discountRate, dc));
            forwardCurve = Handle<YieldTermStructure>(flatRate(today, forwardRate, dc));
        }

        VanillaSwap makeSwap(const ext::shared_ptr<IborIndex>& index,
                             const Period& swapTenor,
                             bool atParCoupons) const {
            auto iborIndex = index->clone(forwardCurve);
            return MakeVanillaSwap(swapTenor, iborIndex, fixedRate)
                .withNominal(nominal)
                .withAtParCoupons(atParCoupons);
        }

        Swaption makeSwaption(const Period& swaptionTenor, const Period& swapTenor) const {
            auto swapIndex =
                ext::make_shared<EuriborSwapIsdaFixA>(swapTenor, forwardCurve, discountCurve);
            return MakeSwaption(swapIndex, swaptionTenor).withNominal(nominal);
        }
    };

    std::vector<ext::shared_ptr<IborIndex>> indices() {
        return {ext::make_shared<Euribor3M>(), ext::make_shared<Euribor6M>(),
                ext::make_shared<Euribor1Y>()};
    }

    std::vector<Period> swapTenors() {
        return {Period(1, Years), Period(2, Years), Period(5, Years), Period(10, Years),
                Period(20, Years)};
    }

    std::vector<Period> swaptionTenors() {
        return {Period(1, Years), Period(2, Years), Period(5, Years), Period(10, Years),
                Period(20, Years)};
    }
}

void HullWhiteWithTwoCurves::testSwapPricing() {

    BOOST_TEST_MESSAGE("Testing HullWhiteWithTwoCurves swap against discounting engine...");

    hw2c_test::CommonVars vars;

    for (const auto& index : hw2c_test::indices()) {
        for (const auto& swapTenor : hw2c_test::swapTenors()) {
            for (const auto atParCoupons : {true, false}) {
                auto swap = vars.makeSwap(index, swapTenor, atParCoupons);

                auto discountingEngine =
                    ext::make_shared<DiscountingSwapEngine>(vars.discountCurve);
                swap.setPricingEngine(discountingEngine);
                auto discountingNpv = swap.NPV();

                auto hw2cModel = ext::make_shared<HW2CModel>(vars.discountCurve, vars.forwardCurve);
                auto treeEngine = ext::make_shared<HW2CTreeSwapEngine>(hw2cModel, 40);
                swap.setPricingEngine(treeEngine);
                auto treeNpv = swap.NPV();

                auto relativeDiff = (discountingNpv - treeNpv) / vars.nominal;
                auto relativeTolerance = atParCoupons ? 1e-14 : 1e-5;
                if (std::abs(relativeDiff) > relativeTolerance) {
                    BOOST_FAIL(std::setprecision(2)
                               << std::fixed << std::boolalpha << "The npvs with index='"
                               << index->name() << "', maturity=" << swapTenor << ", atParCoupons="
                               << atParCoupons << " from the discounting engine (" << discountingNpv
                               << ") and the HW2CModel tree engine (" << treeNpv
                               << ") do not match. " << std::setprecision(4) << std::defaultfloat
                               << "The relative difference compared to the nominal is "
                               << relativeDiff << ", tolerance " << relativeTolerance << ".");
                }
            }
        }
    }
}

void HullWhiteWithTwoCurves::testEuropeanSwaptionPricing() {

    BOOST_TEST_MESSAGE(
        "Testing HullWhiteWithTwoCurves european swaption against bachelier engine...");

    hw2c_test::CommonVars vars;

    for (const auto& swapTenor : hw2c_test::swapTenors()) {
        for (const auto& swaptionTenor : hw2c_test::swaptionTenors()) {
            auto swaption = vars.makeSwaption(swaptionTenor, swapTenor);

            auto blackEngine =
                ext::make_shared<BlackSwaptionEngine>(vars.discountCurve, vars.swaptionVola);
            swaption.setPricingEngine(blackEngine);
            auto bachelierNpv = swaption.NPV();

            auto hw2cModel = ext::make_shared<HW2CModel>(vars.discountCurve, vars.forwardCurve);
            auto treeEngine = ext::make_shared<HW2CTreeSwaptionEngine>(hw2cModel, 40);
            swaption.setPricingEngine(treeEngine);
            auto treeNpv = swaption.NPV();

            BOOST_TEST_MESSAGE("" << std::boolalpha << "" << swaptionTenor << "*" << swapTenor);
            BOOST_TEST_MESSAGE("" << std::setprecision(2) << std::fixed
                                  << "\tNPV BachelierSwaptionEngine: " << bachelierNpv);
            BOOST_TEST_MESSAGE("" << std::setprecision(2) << std::fixed
                                  << "\tNPV  HW2CTreeSwaptionEngine: " << treeNpv);
        }
    }
}

test_suite* HullWhiteWithTwoCurves::suite() {
    auto* suite = BOOST_TEST_SUITE("HullWhiteWithTwoCurves tests");
    suite->add(QUANTLIB_TEST_CASE(&HullWhiteWithTwoCurves::testSwapPricing));
    suite->add(QUANTLIB_TEST_CASE(&HullWhiteWithTwoCurves::testEuropeanSwaptionPricing));
    return suite;
}
