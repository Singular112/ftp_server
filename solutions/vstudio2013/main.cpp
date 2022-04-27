#include "stdafx.h"

#include <string>

#include "../../src/ftp_server.h"

#include "../../src/filesystem_tools.h"
using namespace filesystem_tools::helpers;

#include <sys/stat.h>
#include <direct.h>


void directory_iterator_test()
{
	//std::string path = "D:\\Projects\\\\\\ftp_server\\FTP";
	std::string path = "/Projects\\\\\\ftp_server\\FTP";
	auto tokens = filesystem_tools::helpers::split_path(path);

	auto directory_name = filesystem_tools::helpers::directory_name_from_path(path);

	auto check = filesystem_tools::helpers::rebuild_path(path);

	filesystem_tools::directory_iterator_c iterator;
	iterator.set_root(path);
	auto p1 = iterator.absolute_path();
	auto p2 = iterator.relative_path();
	iterator.change_dir("1234//sfdgsfdg");
	auto p3 = iterator.absolute_path();
	auto p4 = iterator.relative_path();

	iterator.change_dir("../789/");
	auto p5 = iterator.absolute_path();
	auto p6 = iterator.relative_path();

	iterator.change_dir("../../1234");
	auto p7 = iterator.absolute_path();
	auto p8 = iterator.relative_path();

	iterator.enum_files
	(
		[&](const filesystem_tools::directory_iterator_c::entity_info_s& entity) -> bool
		{
			if (entity.attributes & filesystem_tools::directory_iterator_c::e_attribute_directory)
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

			return true;	// true = continue, false = interrupt
		},

		iterator.absolute_path()
	);
}


int _tmain(int argc, _TCHAR* argv[])
{
	setlocale(LC_ALL, "rus");

	//directory_iterator_test();

	const char* basedir = "/FTP";
	uint16_t port = 21;

	ftp_server::ftp_server_c ftp_server;

	//ftp_server.set_homedir(application_directory() + basedir);
	ftp_server.set_homedir("D:\\Projects\\Research\\simplest http server\\x64\\Debug\\webroot");

	ftp_server.set_native_encoding(ftp_server::e_encoding_win1251);

	ftp_server.start(port);

	_getch();

	return 0;
}
