/*
    Author of the starter code
    Yifan Ren
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 9/15/2024

    Please include your Name, UIN, and the date below
    Name: Robert Longo
    UIN: 632002451
    Date: 9-29-24
*/
#include "common.h"
#include "FIFORequestChannel.h"
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fstream>
#include <sys/time.h>
#include <fcntl.h>

using namespace std;

int main (int argc, char *argv[]) {
    int opt;
    int p = -1;
    double t = -1;
    int e = -1;
    string filename = "";
    int m = MAX_MESSAGE;
    bool newchan = false;
    vector<FIFORequestChannel*> channels;

    // Add other arguments here
    while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
        switch (opt) {
            case 'p':
                p = atoi(optarg);
                break;
            case 't':
                t = atof(optarg);
                break;
            case 'e':
                e = atoi(optarg);
                break;
            case 'f':
                filename = optarg;
                break;
            case 'm':
                m = atoi(optarg);
                break;
            case 'c':
                newchan = true;
                break;
        }
    }

    // Task 1: Run the server process as a child of the client process
    pid_t pid = fork();
    if (pid == 0) {
        // Server arguments: './server', '-m', '<value for -m arg>', 'NULL'
        char* args[] = {
            (char*)"./server",
            (char*)"-m",
            (char*)to_string(m).c_str(),
            NULL
        };
        execvp(args[0], args);
        perror("execvp failed");
        exit(1);
    }

    // Parent process continues
    FIFORequestChannel cont_chan("control", FIFORequestChannel::CLIENT_SIDE);
    channels.push_back(&cont_chan);

    // Task 4: Request a new channel
    if (newchan) {
        MESSAGE_TYPE nc = NEWCHANNEL_MSG;
        cont_chan.cwrite(&nc, sizeof(MESSAGE_TYPE));
        char newchan_name[1024];
        cont_chan.cread(newchan_name, sizeof(newchan_name));
        FIFORequestChannel *new_channel = new FIFORequestChannel(newchan_name, FIFORequestChannel::CLIENT_SIDE);
        channels.push_back(new_channel);
    }

    FIFORequestChannel* chan = channels.back();

    // Task 2: Request data points
    if (!(p == -1 || t == -1 || e == -1)) {
        char buf[MAX_MESSAGE];
        datamsg x(p, t, e);
        memcpy(buf, &x, sizeof(datamsg));
        chan->cwrite(buf, sizeof(datamsg));
        double reply;
        chan->cread(&reply, sizeof(double));
        cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
    } else if (p != -1) {
        ofstream out_file("received/x1.csv", ios::app);
        for (double i = 0; i < 1000; ++i) {
            out_file << (i * 0.004);
            for (int j = 1; j <= 2; ++j) {
                char buf[MAX_MESSAGE];
                datamsg x(p, (i * 0.004), j);
                memcpy(buf, &x, sizeof(datamsg));
                chan->cwrite(buf, sizeof(datamsg));
                double reply;
                chan->cread(&reply, sizeof(double));
                out_file << "," << reply;
            }
            out_file << endl;
        }
        out_file.close();
    }

    // Task 3: Request files
    if (!filename.empty()) {
        struct timeval start, end;
        gettimeofday(&start, NULL);
        filemsg fm(0, 0);
        string fname = filename;

        int len = sizeof(filemsg) + (fname.size() + 1);
        char* buf2 = new char[len];
        memcpy(buf2, &fm, sizeof(filemsg));
        strcpy(buf2 + sizeof(filemsg), fname.c_str());
        chan->cwrite(buf2, len);

        __int64_t file_length;
        chan->cread(&file_length, sizeof(__int64_t));
        cout << "The length of " << fname << " is " << file_length << endl;

        char* buf3 = new char[m];
        for (__int64_t offset = 0; offset < file_length; offset += m) {
            filemsg* file_req = (filemsg*)buf2;
            file_req->offset = offset;
            file_req->length = min(static_cast<__int64_t>(m), file_length - offset);

            chan->cwrite(buf2, len);
            chan->cread(buf3, file_req->length);

            ofstream out_file("received/" + fname, ios::binary | ios::app);
            out_file.write(buf3, file_req->length);
            out_file.close();
        }

        delete[] buf2;
        delete[] buf3;
        gettimeofday(&end, NULL);
        double timeTaken = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) * 1e-6;
        cout << "Time taken to transfer file: " << timeTaken << " seconds." << endl;
    }

    // Task 5: Closing all the channels
    MESSAGE_TYPE q = QUIT_MSG;

    if (newchan) {
        channels.back()->cwrite(&q, sizeof(MESSAGE_TYPE));
        delete channels.back();
        channels.pop_back();
    }

    chan->cwrite(&q, sizeof(MESSAGE_TYPE));
    if (chan != &cont_chan) {
        delete chan;
    }

    // Wait for server process to terminate
    wait(0);

    return 0;
}
