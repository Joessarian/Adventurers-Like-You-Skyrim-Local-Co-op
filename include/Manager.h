#pragma once
#include <Enums.h>
#include <Util.h>

namespace ALYSLC
{
	using SteadyClock = std::chrono::steady_clock;
	// Outlines a system of per-frame execution that performs a task, tracks its own running state,
	// can self-start/pause, refresh its data, and react to external running state change requests.
	class Manager
	{
	public:
		Manager(ManagerType a_type) :
			currentState(ManagerState::kUninitialized), 
			nextState(ManagerState::kUninitialized), 
			type(a_type)
		{ }

		// External thread requesting a manager state change.
		// Currently, requests are not enqueued, so whatever request is made last
		// in a series of quick requests will be handled, and all other requests will be discarded.
		inline void RequestStateChange(const ManagerState& a_newState)
		{
			// Can only change to the running state when the manager is uninitialized.
			if (currentState == ManagerState::kUninitialized && 
				a_newState != ManagerState::kRunning) 
			{
				return;
			}

			{
				SPDLOG_DEBUG
				(
					"{} RequestStateChange: Current State: {}, next state: {}. "
					"Getting lock. (0x{:X})", 
					type, currentState, 
					a_newState, 
					std::hash<std::jthread::id>()(std::this_thread::get_id())
				);
				std::unique_lock<std::mutex> lock(setStateMutex);
				SPDLOG_DEBUG("{} RequestStateChange: Set state lock acquired. (0x{:X})", 
					type, 
					std::hash<std::jthread::id>()(std::this_thread::get_id()));
				nextState = a_newState;
			}
		}

		// Called per frame to run main task and update the manager's state.
		inline void Update()
		{
			// Check if the manager should self-pause or self-resume execution,
			// and sets the next execution state.
			if (currentState == ManagerState::kRunning)
			{
				SelfPauseCheck();
			}
			else if (currentState == ManagerState::kPaused || 
					 currentState == ManagerState::kAwaitingRefresh)
			{
				SelfResumeCheck();
			}

			// Checks if the next execution state has changed
			// and update the current state after performing any pre-state change tasks.
			CheckForStateChange();

			// Run the main task if the current state is set to running.
			if (currentState == ManagerState::kRunning)
			{
				MainTask();
			}
		}

		inline bool IsAwaitingRefresh() const 
		{
			return currentState == ManagerState::kAwaitingRefresh;
		}

		inline bool IsPaused() const
		{
			return currentState == ManagerState::kPaused;
		}

		inline bool IsRunning() const
		{
			return currentState == ManagerState::kRunning;
		}

		inline bool IsUninitialized() const
		{
			return currentState == ManagerState::kUninitialized;
		}

		// Current execution state.
		ManagerState currentState;

	protected:
		virtual void MainTask() = 0;
		virtual void PrePauseTask() = 0;
		virtual void PreStartTask() = 0;
		virtual void RefreshData() = 0;
		virtual const ManagerState ShouldSelfPause() = 0;
		virtual const ManagerState ShouldSelfResume() = 0;

		// Next execution state to switch to.
		ManagerState nextState;
		// The type of manager.
		// Primarily used for debug prints.
		ManagerType type;
		// Mutex for execution state changes.
		std::mutex setStateMutex;

	private:
		// Monitor requests to change the current execution state and perform the state change.
		inline void CheckForStateChange()
		{
			// Execution state was requested to change.
			if (currentState != nextState)
			{
				if (nextState == ManagerState::kPaused || 
					nextState == ManagerState::kAwaitingRefresh)
				{
					SPDLOG_DEBUG
					(
						"{} CheckForStateChange: Waiting to resume. "
						"State goes from {} -> {}.", 
						type, currentState, nextState
					);

					// Run pre-pause task 
					// and switch to the paused/awaiting refresh execution state.
					PrePauseTask();
					currentState = nextState;
				}
				else if (nextState == ManagerState::kRunning)
				{
					SPDLOG_DEBUG("{} CheckForStateChange: Resuming. State goes from {} -> {}.", 
						type, currentState, nextState);

					// Refresh data if currently waiting to refresh data or uninitialized.
					// Then run pre-start task and switch to the running execution state.
					if (currentState == ManagerState::kAwaitingRefresh || 
						currentState == ManagerState::kUninitialized)
					{
						RefreshData();
					}

					PreStartTask();
					currentState = nextState;
				}
			}
		}

