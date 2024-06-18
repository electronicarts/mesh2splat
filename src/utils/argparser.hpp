#include <algorithm>
#include <vector>
#include <string>

//https://stackoverflow.com/a/868894
class InputParser {
public:
    InputParser(int& argc, char** argv) {
        for (int i = 1; i < argc; ++i)
            this->tokens.push_back(std::string(argv[i]));
    }

    const std::string& getCmdOption(const std::string& option) const;

    bool cmdOptionExists(const std::string& option) const;

private:
    std::vector<std::string> tokens;
};