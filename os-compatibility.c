
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/epoll.h>
#include <string.h>
#include <stdlib.h>

#include "os-compatibility.h"

static int
create_tmpfile_cloexec(char *tmpname)
{
	int fd;

	fd = mkostemp(tmpname, O_CLOEXEC);
	if (fd >= 0)
		unlink(tmpname);

	return fd;
}

int
os_create_anonymous_file(off_t size)
{
	static const char template_[] = "/weston-shared-XXXXXX";
	const char *path;
	char *name;
	int fd;
	int ret;

	path = getenv("XDG_RUNTIME_DIR");
	if (!path) {
		errno = ENOENT;
		return -1;
	}

	name = (char *)malloc(strlen(path) + sizeof(template_));
	if (!name)
		return -1;

	strcpy(name, path);
	strcat
        (name, template_);

	fd = create_tmpfile_cloexec(name);

	free(name);

	if (fd < 0)
		return -1;

	ret = posix_fallocate(fd, 0, size);
	if (ret != 0) {
		close(fd);
		errno = ret;
		return -1;
	}

	return fd;
}
