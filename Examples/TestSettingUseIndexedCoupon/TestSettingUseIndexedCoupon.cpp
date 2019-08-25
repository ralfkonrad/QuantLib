/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*!
 Copyright (C) 2008 Allen Kuo
 Copyright (C) 2019 Ralf Konrad Eckel

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

/* This example sets up a callable fixed rate bond with a Hull White pricing
   engine and compares to Bloomberg's Hull White price/yield calculations.
*/

#include <ql/qldefines.hpp>
#ifdef BOOST_MSVC
#include <ql/auto_link.hpp>
#endif

#include <ql/indexes/ibor/euribor.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/instruments/vanillaswap.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <boost/timer.hpp>
#include <iomanip>
#include <iostream>
#include <vector>

// This makes it easier to use array literals (alas, no std::vector literals)
#define LENGTH(a) (sizeof(a) / sizeof(a[0]))

using namespace QuantLib;

#if defined(QL_ENABLE_SESSIONS)
namespace QuantLib {
    Integer sessionId() { return 0; }
}
#endif

namespace {

    ext::shared_ptr<YieldTermStructure>
    flatRate(const Date& today, const ext::shared_ptr<Quote>& forward, const DayCounter& dc) {
        return ext::shared_ptr<YieldTermStructure>(
            new FlatForward(today, Handle<Quote>(forward), dc));
    }

    ext::shared_ptr<YieldTermStructure>
    flatRate(const Date& today, Rate forward, const DayCounter& dc) {
        return flatRate(today, ext::shared_ptr<Quote>(new SimpleQuote(forward)), dc);
    }

    struct CommonVars {
        // global data
        Date today, settlement;
        VanillaSwap::Type type;
        Real nominal;
        Calendar calendar;
        BusinessDayConvention fixedConvention, floatingConvention;
        Frequency fixedFrequency, floatingFrequency;
        DayCounter fixedDayCount;
        ext::shared_ptr<IborIndex> index;
        Natural settlementDays;
        RelinkableHandle<YieldTermStructure> termStructure;

        // cleanup
        SavedSettings backup;

        // utilities
        ext::shared_ptr<VanillaSwap>
        makeSwap(Integer length, Rate fixedRate, Spread floatingSpread) {
            Date maturity = calendar.advance(settlement, length, Years, floatingConvention);
            Schedule fixedSchedule(settlement, maturity, Period(fixedFrequency), calendar,
                                   fixedConvention, fixedConvention, DateGeneration::Forward,
                                   false);
            Schedule floatSchedule(settlement, maturity, Period(floatingFrequency), calendar,
                                   floatingConvention, floatingConvention, DateGeneration::Forward,
                                   false);
            ext::shared_ptr<VanillaSwap> swap(
                new VanillaSwap(type, nominal, fixedSchedule, fixedRate, fixedDayCount,
                                floatSchedule, index, floatingSpread, index->dayCounter()));
            swap->setPricingEngine(
                ext::shared_ptr<PricingEngine>(new DiscountingSwapEngine(termStructure)));
            return swap;
        }

        CommonVars() {
            type = VanillaSwap::Payer;
            settlementDays = 2;
            nominal = 100.0;
            fixedConvention = Unadjusted;
            floatingConvention = ModifiedFollowing;
            fixedFrequency = Annual;
            floatingFrequency = Semiannual;
            fixedDayCount = Thirty360();
            index =
                ext::shared_ptr<IborIndex>(new Euribor(Period(floatingFrequency), termStructure));
            calendar = index->fixingCalendar();
            today = calendar.adjust(Settings::instance().evaluationDate());
            settlement = calendar.advance(today, settlementDays, Days);
            termStructure.linkTo(flatRate(settlement, 0.01, Actual365Fixed()));
        }
    };
}


int main(int, char*[]) {
    try {

        boost::timer timer;

        Date today = Date(25, August, 2019);
        Settings::instance().evaluationDate() = today;

        CommonVars vars;

        Size tries = 1000;
        Integer lengths[] = {1, 2, 5, 10, 20};
        Rate rates[] = {0.0, 1.0, 2.0, 3.0, 4.0};
        Spread spreads[] = {-0.001, -0.01, 0.0, 0.01, 0.001};

        Size total = tries * LENGTH(lengths) * LENGTH(rates) * LENGTH(spreads);

        ext::shared_ptr<VanillaSwap> swap;
        for (Size i = 0; i < tries; i++) {
            for (Size j = 0; j < LENGTH(lengths); j++) {
                for (Size k = 0; k < LENGTH(rates); k++) {
                    for (Size l = 0; l < LENGTH(spreads); l++) {
                        ext::shared_ptr<VanillaSwap> swap =
                            vars.makeSwap(lengths[j], rates[k], spreads[l]);
                        swap->NPV();
                    }
                }
            }
        }

        double seconds = timer.elapsed();
        Integer hours = int(seconds / 3600);
        seconds -= hours * 3600;
        Integer minutes = int(seconds / 60);
        seconds -= minutes * 60;
        std::cout << total << " new created swaps completed in ";
        if (hours > 0)
            std::cout << hours << " h ";
        if (hours > 0 || minutes > 0)
            std::cout << minutes << " m ";
        std::cout << std::fixed << std::setprecision(4) << seconds << " s" << std::endl;

        timer.restart();

        swap = vars.makeSwap(5, 0.025, 0.001);
        for (Size i = 0; i < total; i++) {
            swap->NPV();
            swap->recalculate();
        }

        seconds = timer.elapsed();
        hours = int(seconds / 3600);
        seconds -= hours * 3600;
        minutes = int(seconds / 60);
        seconds -= minutes * 60;
        std::cout << "Recalculated swap " << total << " times completed in ";
        if (hours > 0)
            std::cout << hours << " h ";
        if (hours > 0 || minutes > 0)
            std::cout << minutes << " m ";
        std::cout << std::fixed << std::setprecision(4) << seconds << " s\n" << std::endl;

        return 0;

    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "unknown error" << std::endl;
        return 1;
    }
}
