#pragma once

#pragma GCC optimize ("O3,function-inlining")
#pragma GCC target("avx2,bmi,bmi2")

#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <queue>
#include <deque>
#include <bitset>
#include <random>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <climits>
#include <cstdlib>
#include <cinttypes>
#include <utility>
#include <cstdlib>
#include <time.h>
#include <cstdint>
#include <limits.h>
#include <type_traits>

using namespace std;

struct Interval {
    int start, end;
    vector<int> users;

    Interval() : start(0), end(0) {}
    Interval(int start, int end) : start(start), end(end) {}
};


struct UserInfo {
    int rbNeed;
    int beam;
    int id;
};

//float loss_threshold_multiplier_A = -0.172f;
//float loss_threshold_multiplier_B = 0.906f;
//
//int max_attempts = 3;

float loss_threshold_multiplier_A = -0.212f;
float loss_threshold_multiplier_B = 0.92f;

int max_attempts = 4;

unordered_map<int, int> test_metrics;

vector<UserInfo> user_data;

vector<pair<int, int>> user_intervals;

void setHyperParams(float a, float b) {
    loss_threshold_multiplier_A = a;
    loss_threshold_multiplier_B = b;
}

void setMaxAttempts(int V) {
    max_attempts = V;
}

unordered_map<int, int>& getTestMetrics() {
    return test_metrics;
}

struct UserSetComparator {
    bool operator()(int id1, int id2) const {
        const UserInfo& U1 = user_data[id1];
        const UserInfo& U2 = user_data[id2];
        if (U1.rbNeed == U2.rbNeed) {
            return U1.beam > U2.beam;
        }
        return U1.rbNeed > U2.rbNeed;
    }
};

struct MaskedInterval : public Interval {

    unsigned int mask = 0;
    int mask_indices[32];

    MaskedInterval(int start, int end) : Interval(start, end) {}

    int getLength() const {
        return end - start;
    }

    bool hasMaskCollision(const UserInfo& user) const {
        return (mask & (1 << user.beam)) != 0;
    }

    void eraseUser(int user_id) {
        if (!hasMaskCollision(user_data[user_id])) return;

        int beam = user_data[user_id].beam;
        int index = mask_indices[beam];
        if (users[index] != user_id) return;
        mask ^= (1 << beam);
        users.erase(users.begin() + index);
        for (int i = 0; i < users.size(); ++i) mask_indices[user_data[users[i]].beam] = i;
    }

    void insertSplitUser(const UserInfo& user) {
        if (user_intervals[user.id].first == -1) {
            throw "Error in the function \"insertSplitUser\": user_intervals[user.id].first != -1";
        }

        users.push_back(user.id);
        mask |= (1 << user.beam);
        mask_indices[user.beam] = users.size() - 1;
    }

    void insertNewUser(const UserInfo& user) {
        if (user_intervals[user.id].first != -1) {
            throw "Error in the function \"insertNewUser\": user_intervals[user.id].first == -1";
        }

        // Вставка с сортировкой
        int i = 0;
        for (; i < users.size(); ++i) {
            int new_user_bound = start + user.rbNeed;
            int old_user_bound = user_intervals[users[i]].second;
            if (new_user_bound >= old_user_bound) {
                break;
            }
        }

        if (i == users.size()) {
            users.push_back(user.id);
        }
        else {
            users.insert(users.begin() + i, user.id);
        }


        user_intervals[user.id] = { start, min(end, start + user.rbNeed) };

        mask |= (1 << user.beam);
        for (int i = 0; i < users.size(); ++i) mask_indices[user_data[users[i]].beam] = i;
    }

    bool canBeDeferred(int user_index) const {
        int user_id = users[user_index];
        if (user_intervals[user_id].first == start && user_intervals[user_id].second <= end) {
            return true;
        }
        return false;
    }

