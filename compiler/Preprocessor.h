#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include <string>
#include <vector>

class Preprocessor {
public:
    struct PreprocessError {
        std::string type;
        std::string message;
        int lineNum;
    };
    
    std::string cleanCode(const std::string& code);
    std::vector<PreprocessError> checkErrors(const std::string& code);
    bool hasErrors() const { return !errors.empty(); }
    void printErrors() const;
    
private:
    std::vector<PreprocessError> errors;
    
    void addError(const std::string& type, const std::string& msg, int line);
    bool isCommentChar(char c) const;
};

#endif