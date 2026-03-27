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

/*! \file hw2cdiscretizedasset.hpp
    \brief Dual-lattice initialization interface for two-curve
           Hull-White discretized assets
*/

#ifndef quantlib_hw2c_discretized_asset_hpp
#define quantlib_hw2c_discretized_asset_hpp

#include <ql/methods/lattices/lattice.hpp>

namespace QuantLib {

    //! Interface for discretized assets requiring dual-lattice initialization.
    /*! Concrete discretized assets in the two-curve Hull-White
        framework inherit from both DiscretizedAsset (or
        DiscretizedOption) and this interface.  The additional
        contract requires that the asset can be initialized with two
        separate lattices: one for discounting and one for forward-rate
        projection.

        \ingroup shortrate
    */
    class HW2CDiscretizedAsset {
      public:
        virtual ~HW2CDiscretizedAsset() = default;

        //! Returns the lattice used for discounting.
        virtual const ext::shared_ptr<Lattice>& discountMethod() const = 0;
        //! Returns the lattice used for forward-rate projection.
        virtual const ext::shared_ptr<Lattice>& forwardMethod() const = 0;

        //! Initializes the asset with dual lattices and a starting time.
        virtual void initialize(const ext::shared_ptr<Lattice>& discountMethod,
                                const ext::shared_ptr<Lattice>& forwardMethod,
                                Time t) = 0;
    };
}

#endif // quantlib_hw2c_discretized_asset_hpp