    int replaceUser(const UserInfo& user, int index) {

        int deferred_index = -1;
        if (index < users.size()) {
            if (canBeDeferred(index)) {
                deferred_index = users[index];
            }

            int user_id = users[index];
            if (user_intervals[user_id].second > end) {
                throw "Error in the function \"replaceUser\": user_intervals[user_id].second <= end";
            }

            if (user_intervals[user_id].first < start) {
                user_intervals[user_id].second = start; // Оставили только левую часть
            }
            else {
                user_intervals[user_id] = { -1, -1 }; // Пользователь удален полностью
            }

            mask_indices[user_data[user_id].beam] = -1;
            mask ^= (1 << user_data[user_id].beam);
            users.erase(users.begin() + index);
        }

        insertNewUser(user);

        return deferred_index;
    }

    pair<int, int> getInsertionProfit(const UserInfo& user, int L) const {

        if (hasMaskCollision(user)) {
            int index = mask_indices[user.beam];
            int user_id = users[index];
            return { min(end, start + user.rbNeed) - min(end, user_intervals[user_id].second), index };
        }

        if (users.size() < L) {
            return { min(end - start, user.rbNeed), users.size() };
        }

        int index = (int)users.size() - 1;
        int profit = min(end, start + user.rbNeed) - min(end, user_intervals[users.back()].second);

        return { profit, index };
    }

    pair<int, int> getReduceProfit(const UserInfo& user) const {

        if (user.rbNeed < getLength()) return { -1, -1 };

        if (hasMaskCollision(user)) {
            int index = mask_indices[user.beam];
            int user_id = users[index];
            int old_user_bound = user_intervals[user_id].first + user_data[user_id].rbNeed;
            int new_user_bound = start + user.rbNeed;
            if (old_user_bound <= end || !canBeDeferred(index)) return { -1, -1 };
            return { max(0, old_user_bound - new_user_bound), index };
        }

        for (int i = 0; i < users.size(); ++i) {
            int user_id = users[i];
            int old_user_bound = user_intervals[user_id].first + user_data[user_id].rbNeed;
            int new_user_bound = start + user.rbNeed;
            if (old_user_bound <= end) break;
            if (canBeDeferred(i)) {
                return { max(0, old_user_bound - new_user_bound), i };
            }
        }

        return { -1, -1 };
    }
};

void (*log_step) (const vector<MaskedInterval>& intervals);

inline void setStepLogger(void (*logger) (const vector<MaskedInterval>& intervals)) {
    log_step = logger;
}

inline bool sortUsersByRbNeedDescendingComp(int id1, int id2) {
    const UserInfo& U1 = user_data[id1];
    const UserInfo& U2 = user_data[id2];

    if (U1.rbNeed == U2.rbNeed) {
        if (U1.beam == U2.beam) return id1 > id2;
        return U1.beam > U2.beam;
    }
    return U1.rbNeed > U2.rbNeed;
}

inline bool sortIntervalsDescendingComp(const MaskedInterval& I1, const MaskedInterval& I2) {

    int l1 = I1.getLength(), l2 = I2.getLength();

    if (l1 == l2) {
        int users1 = I1.users.size(), users2 = I2.users.size();
        if (users1 == users2) return I1.end < I2.end;

        return users1 > users2;
    }

    return l1 > l2;
}

inline vector<MaskedInterval> getNonReservedIntervals(const vector<Interval>& reserved, int M) {

    int start = 0;
    vector<MaskedInterval> result; result.reserve(10);
    for (int i = 0; i < reserved.size(); ++i) {
        if (start < reserved[i].start) {
            result.push_back(MaskedInterval(start, reserved[i].start));
            start = reserved[i].end;
        }
    }

    if (start < M) {
        result.push_back({ start, M });
    }

    return result;
}

