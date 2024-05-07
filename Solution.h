﻿#pragma once
#pragma optimize("O3")

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

struct UserInfoComparator {
    bool operator()(const UserInfo& a, const UserInfo& b) const {
        return a.rbNeed > b.rbNeed;
    }
};

float loss_threshold_multiplier_A = -0.172f;
float loss_threshold_multiplier_B = 0.906f;

map<int, int> test_metrics;

vector<UserInfo> user_data;
vector<pair<int, int>> user_intervals;

void setHyperParams(float a, float b) {
    loss_threshold_multiplier_A = a;
    loss_threshold_multiplier_B = b;
}

map<int, int>& getTestMetrics() {
    return test_metrics;
}

struct MaskedInterval : public Interval {

    unsigned int mask = 0;
    MaskedInterval(int start, int end) : Interval(start, end) {}

    int getLength() const {
        return end - start;
    }

    bool hasMaskCollision(const UserInfo& user) const {
        return (mask & (1 << user.beam)) != 0;
    }

    void insertSplitUser(const UserInfo& user) {
        users.push_back(user.id);
        mask |= (1 << user.beam);
    }

    int getLoss() const {
        int loss = 0;
        for (int user : users) {
            loss += end - min(end, user_intervals[user].second);
        }

        return loss;
    }

    void insertNewUser(const UserInfo& user) {
        if (user_intervals[user.id].first != -1) {
            throw "Error in the function \"insertNewUser\": user_intervals[user.id].first == -1";
        }

        // Вставка с сортировкой
        int i = 0;
        for (; i < users.size(); ++i) {
            int new_user_bound = min(end, start + user.rbNeed);
            int old_user_bound = min(end, user_intervals[users[i]].second);
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

            mask ^= (1 << user_data[user_id].beam);
            users.erase(users.begin() + index);
        }

        insertNewUser(user);

        return deferred_index;
    }

