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

command::command(const dt_command_t *raw_cmd)
{
	if (raw_cmd != NULL)
	{
		this->fillFromCmd(raw_cmd);
	}
}

command::~command()
{
}

void command::fillFromCmd(const dt_command_t *raw_cmd)
{
	this->clear();

	if (raw_cmd != NULL)
	{
		this->cmd = raw_cmd->cmd;

		if (raw_cmd->args != NULL)
		{
			this->args.reserve(raw_cmd->args_count);

			for (size_t i = 0; i < raw_cmd->args_count; ++i)
			{
				if (raw_cmd->args[i] != NULL)
				{
					this->args.push_back(raw_cmd->args[i]);
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

removable_media::removable_media(const removable_media_private &)
	: type(dtmd_removable_media_type_unknown_or_persistent),
	subtype(dtmd_removable_media_subtype_unknown_or_persistent),
	state(dtmd_removable_media_state_unknown)
{
}

removable_media::removable_media(const removable_media_private &,
	const std::shared_ptr<removable_media> &l_parent,
	const std::string &l_path,
	dtmd_removable_media_type_t l_type,
	dtmd_removable_media_subtype_t l_subtype,
	dtmd_removable_media_state_t l_state,
	const std::string &l_fstype,
	const std::string &l_label,
	const std::string &l_mnt_point,
	const std::string &l_mnt_opts)
	: path(l_path),
	type(l_type),
	subtype(l_subtype),
	state(l_state),
	fstype(l_fstype),
	label(l_label),
	mnt_point(l_mnt_point),
	mnt_opts(l_mnt_opts),
	parent(l_parent)
{
}

removable_media::~removable_media()
{
}

void removable_media::copyFromRemovableMedia(const removable_media &other)
{
	this->path      = other.path;
	this->type      = other.type;
	this->subtype   = other.subtype;
	this->state     = other.state;
	this->fstype    = other.fstype;
	this->label     = other.label;
	this->mnt_point = other.mnt_point;
	this->mnt_opts  = other.mnt_opts;
}

std::shared_ptr<removable_media> removable_media::createFromRemovableMedia(const dtmd_removable_media_t *raw_removable_media)
{
	std::shared_ptr<removable_media> result = removable_media::create();
	result->fillFromRemovableMedia(raw_removable_media);
	return result;
}

bool removable_media::operator<(const removable_media &other) const
{
	return (this->path < other.path);
}

void removable_media::fillFromRemovableMedia(const dtmd_removable_media_t *raw_removable_media)
{
	this->clear();

	if (raw_removable_media != NULL)
	{
		if (raw_removable_media->path != NULL)
		{
			this->path = raw_removable_media->path;
		}

		this->type = raw_removable_media->type;
		this->subtype = raw_removable_media->subtype;
		this->state = raw_removable_media->state;

		if (raw_removable_media->fstype != NULL)
		{
			this->fstype = raw_removable_media->fstype;
		}

		if (raw_removable_media->label != NULL)
		{
			this->label = raw_removable_media->label;
		}

		if (raw_removable_media->mnt_point != NULL)
		{
			this->mnt_point = raw_removable_media->mnt_point;
		}

		if (raw_removable_media->mnt_opts != NULL)
		{
			this->mnt_opts = raw_removable_media->mnt_opts;
		}

		for (dtmd_removable_media_t *iter = raw_removable_media->children_list; iter != NULL; iter = iter->next_node)
		{
			auto child = removable_media::createFromRemovableMedia(iter);
			child->parent = this->shared_from_this();
			children.push_back(child);
		}
	}
}

void removable_media::clear()
{
	this->path.clear();
	this->type = dtmd_removable_media_type_unknown_or_persistent;
	this->subtype = dtmd_removable_media_subtype_unknown_or_persistent;
	this->state = dtmd_removable_media_state_unknown;
	this->fstype.clear();
	this->label.clear();
	this->mnt_point.clear();
	this->mnt_opts.clear();
	this->parent.reset();
	this->children.clear();
}

std::string removable_media::getParentPath() const
{
	auto parent_object = this->parent.lock();
	if (parent_object)
	{
		return parent_object->path;
	}
	else
	{
		return dtmd_root_device_path;
	}
}

bool removable_media::isEmpty() const
{
	return (this->path.empty()
		&& (this->type  == dtmd_removable_media_type_unknown_or_persistent)
		&& (this->subtype == dtmd_removable_media_subtype_unknown_or_persistent)
		&& (this->state == dtmd_removable_media_state_unknown)
		&& this->fstype.empty()
		&& this->label.empty()
		&& this->mnt_point.empty()
		&& this->mnt_opts.empty()
		&& this->parent.expired()
		&& this->children.empty());
}

dtmd_removable_media_type_t removable_media::getValidType() const
{
	if (this->path.empty())
	{
		return dtmd_removable_media_type_unknown_or_persistent;
	}

	switch (this->type)
	{
	case dtmd_removable_media_type_device_partition:
		if ((this->subtype == dtmd_removable_media_subtype_unknown_or_persistent)
			&& (this->state == dtmd_removable_media_state_unknown))
		{
			return dtmd_removable_media_type_device_partition;
		}
		break;

	case dtmd_removable_media_type_stateless_device:
		if ((this->subtype != dtmd_removable_media_subtype_unknown_or_persistent)
			&& (this->state == dtmd_removable_media_state_unknown)
			&& this->fstype.empty()
			&& this->label.empty()
			&& this->mnt_point.empty()
			&& this->mnt_opts.empty())
		{
			return dtmd_removable_media_type_stateless_device;
		}
		break;

	case dtmd_removable_media_type_stateful_device:
		if ((this->subtype != dtmd_removable_media_subtype_unknown_or_persistent)
			&& (this->state != dtmd_removable_media_state_unknown))
		{
			return dtmd_removable_media_type_stateful_device;
		}
		break;

	case dtmd_removable_media_type_unknown_or_persistent:
	default:
		break;
	}

	return dtmd_removable_media_type_unknown_or_persistent;
}

library::library(callback cb, state_callback state_cb, void *arg)
	: m_handle(NULL),
	m_cb(cb),
	m_state_cb(state_cb),
	m_arg(arg)
{
	m_handle = dtmd_init(&library::local_callback, &library::local_state_callback, this, NULL);
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

dtmd_result_t library::list_all_removable_devices(int timeout, std::list<std::shared_ptr<removable_media> > &removable_devices_list)
{
	dtmd_result_t result;
	dtmd_removable_media_t *returned_removable_device;

	result = dtmd_list_all_removable_devices(this->m_handle, timeout, &returned_removable_device);
	if (result == dtmd_ok)
	{
		try
		{
			removable_devices_list.clear();

			for (dtmd_removable_media_t *removable_devices_iter = returned_removable_device; removable_devices_iter != NULL; removable_devices_iter = removable_devices_iter->next_node)
			{
				auto item = removable_media::createFromRemovableMedia(removable_devices_iter);
				removable_devices_list.push_back(item);
			}

			dtmd_free_removable_devices(this->m_handle, returned_removable_device);
		}
		catch (...)
		{
			dtmd_free_removable_devices(this->m_handle, returned_removable_device);
			throw;
		}
	}

	return result;
}

dtmd_result_t library::list_removable_device(int timeout, const std::string &removable_device_path, std::list<std::shared_ptr<removable_media> > &removable_devices_list)
{
	dtmd_result_t result;
	dtmd_removable_media_t *returned_removable_device;

	result = dtmd_list_removable_device(this->m_handle, timeout, removable_device_path.c_str(), &returned_removable_device);
	if (result == dtmd_ok)
	{
		try
		{
			removable_devices_list.clear();

			for (dtmd_removable_media_t *removable_devices_iter = returned_removable_device; removable_devices_iter != NULL; removable_devices_iter = removable_devices_iter->next_node)
			{
				auto item = removable_media::createFromRemovableMedia(removable_devices_iter);
				removable_devices_list.push_back(item);
			}

			dtmd_free_removable_devices(this->m_handle, returned_removable_device);
		}
		catch (...)
		{
			dtmd_free_removable_devices(this->m_handle, returned_removable_device);
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
	size_t supported_filesystems_count;
	const char **supported_filesystems_array;

	result = dtmd_list_supported_filesystems(this->m_handle, timeout, &supported_filesystems_count, &supported_filesystems_array);
	if (result == dtmd_ok)
	{
		try
		{
			if (supported_filesystems_array != NULL)
			{
				supported_filesystems_list.reserve(supported_filesystems_count);

				for (size_t i = 0; i < supported_filesystems_count; ++i)
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

dtmd_result_t library::list_supported_filesystem_options(int timeout, const std::string &filesystem, std::vector<std::string> &supported_filesystem_options_list)
{
	dtmd_result_t result;
	size_t supported_filesystem_options_count;
	const char **supported_filesystem_options_array;

	result = dtmd_list_supported_filesystem_options(this->m_handle, timeout, filesystem.c_str(), &supported_filesystem_options_count, &supported_filesystem_options_array);
	if (result == dtmd_ok)
	{
		try
		{
			if (supported_filesystem_options_array != NULL)
			{
				supported_filesystem_options_list.reserve(supported_filesystem_options_count);

				for (size_t i = 0; i < supported_filesystem_options_count; ++i)
				{
					if (supported_filesystem_options_array[i] != NULL)
					{
						supported_filesystem_options_list.push_back(std::string(supported_filesystem_options_array[i]));
					}
				}
			}

			dtmd_free_supported_filesystem_options_list(this->m_handle, supported_filesystem_options_count, supported_filesystem_options_array);
		}
		catch (...)
		{
			dtmd_free_supported_filesystem_options_list(this->m_handle, supported_filesystem_options_count, supported_filesystem_options_array);
			throw;
		}
	}

	return result;
}

dtmd_result_t library::fill_removable_device_from_notification(const command &cmd, std::shared_ptr<removable_media> &removable_device) const
{
	dtmd_result_t result;
	dtmd_removable_media_t *returned_removable_device;
	dt_command_t reconstructed_command;
	std::vector<const char*> reconstructed_args_array;

	reconstructed_args_array.reserve(cmd.args.size());

	{
		auto iter_end = cmd.args.end();
		for (auto iter = cmd.args.begin(); iter != iter_end; ++iter)
		{
			reconstructed_args_array.push_back((!iter->empty()) ? iter->c_str() : NULL);
		}
	}

	reconstructed_command.cmd        = const_cast<char*>((!cmd.cmd.empty()) ? cmd.cmd.c_str() : NULL);
	reconstructed_command.args_count = reconstructed_args_array.size();
	reconstructed_command.args       = const_cast<char**>((!reconstructed_args_array.empty()) ? reconstructed_args_array.data() : NULL);

	result = dtmd_fill_removable_device_from_notification(this->m_handle, &reconstructed_command, dtmd_fill_link, &returned_removable_device);
	if (result == dtmd_ok)
	{
		try
		{
			removable_device = removable_media::createFromRemovableMedia(returned_removable_device);
			dtmd_free_removable_devices(this->m_handle, returned_removable_device);
		}
		catch (...)
		{
			dtmd_free_removable_devices(this->m_handle, returned_removable_device);
			throw;
		}
	}

	return result;
}

bool library::isStateInvalid() const
{
	return dtmd_is_state_invalid(this->m_handle);
}

bool library::isNotificationValidRemovableDevice(const command &cmd) const
{
	dt_command_t reconstructed_command;
	std::vector<const char*> reconstructed_args_array;

	reconstructed_args_array.reserve(cmd.args.size());

	{
		auto iter_end = cmd.args.end();
		for (auto iter = cmd.args.begin(); iter != iter_end; ++iter)
		{
			reconstructed_args_array.push_back((!iter->empty()) ? iter->c_str() : NULL);
		}
	}

	reconstructed_command.cmd        = const_cast<char*>((!cmd.cmd.empty()) ? cmd.cmd.c_str() : NULL);
	reconstructed_command.args_count = reconstructed_args_array.size();
	reconstructed_command.args       = const_cast<char**>((!reconstructed_args_array.empty()) ? reconstructed_args_array.data() : NULL);

	return dtmd_is_notification_valid_removable_device(this->m_handle, &reconstructed_command);
}

dtmd_error_code_t library::getCodeOfCommandFail() const
{
	return dtmd_get_code_of_command_fail(this->m_handle);
}

void library::local_callback(dtmd_t *library_ptr, void *arg, const dt_command_t *cmd)
{
	library *lib = (library*) arg;

	try
	{
		command local_command(cmd);

		lib->m_cb(*lib, lib->m_arg, local_command);
	}
	catch (...)
	{
		// signal failure
		try
		{
			lib->m_state_cb(*lib, lib->m_arg, dtmd_state_failure);
		}
		catch (...)
		{
		}
	}
}

void library::local_state_callback(dtmd_t *library_ptr, void *arg, dtmd_state_t state)
{
	library *lib = (library*) arg;

	try
	{
		lib->m_state_cb(*lib, lib->m_arg, state);
	}
	catch (...)
	{
		// signal failure
		try
		{
			lib->m_state_cb(*lib, lib->m_arg, dtmd_state_failure);
		}
		catch (...)
		{
		}
	}
}

std::shared_ptr<removable_media> find_removable_media(const std::string &path, const std::list<std::shared_ptr<removable_media> > &root)
{
	std::shared_ptr<removable_media> result;

	{
		auto iter_end = root.end();
		for (auto iter = root.begin(); iter != iter_end; ++iter)
		{
			result = find_removable_media(path, *iter);
			if (result)
			{
				break;
			}
		}
	}

	return result;
}

std::shared_ptr<removable_media> find_removable_media(const std::string &path, const std::shared_ptr<removable_media> &root)
{
	if (root)
	{
		if (root->path == path)
		{
			return root;
		}

		return find_removable_media(path, root->children);
	}
	else
	{
		return std::shared_ptr<removable_media>();
	}
}

} // namespace dtmd
