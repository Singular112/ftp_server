/*
 *	Author: Ilia Vasilchikov
 *	mail: gravity@hotmail.ru
 *	gihub page: https://github.com/Singular112/
 *	Licence: MIT
*/

#include "ftp_server.h"

// common includes for all platfoms
#include <time.h>
#include <vector>
#include <string>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include <fcntl.h>
#include <sys/stat.h>

#if defined(WIN32)
#	include <io.h>
#	include <direct.h>
#	include <direct.h>

#	define socklen_t int
#elif defined(__linux__)
#	define MAX_PATH					260
#	define INVALID_SOCK				-1
#	define CONFIG_MAX_SOCKETS		64

#	include <arpa/inet.h>
#	include <sys/ioctl.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#else // ESP32
#	define MAX_PATH					260
#	define INVALID_SOCK				-1
#	define CONFIG_MAX_SOCKETS		CONFIG_LWIP_MAX_SOCKETS

#	include "unique_ptr_impl.h"
#endif

#include "convert_utf8_to_windows1251.h"
using namespace filesystem_tools::helpers;

#if defined(WIN32) || defined(__linux__)
#define DECLARE_SMART_CLOSER(type_name, type, func)		\
	struct type_name##_s								\
	{													\
		void operator()(type* p)						\
		{												\
			if (p) func(p); p = nullptr;				\
		}												\
	};													\
	using type_name = std::unique_ptr<type, type_name##_s>;
#else
#define DECLARE_SMART_CLOSER(type_name, type, func)		\
	struct type_name##_s								\
	{													\
		void operator()(type* p)						\
		{												\
			if (p) func(p); p = nullptr;				\
		}												\
	};													\
	using type_name = unique_ptr<type, type_name##_s>;
#endif

DECLARE_SMART_CLOSER(smart_fp, FILE, fclose);

#if !defined(WIN32) && !defined(__linux__)
DECLARE_SMART_CLOSER(smart_dp, DIR, closedir);
#endif

#if defined(WIN32) || defined(__linux__)
template <typename T>
struct array_deleter_s
{
	void operator()(T* ptr) { if (*ptr != nullptr) delete[] *ptr; }
};
#endif

class smart_socket
{
public:
	smart_socket(SOCKET sock) : m_socket(sock) {}
	~smart_socket()
	{
		if (m_socket)
		{
			closesocket(m_socket);
			m_socket = 0;
		}
	}

	void set(SOCKET sock)
	{
		m_socket = sock;
	}

	SOCKET get()
	{
		return m_socket;
	}

private:
	SOCKET m_socket;
};

//
static const char* TAG = "FTP";

//

