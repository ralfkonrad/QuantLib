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
#include <ql/experimental/hullwhitewithtwocurves/model/hw2c.hpp>
#include <ql/time/daycounters/actual360.hpp>

using namespace QuantLib;
using namespace boost::unit_test_framework;

void HullWhiteWithTwoCurves::testSwapPricing() {

    BOOST_TEST_MESSAGE("Testing HullWhiteWithTwoCurves swap against discounting engine...");

    Date today = Settings::instance().evaluationDate();
    DayCounter dc = Actual360();

    Handle<YieldTermStructure> discountCurve(flatRate(today, 0.05, dc));
    Handle<YieldTermStructure> forwardCurve(flatRate(today, 0.05, dc));

    auto hw2c = ext::make_shared<HW2C>(discountCurve, forwardCurve);
}

test_suite* HullWhiteWithTwoCurves::suite() {
    auto* suite = BOOST_TEST_SUITE("HullWhiteWithTwoCurves tests");
    suite->add(QUANTLIB_TEST_CASE(&HullWhiteWithTwoCurves::testSwapPricing));
    return suite;
}
