/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2005 StatPro Italia srl
 Copyright (C) 2026 Ralf Konrad Eckel

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
#include <ql/methods/montecarlo/mctraits.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/processes/geometricbrownianprocess.hpp>
#include <ql/processes/ornsteinuhlenbeckprocess.hpp>
#include <ql/processes/squarerootprocess.hpp>
#include <ql/processes/stochasticprocessarray.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancecurve.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancesurface.hpp>
#include <ql/termstructures/volatility/equityfx/localvolsurface.hpp>
#include <ql/termstructures/yield/zerocurve.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/utilities/dataformatters.hpp>
#include <typeinfo>

using namespace QuantLib;
using namespace boost::unit_test_framework;

BOOST_FIXTURE_TEST_SUITE(QuantLibTests, TopLevelFixture)

BOOST_AUTO_TEST_SUITE(PathGeneratorTests)

void testSingle(const ext::shared_ptr<StochasticProcess1D>& process,
                const std::string& tag, bool brownianBridge,
                Real expected, Real antithetic) {
    typedef PseudoRandom::rsg_type rsg_type;
    typedef PathGenerator<rsg_type>::sample_type sample_type;

    BigNatural seed = 42;
    Time length = 10;
    Size timeSteps = 12;
    rsg_type rsg = PseudoRandom::make_sequence_generator(timeSteps, seed);
    PathGenerator<rsg_type> generator(process, length, timeSteps,
                                      rsg, brownianBridge);
    Size i;
    for (i=0; i<100; i++)
        generator.next();

    sample_type sample = generator.next();
    Real calculated = sample.value.back();
    Real error = std::fabs(calculated-expected);
    Real tolerance = 2.0e-8;
    if (error > tolerance) {
        BOOST_ERROR("using " << tag << " process "
                    << (brownianBridge ? "with " : "without ")
                    << "brownian bridge:\n"
                    << std::setprecision(13)
                    << "    calculated: " << calculated << "\n"
                    << "    expected:   " << expected << "\n"
                    << "    error:      " << error << "\n"
                    << "    tolerance:  " << tolerance);
    }

    sample = generator.antithetic();
    calculated = sample.value.back();
    error = std::fabs(calculated-antithetic);
    tolerance = 2.0e-7;
    if (error > tolerance) {
        BOOST_ERROR("using " << tag << " process "
                    << (brownianBridge ? "with " : "without ")
                    << "brownian bridge:\n"
                    << "antithetic sample:\n"
                    << std::setprecision(13)
                    << "    calculated: " << calculated << "\n"
                    << "    expected:   " << antithetic << "\n"
                    << "    error:      " << error << "\n"
                    << "    tolerance:  " << tolerance);
    }

}

void testMultiple(const ext::shared_ptr<StochasticProcess>& process,
                  const std::string& tag,
                  Real expected[], Real antithetic[]) {
    typedef PseudoRandom::rsg_type rsg_type;
    typedef MultiPathGenerator<rsg_type>::sample_type sample_type;

    BigNatural seed = 42;
    Time length = 10;
    Size timeSteps = 12;
    Size assets = process->size();
    rsg_type rsg = PseudoRandom::make_sequence_generator(timeSteps*assets,
                                                         seed);
    MultiPathGenerator<rsg_type> generator(process,
                                           TimeGrid(length, timeSteps),
                                           rsg, false);
    Size i, j;
    for (i=0; i<100; i++)
        generator.next();

    sample_type sample = generator.next();
    Array calculated(assets);
    Real error, tolerance = 2.0e-7;
    for (j=0; j<assets; j++)
        calculated[j] = sample.value[j].back();
    for (j=0; j<assets; j++) {
        error = std::fabs(calculated[j]-expected[j]);
        if (error > tolerance) {
            BOOST_ERROR("using " << tag << " process "
                        << "(" << io::ordinal(j+1) << " asset:)\n"
                        << std::setprecision(13)
                        << "    calculated: " << calculated[j] << "\n"
                        << "    expected:   " << expected[j] << "\n"
                        << "    error:      " << error << "\n"
                        << "    tolerance:  " << tolerance);
        }
    }

    sample = generator.antithetic();
    for (j=0; j<assets; j++)
        calculated[j] = sample.value[j].back();
    for (j=0; j<assets; j++) {
        error = std::fabs(calculated[j]-antithetic[j]);
        if (error > tolerance) {
            BOOST_ERROR("using " << tag << " process "
                        << "(" << io::ordinal(j+1) << " asset:)\n"
                        << "antithetic sample:\n"
                        << std::setprecision(13)
                        << "    calculated: " << calculated[j] << "\n"
                        << "    expected:   " << antithetic[j] << "\n"
                        << "    error:      " << error << "\n"
                        << "    tolerance:  " << tolerance);
        }
    }
}


