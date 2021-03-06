//
//  RTConnectionManager.h
//  dyncRTMeeting
//
//  Created by hp on 12/3/15.
//  Copyright (c) 2015 hp. All rights reserved.
//

#ifndef __dyncRTMeeting__RTConnectionManager__
#define __dyncRTMeeting__RTConnectionManager__

#include <stdio.h>
#include <iostream>
#include <map>
#include <list>
#include "RTTransferSession.h"
#include "RTMessage.h"

class RTConnectionManager {
public:
    typedef struct _ModuleInfo{
        int             flag;
        TRANSFERMODULE  othModuleType;
        std::string     othModuleId;
        RTTransferSession*     pModule;
        _ModuleInfo() {
            flag = 0;
            othModuleType = transfermodule_invalid;
            othModuleId = "";
            pModule = NULL;
        }
    }ModuleInfo;
    
    //store  module usage information
    //meet meet123456  session123456
    //msgqueue queue123456  session234567
    typedef struct _TypeModuleSessionInfo{
        TRANSFERMODULE moduleType;
        std::string moduleId;
        std::list<std::string> sessionIds;
        _TypeModuleSessionInfo() {
            moduleType = transfermodule_invalid;
            moduleId = "";
            sessionIds.clear();
        }
    }TypeModuleSessionInfo;
    
    //match the module type and session id
    typedef struct _TypeSessionInfo{
        TRANSFERMODULE moduleType;
        std::string sessionId;
        _TypeSessionInfo() {
            moduleType = transfermodule_invalid;
            sessionId = "";
        }
    }TypeSessionInfo;
    
    //store which session the user use most
    typedef struct _UserSessionInfo{
        std::string userId;
        std::list<TypeSessionInfo> typeSessionInfo;
        _UserSessionInfo() {
            userId = "";
            typeSessionInfo.clear();
        }
    }UserSessionInfo;
    
    typedef std::map< std::string, ModuleInfo* >     ModuleInfoMaps;
    
    //<user_id, UserModuleTypeInfo>
    typedef std::list<TypeModuleSessionInfo*> TypeModuleSessionInfoLists;
    
    //check list and map which is better
    typedef std::list<UserSessionInfo*> UserSessionInfoLists;
    
    typedef std::map<std::string, std::list<TypeSessionInfo*> > UserSessionInfoMaps;
    
    
    static ModuleInfoMaps*  GetModuleInfo() {
        static ModuleInfoMaps  s_ModuleInfoMap;
        return &s_ModuleInfoMap;
    }
    
    static TypeModuleSessionInfoLists* GetTypeModuleSessionInfo() {
        static TypeModuleSessionInfoLists s_TypeModuleSessionInfoList;
        return &s_TypeModuleSessionInfoList;
    }
    
    static UserSessionInfoLists* GetUserSessionInfoList() {
        static UserSessionInfoLists s_UserSessionInfoList;
        return &s_UserSessionInfoList;
    }
    
    static UserSessionInfoMaps* GetUserSessionInfoMap() {
        static UserSessionInfoMaps s_UserSessionInfoMap;
        return &s_UserSessionInfoMap;
    }
    
    static RTConnectionManager* Instance() {
        static RTConnectionManager s_manager;
        return &s_manager;
    }
    
    ModuleInfo*  findModuleInfo(const std::string& userid, TRANSFERMODULE module);
    
    bool AddModuleInfo(ModuleInfo* pmi, const std::string& sid);
    bool DelModuleInfo(const std::string& sid);
    bool AddTypeModuleSession(TRANSFERMODULE mtype, const std::string& mid, const std::string& sid);
    bool DelTypeModuleSession(const std::string& sid);
    
    void TransferSessionLostNotify(const std::string& sid);
    
    void    GenericSessionId(std::string& strId);
    bool    ConnectConnector();
    void    RefreshConnection();
    
    void SetMeetingId(const std::string& mid) { m_meetingId = mid; }
    std::string& MeetingId() { return m_meetingId; }
    std::list<std::string>* GetAddrsList() { return &m_ipList; }
private:
    RTConnectionManager():m_lastUpdateTime("RTConnectionManager") {}
    ~RTConnectionManager() {}
    bool DoConnectConnector(const std::string ip, unsigned short port);
    std::list<std::string>    m_ipList;
    std::string               m_lastUpdateTime;
    std::string               m_meetingId;

};

#endif /* defined(__dyncRTMeeting__RTConnectionManager__) */
