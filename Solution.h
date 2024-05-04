#pragma once
// Одновременно должно быть включено что-то одно
//#pragma GCC target("avx2")
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

using namespace std;

struct Interval {
    int start, end;
    vector<int> users;

    Interval(): start(0), end(0) {}
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

float loss_threshold_multiplier_A = -0.171f;
float loss_threshold_multiplier_B = 0.906f;

vector<UserInfo> user_data;
vector<pair<int, int>> user_intervals;

void setHyperParams(float a, float b) {
    loss_threshold_multiplier_A = a;
    loss_threshold_multiplier_B = b;
}

struct MaskedInterval : public Interval {

    unsigned int mask = 0;
    MaskedInterval(int start, int end): Interval(start, end) {}

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

    void insertNewUser(const UserInfo& user) {
        if (user_intervals[user.id].first != -1) {
            throw "Error in the function \"insertNewUser\": user_intervals[user.id].first == -1";
        }

        // Вставка с сортировкой
        int i = 0;
        for (; i < users.size(); i++) {
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

        user_intervals[user.id] = {start, min(end, start + user.rbNeed)};
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
            for (int i = 0; i < users.size(); i++) {
                int user_id = users[i];
                if (user_data[user_id].beam == user.beam) {
                    return { min(end, start + user.rbNeed) - min(end, user_intervals[user_id].second), i };
                }
            }
        }

        if (users.size() < L) {
            return { min(end - start, user.rbNeed), users.size() };
        }

        int index = users.size() - 1;
        int profit = min(end, start + user.rbNeed) - min(end, user_intervals[users.back()].second);

        return { profit, index };
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
    vector<MaskedInterval> res;
    for (size_t i = 0; i < reserved.size(); i++) {
        if (start < reserved[i].start) {
            res.push_back(MaskedInterval(start, reserved[i].start));
            start = reserved[i].end;
        }
    }

    if (start < M) {
        res.push_back({ start, M });
    }

    return res;
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

/// <summary>
/// Разделяет интервал для экономии места, при этом счёт не уменьшается
/// |-----|    |--|  |
/// |-----| -> |--|  |
/// |-----|    |-----|
/// |-----|    |-----|
/// </summary>
bool trySplitInterval(vector<MaskedInterval>& intervals, int index, float loss_threshold) {

    MaskedInterval& interval = intervals[index];
    int length = interval.getLength();
    if (length < 2) {
        return false;
    }

    int start_split_index = getSplitIndex(interval, loss_threshold);
    if (start_split_index == interval.users.size()) {
        return false;
    }

    int middle_position = user_intervals[interval.users[start_split_index]].second;
    if (middle_position <= interval.start || middle_position >= interval.end) {
        return false;
    }

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

bool tryReplaceUser(vector<MaskedInterval>& intervals, const UserInfo& user, int replace_threshold, int L, set<UserInfo, UserInfoComparator>& deferred, bool reinsert) {
    
    int best_index = -1;
    pair<int, int> best_profit = { INT_MIN, -1 };
    for (int i = 0; i < intervals.size(); ++i) {
        pair<int, int> profit = intervals[i].getInsertionProfit(user, L);
        if (profit.first >= best_profit.first) {
            best_profit = profit;
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

int findInsertIndex(vector<MaskedInterval>& intervals, const UserInfo& user, int L) {

    int first_or_shortest = -1;
    int min_filled = INT_MAX;
    for (int i = 0; i < intervals.size(); ++i) {

        auto& interval = intervals[i];
        if (interval.users.size() >= L || interval.hasMaskCollision(user)) continue;

        if (interval.getLength() >= user.rbNeed && interval.users.size() <= min_filled) {
            first_or_shortest = i;
            min_filled = interval.users.size();
        }
    }

    return first_or_shortest;
}

bool splitRoutine(vector<MaskedInterval>& intervals, const UserInfo& user, float loss_threshold_multiplier) {

    for (int i = 0; i < intervals.size(); ++i) {

        float lossThreshold = min(intervals[i].getLength(), user.rbNeed) * loss_threshold_multiplier;
        if (trySplitInterval(intervals, i, lossThreshold)) {
            sort(intervals.begin(), intervals.end(), sortIntervalsDescendingComp);
            return true;
        }
    }

    return false;
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
    
    user_data = userInfos;
    sort(userInfos.begin(), userInfos.end(), sortUsersByRbNeedDescendingComp);
    user_intervals.assign(userInfos.size(), { -1, -1 });

    vector<MaskedInterval> intervals = getNonReservedIntervals(reservedRBs, M);
    sort(intervals.begin(), intervals.end(), sortIntervalsDescendingComp);

    set<UserInfo, UserInfoComparator> deferred;

    // Какие-то параметры, которые возможно влияют на точность
    const int max_attempts = 4;
    int attempt = 0;

    int user_index = 0;
    while (user_index < user_data.size()) {

        attempt++;
        bool inserted = false;
        const UserInfo& user = userInfos[user_index];

        float loss_threshold_multiplier = getLossThresholdMultiplier(user_index, user_data.size());
        int insertion_index = findInsertIndex(intervals, user, L);

        // Eсли нет пустой ячейки то попробовать заменить что-то
        if (insertion_index == -1) {
            auto it = deferred.begin();
            while (it != deferred.end()) {
                bool result = tryReplaceUser(intervals, *it, 0, L, deferred, true);
                auto last_it = it;
                ++it;
                if (result) {
                    deferred.erase(last_it);
                }
            }
        }
        else {
            intervals[insertion_index].insertNewUser(user);
            inserted = true;
        }

        if (inserted || attempt >= max_attempts) {
            if (!inserted) {
                deferred.insert(user);
            }
            attempt = 0;
            user_index++;
        }
        else {
            if (intervals.size() < J) {
                splitRoutine(intervals, user, loss_threshold_multiplier);
            }
            else {
                attempt = max_attempts + 1;
            }
        }
    }

    // Последняя попытка вставить
    for (int i = 0; i < max_attempts; ++i) {

        auto it = deferred.begin();
        while (it != deferred.end()) {
            bool result = tryReplaceUser(intervals, *it, 0, L, deferred, true);
            auto last_it = it;
            ++it;
            if (result) {
                deferred.erase(last_it);
            }
        }
        if (intervals.size() >= J || deferred.size() == 0) break;
        float loss_threshold_multiplier = getLossThresholdMultiplier(user_data.size() - deferred.size(), user_data.size());

        if (!splitRoutine(intervals, *deferred.begin(), loss_threshold_multiplier)) {
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