BOOST_AUTO_TEST_CASE(testPathGenerator) {

    BOOST_TEST_MESSAGE("Testing 1-D path generation against cached values...");

    Settings::instance().evaluationDate() = Date(26,April,2005);

    Handle<Quote> x0(ext::shared_ptr<Quote>(new SimpleQuote(100.0)));
    Handle<YieldTermStructure> r(flatRate(0.05, Actual360()));
    Handle<YieldTermStructure> q(flatRate(0.02, Actual360()));
    Handle<BlackVolTermStructure> sigma(flatVol(0.20, Actual360()));
    // commented values must be used when Halley's correction is enabled
    testSingle(ext::shared_ptr<StochasticProcess1D>(
                                 new BlackScholesMertonProcess(x0,q,r,sigma)),
               "Black-Scholes", false, 26.13784357783, 467.2928561411);
                                    // 26.13784357783, 467.2928562519);
    testSingle(ext::shared_ptr<StochasticProcess1D>(
                                 new BlackScholesMertonProcess(x0,q,r,sigma)),
               "Black-Scholes", true, 60.28215549393, 202.6143139999);
                                   // 60.28215551021, 202.6143139437);

    testSingle(ext::shared_ptr<StochasticProcess1D>(
                       new GeometricBrownianMotionProcess(100.0, 0.03, 0.20)),
               "geometric Brownian", false, 27.62223714065, 483.6026514084);
                                         // 27.62223714065, 483.602651493);

    testSingle(ext::shared_ptr<StochasticProcess1D>(
                                     new OrnsteinUhlenbeckProcess(0.1, 0.20)),
               "Ornstein-Uhlenbeck", false, -0.8372003433557, 0.8372003433557);

    testSingle(ext::shared_ptr<StochasticProcess1D>(
                                 new SquareRootProcess(0.1, 0.1, 0.20, 10.0)),
               "square-root", false, 1.70608664108, 6.024200546031);
}

