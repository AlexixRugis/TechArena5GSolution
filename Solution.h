#pragma once

#include <vector>
#include <unordered_map>
#include <algorithm>

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

struct MaskedInterval : public Interval {
  unsigned int mask;

  MaskedInterval(int start_, int end_) {
    start = start_;
    end = end_;
    mask = 0;
  }

  bool CanAddUser(const UserInfo& u) {
    return (mask & (1 << u.beam)) == 0;
  }

  void AddUser(const UserInfo& u) {
    users.push_back(u.id);
    mask |= (1 << u.beam);
  }
};

/// <summary>
/// Получить длину интервала
/// </summary>
int getIntervalLength(const Interval& i) {
  return i.end - i.start;
}

/// <summary>
/// Сортировка пользователей по убыванию требований
/// </summary>
bool sortUsersByRbNeedDescending(const UserInfo& u1, const UserInfo& u2) {
  return u1.rbNeed > u2.rbNeed;
}

bool sortIntervalsDescending(const Interval& i1, const Interval& i2) {
  return getIntervalLength(i1) > getIntervalLength(i2);
}

/// <summary>
/// Получить интервалы в которые можно вставлять пользователей
/// </summary>
vector<MaskedInterval> getFreeIntervals(const vector<Interval>& reserved, int m) {
  int start = 0;
  vector<MaskedInterval> res;

  for (size_t i = 0; i < reserved.size(); i++) {
    if (start < reserved[i].start) {
      res.push_back(MaskedInterval(start, reserved[i].start));
      start = reserved[i].end;
    }
  }

  if (start < m) {
    res.push_back({ start, m });
  }

  return res;
}


/// <summary>
/// Разделяет интервал пополам для экономии места
/// |-----|    |  |  |
/// |-----| -> |--|--|
/// |-----|    |-----|
/// |-----|    |-----|
/// </summary>
bool TrySplitInterval(vector<MaskedInterval>& intervals, int index, int lossThreshold, const vector<UserInfo>& users) {
  MaskedInterval& interval = intervals[index];
  int intLen = getIntervalLength(interval);
  if (intLen < 2) return false;
  
  int midPos = interval.start + intLen/2;

  int loss = 0;
  for (const auto& u : interval.users) {
    // сколько потеряется очков, если пользователь помещен в половину интервала
    loss += max(0, min(intLen,users[u].rbNeed) - intLen/2);
  }

  size_t startSplitIndex = 0;
  for (; startSplitIndex < interval.users.size(); startSplitIndex++) {
    if (loss <= lossThreshold) {
      break;
    }

    // пользователь остается в обоих частях разбиения
    int selfLoss = max(0, min(intLen,users[interval.users[startSplitIndex]].rbNeed) - intLen/2);
    loss -= selfLoss;
  }

  if (startSplitIndex == interval.users.size()) return false;

  MaskedInterval int1(interval.start, midPos);
  MaskedInterval int2(midPos, interval.end);
  
  for (size_t i = 0; i < startSplitIndex; i++) {
    int1.AddUser(users[interval.users[i]]);
    int2.AddUser(users[interval.users[i]]);
  }

  bool flag = false;
  for (size_t i = startSplitIndex; i < interval.users.size(); i++) {
    if (flag) int1.AddUser(users[interval.users[i]]);
    else int2.AddUser(users[interval.users[i]]);
    flag = !flag;
  }

  intervals[index] = int1;
  intervals.push_back(int2);

  return true;
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

  vector<MaskedInterval> intervals = getFreeIntervals(reservedRBs, M);
  sort(intervals.begin(), intervals.end(), sortIntervalsDescending);

  // Какие-то параметры, которые возможно влияют на точность
  const int maxAttempts = 10;
  const float lossThresholdMultiplier = 0.88f;

  int attempt = 0;
  size_t userIndex = 0;
  while (userIndex < originalUserInfos.size()) {
    attempt++;
    bool fit = false;
    for (auto& interval : intervals) {
      if (interval.users.size() >= L) continue;
      if (!interval.CanAddUser(userInfos[userIndex])) continue;

      interval.AddUser(userInfos[userIndex]);
      fit = true;
      break;
    }

    if (fit || attempt >= maxAttempts) {
      attempt = 0;
      userIndex++;
    }
    else {
      if (intervals.size() < J) {
        for (size_t j = 0; j < intervals.size(); j++) {
          int lossThreshold = (int)(min(getIntervalLength(intervals[j]) / 2, userInfos[userIndex].rbNeed) * lossThresholdMultiplier);
          if (TrySplitInterval(intervals, j, lossThreshold, originalUserInfos)) {
            sort(intervals.begin(), intervals.end(), sortIntervalsDescending);
            break;
          }
        }
      }
    }
  }
  
  
  vector<Interval> answer;
  for (size_t i = 0; answer.size() < J && i < intervals.size(); i++) {
    if (intervals[i].users.size() > 0) {
      answer.push_back((Interval)intervals[i]);
    }
  }

  return answer;
}
