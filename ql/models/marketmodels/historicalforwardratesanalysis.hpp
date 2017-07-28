/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2007 Ferdinando Ametrano
 Copyright (C) 2007 François du Vignaud
 Copyright (C) 2007 Marco Bianchetti
 Copyright (C) 2007 Katiuscia Manzoni

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

/*! \file historicalforwardratesanalysis.hpp
    \brief Statistical analysis of historical forward rates
*/

#ifndef quantlib_historical_forward_rates_analysis_hpp
#define quantlib_historical_forward_rates_analysis_hpp

#include <ql/math/matrix.hpp>
#include <ql/time/calendar.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/indexes/swapindex.hpp>
#include <ql/termstructures/yield/piecewiseyieldcurve.hpp>
#include <ql/termstructures/yield/ratehelpers.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/math/statistics/sequencestatistics.hpp>
#include <ql/time/date.hpp>

namespace QuantLib {

    template<class Traits, class Interpolator>
    void historicalForwardRatesAnalysis(
                SequenceStatistics& statistics,
                std::vector<Date>& skippedDates,
                std::vector<std::string>& skippedDatesErrorMessage,
                std::vector<Date>& failedDates,
                std::vector<std::string>& failedDatesErrorMessage,
                std::vector<Period>& fixingPeriods,
                const Date& startDate,
                const Date& endDate,
                const Period& step,
                const std::shared_ptr<InterestRateIndex>& fwdIndex,
                const Period& initialGap,
                const Period& horizon,
                const std::vector<std::shared_ptr<IborIndex> >& iborIndexes,
                const std::vector<std::shared_ptr<SwapIndex> >& swapIndexes,
                const DayCounter& yieldCurveDayCounter,
                Real yieldCurveAccuracy = 1.0e-12,
                const Interpolator& i = Interpolator()) {


        statistics.reset();
        skippedDates.clear();
        skippedDatesErrorMessage.clear();
        failedDates.clear();
        failedDatesErrorMessage.clear();
        fixingPeriods.clear();

        SavedSettings backup;
        Settings::instance().enforcesTodaysHistoricFixings() = true;

        std::vector<std::shared_ptr<RateHelper> > rateHelpers;

        // Create DepositRateHelper
        std::vector<std::shared_ptr<SimpleQuote> > iborQuotes;
        std::vector<std::shared_ptr<IborIndex> >::const_iterator ibor;
        for (ibor=iborIndexes.begin(); ibor!=iborIndexes.end(); ++ibor) {
            std::shared_ptr<SimpleQuote> quote = std::make_shared<SimpleQuote>();
            iborQuotes.emplace_back(quote);
            Handle<Quote> quoteHandle(quote);
            rateHelpers.emplace_back(std::make_shared<DepositRateHelper>(quoteHandle,
                                  (*ibor)->tenor(),
                                  (*ibor)->fixingDays(),
                                  (*ibor)->fixingCalendar(),
                                  (*ibor)->businessDayConvention(),
                                  (*ibor)->endOfMonth(),
                                  (*ibor)->dayCounter()));
        }

        // Create SwapRateHelper
        std::vector<std::shared_ptr<SimpleQuote> > swapQuotes;
        std::vector<std::shared_ptr<SwapIndex> >::const_iterator swap;
        for (swap=swapIndexes.begin(); swap!=swapIndexes.end(); ++swap) {
            std::shared_ptr<SimpleQuote> quote = std::make_shared<SimpleQuote>();
            swapQuotes.emplace_back(quote);
            Handle<Quote> quoteHandle(quote);
            rateHelpers.emplace_back(std::make_shared<SwapRateHelper>(quoteHandle,
                               (*swap)->tenor(),
                               (*swap)->fixingCalendar(),
                               (*swap)->fixedLegTenor().frequency(),
                               (*swap)->fixedLegConvention(),
                               (*swap)->dayCounter(),
                               (*swap)->iborIndex()));
        }

        // Set up the forward rates time grid
        Period indexTenor = fwdIndex->tenor();
        Period fixingPeriod = initialGap;
        while (fixingPeriod<=horizon) {
            fixingPeriods.emplace_back(fixingPeriod);
            fixingPeriod += indexTenor;
        }

        Size nRates = fixingPeriods.size();
        statistics.reset(nRates);
        std::vector<Rate> fwdRates(nRates);
        std::vector<Rate> prevFwdRates(nRates);
        std::vector<Rate> fwdRatesDiff(nRates);
        DayCounter indexDayCounter = fwdIndex->dayCounter();
        Calendar cal = fwdIndex->fixingCalendar();

        // Bootstrap the yield curve at the currentDate
        Natural settlementDays = 0;
        PiecewiseYieldCurve<Traits, Interpolator> yc(settlementDays,
                                                     cal,
                                                     rateHelpers,
                                                     yieldCurveDayCounter,
                                                     std::vector<Handle<Quote> >(),
                                                     std::vector<Date>(),
                                                     yieldCurveAccuracy,
                                                     i);

        // start with a valid business date
        Date currentDate = cal.advance(startDate, 1*Days, Following);
        bool isFirst = true;
        // Loop over the historical dataset
        for (; currentDate<=endDate;
            currentDate = cal.advance(currentDate, step, Following)) {

            // move the evaluationDate to currentDate
            // and update ratehelpers dates...
            Settings::instance().evaluationDate() = currentDate;

            try {
                // update the quotes...
                for (Size j=0; j<iborIndexes.size(); ++j) {
                    Rate fixing = iborIndexes[j]->fixing(currentDate, false);
                    iborQuotes[j]->setValue(fixing);
                }
                for (Size j=0; j<swapIndexes.size(); ++j) {
                    Rate fixing = swapIndexes[j]->fixing(currentDate, false);
                    swapQuotes[j]->setValue(fixing);
                }
            } catch (std::exception& e) {
                skippedDates.emplace_back(currentDate);
                skippedDatesErrorMessage.emplace_back(e.what());
                continue;
            }

            try {
                for (Size j=0; j<nRates; ++j) {
                    // Time-to-go forwards
                    Date d = currentDate + fixingPeriods[j];
                    fwdRates[j] = yc.forwardRate(d,
                                                 indexTenor,
                                                 indexDayCounter,
                                                 Simple);
                }
            } catch (std::exception& e) {
                failedDates.emplace_back(currentDate);
                failedDatesErrorMessage.emplace_back(e.what());
                continue;
            }

            // From 2nd step onwards, calculate forward rate
            // relative differences
            if (!isFirst){
                for (Size j=0; j<nRates; ++j)
                    fwdRatesDiff[j] = fwdRates[j]/prevFwdRates[j] -1.0;
                // add observation
                statistics.add(fwdRatesDiff.begin(), fwdRatesDiff.end());
            }
            else
                isFirst = false;

            // Store last calculated forward rates
            std::swap(prevFwdRates, fwdRates);

        }
    }

