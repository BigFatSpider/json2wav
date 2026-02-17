// Copyright Dan Price 2026.

#include "Memory.h"
#include <vector>
#include <algorithm>
#include <fstream>
#include <cstdint>

namespace json2wav::riff
{
	using RiffSize = uint32_t;
	using Byte = uint8_t;

	constexpr const RiffSize MAX_SIZE = 0xffffffff;

	class RiffData
	{
	public:
		virtual Byte& GetByte(const RiffSize position) = 0;
		virtual Byte GetByte(const RiffSize position) const = 0;

	private:
		class IteratorBase
		{
		public:
			virtual void Invalidate() const noexcept = 0;
			virtual bool IsValid() const noexcept = 0;
		};

		mutable Vector<const IteratorBase*> iterators;

		void AddIterator(const IteratorBase* const it) const
		{
			iterators.push_back(it);
		}

		void RemoveIterator(const IteratorBase* const it) const
		{
			iterators.erase(std::remove(iterators.begin(), iterators.end(), it), iterators.end());
		}

	public:
		class Iterator : public IteratorBase
		{
		private:
			virtual void Invalidate() const noexcept override
			{
				dataref = nullptr;
			}

		public:
			virtual bool IsValid() const noexcept override
			{
				return dataref != nullptr;
			}

		public:
			Iterator(const Iterator& other) noexcept
				: position(other.position), dataref(other.dataref)
			{
				if (dataref)
				{
					dataref->AddIterator(this);
				}
			}

			Iterator(Iterator&& other) noexcept
				: position(other.position), dataref(other.dataref)
			{
				if (dataref)
				{
					dataref->AddIterator(this);
				}
			}

			explicit Iterator(RiffData* const datarefInit, const RiffSize positionInit = 0)
				: position(positionInit), dataref(datarefInit)
			{
				if (dataref)
				{
					dataref->AddIterator(this);
				}
			}

			~Iterator() noexcept
			{
				if (dataref)
				{
					dataref->RemoveIterator(this);
				}
			}

			Byte& operator*() const
			{
				return dataref->GetByte(position);
			}

			Iterator& operator++()
			{
				++position;
				return *this;
			}

			Iterator operator++(int)
			{
				Iterator it(*this);
				++(*this);
				return it;
			}

			Iterator& operator--()
			{
				--position;
				return *this;
			}

			Iterator operator--(int)
			{
				Iterator it(*this);
				--(*this);
				return it;
			}

			Iterator& operator+=(const RiffSize advance)
			{
				position += advance;
				return *this;
			}

			Iterator& operator-=(const RiffSize retreat)
			{
				position -= retreat;
				return *this;
			}

			friend bool operator==(const Iterator& lhs, const Iterator& rhs) noexcept
			{
				return lhs.dataref && lhs.dataref == rhs.dataref && lhs.position == rhs.position;
			}

			friend bool operator!=(const Iterator& lhs, const Iterator& rhs) noexcept
			{
				return !(lhs == rhs);
			}

			friend bool operator<(const Iterator& lhs, const Iterator& rhs) noexcept
			{
				return lhs.dataref && lhs.dataref == rhs.dataref && lhs.position < rhs.position;
			}

			friend bool operator>(const Iterator& lhs, const Iterator& rhs) noexcept
			{
				return lhs.dataref && lhs.dataref == rhs.dataref && lhs.position > rhs.position;
			}

			friend bool operator<=(const Iterator& lhs, const Iterator& rhs) noexcept
			{
				return lhs.dataref && lhs.dataref == rhs.dataref && lhs.position <= rhs.position;
			}

			friend bool operator>=(const Iterator& lhs, const Iterator& rhs) noexcept
			{
				return lhs.dataref && lhs.dataref == rhs.dataref && lhs.position >= rhs.position;
			}

		private:
			RiffSize position;
			mutable RiffData* dataref;
		};

		class ConstIterator : public IteratorBase
		{
		private:
			virtual void Invalidate() const noexcept override
			{
				dataref = nullptr;
			}

		public:
			virtual bool IsValid() const noexcept override
			{
				return dataref != nullptr;
			}