    pair<int, int> getInsertionProfit(const UserInfo& user, int L) const {
        if (users.size() == 0) {
            return { min(end - start, user.rbNeed), 0 };
        }

        if (hasMaskCollision(user)) {
            for (int i = 0; i < users.size(); ++i) {
                int user_id = users[i];
                if (user_data[user_id].beam == user.beam) {
                    return { min(end, start + user.rbNeed) - min(end, user_intervals[user_id].second), i };
                }
            }
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
            for (int i = 0; i < users.size(); ++i) {
                int user_id = users[i];
                if (user_data[user_id].beam == user.beam) {
                    int old_user_bound = user_intervals[user_id].first + user_data[user_id].rbNeed;
                    int new_user_bound = start + user.rbNeed;
                    if (old_user_bound <= end || !canBeDeferred(i)) return { -1, -1 };
                    return { max(0, old_user_bound - new_user_bound), i };
                }
            }
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

bool sortUsersByRbNeedDescendingComp(const UserInfo& U1, const UserInfo& U2) {
    return U1.rbNeed > U2.rbNeed;
}

bool sortIntervalsDescendingComp(const MaskedInterval& I1, const MaskedInterval& I2) {
    return I1.getLength() > I2.getLength();
}

vector<MaskedInterval> getNonReservedIntervals(const vector<Interval>& reserved, int M) {

    int start = 0;
    vector<MaskedInterval> result;
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

int getSplitIndex(const MaskedInterval& interval, float loss_threshold) {

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

        int loss = interval.end - min(interval.end, user_intervals[interval.users[start_split_index]].second);
        if (loss > loss_threshold) {
            break;
        }
    }

    return start_split_index;
}

pair<int, int> getSplitPositionAndIndex(vector<MaskedInterval>& intervals, int index, float loss_threshold) {
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
bool trySplitInterval(vector<MaskedInterval>& intervals, int index, float loss_threshold) {

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

    intervals[index] = intL;
    intervals.push_back(intR);

    return true;
}

float getLossThresholdMultiplier(int user_index, int users_count) {
    float x = (float)user_index / users_count;
    return loss_threshold_multiplier_A * x + loss_threshold_multiplier_B;
}

bool tryReplaceUser(vector<MaskedInterval>& intervals, const UserInfo& user, int replace_threshold, int overfill_threshold, int L, set<UserInfo, UserInfoComparator>& deferred, bool reinsert) {

    int best_index = -1;
    int best_overfill = INT_MAX;
    pair<float, int> best_profit = { 0, -1 };

    for (int i = 0; i < intervals.size(); ++i) {
        int overfill = user.rbNeed - intervals[i].getLength();
        if (overfill > overfill_threshold) continue;
        pair<int, int> profit = intervals[i].getInsertionProfit(user, L);
        float coef = 0.5f + 0.5f * sqrt(sqrt(1.1f - (float)i / intervals.size()));
        float scaledProfit = profit.first * coef;
        if (scaledProfit > best_profit.first || scaledProfit == best_profit.first && overfill < best_overfill) {
            best_overfill = overfill;
            best_profit = { scaledProfit, profit.second };
            best_index = i;
        }
    }

    if (best_profit.first > replace_threshold && best_index != -1) {
        int deferred_index = intervals[best_index].replaceUser(user, best_profit.second);
        if (reinsert && deferred_index >= 0) {
            deferred.insert(user_data[deferred_index]);
        }
        return true;
    }

    return false;
}

bool tryReduceUser(vector<MaskedInterval>& intervals, const UserInfo& user, int replace_threshold, set<UserInfo, UserInfoComparator>& deferred) {

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

        deferred.insert(user_data[deferred_index]);
        return true;
    }

    return false;
}

int findInsertIndex(vector<MaskedInterval>& intervals, const UserInfo& user, int L) {

    int first_or_shortest = -1;
    int min_filled = INT_MAX;
    for (int i = 0; i < intervals.size(); ++i) {

        auto& interval = intervals[i];
        if (interval.users.size() >= L || interval.hasMaskCollision(user)) continue;

        if (interval.getLength() >= user.rbNeed && interval.users.size() < min_filled) {
            first_or_shortest = i;
            min_filled = (int)interval.users.size();
        }
    }

    return first_or_shortest;
}

int findIntervalToSplit(vector<MaskedInterval>& intervals, const UserInfo& user, float loss_threshold_multiplier, int L) {

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

bool splitRoutine(vector<MaskedInterval>& intervals, const UserInfo& user, int index, float loss_threshold_multiplier) {
    if (index == -1) {
        throw "Error in the function \"splitRoutine\": index == -1";
    }

    // здесь почему то нужно ограничивать а при поиске нет, надо бы разобраться почему
    float lossThreshold = min(intervals[index].getLength(), user.rbNeed) * loss_threshold_multiplier;
    trySplitInterval(intervals, index, lossThreshold);
    sort(intervals.begin(), intervals.end(), sortIntervalsDescendingComp);

    return false;
}

vector<Interval> realSolver(int N, int M, int K, int J, int L, vector<MaskedInterval> reservedRBs, const vector<UserInfo>& userInfos);

float checker(int N, int M, int K, int J, int L, const vector<Interval>& reserved, const vector<Interval>& output) {
    
    int output_score = 0;
    int max_user_score = 0;

    unordered_map<int, int> user_metrics;
    for (const auto& interval : output) {
        for (int user_id : interval.users) {
            user_metrics[user_id] += interval.end - interval.start;
        }
    }

    int max_test_score = M;
    for (const auto& R : reserved) {
        max_test_score -= R.end - R.start;
    }
    max_test_score *= L;

    for (const auto& U : user_data) {
        max_user_score += U.rbNeed;
        output_score += min(U.rbNeed, user_metrics[U.id]);
    }

    int totalScore = min(max_user_score, max_test_score);
    float testScore = output_score * 100.0f / (float)totalScore;

    return testScore;
}

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

    bool random_enable = true;

    srand((unsigned int)time(0));

    user_data = userInfos;

    vector<MaskedInterval> intervals = getNonReservedIntervals(reservedRBs, M);
    sort(intervals.begin(), intervals.end(), sortIntervalsDescendingComp);

    int best_test_index = 1;

    float best_value = 0;
    vector<UserInfo> userInfosMy;
    vector<Interval> result;

    // Просчёт с просто отсортированными отрезками
    try {
        userInfosMy = userInfos;
        sort(userInfosMy.begin(), userInfosMy.end(), sortUsersByRbNeedDescendingComp);
        result = realSolver(N, M, K, J, L, intervals, userInfosMy);
        best_value = checker(N, M, K, J, L, reservedRBs, result);
    }
    catch (...) {}

    // Попытки улучшить
    float curr_value = 0;
    vector<Interval> temp;

    //#2 - Инверсия блоков длины 4 в отсортированном массиве
    try {
        userInfosMy = userInfos;
        sort(userInfosMy.begin(), userInfosMy.end(), sortUsersByRbNeedDescendingComp);
        for (int i = 0; i < userInfosMy.size(); i += 4) {
            if (i + 3 < userInfosMy.size()) {
                swap(userInfosMy[i], userInfosMy[i + 3]);
                swap(userInfosMy[i + 1], userInfosMy[i + 2]);
            }
        }
        temp = realSolver(N, M, K, J, L, intervals, userInfosMy);
        curr_value = checker(N, M, K, J, L, reservedRBs, temp);
        if (curr_value > best_value) {
            best_test_index = 2;
            best_value = curr_value;
            result = temp;
        }
    }
    catch (...) {}

    //#3 - Свапы соседних в отсортированном массиве
    try {
        userInfosMy = userInfos;
        sort(userInfosMy.begin(), userInfosMy.end(), sortUsersByRbNeedDescendingComp);
        for (int i = 0; i < userInfosMy.size(); i += 2) {
            if (i + 1 < userInfosMy.size()) {
                swap(userInfosMy[i], userInfosMy[i + 1]);
            }
        }
        temp = realSolver(N, M, K, J, L, intervals, userInfosMy);
        curr_value = checker(N, M, K, J, L, reservedRBs, temp);
        if (curr_value > best_value) {
            best_test_index = 3;
            best_value = curr_value;
            result = temp;
        }
    }
    catch (...) {}

    //#4 - Инверсия блоков длины 3 в отсортированном массиве
    try {
        userInfosMy = userInfos;
        sort(userInfosMy.begin(), userInfosMy.end(), sortUsersByRbNeedDescendingComp);
        for (int i = 0; i < userInfosMy.size(); i += 3) {
            if (i + 2 < userInfosMy.size()) {
                swap(userInfosMy[i], userInfosMy[i + 2]);
            }
        }
        temp = realSolver(N, M, K, J, L, intervals, userInfosMy);
        curr_value = checker(N, M, K, J, L, reservedRBs, temp);
        if (curr_value > best_value) {
            best_test_index = 4;
            best_value = curr_value;
            result = temp;
        }
    }
    catch (...) {}

    //#5 - Хитрая инверсия блоков длины 6 в отсортированном массиве
    try {
        userInfosMy = userInfos;
        sort(userInfosMy.begin(), userInfosMy.end(), sortUsersByRbNeedDescendingComp);
        for (int i = 0; i < userInfosMy.size(); i += 6) {
            if (i + 5 < userInfosMy.size()) {
                swap(userInfosMy[i], userInfosMy[i + 5]);
                //swap(userInfosMy[i + 1], userInfosMy[i + 4]);
                swap(userInfosMy[i + 2], userInfosMy[i + 3]);
            }
        }
        temp = realSolver(N, M, K, J, L, intervals, userInfosMy);
        curr_value = checker(N, M, K, J, L, reservedRBs, temp);
        if (curr_value > best_value) {
            best_test_index = 5;
            best_value = curr_value;
            result = temp;
        }
    }
    catch (...) {}

    //#6 - random_shuffle блоков длины curr_size = 5 в отсортированном массиве
    try {
        int curr_size = 5;
        for (int j = 0; j < 3 && random_enable; j++) {
            userInfosMy = userInfos;
            sort(userInfosMy.begin(), userInfosMy.end(), sortUsersByRbNeedDescendingComp);
            auto it = userInfosMy.begin();
            for (int i = 0; i < userInfosMy.size(); i += curr_size, it += curr_size) {
                if (i + curr_size < userInfosMy.size()) {
                    random_shuffle(it, it + curr_size);
                }
            }
            temp = realSolver(N, M, K, J, L, intervals, userInfosMy);
            curr_value = checker(N, M, K, J, L, reservedRBs, temp);
            if (curr_value > best_value) {
                best_test_index = 6;
                best_value = curr_value;
                result = temp;
            }
        }
    }
    catch (...) {}

    //#7 - random_shuffle блоков длины user.size() / 4 в отсортированном массиве
    try {
        for (int j = 0; j < 3 && random_enable; j++) {
            userInfosMy = userInfos;
            sort(userInfosMy.begin(), userInfosMy.end(), sortUsersByRbNeedDescendingComp);
            auto it = userInfosMy.begin();
            int d = max(2, (int)userInfos.size() / 4);
            for (int i = 0; i < userInfosMy.size(); i += d, it += d) {
                if (i + d < userInfosMy.size()) {
                    random_shuffle(it, it + d);
                }
            }
            temp = realSolver(N, M, K, J, L, intervals, userInfosMy);
            curr_value = checker(N, M, K, J, L, reservedRBs, temp);
            if (curr_value > best_value) {
                best_test_index = 7;
                best_value = curr_value;
                result = temp;
            }
        }
    }
    catch (...) {}

    ++test_metrics[best_test_index];

    return result;
}

vector<Interval> realSolver(int N, int M, int K, int J, int L, vector<MaskedInterval> intervals, const vector<UserInfo>& user_infos) {

    user_intervals.assign(user_infos.size(), { -1, -1 });

    set<UserInfo, UserInfoComparator> deferred;

    // Какие-то параметры, которые возможно влияют на точность
    const int max_attempts = 3;
    int attempt = 0;

    int user_index = 0;
    while (user_index < user_infos.size()) {

        ++attempt;
        bool inserted = false;
        const UserInfo& user = user_infos[user_index];

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
                bool result = tryReplaceUser(intervals, *it, 0, 250, L, deferred, true);
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
                deferred.insert(user);
            }
            attempt = 0;
            ++user_index;
        }
        else {
            if (intervals[0].getLength() > user_infos[user_index].rbNeed && intervals.size() < J) {
                float loss_threshold_multiplier = getLossThresholdMultiplier(user_index, (int)user_data.size());

                // Заменить слишком больших пользователей на тех кто поменьше и разделить
                int split_index = findIntervalToSplit(intervals, user, loss_threshold_multiplier, L);
                if (split_index != -1) {
                    auto it = deferred.begin();
                    while (it != deferred.end()) {
                        bool result = tryReduceUser(intervals, *it, 0, deferred);
                        auto last_it = it;
                        ++it;
                        if (result) {
                            deferred.erase(last_it);
                        }
                    }
                    splitRoutine(intervals, user, split_index, loss_threshold_multiplier);
                    continue;
                }
            }

            // Так плохо писать, но код выполнится, только если разделение не произошло
            attempt = max_attempts + 1;
        }
    }

    // оптимизация перезаполнения
    {
        int max_cycles = 100;
        while (max_cycles--) {
            bool success = false;
            auto it = deferred.begin();
            while (it != deferred.end()) {
                bool result = tryReduceUser(intervals, *it, 0, deferred);
                auto last_it = it;
                ++it;
                if (result) {
                    deferred.erase(last_it);
                    success = true;
                }
            }
            if (!success) break;
        }
    }

    // Последняя попытка вставить
    for (int i = 0; i < 5 * max_attempts; ++i) {

        // довставить
        bool success = false;
        auto it = deferred.begin();
        while (it != deferred.end()) {
            bool result = tryReplaceUser(intervals, *it, 0, 10000, L, deferred, true);
            auto last_it = it;
            ++it;
            if (result) {
                success = true;
                deferred.erase(last_it);
            }
        }
        if (success) continue;

        if (intervals.size() >= J || deferred.size() == 0) break;
        float loss_threshold_multiplier = getLossThresholdMultiplier((int)user_data.size() - (int)deferred.size(), (int)user_data.size());

        int split_index = findIntervalToSplit(intervals, *deferred.begin(), loss_threshold_multiplier, L);
        if (split_index >= 0) {       
            splitRoutine(intervals, *deferred.begin(), split_index, loss_threshold_multiplier);
        }
        else {
            deferred.erase(deferred.begin());
        }
    }

    // Формируем ответ
    vector<Interval> answer(J);
    int j = 0;
    for (int i = 0; j < J && i < intervals.size(); ++i) {
        if (intervals[i].users.size() > 0) {
            answer[j++] = intervals[i];
        }
    }

    while (answer.size() > j) {
        answer.pop_back();
    }

    return answer;
}