/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2003 Ferdinando Ametrano
 Copyright (C) 2001, 2002, 2003 Sadruddin Rejeb
 Copyright (C) 2004, 2005 StatPro Italia srl
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

#include <ql/stochasticprocess.hpp>
#include <ql/timegrid.hpp>
#include <utility>

namespace QuantLib {

    // base class

    StochasticProcess::StochasticProcess(ext::shared_ptr<discretization> disc)
    : discretization_(std::move(disc)) {}

    Size StochasticProcess::factors() const {
        return size();
    }

    Array StochasticProcess::expectation(Time t0,
                                         const Array& x0,
                                         Time dt) const {
        return apply(x0, discretization_->drift(*this, t0, x0, dt));
    }

    Matrix StochasticProcess::stdDeviation(Time t0,
                                           const Array& x0,
                                           Time dt) const {
        return discretization_->diffusion(*this, t0, x0, dt);
    }

    Matrix StochasticProcess::covariance(Time t0,
                                         const Array& x0,
                                         Time dt) const {
        return discretization_->covariance(*this, t0, x0, dt);
    }

    Array StochasticProcess::evolve(Time t0, const Array& x0,
                                    Time dt, const Array& dw) const {
        return apply(expectation(t0,x0,dt), stdDeviation(t0,x0,dt)*dw);
    }

    Array StochasticProcess::apply(const Array& x0,
                                   const Array& dx) const {
        return x0 + dx;
    }

    Time StochasticProcess::time(const Date& ) const {
        QL_FAIL("date/time conversion not supported");
    }

    void StochasticProcess::update() {
        notifyObservers();
    }


    // 1-D specialization

    StochasticProcess1D::StochasticProcess1D(ext::shared_ptr<discretization> disc)
    : discretization_(std::move(disc)) {}

    Real StochasticProcess1D::expectation(Time t0, Real x0, Time dt) const {
        return apply(x0, discretization_->drift(*this, t0, x0, dt));
    }

    Real StochasticProcess1D::stdDeviation(Time t0, Real x0, Time dt) const {
        return discretization_->diffusion(*this, t0, x0, dt);
    }

    Real StochasticProcess1D::variance(Time t0, Real x0, Time dt) const {
        return discretization_->variance(*this, t0, x0, dt);
    }

    Real StochasticProcess1D::evolve(Time t0, Real x0,
                                     Time dt, Real dw) const {
        return apply(expectation(t0,x0,dt), stdDeviation(t0,x0,dt)*dw);
    }

    Real StochasticProcess1D::apply(Real x0, Real dx) const {
        return x0 + dx;
    }

    void StochasticProcess1D::StepTable1D::evolvePath(Real x0,
                                                      const std::vector<Real>& dw,
                                                      bool antithetic,
                                                      Real* path) const {
        path[0] = x0;
        for (Size i = 0; i < dw.size(); ++i)
            path[i + 1] = evolve(i, path[i], antithetic ? -dw[i] : dw[i]);
    }

    namespace {

        class ForwardingStepTable : public StochasticProcess1D::StepTable1D {
          public:
            ForwardingStepTable(const StochasticProcess1D& process, TimeGrid grid)
            : process_(process), grid_(std::move(grid)) {}
            Real evolve(Size i, Real x0, Real dw) const override {
                return process_.evolve(grid_[i], x0, grid_.dt(i), dw);
            }
            void evolvePath(Real x0,
                            const std::vector<Real>& dw,
                            bool antithetic,
                            Real* path) const override {
                // one virtual call per step, to the process, as before step tables
                const Size steps = grid_.size() - 1;
#if defined(QL_EXTRA_SAFETY_CHECKS)
                QL_REQUIRE(dw.size() == steps,
                           dw.size() << " increments given for " << steps << " steps");
#endif
                path[0] = x0;
                for (Size i = 0; i < steps; ++i)
                    path[i + 1] = process_.evolve(grid_[i], path[i], grid_.dt(i),
                                                  antithetic ? -dw[i] : dw[i]);
            }

          private:
            const StochasticProcess1D& process_;
            TimeGrid grid_;
        };

    }

    ext::shared_ptr<const StochasticProcess1D::StepTable1D>
    StochasticProcess1D::stepTable1D(const TimeGrid& grid) const {
        return ext::make_shared<ForwardingStepTable>(*this, grid);
    }

}
