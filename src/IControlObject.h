// Copyright Dan Price 2026.

#pragma once

#include "Memory.h"

#ifdef ALBUMBOT_DEBUGNEW
#include "StdMapWrapper.h"
#else
#include <map>
#endif

#include <utility>
#include <stdexcept>
#include <type_traits>
#include <cstdlib>

namespace json2wav
{
	class ControlObjectHolder;

	class IEvent
	{
	public:
		virtual ~IEvent() noexcept {}
		virtual void Activate(ControlObjectHolder& ctrl, const size_t samplenum) const = 0;
	};

	template<typename EventType, typename AddEventType> class ControlObject;

	// The user is responsible for tracking the contained type
	class ControlObjectHolder
	{
	public:
		ControlObjectHolder(const ControlObjectHolder& other) noexcept
			: ctrl(other.ctrl)
		{
			if (ctrl)
			{
				ctrl->AddHolder(this);
			}
		}
		ControlObjectHolder& operator=(const ControlObjectHolder& other) noexcept
		{
			if (&other != this)
			{
				if (ctrl)
				{
					ctrl->RemoveHolder(this);
				}
				ctrl = other.ctrl;
				if (ctrl)
				{
					ctrl->AddHolder(this);
				}
			}
			return *this;
		}

		ControlObjectHolder(ControlObjectHolder&& other) noexcept
			: ctrl(std::move(other.ctrl))
		{
			if (ctrl)
			{
				ctrl->ReplaceHolder(&other, this);
				other.ctrl = nullptr;
			}
		}
		ControlObjectHolder& operator=(ControlObjectHolder&& other) noexcept
		{
			if (&other != this)
			{
				if (ctrl)
				{
					ctrl->RemoveHolder(this);
				}

				if (other.ctrl)
				{
					ctrl = std::move(other.ctrl);
					other.ctrl = nullptr;
					ctrl->ReplaceHolder(&other, this);
				}
				else
				{
					ctrl = nullptr;
				}
			}
			return *this;
		}

		template<typename T> explicit ControlObjectHolder(const SharedPtr<T>& ctrlInit)
			: ctrl(ctrlInit)
		{
			if (ctrl)
			{
				ctrl->AddHolder(this);
			}
		}
		template<typename T> explicit ControlObjectHolder(SharedPtr<T>&& ctrlInit)
			: ctrl(std::move(ctrlInit))
		{
			if (ctrl)
			{
				ctrl->AddHolder(this);
			}
		}

		~ControlObjectHolder() noexcept
		{
			if (ctrl)
			{
				ctrl->RemoveHolder(this);
			}
		}

		template<typename T> const T& Get() const
		{
			return static_cast<const T&>(*ctrl);
		}
		template<typename T> T& Get()
		{
			return static_cast<T&>(*ctrl);
		}

		template<typename T> SharedPtr<T> GetPtr() const
		{
			if (!ctrl)
			{
				return nullptr;
			}
			SharedPtr<T> ptr(SharedPtrCast<T>(ctrl));
			return ptr;
		}

		template<typename T> SharedPtr<T> CopyPtr() const
		{
			if (!ctrl)
			{
				return nullptr;
			}
			SharedPtr<T> ptr(SharedPtrCast<T>(ctrl));
			return ptr;
		}

	private:
		template<typename T, typename U>
		friend class ControlObject;

		friend class ControlSet;

		class ControlObjectBase
		{
			template<typename T, typename U>
			friend class ControlObject;

			friend class ControlObjectHolder;

		protected:
			ControlObjectBase() noexcept {}

		public:
			virtual ~ControlObjectBase() noexcept = 0;

		protected:
			template<typename EventType, typename... CtorParamTypes>
			bool AddEventInternal(const size_t samplenum, CtorParamTypes&&... params)
			{
				auto it = events.find(samplenum);
				if (it == events.end())
				{
					auto empit = events.emplace(samplenum, Vector<SharedPtr<IEvent>>());
					if (!empit.second)
					{
						return false;
					}
					it = empit.first;
				}
				it->second.emplace_back(MakeShared<EventType>(std::forward<CtorParamTypes>(params)...));
				return true;
			}

			std::map<size_t, Vector<SharedPtr<IEvent>>>& GetEventsMap() noexcept { return events; }
			const std::map<size_t, Vector<SharedPtr<IEvent>>>& GetEventsMap() const noexcept { return events; }