namespace ftp_server
{

#define SELECT_SLEEP_DURATION	1000 * 500	// 500 ms


ftp_server_c::ftp_server_c()
	: m_native_encoding(e_encoding_utf8)
{
}


ftp_server_c::~ftp_server_c()
{
	stop();
}


void ftp_server_c::set_homedir(const std::string& abs_path)
{
	m_home_dir = filesystem_tools::helpers::rebuild_path(abs_path);
}


std::string ftp_server_c::homedir() const
{
	return m_home_dir;
}


bool ftp_server_c::start(uint16_t port)
{
	if (!check_directory_exists(m_home_dir))
	{
		if (mkdir(m_home_dir.c_str()
#ifndef WIN32
			, 0777
#endif
			))
		{
			printf("Failed to create directory on path: %s\n",
				m_home_dir.c_str());

			return false;
		}
	}

#ifdef _WIN32
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

	if (!initialize_sock_channel(m_listen_socket, port, true))
		return false;

	m_working = true;

	server_routine();

	return true;
}


void ftp_server_c::stop()
{
	m_working = false;

	// todo: wait for thread finished

	if (m_listen_socket)
	{
		closesocket(m_listen_socket);
		m_listen_socket = 0;
	}
}


#if defined(WIN32)
void ftp_server_c::server_routine()
{
	fd_set master_read_fds;
	fd_set master_exception_fds;

	FD_ZERO(&master_read_fds);
	FD_ZERO(&master_exception_fds);

	FD_SET(m_listen_socket, &master_read_fds);
	FD_SET(m_listen_socket, &master_exception_fds);

	struct timeval tv = { 0, SELECT_SLEEP_DURATION };

	while (m_working)
	{
		fd_set read_fds;
		fd_set exception_fds;

		memcpy(&read_fds, &master_read_fds, sizeof(fd_set));
		memcpy(&exception_fds, &master_exception_fds, sizeof(fd_set));

		int retval = select(0, &read_fds, NULL, &exception_fds, &tv);
		if (retval > 0)
		{
			for (uint32_t i = 0; i < read_fds.fd_count; ++i)
			{
				auto& client_socket = read_fds.fd_array[i];

				if (client_socket == m_listen_socket)
				{
					sockaddr_in client_addr;
					int addr_len = sizeof(client_addr);
					SOCKET connected_client_sock = accept(m_listen_socket, (sockaddr*)&client_addr, &addr_len);

					if (connected_client_sock == INVALID_SOCKET)
					{
						// todo: log error

						continue;
					}
					else
					{
						FD_SET(connected_client_sock, &master_read_fds);
						FD_SET(connected_client_sock, &master_exception_fds);

						handle_connection(connected_client_sock);
					}
				}
				else
				{
					char data_buf[2048] = { 0 };
					size_t m_data_buf_sz = sizeof(data_buf);

					auto client_connection = find_connection_by_socket(client_socket);
					if (!client_connection)
					{
						// impossible situation
						continue;
					}

					bool remove_connection_flag = false;

					if (FD_ISSET(client_socket, &read_fds))
					{
						// receive incoming data
						int rc = recv(client_socket, data_buf, m_data_buf_sz, 0);

						//
						int last_err = WSAGetLastError();
						if (rc > 0)
						{
							handle_incoming_data(client_connection.get(), (uint8_t*)data_buf, rc);
						}
						else if (rc == 0 || (rc == SOCKET_ERROR && last_err == WSAECONNRESET))
						{
							// client disconnected
							remove_connection_flag = true;
						}
						else if (rc == SOCKET_ERROR)
						{
							remove_connection_flag = true;

							// todo: log error
						}
					}
					else if (FD_ISSET(client_socket, &exception_fds))
					{
						remove_connection_flag = true;

						// todo: log error
					}

					if (remove_connection_flag)
					{
						remove_client_connection(client_socket);

						FD_CLR(client_socket, &master_read_fds);
						FD_CLR(client_socket, &master_exception_fds);
					}
				}
			}
		}
		else if (retval == SOCKET_ERROR)
		{
			// todo: log error
		}
	}
}
#elif 1 /* work both good for linux & esp32 */ //defined (__linux__)
void ftp_server_c::server_routine()
{
	fd_set master_set, working_set;

	FD_ZERO(&master_set);
	FD_SET(m_listen_socket, &master_set);

	SOCKET max_sd = m_listen_socket;

	struct sockaddr_storage source_addr;
	socklen_t addr_len = sizeof(source_addr);

	struct timeval timeout;
	{
		timeout.tv_sec = 0;
		timeout.tv_usec = 1000 * 500;	// 500 ms
	}

	while (m_working)
	{
		memcpy(&working_set, &master_set, sizeof(master_set));

		auto rc = select(max_sd + 1, &working_set, NULL, NULL, &timeout);

		// reset timeout (select could change it)
		{
			timeout.tv_sec = 0;
			timeout.tv_usec = 1000 * 500;	// 500 ms
		}

		if (rc == 0)
		{
			continue;
		}
		else if (rc < 0)
		{
			ESP_LOGE(TAG, "select() failed");
			break;
		}

		int desc_ready = rc;
		for (SOCKET sock = 0; sock <= max_sd && desc_ready > 0; ++sock)
		{
			if (FD_ISSET(sock, &working_set))
			{
				desc_ready -= 1;

				if (sock == m_listen_socket)
				{
					// accept a new connections
					SOCKET client_socket = accept
					(
						m_listen_socket,
						(struct sockaddr*)&source_addr,
						&addr_len
					);

					if (client_socket < 0)
					{
						if (errno != EWOULDBLOCK)
						{
							ESP_LOGE(TAG, "Error when accepting connection: %s",
								strerror(errno));
						}
					}
					else
					{
						FD_SET(client_socket, &master_set);
						if (client_socket > max_sd)
							max_sd = client_socket;

						handle_connection(client_socket);
					}
				}
				else
				{
					if (sock != INVALID_SOCK)
					{
						char data_buf[128] = { 0 };
						size_t data_buf_sz = sizeof(data_buf);

						int rc = recv(sock, data_buf, data_buf_sz, 0);

						if (rc < 0)
						{
							if (errno == EINPROGRESS ||
								errno == EAGAIN ||
								errno == EWOULDBLOCK)
							{
								continue;
							}

							if (errno == ENOTCONN)
							{
								ESP_LOGI(TAG, "Connection %d closed", sock);
							}
							else
							{
								ESP_LOGE(TAG, "Error occurred during receiving (sock: %d, err: %s). Close connection",
									sock, strerror(errno));
							}

							remove_client_connection(sock);

							FD_CLR(sock, &master_set);
							if (sock == max_sd)
							{
								while (!FD_ISSET(max_sd, &master_set))
									max_sd -= 1;
							}
						}
						else if (rc > 0)
						{
							auto client_connection = find_connection_by_socket(sock);
							if (!client_connection)
							{
								// impossible situation
								continue;
							}

							handle_incoming_data(client_connection.get(), (uint8_t*)data_buf, rc);
						}
					}
				}
			}
		}
	}
}
#elif 0 // works on esp32, but load cpu, old code
void ftp_server_c::server_routine()
{
	const size_t max_socks = CONFIG_MAX_SOCKETS - 1;
	static SOCKET client_sockets[CONFIG_MAX_SOCKETS - 1];

	struct sockaddr_storage source_addr;
	socklen_t addr_len = sizeof(source_addr);

	// prepare a list of file descriptors to hold client's sockets, mark all of them as invalid, i.e. available
	for (int i = 0; i < max_socks; ++i)
	{
		client_sockets[i] = INVALID_SOCK;
	}

	while (m_working)
	{
		// find a free socket
		int free_sock_index = 0;
		for (free_sock_index = 0; free_sock_index < max_socks; ++free_sock_index)
		{
			if (client_sockets[free_sock_index] == INVALID_SOCK)
 				break;
		}

		// accept a new connection only if we have a free socket
		if (free_sock_index < max_socks)
		{
			auto& free_socket = client_sockets[free_sock_index];

			// accept a new connections
			free_socket = accept
			(
				m_listen_socket,
				(struct sockaddr*)&source_addr,
				&addr_len
			);

			if (free_socket < 0)
			{
				if (errno != EWOULDBLOCK)
				{
					ESP_LOGE(TAG, "Error when accepting connection: %s",
						strerror(errno));
				}
			}
			else
			{
				handle_connection(free_socket);
			}
		}

		// receive data
		for (int i = 0; i < max_socks; ++i)
		{
			auto& client_socket = client_sockets[i];

			if (client_socket != INVALID_SOCK)
			{
				char data_buf[128] = { 0 };
				size_t data_buf_sz = sizeof(data_buf);

				int rc = recv(client_socket, data_buf, data_buf_sz, 0);

				if (rc < 0)
				{
					if (errno == EINPROGRESS ||
						errno == EAGAIN ||
						errno == EWOULDBLOCK)
					{
						continue;
					}

					if (errno == ENOTCONN)
					{
						ESP_LOGI(TAG, "Connection %d closed", client_socket);
					}
					else
					{
						ESP_LOGE(TAG, "Error occurred during receiving (sock: %d, err: %s). Close connection",
							client_socket, strerror(errno));
					}

					remove_client_connection(client_socket);

					client_socket = INVALID_SOCK;
				}
				else if (rc > 0)
				{
					auto client_connection = find_connection_by_socket(client_socket);
					if (!client_connection)
					{
						// impossible situation
						continue;
					}

					handle_incoming_data(client_connection.get(), (uint8_t*)data_buf, rc);
				}
			}
		}
	}
}
#endif


void ftp_server_c::remove_client_connection(SOCKET sock)
{
	if (sock == 0)
		return;

	closesocket(sock);

	auto it = m_client_connections.begin();
	while (it != m_client_connections.end())
	{
		auto& client_connection = *it;

		if (client_connection->command_socket() == sock)
		{
			it = m_client_connections.erase(it);
			break;
		}

		++it;
	}
};


ftp_server_c::ftp_client_connection_t ftp_server_c::find_connection_by_socket(SOCKET sock)
{
	for (auto& client_connection : m_client_connections)
	{
		if (client_connection->command_socket() == sock)
		{
			return client_connection;
		}
	}

	return ftp_client_connection_t();
}


void ftp_server_c::handle_connection(SOCKET client_socket)
{
	uint32_t ip[4];
	get_ip_data(client_socket, ip);

	ESP_LOGI
	(
		TAG, "New client connected (sock: %d, ip: %d.%d.%d.%d)",
		client_socket,
		ip[0], ip[1], ip[2], ip[3]
	);

	// set socket non-blocking mode
#ifdef WIN32
	u_long non_blocking_mode = 1;
	if (ioctlsocket(client_socket, FIONBIO, &non_blocking_mode) == SOCKET_ERROR)
	{
		ESP_LOGE
		(
			TAG, "Failed to set non blocking mode for socket %d (err: %s)",
			client_socket,
			strerror(errno)
		);

		return;
	}
#else
	{
		int flags = fcntl(client_socket, F_GETFL);
		if (fcntl(client_socket, F_SETFL, flags | O_NONBLOCK) == -1)
		{
			ESP_LOGE
			(
				TAG, "Failed to set non blocking mode for socket %d (err: %s)",
				client_socket,
				strerror(errno)
			);

			return;
		}
	}
#endif

	auto client_connection = std::make_shared<ftp_client_connection_c>(client_socket);
	client_connection->set_ftp_root_directory(m_home_dir);
	client_connection->set_encoding(m_native_encoding);

	// send initial message
	send_to_client(client_connection.get(), "220 lwftp ready\r\n");

	m_client_connections.emplace_back(std::move(client_connection));
}


uint16_t ftp_server_c::get_first_free_port()
{
	if (m_data_channel_ports_map.size() == 0)
		return 0;

	uint16_t free_port = 0;
	for (auto& port : m_data_channel_ports_map)
	{
		if (!port.busy)
		{
			port.busy = true;

			return port.port;
		}
	}

	return free_port;
}


void ftp_server_c::mark_port_as_free(uint16_t port_number)
{
	uint16_t free_port = 0;
	for (auto& port : m_data_channel_ports_map)
	{
		if (port.port == port_number)
		{
			port.busy = false;
			return;
		}
	}
}


void ftp_server_c::get_ip_data(int sock, uint32_t* ip)
{
	socklen_t addr_size = (socklen_t)sizeof(struct sockaddr_in);
	struct sockaddr_in addr;
	getsockname(sock, (struct sockaddr*)&addr, &addr_size);

	char* host = inet_ntoa(addr.sin_addr);
	sscanf(host, "%d.%d.%d.%d", &ip[0], &ip[1], &ip[2], &ip[3]);
}


void ftp_server_c::gen_port(uint16_t& p1, uint16_t& p2)
{
	srand((uint32_t)time(NULL));
	p1 = 128 + (rand() % 64);
	p2 = rand() % 0xff;
}


void ftp_server_c::translate_path(ftp_client_connection_c* client_connection,
	std::string& path,
	e_encoding source_encoding,
	e_encoding dest_encoding)
{
	if (client_connection->current_encoding() == m_native_encoding)
		return;

	if (source_encoding == e_encoding_win1251 && dest_encoding == e_encoding_utf8)
	{
		std::string utf8_path;
		utf8_path.resize(MAX_PATH);
		convert_windows1251_to_utf8(path.c_str(), &utf8_path[0]);

		path = utf8_path;
	}
	else if (source_encoding == e_encoding_utf8 && dest_encoding == e_encoding_win1251)
	{
		std::string win1251_path;
		win1251_path.resize(MAX_PATH);
		convert_utf8_to_windows1251(path.c_str(), &win1251_path[0], win1251_path.size());

		path = win1251_path;
	}
}


bool ftp_server_c::initialize_sock_channel(SOCKET& sock,
	uint16_t port,
	bool non_blocking_sock)
{
	struct sockaddr_in server_address;

	// Create a socket that we will listen upon.
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET)
	{
		ESP_LOGE(TAG, "Failed to initialize socket channel (sock result: %d, err: %s)",
			sock,
			strerror(errno));

		return false;
	}

#ifdef WIN32
	if (non_blocking_sock)
	{
		// set non-blocking mode
		u_long non_blocking_mode = 1;
		if (ioctlsocket(sock, FIONBIO, &non_blocking_mode) == SOCKET_ERROR)
		{
			return false;
		}
	}
#else
	int flags = fcntl(sock, F_GETFL);
	if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1)
	{
		ESP_LOGE(TAG, "Unable to set socket %d non blocking: %s",
			sock,
			strerror(errno));
	}
