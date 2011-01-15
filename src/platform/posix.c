/**
 * Copyright (c) 2010 William Light <wrl@illest.net>
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* for asprintf */
#define _GNU_SOURCE

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>

#include <monome.h>
#include "internal.h"
#include "platform.h"

monome_t *monome_platform_load_protocol(const char *proto) {
	void *dl_handle;
	monome_t *(*monome_protocol_new)();
	monome_t *monome;
	char *buf;

	if( asprintf(&buf, "%s/monome/protocol_%s%s", LIBDIR, proto, LIBSUFFIX) < 0 )
		return NULL;

	dl_handle = dlopen(buf, RTLD_LAZY);
	free(buf);

	if( !dl_handle ) {
		fprintf(stderr, "couldn't load monome protocol module.  "
				"dlopen said: \n\t%s\n\n"
				"please make sure that libmonome is installed correctly!\n",
				dlerror());
		return NULL;
	}

	monome_protocol_new = dlsym(dl_handle, "monome_protocol_new");

	if( !monome_protocol_new ) {
		fprintf(stderr, "couldn't initialize monome protocol module.  "
				"dlopen said:\n\t%s\n\n"
				"please make sure you're using a valid protocol library!\n"
				"if this is a protocol library you wrote, make sure you're"
				"providing a \e[1mmonome_protocol_new\e[0m function.\n",
				dlerror());
		goto err;
	}

	monome = (*monome_protocol_new)();

	if( !monome )
		goto err;

	monome->dl_handle = dl_handle;
	return monome;

err:
	dlclose(dl_handle);
	return NULL;
}

void monome_platform_free(monome_t *monome) {
	void *dl_handle = monome->dl_handle;

	monome->free(monome);
	dlclose(dl_handle);
}

int monome_platform_open(monome_t *monome, const char *dev) {
	struct termios nt, ot;
	int fd;

	if( (fd = open(dev, O_RDWR | O_NOCTTY | O_NONBLOCK)) < 0 ) {
		perror("libmonome: could not open monome device");
		return 1;
	}

	tcgetattr(fd, &ot);
	nt = ot;

	/* baud rate */
	cfsetispeed(&nt, B115200);
	cfsetospeed(&nt, B115200);

	/* parity (8N1) */
	nt.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
	nt.c_cflag |=  (CS8 | CLOCAL | CREAD);

	/* no line processing */
	nt.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG | IEXTEN);

	/* raw input */
	nt.c_iflag &= ~(IGNBRK | BRKINT | ICRNL | INLCR | PARMRK |
	                INPCK | ISTRIP | IXON);

	/* raw output */
	nt.c_oflag &= ~(OCRNL | ONLCR | ONLRET | ONOCR |
	                OFILL | OPOST);

	/* block until one character is read */
	nt.c_cc[VMIN]  = 1;
	nt.c_cc[VTIME] = 0;

	if( tcsetattr(fd, TCSANOW, &nt) < 0 ) {
		perror("libmonome: could not set terminal attributes");
		return 1;
	}

	tcflush(fd, TCIOFLUSH);

	monome->fd = fd;
	monome->ot = ot;

	return 0;
}

int monome_platform_close(monome_t *monome) {
	tcsetattr(monome->fd, TCSANOW, &monome->ot);
	return close(monome->fd);
}

ssize_t monome_platform_write(monome_t *monome, const uint8_t *buf, ssize_t bufsize) {
	ssize_t ret = write(monome->fd, buf, bufsize);

	if( ret < bufsize )
		perror("libmonome: write is missing bytes");

	if( ret < 0 )
		perror("libmonome: error in write");

	if( tcdrain(monome->fd) < 0 )
		perror("libmonome: error in tcdrain");

	return ret;
}

ssize_t monome_platform_read(monome_t *monome, uint8_t *buf, ssize_t count) {
	return read(monome->fd, buf, count);
}