			bool IsHeld() const noexcept { return holders.size() > 0; }
			ControlObjectHolder& GetHolder(const size_t idx = 0) { return *holders[idx]; }

		private:
			void AddHolder(ControlObjectHolder* const newHolder)
			{
				if (!newHolder)
				{
					return;
				}

				for (ControlObjectHolder* const holder : holders)
				{
					if (holder == newHolder)
					{
						return;
					}
				}

				holders.push_back(newHolder);
				if (&*newHolder->ctrl != this)
				{
					throw std::logic_error("ControlObjectHolder::ControlObjectBase::SetHolder(): Set incorrect holder");
				}
			}

			void RemoveHolder(ControlObjectHolder* const oldHolder)
			{
				if (!oldHolder)
				{
					return;
				}

				for (auto it = holders.begin(), end = holders.end(); it != end; ++it)
				{
					if (*it == oldHolder)
					{
						holders.erase(it);
						return;
					}
				}
			}

			void ReplaceHolder(ControlObjectHolder* const oldHolder, ControlObjectHolder* const newHolder)
			{
				if (!oldHolder || oldHolder == newHolder)
				{
					return;
				}

				if (!newHolder)
				{
					RemoveHolder(oldHolder);
					return;
				}

				for (ControlObjectHolder*& holder : holders)
				{
					if (holder == oldHolder)
					{
						holder = newHolder;
						return;
					}
				}
			}

		private:
			Vector<ControlObjectHolder*> holders;
			std::map<size_t, Vector<SharedPtr<IEvent>>> events;
		};

		SharedPtr<ControlObjectBase> ctrl;
	};

	inline ControlObjectHolder::ControlObjectBase::~ControlObjectBase() noexcept
	{
	}

	template<typename ControlType, typename... ParamTypes>
	inline ControlObjectHolder CreateControl(ParamTypes&&... params)
	{
		ControlObjectHolder holder(MakeShared<ControlType>(std::forward<ParamTypes>(params)...));
		return holder;
	}

	template<typename ControlType>
	inline ControlObjectHolder WrapControl(const SharedPtr<ControlType>& ptr)
	{
		ControlObjectHolder holder(ptr);
		return holder;
	}
	template<typename ControlType>
	inline ControlObjectHolder WrapControl(SharedPtr<ControlType>&& ptr)
	{
		ControlObjectHolder holder(std::move(ptr));
		return holder;
	}

	class ControlSet
	{
	public:
		template<typename ControlType, typename... ParamTypes>
		SharedPtr<ControlType> CreatePtr(ParamTypes&&... params)
		{
			ctrls.push_back(CreateControl<ControlType>(std::forward<ParamTypes>(params)...));
			return ctrls.back().GetPtr<ControlType>();
		}

		template<typename ControlType, typename... ParamTypes>
		std::pair<SharedPtr<ControlType>, size_t> CreatePair(ParamTypes&&... params)
		{
			const size_t idx(ctrls.size());
			return std::make_pair(CreatePtr<ControlType>(std::forward<ParamTypes>(params)...), idx);
		}

		template<typename ControlType>
		size_t Find(const SharedPtr<ControlType>& ptr)
		{
			static_assert(std::is_base_of_v<ControlObjectHolder::ControlObjectBase, ControlType>, "json2wav::ControlSet::Find() only operates on controls");
			for (size_t idx = 0; idx < ctrls.size(); ++idx)
			{
				SharedPtr<ControlObjectHolder::ControlObjectBase> basePtr(ctrls[idx].GetPtr<ControlObjectHolder::ControlObjectBase>());
				if (static_cast<void*>(basePtr.get()) == static_cast<void*>(ptr.get()))
				{
					return idx;
				}
			}
			return SIZE_MAX;
		}

		void Remove(const size_t idx)
		{
			if (idx < ctrls.size())
			{
				ctrls.erase(ctrls.begin() + idx);
			}
		}

		template<typename ControlType>
		void Remove(const SharedPtr<ControlType>& ptr)
		{
			Remove(Find(ptr));
		}

		ControlObjectHolder& operator[](const size_t idx) { return ctrls[idx]; }
		const ControlObjectHolder& operator[](const size_t idx) const { return ctrls[idx]; }
		size_t size() const noexcept { return ctrls.size(); }

	private:
		Vector<ControlObjectHolder> ctrls;
	};

