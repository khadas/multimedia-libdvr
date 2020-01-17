#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <dvr_types.h>
#include <dvr_utils.h>

int DVR_FileEcho(const char *name, const char *cmd)
{
	int fd, len, ret;

	fd = open(name, O_WRONLY);
	if (fd == -1)
	{
		DVR_DEBUG(1, "cannot open file \"%s\"", name);
		return DVR_FAILURE;
	}

	len = strlen(cmd);
	ret = write(fd, cmd, len);
	if (ret != len)
	{
		DVR_DEBUG(1, "write failed file:\"%s\" cmd:\"%s\" error:\"%s\"", name, cmd, strerror(errno));
		close(fd);
		return DVR_FAILURE;
	}
	close(fd);

	return DVR_SUCCESS;
}
