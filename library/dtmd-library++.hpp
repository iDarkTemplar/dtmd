/*
 * Copyright (C) 2016 i.Dark_Templar <darktemplar@dark-templar-archives.net>
 *
 * This file is part of DTMD, Dark Templar Mount Daemon.
 *
 * DTMD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * DTMD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with DTMD.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef DTMD_LIBRARY_CXX_HPP
#define DTMD_LIBRARY_CXX_HPP

#include <dtmd.h>
#include <dtmd-misc.h>
#include <dtmd-library.h>

#include <string>
#include <vector>
#include <set>
#include <memory>

namespace dtmd {

const int timeout_infinite = dtmd_library_timeout_infinite;

template <typename T>
struct shared_ptr_value_less: std::binary_function<std::shared_ptr<T>, std::shared_ptr<T>, bool>
{
	bool operator() (const std::shared_ptr<T> &left, const std::shared_ptr<T> &right) const;
};

class removable_media;

typedef std::set<std::shared_ptr<removable_media>, shared_ptr_value_less<removable_media> > removable_media_container;

class command
{
public:
	command();
	explicit command(const dt_command_t *raw_cmd);

	command(const command &other) = default;
	command& operator=(const command &other) = default;

	virtual ~command();

	void fillFromCmd(const dt_command_t *raw_cmd);
	void clear();

	bool isEmpty() const;

	std::string cmd;
	std::vector<std::string> args;
};

class removable_media: public std::enable_shared_from_this<removable_media>
{
protected:
	struct removable_media_private
	{
		explicit removable_media_private(int) {}
	};

public:
	explicit removable_media(const removable_media_private &);
	explicit removable_media(const removable_media_private &,
		const std::shared_ptr<removable_media> &l_parent,
		const std::string &l_path,
		dtmd_removable_media_type_t l_type,
		dtmd_removable_media_subtype_t l_subtype,
		dtmd_removable_media_state_t l_state,
		const std::string &l_fstype = std::string(),
		const std::string &l_label = std::string(),
		const std::string &l_mnt_point = std::string(),
		const std::string &l_mnt_opts = std::string());

	virtual ~removable_media();

	// Next function copies data of current node, but leaves parent pointer and children list intact
	void copyFromRemovableMedia(const removable_media &other);

	static std::shared_ptr<removable_media> createFromRemovableMedia(const dtmd_removable_media_t *raw_removable_media);

	template <typename... T>
	static std::shared_ptr<removable_media> create(T &&...args)
	{
		return std::make_shared<removable_media>(removable_media_private{0}, std::forward<T>(args)...);
	}

	bool operator<(const removable_media &other) const;

	void fillFromRemovableMedia(const dtmd_removable_media_t *raw_removable_media);
	void clear();

	std::string getParentPath() const;
	bool isEmpty() const;

	dtmd_removable_media_type_t getValidType() const;

	std::string path;
	dtmd_removable_media_type_t type;
	dtmd_removable_media_subtype_t subtype;
	dtmd_removable_media_state_t state;
	std::string fstype;
	std::string label;
	std::string mnt_point;
	std::string mnt_opts;

	std::weak_ptr<removable_media> parent;
	removable_media_container children;

private:
	removable_media(const removable_media &other) = delete;
	removable_media& operator=(const removable_media &other) = delete;
};

class library;

typedef void (*callback)(const library &library_instance, void *arg, const command &cmd);
typedef void (*state_callback)(const library &library_instance, void *arg, dtmd_state_t state);

class library
{
public:
	library(callback cb, state_callback state_cb, void *arg);
	virtual ~library();

	dtmd_result_t list_all_removable_devices(int timeout, removable_media_container &removable_devices_list);
	dtmd_result_t list_removable_device(int timeout, const std::string &removable_device_path, removable_media_container &removable_devices_list);
	dtmd_result_t mount(int timeout, const std::string &path);
	dtmd_result_t mount(int timeout, const std::string &path, const std::string &mount_options);
	dtmd_result_t unmount(int timeout, const std::string &path);
	dtmd_result_t list_supported_filesystems(int timeout, std::vector<std::string> &supported_filesystems_list);
	dtmd_result_t list_supported_filesystem_options(int timeout, const std::string &filesystem, std::vector<std::string> &supported_filesystem_options_list);

	dtmd_result_t fill_removable_device_from_notification(const command &cmd, std::shared_ptr<removable_media> &removable_device) const;

	bool isStateInvalid() const;
	bool isNotificationValidRemovableDevice(const command &cmd) const;
	dtmd_error_code_t getCodeOfCommandFail() const;

private:
	// delete default constructor, copy constructor and assign operator
	library() = delete;
	library(const library &other) = delete;
	library& operator=(const library &other) = delete;

	static void local_callback(dtmd_t *library_ptr, void *arg, const dt_command_t *cmd);
	static void local_state_callback(dtmd_t *library_ptr, void *arg, dtmd_state_t state);

	dtmd_t *m_handle;
	callback m_cb;
	state_callback m_state_cb;
	void *m_arg;
};

std::shared_ptr<removable_media> find_removable_media(const std::string &path, const removable_media_container &root);
std::shared_ptr<removable_media> find_removable_media(const std::string &path, const std::shared_ptr<removable_media> &root);

} // namespace dtmd

#endif /* DTMD_LIBRARY_CXX_HPP */