inline int getSplitIndex(const MaskedInterval& interval, float loss_threshold) {

    int start_split_index = 0;
    for (; start_split_index < interval.users.size(); ++start_split_index) {
        /*
        \         |
         \        |
           \      |
              \   |-------------| loss
        -.-.-.-.-.-.-.-.-.-.-.-.- startSplitIndex
                  \
                  |       \
                  |             \
        Все что справа от черты округляется вправо, слева округляется по черте
        до разделения округление было вправо для всех
        */

        float loss = interval.end - min(interval.end, user_intervals[interval.users[start_split_index]].second);
        if (loss > loss_threshold) {
            break;
        }
    }

    return start_split_index;
}

inline pair<int, int> getSplitPositionAndIndex(vector<MaskedInterval>& intervals, int index, float loss_threshold) {
    MaskedInterval& interval = intervals[index];
    int length = interval.getLength();
    if (length < 2) {
        return { -1, -1 };
    }

    int start_split_index = getSplitIndex(interval, loss_threshold);
    if (start_split_index == interval.users.size()) {
        return { -1, -1 };
    }

    int middle_position = user_intervals[interval.users[start_split_index]].second;
    if (middle_position <= interval.start || middle_position >= interval.end) {
        return { -1, -1 };
    }

    return { middle_position, start_split_index };
}

/// <summary>
/// Разделяет интервал для экономии места, при этом счёт не уменьшается
/// |-----|    |--|  |
/// |-----| -> |--|  |
/// |-----|    |-----|
/// |-----|    |-----|
/// </summary>
inline bool trySplitInterval(vector<MaskedInterval>& intervals, int index, float loss_threshold) {

    pair<int, int> split = getSplitPositionAndIndex(intervals, index, loss_threshold);
    int middle_position = split.first;
    int start_split_index = split.second;
    if (middle_position == -1) return false;

    MaskedInterval& interval = intervals[index];
    MaskedInterval intL(interval.start, middle_position);
    MaskedInterval intR(middle_position, interval.end);

    for (int i = 0; i < start_split_index; ++i) {
        intL.insertSplitUser(user_data[interval.users[i]]);
        intR.insertSplitUser(user_data[interval.users[i]]);
    }

    for (int i = start_split_index; i < interval.users.size(); ++i) {
        intL.insertSplitUser(user_data[interval.users[i]]);
    }


    intervals[index] = intR;
    intervals.push_back(intL);

    return true;
}

inline float getLossThresholdMultiplier(int user_index, int users_count) {
    float x = (float)user_index / users_count;
    return loss_threshold_multiplier_A * x + loss_threshold_multiplier_B;
}

inline bool tryReplaceUser(vector<MaskedInterval>& intervals, const UserInfo& user, int replace_threshold, int overfill_threshold, int L, set<uint8_t, UserSetComparator>& deferred, bool reinsert) {

    int best_index = -1;
    int best_overfill = INT_MAX;
    pair<float, int> best_profit = { 0, -1 };

    for (int i = 0; i < intervals.size(); ++i) {
        int overfill = user.rbNeed - intervals[i].getLength();
        if (overfill > overfill_threshold) continue;
        pair<int, int> profit = intervals[i].getInsertionProfit(user, L);
        float x = (float)i / intervals.size();
        x *= x;
        x *= x;
        x *= x;

        float coef = 1.0f + -x;
        float scaledProfit = profit.first * coef;
        if (scaledProfit > best_profit.first) {
            best_overfill = overfill;
            best_profit = { scaledProfit, profit.second };
            best_index = i;
        }
    }

    if (best_profit.first > replace_threshold && best_index != -1) {
        int deferred_index = intervals[best_index].replaceUser(user, best_profit.second);
        if (reinsert && deferred_index >= 0) {
            deferred.insert(deferred_index);
        }
        return true;
    }

    return false;
}