#endif

	// bind our server socket to a port.
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	server_address.sin_port = htons(port);
	int rc = bind(sock, (struct sockaddr *)&server_address, sizeof(server_address));
	if (rc == INVALID_SOCKET)
	{
		ESP_LOGE(TAG, "bind sock %d failed (bind result: %d, err: %s)",
			sock, rc, strerror(errno));

		closesocket(sock);

		return false;
	}

	// flag the socket as listening for new connections.
	rc = listen(sock, 5);
	if (rc == INVALID_SOCKET)
	{
		ESP_LOGE(TAG, "listen sock %d failed (listen result: %d, err: %s",
			sock, rc, strerror(errno));

		closesocket(sock);

		return false;
	}

	return true;
}


bool ftp_server_c::send_to_client(SOCKET client_socket,
	const char* data, size_t data_size)
{
	// printf("send data '%s' (data_size: %d) to sock %d\n",
	// 	data,
	// 	data_size,
	// 	(int)client_socket);

	while (data_size > 0)
	{
		int written = send(client_socket, data, data_size, 0);

		if (written < 0 && errno != EINPROGRESS)
		{
			ESP_LOGE(TAG, "Failed to send data (sock: %d, written: %d, err: %s)",
				client_socket, written, strerror(errno));

			return false;
		}

		data_size -= written;
	}

	return true;
}


