#pragma once

#include <vector>
#include <unordered_map>
#include <queue>
#include <algorithm>
#include <climits>

using namespace std;

float lossThresholdMultiplierA = 0.0314823f;
float lossThresholdMultiplierB = 0.75f;

void SetHyperParams(float a, float b) {
  lossThresholdMultiplierA = a;
  lossThresholdMultiplierB = b;
}

struct Interval {
  int start, end;
  vector<int> users;
};


struct UserInfo {
  int rbNeed;
  int beam;
  int id;
};

int getIntervalLength(const Interval& i) {
  return i.end - i.start;
}

struct MaskedInterval : public Interval {
  unsigned int mask;

  vector<int> userStarts;

  MaskedInterval(int start_, int end_) {
    start = start_;
    end = end_;
    mask = 0;
  }

  bool HasMaskCollision(const UserInfo& u) const {
    return (mask & (1 << u.beam)) != 0;
  }

  void AddUserUnordered(const UserInfo& u, int userStart) {
    users.push_back(u.id);
    userStarts.push_back(userStart);
    mask |= (1 << u.beam);
  }

  void AddUserSorted(const UserInfo& u, int userStart, const vector<UserInfo>& userInfos) {
    // вставка с сортировкой
    int i = 0;
    for (; i < users.size(); i++) {
      int newUserBound = min(end, userStart + u.rbNeed);
      int oldUserBound = min(end, userStarts[i] + userInfos[users[i]].rbNeed);
      if (newUserBound >= oldUserBound) break;
    }

    if (i == users.size()) {
      users.push_back(u.id);
      userStarts.push_back(userStart);
    }
    else {
      users.insert(users.begin() + i, u.id);
      userStarts.insert(userStarts.begin() + i, userStart);
    }

    mask |= (1 << u.beam);
  }

  int ReplaceUser(const UserInfo& u, int index, const vector<UserInfo>& userInfos) {
    int canBeDeferred = -1;
    if (userStarts[index] == start && userInfos[users[index]].rbNeed <= getIntervalLength(*this)) canBeDeferred = users[index];

    mask ^= (1 << userInfos[users[index]].beam);
    users.erase(users.begin() + index);
    userStarts.erase(userStarts.begin() + index);

    AddUserSorted(u, start, userInfos);

    return canBeDeferred;
  }

  pair<int, int> GetInsertionProfit(const UserInfo& user, const vector<UserInfo>& userInfos) const {
    if (users.size() == 0) {
      return { min(end - start, user.rbNeed), 0 };
    }

    if (HasMaskCollision(user)) {
      for (size_t i = 0; i < users.size(); i++) {
        int u = users[i];
        if (userInfos[u].beam == user.beam) {
          return { min(end, start + user.rbNeed) - min(end, userStarts[i] + userInfos[u].rbNeed), i };
        }
      }
    }

    int maxProfit = INT_MIN;
    int maxProfitIndex = -1;
    for (size_t i = 0; i < users.size(); i++) {
      int u = users[i];
      
      int profit = min(end, start + user.rbNeed) - min(end, userStarts[i] + userInfos[u].rbNeed);
      if (profit > maxProfit) {
        maxProfit = profit;
        maxProfitIndex = i;
      }
    }

    return { maxProfit, maxProfitIndex };
  }
};

bool sortUsersByRbNeedDescending(const UserInfo& u1, const UserInfo& u2) {
  return u1.rbNeed > u2.rbNeed;
}

bool sortIntervalsDescending(const Interval& i1, const Interval& i2) {
  return getIntervalLength(i1) > getIntervalLength(i2);
}

vector<MaskedInterval> GetUnlockedIntervals(const vector<Interval>& reserved, int m) {
  int start = 0;
  vector<MaskedInterval> res;

  for (size_t i = 0; i < reserved.size(); i++) {
    if (start < reserved[i].start) {
      res.push_back(MaskedInterval(start, reserved[i].start));
      start = reserved[i].end;
    }
  }

  if (start < m) res.push_back({ start, m });

  return res;
}

size_t GetSplitIndex(const MaskedInterval& interval, float lossThreshold, const vector<UserInfo>& users) {
  size_t startSplitIndex = 0;
  for (; startSplitIndex < interval.users.size(); startSplitIndex++) {
    /*
    \         |
     \        |
       \      |
          \   |-------------| loss
    -.-.-.-.-.-.-.-.-.-.-.-.- startSplitIndex
              \
              |       \
              |             \
    все что справа от черты округляется вправо, слева округляется по черте
    до разделения округление было вправо для всех
    */

    int loss = interval.end - min(interval.end, interval.userStarts[startSplitIndex] + users[interval.users[startSplitIndex]].rbNeed);
    if (loss > lossThreshold) {
      break;
    }
  }
  return startSplitIndex;
}

