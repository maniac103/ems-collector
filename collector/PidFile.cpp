/*
 * Copyright (c) 2005, 2006 Riccardo Murri <riccardo.murri@ictp.it>
 * Copyright (c) 2005, 2006 Antonio Messina <antonio.messina@ictp.it>
 * for the ICTP project EGRID.
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "PidFile.h"

#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/types.h>

PidFile::PidFile(const std::string& fileName) :
    m_path(fileName),
    m_fd(-1)
{
}

PidFile::~PidFile()
{
    if (m_fd != -1) {
	/* pidfile has been opened and locked */
	lockf(m_fd, F_ULOCK, 0);
	close(m_fd);
	unlink(m_path.c_str());
    }
}

void
PidFile::aquire()
{
    /* open pidfile */
    m_fd = open(m_path.c_str(), O_WRONLY | O_CREAT | O_NOFOLLOW, 0644);
    if (m_fd < 0) {
	std::ostringstream msg;
	msg << "Cannot open pidfile '" << m_path << "': " << strerror(errno);
	throw std::runtime_error(msg.str());
    }

    /* lock it */
    if (lockf(m_fd, F_TLOCK, 0) < 0) {
	std::ostringstream msg;
	msg << "Cannot lock pidfile '" << m_path << "': " << strerror(errno);
	throw std::runtime_error(msg.str());
    }
}

void
PidFile::write()
{
    if (m_fd < 0) {
	aquire();
    }

    /* truncate pidfile at 0 length */
    ftruncate(m_fd, 0);

    /* write our pid */
    try {
	std::ofstream pidf(m_path.c_str());
	pidf << getpid();
    } catch(std::exception& e) {
	std::ostringstream msg;
	msg << "Cannot write pidfile '" << m_path << "': " << e.what();
	throw std::runtime_error(msg.str());
    }
}