inline bool tryReduceUser(vector<MaskedInterval>& intervals, const UserInfo& user, int replace_threshold, set<uint8_t, UserSetComparator > & deferred) {

    int best_index = -1;
    pair<int, int> best_profit = { 0, -1 };

    for (int i = 0; i < intervals.size(); ++i) {
        pair<int, int> profit = intervals[i].getReduceProfit(user);
        if (profit.first > best_profit.first) {
            best_profit = profit;
            best_index = i;
        }
    }

    if (best_profit.first > replace_threshold && best_index != -1) {
        int deferred_index = intervals[best_index].replaceUser(user, best_profit.second);
        if (deferred_index == -1) {
            throw "Error in the function \"tryReduceUser\": deferred_index == -1";
        }

        deferred.insert(deferred_index);
        return true;
    }

    return false;
}

inline int findInsertIndex(vector<MaskedInterval>& intervals, const UserInfo& user, int L) {

    int first_or_shortest = -1;
    int min_filled = L;
    for (int i = 0; i < intervals.size(); ++i) {

        auto& interval = intervals[i];
        if (interval.getLength() < user.rbNeed) break;
        if (interval.hasMaskCollision(user)) continue;

        if (interval.users.size() < min_filled) {
            first_or_shortest = i;
            min_filled = interval.users.size();
        }
    }

    return first_or_shortest;
}

inline int findIntervalToSplit(vector<MaskedInterval>& intervals, const UserInfo& user, float loss_threshold_multiplier, int L) {

    float minPosition = 1000;
    int optimal_index = -1;
    for (int i = 0; i < intervals.size(); i++) {
        float loss_threshold = user.rbNeed * loss_threshold_multiplier;
        auto res = getSplitPositionAndIndex(intervals, i, loss_threshold);

        if (res.first != -1) {
            float value = res.second;
            if (value < minPosition) {
                minPosition = value;
                optimal_index = i;
            }
        }
    }
    return optimal_index;
}


inline bool splitRoutine(vector<MaskedInterval>& intervals, const UserInfo& user, int index, float loss_threshold_multiplier) {
    if (index == -1) {
        throw "Error in the function \"splitRoutine\": index == -1";
    }

    // Здесь почему то нужно ограничивать а при поиске нет, надо бы разобраться почему
    float lossThreshold = min(intervals[index].getLength(), user.rbNeed) * loss_threshold_multiplier;
    trySplitInterval(intervals, index, lossThreshold);
    sort(intervals.begin(), intervals.end(), sortIntervalsDescendingComp);

    return false;
}

inline void reinsertRoutine(vector<MaskedInterval>& intervals, int N, int L) {
    // оптимизация перевставкой
    for (size_t i = 0; i < N; i++) {
        if (user_intervals[i].first == -1) continue;
        int fill = user_intervals[i].second - user_intervals[i].first;
        if (fill >= user_data[i].rbNeed) continue;

        int max_profit = 0;
        int optimal_index = -1;
        for (size_t j = 0; j < intervals.size(); ++j) {
            auto& interval = intervals[j];
            if (interval.getLength() <= fill) break;
            if (interval.users.size() >= L || interval.hasMaskCollision(user_data[i])) continue;

            int new_length = min(user_data[i].rbNeed, interval.getLength());
            int profit = new_length - fill;
            if (profit >= max_profit) {
                max_profit = profit;
                optimal_index = j;
            }
        }

        if (optimal_index == -1) continue;

        // delete
        for (auto& interval : intervals) interval.eraseUser(i);
        user_intervals[i] = { -1, -1 };
        intervals[optimal_index].insertNewUser(user_data[i]);
    }
}

inline int getMaxTestScore(int M, int L, const vector<Interval>& reserved) {

    int max_test_score = M;
    for (const auto& R : reserved) {
        max_test_score -= R.end - R.start;
    }

    return max_test_score;
}

