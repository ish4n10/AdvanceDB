#include <string>

class DiskManager {
public:
    DiskManager(std::string& file_path);
    ~DiskManager();

    void read_page(int page_id, char* page_data);
    void write_page(int page_id, const void* page_data); // void as pointer can be anything for now
    void flush();

private: 
    int file_descriptor;
};