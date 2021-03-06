//
//  RTTransferSession.cpp
//  dyncRTMsgQueue
//
//  Created by hp on 11/26/15.
//  Copyright (c) 2015 hp. All rights reserved.
//

#include "RTTransferSession.h"
#include "RTMessage.h"
#include "atomic.h"
#include "RTConnectionManager.h"
#include <list>

#define TIMEOUT_TS (60*1000)

static unsigned int	g_trans_id = 0;
static unsigned int	g_msg_id = 0;

RTTransferSession::RTTransferSession()
: m_pBuffer(NULL)
, m_nBufLen(0)
, m_nBufOffset(0)
, m_lastUpdateTime(0)
{
    AddObserver(this);
}

RTTransferSession::~RTTransferSession()
{
    DelObserver(this);
    Unit();
    if (m_pBuffer) {
        free(m_pBuffer);
        m_pBuffer = NULL;
    }
}

void RTTransferSession::Init()
{
    TCPSocket* socket = this->GetSocket();
    
    socket->Open();
    
    socket->InitNonBlocking(socket->GetSocketFD());
    socket->NoDelay();
    socket->KeepAlive();
    socket->SetSocketBufSize(96L * 1024L);
    
    socket->SetTask(this);
    this->SetTimer(120*1000);
}

void RTTransferSession::Unit()
{
    Disconn();
}

bool RTTransferSession::Connect(const std::string addr, int port)
{
    if (addr.empty() || port < 8192) {
        LE("%s invalid params addr:%s, port:%d\n", __FUNCTION__, addr.c_str(), port);
        return false;
    }
    OS_Error err = GetSocket()->Connect(SocketUtils::ConvertStringToAddr(addr.c_str()), port);
    if (err == OS_NoErr || err == EISCONN) {
        return true;
    } else {
        LE("%s ERR:%d\n", __FUNCTION__, err);
        return false;
    }
}

void RTTransferSession::Disconn()
{
    GetSocket()->Cleanup();
}

bool RTTransferSession::RefreshTime()
{
    UInt64 now = OS::Milliseconds();
    if (m_lastUpdateTime <= now) {
        m_lastUpdateTime = now  + TIMEOUT_TS;
        RTTcp::UpdateTimer();
        return true;
    } else {
        return false;
    }
}

void RTTransferSession::KeepAlive()
{
    TRANSFERMSG t_msg;
    CONNMSG c_msg;
    t_msg._action = TRANSFERACTION::req;
    t_msg._fmodule = TRANSFERMODULE::mmsgqueue;
    t_msg._type = TRANSFERTYPE::conn;
    t_msg._trans_seq = GenericTransSeq();
    t_msg._trans_seq_ack = 0;
    t_msg._valid = 1;
    
    c_msg._tag = CONNTAG::co_keepalive;
    c_msg._msg = "1";
    c_msg._id = "";
    c_msg._msgid = "";
    c_msg._moduleid = "";
    t_msg._content = c_msg.ToJson();
    
    std::string s = t_msg.ToJson();
    SendTransferData(s.c_str(), (int)s.length());
}

void RTTransferSession::TestConnection()
{
    
}

void RTTransferSession::EstablishConnection()
{
    TRANSFERMSG t_msg;
    CONNMSG c_msg;
    t_msg._action = TRANSFERACTION::req;
    t_msg._fmodule = TRANSFERMODULE::mmsgqueue;
    t_msg._type = TRANSFERTYPE::conn;
    t_msg._trans_seq = GenericTransSeq();
    t_msg._trans_seq_ack = 0;
    t_msg._valid = 1;
    
    c_msg._tag = CONNTAG::co_msg;
    c_msg._msg = "hello";
    c_msg._id = "";
    c_msg._msgid = "";
    c_msg._moduleid = "";
    
    t_msg._content = c_msg.ToJson();
    
    std::string s = t_msg.ToJson();
    SendTransferData(s.c_str(), (int)s.length());
}

void RTTransferSession::SendTransferData(const char* pData, int nLen)
{
    RTTcp::SendTransferData(pData, nLen);
    GetSocket()->RequestEvent(EV_RE);
}