inline bool move_bounds_right(vector<MaskedInterval>& intervals, vector<pair<int, int>> actual_user_intervals) {
    vector<int> right_intervals(actual_user_intervals.size());
    bool success = false;

    for (size_t i = 0; i < intervals.size(); ++i) {
        for (auto u : intervals[i].users) right_intervals[u] = i;
    }

    for (size_t i = 1; i < intervals.size(); ++i) {
        MaskedInterval& right = intervals[i];
        MaskedInterval& left = intervals[i - 1];

        if (left.end != right.start || right.getLength() <= 1) {
            continue;
        }

        int rightLoss = 0;
        for (auto u : right.users) {
            if (actual_user_intervals[u].first == -1) throw - 1;
            if (actual_user_intervals[u].second == intervals[right_intervals[u]].end) rightLoss++;
        }
        int leftProfit = 0;
        for (auto u : left.users) {
            if (actual_user_intervals[u].first == -1) throw - 1;
            if (actual_user_intervals[u].second != left.end) continue;
            int fill = actual_user_intervals[u].second - actual_user_intervals[u].first;
            if (fill < user_data[u].rbNeed) leftProfit++;
        }

        if (leftProfit > rightLoss) {
            // move right
            for (auto u : left.users)
                if (actual_user_intervals[u].second = left.end)
                    if (actual_user_intervals[u].second - actual_user_intervals[u].first < user_data[u].rbNeed)
                        actual_user_intervals[u].second++;

            for (auto u : right.users) {
                if (actual_user_intervals[u].first == right.start) {
                    actual_user_intervals[u].first++;
                    actual_user_intervals[u].second = min(intervals[right_intervals[u]].end, actual_user_intervals[u].second + 1);
                }
            }

            left.end++;
            right.start++;
            success = true;
        }
    }

    return success;
}

inline bool move_bounds_left(vector<MaskedInterval>& intervals, vector<pair<int, int>>& actual_user_intervals) {
    // двигать границы интервалов
    bool success = false;
    for (size_t i = intervals.size() - 1; i >= 1; --i) {
        MaskedInterval& right = intervals[i];
        MaskedInterval& left = intervals[i - 1];

        if (left.end != right.start || left.getLength() <= 1) {
            continue;
        }

        int leftLoss = 0;
        for (auto u : left.users) {
            if (actual_user_intervals[u].first == -1) throw -1;
            if (actual_user_intervals[u].second == left.end) leftLoss++;
        }
        int rightProfit = 0;
        for (auto u : right.users) {
            if (actual_user_intervals[u].first == -1) throw -1;
            if (actual_user_intervals[u].first != right.start) continue;
            int fill = actual_user_intervals[u].second - actual_user_intervals[u].first;
            if (fill < user_data[u].rbNeed) rightProfit++;
        }

        if (rightProfit > leftLoss) {
            // move left
            for (auto u : left.users)
                if (actual_user_intervals[u].second == left.end)
                    --actual_user_intervals[u].second;
            for (auto u : right.users)
                if (actual_user_intervals[u].first == right.start) {
                    --actual_user_intervals[u].first;
                    if (actual_user_intervals[u].second - actual_user_intervals[u].first > user_data[u].rbNeed)
                        --actual_user_intervals[u].second;
                }
            left.end--;
            right.start--;
            success = true;
        }
    }
    return success;
}

inline float checker(int N, int M, int K, int J, int L, int max_test_score_row) {

    int output_score = 0;
    int max_user_score = 0;

    for (size_t i = 0; i < N; ++i) {
        max_user_score += min(max_test_score_row, user_data[i].rbNeed);

        if (user_intervals[i].first != -1) {
            output_score += user_intervals[i].second - user_intervals[i].first;
        }
    }

    int totalScore = min(max_user_score, max_test_score_row * L);
    float testScore = output_score * 100.0f / (float)totalScore;

    return testScore;
}

void riffle_shuffle(vector<uint8_t>& vec, int startIndex, int endIndex) {
    int i = (startIndex + endIndex) / 2;
    int j = endIndex - 1;
    while (i > startIndex) {
        uint8_t temp = vec[i];
        vec[i--] = vec[j];
        vec[j--] = temp;
    }
}

void shuffle(vector<uint8_t>& vec, int startIndex, int endIndex) {
    int length = endIndex - startIndex;
    for (int i = endIndex - 1; i > startIndex; --i) {
        int index = rand() % length;
        uint8_t temp = vec[i];
        vec[i] = vec[startIndex + index];
        vec[startIndex + index] = temp;
    }
}

