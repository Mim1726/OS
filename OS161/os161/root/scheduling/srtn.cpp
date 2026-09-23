#include <bits/stdc++.h>
using namespace std;
//shortest remaining time next
int main() {
    int n;
    cin >> n;

    vector<int> at(n), bt(n), rt(n), ct(n);

    for(int i = 0; i < n; i++) {
        cin >> at[i] >> bt[i];
        rt[i] = bt[i];
    }

    int completed = 0, time = 0;

    while(completed < n) {
        int idx = -1, mn = INT_MAX;

        for(int i = 0; i < n; i++) {
            if(at[i] <= time && rt[i] > 0 && rt[i] < mn) {
                mn = rt[i];
                idx = i;
            }
        }

        if(idx != -1) {
            rt[idx]--;

            if(rt[idx] == 0) {
                ct[idx] = time + 1;
                completed++;
            }
        }

        time++;
    }

    cout << "Process\tAT\tBT\tCT\tTAT\tWT\n";
    double avgWT = 0, avgTAT = 0;


    for(int i = 0; i < n; i++) {
        int tat = ct[i] - at[i];
        int wt = tat - bt[i];

        avgWT += wt;
        avgTAT += tat;

        cout << "P" << i + 1 << "\t"
             << at[i] << "\t"
             << bt[i] << "\t"
             << ct[i] << "\t"
             << tat << "\t"
             << wt << "\n";
    }

    cout << "\nAverage Waiting Time = "
         << avgWT / n << endl;

    cout << "Average Turnaround Time = "
         << avgTAT / n << endl;

    return 0;
}