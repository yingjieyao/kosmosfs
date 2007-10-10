/*!
 * $Id$ 
 *
 * \file replay.cc
 * \brief log replay
 * \author Blake Lewis (Kosmix Corp.)
 *
 * Copyright 2006 Kosmix Corp.
 *
 * This file is part of Kosmos File System (KFS).
 *
 * Licensed under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include <cassert>
#include <cstdlib>
#include <iostream>

#include "logger.h"
#include "replay.h"
#include "restore.h"
#include "util.h"
#include "entry.h"
#include "kfstree.h"
#include "LayoutManager.h"

using namespace KFS;

Replay KFS::replayer;

/*!
 * \brief open saved log file for replay
 * \param[in] p	a path in the form "<logdir>/log.<number>"
 */
void
Replay::openlog(const string &p)
{
	if (!file_exists(p)) {
		std::cerr << "log file " << p << " not found!\n";
		number = 0;
		path = oplog.logfile(0);
	} else {
		KFS_LOG_DEBUG("Doing log replay from file: %s", p.c_str());
		string::size_type dot = p.rfind('.');
		assert(dot != string::npos);
		number = std::atoi(p.substr(dot + 1).c_str());
		assert(number >= 0);
		path = p;
		file.open(path.c_str());
		assert(!file.fail());
	}
}

/*!
 * \brief check log version
 * format: version/<number>
 */
static bool
replay_version(deque <string> &c)
{
	fid_t vers;
	bool ok = pop_fid(vers, "version", c, true);
	return (ok && vers == Logger::VERSION);
}

/*!
 * \brief handle common prefix for all log records
 */
static bool
pop_parent(fid_t &id, deque <string> &c)
{
	c.pop_front();		// get rid of record type
	return pop_fid(id, "dir", c, true);
}

/*!
 * \brief update the seed of a UniqueID with what is passed in.
 * Since this function is called in the context of log replay, it
 * better be the case that the seed passed in is higher than
 * the id's seed (which was set from a checkpoint file).
*/
static void
updateSeed(UniqueID &id, seqid_t seed)
{
	if (seed < id.getseed()) {
		KFS_LOG_ERROR("Seed from log: %lld < id's seed: %lld", 
				seed, id.getseed());
		panic("Seed", false);
	}
	id.setseed(seed);
}

/*!
 * \brief replay a file create
 * format: create/dir/<parentID>/name/<name>/id/<myID>
 */
static bool
replay_create(deque <string> &c)
{
	fid_t parent, me;
	string myname;
	int status = 0;
	int16_t numReplicas;

	bool ok = pop_parent(parent, c);
	ok = pop_name(myname, "name", c, ok);
	ok = pop_fid(me, "id", c, ok);
	ok = pop_short(numReplicas, "numReplicas", c, ok);
	if (ok) {
		// for all creates that were successful during normal operation,
		// when we replay it should work; so, exclusive = false
		status = metatree.create(parent, myname, &me, numReplicas, false);
		if (status == 0)
			updateSeed(fileID, me);
	}
	KFS_LOG_DEBUG("Replay create: name=%s, id=%lld", myname.c_str(), me);
	return (ok && status == 0);
}

/*!
 * \brief replay mkdir
 * format: mkdir/dir/<parentID>/name/<name>/id/<myID>
 */
static bool
replay_mkdir(deque <string> &c)
{
	fid_t parent, me;
	string myname;
	int status = 0;
	bool ok = pop_parent(parent, c);
	ok = pop_name(myname, "name", c, ok);
	ok = pop_fid(me, "id", c, ok);
	if (ok) {
		status = metatree.mkdir(parent, myname, &me);
		if (status == 0)
			updateSeed(fileID, me);
	}
	KFS_LOG_DEBUG("Replay mkdir: name=%s, id=%lld", myname.c_str(), me);
	return (ok && status == 0);
}

/*!
 * \brief replay remove
 * format: remove/dir/<parentID>/name/<name>
 */
static bool
replay_remove(deque <string> &c)
{
	fid_t parent;
	string myname;
	int status = 0;
	bool ok = pop_parent(parent, c);
	ok = pop_name(myname, "name", c, ok);
	if (ok)
		status = metatree.remove(parent, myname);
	return (ok && status == 0);
}

/*!
 * \brief replay rmdir
 * format: rmdir/dir/<parentID>/name/<name>
 */
