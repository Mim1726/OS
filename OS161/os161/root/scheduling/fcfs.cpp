#include <iostream>
using namespace std;

int main() {
    int n;
    cin >> n;

    int bt[n], wt[n], tat[n];

    for(int i = 0; i < n; i++)
        cin >> bt[i];

    wt[0] = 0;

    for(int i = 1; i < n; i++) {
        wt[i] = wt[i - 1] + bt[i - 1];
    }

    double avgWT = 0, avgTAT = 0;

    for(int i = 0; i < n; i++) {
        tat[i] = wt[i] + bt[i];

        avgWT += wt[i];
        avgTAT += tat[i];
    }

    cout << "Process\tBT\tWT\tTAT\n";

    for(int i = 0; i < n; i++) {
        cout << "P" << i + 1 << "\t"
             << bt[i] << "\t"
             << wt[i] << "\t"
             << tat[i] << "\n";
    }

    cout << "\nAverage Waiting Time = "
         << avgWT / n << endl;

    cout << "Average Turnaround Time = "
         << avgTAT / n << endl;

    return 0;
}