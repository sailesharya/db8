/* @@@LICENSE
*
* Copyright (c) 2009-2013 LG Electronics, Inc.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* LICENSE@@@ */


#include "db-luna/MojDbLunaServiceApp.h"
#include "db/MojDbServiceDefs.h"
#include "db/MojDbServiceHandler.h"

#ifdef MOJ_USE_BDB
#include "db-luna/MojDbBerkeleyFactory.h"
#elif MOJ_USE_LDB
#include "db-luna/leveldb/MojDbLevelFactory.h"
#else
#error "Set database type"
#endif

#ifndef MOJ_VERSION_STRING
#define MOJ_VERSION_STRING NULL
#endif

const MojChar* const MojDbLunaServiceApp::VersionString = MOJ_VERSION_STRING;
const MojChar* const MojDbLunaServiceApp::MainDir = _T("main");
const MojChar* const MojDbLunaServiceApp::MediaDir = _T("media");
const MojChar* const MojDbLunaServiceApp::TempDir = _T("temp");
const MojChar* const MojDbLunaServiceApp::TempStateDir = _T("/tmp/mojodb");
const MojChar* const MojDbLunaServiceApp::TempInitStateFile = _T("/tmp/mojodb/tempdb_init");

int main(int argc, char** argv)
{
   MojAutoPtr<MojDbLunaServiceApp> app(new MojDbLunaServiceApp);
   MojAllocCheck(app.get());
   return app->main(argc, argv);
}

MojDbLunaServiceApp::MojDbLunaServiceApp()
: MojReactorApp<MojGmainReactor>(MajorVersion, MinorVersion, VersionString)
, m_mainService(m_dispatcher)
, m_mediaService(m_dispatcher)
, m_tempService(m_dispatcher)
{
   // set up db first
#ifdef MOJ_USE_BDB
   MojDbStorageEngine::setEngineFactory(new MojDbBerkeleyFactory());
#elif MOJ_USE_LDB
   MojDbStorageEngine::setEngineFactory(new MojDbLevelFactory());
#else
  #error "Database not set"
#endif
}

MojDbLunaServiceApp::~MojDbLunaServiceApp()
{
}

MojErr MojDbLunaServiceApp::init()
{
    LOG_TRACE("Entering function %s", __FUNCTION__);

	MojErr err = Base::init();
	MojErrCheck(err);

    MojDbStorageEngine::createEnv(m_env);
    MojAllocCheck(m_env.get());

    m_internalHandler.reset(new MojDbServiceHandlerInternal(m_mainService.db(), m_reactor, m_mainService.service()));
    MojAllocCheck(m_internalHandler.get());
    m_tempInternalHandler.reset(new MojDbServiceHandlerInternal(m_tempService.db(), m_reactor, m_tempService.service()));
    MojAllocCheck(m_tempInternalHandler.get());
    m_mediaInternalHandler.reset(new MojDbServiceHandlerInternal(m_mediaService.db(), m_reactor, m_mediaService.service()));
    MojAllocCheck(m_mediaInternalHandler.get());

    err = m_mainService.init(m_reactor);
    MojErrCheck(err);
    err = m_mediaService.init(m_reactor);
    MojErrCheck(err);
    err = m_tempService.init(m_reactor);
    MojErrCheck(err);

    return MojErrNone;
}

MojErr MojDbLunaServiceApp::configure(const MojObject& conf)
{
    LOG_TRACE("Entering function %s", __FUNCTION__);

	MojErr err = Base::configure(conf);
	MojErrCheck(err);

    MojObject dbConf;

    conf.get(MojDbStorageEngine::engineFactory()->name(), dbConf);
    err = m_env->configure(dbConf);
    MojErrCheck(err);
    err = m_mainService.db().configure(conf);
    MojErrCheck(err);
    err = m_mediaService.db().configure(conf);
    MojErrCheck(err);
    err = m_tempService.db().configure(conf);
    MojErrCheck(err);

    m_conf = dbConf;

    return MojErrNone;
}