// from RTTcp
void RTTransferSession::OnRecvData(const char*pData, int nLen)
{
    if (!pData) {
        return;
    }
    {
        while((m_nBufOffset + nLen) > m_nBufLen)
        {
            m_nBufLen += kRequestBufferSizeInBytes;
            char* ptr = (char *)realloc(m_pBuffer, m_nBufLen);
            if(ptr != NULL)
            {//
                m_pBuffer = ptr;
            }
            else
            {//
                m_nBufLen -= kRequestBufferSizeInBytes;
                continue;
            }
        }
        
        memcpy(m_pBuffer + m_nBufOffset, pData, nLen);
        m_nBufOffset += nLen;
    }
    
    {
        int parsed = 0;
        parsed = RTTransfer::ProcessData(m_pBuffer, m_nBufOffset);
        
        if(parsed > 0)
        {
            m_nBufOffset -= parsed;
            if(m_nBufOffset == 0)
            {
                memset(m_pBuffer, 0, m_nBufLen);
            }
            else
            {
                memmove(m_pBuffer, m_pBuffer + parsed, m_nBufOffset);
            }
        }
    }
}

void RTTransferSession::OnLcsEvent()
{

}

void RTTransferSession::OnPeerEvent()
{

}

void RTTransferSession::OnTickEvent()
{

}

// from RTTransfer

void RTTransferSession::OnTransfer(const std::string& str)
{
    RTTcp::SendTransferData(str.c_str(), (int)str.length());
}

void RTTransferSession::OnTypeConn(TRANSFERMODULE fmodule, const std::string& str)
{
    CONNMSG c_msg;
    std::string err;
    c_msg.GetMsg(str, err);
    if (err.length() > 0) {
        //connid error
        LE("%s parsed err:%s\n", __FUNCTION__, err.c_str());
        return;
    }
    if ((c_msg._tag == CONNTAG::co_msg) && c_msg._msg.compare("hello") == 0) {
        // when other connect to ME:
        // send the transfersessionid and MsgQueueId to other
        TRANSFERMSG t_msg;
        std::string trid;
        RTConnectionManager::Instance()->GenericSessionId(trid);
        m_transferSessId = trid;
        
        t_msg._action = TRANSFERACTION::req;
        //this is for transfer
        t_msg._fmodule = TRANSFERMODULE::mmsgqueue;
        t_msg._type = TRANSFERTYPE::conn;
        t_msg._trans_seq = GenericTransSeq();
        t_msg._trans_seq_ack = 0;
        t_msg._valid = 1;
        
        c_msg._tag = CONNTAG::co_id;
        c_msg._id = m_transferSessId;
        //send self MsgQueue id to other
        c_msg._moduleid = RTConnectionManager::Instance()->MsgQueueId();
        
        t_msg._content = c_msg.ToJson();
        
        std::string s = t_msg.ToJson();
        SendTransferData(s.c_str(), (int)s.length());
    } else if ((c_msg._tag == CONNTAG::co_id) && c_msg._msg.compare("hello") == 0) {
        // when ME connector to other:
        // store other's transfersessionid and other's moduleId
        if (c_msg._id.length()>0) {
            m_transferSessId = c_msg._id;
            {
                RTConnectionManager::ModuleInfo* pmi = new RTConnectionManager::ModuleInfo();
                if (pmi) {
                    pmi->flag = 1;
                    pmi->othModuleType = fmodule;
                    pmi->othModuleId = m_transferSessId;
                    pmi->pModule = this;
                    //bind session and transfer id
                    RTConnectionManager::Instance()->GetModuleInfo()->insert(make_pair(m_transferSessId, pmi));
                    //store which moudle connect to this connector
                    //c_msg._moduleid:store other's module id
                    LI("store other connector moduleid:%s, transfersessionid:%s\n", c_msg._moduleid.c_str(), m_transferSessId.c_str());
                    RTConnectionManager::Instance()->AddTypeModuleSession(fmodule, c_msg._moduleid, m_transferSessId);
                } else {
                    LE("new ModuleInfo error!!!\n");
                }
            }
            
            TRANSFERMSG t_msg;
            
            t_msg._action = TRANSFERACTION::req;
            t_msg._fmodule = TRANSFERMODULE::mmsgqueue;
            t_msg._type = TRANSFERTYPE::conn;
            t_msg._trans_seq = GenericTransSeq();
            t_msg._trans_seq_ack = 0;
            t_msg._valid = 1;
            
            c_msg._tag = CONNTAG::co_msgid;
            c_msg._id = m_transferSessId;
            c_msg._msgid = "ok";
            //send self MsgQueue id to other
            c_msg._moduleid = RTConnectionManager::Instance()->MsgQueueId();
            
            t_msg._content = c_msg.ToJson();
            
            std::string s = t_msg.ToJson();
            SendTransferData(s.c_str(), (int)s.length());
        } else {
            LE("Connection id:%s error!!!\n", c_msg._id.c_str());
        }
    } else if ((c_msg._tag == CONNTAG::co_msgid) && c_msg._msgid.compare("ok") == 0) {
        // when other connect to ME:
        if (m_transferSessId.compare(c_msg._id) == 0) {
            RTConnectionManager::ModuleInfo* pmi = new RTConnectionManager::ModuleInfo();
            if (pmi) {
                pmi->flag = 1;
                pmi->othModuleType = fmodule;
                pmi->othModuleId = m_transferSessId;
                pmi->pModule = this;
                //bind session and transfer id
                RTConnectionManager::Instance()->GetModuleInfo()->insert(make_pair(m_transferSessId, pmi));
                //store which moudle connect to this connector
                //store other module id
                LI("store moduleid:%s, transfersessid:%s\n", c_msg._moduleid.c_str(), m_transferSessId.c_str());
                RTConnectionManager::Instance()->AddTypeModuleSession(fmodule, c_msg._moduleid, m_transferSessId);
            } else {
                LE("new ModuleInfo error!!!!\n");
            }
        }
        
    }  else if (c_msg._tag == CONNTAG::co_keepalive) {
        RTTcp::UpdateTimer();
    } else {
        LE("%s invalid msg tag\n", __FUNCTION__);
    }
}

