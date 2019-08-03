/*
 * Copyright (C) 2019 i.Dark_Templar <darktemplar@dark-templar-archives.net>
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

#include "daemon/poweroff.h"

#include "daemon/return_codes.h"
#include "daemon/log.h"

#include <dtmd-misc.h>

#if (defined OS_Linux)
#include <linux/limits.h>
#include <sys/stat.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/bsg.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>
#include <scsi/scsi_ioctl.h>

static int send_scsi_command_sync(int fd, uint8_t *cdb, size_t cdb_len, const char *devname)
{
	struct sg_io_v4 io_v4;
	uint8_t sense[32];
	struct sg_io_hdr io_hdr;
	int ret = result_fail;
	int rc;
	int timeout_msec = 30000; /* 30 seconds */

	/*
	 * See http://sg.danny.cz/sg/sg_io.html and http://www.tldp.org/HOWTO/SCSI-Generic-HOWTO/index.html
	 * for detailed information about how the SG_IO ioctl work
	 */

	memset(sense, 0, sizeof(sense));
	memset(&io_v4, 0, sizeof(io_v4));
	io_v4.guard            = 'Q';
	io_v4.protocol         = BSG_PROTOCOL_SCSI;
	io_v4.subprotocol      = BSG_SUB_PROTOCOL_SCSI_CMD;
	io_v4.request_len      = cdb_len;
	io_v4.request          = (uintptr_t) cdb;
	io_v4.max_response_len = sizeof(sense);
	io_v4.response         = (uintptr_t) sense;
	io_v4.timeout          = timeout_msec;

	rc = ioctl(fd, SG_IO, &io_v4);
	if (rc != 0)
	{
		/* could be that the driver doesn't do version 4, try version 3 */
		if (errno == EINVAL)
		{
			memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
			io_hdr.interface_id    = 'S';
			io_hdr.cmdp            = (unsigned char*) cdb;
			io_hdr.cmd_len         = cdb_len;
			io_hdr.dxfer_direction = SG_DXFER_NONE;
			io_hdr.sbp             = sense;
			io_hdr.mx_sb_len       = sizeof(sense);
			io_hdr.timeout         = timeout_msec;

			rc = ioctl(fd, SG_IO, &io_hdr);
			if (rc != 0)
			{
				WRITE_LOG_ARGS(LOG_WARNING, "SGIO v3 ioctl failed (v4 not supported): %s", devname);
				goto out;
			}
			else
			{
				if (!((io_hdr.status == 0)
					&& (io_hdr.host_status == 0)
					&& (io_hdr.driver_status == 0)))
				{
					WRITE_LOG_ARGS(LOG_WARNING, "Non-GOOD SCSI status from SGIO v3 ioctl: %s, status=%d host_status=%d driver_status=%d",
						devname, io_hdr.status, io_hdr.host_status, io_hdr.driver_status);
					goto out;
				}
			}
		}
		else
		{
			WRITE_LOG_ARGS(LOG_WARNING, "SGIO v4 ioctl failed: %s", devname);
			goto out;
		}
	}
	else
	{
		if (!((io_v4.device_status == 0)
			&& (io_v4.transport_status == 0)
			&& (io_v4.driver_status == 0)))
		{
			WRITE_LOG_ARGS(LOG_WARNING, "Non-GOOD SCSI status from SGIO v4 ioctl: %s, device_status=%d transport_status=%d driver_status=%d",
				devname, io_v4.device_status, io_v4.transport_status, io_v4.driver_status);
			goto out;
		}
	}

	ret = result_success;

out:
	return ret;
}

static int send_scsi_synchronize_cache_command_sync(int fd, const char *devname)
{
	uint8_t cdb[10];

	/*
	 * SBC3 (SCSI Block Commands), 5.18 SYNCHRONIZE CACHE (10) command
	 */
	memset (cdb, 0, sizeof cdb);
	cdb[0] = 0x35;	/* OPERATION CODE: SYNCHRONIZE CACHE (10) */

	return send_scsi_command_sync(fd, cdb, sizeof(cdb), devname);
}

static int send_scsi_start_stop_unit_command_sync(int fd, const char *devname)
{
	uint8_t cdb[6];

	/*
	 * SBC3 (SCSI Block Commands), 5.20 START STOP UNIT command
	 */
	memset (cdb, 0, sizeof cdb);
	cdb[0] = 0x1b;	/* OPERATION CODE: START STOP UNIT */

	return send_scsi_command_sync(fd, cdb, sizeof(cdb), devname);
}