MojErr MojDbLunaServiceApp::open()
{
    LOG_TRACE("Entering function %s", __FUNCTION__);
    LOG_DEBUG("[mojodb] starting...");

	MojErr err = Base::open();
	MojErrCheck(err);

	// start message queue thread pool
	err = m_dispatcher.start(NumThreads);
	MojErrCheck(err);

	// open db env
	bool dbOpenFailed = false;
	err = m_env->open(m_dbDir);
	MojErrCatchAll(err) {
		dbOpenFailed = true;
	}

	// open db services
	err = m_mainService.open(m_reactor, m_env.get(), MojDbServiceDefs::ServiceName, m_dbDir, MainDir, m_conf);
	MojErrCatchAll(err) {
		dbOpenFailed = true;
	}
	err = m_mediaService.open(m_reactor, m_env.get(), MojDbServiceDefs::MediaServiceName, m_mediaDbDir, MediaDir, m_conf);
    MojErrCatchAll(err) {
        dbOpenFailed = true;
    }
	err = m_tempService.open(m_reactor, m_env.get(), MojDbServiceDefs::TempServiceName, m_dbDir, TempDir, m_conf);
	MojErrCatchAll(err) {
		dbOpenFailed = true;
	}
	if (!dbOpenFailed) {
		err = dropTemp();
		MojErrCheck(err);
	}

	// open internal handler
	err = m_internalHandler->open();
	MojErrCheck(err);
	err = m_mainService.service().addCategory(MojDbServiceDefs::InternalCategory, m_internalHandler.get());
	MojErrCheck(err);
	err = m_internalHandler->configure(dbOpenFailed);
	MojErrCheck(err);
    // tempdb and mediadb service also need internal category
    // to use scheduled space check and scheduled purge method through activity manager.
    err = m_tempInternalHandler->open();
    MojErrCheck(err);
    err = m_tempService.service().addCategory(MojDbServiceDefs::InternalCategory, m_tempInternalHandler.get());
    MojErrCheck(err);
    err = m_mediaInternalHandler->open();
    MojErrCheck(err);
    err = m_mediaService.service().addCategory(MojDbServiceDefs::InternalCategory, m_mediaInternalHandler.get());
    MojErrCheck(err);

    err = m_internalHandler->subscribe();
    MojErrCheck(err);
    err = m_tempInternalHandler->subscribe();
    MojErrCheck(err);
    err = m_mediaInternalHandler->subscribe();
    MojErrCheck(err);

    LOG_DEBUG("[mojodb] started");

	return MojErrNone;
}

MojErr MojDbLunaServiceApp::close()
{
    LOG_TRACE("Entering function %s", __FUNCTION__);
    LOG_DEBUG("[mojodb] stopping...");

	// stop dispatcher
	MojErr err = MojErrNone;
	MojErr errClose = m_dispatcher.stop();
	MojErrAccumulate(err, errClose);
	errClose = m_dispatcher.wait();
	MojErrAccumulate(err, errClose);
	// close services
	errClose = m_mainService.close();
	MojErrAccumulate(err, errClose);
	errClose = m_tempService.close();
	MojErrAccumulate(err, errClose);
    errClose = m_mediaService.close();
    MojErrAccumulate(err, errClose);

	m_internalHandler->close();
	m_internalHandler.reset();

	errClose = Base::close();
	MojErrAccumulate(err, errClose);

    LOG_DEBUG("[mojodb] stopped");

	return err;
}

MojErr MojDbLunaServiceApp::dropTemp()
{
    LOG_TRACE("Entering function %s", __FUNCTION__);

	MojStatT stat;
	MojErr err = MojStat(TempInitStateFile, &stat);
	MojErrCatch(err, MojErrNotFound) {
		err = m_tempService.db().drop(TempDir);
		MojErrCheck(err);
		err = m_tempService.openDb(m_env.get(), m_dbDir, TempDir, m_conf);
		MojErrCheck(err);
		err = MojCreateDirIfNotPresent(TempStateDir);
		MojErrCheck(err);
		err = MojFileFromString(TempInitStateFile, _T(""));
		MojErrCheck(err);
	}
	MojErrCheck(err);

	return MojErrNone;
}

MojErr MojDbLunaServiceApp::handleArgs(const StringVec& args)
{
    LOG_TRACE("Entering function %s", __FUNCTION__);

	MojErr err = Base::handleArgs(args);
	MojErrCheck(err);

	if (args.size() != 2)
		MojErrThrow(MojErrInvalidArg);

    m_dbDir = args[0];
    m_mediaDbDir = args[1];

	return MojErrNone;
}

MojErr MojDbLunaServiceApp::displayUsage()
{
    LOG_TRACE("Entering function %s", __FUNCTION__);

    MojErr err = displayMessage(_T("Usage: %s [OPTION]... db-dir mediadb-dir\n"), name().data());
    MojErrCheck(err);

	return MojErrNone;
}