static bool
replay_rmdir(deque <string> &c)
{
	fid_t parent;
	string myname;
	int status = 0;
	bool ok = pop_parent(parent, c);
	ok = pop_name(myname, "name", c, ok);
	if (ok)
		status = metatree.rmdir(parent, myname);
	return (ok && status == 0);
}

/*!
 * \brief replay rename
 * format: rename/dir/<parentID>/old/<oldname>/new/<newpath>
 * NOTE: <oldname> is the name of file/dir in parent.  This
 * will never contain any slashes.
 * <newpath> is the full path of file/dir. This may contain slashes.
 * Since it is the last component, everything after new is <newpath>.
 * So, unlike <oldname> which just requires taking one element out,
 * we need to take everything after "new" for the <newpath>.
 * 
 */
static bool
replay_rename(deque <string> &c)
{
	fid_t parent;
	string oldname, newpath;
	int status = 0;
	bool ok = pop_parent(parent, c);
	ok = pop_name(oldname, "old", c, ok);
	ok = pop_path(newpath, "new", c, ok);
	if (ok)
		status = metatree.rename(parent, oldname, newpath, true);
	return (ok && status == 0);
}

/*!
 * \brief replay allocate
 * format: allocate/file/<fileID>/offset/<offset>/chunkId/<chunkID>/
 * chunkVersion/<chunkVersion>
 */
static bool
replay_allocate(deque <string> &c)
{
	fid_t fid;
	chunkId_t cid, logChunkId;
	chunkOff_t offset;
	seq_t chunkVersion, logChunkVersion;
	int status = 0;

	c.pop_front();
	bool ok = pop_fid(fid, "file", c, true);
	ok = pop_fid(offset, "offset", c, ok);
	ok = pop_fid(logChunkId, "chunkId", c, ok);
	ok = pop_fid(logChunkVersion, "chunkVersion", c, ok);
	if (ok) {
		cid = logChunkId;
		status = metatree.allocateChunkId(fid, offset, &cid, 
						&chunkVersion, NULL);
		if (status == -EEXIST) {
			// if the entry exists in the tree, the versions
			// better be different.
			assert(chunkVersion != logChunkVersion);
			status = 0;
		}

		if (status == 0) {
			assert(cid == logChunkId);
			chunkVersion = logChunkVersion;
			status = metatree.assignChunkId(fid, offset,
							cid, chunkVersion);
			if (status == 0) {
				gLayoutManager.AddChunkToServerMapping(cid, fid, NULL);
				updateSeed(chunkID, cid);
			}
		}
	}
	return (ok && status == 0);
}

/*!
 * \brief replay truncate
 * format: truncate/file/<fileID>/offset/<offset>
 */
static bool
replay_truncate(deque <string> &c)
{
	fid_t fid;
	chunkOff_t offset;
	int status = 0;

	c.pop_front();
	bool ok = pop_fid(fid, "file", c, true);
	ok = pop_fid(offset, "offset", c, ok);
	if (ok) {
		chunkOff_t allocOffset;

		// an allocation should not occur during replay
		status = metatree.truncate(fid, offset, &allocOffset);
	}
	return (ok && status == 0);
}

/*!
 * \brief restore time
 * format: time/<time>
 */
static bool
restore_time(deque <string> &c)
{
	c.pop_front();
	std::cout << "Log time: " << c.front() << std::endl;
	return true;
}

static void
init_map(DiskEntry &e)
{
	e.add_parser("version", replay_version);
	e.add_parser("create", replay_create);
	e.add_parser("mkdir", replay_mkdir);
	e.add_parser("remove", replay_remove);
	e.add_parser("rmdir", replay_rmdir);
	e.add_parser("rename", replay_rename);
	e.add_parser("allocate", replay_allocate);
	e.add_parser("truncate", replay_truncate);
	e.add_parser("chunkVersionInc", restore_chunkVersionInc);
	e.add_parser("time", restore_time);
}

/*!
 * \brief replay contents of log file
 * \return	zero if replay successful, negative otherwise
 */
int
Replay::playlog()
{
	if (!file.is_open()) {
		//!< no log...so, reset the # to 0.
		number = 0;
		return 0;
	}

	const int MAXLINE = 400;
	char line[MAXLINE];
	int lineno = 0;

	DiskEntry entrymap;
	init_map(entrymap);

	bool is_ok = true;
	while (is_ok && !file.eof()) {
		++lineno;
		file.getline(line, MAXLINE);
		is_ok = entrymap.parse(line);
		if (!is_ok)
			std::cerr << "Error at line " << lineno << ": "
					<< line << '\n';
	}

	file.close();
	return is_ok ? 0 : -EIO;
}