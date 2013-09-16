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

namespace dtmd {

class command
{
public:
	command(const dtmd_command_t *cmd);

	void fillFromCmd(const dtmd_command_t *cmd);
	void clear();

	bool isEmpty() const;

	std::string cmd;
	std::vector<std::string> args;
};

class partition
{
public:
	partition(const dtmd_partition_t *part);

	void fillFromPartition(const dtmd_partition_t *part);
	void clear();

	bool isEmpty() const;

	std::string path;
	std::string type;
	std::string label;
	std::string mnt_point;
	std::string mnt_opts;
};

class device
{
public:
	device(const dtmd_device_t *dev);

	void fillFromDevice(const dtmd_device_t *dev);
	void clear();

	bool isEmpty() const;

	std::string path;
	dtmd_removable_media_type_t type;
	std::vector<partition> partitions;
};

typedef void (*callback)(void *arg, const command &cmd);

class library
{
public:
	library(callback cb, void *arg);
	~library();

	dtmd_result_t enum_devices(int timeout, std::vector<device> &devices);
	dtmd_result_t list_device(int timeout, const std::string &device_path, device &result_device);
	dtmd_result_t list_partition(int timeout, const std::string &partition_path, partition &result_partition);
	dtmd_result_t mount(int timeout, const std::string &path, const std::string &mount_options = std::string());
	dtmd_result_t unmount(int timeout, const std::string &path);

	bool isStateInvalid() const;

private:
	// delete default constructor
	library();

	static void local_callback(void *arg, const dtmd_command_t *cmd);

	dtmd_t *m_handle;
	callback m_cb;
	void *m_arg;
};

} // namespace dtmd

#endif /* DTMD_LIBRARY_CXX_HPP */
