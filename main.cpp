#include <iostream>
#include <vector>
#include <string>
#include <shared_mutex>
#include <mutex>
#include <thread>
#include <fstream>
#include <sstream>
#include <random>
#include <chrono>
#include <iomanip>
#include <algorithm>

using namespace std;

class DataStructure {
private:
    vector<int> data;
    mutable vector<shared_mutex> mutexes;

public:
    DataStructure() : data(2, 0), mutexes(2) {}

    DataStructure(const DataStructure&) = delete;
    DataStructure& operator=(const DataStructure&) = delete;

    int read(int index) const {
        if (index < 0 || index >= 2) return 0;
        shared_lock<shared_mutex> lock(mutexes[index]);
        return data[index];
    }

    void write(int index, int value) {
        if (index < 0 || index >= 2) return;
        unique_lock<shared_mutex> lock(mutexes[index]);
        data[index] = value;
    }


    operator string() const {
        shared_lock<shared_mutex> lock0(mutexes[0]);
        shared_lock<shared_mutex> lock1(mutexes[1]);
        
        return to_string(data[0]) + " " + to_string(data[1]);
    }
};

enum OpType { OP_READ, OP_WRITE, OP_STRING };
struct Operation {
    OpType type;
    int field_index;
    int value;
};

void generate_file(const string& filename, int count, const vector<double>& probs) {
    ofstream file(filename);
    random_device rd;
    mt19937 gen(rd());
    discrete_distribution<> d(probs.begin(), probs.end());
    

    // 0: read 0, 1: write 0, 2: read 1, 3: write 1, 4: string
    
    for(int i = 0; i < count; ++i) {
        int p = d(gen);
        if (p == 0) file << "read 0\n";
        else if (p == 1) file << "write 0 1\n";
        else if (p == 2) file << "read 1\n";
        else if (p == 3) file << "write 1 1\n";
        else file << "string\n";
    }
}

vector<Operation> load_operations(const string& filename) {
    vector<Operation> ops;
    ifstream file(filename);
    string line, cmd;
    while(getline(file, line)) {
        stringstream ss(line);
        ss >> cmd;
        if (cmd == "string") {
            ops.push_back({OP_STRING, -1, 0});
        } else {
            int idx;
            ss >> idx;
            if (cmd == "read") {
                ops.push_back({OP_READ, idx, 0});
            } else if (cmd == "write") {
                int val;
                ss >> val;
                ops.push_back({OP_WRITE, idx, val});
            }
        }
    }
    return ops;
}

void worker(DataStructure& ds, const vector<Operation>& ops) {
    for (const auto& op : ops) {
        switch (op.type) {
            case OP_READ: {
                volatile int val = ds.read(op.field_index);
                break;
            }
            case OP_WRITE:
                ds.write(op.field_index, op.value);
                break;
            case OP_STRING: {
                string s = ds;
                volatile size_t len = s.length(); 
                break;
            }
        }
    }
}

double run_test(DataStructure& ds, int num_threads, const vector<string>& file_names) {
    vector<thread> threads;
    vector<vector<Operation>> all_ops;

    for(int i = 0; i < num_threads; ++i) {
        all_ops.push_back(load_operations(file_names[i]));
    }

    auto start_time = chrono::high_resolution_clock::now();

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker, ref(ds), ref(all_ops[i]));
    }

    for (auto& t : threads) {
        if(t.joinable()) t.join();
    }

    auto end_time = chrono::high_resolution_clock::now();
    return chrono::duration<double>(end_time - start_time).count();
}

int main() {
    vector<double> probs_var = {40, 5, 5, 5, 45}; 
    vector<double> probs_equal = {20, 20, 20, 20, 20};
    vector<double> probs_random = {0, 50, 0, 50, 0};

    int N_OPS = 1000000; 
    

    for(int i=0; i<3; ++i) {
        generate_file("var" + to_string(i) + ".txt", N_OPS, probs_var);
        generate_file("equal_" + to_string(i) + ".txt", N_OPS, probs_equal);
        generate_file("random_" + to_string(i) + ".txt", N_OPS, probs_random);
    }

    cout << "Test running with " << N_OPS << " operations per thread.\n";
    cout << setw(20) << "Scenario" << setw(15) << "1 Thread" << setw(15) << "2 Threads" << setw(15) << "3 Threads" << endl;

    struct Scenario { string name; string prefix; };
    vector<Scenario> scenarios = {
        {"Spesific", "var"},
        {"Equal", "equal_"},
        {"Random", "random_"}
    };

    for (const auto& sc : scenarios) {
        cout << setw(20) << sc.name;
        
        {
            DataStructure ds;
            vector<string> files = {sc.prefix + "0.txt"};
            cout << setw(15) << run_test(ds, 1, files);
        }

        {
            DataStructure ds;
            vector<string> files = {sc.prefix + "0.txt", sc.prefix + "1.txt"};
            cout << setw(15) << run_test(ds, 2, files);
        }

        {
            DataStructure ds;
            vector<string> files = {sc.prefix + "0.txt", sc.prefix + "1.txt", sc.prefix + "2.txt"};
            cout << setw(15) << run_test(ds, 3, files);
        }
        cout << endl;
    }

    return 0;
}