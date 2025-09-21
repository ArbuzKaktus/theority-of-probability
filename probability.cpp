#include <iostream>
#include <vector>
#include <map>
#include <numeric>
#include <algorithm>
#include <iomanip>
#include <functional>

// Глобальная переменная для накопления вероятности победы красного
double red_win_probability = 0.0;

// Определяем возможные значения для сокровищ каждой глубины
const std::map<int, std::vector<int>> treasure_values = {
    {1, {0, 1, 2, 3}},
    {2, {4, 5, 6, 7}},
    {3, {8, 9, 10, 11}},
    {4, {11, 12, 13, 14}}
};

// Вероятности выпадения суммы на двух d3 кубиках
const std::map<int, double> dice_probabilities = {
    {2, 1.0 / 9.0}, {3, 2.0 / 9.0}, {4, 3.0 / 9.0},
    {5, 2.0 / 9.0}, {6, 1.0 / 9.0}
};

struct GameState {
    int oxygen;
    int red_pos;
    int green_pos;
    int red_score;
    int green_score;
    std::vector<int> red_treasures;
    std::vector<int> green_treasures;
    std::map<int, int> board;
    bool red_is_back = false;
    bool green_is_back = false;
};

void calculate_final_win_prob(GameState state, double final_prob) {
    // Если кислород кончился, любой игрок, который НЕ находится в лодке, теряет свои сокровища.
    if (state.oxygen <= 0) {
        if (!state.red_is_back) {
            state.red_treasures.clear();
        }
        if (!state.green_is_back) {
            state.green_treasures.clear();
        }
    }

    // 2. ГЕНЕРИРУЕМ ВСЕ ВОЗМОЖНЫЕ СТОИМОСТИ СОХРАНЕННЫХ СОКРОВИЩ
    std::vector<int> red_values_to_sum, green_values_to_sum;

    std::function<void(int, int)> generate_red_sums = 
        [&](int treasure_idx, int current_sum) {
        if (treasure_idx == state.red_treasures.size()) {
            red_values_to_sum.push_back(current_sum);
            return;
        }
        for (int val : treasure_values.at(state.red_treasures[treasure_idx])) {
            generate_red_sums(treasure_idx + 1, current_sum + val);
        }
    };

    std::function<void(int, int)> generate_green_sums = 
        [&](int treasure_idx, int current_sum) {
        if (treasure_idx == state.green_treasures.size()) {
            green_values_to_sum.push_back(current_sum);
            return;
        }
        for (int val : treasure_values.at(state.green_treasures[treasure_idx])) {
            generate_green_sums(treasure_idx + 1, current_sum + val);
        }
    };

    generate_red_sums(0, 0);
    if (state.red_treasures.empty()) red_values_to_sum.push_back(0);
    
    generate_green_sums(0, 0);
    if (state.green_treasures.empty()) green_values_to_sum.push_back(0);

    // 3. СРАВНИВАЕМ ВСЕ ИСХОДЫ И СЧИТАЕМ ПОБЕДЫ КРАСНОГО
    long long red_winning_combinations = 0;
    for (int r_sum : red_values_to_sum) {
        for (int g_sum : green_values_to_sum) {
            if ((state.red_score + r_sum) > (state.green_score + g_sum)) {
                red_winning_combinations++;
            }
        }
    }
    
    long long total_combinations = red_values_to_sum.size() * green_values_to_sum.size();
    if (total_combinations > 0) {
        red_win_probability += final_prob * (static_cast<double>(red_winning_combinations) / total_combinations);
    }
}


