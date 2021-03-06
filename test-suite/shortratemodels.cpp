/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2007 Marco Bianchetti
 Copyright (C) 2007 Giorgio Facchinetti
 Copyright (C) 2006 Chiara Fornarola
 Copyright (C) 2005 StatPro Italia srl
 Copyright (C) 2013 Peter Caspers

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

#include "utilities.hpp"
#include <ql/models/shortrate/onefactormodels/hullwhite.hpp>
#include <ql/models/shortrate/calibrationhelpers/swaptionhelper.hpp>
#include <ql/pricingengines/swaption/jamshidianswaptionengine.hpp>
#include <ql/pricingengines/swap/treeswapengine.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/indexes/indexmanager.hpp>
#include <ql/math/optimization/simplex.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/termstructures/yield/discountcurve.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/schedule.hpp>
#include <ql/quotes/simplequote.hpp>

using namespace QuantLib;


namespace {

    struct CalibrationData {
        Integer start;
        Integer length;
        Volatility volatility;
    };

}


TEST_CASE("ShortRateModel_CachedHullWhite", "[ShortRateModel]") {
    INFO("Testing Hull-White calibration against cached values using swaptions with start delay...");

    SavedSettings backup;
    IndexHistoryCleaner cleaner;

    Date today(15, February, 2002);
    Date settlement(19, February, 2002);
    Settings::instance().evaluationDate() = today;
    Handle<YieldTermStructure> termStructure(flatRate(settlement, 0.04875825,
                                                      Actual365Fixed()));
    std::shared_ptr < HullWhite > model = std::make_shared<HullWhite>(termStructure);
    CalibrationData data[] = {{1, 5, 0.1148},
                              {2, 4, 0.1108},
                              {3, 3, 0.1070},
                              {4, 2, 0.1021},
                              {5, 1, 0.1000}};
    std::shared_ptr < IborIndex > index = std::make_shared<Euribor6M>(termStructure);

    std::shared_ptr < PricingEngine > engine =
            std::make_shared<JamshidianSwaptionEngine>(model);

    std::vector<std::shared_ptr<CalibrationHelper> > swaptions;
    for (Size i = 0; i < LENGTH(data); i++) {
        std::shared_ptr < Quote > vol = std::make_shared<SimpleQuote>(data[i].volatility);
        std::shared_ptr < CalibrationHelper > helper =
                std::make_shared<SwaptionHelper>(Period(data[i].start, Years),
                                                 Period(data[i].length, Years),
                                                 Handle<Quote>(vol),
                                                 index,
                                                 Period(1, Years), Thirty360(),
                                                 Actual360(), termStructure);
        helper->setPricingEngine(engine);
        swaptions.emplace_back(helper);
    }

    // Set up the optimization problem
    // Real simplexLambda = 0.1;
    // Simplex optimizationMethod(simplexLambda);
    LevenbergMarquardt optimizationMethod(1.0e-8, 1.0e-8, 1.0e-8);
    EndCriteria endCriteria(10000, 100, 1e-6, 1e-8, 1e-8);

    //Optimize
    model->calibrate(swaptions, optimizationMethod, endCriteria);
    EndCriteria::Type ecType = model->endCriteria();

    // Check and print out results
#if defined(QL_USE_INDEXED_COUPON)
    Real cachedA = 0.0463679, cachedSigma = 0.00579831;
#else
    Real cachedA = 0.0464041, cachedSigma = 0.00579912;
#endif
    Real tolerance = 1.0e-5;
    Array xMinCalculated = model->params();
    Real yMinCalculated = model->value(xMinCalculated, swaptions);
    Array xMinExpected(2);
    xMinExpected[0] = cachedA;
    xMinExpected[1] = cachedSigma;
    Real yMinExpected = model->value(xMinExpected, swaptions);
    if (std::fabs(xMinCalculated[0] - cachedA) > tolerance
        || std::fabs(xMinCalculated[1] - cachedSigma) > tolerance) {
        FAIL_CHECK("Failed to reproduce cached calibration results:\n"
                           << "calculated: a = " << xMinCalculated[0] << ", "
                           << "sigma = " << xMinCalculated[1] << ", "
                           << "f(a) = " << yMinCalculated << ",\n"
                           << "expected:   a = " << xMinExpected[0] << ", "
                           << "sigma = " << xMinExpected[1] << ", "
                           << "f(a) = " << yMinExpected << ",\n"
                           << "difference: a = " << xMinCalculated[0] - xMinExpected[0] << ", "
                           << "sigma = " << xMinCalculated[1] - xMinExpected[1] << ", "
                           << "f(a) = " << yMinCalculated - yMinExpected << ",\n"
                           << "end criteria = " << ecType);
    }
}

