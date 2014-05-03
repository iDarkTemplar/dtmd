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

command::command()
{
}

command::command(const dtmd_command_t *cmd)
{
	if (cmd != NULL)
	{
		this->fillFromCmd(cmd);
	}
}

command::~command()
{
}

void command::fillFromCmd(const dtmd_command_t *cmd)
{
	this->clear();

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

partition::partition()
{
}

partition::partition(const dtmd_partition_t *part)
{
	this->fillFromPartition(part);
}

partition::partition(const std::string &l_path,
	const std::string &l_fstype,
	const std::string &l_label,
	const std::string &l_mnt_point,
	const std::string &l_mnt_opts)
	: path(l_path),
	fstype(l_fstype),
	label(l_label),
	mnt_point(l_mnt_point),
	mnt_opts(l_mnt_opts)
{
}

partition::~partition()
{
}

void partition::fillFromPartition(const dtmd_partition_t *part)
{
	this->clear();

	if (part != NULL)
	{
		if (part->path != NULL)
		{
			this->path = part->path;
		}

		if (part->fstype != NULL)
		{
			this->fstype = part->fstype;
		}

		if (part->label != NULL)
		{
			this->label = part->label;
		}

		if (part->mnt_point != NULL)
		{
			this->mnt_point = part->mnt_point;
		}

		if (part->mnt_opts != NULL)
		{
			this->mnt_opts = part->mnt_opts;
		}
	}
}

void partition::clear()
{
	this->path.clear();
	this->fstype.clear();
	this->label.clear();
	this->mnt_point.clear();
	this->mnt_opts.clear();
}

bool partition::isEmpty() const
{
	return (this->path.empty()
		&& this->fstype.empty()
		&& this->label.empty()
		&& this->mnt_point.empty()
		&& this->mnt_opts.empty());
}

device::device()
	: type(dtmd_removable_media_unknown_or_persistent)
{
}

device::device(const dtmd_device_t *dev)
{
	fillFromDevice(dev);
}

device::device(const std::string &l_path, dtmd_removable_media_type_t l_type)
	: path(l_path),
	type(l_type)
{
}

device::~device()
{
}

void device::fillFromDevice(const dtmd_device_t *dev)
{
	this->clear();

	if (dev != NULL)
	{
		if (dev->path != NULL)
		{
			this->path = dev->path;
		}

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
}

void device::clear()
{
	this->path.clear();
	this->type = dtmd_removable_media_unknown_or_persistent;
	this->partitions.clear();
}

bool device::isEmpty() const
{
	return (this->path.empty()
		&& (this->type == dtmd_removable_media_unknown_or_persistent)
		&& this->partitions.empty());
}

stateful_device::stateful_device()
	: type(dtmd_removable_media_unknown_or_persistent),
	state(dtmd_removable_media_state_unknown)
{
}

stateful_device::stateful_device(const dtmd_stateful_device_t *dev)
{
	fillFromStatefulDevice(dev);
}

stateful_device::stateful_device(const std::string &l_path,
	dtmd_removable_media_type_t l_type,
	dtmd_removable_media_state_t l_state,
	const std::string &l_fstype,
	const std::string &l_label,
	const std::string &l_mnt_point,
	const std::string &l_mnt_opts)
	: path(l_path),
	type(l_type),
	state(l_state),
	fstype(l_fstype),
	label(l_label),
	mnt_point(l_mnt_point),
	mnt_opts(l_mnt_opts)
{
}

stateful_device::~stateful_device()
{
}

void stateful_device::fillFromStatefulDevice(const dtmd_stateful_device_t *dev)
{
	clear();

	if (dev != NULL)
	{
		if (dev->path != NULL)
		{
			this->path = dev->path;
		}

		this->type  = dev->type;
		this->state = dev->state;

		if (dev->fstype != NULL)
		{
			this->fstype = dev->fstype;
		}

		if (dev->label != NULL)
		{
			this->label = dev->label;
		}

		if (dev->mnt_point != NULL)
		{
			this->mnt_point = dev->mnt_point;
		}

		if (dev->mnt_opts != NULL)
		{
			this->mnt_opts = dev->mnt_opts;
		}
	}
}

void stateful_device::clear()
{
	this->path.clear();
	this->type  = dtmd_removable_media_unknown_or_persistent;
	this->state = dtmd_removable_media_state_unknown;
	this->fstype.clear();
	this->label.clear();
	this->mnt_point.clear();
	this->mnt_opts.clear();
}

bool stateful_device::isEmpty() const
{
	return (this->path.empty()
		&& (this->type  == dtmd_removable_media_unknown_or_persistent)
		&& (this->state == dtmd_removable_media_state_unknown)
		&& this->fstype.empty()
		&& this->label.empty()
		&& this->mnt_point.empty()
		&& this->mnt_opts.empty());
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

dtmd_result_t library::enum_devices(int timeout, std::vector<device> &devices, std::vector<stateful_device> &stateful_devices)
{
	dtmd_result_t result;
	unsigned int count;
	dtmd_device_t **devices_array;
	unsigned int stateful_count;
	dtmd_stateful_device_t **stateful_devices_array;

	result = dtmd_enum_devices(this->m_handle, timeout, &count, &devices_array, &stateful_count, &stateful_devices_array);

	if (result == dtmd_ok)
	{
		try
		{
			devices.clear();
			stateful_devices.clear();

			devices.reserve(count);
			stateful_devices.reserve(stateful_count);

			for (unsigned int i = 0; i < count; ++i)
			{
				devices.push_back(device(devices_array[i]));
			}

			for (unsigned int i = 0; i < stateful_count; ++i)
			{
				stateful_devices.push_back(stateful_device(stateful_devices_array[i]));
			}

			dtmd_free_stateful_devices_array(this->m_handle, stateful_count, stateful_devices_array);
			dtmd_free_devices_array(this->m_handle, count, devices_array);
		}
		catch (...)
		{
			dtmd_free_stateful_devices_array(this->m_handle, stateful_count, stateful_devices_array);
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

dtmd_result_t library::list_stateful_device(int timeout, const std::string &device_path, stateful_device &result_stateful_device)
{
	dtmd_result_t result;
	dtmd_stateful_device_t *stateful_device;

	result = dtmd_list_stateful_device(this->m_handle, timeout, device_path.c_str(), &stateful_device);

	if (result == dtmd_ok)
	{
		try
		{
			result_stateful_device.fillFromStatefulDevice(stateful_device);
			dtmd_free_stateful_device(this->m_handle, stateful_device);
		}
		catch (...)
		{
			dtmd_free_stateful_device(this->m_handle, stateful_device);
			throw;
		}
	}

	return result;
}

dtmd_result_t library::mount(int timeout, const std::string &path)
{
	return dtmd_mount(this->m_handle, timeout, path.c_str(), NULL);
}

dtmd_result_t library::mount(int timeout, const std::string &path, const std::string &mount_options)
{
	return dtmd_mount(this->m_handle, timeout, path.c_str(), mount_options.c_str());
}

dtmd_result_t library::unmount(int timeout, const std::string &path)
{
	return dtmd_unmount(this->m_handle, timeout, path.c_str());
}

dtmd_result_t library::list_supported_filesystems(int timeout, std::vector<std::string> &supported_filesystems_list)
{
	dtmd_result_t result;
	unsigned int supported_filesystems_count;
	const char **supported_filesystems_array;
	unsigned int i;

	result = dtmd_list_supported_filesystems(this->m_handle, timeout, &supported_filesystems_count, &supported_filesystems_array);

	if (result == dtmd_ok)
	{
		try
		{
			if (supported_filesystems_array != NULL)
			{
				supported_filesystems_list.reserve(supported_filesystems_count);

				for (i = 0; i < supported_filesystems_count; ++i)
				{
					if (supported_filesystems_array[i] != NULL)
					{
						supported_filesystems_list.push_back(std::string(supported_filesystems_array[i]));
					}
				}
			}

			dtmd_free_supported_filesystems_list(this->m_handle, supported_filesystems_count, supported_filesystems_array);
		}
		catch (...)
		{
			dtmd_free_supported_filesystems_list(this->m_handle, supported_filesystems_count, supported_filesystems_array);
			throw;
		}
	}

	return result;
}

bool library::isStateInvalid() const
{
	return dtmd_is_state_invalid(this->m_handle);
}

void library::local_callback(void *arg, const dtmd_command_t *cmd)
{
	library *lib = (library*) arg;

	try
	{
		command local_command(cmd);

		lib->m_cb(lib->m_arg, local_command);
	}
	catch (...)
	{
		// signal failure
		lib->m_cb(lib->m_arg, command());
	}
}

} // namespace dtmd
