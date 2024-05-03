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

#include "Solution.h"

using namespace std;
using namespace std::chrono;

// [START_TEST, END_TEST], 1 <= START_TEST, defines might be commented
#define START_TEST 1
#define END_TEST 1

const bool LOGS_ENABLED = true;

float run(bool logs_flag) {
    ifstream in("open.txt");

    int __cnt_of_tests__;
    in >> __cnt_of_tests__;

    int __start_test__ = 0;

#ifdef START_TEST
    __start_test__ = START_TEST - 1;
#endif // START_TEST

#ifdef END_TEST
    __cnt_of_tests__ = END_TEST;
#endif // END_TEST

    float allTestsScore = 0.0f;
    for (int __test_case__ = __start_test__; __test_case__ < __cnt_of_tests__; __test_case__++) {
        int N, M, K, J, L;
        in >> N >> M >> K >> J >> L;

        vector<Interval> reserved(K);
        for (int i = 0; i < K; i++) {
            int start, end;
            in >> start >> end;
            reserved[i].start = start;
            reserved[i].end = end;
        }

        vector<UserInfo> users(N);
        for (int i = 0; i < N; i++) {
            int rbNeed, beam;
            in >> rbNeed >> beam;
            users[i].id = i;
            users[i].rbNeed = rbNeed;
            users[i].beam = beam;
        }

        vector<Interval> output = Solver(N, M, K, J, L, reserved, users);

        int outputScore = 0;
        int totalScore = 0;

        map<int, int> userMetrics;
        for (const auto& interval : output) {
            for (auto u : interval.users) {
                userMetrics[u] += interval.end - interval.start;
            }
        }

        for (const auto& u : users) {
            totalScore += u.rbNeed;
            outputScore += min(u.rbNeed, userMetrics[u.id]);
        }

        float testScore = outputScore * 100.0f / totalScore;
        allTestsScore += testScore;

        if (logs_flag) {
            cout << "Test: " << __test_case__ + 1 << " Filled: " << testScore << "%" << '\n';
            cout << "Intervals: " << output.size() << '\n' << left;
            cout << setw(10) << "Begin" << setw(10) << "End" << setw(10) << "Users" << '\n';

            for (const auto& interval : output) {
                cout << setw(10) << interval.start << setw(10) << interval.end;
                for (const auto& user : interval.users) {
                    cout << user << " ";
                }
                cout << '\n';
            }
        }
    }

    return allTestsScore / (__cnt_of_tests__ - __start_test__);
}

int main() {

    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);
    cout.tie(nullptr);

    float p2 = 0.75f;
    float l = -1.0f, r = 1.0f;

    for (int i = 0; i < 10; i++) {
        float m1 = (2 * l + r) / 3;
        float m2 = (l + 2 * r) / 3;

        SetHyperParams(m1, p2);
        float r1 = run(false);
        SetHyperParams(m2, p2);
        float r2 = run(false);

        if (r1 < r2) {
            l = m1;
        }
        else {
            r = m2;
        }
    }

    SetHyperParams((l + r) / 2, p2);

    auto start = high_resolution_clock::now();

    cout << "Average filled: " << run(LOGS_ENABLED) << "%" << '\n';

    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop - start);

    // double because 1000 of 2000 tests
    cout << "Time taken by function: " << duration.count() * 2 << " ms" << '\n';

}