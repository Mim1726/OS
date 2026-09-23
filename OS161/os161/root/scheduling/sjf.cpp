#include <bits/stdc++.h>
using namespace std;

int main() {
    int n;
    cin >> n;

    vector<int> bt(n), wt(n), tat(n);
    vector<int> id(n);

    for(int i = 0; i < n; i++) {
        cin >> bt[i];
        id[i] = i;
    }

    sort(id.begin(), id.end(),
         [&](int a, int b) {
             return bt[a] < bt[b];
         });

    int time = 0;
    double avgWT = 0, avgTAT = 0;

    for(int i = 0; i < n; i++) {
        wt[id[i]] = time;
        time += bt[id[i]];
        tat[id[i]] = time;

        avgWT += wt[id[i]];
        avgTAT += tat[id[i]];
    }

    cout << "Process\tBT\tWT\tTAT\n";

    for(int i = 0; i < n; i++) {
        cout << "P" << i + 1 << "\t"
             << bt[i] << "\t"
             << wt[i] << "\t"
             << tat[i] << "\n";
    }

    cout << fixed << setprecision(2);

    cout << "\nAverage Waiting Time = "
         << avgWT / n << endl;

    cout << "Average Turnaround Time = "
         << avgTAT / n << endl;

    return 0;
}