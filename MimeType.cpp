//
// Created by pda on 24.09.18.
//

#include "MimeType.h"


MimeType::MimeType():
    types({
            {".html", "text/html"},
            {".css", "text/css"},
            {".js", "text/javascript"},
            {".jpg", "image/jpeg"},
            {".jpeg", "image/jpeg"},
            {".png", "image/png"},
            {".gif", "image/gif"},
            {".swf", "application/x-shockwave-flash"},
    }) {};

MimeType* MimeType::_instance = nullptr;

MimeType* MimeType::Instance() {
    if(_instance == nullptr){
        _instance = new MimeType();
    }
    return _instance;
}

std::string MimeType::get_mime_type(const std::string& path) {
    auto start_extension = path.find_last_of('.');
    if (start_extension == std::string::npos) {
        return "text/plain";
    }
    auto iter = types.find(path.substr(start_extension));
    if (iter == types.end()) {
        return "text/plain";
    }
    return iter->second;
}
