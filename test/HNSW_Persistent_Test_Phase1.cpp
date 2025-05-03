#include "../kvstore.h"
#include <fstream>
#include <vector>

std::vector<std::string> load_text(std::string filename) {
    std::ifstream file(filename);
    std::string line;
    std::vector<std::string> text;
    while (std::getline(file, line)) {
        text.push_back(line);
    }
    return text;
}

int main() {
    KVStore store("data/");
    store.reset();

    std::vector<std::string> text = load_text("data/trimmed_text.txt");
    const int total = 128;
    const int phase[4] = {0, 32, 64, 96};

    for (int i = 0; i < total; i++) {
        store.put(i, text[i]);
    }

    for(int i = phase[1]; i < total; ++i) {
        store.del(i);
    }

    for(int i = phase[2]; i < phase[3]; ++i) {
        store.put(i, text[i]);
    }

    for(int i = phase[0]; i < phase[1]; ++i) {
        int j = i + phase[3];
        store.put(i, text[j]);
    }

    store.save_hnsw_index_to_disk("hnsw_data/");

    return 0;
}