	template<typename EventType, typename AddEventType = ControlObjectHolder::ControlObjectBase>
	class ControlObject : public ControlObjectHolder::ControlObjectBase
	{
	protected:
		ControlObject() : currentSampleNum(0), bRefreshEvents(false) {}

	public:
		virtual ~ControlObject() noexcept
		{
		}

		void Reset()
		{
			GetEventsMap().clear();
			currentSampleNum = 0;
			bRefreshEvents = false;
		}

		template<typename... Ts>
		bool AddEvent(const size_t samplenum, Ts&&... args)
		{
			return static_cast<AddEventType*>(this)->template AddEventInternal<EventType>(samplenum, std::forward<Ts>(args)...);
		}

		bool RemoveEvent(const size_t samplenum, const size_t idx)
		{
			const auto foundevents = GetEventsMap().find(samplenum);
			if (foundevents == GetEventsMap().end() || idx >= foundevents->second.size())
			{
				return false;
			}
			foundevents->second.erase(foundevents->second.begin() + idx);
			return true;
		}

		Vector<SharedPtr<IEvent>>& GetEvents(const size_t samplenum)
		{
			auto foundevents = GetEventsMap().find(samplenum);
			if (foundevents == GetEventsMap().end())
			{
				const auto empit = GetEventsMap().emplace(samplenum, Vector<SharedPtr<IEvent>>());
				if (!empit.second)
				{
					throw std::runtime_error("ControlObject::GetEvents: Couldn't create empty event vector.");
				}
				foundevents = empit.first;
			}
			return foundevents->second;
		}

		const Vector<SharedPtr<IEvent>>& GetEvents(const size_t samplenum) const
		{
			const auto foundevents = GetEventsMap().find(samplenum);
			if (foundevents == GetEventsMap().end())
			{
				throw std::out_of_range("ControlObject::GetEvents const: Couldn't find key.");
			}
			return foundevents->second;
		}

		Vector<size_t> GetEventKeysInRange(const size_t start, const size_t end) const
		{
			Vector<size_t> keys;

			if (start < end)
			{
				const auto startit = GetEventsMap().lower_bound(start);
				const auto endit = GetEventsMap().upper_bound(end - 1);
				if (startit != GetEventsMap().end())
				{
					for (auto it = startit; it != endit; ++it)
					{
						keys.emplace_back(it->first);
					}
				}
			}

			return keys;
		}

		size_t GetSampleNum() const noexcept
		{
			return currentSampleNum;
		}

		void RefreshEvents()
		{
			bRefreshEvents = true;
		}

	protected:
		void SetSampleNum(const size_t newSampleNum) noexcept
		{
			currentSampleNum = newSampleNum;
		}

		void IncrementSampleNum(const size_t deltasamples) noexcept
		{
			currentSampleNum += deltasamples;
		}

		template<typename ProcSampFunc>
		void ProcessEvents(const size_t numSamples, ProcSampFunc&& ProcessSample)
		{
			const size_t sampleNum = GetSampleNum();
			Vector<size_t> eventkeys(GetEventKeysInRange(sampleNum, sampleNum + numSamples));
			size_t keyIdx = 0;

			for (size_t i = 0, n = sampleNum; i < numSamples; )
			{
				if (bRefreshEvents)
				{
					eventkeys = GetEventKeysInRange(n + 1, sampleNum + numSamples);
					keyIdx = 0;
					bRefreshEvents = false;
				}

				const size_t k = (eventkeys.size() > keyIdx) ? eventkeys[keyIdx] : sampleNum + numSamples;
				for ( ; n < k; ++n, ++i)
				{
					ProcessSample(i);
				}

				if (eventkeys.size() > keyIdx)
				{
					TriggerEvents(k);
					++keyIdx;
				}
			}

			IncrementSampleNum(numSamples);
		}

	private:
		void TriggerEvents(const size_t samplenum)
		{
			if (!IsHeld())
			{
				return;
			}

			const auto it = GetEventsMap().find(samplenum);
			if (it != GetEventsMap().end())
			{
				for (const SharedPtr<IEvent>& event : it->second)
				{
					if (event)
					{
						event->Activate(GetHolder(), samplenum);
					}
				}
				GetEventsMap().erase(it);
			}
		}

	private:
		size_t currentSampleNum;
		bool bRefreshEvents;
	};
}