bool ftp_server_c::send_to_client(ftp_client_connection_c* client_connection,
	char* data)
{
	return send_to_client(client_connection->command_socket(), data, strlen(data));
}


bool ftp_server_c::send_system_error(ftp_client_connection_c* client_connection)
{
	char buf[200];
#ifdef WIN32
	sprintf(buf, "550 %s\r\n", sys_errlist[errno]);
#else
	sprintf(buf, "550 %s\r\n", strerror(errno));
#endif

	return send_to_client(client_connection, buf);
}


void ftp_server_c::handle_incoming_data(ftp_client_connection_c* client_connection,
	uint8_t* data, size_t data_size)
{
//#ifdef _DEBUG
	printf("received command: %s\n", data);
//#endif

	uint8_t* str_ptr = data;

	std::string command_name;
	std::string command_value;

	bool single_command = false;

	// read command tag
	{
		uint8_t* str_begin = data;
		while (*str_ptr)
		{
			if (!*str_ptr || *str_ptr == ' ')
			{
				command_name = std::string(str_begin, str_ptr);
				++str_ptr;
				break;
			}

			if (*str_ptr == '\r')
			{
				single_command = true;

				command_name = std::string(str_begin, str_ptr);
				break;
			}

			++str_ptr;
		}
	}

	// read command value
	if (!single_command)
	{
		uint8_t* str_begin = str_ptr;
		while (*str_ptr)
		{
			if (!*str_ptr || *str_ptr == '\r')
			{
				command_value = std::string(str_begin, str_ptr);
				++str_ptr;
				break;
			}

			++str_ptr;
		}
	}

	handle_command
	(
		client_connection,
		determine_command(command_name),
		command_value
	);
}


