// Copyright 2017-2017 the openage authors. See copying.md for legal info.

#include "directory.h"

#include <cstdio>
#include <dirent.h>
#include <fcntl.h>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef __APPLE__
#include <sys/time.h>
#endif

#include "./native.h"
#include "../file.h"
#include "../filelike/native.h"
#include "../misc.h"
#include "../path.h"


namespace openage {
namespace util {
namespace fslike {


Directory::Directory(const std::string &basepath, bool create_if_missing)
	:
	basepath{basepath} {

	if (create_if_missing) {
		this->mkdirs({});
	}
}


std::string Directory::resolve(const Path::parts_t &parts) const {
	std::string ret = this->basepath;
	for (auto &part : parts) {
		ret += PATHSEP + part;
	}
	return ret;
}


// TODO: use std::optional when available
std::tuple<struct stat, int> Directory::do_stat(const Path::parts_t &parts) const {
	const std::string path = this->resolve(parts);
	struct stat buf;
	int result = stat(path.c_str(), &buf);

	return std::make_tuple(buf, result);
}


bool Directory::is_file(const Path::parts_t &parts) {
	auto stat_result = this->do_stat(parts);

	// test for regular file
	if (std::get<1>(stat_result) == 0 and
	    S_ISREG(std::get<0>(stat_result).st_mode)) {
		return true;
	}

	return false;
}


bool Directory::is_dir(const Path::parts_t &parts) {
	auto stat_result = this->do_stat(parts);

	// test for regular file
	if (std::get<1>(stat_result) == 0 and
	    S_ISDIR(std::get<0>(stat_result).st_mode)) {
		return true;
	}

	return false;
}


bool Directory::writable(const Path::parts_t &parts) {
	Path::parts_t parts_test = parts;

	// try to find the first existing path-part
	while (not (this->is_dir(parts_test) or
	            this->is_file(parts_test))) {

		if (parts_test.size() == 0) {
			throw Error{ERR << "file not found"};
		}

		parts_test.pop_back();
	}
	const std::string path = this->resolve(parts_test);

	return access(path.c_str(), W_OK);
}


std::vector<Path::part_t> Directory::list(const Path::parts_t &parts) {

	const std::string path = this->resolve(parts);
	std::vector<Path::part_t> ret;

	DIR *dir;
	struct dirent *ent;

	dir = opendir(path.c_str());

	if (dir == nullptr) {
		throw Error(ERR << "could not list contents of '" << path << "'");
	}

	// walk over dir contents
	while ((ent = readdir(dir)) != nullptr) {
		ret.push_back(ent->d_name);
	}

	closedir(dir);

	return ret;
}


bool Directory::mkdirs(const Path::parts_t &parts) {

	Path::parts_t all_parts = util::split(this->basepath, PATHSEP);

	vector_extend(all_parts, parts);

	std::string dirpath;

	for (auto &part : all_parts) {
		dirpath += PATHSEP + part;

		struct stat buf;

		// it it exists already, try creating the next one
		if (stat(dirpath.c_str(), &buf) == 0 and
		    S_ISDIR(buf.st_mode)) {

			continue;
		}

		// create the folder, umask will turn it to 755.
		bool dir_created = mkdir(dirpath.c_str(), 0777) == 0;

		if (not dir_created) {
			return false;
		}
	}
	return true;
}


File Directory::open_r(const Path::parts_t &parts) {
	return File{
		std::make_shared<filelike::Native>(this->resolve(parts),
		                                   filelike::Native::mode_t::R)
	};
}


File Directory::open_w(const Path::parts_t &parts) {
	return File{
		std::make_shared<filelike::Native>(this->resolve(parts),
		                                   filelike::Native::mode_t::W)
	};
}


std::string Directory::get_native_path(const Path::parts_t &parts) {
	return this->resolve(parts);
}


bool Directory::rename(const Path::parts_t &parts,
                       const Path::parts_t &target_parts) {

	return std::rename(this->resolve(parts).c_str(),
	                   this->resolve(target_parts).c_str()) == 0;
}


bool Directory::rmdir(const Path::parts_t &parts) {
	return ::rmdir(this->resolve(parts).c_str()) == 0;
}


bool Directory::touch(const Path::parts_t &parts) {
	const std::string path = this->resolve(parts);

	// create the file if missing
	int fd = open(
		path.c_str(),
		O_WRONLY|O_CREAT|O_NOCTTY|O_NONBLOCK,
		0666
	);

	if (fd < 0) {
		return false;
	}

	close(fd);

	// update the timestamp
#ifdef __APPLE__
	int result = utimes(path.c_str(), nullptr) == 0;
#else
	int result = utimensat(AT_FDCWD, path.c_str(), nullptr, 0) == 0;
#endif

	return result;
}


bool Directory::unlink(const Path::parts_t &parts) {
	return std::remove(this->resolve(parts).c_str()) == 0;
}


int Directory::get_mtime(const Path::parts_t &parts) {
	auto stat_result = this->do_stat(parts);

	// return the mtime
	if (std::get<1>(stat_result) == 0) {
#ifdef __APPLE__
		return std::get<0>(stat_result).st_mtimespec.tv_sec;
#else
		return std::get<0>(stat_result).st_mtim.tv_sec;
#endif
	}

	throw Error{ERR << "can't get mtime"};
}


uint64_t Directory::get_filesize(const Path::parts_t &parts) {
	auto stat_result = this->do_stat(parts);

	// return the mtime
	if (std::get<1>(stat_result) == 0) {
		return std::get<0>(stat_result).st_size;
	}

	throw Error{ERR << "can't get filesize"};
}


std::ostream &Directory::repr(std::ostream &stream) {
	stream << this->basepath;
	return stream;
}

}}} // openage::util::fslike
