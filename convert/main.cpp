#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <cstdint>

using namespace std;

struct Note {
    int freq;
    uint32_t startTime;
    uint16_t duration;
};

int main() {
    // Đổi tên file dưới đây thành tên file text của bạn nếu cần
    ifstream infile("output (1).txt");
    if (!infile.is_open()) {
        cout << "Lỗi: Không thể mở file. Hãy kiểm tra lại tên file." << endl;
        return 1;
    }

    vector<Note> track1, track2, track3;
    string line;

    while (getline(infile, line)) {
        if (line.empty()) continue;

        stringstream ss(line);
        string motorStr, freq
        Str, startStr, durStr;

        // Tách chuỗi theo dấu phẩy
        getline(ss, motorStr, ',');
        getline(ss, freqStr, ',');
        getline(ss, startStr, ',');
        getline(ss, durStr, ',');

        Note n;
        n.freq = stoi(freqStr);
        // Nhân 1000 và làm tròn để chuyển từ giây sang mili-giây
        n.startTime = round(stof(startStr) * 1000.0);
        n.duration = round(stof(durStr) * 1000.0);

        // Phân loại vào 3 mảng track tương ứng
        if (motorStr == "M0") track1.push_back(n);
        else if (motorStr == "M1") track2.push_back(n);
        else if (motorStr == "M2") track3.push_back(n);
    }

    // Hàm in dữ liệu ra định dạng mảng C
    auto printTrack = [](const string& name, const vector<Note>& track) {
        cout << "const Note " << name << "[" << track.size() << "] = {\n";
        for (const auto& n : track) {
            cout << "    {" << n.freq << ", " << n.startTime << ", " << n.duration << "},\n";
        }
        cout << "};\n\n";
    };

    // Chuyển hướng output ra một file .h để dễ copy
    freopen("midi_data.h", "w", stdout);

    cout << "#include <stdint.h>\n\n";
    printTrack("track1_notes", track1);
    printTrack("track2_notes", track2);
    printTrack("track3_notes", track3);

    return 0;
}