    class HistoricalForwardRatesAnalysis {
      public:
        virtual ~HistoricalForwardRatesAnalysis() {}
        virtual const std::vector<Date>& skippedDates() const = 0;
        virtual const std::vector<std::string>& skippedDatesErrorMessage() const = 0;
        virtual const std::vector<Date>& failedDates() const = 0;
        virtual const std::vector<std::string>& failedDatesErrorMessage() const = 0;
        virtual const std::vector<Period>& fixingPeriods() const = 0;
    };

    //! %Historical correlation class
    template<class Traits, class Interpolator>
    class HistoricalForwardRatesAnalysisImpl : public HistoricalForwardRatesAnalysis {
      public:
        HistoricalForwardRatesAnalysisImpl(
                const std::shared_ptr<SequenceStatistics>& stats,
                const Date& startDate,
                const Date& endDate,
                const Period& step,
                const std::shared_ptr<InterestRateIndex>& fwdIndex,
                const Period& initialGap,
                const Period& horizon,
                const std::vector<std::shared_ptr<IborIndex> >& iborIndexes,
                const std::vector<std::shared_ptr<SwapIndex> >& swapIndexes,
                const DayCounter& yieldCurveDayCounter,
                Real yieldCurveAccuracy);
        HistoricalForwardRatesAnalysisImpl(){}
        const std::vector<Date>& skippedDates() const;
        const std::vector<std::string>& skippedDatesErrorMessage() const;
        const std::vector<Date>& failedDates() const;
        const std::vector<std::string>& failedDatesErrorMessage() const;
        const std::vector<Period>& fixingPeriods() const;
        //const std::shared_ptr<SequenceStatistics>& stats() const;
      private:
        // calculated data
        std::shared_ptr<SequenceStatistics> stats_;
        std::vector<Date> skippedDates_;
        std::vector<std::string> skippedDatesErrorMessage_;
        std::vector<Date> failedDates_;
        std::vector<std::string> failedDatesErrorMessage_;
        std::vector<Period> fixingPeriods_;
    };

    // inline
    template<class Traits, class Interpolator>
    const std::vector<Period>&
    HistoricalForwardRatesAnalysisImpl<Traits, Interpolator>::fixingPeriods() const {
        return fixingPeriods_;
    }

    template<class Traits, class Interpolator>
    inline const std::vector<Date>&
    HistoricalForwardRatesAnalysisImpl<Traits, Interpolator>::skippedDates() const {
        return skippedDates_;
    }

    template<class Traits, class Interpolator>
    inline const std::vector<std::string>&
    HistoricalForwardRatesAnalysisImpl<Traits, Interpolator>::skippedDatesErrorMessage() const {
        return skippedDatesErrorMessage_;
    }

    template<class Traits, class Interpolator>
    inline const std::vector<Date>&
    HistoricalForwardRatesAnalysisImpl<Traits, Interpolator>::failedDates() const {
        return failedDates_;
    }

    template<class Traits, class Interpolator>
    inline const std::vector<std::string>&
    HistoricalForwardRatesAnalysisImpl<Traits, Interpolator>::failedDatesErrorMessage() const {
        return failedDatesErrorMessage_;
    }

    //inline const std::shared_ptr<SequenceStatistics>&
    //HistoricalForwardRatesAnalysis::stats() const {
    //    return stats_;
    //}
    template<class Traits, class Interpolator>
    HistoricalForwardRatesAnalysisImpl<Traits, Interpolator>::HistoricalForwardRatesAnalysisImpl(
                const std::shared_ptr<SequenceStatistics>& stats,
                const Date& startDate,
                const Date& endDate,
                const Period& step,
                const std::shared_ptr<InterestRateIndex>& fwdIndex,
                const Period& initialGap,
                const Period& horizon,
                const std::vector<std::shared_ptr<IborIndex> >& iborIndexes,
                const std::vector<std::shared_ptr<SwapIndex> >& swapIndexes,
                const DayCounter& yieldCurveDayCounter,
                Real yieldCurveAccuracy)
    : stats_(stats) {
        historicalForwardRatesAnalysis<Traits,
                                       Interpolator>(
                    *stats_,
                    skippedDates_, skippedDatesErrorMessage_,
                    failedDates_, failedDatesErrorMessage_,
                    fixingPeriods_, startDate, endDate, step,
                    fwdIndex, initialGap, horizon,
                    iborIndexes, swapIndexes,
                    yieldCurveDayCounter, yieldCurveAccuracy);
      }
}

#endif