BOOST_AUTO_TEST_CASE(testMultiPathGenerator) {

    BOOST_TEST_MESSAGE("Testing n-D path generation against cached values...");

    Settings::instance().evaluationDate() = Date(26,April,2005);

    Handle<Quote> x0(ext::shared_ptr<Quote>(new SimpleQuote(100.0)));
    Handle<YieldTermStructure> r(flatRate(0.05, Actual360()));
    Handle<YieldTermStructure> q(flatRate(0.02, Actual360()));
    Handle<BlackVolTermStructure> sigma(flatVol(0.20, Actual360()));

    Matrix correlation(3,3);
    correlation[0][0] = 1.0; correlation[0][1] = 0.9; correlation[0][2] = 0.7;
    correlation[1][0] = 0.9; correlation[1][1] = 1.0; correlation[1][2] = 0.4;
    correlation[2][0] = 0.7; correlation[2][1] = 0.4; correlation[2][2] = 1.0;

    std::vector<ext::shared_ptr<StochasticProcess1D> > processes(3);
    ext::shared_ptr<StochasticProcess> process;

    processes[0] = ext::shared_ptr<StochasticProcess1D>(
                                 new BlackScholesMertonProcess(x0,q,r,sigma));
    processes[1] = ext::shared_ptr<StochasticProcess1D>(
                                 new BlackScholesMertonProcess(x0,q,r,sigma));
    processes[2] = ext::shared_ptr<StochasticProcess1D>(
                                 new BlackScholesMertonProcess(x0,q,r,sigma));
    process = ext::shared_ptr<StochasticProcess>(
                           new StochasticProcessArray(processes,correlation));
    // commented values must be used when Halley's correction is enabled
    Real result1[] = {
        188.2235868185,
        270.6713069569,
        113.0431145652 };
    // Real result1[] = {
    //     188.2235869273,
    //     270.6713071508,
    //     113.0431145652 };
    Real result1a[] = {
        64.89105742957,
        45.12494404804,
        108.0475146914 };
    // Real result1a[] = {
    //     64.89105739157,
    //     45.12494401537,
    //     108.0475146914 };
    testMultiple(process, "Black-Scholes", result1, result1a);

    processes[0] = ext::shared_ptr<StochasticProcess1D>(
                       new GeometricBrownianMotionProcess(100.0, 0.03, 0.20));
    processes[1] = ext::shared_ptr<StochasticProcess1D>(
                       new GeometricBrownianMotionProcess(100.0, 0.03, 0.20));
    processes[2] = ext::shared_ptr<StochasticProcess1D>(
                       new GeometricBrownianMotionProcess(100.0, 0.03, 0.20));
    process = ext::shared_ptr<StochasticProcess>(
                           new StochasticProcessArray(processes,correlation));
    Real result2[] = {
        174.8266131680,
        237.2692443633,
        119.1168555440 };
    // Real result2[] = {
    //     174.8266132344,
    //     237.2692444869,
    //     119.1168555605 };
    Real result2a[] = {
        57.69082393020,
        38.50016862915,
        116.4056510107 };
    // Real result2a[] = {
    //     57.69082387657,
    //     38.50016858691,
    //     116.4056510107 };
    testMultiple(process, "geometric Brownian", result2, result2a);

    processes[0] = ext::shared_ptr<StochasticProcess1D>(
                                     new OrnsteinUhlenbeckProcess(0.1, 0.20));
    processes[1] = ext::shared_ptr<StochasticProcess1D>(
                                     new OrnsteinUhlenbeckProcess(0.1, 0.20));
    processes[2] = ext::shared_ptr<StochasticProcess1D>(
                                     new OrnsteinUhlenbeckProcess(0.1, 0.20));
    process = ext::shared_ptr<StochasticProcess>(
                           new StochasticProcessArray(processes,correlation));
    Real result3[] = {
        0.2942058437284,
        0.5525006418386,
        0.02650931054575 };
    Real result3a[] = {
        -0.2942058437284,
        -0.5525006418386,
        -0.02650931054575 };
    testMultiple(process, "Ornstein-Uhlenbeck", result3, result3a);

    processes[0] = ext::shared_ptr<StochasticProcess1D>(
                                 new SquareRootProcess(0.1, 0.1, 0.20, 10.0));
    processes[1] = ext::shared_ptr<StochasticProcess1D>(
                                 new SquareRootProcess(0.1, 0.1, 0.20, 10.0));
    processes[2] = ext::shared_ptr<StochasticProcess1D>(
                                 new SquareRootProcess(0.1, 0.1, 0.20, 10.0));
    process = ext::shared_ptr<StochasticProcess>(
                           new StochasticProcessArray(processes,correlation));
    Real result4[] = {
        4.279510844897,
        4.943783503533,
        3.590930385958 };
    Real result4a[] = {
        2.763967737724,
        2.226487196647,
        3.503859264341 };
    testMultiple(process, "square-root", result4, result4a);
}

// overrides apply(), which evolve() calls and the exact step table inlines
class LinearApplyProcess : public BlackScholesMertonProcess {
  public:
    using BlackScholesMertonProcess::BlackScholesMertonProcess;
    Real apply(Real x0, Real dx) const override { return x0 * (1.0 + dx); }
};

// leaves the step unchanged and says so, for itself but not for its own subclasses
class OptedInProcess : public BlackScholesMertonProcess {
  public:
    using BlackScholesMertonProcess::BlackScholesMertonProcess;
    ext::shared_ptr<const StepTable1D> stepTable1D(const TimeGrid& grid) const override {
        if (typeid(*this) != typeid(OptedInProcess))
            return StochasticProcess1D::stepTable1D(grid);
        return exactStepTable(grid);
    }
};

// implements evolve() only, so that it runs the default evolvePath()
class EvolveOnlyStepTable : public StochasticProcess1D::StepTable1D {
  public:
    EvolveOnlyStepTable(const StochasticProcess1D& process, TimeGrid grid)
    : process_(process), grid_(std::move(grid)) {}
    Real evolve(Size i, Real x0, Real dw) const override {
        return process_.evolve(grid_[i], x0, grid_.dt(i), dw);
    }

  private:
    const StochasticProcess1D& process_;
    TimeGrid grid_;
};