void simulate_turn(GameState state, bool is_red_turn, double current_prob) {
    if (current_prob < 1e-9) {
        return;
    }

    if (state.red_is_back && state.green_is_back) {
        calculate_final_win_prob(state, current_prob);
        return;
    }

    // Лямбда-функция для проверки кислорода и передачи хода/завершения раунда.
    auto handle_next_step = [&](GameState resulting_state, double probability_of_this_path) {
        if (resulting_state.oxygen <= 0) {
            // Раунд заканчивается ПОСЛЕ хода текущего игрока
            calculate_final_win_prob(resulting_state, probability_of_this_path);
        } else {
            // Кислород еще есть, передаем ход следующему игроку
            simulate_turn(resulting_state, !is_red_turn, probability_of_this_path);
        }
    };

    bool& current_player_is_back = is_red_turn ? state.red_is_back : state.green_is_back;

    // Если игрок уже вернулся, он не тратит кислород и просто передает ход.
    if (current_player_is_back) {
        simulate_turn(state, !is_red_turn, current_prob);
        return;
    }

    // 1. Расход кислорода в начале хода
    std::vector<int>& current_treasures = is_red_turn ? state.red_treasures : state.green_treasures;
    state.oxygen -= current_treasures.size();

    // 2. Бросок кубиков.
    for (const auto& pair : dice_probabilities) {
        int dice_sum = pair.first;
        double dice_prob = pair.second;
        
        int move_dist = std::max(0, dice_sum - (int)current_treasures.size());
        
        int& current_pos = is_red_turn ? state.red_pos : state.green_pos;
        int& other_pos = is_red_turn ? state.green_pos : state.red_pos;
        int new_pos = current_pos - move_dist;

        if (current_pos > other_pos && new_pos <= other_pos && other_pos > 0) {
            new_pos--;
        }

        GameState next_state = state;
        int& next_player_pos = is_red_turn ? next_state.red_pos : next_state.green_pos;
        next_player_pos = new_pos;

        // 3. Игрок завершает действие
        if (new_pos <= 0) { // Игрок вернулся на корабль
            bool& is_back_flag = is_red_turn ? next_state.red_is_back : next_state.green_is_back;
            is_back_flag = true;
            handle_next_step(next_state, current_prob * dice_prob);
        } 
        else { // Игрок остановился на клетке
            if (next_state.board.count(new_pos) && next_state.board.at(new_pos) > 0) { // Клетка с сокровищем
                // Вариант А: Взять сокровище
                GameState state_after_taking = next_state;
                std::vector<int>& treasures = is_red_turn ? state_after_taking.red_treasures : state_after_taking.green_treasures;
                treasures.push_back(state_after_taking.board.at(new_pos));
                state_after_taking.board[new_pos] = 0;
                handle_next_step(state_after_taking, current_prob * dice_prob * 0.5);

                // Вариант Б: Не брать
                handle_next_step(next_state, current_prob * dice_prob * 0.5);
            }
            else { // Клетка пустая
                std::vector<int>& treasures_to_drop_from = is_red_turn ? next_state.red_treasures : next_state.green_treasures;
                if (!treasures_to_drop_from.empty()) {
                    // Вариант А: Сбросить сокровище
                    GameState state_after_dropping = next_state;
                    std::vector<int>& treasures = is_red_turn ? state_after_dropping.red_treasures : state_after_dropping.green_treasures;
                    auto min_it = std::min_element(treasures.begin(), treasures.end());
                    state_after_dropping.board[new_pos] = *min_it;
                    treasures.erase(min_it);
                    handle_next_step(state_after_dropping, current_prob * dice_prob * 0.5);

                    // Вариант Б: Не сбрасывать
                    handle_next_step(next_state, current_prob * dice_prob * 0.5);
                }
                else { // Сокровищ для сброса нет, выбора тоже нет
                    handle_next_step(next_state, current_prob * dice_prob);
                }
            }
        }
    }
}


int main() {
    // --- Начальное состояние игры ---
    GameState initial_state;
    initial_state.oxygen = 9;
    initial_state.red_pos = 5;
    initial_state.green_pos = 2;
    initial_state.red_score = 21;
    initial_state.green_score = 27;
    initial_state.red_treasures = {4, 2};
    initial_state.green_treasures = {3, 1};
    initial_state.board = {
        {1, 1}, {2, 0}, {3, 1}, {4, 1}, {5, 1}
    };

    std::cout << "Calculating win probability for Red player " << std::endl;

    // Запускаем симуляцию для случая, когда первым ходит красный (вероятность 0.5)
    simulate_turn(initial_state, true, 0.5);
    
    // Запускаем симуляцию для случая, когда первым ходит зеленый (вероятность 0.5)
    simulate_turn(initial_state, false, 0.5);

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "------------------------------------------" << std::endl;
    std::cout << "Total Red Player Win Probability: " << red_win_probability * 100 << "%" << std::endl;
    std::cout << "------------------------------------------" << std::endl;

    return 0;
}