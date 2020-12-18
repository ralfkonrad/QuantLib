/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*!
 Copyright (C) 2020 Ralf Konrad Eckel

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

/* This example is related to https://github.com/lballabio/QuantLib/issues/930
 */

#include <ql/qldefines.hpp>
#ifdef BOOST_MSVC
#include <ql/auto_link.hpp>
#endif

#include <ql/experimental/callablebonds/callablebond.hpp>
#include <ql/experimental/callablebonds/treecallablebondengine.hpp>
#include <ql/instruments/callabilityschedule.hpp>
#include <ql/models/shortrate/onefactormodels/hullwhite.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/unitedstates.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <ql/time/schedule.hpp>
#include <iostream>

using namespace QuantLib;

#if defined(QL_ENABLE_SESSIONS)
namespace QuantLib {
    ThreadKey sessionId() { return 0; }
}
#endif

// Date today = Date(03, Nov, 2020);
Date today = Date(10, May, 2021);

Real faceAmount = 100.00;
Real coupon = 0.06;
Date issueDate = Date(25, May, 2016);
Date maturityDate = Date(15, May, 2026);
Frequency frequency = Semiannual;

Calendar calendar = UnitedStates();
DayCounter dayCounter = Thirty360();
BusinessDayConvention businessDayConvention = Unadjusted;
DateGeneration::Rule dateGenerationRule = DateGeneration::Backward;
Natural settlementDays = 0;
bool isEndOfMonth = false;

Schedule makeSchedule() {
    return Schedule(issueDate, maturityDate, Period(frequency), calendar, businessDayConvention,
                    businessDayConvention, dateGenerationRule, isEndOfMonth);
}

CallabilitySchedule makeCallabilitySchedule() {
    Date firstCallDate = Date(15, May, 2021);
    Date lastCallDate = Date(15, May, 2024);

    Schedule callSchedule =
        Schedule(firstCallDate, lastCallDate, Period(Annual), calendar, businessDayConvention,
                 businessDayConvention, dateGenerationRule, isEndOfMonth);
    const std::vector<Date>& callDates = callSchedule.dates();

    CallabilitySchedule callabilitySchedule;
    callabilitySchedule.push_back(ext::make_shared<Callability>(
        Bond::Price(103.00, Bond::Price::Clean), Callability::Call, Date(15, May, 2021)));
    callabilitySchedule.push_back(ext::make_shared<Callability>(
        Bond::Price(102.00, Bond::Price::Clean), Callability::Call, Date(15, May, 2022)));
    callabilitySchedule.push_back(ext::make_shared<Callability>(
        Bond::Price(101.00, Bond::Price::Clean), Callability::Call, Date(15, May, 2023)));
    callabilitySchedule.push_back(ext::make_shared<Callability>(
        Bond::Price(100.00, Bond::Price::Clean), Callability::Call, Date(15, May, 2024)));

    return callabilitySchedule;
}

ext::shared_ptr<CallableFixedRateBond> makeCallableBond() {
    Schedule schedule = makeSchedule();
    std::vector<Real> coupons = std::vector<Real>(schedule.size() - 1, coupon);
    CallabilitySchedule callabilitySchedule = makeCallabilitySchedule();

    ext::shared_ptr<CallableFixedRateBond> callableBond =
        ext::shared_ptr<CallableFixedRateBond>(new CallableFixedRateBond(
            settlementDays, faceAmount, schedule, coupons, dayCounter, businessDayConvention,
            faceAmount, issueDate, callabilitySchedule));

    return callableBond;
}

Handle<YieldTermStructure> getTermStructure() {
    return Handle<YieldTermStructure>(ext::shared_ptr<YieldTermStructure>(
        new FlatForward(Settings::instance().evaluationDate(), 0.0, Thirty360())));
}

int main(int, char*[]) {
    try {
        std::cout << std::endl;
        std::cout << "Initial project setup..." << std::endl << std::endl;

        Settings::instance().evaluationDate() = today;

        ext::shared_ptr<CallableFixedRateBond> callableBond = makeCallableBond();

        Leg cashflows = callableBond->cashflows();
        for (Size i = 0; i < cashflows.size(); i++) {
            ext::shared_ptr<CashFlow> cf = cashflows[i];
            std::cout << io::iso_date(cf->date()) << " " << cf->amount() << std::endl;
        }

        Handle<YieldTermStructure> termStructure = getTermStructure();

        ext::shared_ptr<ShortRateModel> model =
            ext::shared_ptr<ShortRateModel>(new HullWhite(termStructure, 0.01, 0.012));
        ext::shared_ptr<PricingEngine> engine = ext::shared_ptr<PricingEngine>(
            new TreeCallableFixedRateBondEngine(model, 80, termStructure));

        callableBond->setPricingEngine(engine);

        Real npv = callableBond->NPV();
        Real dirtyPrice = callableBond->dirtyPrice();
        Real cleanPrice = callableBond->cleanPrice();
        Real oas = callableBond->cleanPriceOAS(1.0 / 10000.0, termStructure, dayCounter, Compounded,
                                               frequency, today);

        std::cout << npv << std::endl;
        std::cout << dirtyPrice << std::endl;
        std::cout << cleanPrice << std::endl;
        std::cout << oas << std::endl;

        return 0;
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "unknown error" << std::endl;
        return 1;
    }
}
