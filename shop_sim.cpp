
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <iomanip>

struct Offer {
    int volume = 0;
    double price_per_unit = 0.0;
    std::vector<double> stages; // доли оплаты, сумма = 1.0
    int paid_stage_index = 0;

    void create(int v, double p, const std::vector<double>& s) {
        volume = v;
        price_per_unit = p;
        stages = s;
        paid_stage_index = 0;
    }

    double next_payment_amount() const {
        if (paid_stage_index >= (int)stages.size()) return 0.0;
        return volume * price_per_unit * stages[paid_stage_index];
    }

    void advance_stage() { if (paid_stage_index < (int)stages.size()) ++paid_stage_index; }

    bool is_completed() const { return paid_stage_index >= (int)stages.size(); }
};

struct ModelState {
    int day = 0;
    int base_stock = 0;
    int truck_intransit = 0;
    int store_stock = 0;
    double bank_account = 0.0;
    double credit_used = 0.0;
    double tax_accrued_base = 0.0;
    double total_tax_paid = 0.0;
    double sales_skill = 0.8;
    double sales_motivation = 0.8;
    Offer offer;
    double total_revenue = 0.0;
    double total_expenses = 0.0;
};

struct Config {
    double base_price = 100.0;
    double base_offer_price = 80.0;
    double initial_balance = 10000.0;
    int initial_base_stock = 500;
    int initial_store_stock = 50;
    double credit_limit = 5000.0;
    double credit_rate_monthly = 0.02;
    double tax_rate = 0.18;
    int days = 90;
};

double calc_demand(double base_demand, double selling_price, const Config& cfg, double skill, double motiv) {
    double price_factor = 1.0 - (selling_price - cfg.base_price) / cfg.base_price;
    if (price_factor < 0.0) price_factor = 0.0;
    double staff_factor = 0.6 + 0.4 * ((skill + motiv) / 2.0); // 0.6 .. 1.0
    double demand = base_demand * price_factor * staff_factor;
    if (demand < 0.0) demand = 0.0;
    return demand;
}

ModelState initialize(const Config& cfg) {
    ModelState s;
    s.day = 0;
    s.base_stock = cfg.initial_base_stock;
    s.truck_intransit = 0;
    s.store_stock = cfg.initial_store_stock;
    s.bank_account = cfg.initial_balance;
    s.credit_used = 0.0;
    s.tax_accrued_base = 0.0;
    s.total_tax_paid = 0.0;
    s.sales_skill = 0.8;
    s.sales_motivation = 0.8;
    s.offer = Offer();
    s.total_revenue = 0.0;
    s.total_expenses = 0.0;
    return s;
}

struct DayResult {
    int day;
    double bank_account;
    double credit_used;
    int base_stock;
    int truck_intransit;
    int store_stock;
    int offer_volume;
    int offer_paid_stage;
    double tax_accrued_base;
    double total_tax_paid;
    int daily_sales_qty;
    double daily_revenue;
};

DayResult simulate_day(ModelState& state, const Config& cfg,
                       int transfer_volume, bool buy_offer, double selling_price,
                       double base_daily_demand = 20.0)
{
    state.day += 1;

    // 1) Загрузка в машину (если есть запас)
    int load = std::min(transfer_volume, state.base_stock);
    state.base_stock -= load;
    state.truck_intransit += load;

    // 2) Доставка/выгрузка: упрощенно 90% доставлено в тот же день
    int delivered = static_cast<int>(std::floor(state.truck_intransit * 0.9));
    state.truck_intransit -= delivered;
    state.store_stock += delivered;

    // 3) Покупка мелко-опта
    if (buy_offer) {
        int offer_volume = 100;
        double offer_price = cfg.base_offer_price;
        std::vector<double> stages = {0.5, 0.5};
        state.offer.create(offer_volume, offer_price, stages);
        double pay = state.offer.next_payment_amount();
        state.bank_account -= pay;
        state.total_expenses += pay;
        state.offer.advance_stage();
    }

    // 4) Продажи
    double demand_d = calc_demand(base_daily_demand, selling_price, cfg, state.sales_skill, state.sales_motivation);
    int sales_qty = static_cast<int>(std::round(demand_d));
    sales_qty = std::min(sales_qty, state.store_stock);
    double revenue = sales_qty * selling_price;
    state.store_stock -= sales_qty;
    state.bank_account += revenue;
    state.total_revenue += revenue;

    // 5) Обслуживание поэтапных платежей (каждые 30 дней)
    if (!state.offer.is_completed() && (state.day % 30 == 0)) {
        double pay = state.offer.next_payment_amount();
        state.bank_account -= pay;
        state.total_expenses += pay;
        state.offer.advance_stage();
    }

    // 6) Налог: накапливаем налоговую базу (упрощённо: от выручки)
    double daily_profit = revenue; // упрощённая логика
    if (daily_profit > 0.0) state.tax_accrued_base += daily_profit;

    if (state.day % 30 == 0) {
        double tax = state.tax_accrued_base * cfg.tax_rate;
        state.tax_accrued_base = 0.0;
        state.bank_account -= tax;
        state.total_tax_paid += tax;
        state.total_expenses += tax;
    }

    // 7) Кредит: если баланс < 0, используем кредит
    if (state.bank_account < 0.0) {
        double need = -state.bank_account;
        double available = std::max(0.0, cfg.credit_limit - state.credit_used);
        double used = std::min(need, available);
        state.credit_used += used;
        state.bank_account += used;
    }

    // Поднятие процентов по кредиту раз в 30 дней
    if (state.day % 30 == 0 && state.credit_used > 0.0) {
        double interest = state.credit_used * cfg.credit_rate_monthly;
        state.bank_account -= interest;
        state.total_expenses += interest;
    }

    DayResult res;
    res.day = state.day;
    res.bank_account = state.bank_account;
    res.credit_used = state.credit_used;
    res.base_stock = state.base_stock;
    res.truck_intransit = state.truck_intransit;
    res.store_stock = state.store_stock;
    res.offer_volume = state.offer.volume;
    res.offer_paid_stage = state.offer.paid_stage_index;
    res.tax_accrued_base = state.tax_accrued_base;
    res.total_tax_paid = state.total_tax_paid;
    res.daily_sales_qty = sales_qty;
    res.daily_revenue = revenue;
    return res;
}

