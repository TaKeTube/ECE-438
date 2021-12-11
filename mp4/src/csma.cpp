#include<stdio.h>
#include<string.h>
#include<stdlib.h>

#include<fstream>
#include<iostream>
#include<string>
#include<vector>

using namespace std;

int N,L,M,T;
int num_trans_slots;
vector<int> R;

struct Node {
    int backoff;
    int collision;
    int trans_time;
    bool is_collision;
};

void read_data(char* fname){
    ifstream infile;
    infile.open(fname, ios::in);
    string tmp;
    char* d;
    char R_str[200];
    const char s[2] = " ";

    while(getline(infile, tmp)){
        if(tmp[0] == 'N')
            N = atoi(tmp.substr(2).c_str());
        else if(tmp[0] == 'L')
            L = atoi(tmp.substr(2).c_str());
        else if(tmp[0] == 'M')
            M = atoi(tmp.substr(2).c_str());
        else if(tmp[0] == 'T')
            T = atoi(tmp.substr(2).c_str());
        else if(tmp[0] == 'R'){
            strcpy(R_str ,tmp.substr(2).data());
            d = strtok(R_str, s);
            while (d != NULL)
            {
                R.push_back(atoi(d));
                d = strtok(NULL, s);
            }
        }
    }

    cout << "N: " << N << endl;
    cout << "L: " << L << endl;
    cout << "M: " << M << endl;
    cout << "T: " << T << endl;
    cout << "R: ";
    for(int r : R)
        cout << r << " ";
    cout << endl;
}

void simulation(){
    int tick = 0;
    vector<Node> nodes;

    for(int i = 0; i < N; ++i){
        Node n;
        n.collision = 0;
        n.backoff = i % R[n.collision];
        n.trans_time = L;
        n.is_collision = false;
        nodes.push_back(n);
    }

    while(tick < T){
        vector<int> trans_nodes;
        for(int i = 0; i < N; ++i){
            Node& n = nodes.at(i);
            if(n.trans_time == 0 || n.collision == M){
                n.collision = 0;
                n.backoff = (i + tick) % R[n.collision];
                n.trans_time = L;
            }else if(n.is_collision){
                n.backoff = (i + tick) % R[n.collision];
                n.trans_time = L;
            }
            n.is_collision = false;
            if(n.backoff == 0)
                trans_nodes.push_back(i);
        }
        if(trans_nodes.empty()){
            for(Node& n : nodes)
                n.backoff--;
        }else if(trans_nodes.size() == 1){
            num_trans_slots++;
            nodes.at(trans_nodes.at(0)).trans_time--;
        }else if(trans_nodes.size() > 1){
            for(int idx : trans_nodes){
                Node& n = nodes.at(idx);
                n.collision++;
                n.is_collision = true;
            }
        }
        tick++;
    }
}

int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 2) {
        printf("Usage: ./csma input.txt\n");
        return -1;
    }

    read_data(argv[1]);
    simulation();

    FILE *fpOut;
    fpOut = fopen("output.txt", "w");
    fprintf(fpOut,"%.2f",(float)num_trans_slots/T);
    fclose(fpOut);

    return 0;
}