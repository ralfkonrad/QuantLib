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

/*! \file hw2cmodel.hpp
    \brief
*/

#ifndef quantlib_hw2c_model_hpp
#define quantlib_hw2c_model_hpp

#include <ql/models/model.hpp>
#include <ql/models/shortrate/onefactormodels/hullwhite.hpp>

namespace QuantLib {
    class HW2CModel : public CalibratedModel {
      public:
        HW2CModel(const Handle<YieldTermStructure>& discountTermStructure,
                  const Handle<YieldTermStructure>& forwardTermStructure,
                  Real a = 0.1,
                  Real sigma = 0.01);

        Real a() const { return a_(0.0); }
        Real sigma() const { return sigma_(0.0); }

        const Handle<YieldTermStructure>& discountTermStructure() const {
            return discountTermStructure_;
        }
        const Handle<YieldTermStructure>& forwardTermStructure() const {
            return forwardTermStructure_;
        }

        Handle<HullWhite> discountModel() const { return discountModel_; }
        Handle<HullWhite> forwardModel() const { return forwardModel_; }

        ext::shared_ptr<Lattice> discountTree(const TimeGrid& timeGrid) const;
        ext::shared_ptr<Lattice> forwardTree(const TimeGrid& timeGrid) const;

      protected:
        void generateArguments() override;

      private:
        Parameter& a_;
        Parameter& sigma_;

        Handle<YieldTermStructure> discountTermStructure_;
        Handle<YieldTermStructure> forwardTermStructure_;

        RelinkableHandle<HullWhite> discountModel_;
        RelinkableHandle<HullWhite> forwardModel_;
    };
}


#endif // quantlib_hw2c_model_hpp
