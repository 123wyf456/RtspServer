#include "MediaSessionManager.h"
#include "MediaSession.h"

MediaSessionManager* MediaSessionManager::createNew() {
    return new MediaSessionManager();
}

MediaSessionManager::MediaSessionManager() {

}

MediaSessionManager::~MediaSessionManager() {

}

bool MediaSessionManager::addSessdion(MediaSession* session) {
    if(mSessionMap.find(session->name()) != mSessionMap.end()) {    // 会话已经存在
        return false;
    }
    else {
        mSessionMap.insert(std::make_pair(session->name(), session));
        return true;
    }
}

bool MediaSessionManager::removeSession(MediaSession* session) {
    std::map<std::string, MediaSession*>::iterator it = mSessionMap.find(session->name());
    if(mSessionMap.find(session->name()) == mSessionMap.end()) {
        return false;
    }
    else {
        mSessionMap.erase(it);
        return true;
    }
}

MediaSession* MediaSessionManager::getMediaSession(const std::string& name) {
    std::map<std::string, MediaSession*>::iterator it = mSessionMap.find(name);
    if(it == mSessionMap.end()) {
        return NULL;
    }
    else {
        return it->second;
    }

}