#include <iostream>
#include <iomanip>
#include <fstream>
#include "Solution.h"

using namespace std;

#define MAX_TESTS 10

int main()
{
  ifstream ifs("open.txt");
  
  int t;
  ifs >> t;

#ifdef MAX_TESTS
  t = min(t, MAX_TESTS);
#endif // MAX_TESTS

  int testNumber = 1;
  while (t--) {
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

    vector<Interval> output = Solver(n, m, k, j, l, reserved, users);

    cout << "Test: " << testNumber++ << endl;
    cout << "Intervals: " << output.size() << endl;
    cout << left;
    cout << setw(10) << "Begin" << setw(10) << "End" << setw(10) << "Users" << endl;
    for (const auto& interval : output) {
      cout << setw(10) << interval.end << setw(10) << interval.end;
      for (const auto& user : interval.users) {
        cout << user << ' ';
      }
      cout << endl;
    }
  }
  
}