class EvolveOnlyTableProcess : public BlackScholesMertonProcess {
  public:
    using BlackScholesMertonProcess::BlackScholesMertonProcess;
    ext::shared_ptr<const StepTable1D> stepTable1D(const TimeGrid& grid) const override {
        return ext::make_shared<EvolveOnlyStepTable>(*this, grid);
    }
};

void testStepTableAgainstEvolve(const StochasticProcess1D& process,
                                const TimeGrid& grid,
                                const std::string& tag) {
    ext::shared_ptr<const StochasticProcess1D::StepTable1D> table = process.stepTable1D(grid);
    for (Size i = 0; i < grid.size() - 1; ++i) {
        for (Real x0 : {50.0, 100.0, 175.0}) {
            for (Real dw : {-2.1, -0.3, 0.0, 0.8, 1.7}) {
                Real expected = process.evolve(grid[i], x0, grid.dt(i), dw);
                Real calculated = table->evolve(i, x0, dw);
                if (calculated != expected)
                    BOOST_ERROR("step table differs from evolve() for the "
                                << tag << " process:\n"
                                << std::setprecision(17) << "    step:       " << i << "\n"
                                << "    x0:         " << x0 << "\n"
                                << "    dw:         " << dw << "\n"
                                << "    calculated: " << calculated << "\n"
                                << "    expected:   " << expected);
            }
        }
    }

    std::vector<Real> dw(grid.size() - 1);
    for (Size i = 0; i < dw.size(); ++i)
        dw[i] = 0.9 * std::sin(1.0 + 2.3 * i);
    std::vector<Real> path(grid.size());
    for (bool antithetic : {false, true}) {
        table->evolvePath(100.0, dw, antithetic, path.data());
        if (path[0] != 100.0)
            BOOST_ERROR("step-table path for the " << tag << " process does not start at x0");
        Real expected = 100.0;
        for (Size i = 0; i < dw.size(); ++i) {
            expected = process.evolve(grid[i], expected, grid.dt(i), antithetic ? -dw[i] : dw[i]);
            if (path[i + 1] != expected)
                BOOST_ERROR("step-table path differs from evolve() for the "
                            << tag << " process" << (antithetic ? " (antithetic)" : "") << ":\n"
                            << std::setprecision(17) << "    step:       " << i << "\n"
                            << "    calculated: " << path[i + 1] << "\n"
                            << "    expected:   " << expected);
        }
    }
}

