//
//  interface.cpp
//  Backtest Environment
//
//  Created by Evan Kirkiles on 6/9/18.
//  Copyright © 2018 Evan Kirkiles. All rights reserved.
//

#include "interface.hpp"



// Constructor that initializes the executor and replaces the empty portfolio and pipeline with functioning ones
TradingInterface::TradingInterface(vector<string>i_symbol_list, vector<string>i_benchmarksymbols, double i_initial_cap, char* i_start_date, char* i_end_date) : executor(&events) {
    
    // Initialize variables inputted in constructor
    symbol_list = i_symbol_list;
    benchmarksymbols = i_benchmarksymbols;
    initial_capital = i_initial_cap;
    startdate = i_start_date;
    enddate = i_end_date;
    continue_backtest = 0;
    
    vector<string> spy = {string("SPY")};
    
    // Create data handler and portfolio management
    pipeline = HistoricalCSVDataHandler(&events, &symbol_list, startdate, enddate, &continue_backtest);
    portfolio = NaivePortfolio(&pipeline, symbol_list, &events, startdate, enddate, initial_capital);
    
    // Initialize benchmark portfolio and data
    benchmarkpipeline = HistoricalCSVDataHandler(&events, &benchmarksymbols, startdate, enddate, &continue_backtest);
    benchmarkportfolio = NaivePortfolio(&benchmarkpipeline, benchmarksymbols, &events, startdate, enddate, initial_capital);
}

// Begins the backtest!
void TradingInterface::runbacktest(BuyAndHoldStrategy strategy, Benchmark i_benchmark, GNUPlotter* plot) {
    continue_backtest = 1;
    pipeline.format_csv_data();
    pipeline.update_bars();
    benchmarkpipeline.format_csv_data();
    benchmarkpipeline.update_bars();
    
    strat = strategy;
    benchmark = i_benchmark;
    
    cout << "Initializing backtest..." << endl;
    
    // Event-driven loop that continues to check for events
    while(continue_backtest == 1) {
        
        // Handles each event in the list and removes it from the stack
        if (events.size() != 0) {
            if (events[0].type == "MARKET") {
                MarketEvent* marketevent = dynamic_cast<MarketEvent*>(&events[0]);
                
                // In case of a MarketEvent, use updated data to calculate next strategy's next move and send a signal
                strat.calculate_signals(*marketevent);
                benchmark.calculate_signals(*marketevent);
                
            } else if (events[0].type == "SIGNAL") {
                SignalEvent* signalevent = dynamic_cast<SignalEvent*>(&events[0]);
                
                // In case of a SignalEvent, portfolio sends necessary orders based on signal send by strategy
                // Determine who is target for the signal event
                if (events[0].target == "ALGO") {
                    portfolio.update_signal(*signalevent);
                } else if (events[0].target == "BENCH") {
                    benchmarkportfolio.update_signal(*signalevent);
                }
                
            } else if (events[0].type == "ORDER") {
                OrderEvent* orderevent = dynamic_cast<OrderEvent*>(&events[0]);
                
                // In case of an OrderEvent, the execution handler fills the received order (like a brokerage)
                executor.execute_order(*orderevent);
                orderevent->print_order();
                
            } else if (events[0].type == "FILL") {
                FillEvent* fillevent = dynamic_cast<FillEvent*>(&events[0]);
                
                // In case of a FillEvent, the portfolio updates its information based on the fill information
                // Determine who is target for the fill event
                if (events[0].target == "ALGO") {
                    cout << "FILLED " << fillevent->symbol << " FOR ALGO" << endl;
                    portfolio.update_fill(*fillevent);
                } else if (events[0].target == "BENCH") {
                    cout << "FILLED " << fillevent->symbol << " FOR BENCH" << endl;
                    benchmarkportfolio.update_fill(*fillevent);
                }
            }
            events.erase(events.begin());
        } else {
            benchmarkportfolio.update_timeindex();
            benchmarkpipeline.update_bars();
            portfolio.update_timeindex();
            pipeline.update_bars();
            plot->updatePlot();
        }
    }
    plot->quitPlot();
}