/// <summary>
/// Разделяет интервал для экономии места, при этом счёт не уменьшается
/// |-----|    |--|  |
/// |-----| -> |--|  |
/// |-----|    |-----|
/// |-----|    |-----|
/// </summary>
bool TrySplitInterval(vector<MaskedInterval>& intervals, int index, float lossThreshold, const vector<UserInfo>& users) {
  MaskedInterval& interval = intervals[index];
  int intLen = getIntervalLength(interval);
  if (intLen < 2) return false;

  size_t startSplitIndex = GetSplitIndex(interval, lossThreshold, users);
  if (startSplitIndex == interval.users.size()) return false;

  int midPos = interval.userStarts[startSplitIndex] + users[interval.users[startSplitIndex]].rbNeed;
  if (midPos <= interval.start || midPos >= interval.end) return false;

  MaskedInterval intL(interval.start, midPos);
  MaskedInterval intR(midPos, interval.end);

  for (size_t i = 0; i < startSplitIndex; i++) {
    intL.AddUserUnordered(users[interval.users[i]], interval.userStarts[i]);
    intR.AddUserUnordered(users[interval.users[i]], interval.userStarts[i]);
  }

  for (size_t i = startSplitIndex; i < interval.users.size(); i++) {
    intL.AddUserUnordered(users[interval.users[i]], interval.userStarts[i]);
  }

  intervals[index] = intL;
  intervals.push_back(intR);

  return true;
}

float getLossThresholdMultiplier(int userIndex, int usersCount) {
  float x = (float)userIndex / usersCount;
  return lossThresholdMultiplierA*x*x + lossThresholdMultiplierB;
}

bool TryReplaceUser(vector<MaskedInterval>& intervals, const UserInfo& user, int replaceThreshold, const vector<UserInfo>& userInfos, vector<UserInfo>* deferred) {
  int fitIndex = -1;
  pair<int, int> maxProfit = { 0, -1 };
  for (size_t i = 0; i < intervals.size(); i++) {
    pair<int, int> profit = intervals[i].GetInsertionProfit(user, userInfos);
    if (profit.first > maxProfit.first) {
      maxProfit = profit;
      fitIndex = i;
    }
  }

  if (maxProfit.first > replaceThreshold && fitIndex != -1) {
    int canBeDeferred = intervals[fitIndex].ReplaceUser(user, maxProfit.second, userInfos);
    if (deferred != nullptr && canBeDeferred >= 0) deferred->push_back(userInfos[canBeDeferred]);
    return true;
  }

  return false;
}

int FindInsertIndex(vector<MaskedInterval>& intervals, const UserInfo& user, int L) {
  int firstOrShortest = -1;
  for (size_t i = 0; i < intervals.size(); i++) {
    auto& interval = intervals[i];
    if (interval.users.size() >= L) continue;
    if (interval.HasMaskCollision(user)) continue;

    if (firstOrShortest == -1 || getIntervalLength(interval) >= user.rbNeed)
      firstOrShortest = i;
  }

  return firstOrShortest;
}

void SplitRoutine(vector<MaskedInterval>& intervals, const UserInfo& user,  float ltm, const vector<UserInfo>& userInfos) {
  for (size_t j = 0; j < intervals.size(); j++) {
    float lossThreshold = min(getIntervalLength(intervals[j]), user.rbNeed) * ltm;
    if (TrySplitInterval(intervals, j, lossThreshold, userInfos)) {
      sort(intervals.begin(), intervals.end(), sortIntervalsDescending);
      break;
    }
  }
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
vector<Interval> Solver(int N, int M, int K, int J, int L,
  vector<Interval> reservedRBs,
  vector<UserInfo> userInfos) {
  vector<UserInfo> originalUserInfos = userInfos;

  sort(userInfos.begin(), userInfos.end(), sortUsersByRbNeedDescending);

  vector<MaskedInterval> intervals = GetUnlockedIntervals(reservedRBs, M);
  sort(intervals.begin(), intervals.end(), sortIntervalsDescending);

  vector<UserInfo> deferred;

  // Какие-то параметры, которые возможно влияют на точность
  const int maxAttempts = 4;

  int attempt = 0;
  size_t userIndex = 0;
  while (userIndex < originalUserInfos.size()) {
    attempt++;
    bool inserted = false;
    const UserInfo& user = userInfos[userIndex];
    float ltm = getLossThresholdMultiplier(userIndex, originalUserInfos.size());

    int firstOrShortest = FindInsertIndex(intervals, user, L);

    // если нет пустой ячейки то попробовать заменить что-то
    if (firstOrShortest == -1) {
      inserted = TryReplaceUser(intervals, user, 4, originalUserInfos, &deferred);
    }
    else {
      intervals[firstOrShortest].AddUserSorted(user, intervals[firstOrShortest].start, originalUserInfos);
      inserted = true;
    }

    if (inserted || attempt >= maxAttempts) {
      if (!inserted) deferred.push_back(user);

      attempt = 0;
      userIndex++;
    }
    else {
      if (intervals.size() < J) SplitRoutine(intervals, user, ltm, originalUserInfos);
      else attempt = maxAttempts + 1;
    }
  }
  
  // последняя попытка вставить
  for (const auto& user : deferred) {
    TryReplaceUser(intervals, user, 0, originalUserInfos, nullptr);
  }


  // формируем ответ
  vector<Interval> answer;
  for (size_t i = 0; answer.size() < J && i < intervals.size(); i++) {
    if (intervals[i].users.size() > 0) {
      answer.push_back((Interval)intervals[i]);
    }
  }

  return answer;
}