TEST_CASE("ShortRateModel_CachedHullWhiteFixedReversion", "[ShortRateModel]") {
    INFO("Testing Hull-White calibration with fixed reversion against cached values...");

    SavedSettings backup;
    IndexHistoryCleaner cleaner;

    Date today(15, February, 2002);
    Date settlement(19, February, 2002);
    Settings::instance().evaluationDate() = today;
    Handle<YieldTermStructure> termStructure(flatRate(settlement, 0.04875825,
                                                      Actual365Fixed()));
    std::shared_ptr < HullWhite > model = std::make_shared<HullWhite>(termStructure, 0.05, 0.01);
    CalibrationData data[] = {{1, 5, 0.1148},
                              {2, 4, 0.1108},
                              {3, 3, 0.1070},
                              {4, 2, 0.1021},
                              {5, 1, 0.1000}};
    std::shared_ptr < IborIndex > index = std::make_shared<Euribor6M>(termStructure);

    std::shared_ptr < PricingEngine > engine =
            std::make_shared<JamshidianSwaptionEngine>(model);

    std::vector<std::shared_ptr<CalibrationHelper> > swaptions;
    for (Size i = 0; i < LENGTH(data); i++) {
        std::shared_ptr < Quote > vol = std::make_shared<SimpleQuote>(data[i].volatility);
        std::shared_ptr < CalibrationHelper > helper =
                std::make_shared<SwaptionHelper>(Period(data[i].start, Years),
                                                 Period(data[i].length, Years),
                                                 Handle<Quote>(vol),
                                                 index,
                                                 Period(1, Years), Thirty360(),
                                                 Actual360(), termStructure);
        helper->setPricingEngine(engine);
        swaptions.emplace_back(helper);
    }

    // Set up the optimization problem
    //Real simplexLambda = 0.1;
    //Simplex optimizationMethod(simplexLambda);
    LevenbergMarquardt optimizationMethod;//(1.0e-18,1.0e-18,1.0e-18);
    EndCriteria endCriteria(1000, 500, 1E-8, 1E-8, 1E-8);

    //Optimize
    model->calibrate(swaptions, optimizationMethod, endCriteria, Constraint(), std::vector<Real>(),
                     HullWhite::FixedReversion());
    EndCriteria::Type ecType = model->endCriteria();

    // Check and print out results
#if defined(QL_USE_INDEXED_COUPON)
    Real cachedA = 0.05, cachedSigma = 0.00585835;
#else
    Real cachedA = 0.05, cachedSigma = 0.00585858;
#endif
    Real tolerance = 1.0e-5;
    Array xMinCalculated = model->params();
    Real yMinCalculated = model->value(xMinCalculated, swaptions);
    Array xMinExpected(2);
    xMinExpected[0] = cachedA;
    xMinExpected[1] = cachedSigma;
    Real yMinExpected = model->value(xMinExpected, swaptions);
    if (std::fabs(xMinCalculated[0] - cachedA) > tolerance
        || std::fabs(xMinCalculated[1] - cachedSigma) > tolerance) {
        FAIL_CHECK("Failed to reproduce cached calibration results:\n"
                           << "calculated: a = " << xMinCalculated[0] << ", "
                           << "sigma = " << xMinCalculated[1] << ", "
                           << "f(a) = " << yMinCalculated << ",\n"
                           << "expected:   a = " << xMinExpected[0] << ", "
                           << "sigma = " << xMinExpected[1] << ", "
                           << "f(a) = " << yMinExpected << ",\n"
                           << "difference: a = " << xMinCalculated[0] - xMinExpected[0] << ", "
                           << "sigma = " << xMinCalculated[1] - xMinExpected[1] << ", "
                           << "f(a) = " << yMinCalculated - yMinExpected << ",\n"
                           << "end criteria = " << ecType);
    }
}


