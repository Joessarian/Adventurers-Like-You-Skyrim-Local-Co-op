#pragma once
#include <Enums.h>

namespace ALYSLC
{
	//========================================================================================
	// NOTE: Currently unused, as code was refactored to run in sequence in the Main Hook.
	// Keeping this for later, just in case I try to implement a proper multi-threaded system.
	// 
	// Ensures that requests to change a thread's state
	// do not get overwritten before they are carried out.
	// Example: Running->Paused->Running, but the second
	// "Running" request is set before the "Paused" request can be fulfilled.
	// Allows manager threads to set their current state when the requests are fulfilled
	// and thereby give a more accurate representation of their running state to
	// any other threads depending on their state.
	class ManagerStateInfo
	{
	public:
		ManagerStateInfo(ManagerType a_type) :
			state(ManagerState::kUninitialized), type(a_type)
		{
			logger::debug("[ManagerState] Manager created of type {}", type);
			requestedStateChanges.clear();
		}

		inline bool AddRequest(ManagerState a_requestedState)
		{
			// REMOVE
			const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
			logger::debug("[ManagerState] {} AddRequest: Lock: 0x{:X}.", type, hash);
			{
				{
					std::unique_lock<std::mutex> lock(reqMutex);
					// Add request only if the new state is different from the current state and
					// has not just been added to the queue, or if the request is the same as the current state
					// and also does not follow the same state request in a non-empty queue.
					// Discard requests to pause if the current state is uninitialized.
					bool discard = state == ManagerState::kUninitialized && a_requestedState != ManagerState::kRunning;
					bool empty = requestedStateChanges.empty();
					// Do not add the same state twice.
					bool notConsecutive = empty || (!empty && a_requestedState != requestedStateChanges.back());
					bool notCurrentState = empty && a_requestedState != state;
					if (!discard && notConsecutive && (notCurrentState || (!notCurrentState && !empty)))
					{
						logger::debug("[ManagerState] {} AddRequest: Change state to {}. Current request list length, front, back: {}, {}, {}. Current state: {}",
							type, a_requestedState, !empty ? requestedStateChanges.size() : 0,
							!empty ? requestedStateChanges.front() : ManagerState::kNone,
							!empty ? requestedStateChanges.back() : ManagerState::kNone, state);
						requestedStateChanges.push_back(a_requestedState);
						return true;
					}
					else
					{
						logger::debug("[ManagerState] {} AddRequest: Not adding requested state change {}. Same as current state: {}, request list is empty: {}, back ({}) is the same as requested state: {}, discard: {}",
							type, a_requestedState, !notCurrentState, empty,
							!empty ? requestedStateChanges.back() : ManagerState::kNone,
							!empty ? !notConsecutive : false, discard);
						return false;
					}
				}
			}
		}

		inline void ClearRequests()
		{
			// REMOVE
			const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
			logger::debug("[ManagerState] {} ClearRequests: Lock: 0x{:X}.", type, hash);
			{
				std::unique_lock<std::mutex> lock(reqMutex);
				logger::debug("[ManagerState] {} ClearRequests: current length: {}, front: {}, back: {}.",
					type, !requestedStateChanges.empty() ? requestedStateChanges.size() : 0,
					requestedStateChanges.empty() ? ManagerState::kNone : requestedStateChanges.front(),
					requestedStateChanges.empty() ? ManagerState::kNone : requestedStateChanges.back());
				requestedStateChanges.clear();
			}
		}

		inline bool FulfillRequest()
		{
			// REMOVE
			const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
			logger::debug("[ManagerState] {} FulfillRequest: Lock: 0x{:X}.", type, hash);
			{
				std::unique_lock<std::mutex> lock(reqMutex);
				if (!requestedStateChanges.empty())
				{
					auto oldState = state;
					state = requestedStateChanges.front();
					state = state == ManagerState::kNone ? oldState : state;
					requestedStateChanges.pop_front();
					logger::debug("[ManagerState] {} FulfillRequest: Thread state change complete: {} -> {}, list size is now {}, with front {}.",
						type, oldState, state, !requestedStateChanges.empty() ? requestedStateChanges.size() : 0,
						requestedStateChanges.empty() ? ManagerState::kNone : requestedStateChanges.front());
					return true;
				}
				else
				{
					logger::debug("[ManagerState] {} FulfillRequest: Request list is empty. Maintaining current state {}.", type, state);
					return false;
				}
			}
		}

		inline ManagerState GetNextRequest()
		{
			// REMOVE
			const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
			logger::debug("[ManagerState] {} GetNextRequest: Lock: 0x{:X}.", type, hash);
			{
				std::unique_lock<std::mutex> lock(reqMutex);
				logger::debug("[ManagerState] {} GetNextRequest: Lock obtained: 0x{:X}.", type, hash);
				if (!requestedStateChanges.empty())
				{
					return requestedStateChanges.front();
				}
				else
				{
					return ManagerState::kNone;
				}
			}
		}

		inline constexpr bool IsPaused() const
		{
			return state == ManagerState::kPaused;
		}

		inline constexpr bool IsRunning() const
		{
			return state == ManagerState::kRunning;
		}

		inline constexpr bool IsTerminated() const
		{
			return state == ManagerState::kUninitialized;
		}

		ManagerState state;
		ManagerType type;
		std::mutex reqMutex;

	private:
		std::deque<ManagerState> requestedStateChanges;

		// REMOVE LATER: for debug print purposes only.
		static inline const std::unordered_map<ManagerType, std::string_view> typeToStr = {
			{ ManagerType::kCAM, "[CAM]" },
			{ ManagerType::kMIM, "[MIM]" },
			{ ManagerType::kMM, "[MM]" },
			{ ManagerType::kNone, "NONE" },
			{ ManagerType::kPAM, "[PAM]" },
			{ ManagerType::kTotal, "TOTAL" }
		};
	};

