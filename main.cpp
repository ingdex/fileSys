#include <iostream>
#include <vector>
#include "filesys.h"

#define DIR 0   //目录文件
#define REG_FILE 1  //普通文件

using namespace std;

int main() {
    string opStr;
    int op = 1;
    int re = 0;
    string filename = "./testvolume";
    FileSys fileSys(filename);
    cout << " ---------------------------------------" << endl;
    cout << "|  init       mkdir      touch      ls  |" << endl;
    cout << "|  cd         cat        write      rm  |" << endl;
    cout << "|  su         exit                      |" << endl;
    cout << " ---------------------------------------" << endl;
    char type;
    int uid = 0;
    unsigned int curdir = 0; //当前目录inode号
    vector <string> curPath;
    curPath.push_back(string("mainDir"));

    while (op)
    {
        for (int i=0; i<curPath.size(); i++)
        {
            if (i)
            {
                cout << '/';
            }
            cout << curPath[i];
        }
        cout << "$ ";

        cin >> opStr;
        if (opStr == "init")        op = 1;
        else if (opStr == "mkdir")  op = 2;
        else if (opStr == "touch")  op = 3;
        else if (opStr == "ls")     op = 4;
        else if (opStr == "cd")     op = 5;
        else if (opStr == "cat")    op = 6;
        else if (opStr == "write")  op = 7;
        else if (opStr == "rm")     op = 8;
        else if (opStr == "su")     op = 9;
        else if (opStr == "exit")   op = 0;
        else                        op = 100;
        switch (op)
        {
            case 1: //init
                re = fileSys.init();
                if (re == -1)
                {
                    break;
                    //exit(0);
                }
                cout << "init complete" << endl;
                break;
            case 2: //mkdir
                //cout << "filename: ";
                cin >> filename;
                type = DIR;
                re = fileSys.createFile(filename, type, uid, curdir);
                if (re == -1)
                {
                    break;
                }
                break;
            case 3: //touch
                //cout << "filename: ";
                cin >> filename;
                type = REG_FILE;
                re = fileSys.createFile(filename, type, uid, curdir);
                if (re == -1)
                {
                    break;
                }
                break;
            case 4: //ls
                //cout << "displayFile:" << endl;
                re = fileSys.displayFile(curdir);
                if (re == -1)
                {
                    break;
                    //exit(0);
                }
                //cout << "displayFile complete" << endl;
                break;
            case 5: //cd
                cin >> filename;
                re = fileSys.cdDir(filename, curdir);
                if (re == -1)
                {
                    break;
                }
                else if (filename=="../" || filename=="..")
                {
                    curPath.pop_back();
                }
                else if (filename!="./" && filename!=".")
                {
                    curPath.push_back(filename);
                }
                cout << "curdir " << curdir << endl;
                break;
            case 6: //cat
                cin >> filename;
                re = fileSys.catFile(curdir, filename);
                if (re == -1)
                {
                    break;
                }
                break;
            case 7: //writeFile
                cin >> filename;
                re = fileSys.writeFile(curdir, filename);
                break;
//            case 8: //rm
//                cin >> filename;
//                re = fileSys.rmFile(curdir, filename);
//                break;
//            case 9: //su
//                re = fileSys.login();
//                break;
            case 0:
                break;
            default:
                cout << "no such operation" << endl;
                break;
        }
    }

    return 0;
}

