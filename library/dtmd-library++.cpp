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

#include <dtmd-library++.hpp>

#include <stdexcept>

namespace dtmd {

command::command(const dtmd_command_t *cmd)
{
	if (cmd != NULL)
	{
		this->fillFromCmd(cmd);
	}
}

void command::fillFromCmd(const dtmd_command_t *cmd)
{
	if (cmd != NULL)
	{
		this->cmd = cmd->cmd;

		if (cmd->args != NULL)
		{
			this->args.reserve(cmd->args_count);

			for (unsigned int i = 0; i < cmd->args_count; ++i)
			{
				if (cmd->args[i] != NULL)
				{
					this->args.push_back(cmd->args[i]);
				}
				else
				{
					this->args.push_back(std::string());
				}
			}
		}
	}
	else
	{
		this->clear();
	}
}

void command::clear()
{
	this->cmd.clear();
	this->args.clear();
}

bool command::isEmpty() const
{
	return (this->cmd.empty() && this->args.empty());
}

partition::partition(const dtmd_partition_t *part)
{
	if (part != NULL)
	{
		this->fillFromPartition(part);
	}
}

void partition::fillFromPartition(const dtmd_partition_t *part)
{
	if (part != NULL)
	{
		this->path      = part->path;
		this->type      = part->type;
		this->label     = part->label;
		this->mnt_point = part->mnt_point;
		this->mnt_opts  = part->mnt_opts;
	}
	else
	{
		this->clear();
	}
}

void partition::clear()
{
	this->path.clear();
	this->type.clear();
	this->label.clear();
	this->mnt_point.clear();
	this->mnt_opts.clear();
}

bool partition::isEmpty() const
{
	return (this->path.empty()
		&& this->type.empty()
		&& this->label.empty()
		&& this->mnt_point.empty()
		&& this->mnt_opts.empty());
}

device::device(const dtmd_device_t *dev)
{
	if (dev != NULL)
	{
		fillFromDevice(dev);
	}
}

void device::fillFromDevice(const dtmd_device_t *dev)
{
	if (dev != NULL)
	{
		this->path = dev->path;
		this->type = dev->type;

		if (dev->partition != NULL)
		{
			this->partitions.reserve(dev->partitions_count);

			for (unsigned int i = 0; i < dev->partitions_count; ++i)
			{
				this->partitions.push_back(partition(dev->partition[i]));
			}
		}
	}
	else
	{
		this->clear();
	}
}

void device::clear()
{
	this->path.clear();
	type = unknown_or_persistent;
	partitions.clear();
}

bool device::isEmpty() const
{
	return (this->path.empty()
		&& (this->type == unknown_or_persistent)
		&& partitions.empty());
}

library::library()
	: m_handle(NULL),
	m_cb(NULL),
	m_arg(NULL)
{
}

library::library(callback cb, void *arg)
	: m_handle(NULL),
	m_cb(cb),
	m_arg(arg)
{
	m_handle = dtmd_init(&library::local_callback, this, NULL);
	if (m_handle == NULL)
	{
		throw std::runtime_error("Couldn't initialize dtmd library");
	}
}

library::~library()
{
	if (m_handle != NULL)
	{
		dtmd_deinit(m_handle);
	}
}

dtmd_result_t library::enum_devices(int timeout, std::vector<device> &devices)
{
	dtmd_result_t result;
	unsigned int count;
	dtmd_device_t **devices_array;

	result = dtmd_enum_devices(this->m_handle, timeout, &count, &devices_array);

	if (result == dtmd_ok)
	{
		try
		{
			devices.clear();
			devices.reserve(count);

			for (unsigned int i = 0; i < count; ++i)
			{
				devices.push_back(device(devices_array[i]));
			}

			dtmd_free_devices_array(this->m_handle, count, devices_array);
		}
		catch (...)
		{
			dtmd_free_devices_array(this->m_handle, count, devices_array);
			throw;
		}
	}

	return result;
}

dtmd_result_t library::list_device(int timeout, const std::string &device_path, device &result_device)
{
	dtmd_result_t result;
	dtmd_device_t *device;

	result = dtmd_list_device(this->m_handle, timeout, device_path.c_str(), &device);

	if (result == dtmd_ok)
	{
		try
		{
			result_device.fillFromDevice(device);
			dtmd_free_device(this->m_handle, device);
		}
		catch (...)
		{
			dtmd_free_device(this->m_handle, device);
			throw;
		}
	}

	return result;
}

dtmd_result_t library::list_partition(int timeout, const std::string &partition_path, partition &result_partition)
{
	dtmd_result_t result;
	dtmd_partition_t *partition;

	result = dtmd_list_partition(this->m_handle, timeout, partition_path.c_str(), &partition);

	if (result == dtmd_ok)
	{
		try
		{
			result_partition.fillFromPartition(partition);
			dtmd_free_partition(this->m_handle, partition);
		}
		catch (...)
		{
			dtmd_free_partition(this->m_handle, partition);
			throw;
		}
	}

	return result;
}

dtmd_result_t library::mount(int timeout, const std::string &path, const std::string &mount_options)
{
	return dtmd_mount(this->m_handle, timeout, path.c_str(), mount_options.c_str());
}

dtmd_result_t library::unmount(int timeout, const std::string &path)
{
	return dtmd_unmount(this->m_handle, timeout, path.c_str());
}

bool library::isStateInvalid() const
{
	return dtmd_is_state_invalid(this->m_handle);
}

void library::local_callback(void *arg, const dtmd_command_t *cmd)
{
	library *lib = (library*) arg;
	command local_command(cmd);

	lib->m_cb(lib->m_arg, local_command);
}

} // namespace dtmd