static int poweroff(const char *device_name, const char *sysfs_path, dtmd_error_code_t *error_code)
{
	int device_fd;
	int rc;
	char fullpath[PATH_MAX + 1];

	device_fd = open(device_name, O_RDONLY|O_NONBLOCK|O_EXCL);
	if (device_fd < 0)
	{
		if (error_code != NULL)
		{
			*error_code = dtmd_error_code_generic_error;
		}

		return result_fail;
	}

	if (fsync(device_fd) != 0)
	{
		if (error_code != NULL)
		{
			*error_code = dtmd_error_code_generic_error;
		}

		close(device_fd);
		return result_fail;
	}

	rc = send_scsi_synchronize_cache_command_sync(device_fd, device_name);
	if (is_result_failure(rc))
	{
		WRITE_LOG_ARGS(LOG_INFO, "Ignoring SCSI command SYNCHRONIZE CACHE failure on %s", device_name);
	}
	else
	{
		WRITE_LOG_ARGS(LOG_INFO, "Successfully sent SCSI command SYNCHRONIZE CACHE to %s", device_name);
	}

	rc = send_scsi_start_stop_unit_command_sync(device_fd, device_name);
	if (is_result_failure(rc))
	{
		WRITE_LOG_ARGS(LOG_INFO, "Ignoring SCSI command START STOP UNIT failure on %s", device_name);
	}
	else
	{
		WRITE_LOG_ARGS(LOG_INFO, "Successfully sent SCSI command START STOP UNIT to %s", device_name);
	}

	close(device_fd);

	rc = sprintf(fullpath, "%s/remove", sysfs_path);
	if ((rc < 0) || (rc > PATH_MAX))
	{
		if (error_code != NULL)
		{
			*error_code = dtmd_error_code_generic_error;
		}

		return result_fail;
	}

	device_fd = open(fullpath, O_WRONLY);
	if (device_fd < 0)
	{
		if (error_code != NULL)
		{
			*error_code = dtmd_error_code_generic_error;
		}

		return result_fail;
	}

	if (write(device_fd, "1", strlen("1")) != strlen("1"))
	{
		if (error_code != NULL)
		{
			*error_code = dtmd_error_code_generic_error;
		}

		close(device_fd);
		return result_fail;
	}

	close(device_fd);

	WRITE_LOG_ARGS(LOG_INFO, "Powered off device '%s'", device_name);

	return result_success;
}

static int poweroff_check_if_device_is_not_mounted(dtmd_removable_media_t *media_ptr)
{
	dtmd_removable_media_t *iter_media_ptr;

	if (media_ptr->mnt_point != NULL)
	{
		return result_fail;
	}

	for (iter_media_ptr = media_ptr->children_list; iter_media_ptr != NULL; iter_media_ptr = iter_media_ptr->next_node)
	{
		if (is_result_failure(poweroff_check_if_device_is_not_mounted(iter_media_ptr)))
		{
			return result_fail;
		}
	}

	return result_success;
}

int invoke_poweroff(struct client *client_ptr, const char *path, dtmd_error_code_t *error_code)
{
	int result;
	dtmd_removable_media_t *media_ptr;
	dtmd_removable_media_private_t *private_ptr;

	media_ptr = dtmd_find_media(path, removable_media_root);
	if (media_ptr == NULL)
	{
		WRITE_LOG_ARGS(LOG_WARNING, "Failed to poweroff device '%s': device does not exist", path);
		result = result_fail;

		if (error_code != NULL)
		{
			*error_code = dtmd_error_code_no_such_removable_device;
		}

		goto invoke_poweroff_error_1;
	}

	if (media_ptr->type != dtmd_removable_media_type_stateless_device)
	{
		WRITE_LOG_ARGS(LOG_NOTICE, "Failed to poweroff device '%s': poweroff action not supported for this device type", path);
		result = result_fail;

		if (error_code != NULL)
		{
			*error_code = dtmd_error_code_generic_error;
		}

		goto invoke_poweroff_error_1;
	}

	result = poweroff_check_if_device_is_not_mounted(media_ptr);
	if (is_result_failure(result))
	{
		WRITE_LOG_ARGS(LOG_WARNING, "Failed to poweroff device '%s': device is busy", path);

		if (error_code != NULL)
		{
			*error_code = dtmd_error_code_mount_point_busy;
		}

		goto invoke_poweroff_error_1;
	}

	private_ptr = (dtmd_removable_media_private_t*) (media_ptr->private_data);

	if (private_ptr->sysfs_path == NULL)
	{
		WRITE_LOG_ARGS(LOG_WARNING, "Failed to poweroff device '%s': no sysfs path found", path);
		result = result_fail;

		if (error_code != NULL)
		{
			*error_code = dtmd_error_code_generic_error;
		}

		goto invoke_poweroff_error_1;
	}

	result = poweroff(path, private_ptr->sysfs_path, error_code);

invoke_poweroff_error_1:
	return result;
}
#endif /* (defined OS_Linux) */

#if (defined OS_FreeBSD)
int invoke_poweroff(struct client *client_ptr, const char *path, dtmd_error_code_t *error_code)
{
	WRITE_LOG(LOG_NOTICE, "Poweroff action not implemented for FreeBSD");

	// Currently not implemented for FreeBSD, just return failure
	if (error_code != NULL)
	{
		*error_code = dtmd_error_code_generic_error;
	}

	return result_fail;
}
#endif /* (defined OS_FreeBSD) */
