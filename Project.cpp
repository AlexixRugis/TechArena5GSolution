#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <map>
#include <algorithm>
#include "Solution.h"

using namespace std;

//#define MAX_TESTS 5
//#define SINGLE_TEST 1000

//#define LOG_ENABLED

float run() {
  ifstream ifs("open.txt");

  int t;
  ifs >> t;

#ifdef MAX_TESTS
  t = min(t, MAX_TESTS);
#endif // MAX_TESTS

  float allTestsScore = 0.0f;
  for (size_t ti = 0; ti < t; ti++) {
    int n, m, k, j, l;
    ifs >> n >> m >> k >> j >> l;

    vector<Interval> reserved(k);
    for (size_t i = 0; i < k; i++) {
      int start, end;
      ifs >> start >> end;
      reserved[i].start = start;
      reserved[i].end = end;
    }

    vector<UserInfo> users(n);
    for (size_t i = 0; i < n; i++) {
      int rbNeed, beam;
      ifs >> rbNeed >> beam;
      users[i].id = i;
      users[i].rbNeed = rbNeed;
      users[i].beam = beam;
    }

#ifdef SINGLE_TEST

    if (ti + 1 != SINGLE_TEST) {
      continue;
    }

#endif // SINGLE_TEST

    vector<Interval> output = Solver(n, m, k, j, l, reserved, users);

    int outputScore = 0;
    int totalScore = 0;
    map<int, int> userMetrics;
    for (const auto& interval : output) {
      for (auto u : interval.users) userMetrics[u] += interval.end - interval.start;
    }
    for (const auto& u : users) {
      totalScore += u.rbNeed;
      outputScore += min(u.rbNeed, userMetrics[u.id]);
    }
    float testScore = outputScore * 100.0f / totalScore;
    allTestsScore += testScore;

#ifdef LOG_ENABLED
    cout << "Test: " << ti + 1 << " Filled: " << testScore << "%\n";
    cout << "Intervals: " << output.size() << endl;
    cout << left;
    cout << setw(10) << "Begin" << setw(10) << "End" << setw(10) << "Users" << endl;
    for (const auto& interval : output) {
      cout << setw(10) << interval.start << setw(10) << interval.end;
      for (const auto& user : interval.users) {
        cout << user << ' ';
      }
      cout << endl;
    }
#endif // LOG_ENABLED

  }

  return allTestsScore / t; 
}

int main()
{
  float p2 = 0.75f;
  float l = -1.0f, r = 1.0f;
  for (int i = 0; i < 10; i++) {
    float m1 = l + (r - l) / 3;
    float m2 = l + 2 * (r - l) / 3;

    SetHyperParams(m1, p2);
    float r1 = run();
    SetHyperParams(m2, p2);
    float r2 = run();

    if (r1 < r2) {
      l = m1;
    }
    else {
      r = m2;
    }
  }

  cout << (l + r) / 2 << endl;
  SetHyperParams((l + r) / 2, p2);
  cout << "Average filled: " << run() << "%\n";
}
