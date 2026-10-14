/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2002, 2003 Ferdinando Ametrano
 Copyright (C) 2000, 2001, 2002, 2003 RiskMap srl
 Copyright (C) 2003, 2004, 2005, 2006 StatPro Italia srl
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

/*! \file pathgenerator.hpp
    \brief Generates random paths using a sequence generator
*/

#ifndef quantlib_montecarlo_path_generator_hpp
#define quantlib_montecarlo_path_generator_hpp

#include <ql/methods/montecarlo/brownianbridge.hpp>
#include <ql/stochasticprocess.hpp>
#include <utility>

namespace QuantLib {
    class StochasticProcess;
    class StochasticProcess1D;
    //! Generates random paths using a sequence generator
    /*! Generates random paths with drift(S,t) and variance(S,t)
        using a gaussian sequence generator

        The paths follow the process as of its last notification,
        through the step table it returns. After notifications were
        dropped by ObservableSettings::disableUpdates(false), call
        update() on the generator to see the current market.

        \ingroup mcarlo

        \test the generated paths are checked against cached results
    */
    template <class GSG>
    class PathGenerator : public Observer {
      public:
        typedef Sample<Path> sample_type;
        // constructors
        PathGenerator(const ext::shared_ptr<StochasticProcess>&,
                      Time length,
                      Size timeSteps,
                      GSG generator,
                      bool brownianBridge);
        PathGenerator(const ext::shared_ptr<StochasticProcess>&,
                      TimeGrid timeGrid,
                      GSG generator,
                      bool brownianBridge);
        //! \name inspectors
        //@{
        const sample_type& next() const;
        const sample_type& antithetic() const;
        Size size() const { return dimension_; }
        const TimeGrid& timeGrid() const { return timeGrid_; }
        //@}
        //! \name Observer interface
        //@{
        void update() override { table_.reset(); }
        //@}
      private:
        const sample_type& next(bool antithetic) const;
        bool brownianBridge_;
        GSG generator_;
        Size dimension_;
        TimeGrid timeGrid_;
        ext::shared_ptr<StochasticProcess1D> process_;
        mutable ext::shared_ptr<const StochasticProcess1D::StepTable1D> table_;
        mutable sample_type next_;
        mutable std::vector<Real> temp_;
        BrownianBridge bb_;
    };


    // template definitions

    template <class GSG>
    PathGenerator<GSG>::PathGenerator(const ext::shared_ptr<StochasticProcess>& process,
                                      Time length,
                                      Size timeSteps,
                                      GSG generator,
                                      bool brownianBridge)
    : brownianBridge_(brownianBridge), generator_(std::move(generator)),
      dimension_(generator_.dimension()), timeGrid_(length, timeSteps),
      process_(ext::dynamic_pointer_cast<StochasticProcess1D>(process)),
      next_(Path(timeGrid_), 1.0), temp_(dimension_), bb_(timeGrid_) {
        QL_REQUIRE(dimension_==timeSteps,
                   "sequence generator dimensionality (" << dimension_
                   << ") != timeSteps (" << timeSteps << ")");
        registerWith(process_);
    }

    template <class GSG>
    PathGenerator<GSG>::PathGenerator(const ext::shared_ptr<StochasticProcess>& process,
                                      TimeGrid timeGrid,
                                      GSG generator,
                                      bool brownianBridge)
    : brownianBridge_(brownianBridge), generator_(std::move(generator)),
      dimension_(generator_.dimension()), timeGrid_(std::move(timeGrid)),
      process_(ext::dynamic_pointer_cast<StochasticProcess1D>(process)),
      next_(Path(timeGrid_), 1.0), temp_(dimension_), bb_(timeGrid_) {
        QL_REQUIRE(dimension_==timeGrid_.size()-1,
                   "sequence generator dimensionality (" << dimension_
                   << ") != timeSteps (" << timeGrid_.size()-1 << ")");
        registerWith(process_);
    }

    template <class GSG>
    const typename PathGenerator<GSG>::sample_type&
    PathGenerator<GSG>::next() const {
        return next(false);
    }

    template <class GSG>
    const typename PathGenerator<GSG>::sample_type&
    PathGenerator<GSG>::antithetic() const {
        return next(true);
    }

    template <class GSG>
    const typename PathGenerator<GSG>::sample_type&
    PathGenerator<GSG>::next(bool antithetic) const {

        const auto& sequence_ =
            antithetic ? generator_.lastSequence()
                       : generator_.nextSequence();

        if (brownianBridge_) {
            bb_.transform(sequence_.value.begin(),
                          sequence_.value.end(),
                          temp_.begin());
        }
        const auto& increments =
            brownianBridge_ ? temp_ : sequence_.value;

        next_.weight = sequence_.weight;

        // built on the first path rather than in the constructor, so
        // that a handle linked after construction is seen
        if (!table_)
            table_ = process_->stepTable1D(timeGrid_);
        // a notification during the path drops only the member
        const auto table = table_;

        Path& path = next_.value;
        table->evolvePath(process_->x0(), increments, antithetic, &path.front());

        return next_;
    }

}


#endif