	// Outlines a system of per-frame execution that performs a task, tracks its own running state,
	// can self-start/pause, refresh its data, and react to external running state change requests.
	class Manager
	{
	public:
		Manager(ManagerType a_type) :
			currentState(ManagerState::kUninitialized), nextState(ManagerState::kUninitialized), type(a_type)
		{ }

		// External thread requesting a manager state change.
		// Currently, requests are not enqueued, so whatever request is made last
		// in a series of quick requests will be handled, and all other requests will be discarded.
		virtual inline void RequestStateChange(const ManagerState& a_newState)
		{
			// Can only change to the running state when the manager is uninitialized.
			if (currentState == ManagerState::kUninitialized && a_newState != ManagerState::kRunning) 
			{
				return;
			}

			// REMOVE
			const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
			logger::debug("{} RequestStateChange: Lock: 0x{:X}.", type, hash);
			{
				logger::debug("{} RequestStateChange: Current State: {}, next state: {}. ({})", type, currentState, a_newState, hash);
				std::unique_lock<std::mutex> lock(setStateMutex);
				logger::debug("{} RequestStateChange: Set state lock acquired ({}).", type, hash);
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
			else if (currentState == ManagerState::kPaused || currentState == ManagerState::kAwaitingRefresh)
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
				if (nextState == ManagerState::kPaused || nextState == ManagerState::kAwaitingRefresh)
				{
					logger::debug("{} CheckForStateChange: Waiting to resume. State goes from {} -> {}.", type, currentState, nextState);

					// Run pre-pause task and switch to the paused/awaiting refresh execution state.
					PrePauseTask();
					currentState = nextState;
				}
				else if (nextState == ManagerState::kRunning)
				{
					logger::debug("{} CheckForStateChange: Resuming. State goes from {} -> {}.", type, currentState, nextState);

					// Refresh data if currently waiting to refresh data or uninitialized.
					// Then run pre-start task and switch to the running execution state.
					if (currentState == ManagerState::kAwaitingRefresh || currentState == ManagerState::kUninitialized)
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
				// REMOVE
				const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
				logger::debug("{} SelfPauseCheck: Try to lock: 0x{:X}.", type, hash);
				{
					std::unique_lock<std::mutex> lock(setStateMutex, std::try_to_lock);
					if (lock)
					{
						nextState = reqState;
						logger::debug("{} SelfPauseCheck: Succeeded in acquiring set state lock to pause with main thread ({}).", type, hash);
					}
					else
					{
						logger::debug("{} SelfPauseCheck: Failed to acquire set state lock to pause with main thread ({}).", type, hash);
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
				// REMOVE
				const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
				logger::debug("{} SelfResumeCheck: Try to lock: 0x{:X}.", type, hash);
				{
					std::unique_lock<std::mutex> lock(setStateMutex, std::try_to_lock);
					if (lock)
					{
						nextState = reqState;
						logger::debug("{} SelfResumeCheck: Succeeded in acquiring set state lock to resume with main thread ({}).", type, hash);
					}
					else
					{
						logger::debug("{} SelfResumeCheck: Failed to acquire set state lock to resume with main thread ({}).", type, hash);
					}
				}
			}
		}
	};

	// A separate thread of execution that runs enqueued tasks.
	class TaskRunner
	{
	public:
		// NOTE: All member functions are run on detached threads, 
		// so we can use locks without worrying about deadlocking one of the game's main threads.
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
			std::jthread tempEnqueuerThread([this, a_task]() {
				// REMOVE
				const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
				logger::debug("[Manager] TaskRunner: AddTask: Try to lock: 0x{:X}.", hash);
				{
					std::unique_lock<std::mutex> lock(queueMutex, std::try_to_lock);
					if (lock)
					{
						queue.emplace(a_task);
						queueCV.notify_all();

						// REMOVE
						logger::debug("[Manager] TaskRunner: Added a task. Queue size is now: {}", queue.size());
					}
					else
					{
						// REMOVE
						logger::critical("[Manager] TaskRunner: Failed to obtain lock. Not adding task. Queue size is now: {}", queue.size());
					}

					/*std::unique_lock<std::mutex> lock(queueMutex);
					queue.emplace(a_task);
					queueCV.notify_all();*/
				}
			});

			// Detach the thread.
			// Prevents the main thread from locking up if called from the main thread.
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
			// REMOVE
			const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
			logger::debug("[Manager] TaskRunner: ClearTasks: Lock: 0x{:X}.", hash);
			{
				std::unique_lock<std::mutex> lock(queueMutex);
				logger::debug("[Manager] TaskRunner: ClearTasks: Lock obtained: 0x{:X}.", hash);
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
				// REMOVE
				const auto hash = std::hash<std::jthread::id>()(std::this_thread::get_id());
				logger::debug("[Manager] RunTasks: Lock: 0x{:X}.", hash);
				{
					std::unique_lock<std::mutex> lock(queueMutex);
					queueCV.wait(lock,
						[&]() {
							logger::debug("[Manager] TaskRunner: Waiting for a task.");
							return (queue.size() > 0 || a_stoken.stop_requested());
						});

					while (queue.size() > 0)
					{
						logger::debug("[Manager] TaskRunner: Got a task.");
						queue.front()();
						queue.pop();
					}
				}
			}

			// Clear out all remaining tasks when done.
			logger::debug("[Manager] TaskRunner: Requested to stop runner thread.");
			ClearTasks();
		}
	};
};
