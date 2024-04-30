#pragma once

#include <vector>

struct Interval {
  int start, end;
  std::vector<int> users;
};

struct UserInfo {
  int rbNeed;
  int beam;
  int id;
};

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
std::vector<Interval> Solver(int N, int M, int K, int J, int L,
  std::vector<Interval> reservedRBs,
  std::vector<UserInfo> userInfos) {
  std::vector<Interval> answer;
  //your algorithm 
  answer.push_back({
    1, 10,
    {0, 1 , 2, 3}
    });
  return answer;
}