ftp_server_c::e_command_types ftp_server_c::determine_command(const std::string& command_name)
{
	if (command_name == "USER")
	{
		// Authentication username.
		return e_ftpcmd_user;
	}
	else if (command_name == "PASS")
	{
		// Authentication password.
		return e_ftpcmd_pass;
	}
	else if (command_name == "opts")
	{
		// Select options for a feature (for example OPTS UTF8 ON).
		return e_ftpcmd_opts;
	}
	else if (command_name == "PWD")
	{
		// Print working directory. Returns the current directory of the host.
		return e_ftpcmd_pwd;
	}
	else if (command_name == "TYPE")
	{
		// 	Sets the transfer mode (ASCII/Binary).
		return e_ftpcmd_type;
	}
	else if (command_name == "CWD")
	{
		// Change working directory.
		return e_ftpcmd_cwd;
	}
	else if (command_name == "PASV")
	{
		// Enter passive mode.
		return e_ftpcmd_pasv;
	}
	else if (command_name == "LIST")
	{
		// Enter passive mode.
		return e_ftpcmd_list;
	}
	else if (command_name == "SYST")
	{
		// Enter passive mode.
		return e_ftpcmd_syst;
	}
	else if (command_name == "FEAT")
	{
		// Get the feature list implemented by the server.
		return e_ftpcmd_feat;
	}
	else if (command_name == "HELP")
	{
		// Returns usage documentation on a command if specified, else a general help document is returned.
		return e_ftpcmd_help;
	}
	else if (command_name == "noop")
	{
		// No operation (dummy packet; used mostly on keepalives).
		return e_ftpcmd_noop;
	}
	else if (command_name == "DELE")
	{
		// Delete file.
		return e_ftpcmd_delete;
	}
	else if (command_name == "CDUP")
	{
		// Change to Parent Directory.
		return e_ftpcmd_cdup;
	}
	else if (command_name == "RETR")
	{
		// Retrieve a copy of the file.
		return e_ftpcmd_retr;
	}
	else if (command_name == "SIZE")
	{
		// Return the size of a file.
		return e_ftpcmd_size;
	}
	else if (command_name == "MKD")
	{
		// Make directory.
		return e_ftpcmd_mkd;
	}
	else if (command_name == "RNFR")
	{
		// Rename from.
		return e_ftpcmd_rnfr;
	}
	else if (command_name == "RNTO")
	{
		// Rename from.
		return e_ftpcmd_rnto;
	}
	else if (command_name == "RMD")
	{
		// Remove a directory.
		return e_ftpcmd_rmd;
	}
	else if (command_name == "STOR")
	{
		// Accept the data and to store the data as a file at the server site.
		return e_ftpcmd_stor;
	}

	return e_ftpcmd_unknown;
}