TEST_CASE("ShortRateModel_CachedHullWhite2", "[ShortRateModel]") {
    INFO("Testing Hull-White calibration against cached "
                 "values using swaptions without start delay...");

    SavedSettings backup;
    IndexHistoryCleaner cleaner;

    Date today(15, February, 2002);
    Date settlement(19, February, 2002);
    Settings::instance().evaluationDate() = today;
    Handle<YieldTermStructure> termStructure(flatRate(settlement, 0.04875825,
                                                      Actual365Fixed()));
    std::shared_ptr < HullWhite > model = std::make_shared<HullWhite>(termStructure);
    CalibrationData data[] = {{1, 5, 0.1148},
                              {2, 4, 0.1108},
                              {3, 3, 0.1070},
                              {4, 2, 0.1021},
                              {5, 1, 0.1000}};
    std::shared_ptr < IborIndex > index = std::make_shared<Euribor6M>(termStructure);
    std::shared_ptr < IborIndex > index0 =
            std::make_shared<IborIndex>(index->familyName(), index->tenor(), 0, index->currency(),
                                        index->fixingCalendar(),
                                        index->businessDayConvention(), index->endOfMonth(), index->dayCounter(),
                                        termStructure); // Euribor 6m with zero fixing days

    std::shared_ptr < PricingEngine > engine =
            std::make_shared<JamshidianSwaptionEngine>(model);

    std::vector<std::shared_ptr<CalibrationHelper> > swaptions;
    for (Size i = 0; i < LENGTH(data); i++) {
        std::shared_ptr < Quote > vol = std::make_shared<SimpleQuote>(data[i].volatility);
        std::shared_ptr < CalibrationHelper > helper =
                std::make_shared<SwaptionHelper>(Period(data[i].start, Years),
                                                 Period(data[i].length, Years),
                                                 Handle<Quote>(vol),
                                                 index0,
                                                 Period(1, Years), Thirty360(),
                                                 Actual360(), termStructure);
        helper->setPricingEngine(engine);
        swaptions.emplace_back(helper);
    }

    // Set up the optimization problem
    // Real simplexLambda = 0.1;
    // Simplex optimizationMethod(simplexLambda);
    LevenbergMarquardt optimizationMethod(1.0e-8, 1.0e-8, 1.0e-8);
    EndCriteria endCriteria(10000, 100, 1e-6, 1e-8, 1e-8);

    //Optimize
    model->calibrate(swaptions, optimizationMethod, endCriteria);
    EndCriteria::Type ecType = model->endCriteria();

    // Check and print out results
    // The cached values were produced with an older version of the
    // JamshidianEngine not accounting for the delay between option
    // expiry and underlying start
#if defined(QL_USE_INDEXED_COUPON)
    Real cachedA = 0.0481608, cachedSigma = 0.00582493;
#else
    Real cachedA = 0.0482063, cachedSigma = 0.00582687;
#endif
    Real tolerance = 5.0e-6;
    Array xMinCalculated = model->params();
    Real yMinCalculated = model->value(xMinCalculated, swaptions);
    Array xMinExpected(2);
    xMinExpected[0] = cachedA;
    xMinExpected[1] = cachedSigma;
    Real yMinExpected = model->value(xMinExpected, swaptions);
    if (std::fabs(xMinCalculated[0] - cachedA) > tolerance
        || std::fabs(xMinCalculated[1] - cachedSigma) > tolerance) {
        FAIL_CHECK("Failed to reproduce cached calibration results:\n"
                           << "calculated: a = " << xMinCalculated[0] << ", "
                           << "sigma = " << xMinCalculated[1] << ", "
                           << "f(a) = " << yMinCalculated << ",\n"
                           << "expected:   a = " << xMinExpected[0] << ", "
                           << "sigma = " << xMinExpected[1] << ", "
                           << "f(a) = " << yMinExpected << ",\n"
                           << "difference: a = " << xMinCalculated[0] - xMinExpected[0] << ", "
                           << "sigma = " << xMinCalculated[1] - xMinExpected[1] << ", "
                           << "f(a) = " << yMinCalculated - yMinExpected << ",\n"
                           << "end criteria = " << ecType);
    }
}

