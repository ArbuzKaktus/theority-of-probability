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

// Структура для хранения полного состояния игры
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

// Прототип основной рекурсивной функции
void simulate_turn(GameState state, bool is_red_turn, double current_prob);

// Функция для обработки окончания раунда и подсчета очков
void calculate_final_win_prob(const GameState& state, double final_prob) {
    if (state.red_is_back && state.green_is_back) {
        // Оба вернулись, перебираем все возможные значения сокровищ
        long long total_combinations = 0;
        long long red_winning_combinations = 0;

        std::vector<int> red_values_to_sum, green_values_to_sum;

        // Создаем отдельную рекурсивную лямбда-функцию для подсчета всех возможных
        // сумм очков для сокровищ КРАСНОГО игрока.
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

        // То же самое для ЗЕЛЕНОГО игрока.
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

        // Запускаем генерацию сумм
        generate_red_sums(0, 0);
        if (state.red_treasures.empty()) red_values_to_sum.push_back(0);
        
        generate_green_sums(0, 0);
        if (state.green_treasures.empty()) green_values_to_sum.push_back(0);


        // Теперь сравниваем каждую возможную сумму красного с каждой возможной суммой зеленого
        for (int r_sum : red_values_to_sum) {
            for (int g_sum : green_values_to_sum) {
                if ((state.red_score + r_sum) > (state.green_score + g_sum)) {
                    red_winning_combinations++;
                }
            }
        }
        total_combinations = red_values_to_sum.size() * green_values_to_sum.size();
        if (total_combinations > 0) {
            red_win_probability += final_prob * (static_cast<double>(red_winning_combinations) / total_combinations);
        }

    } else {
        // Кислород кончился, но не все вернулись.
        // Очки получают только те, кто уже был в лодке.
        int final_red_score = state.red_score; 
        int final_green_score = state.green_score;
        
        // В этом сценарии сокровища не добавляются, т.к. они потеряны
        if (final_red_score > final_green_score) {
            red_win_probability += final_prob;
        }
    }
}


// Основная рекурсивная функция симуляции
void simulate_turn(GameState state, bool is_red_turn, double current_prob) {
    // Условие отсечения для предотвращения бесконечной рекурсии
    if (current_prob < 1e-9) {
        return;
    }

    // Если оба игрока вернулись, подсчитываем результат
    if (state.red_is_back && state.green_is_back) {
        calculate_final_win_prob(state, current_prob);
        return;
    }

    // Определяем текущего и следующего игрока
    int& current_pos = is_red_turn ? state.red_pos : state.green_pos;
    int& other_pos = is_red_turn ? state.green_pos : state.red_pos;
    std::vector<int>& current_treasures = is_red_turn ? state.red_treasures : state.green_treasures;
    bool& current_player_is_back = is_red_turn ? state.red_is_back : state.green_is_back;

    // Если текущий игрок уже вернулся, ход немедленно переходит к другому
    if (current_player_is_back) {
        simulate_turn(state, !is_red_turn, current_prob);
        return;
    }

    // 1. Расход кислорода
    state.oxygen -= current_treasures.size();

    // 2. Проверка, не кончился ли кислород
    if (state.oxygen <= 0) {
        // Игроки вне лодки теряют сокровища (сбрасываем их счет к начальному)
        if (!state.red_is_back) state.red_score = 21;
        if (!state.green_is_back) state.green_score = 27;
        
        // Раунд окончен
        state.red_is_back = true;
        state.green_is_back = true;
        calculate_final_win_prob(state, current_prob);
        return;
    }

    // 3. Бросок кубиков и перебор всех вариантов
    for (const auto& pair : dice_probabilities) {
        int dice_sum = pair.first;
        double dice_prob = pair.second;
        
        int move_dist = std::max(0, dice_sum - (int)current_treasures.size());
        int new_pos = current_pos - move_dist;

        // Обработка перепрыгивания через другого игрока
        if (current_pos > other_pos && new_pos <= other_pos && other_pos > 0) {
            new_pos--;
        }

        GameState next_state = state;
        int& next_player_pos = is_red_turn ? next_state.red_pos : next_state.green_pos;
        next_player_pos = new_pos;

        // 4. Игрок вернулся на корабль
        if (new_pos <= 0) {
            bool& next_player_is_back = is_red_turn ? next_state.red_is_back : next_state.green_is_back;
            next_player_is_back = true;
            
            // Обновляем очки игрока, который вернулся
            int& score_to_update = is_red_turn ? next_state.red_score : next_state.green_score;
            // Здесь мы НЕ прибавляем очки, это будет сделано в calculate_final_win_prob
            
            simulate_turn(next_state, !is_red_turn, current_prob * dice_prob);
        } 
        // 5. Игрок остановился на клетке
        else {
            // Проверяем, есть ли сокровище на клетке
            if (next_state.board.count(new_pos) && next_state.board.at(new_pos) > 0) {
                // ВАРИАНТ А: Взять сокровище (вероятность 1/2)
                GameState state_after_taking = next_state;
                std::vector<int>& treasures_after_taking = is_red_turn ? state_after_taking.red_treasures : state_after_taking.green_treasures;
                treasures_after_taking.push_back(state_after_taking.board.at(new_pos));
                state_after_taking.board[new_pos] = 0; // клетка становится пустой
                simulate_turn(state_after_taking, !is_red_turn, current_prob * dice_prob * 0.5);

                // ВАРИАНТ Б: Не брать сокровище (вероятность 1/2)
                GameState state_after_not_taking = next_state;
                simulate_turn(state_after_not_taking, !is_red_turn, current_prob * dice_prob * 0.5);
            }
            // Клетка пустая
            else {
                // Если у игрока есть сокровища, он может сбросить одно
                std::vector<int>& current_treasures_for_drop = is_red_turn ? next_state.red_treasures : next_state.green_treasures;
                if (!current_treasures_for_drop.empty()) {
                    // ВАРИАНТ А: Сбросить сокровище (вероятность 1/2)
                    GameState state_after_dropping = next_state;
                    std::vector<int>& treasures_after_dropping = is_red_turn ? state_after_dropping.red_treasures : state_after_dropping.green_treasures;
                    
                    // Находим сокровище наименьшей глубины для сброса
                    auto min_it = std::min_element(treasures_after_dropping.begin(), treasures_after_dropping.end());
                    state_after_dropping.board[new_pos] = *min_it;
                    treasures_after_dropping.erase(min_it);
                    simulate_turn(state_after_dropping, !is_red_turn, current_prob * dice_prob * 0.5);

                    // ВАРИАНТ Б: Не сбрасывать (вероятность 1/2)
                     GameState state_after_not_dropping = next_state;
                     simulate_turn(state_after_not_dropping, !is_red_turn, current_prob * dice_prob * 0.5);
                }
                // Если сокровищ нет, выбора тоже нет
                else {
                    simulate_turn(next_state, !is_red_turn, current_prob * dice_prob);
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

    std::cout << "Calculating win probability for Red player..." << std::endl;

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