BOOST_AUTO_TEST_CASE(testStepTable) {

    BOOST_TEST_MESSAGE("Testing that step tables reproduce evolve() exactly...");

    Date today(26, April, 2005);
    Settings::instance().evaluationDate() = today;
    DayCounter dc = Actual360();

    Handle<Quote> x0(ext::make_shared<SimpleQuote>(100.0));
    std::vector<Date> dates = {today, today + 6 * Months, today + 1 * Years, today + 2 * Years};
    Handle<YieldTermStructure> r(
        ext::make_shared<ZeroCurve>(dates, std::vector<Rate>{0.010, 0.012, 0.015, 0.018}, dc));
    Handle<YieldTermStructure> q(
        ext::make_shared<ZeroCurve>(dates, std::vector<Rate>{0.030, 0.029, 0.027, 0.025}, dc));
    Handle<BlackVolTermStructure> flat(flatVol(today, 0.20, dc));
    Handle<BlackVolTermStructure> curve(ext::make_shared<BlackVarianceCurve>(
        today, std::vector<Date>(dates.begin() + 1, dates.end()),
        std::vector<Volatility>{0.25, 0.22, 0.20}, dc));

    // a smile, sigma(K) = 0.20 - 0.08 ln(K/100) on every expiry
    std::vector<Real> strikes = {40.0, 60.0, 80.0, 100.0, 120.0, 150.0, 200.0, 250.0};
    Matrix smileVols(strikes.size(), dates.size() - 1);
    for (Size i = 0; i < strikes.size(); ++i)
        for (Size j = 0; j < smileVols.columns(); ++j)
            smileVols[i][j] = 0.20 - 0.08 * std::log(strikes[i] / 100.0);
    Handle<BlackVolTermStructure> smile(ext::make_shared<BlackVarianceSurface>(
        today, NullCalendar(), std::vector<Date>(dates.begin() + 1, dates.end()), strikes,
        smileVols, dc, BlackVarianceSurface::ConstantExtrapolation,
        BlackVarianceSurface::ConstantExtrapolation));
    Handle<LocalVolTermStructure> localSmile(ext::make_shared<LocalVolSurface>(smile, r, q, x0));

    // uneven steps, all within the variance curve's last date
    std::vector<Time> mandatory = {0.1, 0.35, 1.0, 1.5};
    TimeGrid grid(mandatory.begin(), mandatory.end(), 12);

    testStepTableAgainstEvolve(BlackScholesMertonProcess(x0, q, r, flat), grid,
                               "constant-volatility");
    testStepTableAgainstEvolve(BlackScholesMertonProcess(x0, q, r, curve), grid, "variance-curve");
    testStepTableAgainstEvolve(BlackScholesProcess(x0, r, curve), grid,
                               "Black-Scholes variance-curve");
    testStepTableAgainstEvolve(GeneralizedBlackScholesProcess(x0, q, r, curve), grid,
                               "generalized Black-Scholes variance-curve");
    testStepTableAgainstEvolve(BlackProcess(x0, r, curve), grid, "Black variance-curve");
    testStepTableAgainstEvolve(GarmanKohlagenProcess(x0, q, r, curve), grid,
                               "Garman-Kohlhagen variance-curve");
    testStepTableAgainstEvolve(
        BlackScholesMertonProcess(x0, q, r, flat, ext::make_shared<EulerDiscretization>(), true),
        grid, "Euler-discretized");
    // two of the x0 values are off spot, so a table that froze the local volatility at spot fails
    testStepTableAgainstEvolve(BlackScholesMertonProcess(x0, q, r, smile), grid, "smile");
    testStepTableAgainstEvolve(GeneralizedBlackScholesProcess(x0, q, r, smile, localSmile), grid,
                               "external local-volatility");
    testStepTableAgainstEvolve(OptedInProcess(x0, q, r, curve), grid, "opted-in subclass");
    testStepTableAgainstEvolve(EvolveOnlyTableProcess(x0, q, r, curve), grid, "default-evolvePath");

    LinearApplyProcess linear(x0, q, r, curve);
    testStepTableAgainstEvolve(linear, grid, "linear-apply subclass");
    // the subclass would fail the check above if its table ignored the override
    BlackScholesMertonProcess plain(x0, q, r, curve);
    if (linear.evolve(grid[0], 100.0, grid.dt(0), 0.8) ==
        plain.evolve(grid[0], 100.0, grid.dt(0), 0.8))
        BOOST_ERROR("the linear-apply subclass does not change the step");
}

BOOST_AUTO_TEST_CASE(testPathGeneratorObservesProcess) {

    BOOST_TEST_MESSAGE("Testing that a path generator follows changes to its process...");

    typedef PseudoRandom::rsg_type rsg_type;

    Date today(26, April, 2005);
    Settings::instance().evaluationDate() = today;
    DayCounter dc = Actual360();

    auto volatility = ext::make_shared<SimpleQuote>(0.20);
    auto process = ext::make_shared<BlackScholesMertonProcess>(
        Handle<Quote>(ext::make_shared<SimpleQuote>(100.0)),
        Handle<YieldTermStructure>(flatRate(today, 0.02, dc)),
        Handle<YieldTermStructure>(flatRate(today, 0.05, dc)),
        Handle<BlackVolTermStructure>(flatVol(today, volatility, dc)));

    TimeGrid grid(1.0, 12);
    BigNatural seed = 42;
    PathGenerator<rsg_type> generator(
        process, grid, PseudoRandom::make_sequence_generator(grid.size() - 1, seed), false);
    rsg_type replica = PseudoRandom::make_sequence_generator(grid.size() - 1, seed);

    for (Volatility sigma : {0.20, 0.35}) {
        volatility->setValue(sigma);
        const Path& path = generator.next().value;
        const std::vector<Real>& dw = replica.nextSequence().value;
        Real expected = process->x0();
        for (Size i = 0; i < grid.size() - 1; ++i) {
            expected = process->evolve(grid[i], expected, grid.dt(i), dw[i]);
            if (path[i + 1] != expected)
                BOOST_ERROR("path differs from evolve() at volatility "
                            << sigma << ":\n"
                            << std::setprecision(17) << "    step:       " << i << "\n"
                            << "    calculated: " << path[i + 1] << "\n"
                            << "    expected:   " << expected);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