		public:
			ConstIterator(const ConstIterator& other) noexcept
				: position(other.position), dataref(other.dataref)
			{
				if (dataref)
				{
					dataref->AddIterator(this);
				}
			}

			ConstIterator(ConstIterator&& other) noexcept
				: position(other.position), dataref(other.dataref)
			{
				if (dataref)
				{
					dataref->AddIterator(this);
				}
			}

			explicit ConstIterator(const RiffData* const datarefInit, const RiffSize positionInit = 0)
				: position(positionInit), dataref(datarefInit)
			{
				if (dataref)
				{
					dataref->AddIterator(this);
				}
			}

			~ConstIterator() noexcept
			{
				if (dataref)
				{
					dataref->RemoveIterator(this);
				}
			}

			Byte operator*()
			{
				return dataref->GetByte(position);
			}

			ConstIterator& operator++()
			{
				++position;
				return *this;
			}

			ConstIterator operator++(int)
			{
				ConstIterator it(*this);
				++(*this);
				return it;
			}

			ConstIterator& operator--()
			{
				--position;
				return *this;
			}

			ConstIterator operator--(int)
			{
				ConstIterator it(*this);
				--(*this);
				return it;
			}

			ConstIterator& operator+=(const RiffSize advance)
			{
				position += advance;
				return *this;
			}

			ConstIterator& operator-=(const RiffSize retreat)
			{
				position -= retreat;
				return *this;
			}

			friend bool operator==(const ConstIterator& lhs, const ConstIterator& rhs) noexcept
			{
				return lhs.dataref && lhs.dataref == rhs.dataref && lhs.position == rhs.position;
			}

			friend bool operator!=(const ConstIterator& lhs, const ConstIterator& rhs) noexcept
			{
				return !(lhs == rhs);
			}

			friend bool operator<(const ConstIterator& lhs, const ConstIterator& rhs) noexcept
			{
				return lhs.dataref && lhs.dataref == rhs.dataref && lhs.position < rhs.position;
			}

			friend bool operator>(const ConstIterator& lhs, const ConstIterator& rhs) noexcept
			{
				return lhs.dataref && lhs.dataref == rhs.dataref && lhs.position > rhs.position;
			}

			friend bool operator<=(const ConstIterator& lhs, const ConstIterator& rhs) noexcept
			{
				return lhs.dataref && lhs.dataref == rhs.dataref && lhs.position <= rhs.position;
			}

			friend bool operator>=(const ConstIterator& lhs, const ConstIterator& rhs) noexcept
			{
				return lhs.dataref && lhs.dataref == rhs.dataref && lhs.position >= rhs.position;
			}

		private:
			RiffSize position;
			mutable const RiffData* dataref;
		};

	public:
		virtual void Serialize(std::ofstream&) const = 0;
		virtual RiffSize GetSize() const = 0;
		virtual bool IsChunk() const noexcept { return false; }

		virtual ~RiffData() noexcept
		{
			for (const IteratorBase* const it : iterators)
			{
				it->Invalidate();
			}
		}

		Iterator begin()
		{
			Iterator it(this);
			return it;
		}

		ConstIterator begin() const
		{
			ConstIterator cit(this);
			return cit;
		}

		ConstIterator cbegin() const
		{
			ConstIterator cit(this);
			return cit;
		}

		Iterator end()
		{
			Iterator it(this, GetSize());
			return it;
		}

		ConstIterator end() const
		{
			ConstIterator cit(this, GetSize());
			return cit;
		}

		ConstIterator cend() const
		{
			ConstIterator cit(this, GetSize());
			return cit;
		}
	};

	using DataPtr = SharedPtr<RiffData>;
	using ConstDataPtr = SharedPtr<const RiffData>;

	template<typename Ptr> struct RemovePtr;
	template<> struct RemovePtr<DataPtr> { using type = RiffData; };
	template<> struct RemovePtr<ConstDataPtr> { using type = const RiffData; };

	template<typename RiffPtr, typename... Ts>
	inline RiffPtr MakePtr(Ts&&... CtorArgs)
	{
		return MakeShared<typename RemovePtr<RiffPtr>::type>(std::forward<Ts>(CtorArgs)...);
	}
}

