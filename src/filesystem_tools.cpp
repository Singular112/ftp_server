/*
 *	Author: Ilia Vasilchikov
 *	mail: gravity@hotmail.ru
 *	gihub page: https://github.com/Singular112/
 *	Licence: MIT
*/

#include "filesystem_tools.h"

// system
#if defined(__linux__)
#	include <limits.h>			// PATH_MAX
#	include <libgen.h>			// dirname
#	include <unistd.h>			// unlink
#	include <sys/stat.h>
#elif defined(WIN32)
#	include <Windows.h>
#	include <direct.h>
#else // ESP32
#	include <libgen.h>			// dirname
#	include <unistd.h>			// unlink
#	include <sys/stat.h>
#	include <sys/syslimits.h>	// PATH_MAX
#endif

#include <vector>
#include <time.h>
#include <stdio.h>

//
#ifdef WIN32
#	define PATH_SLASH_TYPE	"\\"
#else
#	define PATH_SLASH_TYPE	"/"
#endif

//
using namespace filesystem_tools::helpers::linked_list;

//

namespace filesystem_tools
{

namespace helpers
{

std::vector<std::string> split_path(const std::string& path)
{
	std::vector<std::string> result;

	auto str_pbegin = path.c_str();
	auto str_pend = str_pbegin;

	auto str_iterator = str_pbegin;

	while (*str_iterator)
	{
		if (*str_iterator == '/' ||
			*str_iterator == '\\')
		{
			str_pend = str_iterator++;

			if (str_pbegin != str_pend)
				result.emplace_back(str_pbegin, str_pend);

			str_pbegin = str_iterator;

			continue;
		}

		++str_iterator;
	}

	if (str_pbegin != str_iterator)
		result.emplace_back(str_pbegin, str_iterator++);

	return result;
}


std::string rebuild_path(const std::string& path)
{
	if (path.size() == 0)
	{
		return std::string();
	}

	std::string str;

	if (path[0] == '/')
		str = "/";

	auto tokens = split_path(path);
	for (auto& token : tokens)
	{
		str += token;
		str += PATH_SLASH_TYPE;
	}

	return str;
}


std::string get_directory_path(const std::string& fname)
{
	size_t index = fname.find_last_of("\\/");

	return index == std::string::npos ?
		std::string() :
		fname.substr(0, index);
}


std::string application_directory()
{
#if defined(WIN32)
	char buffer[MAX_PATH] = { 0 };
	GetModuleFileNameA(NULL, buffer, MAX_PATH);
	return get_directory_path(buffer);
#elif defined(__linux__)
	char result[PATH_MAX];
	ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
	return std::string(result, (count > 0) ? count : 0);
#else
	return "/";
#endif
}


std::string directory_name_from_path(const std::string path)
{
	auto ssize = path.size();

	if (ssize == 0)
		return {};

	auto indx = path.find_last_of("\\/");

	if (indx == ssize - 1)
	{
		indx = path.find_last_of("\\/", indx - 1);

		return path.substr(indx + 1, ssize - indx - 2);
	}
	else
	{
		return indx == std::string::npos ?
			path : path.substr(indx + 1, ssize - indx);
	}
}


bool check_directory_exists(const std::string& path)
{
#ifdef WIN32
	DWORD file_attr = GetFileAttributesA(path.c_str());
	if (file_attr == INVALID_FILE_ATTRIBUTES)
		return false;

	return (file_attr & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY;
#else
#ifndef __linux__ // ESP32
	std::string fixed_path = path;
	// fix bug in esp-idf api
	if (fixed_path[fixed_path.size() - 1] == '/')
	{
		fixed_path.resize(fixed_path.size() - 1);
	}
#else
	std::string& fixed_path = path;
#endif

	struct stat st;
	bool ok = (stat(fixed_path.c_str(), &st) == 0)
		&& S_ISDIR(st.st_mode);

	//printf("change_dir (%s): %d\n", fixed_path.c_str(), (int)ok);

	return ok;
#endif
}


// https://stackoverflow.com/a/6161822
#define TICKS_PER_SECOND 10000000
#define EPOCH_DIFFERENCE 11644473600LL
time_t convert_windows_time_to_unix_time(long long int input)
{
	long long int temp;
	temp = input / TICKS_PER_SECOND;	//convert from 100ns intervals to seconds;
	temp = temp - EPOCH_DIFFERENCE;		//subtract number of seconds between epochs
	return (time_t)temp;
}


bool string_starts_with(const std::string& s1, const std::string& s2)
{
	return s1.rfind(s2, 0) == 0;
}


bool strings_iequals(const std::string& s1, const std::string& s2)
{
	return
	(
		s1.size() == s2.size()) &&

		std::equal(s1.begin(), s1.end(), s2.begin(), [&](char c1, char c2)
		{
			return (c1 == c2) || (::tolower(c1) == ::tolower(c2));
		}
	);
}


bool remove_directory_r(const std::string& path, bool remove_files)
{
	if (path.empty())
		return false;

#ifdef WIN32
	std::string target_path = helpers::rebuild_path(path);
	target_path += "*";

	WIN32_FIND_DATAA ffd;
	HANDLE hfind = FindFirstFileA(target_path.c_str(), &ffd);

	if (hfind == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	do
	{
		if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (ffd.cFileName[0] == '.')
			{
				continue;
			}

			std::string subdir_path = helpers::rebuild_path(path) + ffd.cFileName;

			remove_directory_r(subdir_path, remove_files);
		}
		else if (remove_files)
		{
			std::string file_path = helpers::rebuild_path(path) + ffd.cFileName;

			unlink(file_path.c_str());
		}
	}
	while (FindNextFileA(hfind, &ffd) != 0);

	rmdir(path.c_str());

	return true;
#else
	std::string target_path = helpers::rebuild_path(path);
#ifndef __linux__ // ESP32
	// fix esp-idf bug
	{
		if (target_path[target_path.size() - 1] == '/')
		{
			target_path.resize(target_path.size()- 1);
		}
	}
#endif

	struct dirent* entry = nullptr;

	DIR* dp = opendir(target_path.c_str());

	if (dp == nullptr)
		return false;

	while ((entry = readdir(dp)))
	{				
		if (entry->d_type == DT_DIR)
		{
			if (entry->d_name[0] == '.')
			{
				continue;
			}

			std::string subdir_path = helpers::rebuild_path(path) + entry->d_name;

			remove_directory_r(subdir_path, remove_files);
		}
		else if (remove_files)
		{
			std::string file_path = helpers::rebuild_path(path) + entry->d_name;

			unlink(file_path.c_str());
		}
	}

	closedir(dp);

	rmdir(target_path.c_str());

	return true;
#endif
}

}


directory_iterator_c::directory_iterator_c()
{
	m_current_node = &m_root_node;

	m_root_node.level = 0;

	linkedlist_initialize(&m_hierarchy);
}


directory_iterator_c::~directory_iterator_c()
{
	// free linked list memory
	while (m_current_node != &m_root_node)
		move_prev_dir();
}


bool directory_iterator_c::set_root(const std::string& absolute_path)
{
	if (!m_root_path.empty())
		return false;	// already initialized

	m_root_path = absolute_path.empty() ?
		helpers::application_directory() :
		helpers::rebuild_path(absolute_path);

	if (!helpers::check_directory_exists(m_root_path))
	{
		return false;
	}

	auto dir_name = helpers::directory_name_from_path(m_root_path);

	m_current_node = &m_root_node;

	m_root_node.name = dir_name;

	linkedlist_insert_front(&m_hierarchy, &m_root_node);

	return true;
}


bool directory_iterator_c::change_dir(const std::string& relative_path)
{
	// check directory exists first
	if (!helpers::check_directory_exists(helpers::rebuild_path(absolute_path() + relative_path)))
	{
		return false;
	}

	auto dir_levels = helpers::split_path(relative_path);

	auto dir_levels_count = dir_levels.size();

	for (uint32_t i = 0; i < dir_levels_count; ++i)
	{
		const auto& dir_level = dir_levels[i];

		if (dir_level == "..")
		{
			// go to prev level
			move_prev_dir();
			continue;
		}

		// move forward
		{
			directory_node_s* next_node = new directory_node_s();
			{
				next_node->level = m_current_node->level + 1;
				next_node->name = dir_level;
			}
			linkedlist_insert_back(&m_hierarchy, next_node);

			m_current_node = next_node;
		}
	}

	return true;
}


void directory_iterator_c::move_prev_dir()
{
	if (m_current_node == &m_root_node)
		return;

	auto prev_node = m_current_node->prev_node;

	linkedlist_erase_node(&m_hierarchy, m_current_node);

	delete m_current_node;

	m_current_node = prev_node;
}


void directory_iterator_c::move_to_root()
{
	while (m_current_node != &m_root_node)
	{
		move_prev_dir();
	}
}


int directory_iterator_c::current_level() const
{
	return m_current_node->level;
}


std::string directory_iterator_c::absolute_path()
{
	if (m_hierarchy.first_node == m_hierarchy.last_node)
	{
		return m_root_path;
	}

	std::string abs_path = m_root_path;

	// initialize iterator
	linkedlist_reset_iterator(&m_hierarchy);
	auto it = linkedlist_iterate(&m_hierarchy);

	while (true)
	{
		it = linkedlist_iterate(&m_hierarchy);

		if (!it)
			break;

		abs_path += it->name + PATH_SLASH_TYPE;
	}
	while (it);

	return abs_path;
}


std::string directory_iterator_c::relative_path()
{
	if (m_hierarchy.first_node == m_hierarchy.last_node)
	{
		return "";
	}

	std::string rel_path;

	// initialize iterator
	linkedlist_reset_iterator(&m_hierarchy);
	auto it = linkedlist_iterate(&m_hierarchy);

	while (true)
	{
		it = linkedlist_iterate(&m_hierarchy);

		if (!it)
			break;

		rel_path += it->name + PATH_SLASH_TYPE;
	}
	while (it);

	return rel_path;
}

}
