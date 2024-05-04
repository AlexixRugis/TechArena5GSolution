#pragma once
#pragma optimize("O3")

#include <vector>
#include <unordered_map>
#include <queue>
#include <algorithm>
#include <list>
#include <climits>
#include <set>

using namespace std;

struct Interval {
  int start, end;
  vector<int> users;
};


struct UserInfo {
  int rbNeed;
  int beam;
  int id;
};

struct UserInfoComparator {
  bool operator()(const UserInfo& a, const UserInfo& b) const
  {
    return a.rbNeed > b.rbNeed;
  }
};

float lossThresholdMultiplierA = -0.12703f;
float lossThresholdMultiplierB = 0.82f;

vector<UserInfo> userData;
vector<pair<int, int>> userIntervals;

void SetHyperParams(float a, float b) {
  lossThresholdMultiplierA = a;
  lossThresholdMultiplierB = b;
}

struct MaskedInterval : public Interval {
  unsigned int mask;

  MaskedInterval(int start_, int end_) {
    start = start_;
    end = end_;
    mask = 0;
  }

  int getLength() const {
    return end - start;
  }

  bool HasMaskCollision(const UserInfo& u) const {
    return (mask & (1 << u.beam)) != 0;
  }

  void InsertSplitUser(const UserInfo& u) {
    users.push_back(u.id);
    mask |= (1 << u.beam);
  }

  void InsertNewUser(const UserInfo& u) {
    if (userIntervals[u.id].first != -1) throw -1;

    // вставка с сортировкой
    int i = 0;
    for (; i < users.size(); i++) {
      int newUserBound = min(end, start + u.rbNeed);
      int oldUserBound = min(end, userIntervals[users[i]].second);
      if (newUserBound >= oldUserBound) break;
    }

    if (i == users.size())
      users.push_back(u.id);
    else
      users.insert(users.begin() + i, u.id);

    userIntervals[u.id] = { start, min(end, start + u.rbNeed) };
    mask |= (1 << u.beam);
  }

  bool CanBeDeferred(int userIndex) const {
    int uid = users[userIndex];
    return userIntervals[uid].first == start && userIntervals[uid].second <= end;
  }

  int ReplaceUser(const UserInfo& u, int index) {
    int deferredIndex = -1;
    
    if (index < users.size()) {
      if (CanBeDeferred(index)) deferredIndex = users[index];

      int uid = users[index];
      if (userIntervals[uid].second > end) throw -1;
      if (userIntervals[uid].first < start)
        userIntervals[uid].second = start; // оставили только левую часть
      else
        userIntervals[uid] = { -1, -1 }; // пользователь удален полностью

      mask ^= (1 << userData[uid].beam);
      users.erase(users.begin() + index);
    }

    InsertNewUser(u);

    return deferredIndex;
  }

  pair<int, int> GetInsertionProfit(const UserInfo& user, int L) const {
    if (users.size() == 0) {
      return { min(end - start, user.rbNeed), 0 };
    }

    if (HasMaskCollision(user)) {
      for (size_t i = 0; i < users.size(); i++) {
        int uid = users[i];
        if (userData[uid].beam == user.beam) {
          return { min(end, start + user.rbNeed) - min(end, userIntervals[uid].second), i};
        }
      }
    }

    if (users.size() < L) {
      return { min(end - start, user.rbNeed), users.size() };
    }

    int index = users.size() - 1;
    int profit = min(end, start + user.rbNeed) - min(end, userIntervals[users.back()].second);

    return { profit, index };
  }
};

bool sortUsersByRbNeedDescending(const UserInfo& u1, const UserInfo& u2) {
  return u1.rbNeed > u2.rbNeed;
}

