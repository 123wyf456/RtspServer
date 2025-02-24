#pragma once
#include <map>
#include <string>

class MediaSession;

class MediaSessionManager {
public:
    static MediaSessionManager* createNew();

    MediaSessionManager();
    ~MediaSessionManager();

public:
    bool addSessdion(MediaSession* session);
    bool removeSession(MediaSession* session);
    MediaSession* getMediaSession(const std::string& name);

private:
    std::map<std::string, MediaSession*> mSessionMap;

};