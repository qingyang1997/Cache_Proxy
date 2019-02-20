//
// Created by Duck2 on 2019-02-18.
//

#ifndef PROXY_MYEXCEPTION_H
#define PROXY_MYEXCEPTION_H
#include <exception>
#include <string>
class ErrorException : public std::exception {
    private:

        std::string message;
    public:
        explicit ErrorException(std::string message): message(message){}

        virtual const char *what(){
            return message.c_str();
        }
    };
#endif //PROXY_MYEXCEPTION_H