vector<MaskedInterval> realSolver(int N, int M, int K, int J, int L, vector<MaskedInterval> reservedRBs, const vector<uint8_t>& user_infos);

/// <summary>
/// Функция решения задачи
/// </summary>
/// <param name="N">Количество пользователей</param>
/// <param name="M">Количество блоков передачи данных</param>
/// <param name="K">Количество зарезервированных интервалов передачи данных</param>
/// <param name="J">Максимальное количество интервалов с пользователями</param>
/// <param name="L">Максимальное количество пользователей на одном интервале</param>
/// <param name="reservedRBs">Зарезервированные интервалы</param>
/// <param name="userInfos">Информация о пользователях</param>
/// <returns>Интервалы передачи данных, до J штук</returns>
vector<Interval> Solver(int N, int M, int K, int J, int L, vector<Interval> reservedRBs, vector<UserInfo> userInfos) {

    bool random_enable = false;

    srand((unsigned int)time(0));

    int max_test_score = getMaxTestScore(M, L, reservedRBs);

    user_data = userInfos;

    vector<uint8_t> userIndices(N);
    for (int i = 0; i < N; ++i) userIndices[i] = userInfos[i].id;
    sort(userIndices.begin(), userIndices.end(), sortUsersByRbNeedDescendingComp);

    vector<MaskedInterval> intervals = getNonReservedIntervals(reservedRBs, M);
    sort(intervals.begin(), intervals.end(), sortIntervalsDescendingComp);

    int best_test_index = 1;

    float best_value = 0;
    vector<uint8_t> userInfosMy;
    vector<MaskedInterval> result;
    vector<pair<int, int>> actual_user_intervals;

    // Просчёт с просто отсортированными отрезками
    try {
        userInfosMy = userIndices;
        result = realSolver(N, M, K, J, L, intervals, userInfosMy);
        best_value = checker(N, M, K, J, L, max_test_score);
        actual_user_intervals = user_intervals;
    }
    catch (...) {}

    // Попытки улучшить
    float curr_value = 0;
    vector<MaskedInterval> temp;

    //#2 - Инверсия блоков длины 4 в отсортированном массиве
    try {
        userInfosMy = userIndices;
        for (int i = 0; i < userInfosMy.size(); i += 4) {
            if (i + 3 < userInfosMy.size()) {
                swap(userInfosMy[i], userInfosMy[i + 3]);
                swap(userInfosMy[i + 1], userInfosMy[i + 2]);
            }
        }
        temp = realSolver(N, M, K, J, L, intervals, userInfosMy);
        curr_value = checker(N, M, K, J, L, max_test_score);
        if (curr_value > best_value) {
            best_test_index = 2;
            best_value = curr_value;
            result = temp;
            actual_user_intervals = user_intervals;

        }
    }
    catch (...) {}

    ////#3 - Свапы соседних в отсортированном массиве
    //try {
    //    userInfosMy = userInfos;
    //    for (int i = 0; i < userInfosMy.size(); i += 2) {
    //        if (i + 1 < userInfosMy.size()) {
    //            swap(userInfosMy[i], userInfosMy[i + 1]);
    //        }
    //    }
    //    temp = realSolver(N, M, K, J, L, intervals, userInfosMy);
    //    curr_value = checker(N, M, K, J, L, max_test_score);
    //    if (curr_value > best_value) {
    //        best_test_index = 3;
    //        best_value = curr_value;
    //        result = temp;
    //    }
    //}
    //catch (...) {}

    ////#4 - Инверсия блоков длины 3 в отсортированном массиве
    //try {
    //    userInfosMy = userInfos;
    //    for (int i = 0; i < userInfosMy.size(); i += 3) {
    //        if (i + 2 < userInfosMy.size()) {
    //            swap(userInfosMy[i], userInfosMy[i + 2]);
    //        }
    //    }
    //    temp = realSolver(N, M, K, J, L, intervals, userInfosMy);
    //    curr_value = checker(N, M, K, J, L, max_test_score);
    //    if (curr_value > best_value) {
    //        best_test_index = 4;
    //        best_value = curr_value;
    //        result = temp;
    //    }
    //}
    //catch (...) {}

    ////#5 - Хитрая инверсия блоков длины 6 в отсортированном массиве
    //try {
    //    userInfosMy = userInfos;
    //    for (int i = 0; i < userInfosMy.size(); i += 6) {
    //        if (i + 5 < userInfosMy.size()) {
    //            swap(userInfosMy[i], userInfosMy[i + 5]);
    //            swap(userInfosMy[i + 1], userInfosMy[i + 4]);
    //            swap(userInfosMy[i + 2], userInfosMy[i + 3]);
    //        }
    //    }
    //    temp = realSolver(N, M, K, J, L, intervals, userInfosMy);
    //    curr_value = checker(N, M, K, J, L, max_test_score);
    //    if (curr_value > best_value) {
    //        best_test_index = 5;
    //        best_value = curr_value;
    //        result = temp;
    //    }
    //}
    //catch (...) {}


    //#6 - random_shuffle блоков разной в отсортированном массиве
    try {
        for (int j = 0; j < 10 && random_enable; j++) {
            userInfosMy = userIndices;
            int curr_size = j + 3;
            for (int i = 0; i < userInfosMy.size(); i += curr_size) {
                if (i + curr_size < userInfosMy.size()) {
                    shuffle(userInfosMy, i, i + curr_size);
                }
            }
            temp = realSolver(N, M, K, J, L, intervals, userInfosMy);
            curr_value = checker(N, M, K, J, L, max_test_score);
            if (curr_value > best_value) {
                best_test_index = 6;
                best_value = curr_value;
                result = temp;
                actual_user_intervals = user_intervals;

            }
        }
        for (int j = 0; j < 6 && random_enable; j++) {
            userInfosMy = userIndices;
            int curr_size = j + 3;
            for (int i = 0; i < userInfosMy.size(); i += curr_size) {
                if (i + curr_size < userInfosMy.size()) {
                    shuffle(userInfosMy, i, i + curr_size);
                }
            }
            temp = realSolver(N, M, K, J, L, intervals, userInfosMy);
            curr_value = checker(N, M, K, J, L, max_test_score);
            if (curr_value > best_value) {
                best_test_index = 6;
                best_value = curr_value;
                result = temp;
                actual_user_intervals = user_intervals;
            }
        }

    }
    catch (...) {}

    ////#7 - random_shuffle блоков длины user.size() / 4 в отсортированном массиве
    //try {
    //    for (int j = 0; j < 6 && random_enable; j++) {
    //        userInfosMy = userInfos;
    //        auto it = userInfosMy.begin();
    //        int d = max(2, (int)userInfos.size() / 4);
    //        for (int i = 0; i < userInfosMy.size(); i += d, it += d) {
    //            if (i + d < userInfosMy.size()) {
    //                random_shuffle(it, it + d);
    //            }
    //        }
    //        temp = realSolver(N, M, K, J, L, intervals, userInfosMy);
    //        curr_value = checker(N, M, K, J, L, max_test_score);
    //        if (curr_value > best_value) {
    //            best_test_index = 7;
    //            best_value = curr_value;
    //            result = temp;
    //        }
    //    }
    //}
    //catch (...) {}

    ++test_metrics[best_test_index];

    sort(result.begin(), result.end(), [](const MaskedInterval& l, const MaskedInterval& r) { return l.start < r.start; });
    int max_iterations = 50;
    while (max_iterations-- && move_bounds_left(result, actual_user_intervals)) {}
    max_iterations = 50;
    while (max_iterations-- && move_bounds_right(result, actual_user_intervals)) {}

    // Формируем ответ
    vector<Interval> answer(J);
    int j = 0;
    for (int i = 0; j < J && i < result.size(); ++i) {
        if (result[i].users.size() > 0) {
            answer[j++] = result[i];
        }
    }

    while (answer.size() > j) {
        answer.pop_back();
    }

    return answer;
}