		// Monitor conditions for automatically switching over to the paused/awaiting refresh
		// execution state and update the next execution state to match.
		// This state change is not triggered externally and is run per-frame.
		inline void SelfPauseCheck()
		{
			const auto reqState = ShouldSelfPause();
			if (reqState == ManagerState::kPaused || reqState == ManagerState::kAwaitingRefresh)
			{
				{
					std::unique_lock<std::mutex> lock(setStateMutex, std::try_to_lock);
					if (lock)
					{
						nextState = reqState;
						SPDLOG_DEBUG
						(
							"{} SelfPauseCheck: Succeeded in acquiring set state lock to pause. "
							"(0x{:X})",
							type, std::hash<std::jthread::id>()(std::this_thread::get_id())
						);
					}
					else
					{
						SPDLOG_DEBUG
						(
							"{} SelfPauseCheck: Failed to acquire set state lock to pause. "
							"(0x{:X})", 
							type, std::hash<std::jthread::id>()(std::this_thread::get_id())
						);
					}
				}
			}
		}

		// Monitor conditions for automatically switching over to the running execution state 
		// and update the next execution state to match.
		// This state change is not triggered externally and is run per-frame.
		inline void SelfResumeCheck()
		{
			const auto reqState = ShouldSelfResume();
			if (reqState == ManagerState::kRunning)
			{
				{
					std::unique_lock<std::mutex> lock(setStateMutex, std::try_to_lock);
					if (lock)
					{
						nextState = reqState;
						SPDLOG_DEBUG
						(
							"{} SelfResumeCheck: Succeeded in acquiring set state lock to resume. "
							"(0x{:X})", 
							type, std::hash<std::jthread::id>()(std::this_thread::get_id())
						);
					}
					else
					{
						SPDLOG_DEBUG
						(
							"{} SelfResumeCheck: Failed to acquire set state lock to resume. "
							"(0x{:X})",
							type, std::hash<std::jthread::id>()(std::this_thread::get_id())
						);
					}
				}
			}
		}
	};

	// A separate thread of execution that runs enqueued tasks.
	class TaskRunner
	{
	public:
		// NOTE: 
		// All member functions are run on detached threads
		// to avoid deadlocking one of the game's main threads when waiting on a condition.
		TaskRunner() :
			queue(),
			runnerThread([this](std::stop_token a_stoken) { RunTasks(a_stoken); })
		{ 
			runnerThread.detach();
		}

		~TaskRunner() 
		{
			runnerThread.request_stop();
		}

		// Add a custom task to the queue.
		inline void AddTask(std::function<void()> a_task) 
		{
			std::jthread tempEnqueuerThread([this, a_task]() 
			{
				// Continue trying to enqueue task for, at most, 5 seconds.
				SteadyClock::time_point startTP = SteadyClock::now();
				while (Util::GetElapsedSeconds(startTP, true) < 5.0f)
				{
					std::unique_lock<std::mutex> lock(queueMutex, std::try_to_lock);
					if (lock)
					{	queue.emplace(a_task);
						queueCV.notify_all();
						break;
					}
					else
					{
						std::this_thread::sleep_for
						(
							std::chrono::seconds(static_cast<long long>(*g_deltaTimeRealTime))
						);
					}
				}
			});

			// Detach the thread to avoid locking up one of the game's threads.
			tempEnqueuerThread.detach();
		}

		// Condition variable waited upon by the task runner thread.
		std::condition_variable queueCV;
		// Worker thread running tasks.
		std::jthread runnerThread;
		// Mutex for enqueueing new tasks.
		std::mutex queueMutex;
		// The task queue.
		std::queue<std::function<void()>> queue;

	private:
		// Clear out all enqueued tasks.
		// None are executed.
		inline void ClearTasks()
		{
			{
				SPDLOG_DEBUG("[Manager] TaskRunner: ClearTasks: Getting lock. (0x{:X})", 
					std::hash<std::jthread::id>()(std::this_thread::get_id()));
				std::unique_lock<std::mutex> lock(queueMutex);
				SPDLOG_DEBUG("[Manager] TaskRunner: ClearTasks: Lock obtained. (0x{:X})", 
					std::hash<std::jthread::id>()(std::this_thread::get_id()));
				while (!queue.empty())
				{
					queue.pop();
				}
			}
		}

		inline void RunTasks(std::stop_token a_stoken)
		{
			// Continue looping until externally prompted to stop.
			while (!a_stoken.stop_requested())
			{
				{
					SPDLOG_DEBUG("[Manager] RunTasks: Getting lock. (0x{:X})", 
						std::hash<std::jthread::id>()(std::this_thread::get_id()));
					std::unique_lock<std::mutex> lock(queueMutex);
					queueCV.wait
					(
						lock,
						[&]() 
						{
							SPDLOG_DEBUG("[Manager] TaskRunner: Waiting for a task.");
							return (queue.size() > 0 || a_stoken.stop_requested());
						}
					);

					while (queue.size() > 0)
					{
						SPDLOG_DEBUG("[Manager] TaskRunner: Got a task.");
						queue.front()();
						queue.pop();
					}
				}
			}

			// Clear out all remaining tasks when done.
			SPDLOG_DEBUG("[Manager] TaskRunner: Requested to stop runner thread.");
			ClearTasks();
		}
	};
};