void pretty_print_day(const DayResult& d) {
    std::cout << "День " << d.day << ": баланс=" << std::fixed << std::setprecision(2) << d.bank_account
              << ", кредит использован=" << d.credit_used << "\n";
    std::cout << "  Запасы: базовый=" << d.base_stock << ", в_пути=" << d.truck_intransit << ", магазин=" << d.store_stock << "\n";
    std::cout << "  Продано " << d.daily_sales_qty << " ед., выручка дня=" << d.daily_revenue << "\n";
    if (d.offer_volume > 0) {
        std::cout << "  Активное предложение: объём=" << d.offer_volume << ", оплачено этапов=" << d.offer_paid_stage << "\n";
    }
    std::cout << "  Налог. база накоплено=" << d.tax_accrued_base << ", всего уплачено=" << d.total_tax_paid << "\n";
    std::cout << std::string(60, '-') << "\n";
}

void run_demo(const Config& cfg, int days = 10) {
    std::cout << "Запуск демонстрации (non-interactive) на " << days << " дней\n";
    ModelState state = initialize(cfg);
    std::vector<std::tuple<int,bool,double>> inputs = {
        {50, false, 120.0},
        {0,  true,  110.0},
        {30, false, 115.0},
        {0,  false, 105.0},
        {80, false, 100.0},
        {0,  false, 95.0},
        {20, false, 100.0},
        {0,  false, 100.0},
        {0,  false, 100.0},
        {0,  false, 90.0}
    };
    for (int i = 0; i < days; ++i) {
        auto [tr, buy, price] = inputs[i % inputs.size()];
        DayResult dr = simulate_day(state, cfg, tr, buy, price);
        pretty_print_day(dr);
    }

    std::cout << "Демо завершено. Итоги:\n";
    std::cout << "  Финал. Баланс=" << std::fixed << std::setprecision(2) << state.bank_account
              << ", кредит использован=" << state.credit_used << "\n";
    std::cout << "  Всего выручки=" << state.total_revenue << ", всего расходов=" << state.total_expenses
              << ", налогов уплачено=" << state.total_tax_paid << "\n";
}

bool parse_yesno(const std::string& s) {
    if (s.empty()) return false;
    char c = std::tolower(s[0]);
    return c == 'y' || c == 'д'; // 'д' для русского "да"
}

int main(int argc, char** argv) {
    Config cfg;
    bool demo = false;
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--demo" || arg == "-d") demo = true;
    }

    if (demo) {
        run_demo(cfg, 10);
        return 0;
    }

    ModelState state = initialize(cfg);
    std::cout << "Интерактивный режим. Вводите данные перед каждым днём моделирования.\n";
    int total_days = cfg.days;
    for (int step = 0; step < total_days; ++step) {
        std::cout << "\n---\n";
        std::cout << "День " << (state.day + 1) << ". Текущие параметры: баланс=" << std::fixed << std::setprecision(2)
                  << state.bank_account << ", базовый склад=" << state.base_stock << ", магазин=" << state.store_stock << "\n";

        std::string line;
        int tr = 0;
        std::cout << "Объём перевозки (0 - нет): ";
        std::getline(std::cin, line);
        if (!line.empty()) {
            std::istringstream iss(line);
            if (!(iss >> tr)) tr = 0;
        }

        std::cout << "Купить мелко-оптовую партию? (Д/Н): ";
        std::getline(std::cin, line);
        bool buy = parse_yesno(line);

        double price = cfg.base_price;
        std::cout << "Цена продажи за единицу (рекомендуемая " << cfg.base_price << "): ";
        std::getline(std::cin, line);
        if (!line.empty()) {
            std::istringstream iss(line);
            if (!(iss >> price)) price = cfg.base_price;
        }

        DayResult dr = simulate_day(state, cfg, tr, buy, price);
        pretty_print_day(dr);
    }

    std::cout << "Моделирование завершено.\n";
    std::cout << "Итоговый Баланс=" << std::fixed << std::setprecision(2) << state.bank_account
              << ", кредит=" << state.credit_used << ", налогов уплачено=" << state.total_tax_paid << "\n";
    return 0;
}