bool sortIntervalsDescending(const MaskedInterval& i1, const MaskedInterval& i2) {
  return i1.getLength() > i2.getLength();
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

size_t GetSplitIndex(const MaskedInterval& interval, float lossThreshold) {
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

    int loss = interval.end - min(interval.end, userIntervals[interval.users[startSplitIndex]].second);
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
bool TrySplitInterval(vector<MaskedInterval>& intervals, int index, float lossThreshold) {
  MaskedInterval& interval = intervals[index];
  int intLen = interval.getLength();
  if (intLen < 2) return false;

  size_t startSplitIndex = GetSplitIndex(interval, lossThreshold);
  if (startSplitIndex == interval.users.size()) return false;

  int midPos = userIntervals[interval.users[startSplitIndex]].second;
  if (midPos <= interval.start || midPos >= interval.end) return false;

  MaskedInterval intL(interval.start, midPos);
  MaskedInterval intR(midPos, interval.end);

  for (size_t i = 0; i < startSplitIndex; i++) {
    intL.InsertSplitUser(userData[interval.users[i]]);
    intR.InsertSplitUser(userData[interval.users[i]]);
  }

  for (size_t i = startSplitIndex; i < interval.users.size(); i++) {
    intL.InsertSplitUser(userData[interval.users[i]]);
  }

  intervals[index] = intL;
  intervals.push_back(intR);

  return true;
}

float getLossThresholdMultiplier(int userIndex, int usersCount) {
  float x = (float)userIndex / usersCount;
  return lossThresholdMultiplierA*x + lossThresholdMultiplierB;
}

bool TryReplaceUser(vector<MaskedInterval>& intervals, const UserInfo& user, int replaceThreshold, int L, set<UserInfo, UserInfoComparator>& deferred, bool reinsert) {
  int fitIndex = -1;
  pair<int, int> maxProfit = { 0, -1 };
  for (size_t i = 0; i < intervals.size(); i++) {
    pair<int, int> profit = intervals[i].GetInsertionProfit(user, L);
    if (profit.first >= maxProfit.first) {
      maxProfit = profit;
      fitIndex = i;
    }
  }

  if (maxProfit.first > replaceThreshold && fitIndex != -1) {
    int deferredIndex = intervals[fitIndex].ReplaceUser(user, maxProfit.second);
    if (reinsert && deferredIndex >= 0) {
      deferred.insert(userData[deferredIndex]);
    }
    return true;
  }

  return false;
}

int FindInsertIndex(vector<MaskedInterval>& intervals, const UserInfo& user, int L) {
  int firstOrShortest = -1;
  int minFill = INT_MAX;
  for (size_t i = 0; i < intervals.size(); i++) {
    auto& interval = intervals[i];
    if (interval.users.size() >= L) continue;
    if (interval.HasMaskCollision(user)) continue;

    if (interval.getLength() >= user.rbNeed && interval.users.size() <= minFill) {
      firstOrShortest = i;
      minFill = interval.users.size();
    }
  }

  return firstOrShortest;
}

bool SplitRoutine(vector<MaskedInterval>& intervals, const UserInfo& user,  float ltm) {
  for (size_t j = 0; j < intervals.size(); j++) {
    float lossThreshold = min(intervals[j].getLength(), user.rbNeed) * ltm;
    if (TrySplitInterval(intervals, j, lossThreshold)) {
      sort(intervals.begin(), intervals.end(), sortIntervalsDescending);
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
vector<Interval> Solver(int N, int M, int K, int J, int L,
  vector<Interval> reservedRBs,
  vector<UserInfo> userInfos) {
  userData = userInfos;

  sort(userInfos.begin(), userInfos.end(), sortUsersByRbNeedDescending);

  userIntervals.assign(userInfos.size(), { -1, -1 });

  vector<MaskedInterval> intervals = GetUnlockedIntervals(reservedRBs, M);
  sort(intervals.begin(), intervals.end(), sortIntervalsDescending);

  set<UserInfo, UserInfoComparator> deferred;

  // Какие-то параметры, которые возможно влияют на точность
  const int maxAttempts = 4;

  int attempt = 0;
  size_t userIndex = 0;
  while (userIndex < userData.size()) {
    attempt++;
    bool inserted = false;
    const UserInfo& user = userInfos[userIndex];
    float ltm = getLossThresholdMultiplier(userIndex, userData.size());

    int insertionIndex = FindInsertIndex(intervals, user, L);

    // если нет пустой ячейки то попробовать заменить что-то
    if (insertionIndex == -1) {
      auto it = deferred.begin();
      while (it != deferred.end()) {
        bool res = TryReplaceUser(intervals, *it, 0, L, deferred, true);
        auto lastIt = it;
        it++;
        if (res) deferred.erase(lastIt);
      }
    }
    else {
      intervals[insertionIndex].InsertNewUser(user);
      inserted = true;
    }

    if (inserted || attempt >= maxAttempts) {
      if (!inserted) deferred.insert(user);

      attempt = 0;
      userIndex++;
    }
    else {
      if (intervals.size() < J) {
        SplitRoutine(intervals, user, ltm);
      }
      else attempt = maxAttempts + 1;
    }
  }
  
  // последняя попытка вставить
  for (int i = 0; i < maxAttempts; i++) {
    auto it = deferred.begin();
    while (it != deferred.end()) {
      bool res = TryReplaceUser(intervals, *it, 0, L, deferred, true);
      auto lastIt = it;
      it++;
      if (res) deferred.erase(lastIt);
    }
    if (intervals.size() >= J || deferred.size() == 0) break;
    float ltm = getLossThresholdMultiplier(userData.size() - deferred.size(), userData.size());
    
    if (!SplitRoutine(intervals, *deferred.begin(), ltm)) {
      deferred.erase(deferred.begin());
    }
  }

  // формируем ответ
  vector<Interval> answer;
  for (size_t i = 0; answer.size() < J && i < intervals.size(); i++) {
    if (intervals[i].users.size() > 0) {
      answer.push_back(intervals[i]);
    }
  }

  return answer;
}