inline vector<MaskedInterval> realSolver(int N, int M, int K, int J, int L, vector<MaskedInterval> intervals, const vector<uint8_t>& user_infos) {

    user_intervals.assign(user_infos.size(), { -1, -1 });

    set<uint8_t, UserSetComparator> deferred;

    int attempt = 0;

    int user_index = 0;
    while (user_index < user_infos.size()) {
        if (log_step != nullptr) log_step(intervals);

        ++attempt;
        bool inserted = false;
        const UserInfo& user = user_data[user_infos[user_index]];

        int insertion_index = findInsertIndex(intervals, user, L);

        // Eсли нет пустой ячейки то попробовать заменить что-то
        if (insertion_index >= 0) {
            intervals[insertion_index].insertNewUser(user);
            inserted = true;
        }
        else {
            bool success = false;
            auto it = deferred.begin();
            while (it != deferred.end()) {
                bool result = tryReplaceUser(intervals, user_data[*it], 0, 250, L, deferred, true);
                auto last_it = it;
                ++it;
                if (result) {
                    success = true;
                    deferred.erase(last_it);
                }
            }
            if (success) continue;
        }

        if (inserted || attempt >= max_attempts) {
            if (!inserted) {
                deferred.insert(user.id);
            }
            attempt = 0;
            ++user_index;
        }
        else {
            if (intervals.size() < J) {
                if (intervals[0].getLength() > user.rbNeed) {
                    float loss_threshold_multiplier = getLossThresholdMultiplier(user_index, N);

                    // Заменить слишком больших пользователей на тех кто поменьше и разделить
                    int split_index = findIntervalToSplit(intervals, user, loss_threshold_multiplier, L);
                    if (split_index != -1) {
                        auto it = deferred.begin();
                        while (it != deferred.end()) {
                            bool result = tryReduceUser(intervals, user_data[*it], 0, deferred);
                            auto last_it = it;
                            ++it;
                            if (result) {
                                deferred.erase(last_it);
                            }
                        }
                        splitRoutine(intervals, user, split_index, loss_threshold_multiplier);
                        reinsertRoutine(intervals, N, L);
                        continue;
                    }
                }
            }

            // Так плохо писать, но код выполнится, только если разделение не произошло
            attempt = max_attempts + 1;
        }
    }

    // оптимизация перезаполнения
    reinsertRoutine(intervals, N, L);

    {
        auto it = deferred.begin();
        while (it != deferred.end()) {
            bool result = tryReduceUser(intervals, user_data[*it], 0, deferred);
            auto last_it = it;
            ++it;
            if (result) {
                deferred.erase(last_it);
            }
        }
    }

    // Последняя попытка вставить
    for (int i = 0; i < 10 * max_attempts; ++i) {

        // довставить
        bool success = false;
        auto it = deferred.begin();
        while (it != deferred.end()) {
            bool result = tryReplaceUser(intervals, user_data[*it], 0, 10000, L, deferred, true);
            auto last_it = it;
            ++it;
            if (result) {
                success = true;
                deferred.erase(last_it);
            }
        }
        if (success) continue;

        if (intervals.size() >= J || deferred.size() == 0) break;
        float loss_threshold_multiplier = getLossThresholdMultiplier(N - (int)deferred.size(), N);

        int split_index = findIntervalToSplit(intervals, user_data[*deferred.begin()], loss_threshold_multiplier, L);
        if (split_index >= 0) {
            splitRoutine(intervals, user_data[*deferred.begin()], split_index, loss_threshold_multiplier);
            reinsertRoutine(intervals, N, L);
        }
        else {
            deferred.erase(deferred.begin());
        }
    }

    return move(intervals);
}