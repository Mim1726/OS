#include <bits/stdc++.h>
using namespace std;

int main() {
    int n, q;
    cin >> n >> q;

    vector<int> bt(n), rt(n), tickets(n);

    for(int i = 0; i < n; i++) {
        cin >> bt[i] >> tickets[i];
        rt[i] = bt[i];
    }

    srand(time(0));
    vector<int> ct(n, 0);

    int time = 0;

    while(true) {
        int total = 0;
        bool done = true;

        for(int i = 0; i < n; i++) {
            if(rt[i] > 0) {
                total += tickets[i];
                done = false;
            }
        }

        if(done) break;

        int draw = rand() % total;
        int sum = 0;
        int idx = -1;

        for(int i = 0; i < n; i++) {
            if(rt[i] > 0) {
                sum += tickets[i];
                if(draw < sum) {
                    idx = i;
                    break;
                }
            }
        }

        int execute = min(q, rt[idx]);

        rt[idx] -= execute;
        time += execute;

        if(rt[idx] == 0) {
            ct[idx] = time;
        }
    }

    cout << "Process\tBT\tTickets\tCT\tTAT\tWT\n";

    double avgWT = 0, avgTAT = 0;

    for(int i = 0; i < n; i++) {
        int tat = ct[i];          
        int wt = tat - bt[i];

        avgWT += wt;
        avgTAT += tat;

        cout << "P" << i + 1 << "\t"
             << bt[i] << "\t"
             << tickets[i] << "\t"
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