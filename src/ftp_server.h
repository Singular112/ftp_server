/*
 *	Author: Ilia Vasilchikov
 *	mail: gravity@hotmail.ru
 *	gihub page: https://github.com/Singular112/
 *	Licence: MIT
*/

#pragma once

// stl
#include <memory>
#include <string>
#include <stdint.h>

// helpers
#include "filesystem_tools.h"

//
#if defined(WIN32)
#	ifndef WIN32_LEAN_AND_MEAN
#		define WIN32_LEAN_AND_MEAN
#	endif
#	include <Windows.h>
#	include <winsock2.h>

#	define ESP_LOGE(LOG_TAG, ...)		\
	printf("ERROR:	[%s] ", LOG_TAG);	\
	printf(__VA_ARGS__);

#	define ESP_LOGI(LOG_TAG, ...)		\
	printf("INFO:	[%s] ", LOG_TAG);	\
	printf(__VA_ARGS__);

#	define ESP_LOGD(LOG_TAG, ...)		\
	printf("DEBUG:	[%s] ", LOG_TAG);	\
	printf(__VA_ARGS__);
#elif defined(__linux__)
#	include <unistd.h>

#	define ESP_LOGE(LOG_TAG, ...)		\
	printf("ERROR:	[%s] ", LOG_TAG);	\
	printf(__VA_ARGS__);

#	define ESP_LOGI(LOG_TAG, ...)		\
	printf("INFO:	[%s] ", LOG_TAG);	\
	printf(__VA_ARGS__);

#	define ESP_LOGD(LOG_TAG, ...)		\
	printf("DEBUG:	[%s] ", LOG_TAG);	\
	printf(__VA_ARGS__);

#	ifndef INVALID_SOCKET
#		define INVALID_SOCKET  (SOCKET)(~0)
#	endif

#	define SOCKET int

#	define closesocket close
#else // ESP32
#	include "esp_log.h"

#	include "lwip/sys.h"
#	include "lwip/api.h"
#	include "lwip/err.h"
#	include "lwip/netdb.h"

#	ifndef INVALID_SOCKET
#		define INVALID_SOCKET  (SOCKET)(~0)
#	endif

#	define SOCKET int
#endif

#define FTPSERVER_DEFAULT_PORT	21

//

namespace ftp_server
{

enum e_encoding
{
	e_encoding_utf8,	// linux ?
	e_encoding_utf16,	// ESP32 long files name
	e_encoding_win1251	// Windows
};

enum e_data_transfer_mode
{
	e_data_transfer_mode_ascii,
	e_data_transfer_mode_binary
};

enum e_data_channel_mode
{
	e_data_channel_mode_active,
	e_data_channel_mode_passive
};

class ftp_server_c
{
	class ftp_client_connection_c
		: public std::enable_shared_from_this<ftp_client_connection_c>
	{
	private:
		ftp_client_connection_c() = delete;
		ftp_client_connection_c(ftp_client_connection_c&&) = delete;
		ftp_client_connection_c(const ftp_client_connection_c&) = delete;

	public:
		ftp_client_connection_c(SOCKET command_socket)
			: m_command_socket(command_socket)
			, m_data_socket(0)
			, m_data_transfer_mode(e_data_transfer_mode_binary)
			, m_data_channel_mode(e_data_channel_mode_active)
		{
		}

		virtual ~ftp_client_connection_c()
		{
			if (m_command_socket)
			{
				closesocket(m_command_socket);
				m_command_socket = 0;
			}

			if (m_data_socket)
			{
				closesocket(m_data_socket);
				m_data_socket = 0;
			}
		}

		void assign_data_socket(SOCKET data_socket) { m_data_socket = data_socket; }

		SOCKET command_socket() const { return m_command_socket; }

		SOCKET data_socket() const { return m_data_socket; }

		bool set_ftp_root_directory(const std::string& path) { return m_directory_iterator.set_root(path); }

		filesystem_tools::directory_iterator_c& get_directory_iterator() { return m_directory_iterator; }

		std::string current_directory() { return m_directory_iterator.absolute_path(); }

		void set_encoding(e_encoding encoding) { m_current_encoding = encoding; }
		e_encoding current_encoding() const { return m_current_encoding; }

		void set_rename_file_path(const std::string& path) { m_last_rename_from_file = path; }
		const std::string rename_file_path() const { return m_last_rename_from_file; }

		void set_data_transfer_mode(e_data_transfer_mode data_transfer_mode) { m_data_transfer_mode = data_transfer_mode; }
		e_data_transfer_mode data_transfer_mode() { return m_data_transfer_mode; }

		void set_data_channel_mode(e_data_channel_mode data_channel_mode) { m_data_channel_mode = data_channel_mode; }
		e_data_channel_mode data_channel_mode() { return m_data_channel_mode; }

	protected:
		SOCKET m_command_socket, m_data_socket;

		filesystem_tools::directory_iterator_c m_directory_iterator;

		e_encoding m_current_encoding;

		std::string m_last_rename_from_file;

		e_data_transfer_mode m_data_transfer_mode;
		e_data_channel_mode m_data_channel_mode;
	};