void ftp_server_c::handle_command(ftp_client_connection_c* client_connection,
	e_command_types command,
	const std::string& command_value)
{
	switch (command)
	{
	case e_command_types::e_ftpcmd_user:
	{
		send_to_client(client_connection, "331 pretend login accepted\r\n");
	}
	break;
	case e_command_types::e_ftpcmd_pass:
	{
		send_to_client(client_connection, "230 fake user logged in\r\n");
	}
	break;
	case e_command_types::e_ftpcmd_opts:
	{
		if (command_value == "utf8 on")
		{
			client_connection->set_encoding(e_encoding_utf8);
		}

		send_to_client(client_connection, "200 ok\r\n");
	}
	break;
	case e_command_types::e_ftpcmd_pwd:
	{
		auto& directory_iterator = client_connection->get_directory_iterator();

		auto current_directory_relative_path = "/" + directory_iterator.relative_path();

		translate_path
		(
			client_connection,
			current_directory_relative_path,
			m_native_encoding,
			client_connection->current_encoding()
		);


		char buf[MAX_PATH + 32] = "";
		sprintf(buf, "257 \"%s\"\r\n", current_directory_relative_path.c_str());
		send_to_client(client_connection, buf);
	}
	break;
	case e_command_types::e_ftpcmd_type:
	{
		char buf[MAX_PATH + 32] = "";
		sprintf(buf, "200 Type set to I\r\n");
		send_to_client(client_connection, buf);
	}
	break;
	case e_command_types::e_ftpcmd_cwd:
	{
		auto& directory_iterator = client_connection->get_directory_iterator();

		auto current_path = directory_iterator.absolute_path();

		if (command_value[0] == '/')
		{
			directory_iterator.move_to_root();
		}

		std::string translated_path;

		if (client_connection->current_encoding() == e_encoding_utf8)
		{
			translated_path.resize(MAX_PATH);
			convert_utf8_to_windows1251(command_value.c_str(), &translated_path[0], translated_path.size());
		}
		else
		{
			translated_path = command_value;
		}

		if (directory_iterator.change_dir(translated_path))
		{
			char buf[MAX_PATH + 32] = "";
			sprintf(buf, "250 CWD command successful\r\n");
			send_to_client(client_connection, buf);

			printf("current path: %s, next dir: %s, real path: %s\n",
				current_path.c_str(), translated_path.c_str(),
				directory_iterator.absolute_path().c_str());
		}
		else
		{
			send_to_client(client_connection, "550 Could not change directory");
			return;
		}
	}
	break;
	case e_command_types::e_ftpcmd_pasv:
	{
		// create passive channel
		{
			uint16_t p1, p2;
			gen_port(p1, p2);// ftp_get_first_free_port();
			uint16_t port = (256 * p1) + p2;

			auto prev_data_sock = client_connection->data_socket();
			if (prev_data_sock)
			{
				closesocket(prev_data_sock);
				client_connection->assign_data_socket(0);
			}

			SOCKET new_channel = 0;
			if (initialize_sock_channel(new_channel, port, false))
			{
				client_connection->set_data_channel_mode(e_data_channel_mode_passive);

				client_connection->assign_data_socket(new_channel);
				uint32_t ip[4];
				memset(ip, 0, sizeof(ip));
				get_ip_data(client_connection->command_socket(), ip);

				char buf[62] = "";
				sprintf(buf, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)\r\n",
					ip[0], ip[1], ip[2], ip[3], p1, p2);

				send_to_client(client_connection, buf);
			}
			else
			{
				// todo: send error, log error
			}
		}
	}
	break;
	case e_ftpcmd_list:
	{
		handle_list_command(client_connection);
	}
	break;
	case e_ftpcmd_syst:
	{
		send_to_client(client_connection, "215 WIN32 SingularFTP v.0.01\r\n");
		break;
	}
	break;
	case e_ftpcmd_feat:
	{
		send_to_client(client_connection, "500 command not recognized\r\n");
		break;
	}
	break;
	case e_ftpcmd_help:
	{
		send_to_client(client_connection, "500 command not recognized\r\n");
		break;
	}
	break;
	case e_ftpcmd_noop:
	{
		send_to_client(client_connection, "200 OK\r\n");
		break;
	}
	break;
	case e_ftpcmd_delete:
	{
		auto full_path = client_connection->current_directory() + command_value;

		if (client_connection->current_encoding() != m_native_encoding)
		{
			translate_path
			(
				client_connection, full_path,
				client_connection->current_encoding(),
				m_native_encoding
			);
		}

		// todo: check permissions
		//send_to_client(client_connection, "550 Path permission error\r\n");

		if (unlink(full_path.c_str()) != 0)
		{
			send_system_error(client_connection);
			return;
		}
		else
		{
			send_to_client(client_connection, "250 DELE command successful\r\n");
		}
	}
	break;
	case e_ftpcmd_cdup:
	{
		auto& directory_iterator = client_connection->get_directory_iterator();

		directory_iterator.move_prev_dir();

		send_to_client(client_connection, "200 OK\r\n");
	}
	break;
	case e_ftpcmd_retr:
	{
		handle_retr_command(client_connection, command_value);
	}
	break;
	case e_ftpcmd_size:
	{
		auto full_path = client_connection->current_directory() + command_value;

		translate_path
		(
			client_connection,
			full_path,
			client_connection->current_encoding(),
			m_native_encoding
		);

		struct stat file_stats;
		if (stat(full_path.c_str(), &file_stats))
		{
			send_system_error(client_connection);
			return;
		}

		char buf[32];
		sprintf(buf, "213 %d\r\n", (int)file_stats.st_size);
		send_to_client(client_connection, buf);
	}
	break;
	case e_ftpcmd_mkd:
	{
		auto full_path = client_connection->current_directory() + command_value;

		translate_path
		(
			client_connection,
			full_path,
			client_connection->current_encoding(),
			m_native_encoding
		);

		auto mkdir_result = mkdir(full_path.c_str()
#ifndef WIN32
			, 0777
#endif
			);

		// todo: check permissions
		//send_to_client(client_connection, "550 Path permission error\r\n");

		if (mkdir_result == 0)
		{
			send_to_client(client_connection, "257 Directory created\r\n");
		}
		else
		{
			send_system_error(client_connection);
			return;
		}
	}
	break;
	case e_ftpcmd_rnfr:
	{
		auto full_path = client_connection->current_directory() + command_value;

		translate_path
		(
			client_connection,
			full_path,
			client_connection->current_encoding(),
			m_native_encoding
		);

		struct stat st;

		if (stat(full_path.c_str(), &st) == 0)
		{
			send_to_client(client_connection, "350 File Exists\r\n");
			client_connection->set_rename_file_path(full_path);
		}
		else
		{
			send_to_client(client_connection, "550 Path permission error\r\n");
			client_connection->set_rename_file_path("");
		}
		
	}
	break;
	case e_ftpcmd_rnto:
	{
		auto rename_to_full_path = client_connection->current_directory() + command_value;

		translate_path
		(
			client_connection,
			rename_to_full_path,
			client_connection->current_encoding(),
			m_native_encoding
		);

		auto rename_from_full_path = client_connection->rename_file_path();

		// todo: check permissions
		//send_to_client(client_connection, "550 Path permission error\r\n");

		if (rename(rename_from_full_path.c_str(), rename_to_full_path.c_str()) != 0)
		{
			send_system_error(client_connection);
			return;
		}
		else
		{
			send_to_client(client_connection, "250 RNTO command successful\r\n");
		}
	}
	break;
	case e_ftpcmd_rmd:
	{
		auto full_path = client_connection->current_directory() + command_value;

		translate_path
		(
			client_connection,
			full_path,
			client_connection->current_encoding(),
			m_native_encoding
		);

		// todo: check permissions
		//send_to_client(client_connection, "550 Path permission error\r\n");

		if (remove_directory_r(full_path.c_str(), false))
		{
			send_to_client(client_connection, "250 RMD command successful\r\n");
		}
		else
		{
			send_system_error(client_connection);
			return;
		}
	}
	break;
	case e_ftpcmd_stor:
	{
		handle_stor_command(client_connection, command_value);
	}
	break;
	default:
	{
		send_to_client(client_connection, "500 command not recognized\r\n");
	}
	break;
	}
}


