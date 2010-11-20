/*
	Flexisip, a flexible SIP proxy server with media capabilities.
    Copyright (C) 2010  Belledonne Communications SARL.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "agent.hh"
#include "mediarelay.hh"

#include <poll.h>

#include <algorithm>
#include <list>

using namespace::std;

int MediaSource::recv(uint8_t *buf, size_t buflen){
	slen=sizeof(ss);
	int err=recvfrom(fd,buf,buflen,0,(struct sockaddr*)&ss,&slen);
	if (err==-1) slen=0;
	return err;
}

int MediaSource::send(uint8_t *buf, size_t buflen){
	int err;
	if (slen>0){
		err=sendto(fd,buf,buflen,0,(struct sockaddr*)&ss,slen);
		return err;
	}
	return 0;
}


RelaySession::RelaySession(const std::string &localip) : mLocalIp(localip){
	mLastActivityTime=0;
	mSession[0]=rtp_session_new(RTP_SESSION_SENDRECV);
	mSession[1]=rtp_session_new(RTP_SESSION_SENDRECV);
	rtp_session_set_local_addr(mSession[0],mLocalIp.c_str(),-1);
	rtp_session_set_local_addr(mSession[1],mLocalIp.c_str(),-1);
	mSources[0].fd=rtp_session_get_rtp_socket(mSession[0]);
	mSources[1].fd=rtp_session_get_rtp_socket(mSession[1]);
	mSources[2].fd=rtp_session_get_rtcp_socket(mSession[0]);
	mSources[3].fd=rtp_session_get_rtcp_socket(mSession[1]);
	mUsed=true;
}

RelaySession::~RelaySession(){
	rtp_session_destroy(mSession[0]);
	rtp_session_destroy(mSession[1]);
}

int RelaySession::getPorts(int ports[2])const{
	ports[0]=rtp_session_get_local_port(mSession[0]);
	ports[1]=rtp_session_get_local_port(mSession[1]);
	if (ports[0]==-1 || ports[1]==-1)
		return -1;
	return 0;
}

const std::string & RelaySession::getAddr()const{
	return mLocalIp;
}

void RelaySession::unuse(){
	mUsed=false;
}

void RelaySession::fillPollFd(struct pollfd *tab){
	int i;
	for(i=0;i<4;++i){
		tab[i].fd=mSources[i].fd;
		tab[i].events=POLLIN;
		tab[i].revents=0;
	}
}


void RelaySession::transfer(time_t curtime, struct pollfd *tab){
	uint8_t buf[1500];
	const int maxsize=sizeof(buf);
	int len;
	int i;

	if (mLastActivityTime==0)
		mLastActivityTime=curtime;
	
	for (i=0;i<4;i+=2){
		if (tab[i].revents & POLLIN){
			len=mSources[i].recv(buf,maxsize);
			if (len>0)
				mSources[i+1].send(buf,len);
			mLastActivityTime=curtime;
		}
		if (tab[i+1].revents & POLLIN){
			mLastActivityTime=curtime;
			len=mSources[i+1].recv(buf,maxsize);
			if (len>0)
				mSources[i].send(buf,len);
		}
	}
}


MediaRelayServer::MediaRelayServer(const std::string &localip) : mLocalIp(localip){
	mRunning=true;
	if (pipe(mCtlPipe)==-1){
		LOGF("Could not create MediaRelayServer control pipe.");
	}
	pthread_create(&mThread,NULL,&MediaRelayServer::threadFunc,this);
}


MediaRelayServer::~MediaRelayServer(){
	mRunning=false;
	if (write(mCtlPipe[1],"e",1)==-1)
		LOGE("MediaRelayServer: Fail to write to control pipe.");
	pthread_join(mThread,NULL);
	for_each(mSessions.begin(),mSessions.end(),delete_functor<RelaySession>());
	close(mCtlPipe[0]);
	close(mCtlPipe[1]);
}

RelaySession *MediaRelayServer::createSession(){
	RelaySession *s=new RelaySession(mLocalIp);
	mMutex.lock();
	mSessions.push_back(s);
	mMutex.unlock();
	/*write to the control pipe to wakeup the server thread */
	if (write(mCtlPipe[1],"e",1)==-1)
		LOGE("MediaRelayServer: fail to write to control pipe.");
	return s;
}

void MediaRelayServer::run(){
	int sessionCount;
	int i;
	struct pollfd *pfds;
	list<RelaySession*>::iterator it;
	int err;
	
	while(mRunning){
		mMutex.lock();
		sessionCount=mSessions.size();
		mMutex.unlock();
		pfds=(struct pollfd*)alloca((sessionCount*4) + 1);
		for(i=0,it=mSessions.begin();i<sessionCount;++i,++it){
			(*it)->fillPollFd(&pfds[i*4]);
		}
		
		pfds[sessionCount*4].fd=mCtlPipe[0];
		pfds[sessionCount*4].events=POLLIN;
		pfds[sessionCount*4].revents=0;
		
		err=poll(pfds,(sessionCount*4 )+ 1,-1);
		if (pfds[sessionCount*4].revents){
			char tmp;
			if (read(mCtlPipe[0],&tmp,1)==-1){
				LOGE("Fail to read from control pipe.");
			}
		}
		time_t curtime=time(NULL);
		for(i=0,it=mSessions.begin();i<sessionCount;++i,++it){
			RelaySession *s=(*it);
			if (s->isUsed()){
				s->transfer(curtime,&pfds[i*4]);
			}
		}
		/*cleanup loop*/
		mMutex.lock();
		for(it=mSessions.begin();it!=mSessions.end();){
			if (!(*it)->isUsed()){
				delete *it;
				it=mSessions.erase(it);
			}else{
				++it;
			}
		}
		mMutex.unlock();
	}
}

void *MediaRelayServer::threadFunc(void *arg){
	MediaRelayServer *zis=(MediaRelayServer*)arg;
	zis->run();
	return NULL;
}
