#include <bits/stdc++.h>
using namespace std;

int main() {
    int n, q;
    cin >> n >> q;

    vector<int> bt(n), rt(n), ct(n);

    for(int i = 0; i < n; i++) {
        cin >> bt[i];
        rt[i] = bt[i];
    }

    queue<int> rq;
    for(int i = 0; i < n; i++)
        rq.push(i);

    int time = 0;

    while(!rq.empty()) {
        int i = rq.front();
        rq.pop();

        if(rt[i] > q) {
            rt[i] -= q;
            time += q;
            rq.push(i);
        } else {
            time += rt[i];
            rt[i] = 0;
            ct[i] = time;
        }
    }

    cout << "Process\tBT\tCT\tTAT\tWT\n";
    double avgWT = 0, avgTAT = 0;

    for(int i = 0; i < n; i++) {
        int tat = ct[i];
        int wt = tat - bt[i];

        avgWT += wt;
        avgTAT += tat;

        cout << "P" << i + 1 << "\t"
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