void ftp_server_c::handle_list_command(ftp_client_connection_c* client_connection)
{
	auto& directory_iterator = client_connection->get_directory_iterator();

	send_to_client(client_connection, "150 Opening connection\r\n");

	smart_socket data_socket_ptr
	(
		accept(client_connection->data_socket(), NULL, NULL)
	);

	if (data_socket_ptr.get() == 0)
	{
		// todo: handle error
	}

	directory_iterator.enum_files
	(
		[&](const filesystem_tools::directory_iterator_c::entity_info_s& entity) -> bool
		{
			char answer_buf[256] = "";

			char write_datetime_str[20] = "";
#if 0 // depends on locale
			strftime(write_datetime_str, 20, "%b %d  %Y", &entity.write_time);
#else	// independed format
			static const char* months_str[12] =
			{
				"Jan",
				"Feb",
				"Mar",
				"Apr",
				"May",
				"Jun",
				"Jul",
				"Aug",
				"Sep",
				"Oct",
				"Nov",
				"Dec"
			};

			sprintf(write_datetime_str, "%s %d  %d",
				months_str[entity.write_time.tm_mon],
				entity.write_time.tm_mday,
				entity.write_time.tm_year + 1900);
#endif

			using attrs = filesystem_tools::directory_iterator_c::e_attributes;

			char directory_attr = entity.attributes & attrs::e_attribute_directory ? 'd' : '-';

			char write_attr = entity.attributes & attrs::e_attribute_readonly ? '-' : 'w';

			// translate to target encoding
			std::string translated_entity_name = entity.name;

			translate_path
			(
				client_connection,
				translated_entity_name,
				this->naive_encoding(),
				client_connection->current_encoding()
			);

			sprintf
			(
				answer_buf,
				"%cr%c-r%c-r%c-   1 root  root    %7u %s %s\r\n",

				directory_attr, write_attr, write_attr, write_attr,
				(uint32_t)entity.file_size_bytes,
				write_datetime_str,
				translated_entity_name.c_str()
			);

			send_to_client(data_socket_ptr.get(), answer_buf, strlen(answer_buf)); // todo

#if defined(_DEBUG) && 0
			if (entity.attributes & attrs::e_attribute_directory)
			{
				printf("  %s   <DIR>", entity.name.c_str());
			}
			else
			{
				printf("  %s   %ld bytes", entity.name.c_str(), entity.file_size_bytes);
			}

			printf("   last write time: %04d-%02d-%02d %02d:%02d:%02d\n",
				entity.write_time.tm_year + 1900,
				entity.write_time.tm_mon + 1,
				entity.write_time.tm_mday,
				entity.write_time.tm_hour,
				entity.write_time.tm_min,
				entity.write_time.tm_sec);
#endif

			return true; // true = continue, false = interrupt
		},

		directory_iterator.absolute_path()
	);

	send_to_client(client_connection, "226 Transfer Complete\r\n");
}


