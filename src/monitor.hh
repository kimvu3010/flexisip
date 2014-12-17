/*
    Flexisip, a flexible SIP proxy server with media capabilities.
    Copyright (C) 2014  Belledonne Communications SARL.
 
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

#ifndef monitor_hh
#define monitor_hh

#include <string>
#include "agent.hh"

class Monitor {
public:
	static void exec(int socket);
	static void createAccounts();

private:
	class Init {
	public:
		Init();
	};
	
	static bool isLocalhost(std::string host);
	static bool notLocalhost(std::string host);
	static std::string md5sum(std::string s);
	static std::string generateUsername(std::string host);
	static std::string generatePassword(std::string host, std::string salt);
	static std::string findDomain();
	
	static Init sInit;
	static const std::string PYTHON_INTERPRETOR;
	static const std::string SCRIPT_PATH;
	static const std::string USERNAME_PREFIX;
};

#endif
