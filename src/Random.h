// Copyright Dan Price 2026.

#pragma once

#include "Memory.h"
#include <random>
#include <utility>
#include <new>
#include <mutex>

namespace json2wav
{
	class Seed
	{
	public:
		Seed() = delete;

		Seed(const Seed& other) noexcept
			: lo(other.lo)
			, hi(other.hi)
		{
			InitSeq();
		}
		Seed(Seed&& other) noexcept
			: lo(other.lo)
			, hi(other.hi)
		{
			InitSeq();
		}

		Seed(const uint64_t loInit, const uint64_t hiInit) noexcept
			: lo(loInit), hi(hiInit)
		{
			InitSeq();
		}
		Seed(const uint32_t lolo, const uint32_t lohi, const uint32_t hilo, const uint32_t hihi) noexcept
			: lo(static_cast<uint64_t>(lolo) | (static_cast<uint64_t>(lohi) << 32))
			, hi(static_cast<uint64_t>(hilo) | (static_cast<uint64_t>(hihi) << 32))
		{
			InitSeq();
		}

		Seed& operator=(const Seed& other) noexcept
		{
			if (&other != this)
			{
				lo = other.lo;
				hi = other.hi;
				SetSeq();
			}
			return *this;
		}
		Seed& operator=(Seed&& other) noexcept
		{
			if (&other != this)
			{
				lo = other.lo;
				hi = other.hi;
				SetSeq();
			}
			return *this;
		}

		~Seed() noexcept
		{
			DestroySeq();
		}

		uint64_t GetLo() const noexcept { return lo; }
		uint64_t GetHi() const noexcept { return hi; }

		void Set(const uint64_t newlo, const uint64_t newhi) noexcept
		{
			lo = newlo;
			hi = newhi;
			SetSeq();
		}

		void Set(const uint32_t lolo, const uint32_t lohi, const uint32_t hilo, const uint32_t hihi) noexcept
		{
			lo = static_cast<uint64_t>(lolo) | (static_cast<uint64_t>(lohi) << 32);
			hi = static_cast<uint64_t>(hilo) | (static_cast<uint64_t>(hihi) << 32);
			SetSeq();
		}

		std::seed_seq& Seq() const noexcept
		{
			return *reinterpret_cast<std::seed_seq*>(mem);
		}

	private:
		void InitSeq() const noexcept
		{
			new(mem) std::seed_seq({
				static_cast<uint32_t>(lo & 0xffffffff),
				static_cast<uint32_t>((lo >> 32) & 0xffffffff),
				static_cast<uint32_t>(hi & 0xffffffff),
				static_cast<uint32_t>((hi >> 32) & 0xffffffff)
			});
		}

		void DestroySeq() const noexcept
		{
			reinterpret_cast<std::seed_seq*>(mem)->std::seed_seq::~seed_seq();
		}

		void SetSeq() const noexcept
		{
			DestroySeq();
			InitSeq();
		}

	private:
		uint64_t lo;
		uint64_t hi;
		alignas(std::seed_seq) mutable unsigned char mem[sizeof(std::seed_seq)];
	};

	inline Seed RandomSeed()
	{
		static std::mutex mtx;
		static std::random_device rd;
		std::scoped_lock lock(mtx);
		Seed seed(rd(), rd(), rd(), rd());
		return seed;
	}

	template<bool b64> struct MT_by_size { using type = std::mt19937; };
	template<> struct MT_by_size<true> { using type = std::mt19937_64; };
	template<typename T> struct MT { using type = typename MT_by_size<sizeof(T) >= 8>::type; };
	template<typename T> using MT_t = typename MT<T>::type;

	template<typename T, template<typename> typename DistType>
	class RNGUnique
	{
	public:
		RNGUnique(const T arg1, const T arg2, const Seed& seedInit) : seed(seedInit), mt(seed.Seq()), dist(arg1, arg2) {}
		RNGUnique(const T arg1, const T arg2, Seed&& seedInit) : seed(std::move(seedInit)), mt(seed.Seq()), dist(arg1, arg2) {}
		RNGUnique(const T arg1, const T arg2) : RNGUnique(arg1, arg2, RandomSeed()) {}

