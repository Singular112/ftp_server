/*
 *	Author: Ilia Vasilchikov
 *	mail: gravity@hotmail.ru
 *	gihub page: https://github.com/Singular112/
 *	Licence: MIT
*/

#pragma once

// stl
#include <time.h>
#include <string>
#include <vector>
#include <stdint.h>

// os specific
#ifdef WIN32
#	ifndef WIN32_LEAN_AND_MEAN
#		define WIN32_LEAN_AND_MEAN
#	endif
#	include <Windows.h>
#else
#	include <dirent.h>
#	include <sys/stat.h>
#endif

//

namespace filesystem_tools
{

namespace helpers
{

std::vector<std::string> split_path(const std::string& path);

std::string rebuild_path(const std::string& path);

std::string get_directory_path(const std::string& fname);

std::string application_directory();

std::string directory_name_from_path(const std::string path);

bool check_directory_exists(const std::string& path);

time_t convert_windows_time_to_unix_time(long long int input);

bool string_starts_with(const std::string& s1, const std::string& s2);

bool strings_iequals(const std::string& s1, const std::string& s2);

bool remove_directory_r(const std::string& path, bool remove_files);

//

namespace linked_list
{

template <typename T>
struct list_node_s
{
	T* prev_node;
	T* next_node;
};

template <typename T>
struct linked_list_s
{
	T* iterator;

	T* first_node;
	T* last_node;
};

//

template <typename T>
void linkedlist_initialize(linked_list_s<T>* list)
{
	list->iterator = nullptr;

	list->first_node = nullptr;
	list->last_node = nullptr;
}


template <typename T>
void linkedlist_erase_node(linked_list_s<T>* list, T* node)
{
	if (node->prev_node == nullptr)
	{
		if (node->next_node)
		{
			node->next_node->prev_node = nullptr;

			list->first_node = node->next_node;
		}

		return;
	}
	else if (node->next_node == nullptr)
	{
		list->last_node = node->prev_node;

		node->prev_node->next_node = nullptr;

		return;
	}

	node->prev_node->next_node = node->next_node;
	node->next_node->prev_node = node->prev_node;
}


template <typename T>
void linkedlist_insert_back(linked_list_s<T>* list, T* node)
{
	if (list->last_node)
	{
		list->last_node->next_node = node;
		node->prev_node = list->last_node;
	}
	else
	{
		node->prev_node = nullptr;
		list->first_node = node;
	}

	list->last_node = node;
	node->next_node = nullptr;
}


template <typename T>
void linkedlist_insert_front(linked_list_s<T>* list, T* node)
{
	if (list->first_node)
	{
		node->next_node = list->first_node;
		list->first_node->prev_node = node;
	}
	else
	{
		node->next_node = nullptr;
		list->last_node = node;
	}

	list->first_node = node;
	node->prev_node = nullptr;
}


template <typename T>
void linkedlist_swap_nodes(linked_list_s<T>* list,
	T* node1, T* node2)
{
	std::swap(node1->next_node, node2->next_node);
	std::swap(node1->prev_node, node2->prev_node);

	if (node1->next_node == nullptr)
	{
		list->last_node = node1;
	}
	else if (node2->next_node == nullptr)
	{
		list->last_node = node2;
	}

	if (node1->prev_node == nullptr)
	{
		list->first_node = node1;
	}
	else if (node2->prev_node == nullptr)
	{
		list->first_node = node2;
	}
}


template <typename T>
void linkedlist_reset_iterator(linked_list_s<T>* list)
{
	list->iterator = nullptr;
}


template <typename T>
T* linkedlist_iterate(linked_list_s<T>* list)
{
	if (list->iterator == nullptr)
	{
		return (list->iterator = list->first_node);
	}

	list->iterator = list->iterator->next_node;

	return list->iterator;
}

}

}


class directory_iterator_c
{
public:
	struct directory_node_s
		: public helpers::linked_list::list_node_s<directory_node_s>
	{
		std::string name;
		int16_t level;
	};

#ifdef WIN32
	enum e_attributes : uint32_t
	{
		e_attribute_readonly			= 0x00000001,
		e_attribute_hidden				= 0x00000002,
		e_attribute_system				= 0x00000004,
		e_attribute_directory			= 0x00000010,
		e_attribute_archive				= 0x00000020,
		e_attribute_device				= 0x00000040,
		e_attribute_normal				= 0x00000080,
		e_attribute_temporary			= 0x00000100,
		e_attribute_sparse_file			= 0x00000200,
		e_attribute_reparse_point		= 0x00000400,
		e_attribute_compressed			= 0x00000800,
		e_attribute_offline				= 0x00001000,
		e_attribute_not_content_indexed	= 0x00002000,
		e_attribute_encrypted			= 0x00004000,
		e_attribute_integrity_stream	= 0x00008000,
		e_attribute_virtual				= 0x00010000,
		e_attribute_no_scrub_data		= 0x00020000,
		e_attribute_ea					= 0x00040000
	};
#else
	enum e_attributes : uint32_t
	{
		e_attribute_0					= 0x00000000,
		e_attribute_readonly			= 0x00000001,
		e_attribute_directory			= 0x00000010
	};
#endif

