/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2022, 2026 Ralf Konrad Eckel

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
    \brief Two-curve Hull-White model
*/

#ifndef quantlib_hw2c_model_hpp
#define quantlib_hw2c_model_hpp

#include <ql/models/shortrate/onefactormodels/hullwhite.hpp>

namespace QuantLib {

    //! Two-curve Hull-White model.
    /*! This class implements a Hull-White model variant that supports
        separate forward and discount curves while sharing a common
        mean-reversion speed \f$ a \f$ and volatility \f$ \sigma \f$
        across both curves.

        Mathematically, this is a one-factor model with a single
        stochastic driver \f$ x_t \f$ and two deterministic fitting
        functions:
        \f[
            r_t^{\text{disc}} = x_t + \varphi_{\text{disc}}(t), \qquad
            r_t^{\text{fwd}}  = x_t + \varphi_{\text{fwd}}(t)
        \f]
        where \f$ x_t \f$ follows an Ornstein-Uhlenbeck process.

        The inherited OneFactorAffineModel interface (dynamics(),
        tree(), A(), B()) is fitted to the discount curve.

        Internally, the model also maintains two standard single-factor
        HullWhite model instances — one fitted to the discount curve
        and one fitted to the forward curve — which are rebuilt
        whenever the calibration parameters change.

        The model is intended for pricing interest-rate derivatives
        under a dual-curve framework where rates are projected from a
        forward curve (e.g., EURIBOR) and cash flows are discounted
        using a separate curve (e.g., OIS/ESTR).

        \ingroup shortrate
    */
    class HW2CModel : public OneFactorAffineModel, public TermStructureConsistentModel {
      public:
        /*! Constructs a two-curve Hull-White model.
            \param discountTermStructure the term structure used for
                   discounting cash flows.
            \param forwardTermStructure  the term structure used for
                   projecting forward rates.
            \param a      mean-reversion speed (shared across both
                   curves); defaults to 0.1.
            \param sigma  volatility (shared across both curves);
                   defaults to 0.01.

            \pre The reference dates and day counters of both term
                 structures must match.
        */
        HW2CModel(const Handle<YieldTermStructure>& discountTermStructure,
                  const Handle<YieldTermStructure>& forwardTermStructure,
                  Real a = 0.1,
                  Real sigma = 0.01);

        /*! Constructs a single-curve Hull-White model.

            This is a convenience constructor that uses the same term
            structure for both discounting and forward-rate projection.

            \param termStructure the term structure used for both
                   discounting and projecting forward rates.
            \param a      mean-reversion speed; defaults to 0.1.
            \param sigma  volatility; defaults to 0.01.
        */
        HW2CModel(const Handle<YieldTermStructure>& termStructure, Real a = 0.1, Real sigma = 0.01);

        //! Returns the mean-reversion speed.
        Real a() const { return a_(0.0); }
        //! Returns the volatility.
        Real sigma() const { return sigma_(0.0); }

        //! \name OneFactorModel interface
        //@{
        //! Returns the short-rate dynamics fitted to the discount curve.
        ext::shared_ptr<ShortRateDynamics> dynamics() const override;
        //@}

        //! \name ShortRateModel interface
        //@{
        //! Builds a trinomial tree fitted to the discount curve.
        ext::shared_ptr<Lattice> tree(const TimeGrid& grid) const override;
        //@}

        //! \name AffineModel interface
        //@{
        Real discountBondOption(Option::Type type,
                                Real strike,
                                Time maturity,
                                Time bondMaturity) const override;
        Real discountBondOption(Option::Type type,
                                Real strike,
                                Time maturity,
                                Time bondStart,
                                Time bondMaturity) const override;
        //@}

        //! Returns the discount term structure handle.
        const Handle<YieldTermStructure>& discountTermStructure() const {
            return discountTermStructure_;
        }
        //! Returns the forward term structure handle.
        const Handle<YieldTermStructure>& forwardTermStructure() const {
            return forwardTermStructure_;
        }

        //! Returns the internal Hull-White model fitted to the discount curve.
        const Handle<HullWhite>& discountModel() const { return discountModel_; }
        //! Returns the internal Hull-White model fitted to the forward curve.
        const Handle<HullWhite>& forwardModel() const { return forwardModel_; }

        //! Builds a trinomial tree from the forward model for the given time grid.
        ext::shared_ptr<Lattice> forwardTree(const TimeGrid& timeGrid) const;

      protected:
        //! \name OneFactorAffineModel interface
        //@{
        /*! Discount-curve affine factor \f$ A(t,T) \f$ such that
            \f$ P_{\text{disc}}(t,T,r) = A(t,T)\,e^{-B(t,T)\,r} \f$.
        */
        Real A(Time t, Time T) const override;
        /*! Mean-reversion factor \f$ B(t,T) = \frac{1-e^{-a(T-t)}}{a} \f$.
            This is identical for both curves since \f$ a \f$ is shared.
        */
        Real B(Time t, Time T) const override;
        //@}

        void generateArguments() override;

      private:
        Parameter& a_;
        Parameter& sigma_;

        Handle<YieldTermStructure> discountTermStructure_;
        Handle<YieldTermStructure> forwardTermStructure_;

        Parameter phi_;

        RelinkableHandle<HullWhite> discountModel_;
        RelinkableHandle<HullWhite> forwardModel_;
    };

    // inline definitions

    inline ext::shared_ptr<OneFactorModel::ShortRateDynamics> HW2CModel::dynamics() const {
        return ext::make_shared<HullWhite::Dynamics>(phi_, a(), sigma());
    }

}

#endif // quantlib_hw2c_model_hpp