void RTTransferSession::OnTypeTrans(TRANSFERMODULE fmodule, const std::string& str)
{
    LI("%s was called, str:%s\n", __FUNCTION__, str.c_str());
}

void RTTransferSession::OnTypeQueue(TRANSFERMODULE fmodule, const std::string& str)
{
    TOJSONUSER user;
    QUEUEMSG qmsg;
    DISPATCHMSG dmsg;
    TRANSFERMSG trmsg;
    {
        //get user
        std::string err;
        qmsg.GetMsg(str, err);
        if (err.length()>0) {
            LE("%s QUEUEMSG err:%s\n", __FUNCTION__, err.c_str());
            return;
        }
        user.GetMsg(qmsg._touser, err);
        if (err.length()>0) {
            LE("%s TOJSONUSER err:%s\n", __FUNCTION__, err.c_str());
            return;
        }
    }
    {
        //check user online or offline
    }
    {
        //if offline, push to offline msgqueue
    }
    {
        //if online, push to online msgqueue
    }
    
    std::list<const std::string>::iterator it = user._us.begin();
    for (; it!=user._us.end(); it++) {
        dmsg._flag = 0;
        dmsg._touser = (*it);
        dmsg._connector = qmsg._connector;//which connector comes frome
        dmsg._content = qmsg._content;
        
        trmsg._action = TRANSFERACTION::req;
        trmsg._fmodule = TRANSFERMODULE::mmsgqueue;
        trmsg._type = TRANSFERTYPE::dispatch;
        trmsg._trans_seq = GenericTransSeq();
        trmsg._trans_seq_ack = 0;
        trmsg._valid = 1;
        std::string sd = dmsg.ToJson();
        trmsg._content = sd;
        
        LI("OnTypeQueue dmsg._touser:%s\n", dmsg._touser.c_str());
        //here we should know this msg send to whom
        //find connector and dispatch to it
        std::string st = trmsg.ToJson();
        //connector moduleId
        RTConnectionManager::ModuleInfo* pmi = RTConnectionManager::Instance()->findConnectorInfoById(dmsg._touser);
        if (pmi && pmi->pModule) {
            pmi->pModule->SendTransferData(st.c_str(), (int)st.length());
        } else {
            LE("mi.pModule is NULL\n");
        }
    }
}

void RTTransferSession::OnTypeDispatch(TRANSFERMODULE fmodule, const std::string& str)
{
    LI("%s was called\n", __FUNCTION__);
}

void RTTransferSession::OnTypePush(TRANSFERMODULE fmodule, const std::string& str)
{
    LI("%s was called\n", __FUNCTION__);
}

void RTTransferSession::ConnectionDisconnected()
{
    if (m_transferSessId.length()>0) {
        RTConnectionManager::Instance()->TransferSessionLostNotify(m_transferSessId);
    }
}

////////////////////////////////////////////////////////
////////////////////private/////////////////////////////
////////////////////////////////////////////////////////

void RTTransferSession::GenericMsgId(std::string& strId)
{
    char buf[32] = {0};
    int id_ = (UInt32)atomic_add(&g_msg_id, 1);
    sprintf(buf, "msgqueue_%06d", id_);
    strId = buf;
}

int RTTransferSession::GenericTransSeq()
{
    return atomic_add(&g_trans_id, 1);
}

void RTTransferSession::EstablishAck()
{

}

void RTTransferSession::OnEstablishConn()
{

}

void RTTransferSession::OnEstablishAck()
{

}