	typedef std::shared_ptr<ftp_client_connection_c> ftp_client_connection_t;

	enum e_command_types
	{
		e_ftpcmd_unknown = 0,

		e_ftpcmd_user,
		e_ftpcmd_pass,
		e_ftpcmd_opts,
		e_ftpcmd_pwd,
		e_ftpcmd_type,
		e_ftpcmd_cwd,
		e_ftpcmd_pasv,
		e_ftpcmd_list,
		e_ftpcmd_syst,
		e_ftpcmd_feat,
		e_ftpcmd_help,
		e_ftpcmd_noop,
		e_ftpcmd_delete,
		e_ftpcmd_cdup,
		e_ftpcmd_retr,
		e_ftpcmd_size,
		e_ftpcmd_mkd,
		e_ftpcmd_rnfr,
		e_ftpcmd_rnto,
		e_ftpcmd_rmd,
		e_ftpcmd_stor
	};

	struct port_busy_flag_s
	{
		uint16_t port;
		bool busy;
	};

private:
	ftp_server_c(const ftp_server_c&) = delete;
	ftp_server_c(ftp_server_c&&) = delete;
	ftp_server_c& operator=(ftp_server_c&) = delete;

public:
	ftp_server_c();

	virtual ~ftp_server_c();

	virtual void set_homedir(const std::string& abs_path);

	virtual std::string homedir() const;
	
	virtual void set_native_encoding(e_encoding encoding) { m_native_encoding = encoding; }
	e_encoding naive_encoding() const { return m_native_encoding; }

	virtual bool start(uint16_t port = FTPSERVER_DEFAULT_PORT);

	virtual void stop();

protected:
	virtual void server_routine();

	virtual void remove_client_connection(SOCKET sock);

	virtual ftp_client_connection_t find_connection_by_socket(SOCKET sock);

	virtual void handle_connection(SOCKET client_socket);

	virtual uint16_t get_first_free_port();
	virtual void mark_port_as_free(uint16_t port_number);
	virtual void get_ip_data(int sock, uint32_t* ip);
	virtual void gen_port(uint16_t& p1, uint16_t& p2);

	virtual void translate_path(ftp_client_connection_c* client_connection,
		std::string& path,
		e_encoding source_encoding,
		e_encoding dest_encoding);

	virtual bool initialize_sock_channel(SOCKET& sock, uint16_t port, bool non_blocking_sock);

	virtual bool send_to_client(SOCKET client_socket, const char* data, size_t data_size);
	virtual bool send_to_client(ftp_client_connection_c* client_connection, char* data);
	virtual bool send_system_error(ftp_client_connection_c* client_connection);

	virtual void handle_incoming_data(ftp_client_connection_c* client_connection,
		uint8_t* data, size_t data_size);

	virtual e_command_types determine_command(const std::string& command_name);

	virtual void handle_command(ftp_client_connection_c* client_connection,
		e_command_types command,
		const std::string& command_value);

	virtual void handle_list_command(ftp_client_connection_c* client_connection);

	virtual void handle_retr_command(ftp_client_connection_c* client_connection,
		const std::string& command_value);

	virtual void handle_stor_command(ftp_client_connection_c* client_connection,
		const std::string& command_value);

protected:
	std::string m_home_dir;

	SOCKET m_listen_socket = 0;

	volatile bool m_working = false;

	std::vector<ftp_client_connection_t> m_client_connections;

	std::vector<port_busy_flag_s> m_data_channel_ports_map;

	e_encoding m_native_encoding;
};

}