	struct entity_info_s
	{
		std::string name;
		e_attributes attributes;
		uint32_t file_size_bytes;
		struct tm write_time;
	};

public:
	directory_iterator_c();

	virtual ~directory_iterator_c();

	bool set_root(const std::string& absolute_path);

	bool change_dir(const std::string& relative_path);

	void move_prev_dir();

	void move_to_root();

	int current_level() const;

	std::string absolute_path();

	std::string relative_path();

#ifdef WIN32
	// callback prototype for example: bool(const entity_info_s&);
	template <typename CallbackT>
	bool enum_files(CallbackT callback,
		const std::string& abs_path)
	{
		std::string enum_filter = abs_path.empty() ?
			absolute_path() :
			helpers::rebuild_path(abs_path);
		enum_filter += "*";

		WIN32_FIND_DATAA ffd;
		HANDLE hfind = FindFirstFileA(enum_filter.c_str(), &ffd);

		if (hfind == INVALID_HANDLE_VALUE)
		{
			return false;
		}

		do
		{
			if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY
				&& ffd.cFileName[0] == '.')
			{
				continue;
			}

			entity_info_s file_info;
			{
				file_info.attributes = (decltype(file_info.attributes))ffd.dwFileAttributes;
				file_info.name = ffd.cFileName;
				file_info.file_size_bytes = ((int64_t)ffd.nFileSizeHigh << 32) | (int64_t)ffd.nFileSizeLow;

				auto last_write_time = ((int64_t)ffd.ftLastWriteTime.dwHighDateTime << 32)
					| (int64_t)ffd.ftLastWriteTime.dwLowDateTime;
				auto write_time_total_seconds = helpers::convert_windows_time_to_unix_time(last_write_time);
				file_info.write_time = *localtime(&write_time_total_seconds);
			}

			if (!callback(file_info))
				break;
		}
		while (FindNextFileA(hfind, &ffd) != 0);

		return true;
	}
#else
	// callback prototype for example: bool(const entity_info_s&);
	template <typename CallbackT>
	bool enum_files(CallbackT callback,
		const std::string& abs_path)
	{
		std::string target_path = abs_path.empty() ?
			absolute_path() : abs_path;

		// fix esp-idf bug
		{
			if (target_path[target_path.size() - 1] == '/')
			{
				target_path.resize(target_path.size()- 1);
			}
		}

		struct dirent* entry = nullptr;

		DIR* dp = opendir(target_path.c_str());

		if (dp == nullptr)
			return false;

		while ((entry = readdir(dp)))
		{				
			entity_info_s file_info;
			{
				std::string full_file_path = helpers::rebuild_path(target_path) + entry->d_name;

				struct stat st;
				if (stat(full_file_path.c_str(), &st) != 0)
				{
					printf("stat failed for file %s\n",
						full_file_path.c_str());
					continue;
				}

				file_info.name = entry->d_name;
				file_info.file_size_bytes = (decltype(file_info.file_size_bytes))st.st_size;
				file_info.write_time = *localtime(&st.st_mtime);
#if 0
				file_info.attributes = (decltype(file_info.attributes))st.st_mode/* & _IFMT*/;
#else
				file_info.attributes = e_attribute_0;

				if (entry->d_type == DT_DIR)
				{
					file_info.attributes = e_attribute_directory;
				}
#endif
			}

			if (!callback(file_info))
				break;
		}

		closedir(dp);

		return true;
	}
#endif

private:
	directory_node_s m_root_node;

	std::string m_root_path;

	directory_node_s* m_current_node;

	helpers::linked_list::linked_list_s<directory_node_s> m_hierarchy;
};

}
