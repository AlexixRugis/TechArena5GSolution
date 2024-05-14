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
#define END_TEST 1000

const bool LOGS_ENABLED = false;

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

    float all_tests_score = 0.0f;
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

        if (logs_flag) {
            cout << "Test: " << __test_case__ + 1 << '\n';
        }

        vector<Interval> output = Solver(N, M, K, J, L, reserved, users);

        int output_score = 0;
        int max_user_score = 0;

        map<int, int> user_metrics;
        for (const auto& interval : output) {
            for (int user_id : interval.users) {
                user_metrics[user_id] += interval.end - interval.start;
            }
        }

        int max_test_score = M;
        for (const auto& R : reserved) {
            max_test_score -= R.end - R.start;
        }
        max_test_score *= L;

        for (const auto& U : users) {
            max_user_score += U.rbNeed;
            output_score += min(U.rbNeed, user_metrics[U.id]);
        }

        int total_score = min(max_user_score, max_test_score);
        float test_score = output_score * 100.0f / total_score;
        all_tests_score += test_score;

        if (logs_flag) {
            cout << "Intervals: " << output.size() << '\n' << left;
            cout << setw(10) << "Begin" << setw(10) << "End" << setw(10) << "Users" << '\n';
            for (const auto& interval : output) {
                cout << setw(10) << interval.start << setw(10) << interval.end;
                for (const auto& user : interval.users) {
                    cout << user << " ";
                }
                cout << '\n';
            }

            cout << "Filled: " << test_score << "%" << '\n';
        }
    }

    return all_tests_score / (__cnt_of_tests__ - __start_test__);
}

int main() {

    /*
    float d = 1e-1;
    float best = 0;
    float b1, b2;
    int best_at;
    for (int at = 1; at <= 8; at++) {
        setMaxAttempts(at);
        for (float p1 = -2.0f; p1 <= 2.0f; p1 += d) {
            for (float p2 = 0.0f; p2 <= 1.0f; p2 += d) {
                setHyperParams(p1, p2);
                float res = run(false);
                if (res > best) {
                    best = res;
                    b1 = p1;
                    b2 = p2;
                    best_at = at;
                    cout << at << " " << p1 << " " << p2 << " " << res << '\n';
                }
            }
        }
    }
    */

    //float res = 0;
    //for (int i = 0; i < 500; i++) {
    //    res += run(LOGS_ENABLED);
    //}
    //cout << res / 500 << "-middle\n";

    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);
    cout.tie(nullptr);

    auto start_time = high_resolution_clock::now();

    cout << "Average filled: " << run(LOGS_ENABLED) << "%" << '\n';

    auto stop_time = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(stop_time - start_time);

    // double because 1000 of 2000 tests
    cout << "Time x2 taken by function: " << duration.count() * 2 << " ms" << '\n';

    cout << "Bests: " << '\n';
    auto metrics = getTestMetrics();
    set<pair<int, int>> sorted_metrics;
    for (const auto& p : metrics) {
        sorted_metrics.insert(p);
    }

    for (const auto& p : sorted_metrics) {
        cout << p.first << ": " << p.second << '\n';
    }

    cout << "Stress test ";
    float average = 0.0f;
    for (int i = 0; i < 100; i++) {
        average += run(false);
    }
    average /= 100;
    cout << average << endl;
}