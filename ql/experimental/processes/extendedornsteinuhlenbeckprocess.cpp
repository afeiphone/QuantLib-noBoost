/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2010 Klaus Spanderen

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

#include <ql/math/integrals/gausslobattointegral.hpp>
#include <ql/processes/ornsteinuhlenbeckprocess.hpp>
#include <ql/experimental/processes/extendedornsteinuhlenbeckprocess.hpp>

namespace QuantLib {

    namespace {

        class integrand {
            std::function<Real (Real)> b_;
            Real speed_;
          public:
            integrand(std::function<Real (Real)> b, Real speed)
            : b_(b), speed_(speed) {}
            Real operator()(Real x) const {
                return b_(x) * std::exp(speed_*x);
            }
        };

    }

    ExtendedOrnsteinUhlenbeckProcess::ExtendedOrnsteinUhlenbeckProcess(
                                        Real speed, Volatility vol, Real x0,
                                        std::function<Real (Real)> b,
                                        Discretization discretization,
                                        Real intEps)
    : speed_    (speed),
      vol_      (vol),
      b_        (b),
      intEps_   (intEps),
      ouProcess_(std::make_shared<OrnsteinUhlenbeckProcess>(speed, vol, x0)),
      discretization_(discretization) {
        QL_REQUIRE(speed_ >= 0.0, "negative a given");
        QL_REQUIRE(vol_ >= 0.0, "negative volatility given");
    }

    Real ExtendedOrnsteinUhlenbeckProcess::x0() const {
        return ouProcess_->x0();
    }
    
    Real ExtendedOrnsteinUhlenbeckProcess::drift(Time t, Real x) const {
        return ouProcess_->drift(t, x) + speed_*b_(t);
    }

    Real ExtendedOrnsteinUhlenbeckProcess::diffusion(Time t, Real x) const{
        return ouProcess_->diffusion(t, x);
    }

    Real ExtendedOrnsteinUhlenbeckProcess::stdDeviation(
                                           Time t0, Real x0, Time dt) const{
        return ouProcess_->stdDeviation(t0, x0, dt);
    }

    Real ExtendedOrnsteinUhlenbeckProcess::variance(
                                           Time t0, Real x0, Time dt) const{
        return ouProcess_->variance(t0, x0, dt);
    }

    Real ExtendedOrnsteinUhlenbeckProcess::speed() const {
        return speed_;
    }

    Real ExtendedOrnsteinUhlenbeckProcess::volatility() const {
        return vol_;
    }

    Real ExtendedOrnsteinUhlenbeckProcess::expectation(
                                          Time t0, Real x0, Time dt) const {
        switch (discretization_) {
          case MidPoint:
            return ouProcess_->expectation(t0, x0, dt)
                    + b_(t0+0.5*dt)*(1.0 - std::exp(-speed_*dt));
          case Trapezodial:
            {
              const Time t = t0+dt;
              const Time u = t0;
              const Real bt = b_(t);
              const Real bu = b_(u);
              const Real ex = std::exp(-speed_*dt);

              return ouProcess_->expectation(t0, x0, dt)
                    + bt-ex*bu - (bt-bu)/(speed_*dt)*(1-ex);
            }
          case GaussLobatto:
              return ouProcess_->expectation(t0, x0, dt)
                  + speed_*std::exp(-speed_*(t0+dt))
                  * GaussLobattoIntegral(100000, intEps_)(integrand(b_, speed_),
                                                          t0, t0+dt);
          default:
            QL_FAIL("unknown discretization scheme");
        }
    }
}