void ftp_server_c::handle_retr_command(ftp_client_connection_c* client_connection,
	const std::string& command_value)
{
	smart_fp file_handle_ptr(nullptr);

	auto full_file_path = client_connection->current_directory() + command_value;
	if (client_connection->current_encoding() == e_encoding_utf8)
	{
		translate_path
		(
			client_connection,
			full_file_path,
			client_connection->current_encoding(),
			m_native_encoding
		);
	}

	// check file available
	{
		//printf("open file: %s\n", full_file_path.c_str());

		file_handle_ptr.reset(fopen(full_file_path.c_str(), "rb"));

		if (!file_handle_ptr)
		{
			printf("failed to open file: %s (file_handle_ptr: %d)\n",
				full_file_path.c_str(),
				(int)file_handle_ptr.get());

			send_system_error(client_connection);
			return;
		}
	}

	send_to_client(client_connection, "150 Opening BINARY mode data connection\r\n");
	smart_socket data_socket_ptr
	(
		accept(client_connection->data_socket(), NULL, NULL)
	);

	if (data_socket_ptr.get() == 0)
	{
		// todo: handle error
	}

	//printf("data channel opened: %d\n", (int)data_socket_ptr.get());

	// send file data
	{
#if defined(WIN32) || defined(__linux__)
		struct stat st;
		memset(&st, 0, sizeof(struct stat));
		stat(full_file_path.c_str(), &st);
		const size_t max_buf_sz = 1024 * 1024 * 10;
		size_t buf_sz = st.st_size > max_buf_sz ? max_buf_sz : st.st_size;

		auto dynamic_buf = new char[buf_sz];
		std::unique_ptr<char*, array_deleter_s<char*>> smart_buf(&dynamic_buf);
		char* buf = *smart_buf;
#else	// ESP32
		char buf[256];
		size_t buf_sz = sizeof(buf);
#endif

		while (!feof(file_handle_ptr.get()))
		{
			auto data_sz = fread(buf, 1, buf_sz, file_handle_ptr.get());

			if (data_sz == 0)
			{
				break;
			}
			else if (data_sz < 0)
			{
				send_system_error(client_connection);
				return;
			}

			if (!send_to_client(data_socket_ptr.get(), buf, data_sz))
			{
				printf("Failed to send data to sock %d\n",
					(int)data_socket_ptr.get());

				send_to_client(client_connection, "426 Broken pipe\r\n");
				return;
			}
		}
	}

	send_to_client(client_connection, "226 Transfer Complete\r\n");
}


void ftp_server_c::handle_stor_command(ftp_client_connection_c* client_connection,
	const std::string& command_value)
{
	auto full_file_path = client_connection->current_directory() + command_value;
	if (client_connection->current_encoding() == e_encoding_utf8)
	{
		translate_path
		(
			client_connection,
			full_file_path,
			client_connection->current_encoding(),
			m_native_encoding
		);
	}

	smart_fp file_obj(fopen(full_file_path.c_str(), "wb"));
	if (!file_obj)
	{
		send_system_error(client_connection);
		return;
	}

	send_to_client(client_connection, "150 Opening BINARY mode data connection\r\n");
	smart_socket data_socket_ptr
	(
		accept(client_connection->data_socket(), NULL, NULL)
	);

	if (data_socket_ptr.get() == 0)
	{
		// todo
	}

	// receive file data
	{
#if defined(WIN32) || defined(__linux__)
		const size_t max_buf_sz = 1024 * 1024 * 10;
		size_t buf_sz = max_buf_sz;

		auto dynamic_buf = new char[buf_sz];
		std::unique_ptr<char*, array_deleter_s<char*>> smart_buf(&dynamic_buf);
		char* buf = *smart_buf;
#else	// ESP32
		char buf[256];
		size_t buf_sz = sizeof(buf);
#endif

		while (true)
		{
			auto received_chunk_sz = recv(data_socket_ptr.get(), buf, sizeof(buf), 0);

			if (received_chunk_sz == 0)
			{
				break;
			}
			else if (received_chunk_sz < 0)
			{
				printf("read failed (received_chunk_sz: %d)\n", received_chunk_sz);
				send_system_error(client_connection);
				return;
			}

			// Write to file
			if (fwrite(buf, received_chunk_sz, 1, file_obj.get()) != 1)
			{
				send_system_error(client_connection);
				return;
			}
		}

		send_to_client(client_connection, "226 Transfer Complete\r\n");
	}
}

}
