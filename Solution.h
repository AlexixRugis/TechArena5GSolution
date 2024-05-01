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

  vector<int> userStarts;

  MaskedInterval(int start_, int end_) {
    start = start_;
    end = end_;
    mask = 0;
  }

  bool CanAddUser(const UserInfo& u) {
    return (mask & (1 << u.beam)) == 0;
  }

  void AddUserUnordered(const UserInfo& u, int userStart) {
    users.push_back(u.id);
    userStarts.push_back(userStart);
    mask |= (1 << u.beam);
  }

  void AddUserSorted(const UserInfo& u, int userStart, vector<UserInfo>& userInfos) {
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

size_t GetSplitIndex(const MaskedInterval& interval, int lossThreshold, const vector<UserInfo>& users) {
  size_t startSplitIndex = 0;
  for (; startSplitIndex < interval.users.size(); startSplitIndex++) {
    /*
    \         .
     \        .
       \      .
          \   |-------------| loss
    ------------------------- startSplitIndex
              \
              .       \
              .             \
    все что справа от черты округляется вправо, слева округляется по черте
    до разделения округление было вправо для всех
    */

    int loss = interval.end - min(interval.end, interval.userStarts[startSplitIndex] + users[interval.users[startSplitIndex]].rbNeed);
    //cumulativeLoss += loss;
    if (loss >= lossThreshold) {
      break;
    }
  }
  return startSplitIndex;
}

/// <summary>
/// Разделяет интервал пополам для экономии места
/// |-----|    |--|  |
/// |-----| -> |--|  |
/// |-----|    |-----|
/// |-----|    |-----|
/// </summary>
bool TrySplitInterval(vector<MaskedInterval>& intervals, int index, int lossThreshold, const vector<UserInfo>& users) {
  MaskedInterval& interval = intervals[index];
  int intLen = getIntervalLength(interval);
  if (intLen < 2) return false;

  size_t startSplitIndex = GetSplitIndex(interval, lossThreshold, users);
  if (startSplitIndex == interval.users.size()) return false;

  int midPos = max(interval.start, min(interval.end, interval.userStarts[startSplitIndex] + users[interval.users[startSplitIndex]].rbNeed));
  if (midPos == interval.start || midPos == interval.end) return false;

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
  const int maxAttempts = 8;
  const float lossThresholdMultiplier = 0.69f;

  int attempt = 0;
  size_t userIndex = 0;
  while (userIndex < originalUserInfos.size()) {
    attempt++;
    bool fit = false;
    for (auto& interval : intervals) {
      if (interval.users.size() >= L) continue;
      if (!interval.CanAddUser(userInfos[userIndex])) continue;

      interval.AddUserSorted(userInfos[userIndex], interval.start, originalUserInfos);
      fit = true;
      break;
    }

    if (fit || attempt >= maxAttempts) {
      attempt = 0;
      userIndex++;
    }
    else {
      if (intervals.size() < J) {
        sort(intervals.begin(), intervals.end(), [&](MaskedInterval& i1, MaskedInterval& i2) {
          int length1 = getIntervalLength(i1);
          int length2 = getIntervalLength(i2);
          if (length1 > userInfos[userIndex].rbNeed && length2 > userInfos[userIndex].rbNeed ||
            length1 <= userInfos[userIndex].rbNeed && length2 <= userInfos[userIndex].rbNeed) {
            int splitIndex1 = GetSplitIndex(i1, (int)(min(getIntervalLength(i1), userInfos[userIndex].rbNeed) * lossThresholdMultiplier), originalUserInfos);
            int splitIndex2 = GetSplitIndex(i1, (int)(min(getIntervalLength(i2), userInfos[userIndex].rbNeed) * lossThresholdMultiplier), originalUserInfos);
            return splitIndex1 < splitIndex2;
          }
          return length1 > length2;
          });

        for (size_t j = 0; j < intervals.size(); j++) {
          int lossThreshold = (int)(min(getIntervalLength(intervals[j]), userInfos[userIndex].rbNeed) * lossThresholdMultiplier);
          if (TrySplitInterval(intervals, j, lossThreshold, originalUserInfos)) break;
        }

        sort(intervals.begin(), intervals.end(), sortIntervalsDescending);
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
