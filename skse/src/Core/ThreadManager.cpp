#include "Core/ThreadManager.h"
#include "UI/UIState.h"

#include "GameAPI/Game.h"
#include "Util/Constants.h"
#include "Util/VectorUtil.h"

namespace OStim {

    ThreadManager::ThreadManager() {
        m_excitementThread = std::thread([&]() {
            auto sleepTime = std::chrono::milliseconds(Constants::LOOP_TIME_MILLISECONDS);

            while (true) {
                auto start = std::chrono::system_clock::now();

                if (!GameAPI::Game::isPaused()) {
                    std::shared_lock<std::shared_mutex> lock(m_threadMapMtx);
                    for (auto& it : m_threadMap) {
                        it.second->loop();
                    }

                    for (ThreadId& threadID : threadStopQueue) {
                        stopThreadNoLock(threadID);
                    }
                    threadStopQueue.clear();

                    UI::UIState::GetSingleton()->loop();
                }

                auto end = std::chrono::system_clock::now();
                auto time = end - start;
                if (time < sleepTime) {
                    std::this_thread::sleep_for(sleepTime - time);
                }

                //std::this_thread::sleep_for(std::chrono::milliseconds(Constants::LOOP_TIME_MILLISECONDS));
            }
        });
        m_excitementThread.detach();
    }


    int ThreadManager::startThread(ThreadStartParams params) {
        std::unique_lock<std::shared_mutex> lock(m_threadMapMtx);
        int threadID = -1;
        for (GameAPI::GameActor actor : params.actors) {
            if (actor.isPlayer()) {
                threadID = 0;
                break;
            }
        }
        if (threadID == 0) {
            if (m_threadMap.contains(0)) {
                GameAPI::Game::notification("main thread already running");
                return -1;
            }
        } else {
            threadID = idGenerator.get();
        }

        Thread* thread = new Thread(threadID, params);
        m_threadMap.insert(std::make_pair(threadID, thread));
        thread->initContinue();
        if (params.startingNodes.size() == 1) {
            thread->ChangeNode(params.startingNodes.front().node);
        } else if (!params.startingNodes.empty()) {
            thread->playSequence(params.startingNodes, false, false);
        }
        return threadID;
    }

    Thread* ThreadManager::GetThread(ThreadId a_id) {
        std::shared_lock<std::shared_mutex> lock(m_threadMapMtx);
        auto it = m_threadMap.find(a_id);
        if (it == m_threadMap.end()) {
            return nullptr;
        }
        return it->second;
    }

    Thread* ThreadManager::getPlayerThread() {
        std::shared_lock<std::shared_mutex> lock(m_threadMapMtx);
        for (auto& [threadID, thread] : m_threadMap) {
            if (thread->isPlayerThread()) {
                return thread;
            }
        }
        return nullptr;
    }

    void ThreadManager::queueThreadStop(ThreadId threadID) {
        if (!VectorUtil::contains(threadStopQueue, threadID)) {
            threadStopQueue.push_back(threadID);
        }
    }

    void ThreadManager::UntrackAllThreads() {
        // this is a force close due to the user loading another save
        // so no need to free actors etc. here
        std::unique_lock<std::shared_mutex> lock(m_threadMapMtx);
        for (auto& entry : m_threadMap) {
            delete entry.second;
        }
        m_threadMap.clear();
        idGenerator.reset();
    }

    bool ThreadManager::AnySceneRunning() {
        return m_threadMap.size() > 0;
    }

    bool ThreadManager::playerThreadRunning() {
        std::shared_lock<std::shared_mutex> lock(m_threadMapMtx);
        for (auto& [id, thread] : m_threadMap) {
            if (thread->isPlayerThread()) {
                return true;
            }
        }
        return false;
    }

    Thread* ThreadManager::findThread(GameAPI::GameActor actor) {
        std::shared_lock<std::shared_mutex> lock(m_threadMapMtx);
        for (auto& [id, thread] : m_threadMap) {
            ThreadActor* threadActor = thread->GetActor(actor);
            if (threadActor) {
                return thread;
            }
        }

        return nullptr;
    }

    ThreadActor* ThreadManager::findActor(GameAPI::GameActor actor) {
        std::shared_lock<std::shared_mutex> lock(m_threadMapMtx);
        for (auto&[id, thread] : m_threadMap) {
            ThreadActor* threadActor = thread->GetActor(actor);
            if (threadActor) {
                return threadActor;
            }
        }

        return nullptr;
    }

    // serialized the currently running threads so that the actors can be properly set free on game load
    std::vector<Serialization::OldThread> ThreadManager::serialize() {
        std::vector<Serialization::OldThread> oldThreads;

        for (auto& it : m_threadMap) {
            oldThreads.push_back(it.second->serialize());
        }

        return oldThreads;
    }

    void ThreadManager::stopThreadNoLock(ThreadId threadID) {
        logger::info("trying to stop thread {}", threadID);
        auto it = m_threadMap.find(threadID);
        if (it != m_threadMap.end()) {
            Thread* thread = it->second;
            UI::UIState::GetSingleton()->HandleThreadRemoved(thread);
            m_threadMap.erase(threadID);
            thread->close();
            delete thread;
            if (threadID != 0) {
                idGenerator.free(threadID);
            }
            auto log = RE::ConsoleLog::GetSingleton();
            if (log) {
                log->Print(("Found scene: erasing " + std::to_string(threadID)).c_str());
            }
        } else {
            logger::info("no thread found with id {}", threadID);
        }
    }
}  // namespace OStim