		RNGUnique(const RNGUnique& other) : seed(other.seed), mt(seed.Seq()), dist(other.dist.min(), other.dist.max()) {}
		RNGUnique(RNGUnique&& other) noexcept : seed(std::move(other.seed)), mt(std::move(other.mt)), dist(std::move(other.dist)) {}

		T operator()()
		{
			return dist(mt);
		}

		void SetDist(const T arg1, const T arg2)
		{
			dist.param({ arg1, arg2 });
		}

		const DistType<T>& GetDist() const noexcept
		{
			return dist;
		}

		void SetSeed(const Seed& newSeed)
		{
			seed = newSeed;
			mt.seed(seed.Seq());
		}

		void SetSeed(Seed&& newSeed) noexcept
		{
			seed = std::move(newSeed);
			mt.seed(seed.Seq());
		}

		const Seed& GetSeed() const noexcept
		{
			return seed;
		}

	private:
		Seed seed;
		MT_t<T> mt;
		DistType<T> dist;
	};

	using RNG = RNGUnique<float, std::uniform_real_distribution>;
	using RNG64 = RNGUnique<double, std::uniform_real_distribution>;
	using RNGNorm = RNGUnique<float, std::normal_distribution>;
	using RNGNorm64 = RNGUnique<double, std::normal_distribution>;

	template<typename T>
	class RNGShared
	{
	public:
		explicit RNGShared(const Seed& seedInit) : seed(seedInit), mt(seed.Seq()) {}
		explicit RNGShared(Seed&& seedInit) : seed(std::move(seedInit)), mt(seed.Seq()) {}
		RNGShared(const RNGShared& other) : seed(other.seed), mt(seed.Seq()) {}
		RNGShared(RNGShared&& other) noexcept : seed(std::move(other.seed)), mt(std::move(other.mt)) {}

		RNGShared() = delete;
		RNGShared& operator=(const RNGShared& other) = delete;
		RNGShared& operator=(RNGShared&& other) noexcept = delete;

		void SetSeed(const Seed& newSeed)
		{
			seed = newSeed;
			mt.seed(seed.Seq());
		}

		void SetSeed(Seed&& newSeed) noexcept
		{
			seed = std::move(newSeed);
			mt.seed(seed.Seq());
		}

		const Seed& GetSeed() const noexcept
		{
			return seed;
		}

		template<template<typename> typename DistType>
		//template<typename DistType>
		T operator()(DistType<T>& dist)
		//T operator()(DistType& dist)
		{
			return dist(mt);
		}

	private:
		Seed seed;
		//std:: mt;
		MT_t<T> mt;
	};

	SharedPtr<RNGShared<float>>& getpgrng();
	SharedPtr<RNGShared<double>>& getpgrng64();
	extern RNGShared<float>& grng;
	extern RNGShared<double>& grng64;

	template<typename T>
	class ChooserShared
	{
	public:
		using RNGType = RNGShared<T>;

		ChooserShared()
			: dist(0, 1)
		{
		}

		ChooserShared(const SharedPtr<RNGType>& rInit)
			: r(rInit), dist(0, 1)
		{
		}

		ChooserShared(SharedPtr<RNGType>&& rInit)
			: r(std::move(rInit)), dist(0, 1)
		{
		}

		ChooserShared(const Seed& seedInit)
			: r(MakeShared<RNGType>(seedInit)), dist(0, 1)
		{
		}

		ChooserShared(Seed&& seedInit)
			: r(MakeShared<RNGType>(std::move(seedInit))), dist(0, 1)
		{
		}

		ChooserShared(const SharedPtr<RNGType>& rInit, const Vector<T>& wInit)
			: r(rInit), dist(0, 1), w(wInit)
		{
			RecalcDist();
		}

		ChooserShared(const SharedPtr<RNGType>& rInit, Vector<T>&& wInit)
			: r(rInit), dist(0, 1), w(std::move(wInit))
		{
			RecalcDist();
		}

		ChooserShared(SharedPtr<RNGType>&& rInit, const Vector<T>& wInit)
			: r(std::move(rInit)), dist(0, 1), w(wInit)
		{
			RecalcDist();
		}

		ChooserShared(SharedPtr<RNGType>&& rInit, Vector<T>&& wInit)
			: r(std::move(rInit)), dist(0, 1), w(std::move(wInit))
		{
			RecalcDist();
		}