TEST_CASE("ShortRateModel_Swaps", "[ShortRateModel]") {
    INFO("Testing Hull-White swap pricing against known values...");

    SavedSettings backup;
    IndexHistoryCleaner cleaner;

    Date today = Settings::instance().evaluationDate();
    Calendar calendar = TARGET();
    today = calendar.adjust(today);
    Settings::instance().evaluationDate() = today;

    Date settlement = calendar.advance(today, 2, Days);

    Date dates[] = {
            settlement,
            calendar.advance(settlement, 1, Weeks),
            calendar.advance(settlement, 1, Months),
            calendar.advance(settlement, 3, Months),
            calendar.advance(settlement, 6, Months),
            calendar.advance(settlement, 9, Months),
            calendar.advance(settlement, 1, Years),
            calendar.advance(settlement, 2, Years),
            calendar.advance(settlement, 3, Years),
            calendar.advance(settlement, 5, Years),
            calendar.advance(settlement, 10, Years),
            calendar.advance(settlement, 15, Years)
    };
    DiscountFactor discounts[] = {
            1.0,
            0.999258,
            0.996704,
            0.990809,
            0.981798,
            0.972570,
            0.963430,
            0.929532,
            0.889267,
            0.803693,
            0.596903,
            0.433022
    };

    Handle<YieldTermStructure> termStructure(
            std::make_shared<DiscountCurve>(
                    std::vector<Date>(dates, dates + LENGTH(dates)),
                    std::vector<DiscountFactor>(discounts,
                                                discounts + LENGTH(discounts)),
                    Actual365Fixed()));

    std::shared_ptr < HullWhite > model = std::make_shared<HullWhite>(termStructure);

    Integer start[] = {-3, 0, 3};
    Integer length[] = {2, 5, 10};
    Rate rates[] = {0.02, 0.04, 0.06};
    std::shared_ptr < IborIndex > euribor = std::make_shared<Euribor6M>(termStructure);

    std::shared_ptr < PricingEngine > engine =
            std::make_shared<TreeVanillaSwapEngine>(model, 120);

#if defined(QL_USE_INDEXED_COUPON)
    Real tolerance = 4.0e-3;
#else
    Real tolerance = 1.0e-8;
#endif

    for (Size i = 0; i < LENGTH(start); i++) {

        Date startDate = calendar.advance(settlement, start[i], Months);
        if (startDate < today) {
            Date fixingDate = calendar.advance(startDate, -2, Days);
            TimeSeries<Real> pastFixings;
            pastFixings[fixingDate] = 0.03;
            IndexManager::instance().setHistory(euribor->name(),
                                                pastFixings);
        }

        for (Size j = 0; j < LENGTH(length); j++) {

            Date maturity = calendar.advance(startDate, length[i], Years);
            Schedule fixedSchedule(startDate, maturity, Period(Annual),
                                   calendar, Unadjusted, Unadjusted,
                                   DateGeneration::Forward, false);
            Schedule floatSchedule(startDate, maturity, Period(Semiannual),
                                   calendar, Following, Following,
                                   DateGeneration::Forward, false);
            for (Size k = 0; k < LENGTH(rates); k++) {

                VanillaSwap swap(VanillaSwap::Payer, 1000000.0,
                                 fixedSchedule, rates[k], Thirty360(),
                                 floatSchedule, euribor, 0.0, Actual360());
                swap.setPricingEngine(std::make_shared<DiscountingSwapEngine>(termStructure));
                Real expected = swap.NPV();
                swap.setPricingEngine(engine);
                Real calculated = swap.NPV();

                Real error = std::fabs((expected - calculated) / expected);
                if (error > tolerance) {
                    FAIL_CHECK("Failed to reproduce swap NPV:"
                                       << QL_FIXED << std::setprecision(9)
                                       << "\n    calculated: " << calculated
                                       << "\n    expected:   " << expected
                                       << QL_SCIENTIFIC
                                       << "\n    rel. error: " << error);
                }
            }
        }
    }
}

TEST_CASE("ShortRateModel_FuturesConvexityBias", "[ShortRateModel]") {
    INFO("Testing Hull-White futures convexity bias...");

    // G. Kirikos, D. Novak, "Convexity Conundrums", Risk Magazine, March 1997
    Real futureQuote = 94.0;
    Real a = 0.03;
    Real sigma = 0.015;
    Time t = 5.0;
    Time T = 5.25;

    Rate expectedForward = 0.0573037;
    Real tolerance = 0.0000001;

    Rate futureImpliedRate = (100.0 - futureQuote) / 100.0;
    Rate calculatedForward =
            futureImpliedRate - HullWhite::convexityBias(futureQuote, t, T, sigma, a);

    Real error = std::fabs(calculatedForward - expectedForward);

    if (error > tolerance) {
        FAIL_CHECK("Failed to reproduce convexity bias:"
                           << "\ncalculated: " << calculatedForward
                           << "\n  expected: " << expectedForward
                           << QL_SCIENTIFIC
                           << "\n     error: " << error
                           << "\n tolerance: " << tolerance);
    }
}
