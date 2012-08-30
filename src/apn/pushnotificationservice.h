/*
 Flexisip, a flexible SIP proxy server with media capabilities.
 Copyright (C) 2012  Belledonne Communications SARL.

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

#ifndef PUSH_NOTIFICATION_SERVICE_H
#define PUSH_NOTIFICATION_SERVICE_H

#include "pushnotification.h"

#include <list>

#include <condition_variable>
#include <mutex>
#include <thread>
#include <string>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/version.hpp>

class PushNotificationClient;

class PushNotificationService {
	friend class PushNotificationClient;
public:
	void sendRequest(const std::shared_ptr<PushNotificationRequest> &pn);

	void start();

	void stop();

	PushNotificationService(const std::string &certdir, const std::string &cafile);

	~PushNotificationService();

	std::string handle_password_callback(std::size_t max_length, boost::asio::ssl::context_base::password_purpose purpose) const;
#if BOOST_VERSION >= 104800
	bool handle_verify_callback(bool preverified, boost::asio::ssl::verify_context &ctx) const;
#endif

private:
	void setupClients(const std::string & certdir, const std::string& ca);
  
	int run();

	boost::asio::io_service &getService();

	void clientEnded();

private:
	boost::asio::io_service mIOService;
	

	std::thread *mThread;

	std::mutex mMutex;
	std::condition_variable mClientCondition;

	bool mHaveToStop;
	std::map<std::string,std::shared_ptr<PushNotificationClient> > mClients;
	std::string mPassword;
};

#endif //PUSH_NOTIFICATION_SERVICE_H