/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2022, 2026 Ralf Konrad Eckel

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

/*! \file hw2ctreeswaptionengine.hpp
    \brief Two-curve Hull-White tree engine for swaptions
*/

#ifndef quantlib_hw2c_tree_swaption_engine_hpp
#define quantlib_hw2c_tree_swaption_engine_hpp

#include <ql/experimental/hullwhitewithtwocurves/model/hw2cmodel.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/pricingengines/genericmodelengine.hpp>

namespace QuantLib {

    //! Two-curve Hull-White tree pricing engine for swaptions.
    /*! Prices a Swaption (European or Bermudan) by building dual
        trinomial trees from an HW2CModel and performing backward
        induction through an HW2CDiscretizedSwaption.  The forward
        tree drives rate projection while the discount tree drives
        present-value discounting.

        \warning Cash-settled swaptions using the par-yield curve
                 method are not supported.

        \ingroup swaptionengines
    */
    class HW2CTreeSwaptionEngine
    : public GenericModelEngine<HW2CModel, Swaption::arguments, Swaption::results> {

      public:
        HW2CTreeSwaptionEngine(Handle<HW2CModel> model, Size timeSteps);
        HW2CTreeSwaptionEngine(const ext::shared_ptr<HW2CModel>& model, Size timeSteps);

        void calculate() const override;

      private:
        Size timeSteps_;
    };
}

#endif // quantlib_hw2c_tree_swaption_engine_hpp
