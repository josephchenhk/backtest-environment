//
//  portfolio.cpp
//  Backtest Environment
//
//  Created by Evan Kirkiles on 6/2/18.
//  Copyright © 2018 Evan Kirkiles. All rights reserved.
//

#include "portfolio.hpp"

using namespace std;

// MARK: Naive Portfolio

// Fake constructor
NaivePortfolio::NaivePortfolio() { };

// Constructor
NaivePortfolio::NaivePortfolio(HistoricalCSVDataHandler* i_bars, vector<string> i_symbol_list, boost::ptr_vector<Event>* i_events, char* i_start_date, char* i_end_date, double i_initial_capital=100000.0) {
    
    // Initialize all instance variables
    bars = i_bars;
    events = i_events;
    start_date = i_start_date;
    end_date = i_end_date;
    initial_capital = i_initial_capital;
    symbol_list = i_symbol_list;
    
    // Create positions and holdings
    all_positions = construct_all_positions();
    current_positions = construct_current_positions();
    all_holdings = construct_all_holdings();
    current_holdings = construct_current_holdings();
}

// Positions & holdings constructors
map<long, map<string, double>> NaivePortfolio::construct_all_positions() {
    map<string, double> temp = {};
    for (int i = 0; i < symbol_list.size(); i++) {
        temp[symbol_list[i]] = 0;
    }
    map<long, map<string, double>> c;
    c[get_epoch_time(start_date)] = temp;
    return c;
}
map<string, double> NaivePortfolio::construct_current_positions() {
    map<string, double> temp = {};
    for (int i = 0; i < symbol_list.size(); i++) {
        temp[symbol_list[i]] = 0;
    }
    return temp;
}
map<long, map<string, double>> NaivePortfolio::construct_all_holdings() {
    map<string, double> temp = {};
    for (int i = 0; i < symbol_list.size(); i++) {
        temp[symbol_list[i]] = 0;
    }
    temp["heldcash"] = initial_capital;
    temp["commission"] = 0.0;
    temp["totalholdings"] = initial_capital;
    
    // Equity curve data
    temp["returns"] = 0.0;
    temp["equitycurve"] = 0.0;
    
    map<long, map<string, double>> c;
    c[get_epoch_time(start_date)] = temp;
    return c;
}
map<string, double> NaivePortfolio::construct_current_holdings() {
    map<string, double> temp = {};
    for (int i = 0; i < symbol_list.size(); i++) {
        temp[symbol_list[i]] = 0;
    }
    temp["heldcash"] = initial_capital;
    temp["commission"] = 0.0;
    temp["totalholdings"] = initial_capital;
    return temp;
}

// Update holdings evaluations with most recently completed bar (previous day)
void NaivePortfolio::update_timeindex() {
    map<string, map<string, map<long, double>>> lastbar;
    long date;
    double sumvalues = 0;
    double previoustotal = all_holdings.rbegin().operator*().second["totalholdings"];
    double previouscurve = all_holdings.rbegin().operator*().second["equitycurve"];
    
    for (int i=0; i<symbol_list.size();i++) {
        
        // Update positions
        lastbar[symbol_list[i]] = bars->get_latest_bars(symbol_list[i],1);
        
        date = lastbar[symbol_list[i]]["open"].rbegin()->first;
        all_positions[date][symbol_list[i]] = current_positions[symbol_list[i]];
        
        // Update holdings
        // Estimates market value of a stock by using the quantity * its closing price (most likely inaccurate)
        double market_value = current_positions[symbol_list[i]] * lastbar[symbol_list[i]]["close"][date];
        all_holdings[date][symbol_list[i]] = market_value;
        current_holdings[symbol_list[i]] = market_value;
        sumvalues += market_value;
    }
    
    current_holdings["totalholdings"] = current_holdings["heldcash"] + sumvalues;
    all_holdings[date]["totalholdings"] = current_holdings["heldcash"] + sumvalues;
    all_holdings[date]["heldcash"] = current_holdings["heldcash"];
    all_holdings[date]["commission"] = current_holdings["commission"];
    
    // Equity curve handling
    if (all_holdings.size() > 1) {
        double returns = (all_holdings[date]["totalholdings"]/previoustotal) - 1;
        all_holdings[date]["returns"] = returns;
        all_holdings[date]["equitycurve"] = (previouscurve + 1) * (returns + 1) - 1;
    }
}

// Update positions from a FillEvent
void NaivePortfolio::update_positions_from_fill(FillEvent fill) {
    int fill_dir = 0;
    if (fill.direction == "BUY") {
        fill_dir = 1;
    } else if (fill.direction == "SELL") {
        fill_dir = -1;
    }
    
    // Update the positions with retrieved fill information
    current_positions[fill.symbol] += fill_dir*fill.quantity;
}

// Updates holdings from a FillEvent
// To estimate the cost of a fill, uses the closing price of the last bar
void NaivePortfolio::update_holdings_from_fill(FillEvent fill) {
    int fill_dir = 0;
    if (fill.direction == "BUY") {
        fill_dir = 1;
    } else if (fill.direction == "SELL") {
        fill_dir = -1;
    }
    
    // Update holdings list with calculated fill information
    double fill_cost = bars->get_latest_bars(fill.symbol, 1)["close"].rbegin()->second;
    double cost = fill_dir * fill_cost * fill.quantity;
    current_holdings[fill.symbol] += cost;
    current_holdings["commission"] += fill.commission;
    current_holdings["heldcash"] -= (cost + fill.commission);
    current_holdings["totalholdings"] -= fill.commission;
}

// Updates positions and holdings in case of fill event
void NaivePortfolio::update_fill(FillEvent event) {
    update_positions_from_fill(event);
    update_holdings_from_fill(event);
}
void NaivePortfolio::update_signal(SignalEvent event) {
    generate_naive_order(event);
}

// Generates order signal for 100 or so shares of each asset
// No risk management or position sizing; to be implemented later
void NaivePortfolio::generate_naive_order(SignalEvent signal) {
    
    string symbol = signal.symbol;
    double strength = signal.strength;
    
    int mkt_quantity = calculate_quantity(symbol, strength);
    cout << symbol << " : " << mkt_quantity << endl;
    string order_type = "MKT";
    
    // Order logic
    if (mkt_quantity > 0) {
        events->push_back(new OrderEvent(symbol, order_type, abs(mkt_quantity), "BUY", signal.target));
    } else if (mkt_quantity < 0) {
        events->push_back(new OrderEvent(symbol, order_type, abs(mkt_quantity), "SELL", signal.target));
    } else {
        cout << "Already holding " << strength << "% in " << symbol << ".";
    }
}

// Calculates the number of shares needed to fill the given percentage (rounds down)
int NaivePortfolio::calculate_quantity(string symbol, double percentage) {
    
    // If want to completely exit, then just return the opposite position size
    if (percentage == 0) {
        return -current_positions[symbol];
    }
    
    // Otherwise, calculate required quantity of shares
    double currentposition = current_positions[symbol];
    double sellingprice = bars->get_latest_bars(symbol, 1)["close"].rbegin()->second;
    double requiredcash = (percentage * all_holdings.rbegin()->second["totalholdings"]) - (currentposition * sellingprice);
    
    // Commission included in calculation
    if (requiredcash <= 500 * (sellingprice + 0.013)) {
        return floor(requiredcash  / (sellingprice + 0.013));
    } else {
        return floor(requiredcash  / (sellingprice + 0.008));
    }
}