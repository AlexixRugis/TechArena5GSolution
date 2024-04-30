#pragma once

#include <vector>
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

  sort(userInfos.begin(), userInfos.end(), sortUsersByRbNeedDescending);

  vector<MaskedInterval> intervals = getFreeIntervals(reservedRBs, M);
  sort(intervals.begin(), intervals.end(), sortIntervalsDescending);

  for (size_t i = 0; i < userInfos.size(); i++) {
    for (auto& interval : intervals) {
      if (interval.users.size() >= L) continue;
      if (!interval.CanAddUser(userInfos[i])) continue;

      interval.AddUser(userInfos[i]);
      break;
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