		ChooserShared(const Seed& seedInit, const Vector<T>& wInit)
			: r(MakeShared<RNGType>(seedInit)), dist(0, 1), w(wInit)
		{
			RecalcDist();
		}

		ChooserShared(const Seed& seedInit, Vector<T>&& wInit)
			: r(MakeShared<RNGType>(seedInit)), dist(0, 1), w(std::move(wInit))
		{
			RecalcDist();
		}

		ChooserShared(Seed&& seedInit, const Vector<T>& wInit)
			: r(MakeShared<RNGType>(std::move(seedInit))), dist(0, 1), w(wInit)
		{
			RecalcDist();
		}

		ChooserShared(Seed&& seedInit, Vector<T>&& wInit)
			: r(MakeShared<RNGType>(std::move(seedInit))), dist(0, 1), w(std::move(wInit))
		{
			RecalcDist();
		}

		ChooserShared(const ChooserShared& other)
			: r(other.r)
			, dist(other.dist.min(), other.dist.max())
			, w(other.w)
		{
		}

		ChooserShared(ChooserShared&& other) noexcept
			: r(std::move(other.r)), dist(std::move(other.dist)), w(std::move(other.w))
		{
		}

		~ChooserShared() noexcept
		{
		}

		ChooserShared& operator=(const ChooserShared& other)
		{
			if (&other != this)
			{
				r = other.r;
				dist.param({ other.dist.min(), other.dist.max() });
				w = other.w;
			}
			return *this;
		}

		ChooserShared& operator=(ChooserShared&& other)
		{
			if (&other != this)
			{
				r = std::move(other.r);
				dist.param({ other.dist.min(), other.dist.max() });
				w = std::move(other.w);
			}
			return *this;
		}

		void SetRNG(SharedPtr<RNGType> rng)
		{
			r = rng;
		}

		SharedPtr<RNGType> GetRNG() const noexcept
		{
			return r;
		}

		void Reserve(const size_t num)
		{
			w.reserve(num);
		}

		void AddWeight(const T weight)
		{
			w.push_back(weight);
			RecalcDist();
		}

		void RemoveWeight()
		{
			w.pop_back();
			RecalcDist();
		}

		void SetWeights(const Vector<T>& wNew)
		{
			w = std::move(wNew);
			RecalcDist();
		}

		void SetWeights(Vector<T>&& wNew)
		{
			w = std::move(wNew);
			RecalcDist();
		}

		class WeightProxy
		{
		public:
			WeightProxy(ChooserShared& chooserInit, const size_t idxInit)
				: chooser(chooserInit), idx(idxInit)
			{
			}

			~WeightProxy() noexcept
			{
				chooser.RecalcDist();
			}

			operator T() const
			{
				return chooser.w[idx];
			}

			T& operator=(const T val)
			{
				chooser.w[idx] = val;
				return chooser.w[idx];
			}

			T& operator+=(const T val)
			{
				chooser.w[idx] += val;
				return chooser.w[idx];
			}

			T& operator-=(const T val)
			{
				chooser.w[idx] -= val;
				return chooser.w[idx];
			}

			T& operator*=(const T val)
			{
				chooser.w[idx] *= val;
				return chooser.w[idx];
			}

			T& operator/=(const T val)
			{
				chooser.w[idx] /= val;
				return chooser.w[idx];
			}

		private:
			ChooserShared& chooser;
			const size_t idx;
		};

		WeightProxy operator[](const size_t idx)
		{
			WeightProxy wp(*this, idx);
			return wp;
		}

		const T& operator[](const size_t idx) const
		{
			return w[idx];
		}

		size_t operator()() const
		{
			if (!r)
				return w.size();

			RNGType& rng(*r);
			T roll(rng(dist));
			for (size_t idx = 0; idx < w.size(); ++idx)
			{
				if (roll < w[idx])
					return idx;
				roll -= w[idx];
			}

			return w.size();
		}

	private:
		void RecalcDist()
		{
			if (w.empty())
				return;

			T sum(0.0f);
			for (const T weight : w)
				sum += weight;
			dist.param({ 0.0f, sum });
		}

	private:
		SharedPtr<RNGType> r;
		std::uniform_real_distribution<T> dist;
		Vector<T> w;
	};

	using Chooser32 = ChooserShared<float>;
	using Chooser64 = ChooserShared<double>;
	using Chooser = Chooser32;
}

