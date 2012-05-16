/*
spacenavd - a free software replacement driver for 6dof space-mice.
Copyright (C) 2007-2012 John Tsiombikas <nuclear@member.fsf.org>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#if !defined(__linux__)

#include <stdio.h>
#include "dev.h"

static const char *message =
	"Unfortunately this version of spacenavd does not support USB devices on your "
	"platform yet. Make sure you are using the latest version of spacenavd.\n";

const char *find_usb_device(void)
{
	fputs(message, stderr);
	return 0;
}

int open_dev_usb(struct device *dev, const char *path)
{
	return -1;
}

/* the hotplug functions will also be missing on unsupported platforms */
int init_hotplug(void)
{
	return -1;
}

void shutdown_hotplug(void)
{
}

int get_hotplug_fd(void)
{
	return -1;
}

int handle_hotplug(void)
{
	return -1;
}

#endif	/* unsupported platform */
