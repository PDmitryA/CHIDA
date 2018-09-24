//
// Created by pda on 24.09.18.
//

#ifndef HIGHLOAD_MIMETYPE_H
#define HIGHLOAD_MIMETYPE_H
#include <unordered_map>
#include <string>


class MimeType {
public:
    static MimeType* Instance();
    std::string get_mime_type(const std::string& path);

protected:
    MimeType();

private:
    static MimeType* _instance;
    const std::unordered_map<std::string, std::string> types;

};


#endif //HIGHLOAD_MIMETYPE_H
