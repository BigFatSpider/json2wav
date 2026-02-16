// Copyright Dan Price 2026.

#pragma once

#include "JsonParser.h"
#include "Presets.h"
#include "IAudioObject.h"
#include "IControlObject.h"
#include "AudioFile.h"
#include "Fader.h"
#include "Panner.h"
#include "Filter.h"
#include "Utility.h"
#include "Delay.h"
#include "DrumHitSynth.h"
#include "DrumHitRT60.h"
#include "AdditiveHitSynth.h"
#include "ChebyDist.h"
#include "PWMageComposable.h"
#include "Compressor.h"
#include "FDNVerb.h"
#include "MSProc.h"
#include "Memory.h"
#include <string>
#include <utility>
#include <iostream>
#include <fstream>
#include <functional>
#include <type_traits>
#include <limits>
#include <unordered_map>
#include <cstdint>
#include <cmath>

namespace json2wav
{
	template<typename HitSynth_t> struct HitSynthModeName
	{ static constexpr const char* const name = "Parts::Part::Instrument::HitSynth"; };
	template<> struct HitSynthModeName<DrumHitSynth>
	{ static constexpr const char* const name = "Parts::Part::Instrument::DrumHit"; };
	template<> struct HitSynthModeName<AdditiveHitSynth>
	{ static constexpr const char* const name = "Parts::Part::Instrument::AdditiveHit"; };

	template<Filter::ETopo eTopo, typename... ParamTypes>
	inline SharedPtr<AudioJoin<>> CreateBesselLPPtr(ControlSet& ctrls, const uint_fast8_t order, ParamTypes&&... params)
	{
		SharedPtr<AudioJoin<>> ptr;
		switch (order)
		{
		case 1: ptr = ctrls.template CreatePtr<Filter::BesselLP<1, false, 2, eTopo>>(std::forward<ParamTypes>(params)...); break;
		case 2: ptr = ctrls.template CreatePtr<Filter::BesselLP<2, false, 2, eTopo>>(std::forward<ParamTypes>(params)...); break;
		case 3: ptr = ctrls.template CreatePtr<Filter::BesselLP<3, false, 2, eTopo>>(std::forward<ParamTypes>(params)...); break;
		case 4: ptr = ctrls.template CreatePtr<Filter::BesselLP<4, false, 2, eTopo>>(std::forward<ParamTypes>(params)...); break;
		case 5: ptr = ctrls.template CreatePtr<Filter::BesselLP<5, false, 2, eTopo>>(std::forward<ParamTypes>(params)...); break;
		case 6: ptr = ctrls.template CreatePtr<Filter::BesselLP<6, false, 2, eTopo>>(std::forward<ParamTypes>(params)...); break;
		case 7: ptr = ctrls.template CreatePtr<Filter::BesselLP<7, false, 2, eTopo>>(std::forward<ParamTypes>(params)...); break;
		case 8: ptr = ctrls.template CreatePtr<Filter::BesselLP<8, false, 2, eTopo>>(std::forward<ParamTypes>(params)...); break;
		case 9: ptr = ctrls.template CreatePtr<Filter::BesselLP<9, false, 2, eTopo>>(std::forward<ParamTypes>(params)...); break;
		case 10: ptr = ctrls.template CreatePtr<Filter::BesselLP<10, false, 2, eTopo>>(std::forward<ParamTypes>(params)...); break;
		default: break;
		}
		return ptr;
	}

	template<bool bLog>
	class JsonInterpreter_t : public Utility::TypeIf_t<bLog, IJsonLogger, IJsonWalker>
	{
	public:
		static void AddEffect(
			SharedPtr<AudioJoin<>> effect,
			SharedPtr<AudioJoin<>> output,
			Vector<SharedPtr<AudioJoin<>>>& fx)
		{
			if (!effect)
				return;

			SharedPtr<AudioJoin<>> back((fx.empty()) ? output : fx.back());
			Vector<SharedPtr<IAudioObject>> inputs;
			inputs.reserve(back->GetInputs().size());
			for (auto input : back->GetInputs())
				if (SharedPtr<IAudioObject> obj = Utility::Lock(input))
					inputs.emplace_back(std::move(obj));
			back->ClearInputs();
			back->AddInput(effect);
			for (auto input : inputs)
				effect->AddInput(input);
			fx.emplace_back(std::move(effect));
		}

	private:
		using JsonInterpreter = JsonInterpreter_t<bLog>;

		struct BusData;

		struct PartData
		{
			class SynthWrapper
			{
			public:
				class Methods
				{
					friend class SynthWrapper;

				private:
					Methods(SynthWrapper& rthisInit) : rthis(rthisInit) {}

				public:
					// Parameter pack by value
					template<typename... EventParamTypes>
					void AddEvent(const size_t samplenum, EventParamTypes... eventParams)
					{
						for (size_t i = 0; i < rthis.vaudio.size(); ++i)
						{
							if (rthis.vdrum[i])
								rthis.vdrum[i]->AddEvent(samplenum, eventParams...);
							else if (rthis.vhit[i])
								rthis.vhit[i]->AddEvent(samplenum, eventParams...);
							else if (rthis.vcomp[i])
								json2wav::AddEvent(rthis.vcomp[i], samplenum, 1.0f, eventParams...);
						}
					}

					float GetRelease() const
					{
						if (rthis.pcomp)
							return rthis.pcomp->GetRelease();
						if (rthis.pdrum)
							return rthis.pdrum->GetRelease();
						if (rthis.phit)
							return rthis.phit->GetRelease();
						return 0.0f;
					}

				private:
					SynthWrapper& rthis;
				};

				SynthWrapper() : idx(0), methods(*this)
				{
				}
				template<typename T> SynthWrapper(SharedPtr<T> ptr)
					: paudio(ptr),
					pcomp(Utility::TypeIf_v<std::is_base_of_v<CompositeSynth, T>, SharedPtr<CompositeSynth>>(ptr, nullptr)),
					pdrum(Utility::TypeIf_v<std::is_base_of_v<DrumHitSynth, T>, SharedPtr<DrumHitSynth>>(ptr, nullptr)),
					phit(Utility::TypeIf_v<std::is_base_of_v<AdditiveHitSynth, T>, SharedPtr<AdditiveHitSynth>>(std::move(ptr), nullptr)),
					idx(0),
					methods(*this)
				{
					vaudio.push_back(paudio);
					vcomp.push_back(pcomp);
					vdrum.push_back(pdrum);
					vhit.push_back(phit);
				}

				template<typename T>
				SynthWrapper& operator=(SharedPtr<T> ptr)
				{
					if (idx >= vaudio.size())
					{
						idx = vaudio.size();
						push_back(std::move(ptr));
						return *this;
					}

					vaudio[idx] = ptr;
					vcomp[idx] = Utility::TypeIf_v<std::is_base_of_v<CompositeSynth, T>, SharedPtr<CompositeSynth>>(ptr, nullptr);
					vdrum[idx] = Utility::TypeIf_v<std::is_base_of_v<DrumHitSynth, T>, SharedPtr<DrumHitSynth>>(ptr, nullptr);
					vhit[idx] = Utility::TypeIf_v<std::is_base_of_v<AdditiveHitSynth, T>, SharedPtr<AdditiveHitSynth>>(std::move(ptr), nullptr);
					UpdatePtrs();
					return *this;
				}

				template<typename T>
				void push_back(SharedPtr<T> ptr)
				{
					paudio = ptr;
					pcomp = Utility::TypeIf_v<std::is_base_of_v<CompositeSynth, T>, SharedPtr<CompositeSynth>>(ptr, nullptr);
					pdrum = Utility::TypeIf_v<std::is_base_of_v<DrumHitSynth, T>, SharedPtr<DrumHitSynth>>(ptr, nullptr);
					phit = Utility::TypeIf_v<std::is_base_of_v<AdditiveHitSynth, T>, SharedPtr<AdditiveHitSynth>>(std::move(ptr), nullptr);

					vaudio.push_back(paudio);
					vcomp.push_back(pcomp);
					vdrum.push_back(pdrum);
					vhit.push_back(phit);
					UpdatePtrs();
				}

				Methods* operator->()
				{
					return &methods;
				}

				const Methods* operator->() const
				{
					return &methods;
				}

				operator SharedPtr<IAudioObject>()
				{
					return paudio;
				}

				operator SharedPtr<CompositeSynth>()
				{
					return pcomp;
				}

				operator SharedPtr<DrumHitSynth>()
				{
					return pdrum;
				}

				operator SharedPtr<AdditiveHitSynth>()
				{
					return phit;
				}

				explicit operator bool() const
				{
					return !!paudio;
				}

				SharedPtr<CompositeSynth>& GetComp()
				{
					return pcomp;
				}

				class Iterator
				{
				public:
					Iterator(SynthWrapper& rwrapperInit) : pwrapper(&rwrapperInit), idx(0) {}
					Iterator(const Iterator& other) : pwrapper(other.pwrapper), idx(other.idx) {}
					Iterator& operator=(const Iterator& other)
					{
						pwrapper = other.pwrapper;
						idx = other.idx;
						return *this;
					}
					Iterator& operator=(const size_t idxAssign)
					{
						idx = idxAssign;
						return *this;
					}

					SynthWrapper& operator*()
					{
						pwrapper->idx = idx;
						pwrapper->UpdatePtrs();
						return *pwrapper;
					}
					SynthWrapper* operator->()
					{
						pwrapper->idx = idx;
						pwrapper->UpdatePtrs();
						return pwrapper;
					}

					Iterator& operator++()
					{
						++idx;
						return *this;
					}
					Iterator operator++(int)
					{
						Iterator it(*this);
						++idx;
						return it;
					}

					Iterator& operator--()
					{
						--idx;
						return *this;
					}
					Iterator operator--(int)
					{
						Iterator it(*this);
						--idx;
						return it;
					}

					Iterator& operator+=(const size_t idxadd)
					{
						idx += idxadd;
						return *this;
					}

					Iterator& operator-=(const size_t idxsub)
					{
						idx -= idxsub;
						return *this;
					}

					friend inline size_t operator+(const Iterator& lhs, const Iterator& rhs)
					{
						return lhs.idx + rhs.idx;
					}

					friend inline size_t operator-(const Iterator& lhs, const Iterator& rhs)
					{
						return lhs.idx - rhs.idx;
					}

					friend inline bool operator==(const Iterator& lhs, const Iterator& rhs)
					{
						return lhs.pwrapper == rhs.pwrapper && lhs.idx == rhs.idx;
					}
					friend inline bool operator!=(const Iterator lhs, const Iterator& rhs)
					{
						return !(lhs == rhs);
					}

				private:
					SynthWrapper* pwrapper;
					size_t idx;
				};

				Iterator begin()
				{
					Iterator it(*this);
					return it;
				}
				Iterator end()
				{
					Iterator it(*this);
					it = vaudio.size();
					return it;
				}

			private:
				void UpdatePtrs()
				{
					paudio = vaudio[idx];
					pcomp = vcomp[idx];
					pdrum = vdrum[idx];
					phit = vhit[idx];
				}

			private:
				SharedPtr<IAudioObject> paudio;
				SharedPtr<CompositeSynth> pcomp;
				SharedPtr<DrumHitSynth> pdrum;
				SharedPtr<AdditiveHitSynth> phit;

				Vector<SharedPtr<IAudioObject>> vaudio;
				Vector<SharedPtr<CompositeSynth>> vcomp;
				Vector<SharedPtr<DrumHitSynth>> vdrum;
				Vector<SharedPtr<AdditiveHitSynth>> vhit;
				size_t idx;

				Methods methods;
			};

			struct NoteEventData
			{
				NoteEventData() : time(0.0f), freq(0.0f), amp(1.0f), dur(0.0f) {}
				float time;
				float freq;
				float amp;
				float dur;
			};

			PartData() : volume(0.0), edoinv(0.0), dur(0.0f), isrhythm(false), noteampsdb(false), ndups(0), transpose(1.0) {}
			Vector<SharedPtr<Fader<>>> outfaders;
			Vector<SharedPtr<BusData>> outputs;
			SharedPtr<BasicMult<>> outmult;
			//SharedPtr<CompositeSynth> instrument; // TODO: Make a wrapper type to deal with non-composite synths
			SynthWrapper instrument;
			double volume;
			Vector<SharedPtr<AudioJoin<>>> fx;
			Vector<SharedPtr<AudioJoin<>>> fx2add;
			Vector<NoteEventData> notes;
			double edoinv;
			float dur;
			bool isrhythm;
			bool noteampsdb;
			size_t ndups;
			double transpose;
		};

		struct BusData
		{
			SharedPtr<Fader<>> volume;
			Vector<SharedPtr<AudioJoin<>>> fx;
			Vector<SharedPtr<BusData>> busses;

			BusData() : volume(MakeShared<Fader<>>()) {}

			void AddInput(SharedPtr<IAudioObject> obj)
			{
				if (!obj)
					return;

				if (fx.empty())
					volume->AddInput(obj);
				else
					fx.back()->AddInput(obj);
			}

			void AddEffect(SharedPtr<AudioJoin<>> effect)
			{
				JsonInterpreter::AddEffect(std::move(effect), volume, fx);
			}

			void AddBus(SharedPtr<BusData> bus)
			{
				if (!bus)
					return;

				AddInput(bus->volume);
				busses.emplace_back(std::move(bus));
			}

			void AddBus()
			{
				AddBus(MakeShared<BusData>());
			}
		};

	public:
		JsonInterpreter_t(const std::string& nameInit = "music")
			: mode(nullptr),
			error(*this), done(*this, &error), top(*this, &done),
			meta(*this, &top), mixer(*this, &top), parts(*this, &top),
			volume(*this), fx(*this), paramNum(*this), paramStr(*this), paramBool(*this), paRamp(*this),
			name(nameInit), beatlen(0.0), key(0.0), samplerate(44100), timelen(0.0f),
			pctrls(MakeUnique<ControlSet>()), ctrls(*pctrls),
			partdatas(vpartdatas),
			mainout(MakeShared<BusData>()),
			currentbus(mainout),
			bIsChild(false)
		{
			mode = &top;
			wav.AddInput(mainout->volume);
		}

	private:
		JsonInterpreter_t(JsonInterpreter_t& parent)
			: mode(nullptr),
			error(*this), done(*this, &error), top(*this, &done),
			meta(*this, &top), mixer(*this, &top), parts(*this, &top),
			volume(*this), fx(*this), paramNum(*this), paramStr(*this), paramBool(*this), paRamp(*this),
			name(parent.name), beatlen(parent.beatlen), key(parent.key),
			samplerate(parent.samplerate), timelen(parent.timelen),
			ctrls(parent.ctrls),
			partdatas(parent.partdatas),
			mainout(parent.mainout),
			currentbus(mainout),
			bIsChild(true)
		{
			mode = &top;
			top.NoOutput();
			meta.ForceVisited();
			mixer.ForceVisited();
		}

	private:
		virtual void OnPushNode(std::string&& nodekey) override { mode->OnPushNode(std::move(nodekey)); }
		virtual void OnPushNode() override { mode->OnPushNode(); }
		virtual void OnNextNode(std::string&& nodekey) override { mode->OnNextNode(std::move(nodekey)); }
		virtual void OnNextNode() override { mode->OnNextNode(); }
		virtual void OnPopNode() override { mode->OnPopNode(); }
		virtual void OnString(std::string&& value) override { mode->OnString(std::move(value)); }
		virtual void OnNumber(double value) override { mode->OnNumber(value); }
		virtual void OnBool(bool value) override { mode->OnBool(value); }
		virtual void OnNull() override { mode->OnNull(); }

	private:
		class InterpreterMode : public IJsonWalker
		{
		public:
			virtual std::string ModeName() const = 0;
			virtual std::string ModeKey() const { return ""; }
			virtual bool CanPreset() const { return false; }
		protected:
			InterpreterMode(JsonInterpreter& rthisInit) : rthis(rthisInit) {}
			JsonInterpreter& rthis;
		};

		void PushMode(InterpreterMode* const nextmode, std::function<void(void*)> callback)
		{
			modestack.push_back(std::make_pair(mode, std::move(callback)));
			mode = nextmode;
		}
		void PopMode(void* const pAnything)
		{
			mode = modestack.back().first;
			modestack.back().second(pAnything);
			modestack.pop_back();
		}

		class Error : public InterpreterMode
		{
		public:
			Error(JsonInterpreter& rthisInit) : InterpreterMode(rthisInit) {}
		private:
			virtual std::string ModeName() const override { return "Error"; }
		};

		class NonErrorMode : public InterpreterMode
		{
		protected:
			NonErrorMode(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
				: InterpreterMode(rthisInit), pup(pupInit)
			{
			}
			void error();
			void error(const std::string& msg)
			{
				error();
				std::cerr << msg << '\n';
			}
			void up();
			InterpreterMode* getup() const { return pup; }
			void InvalidKeyError(std::string&& nodekey)
			{
				error(std::string("Invalid key \"") + std::move(nodekey) + "\" in mode " + this->ModeName());
			}
			void InvalidMapError()
			{
				error(std::string("Invalid map in mode ") + this->ModeName());
			}
			void InvalidArrayError()
			{
				error(std::string("Invalid array in mode ") + this->ModeName());
			}
			void InvalidStringError(std::string&& value)
			{
				error(std::string("Invalid string \"") + std::move(value) + "\" in mode " + this->ModeName());
			}
			void InvalidNumberError(double value)
			{
				error(std::string("Invalid number ") + std::to_string(value) + " in mode " + this->ModeName());
			}
			void InvalidBoolError(bool value)
			{
				error(std::string("Invalid bool ") + ((value) ? "true" : "false") + " in mode " + this->ModeName());
			}
			void InvalidNullError()
			{
				error(std::string("Invalid null in mode ") + this->ModeName());
			}

		private:
			template<typename T> void NoImplError(const std::string& methname, const T& value)
			{
				error(methname + "(" + std::to_string(value) + ") not implemented in mode " + this->ModeName());
			}
			template<> void NoImplError<std::string>(const std::string& methname, const std::string& value)
			{
				error(methname + "(\"" + value + "\") not implemented in mode " + this->ModeName());
			}
			void NoImplError(const std::string& methname)
			{
				error(methname + "() not implemented in mode " + this->ModeName());
			}

		private:
			virtual void OnPushNode(std::string&& nodekey) override { NoImplError("OnPushNode", nodekey); }
			virtual void OnPushNode() override { NoImplError("OnPushNode"); }
			virtual void OnNextNode(std::string&& nodekey) override { NoImplError("OnNextNode", nodekey); }
			virtual void OnNextNode() override { NoImplError("OnNextNode"); }
			virtual void OnPopNode() override { NoImplError("OnPopNode"); }
			virtual void OnString(std::string&& value) override { NoImplError("OnString", value); }
			virtual void OnNumber(double value) override { NoImplError("OnNumber", value); }
			virtual void OnBool(bool value) override { NoImplError("OnBool", value); }
			virtual void OnNull() override { NoImplError("OnNull"); }

		private:
			InterpreterMode* pup;
		};

		class Done : public NonErrorMode
		{
		public:
			Done(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
				: NonErrorMode(rthisInit, pupInit)
			{
			}

		private:
			virtual std::string ModeName() const override { return "Done"; }
		};

		class Top : public NonErrorMode
		{
		public:
			Top(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
				: NonErrorMode(rthisInit, pupInit), bOutput(true)
			{
			}

			void NoOutput()
			{
				bOutput = false;
			}

			void Output()
			{
				bOutput = true;
			}

		private:
			virtual std::string ModeName() const override { return "Top"; }

		private:
			void LogWritingWav() const;

			void OnNode(std::string&& nodekey)
			{
				if (nodekey == "meta")
					this->rthis.mode = &this->rthis.meta;
				else if (nodekey == "mixer")
					this->rthis.mode = &this->rthis.mixer;
				else if (nodekey == "parts")
					if (!this->rthis.meta.Visited())
						this->error("Meta must come before parts");
					else if (!this->rthis.mixer.Visited())
						this->error("Mixer must come before parts");
					else
						this->rthis.mode = &this->rthis.parts;
				else
					this->InvalidKeyError(std::move(nodekey));
			}

		private:
			virtual void OnPushNode(std::string&& nodekey) override { OnNode(std::move(nodekey)); }
			virtual void OnNextNode(std::string&& nodekey) override { OnNode(std::move(nodekey)); }
			virtual void OnPopNode() override
			{
				this->up();
				if (bOutput)
				{
					this->LogWritingWav();
#ifdef ALBUMBOT_DEBUGNEW
					{
						const double maptime = QueryMapTime();
						std::cout << "Spent " << maptime << " seconds in std::map so far\n";
						json2wav::PrintAllocTimes("just before rendering wav");
					}
#endif
					const unsigned long sr = this->rthis.samplerate;
					const unsigned long songlen = (unsigned long)std::ceil(static_cast<float>(sr) * this->rthis.timelen) + sr;
					this->rthis.wav.Write(
						(this->rthis.name + ".wav").c_str(),
						songlen + sampleChunkNum - (songlen % sampleChunkNum),
						this->rthis.samplerate,
						ESampleType::Int16);
#ifdef ALBUMBOT_DEBUGNEW
					{
						json2wav::PrintAllocTimes("just after rendering wav");
						const double maptime = QueryMapTime();
						std::cout << "Spent " << maptime << " seconds in std::map while rendering wav\n";
					}
#endif
				}
			}

		private:
			bool bOutput;
		};

		class Meta : public NonErrorMode
		{
		public:
			Meta(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
				: NonErrorMode(rthisInit, pupInit),
				name(rthisInit, this), tempo(rthisInit, this), key(rthisInit, this),
				bVisited(false)
			{
			}

			bool Visited() const noexcept { return bVisited; }
			void ForceVisited() noexcept { bVisited = true; }
			void ForceUnvisited() noexcept { bVisited = false; }

		private:
			virtual std::string ModeName() const override { return "Meta"; }

		private:
			void OnNode(std::string&& nodekey)
			{
				if (nodekey == "name")
					this->rthis.mode = &name;
				else if (nodekey == "tempo")
					this->rthis.mode = &tempo;
				else if (nodekey == "key")
					this->rthis.mode = &key;
				// Meta can contain anything, so no invalid keys, but tempo and key are required
			}

		private:
			virtual void OnPushNode(std::string&& nodekey) override { OnNode(std::move(nodekey)); }
			virtual void OnNextNode(std::string&& nodekey) override { OnNode(std::move(nodekey)); }
			virtual void OnPopNode() override
			{
				bVisited = true;
				this->up();
				if (this->rthis.name.empty())
					this->rthis.name = "music";
				if (this->rthis.beatlen == 0.0)
					this->error("Must specify a tempo in meta");
				else if (this->rthis.key == 0.0)
					this->error("Must specify a key in meta");
			}

		private:
			class Name : public NonErrorMode
			{
			public:
				Name(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
					: NonErrorMode(rthisInit, pupInit)
				{
				}

			private:
				virtual std::string ModeName() const override { return "Meta::Name"; }

			private:
				virtual void OnString(std::string&& value) override
				{
					this->rthis.name = std::move(value);
					this->up();
				}
			};

			class Tempo : public NonErrorMode
			{
			public:
				Tempo(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
					: NonErrorMode(rthisInit, pupInit)
				{
				}

			private:
				virtual std::string ModeName() const override { return "Meta::Tempo"; }

			private:
				virtual void OnNumber(double value) override
				{
					this->rthis.beatlen = 60.0 / value;
					this->up();
				}
			};

			class Key : public NonErrorMode
			{
			public:
				Key(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
					: NonErrorMode(rthisInit, pupInit)
				{
				}

			private:
				virtual std::string ModeName() const override { return "Meta::Key"; }

			private:
				virtual void OnNumber(double value) override
				{
					this->rthis.key = value;
					this->up();
				}
			};

		private:
			Name name;
			Tempo tempo;
			Key key;

		private:
			bool bVisited;
		};

		class Mixer : public NonErrorMode
		{
		public:
			Mixer(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
				: NonErrorMode(rthisInit, pupInit), bus(rthisInit, *this), busses(rthisInit), bVisited(false)
			{
			}

			bool Visited() const noexcept { return bVisited; }
			void ForceVisited() noexcept { bVisited = true; }
			void ForceUnvisited() noexcept { bVisited = false; }

		private:
			virtual std::string ModeName() const override { return "Mixer"; }

		private:
			virtual void OnPushNode(std::string&& nodekey) override { bus.OnNode(std::move(nodekey)); }
			virtual void OnNextNode(std::string&& nodekey) override { bus.OnNode(std::move(nodekey)); }
			virtual void OnPopNode() override
			{
				bVisited = true;
				this->up();
			}

		private:
			void GotBus() { busses.GotBus(); }
			void CheckBus() { busses.CheckBus(); }

		private:
			class Bus : public NonErrorMode
			{
				friend class Mixer;

			public:
				Bus(JsonInterpreter& rthisInit, Mixer& mInit)
					: NonErrorMode(rthisInit, &rthisInit.error), m(mInit)
				{
				}

			private:
				virtual std::string ModeName() const override { return "Mixer::Bus"; }

			private:
				void OnNode(std::string&& nodekey)
				{
					if (nodekey == "volume")
						this->rthis.PushMode(&this->rthis.volume, [this](void* pvalue)
							{ this->rthis.currentbus->volume->SetGainDB(static_cast<float>(*static_cast<double*>(pvalue))); });
					else if (nodekey == "fx")
					{
						this->rthis.PushMode(&this->rthis.fx, [](void*) {});
						this->rthis.addEffect = [this](SharedPtr<AudioJoin<>> effect)
							{ this->rthis.currentbus->AddEffect(std::move(effect)); };
					}
					else if (nodekey == "busses")
						this->rthis.PushMode(&this->rthis.mixer.busses, [](void*) {});
					else
						this->InvalidKeyError(std::move(nodekey));
				}

			private:
				virtual void OnPushNode(std::string&& nodekey) override
				{
					m.GotBus();
					OnNode(std::move(nodekey));
				}
				virtual void OnNextNode(std::string&& nodekey) override
				{
					OnNode(std::move(nodekey));
				}
				virtual void OnPopNode() override
				{
					this->rthis.PopMode(nullptr);
					m.CheckBus();
				}

			private:
				Mixer& m;
			};

			class Busses : public NonErrorMode
			{
			public:
				Busses(JsonInterpreter& rthisInit)
					: NonErrorMode(rthisInit, &rthisInit.error)
				{
				}

				void GotBus()
				{
					gotBusStack.back() = true;
					AddBus();
				}

				void CheckBus()
				{
					if (!gotBusStack.back())
						OnPopNode();
				}

			private:
				virtual std::string ModeName() const override { return "Mixer::Busses"; }

			private:
				void AddBus()
				{
					this->rthis.currentbus->AddBus();
					this->rthis.currentbus = this->rthis.currentbus->busses.back();
				}

				void OnNode()
				{
					SharedPtr<BusData> parentbus(this->rthis.currentbus);
					const size_t stackIdx(gotBusStack.size() - 1);
					this->rthis.PushMode(&this->rthis.mixer.bus,
						[this, parentbus, stackIdx](void*)
						{
							if (gotBusStack[stackIdx])
								this->rthis.currentbus = parentbus;
						});
				}

			private:
				virtual void OnPushNode() override
				{
					gotBusStack.push_back(false);
					OnNode();
				}
				virtual void OnNextNode() override
				{
					OnNode();
				}
				virtual void OnPopNode() override
				{
					this->rthis.PopMode(nullptr);
					gotBusStack.pop_back();
				}

			private:
				Vector<bool> gotBusStack;
			};

		private:
			Bus bus;
			Busses busses;

		private:
			bool bVisited;
		};

		class Parts : public NonErrorMode
		{
		public:
			Parts(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
				: NonErrorMode(rthisInit, pupInit), part(rthisInit, this)
			{
			}

		private:
			virtual std::string ModeName() const override { return "Parts"; }

		private:
			virtual void OnPushNode(std::string&& nodekey) override
			{
				this->rthis.mode = &part;
				if (this->rthis.partdatas.size() == 0)
				{
					auto emplacepair = partdatamap.emplace(std::make_pair(nodekey, 0));
					if (emplacepair.second)
					{
						this->rthis.partdatas.emplace_back();
						partdatakeys.emplace_back(std::move(nodekey));
					}
					else
						this->InvalidMapError();
				}
				else
					this->InvalidMapError();
			}
			virtual void OnPushNode() override
			{
				this->rthis.mode = &part;
				if (this->rthis.partdatas.size() == 0)
					this->rthis.partdatas.emplace_back();
				// Otherwise a preset
			}
			virtual void OnNextNode(std::string&& nodekey) override
			{
				this->rthis.mode = &part;
				auto emplacepair = partdatamap.emplace(std::make_pair(nodekey, this->rthis.partdatas.size()));
				if (emplacepair.second)
				{
					this->rthis.partdatas.emplace_back();
					partdatakeys.emplace_back(std::move(nodekey));
				}
				else
					this->InvalidMapError();
			}
			virtual void OnNextNode() override
			{
				this->rthis.mode = &part;
				this->rthis.partdatas.emplace_back();
			}
			virtual void OnPopNode() override
			{
				this->up();
			}

		private:
			class Part : public NonErrorMode
			{
			public:
				Part(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
					: NonErrorMode(rthisInit, pupInit),
					instrument(rthisInit, this), outputs(rthisInit, this), notes(rthisInit, this)
				{
				}

			private:
				virtual std::string ModeName() const override { return "Parts::Part"; }

			private:
				void OnNode(std::string&& nodekey)
				{
					if (nodekey == "duplication" || nodekey == "dup")
						this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
							{ this->rthis.partdatas.back().ndups = static_cast<size_t>(*static_cast<double*>(pvalue)); });
					else if (nodekey == "instrument")
						this->rthis.mode = &instrument;
					else if (nodekey == "volume")
						this->rthis.PushMode(&this->rthis.volume, [this](void* pvalue)
							{ this->rthis.partdatas.back().volume = *static_cast<double*>(pvalue); });
					else if (nodekey == "outputs")
						this->rthis.mode = &outputs;
					else if (nodekey == "fx")
					{
						this->rthis.PushMode(&this->rthis.fx, [](void*) {});
						this->rthis.addEffect = [this](SharedPtr<AudioJoin<>> effect)
						{
							this->rthis.partdatas.back().fx2add.emplace_back(std::move(effect));
						};
					}
					else if (nodekey == "notes")
						this->rthis.mode = &notes;
					else
						this->InvalidKeyError(std::move(nodekey));
				}

			private:
				virtual void OnPushNode(std::string&& nodekey) override { OnNode(std::move(nodekey)); }
				virtual void OnNextNode(std::string&& nodekey) override { OnNode(std::move(nodekey)); }
				virtual void OnPopNode() override
				{
					this->up();
					if (this->rthis.bIsChild)
						return;

					const size_t pdidx(this->rthis.partdatas.size() - 1);
					PartData& partdata(this->rthis.partdatas[pdidx]);

					static const double DB_TO_EXP = std::log10(2.0) / 6.0;
					const float partamp = static_cast<float>(std::pow(10.0, DB_TO_EXP * partdata.volume));
					typedef float (*famp_t)(const float);
					famp_t famp = (partdata.noteampsdb)
						? static_cast<famp_t>([](const float db)
							{
								static const float DB_TO_EXPF = std::log10f(2.0f) / 6.0f;
								return std::powf(10.0f, DB_TO_EXPF * db);
							})
						: static_cast<famp_t>([](const float amp) { return amp; });

					if (partdata.outputs.size() != partdata.outfaders.size())
					{
						this->error(std::string("Numbers of faders and outputs do not match for part ")
							+ std::to_string(this->rthis.partdatas.size() - 1));
						return;
					}

					if (partdata.outputs.size() == 0)
					{
						this->error(std::string("BUG: No output specified and not automatically routed to main out for part ")
							+ std::to_string(this->rthis.partdatas.size() - 1));
						return;
					}

					SharedPtr<AudioJoin<>> outnode = partdata.outfaders[0];
					if (partdata.outputs.size() > 1)
					{
						partdata.outmult = MakeShared<BasicMult<>>();
						outnode = partdata.outmult;
						for (SharedPtr<Fader<>> pfader : partdata.outfaders)
						{
							if (!pfader)
							{
								this->error(std::string("Null fader pointer for part ")
									+ std::to_string(this->rthis.partdatas.size() - 1));
								return;
							}
							pfader->AddInput(outnode);
						}
					}

					for (SharedPtr<AudioJoin<>>& effect : partdata.fx2add)
						JsonInterpreter::AddEffect(std::move(effect), outnode, partdata.fx);
					partdata.fx2add.clear();

					for (size_t outidx = 0, numouts = partdata.outputs.size(); outidx < numouts; ++outidx)
					{
						auto poutput = partdata.outputs[outidx];
						auto pfader = partdata.outfaders[outidx];
						if (!poutput)
						{
							this->error(std::string("Null output pointer for part ")
								+ std::to_string(this->rthis.partdatas.size() - 1));
							return;
						}

						if (!pfader)
						{
							this->error(std::string("Null fader pointer for part ")
								+ std::to_string(this->rthis.partdatas.size() - 1));
							return;
						}

						poutput->AddInput(pfader);
					}

					for (auto& synth : partdata.instrument)
					{
						if (partdata.fx.size() > 0)
							partdata.fx.back()->AddInput(synth);
						else
							outnode->AddInput(synth);

						static const float ampthresh = 0.0001f; // -80 dB
						float endtime(0.0f);
						if (partdata.isrhythm)
						{
							for (const typename PartData::NoteEventData& note : partdata.notes)
							{
								if (note.amp == -std::numeric_limits<float>::infinity())
									continue;
								const float noteamp = partamp*famp(note.amp);
								if (noteamp <= ampthresh)
									continue;
								synth->AddEvent(note.time * this->rthis.samplerate, noteamp, partdata.dur);
							}
						}
						else
						{
							for (const typename PartData::NoteEventData& note : partdata.notes)
							{
								const float noteamp = partamp*famp(note.amp);
								if (noteamp <= ampthresh)
									continue;
								const float noteend(note.time + note.dur);
								if (noteend > endtime)
									endtime = noteend;
								AddEvent(synth.GetComp(), note.time, note.freq, noteamp, note.dur);
							}
						}

						if (const float partend = endtime + synth->GetRelease(); partend > this->rthis.timelen)
							this->rthis.timelen = partend;
					}
				}

			private:
				class MixerPath : public NonErrorMode
				{
				public:
					MixerPath(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
						: NonErrorMode(rthisInit, pupInit), pathidx(0)
					{
					}

				private:
					virtual std::string ModeName() const override { return "Parts::Part::MixerPath"; }

				private:
					virtual void OnPopNode(const SharedPtr<BusData>& poutput) = 0;

				private:
					virtual void OnPushNode() override
					{
						pathidx = 0;
						poutput = nullptr;
					}
					virtual void OnNextNode() override { ++pathidx; }
					virtual void OnPopNode() override
					{
						this->up();
						if (!poutput)
							poutput = this->rthis.mainout;
						OnPopNode(poutput);
						poutput = nullptr;
					}
					virtual void OnString(std::string&& value) override
					{
						if (pathidx > 0)
						{
							this->error("Bus path nodes after the first must be numbers");
							return;
						}

						if (value == "mixer")
							poutput = this->rthis.mainout;
						else
							this->InvalidStringError(std::move(value));
					}
					virtual void OnNumber(double value) override
					{
						if (pathidx == 0)
						{
							this->error("First bus path node must be a string");
							return;
						}

						if (value < 0.0 || ((value - std::floor(value)) != 0.0))
						{
							this->error("Bus path indices must be nonnegative integers");
							return;
						}

						if (!poutput)
						{
							this->error("Output pointer is null");
							return;
						}

						const size_t busidx = static_cast<size_t>(value);
						if (busidx >= poutput->busses.size())
						{
							this->error(std::string("Invalid output path index: ") + std::to_string(busidx));
							return;
						}

						poutput = poutput->busses[busidx];
					}

				private:
					size_t pathidx;
					SharedPtr<BusData> poutput;
				};

				class Instrument : public NonErrorMode
				{
				public:
					Instrument(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
						: NonErrorMode(rthisInit, pupInit),
						filteredsaw(rthisInit, this), noisehit(rthisInit, this),
						drumhit(rthisInit, this), additivehit(rthisInit, this)
					{
					}

				private:
					virtual std::string ModeName() const override { return "Parts::Part::Instrument"; }

				private:
					virtual void OnPushNode(std::string&& nodekey) override
					{
						if (this->rthis.partdatas.back().instrument)
							this->error("Multiple instruments specified in part");
						else if (nodekey == filteredsaw.ModeKey())
							this->rthis.mode = &filteredsaw;
						else if (nodekey == noisehit.ModeKey())
							this->rthis.mode = &noisehit;
						else if (nodekey == drumhit.ModeKey())
						{
							this->rthis.mode = &drumhit;
							this->rthis.partdatas.back().isrhythm = true;
						}
						else if (nodekey == additivehit.ModeKey())
						{
							this->rthis.mode = &additivehit;
							this->rthis.partdatas.back().isrhythm = true;
						}
						else
							this->InvalidKeyError(std::move(nodekey));
					}
					virtual void OnPopNode() override
					{
						this->up();
					}
					virtual void OnString(std::string&& value) override
					{
						this->up();
						if (this->rthis.partdatas.back().instrument)
							this->error("Multiple instruments specified in part");
						else if (value == "fatsaw0")
							for (size_t i = 0, n = this->rthis.partdatas.back().ndups + 1; i < n; ++i)
								this->rthis.partdatas.back().instrument.push_back(CreateFatSaw0(this->rthis.ctrls));
						else if (value == "fatsaw1")
							for (size_t i = 0, n = this->rthis.partdatas.back().ndups + 1; i < n; ++i)
								this->rthis.partdatas.back().instrument.push_back(CreateFatSaw1(this->rthis.ctrls));
						else if (value == "solidsaw0")
							for (size_t i = 0, n = this->rthis.partdatas.back().ndups + 1; i < n; ++i)
								this->rthis.partdatas.back().instrument.push_back(CreateSolidSaw0(this->rthis.ctrls));
						else if (value == "solidsaw1")
							for (size_t i = 0, n = this->rthis.partdatas.back().ndups + 1; i < n; ++i)
								this->rthis.partdatas.back().instrument.push_back(CreateSolidSaw1(this->rthis.ctrls));
						else
							this->InvalidStringError(std::move(value));
					}

				private:
					template<bool bSaw>
					class FilteredSynth : public NonErrorMode
					{
					public:
						FilteredSynth(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
							: NonErrorMode(rthisInit, pupInit)
						{
						}

						virtual std::string ModeKey() const override
						{
							if constexpr (bSaw)
								return "filteredsaw";
							return "noisehit";
						}

					private:
						virtual std::string ModeName() const override
						{
							return "Parts::Part::Instrument::FilteredSynth";
						}

						void Reset()
						{
							preset = "";

							eTopo = Filter::ETopo::TDF2;
							unison = 1;
							freqSpread = 0.0f;
							phaseSpread = 0.0;
							panSpread = 0.0f;
							noiseAmp = 0.0f;

							ampHiAtt = 0.05f;
							ampHiDec = 0.2f;
							ampHiRel = 0.1f;
							ampHiAttLev = 0.7f;
							ampHiSusLev = 0.5f;
							ampHiAttRamp = ERampShape::Linear;
							ampHiDecRamp = ERampShape::Linear;
							ampHiRelRamp = ERampShape::Linear;

							ampLoAtt = 0.05f;
							ampLoDec = 0.2f;
							ampLoRel = 0.1f;
							ampLoAttLev = 0.6f;
							ampLoSusLev = 0.4f;
							ampLoAttRamp = ERampShape::Linear;
							ampLoDecRamp = ERampShape::Linear;
							ampLoRelRamp = ERampShape::Linear;

							ampExpression = 0.0f;

							filtAtt = 0.05f;
							filtDec = 0.2f;
							filtRel = 0.1f;
							filtAttFreq = 10000.0f;
							filtSusFreq = 5000.0f;
							filtRestFreq = 500.0f;
							filtAttRamp = ERampShape::LogScaleLinear;
							filtDecRamp = ERampShape::LogScaleLinear;
							filtRelRamp = ERampShape::LogScaleLinear;

							filtExpression = 0.0f;

							sawType = "infinisaw";
						}

						void OnNode(std::string&& nodekey)
						{
							if (nodekey == "preset")
								this->rthis.PushMode(&this->rthis.paramStr, [this](void* pvalue)
									{ preset = *static_cast<std::string*>(pvalue); });
							else if (nodekey == "topo")
								this->rthis.PushMode(&this->rthis.paramStr, [this](void* pvalue)
									{
										const std::string& topo(*static_cast<std::string*>(pvalue));
										if (topo == "df2")
											eTopo = Filter::ETopo::DF2;
										else if (topo == "tdf2")
											eTopo = Filter::ETopo::TDF2;
										else
											this->error("Invalid filter topology");
									});
							else if (nodekey == "unison")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{
										const double u(*static_cast<double*>(pvalue));
										unison = (u > 1.0) ? static_cast<size_t>(std::round(u)) : 1;
									});
							else if (nodekey == "freqspread")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ freqSpread = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "phasespread")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ phaseSpread = *static_cast<double*>(pvalue); });
							else if (nodekey == "panspread")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ panSpread = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "noiseamp")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ noiseAmp = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "noisedb")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ noiseAmp = static_cast<float>(Utility::DBToGain(*static_cast<double*>(pvalue))); });
							// "amp" = center amplitude envelope
							else if (nodekey == "ampattack")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ ampHiAtt = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "ampdecay")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ ampHiDec = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "amprelease")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ ampHiRel = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "ampattlevel")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ ampHiAttLev = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "ampattleveldb")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ ampHiAttLev = static_cast<float>(Utility::DBToGain(*static_cast<double*>(pvalue))); });
							else if (nodekey == "ampsuslevel")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ ampHiSusLev = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "ampsusleveldb")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ ampHiSusLev = static_cast<float>(Utility::DBToGain(*static_cast<double*>(pvalue))); });
							else if (nodekey == "ampattshape")
								this->rthis.PushMode(&this->rthis.paRamp, [this](void* pvalue)
									{ ampHiAttRamp = *static_cast<ERampShape*>(pvalue); });
							else if (nodekey == "ampdecshape")
								this->rthis.PushMode(&this->rthis.paRamp, [this](void* pvalue)
									{ ampHiDecRamp = *static_cast<ERampShape*>(pvalue); });
							else if (nodekey == "amprelshape")
								this->rthis.PushMode(&this->rthis.paRamp, [this](void* pvalue)
									{ ampHiRelRamp = *static_cast<ERampShape*>(pvalue); });
							// "flamp" = flanking amplitude = amplitude envelope for flanking unison voices
							else if (nodekey == "flampattack")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ ampLoAtt = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "flampdecay")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ ampLoDec = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "flamprelease")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ ampLoRel = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "flampattlevel")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ ampLoAttLev = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "flampattleveldb")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ ampLoAttLev = static_cast<float>(Utility::DBToGain(*static_cast<double*>(pvalue))); });
							else if (nodekey == "flampsuslevel")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ ampLoSusLev = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "flampsusleveldb")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ ampLoSusLev = static_cast<float>(Utility::DBToGain(*static_cast<double*>(pvalue))); });
							else if (nodekey == "flampattshape")
								this->rthis.PushMode(&this->rthis.paRamp, [this](void* pvalue)
									{ ampLoAttRamp = *static_cast<ERampShape*>(pvalue); });
							else if (nodekey == "flampdecshape")
								this->rthis.PushMode(&this->rthis.paRamp, [this](void* pvalue)
									{ ampLoDecRamp = *static_cast<ERampShape*>(pvalue); });
							else if (nodekey == "flamprelshape")
								this->rthis.PushMode(&this->rthis.paRamp, [this](void* pvalue)
									{ ampLoRelRamp = *static_cast<ERampShape*>(pvalue); });
							// "filt" = filter envelope
							else if (nodekey == "filtattack")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ filtAtt = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "filtdecay")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ filtDec = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "filtrelease")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ filtRel = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "filtattfreq")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ filtAttFreq = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "filtsusfreq")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ filtSusFreq = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "filtrestfreq")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ filtRestFreq = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "filtattshape")
								this->rthis.PushMode(&this->rthis.paRamp, [this](void* pvalue)
									{ filtAttRamp = *static_cast<ERampShape*>(pvalue); });
							else if (nodekey == "filtdecshape")
								this->rthis.PushMode(&this->rthis.paRamp, [this](void* pvalue)
									{ filtDecRamp = *static_cast<ERampShape*>(pvalue); });
							else if (nodekey == "filtrelshape")
								this->rthis.PushMode(&this->rthis.paRamp, [this](void* pvalue)
									{ filtRelRamp = *static_cast<ERampShape*>(pvalue); });
							else if (nodekey == "sawtype" && bSaw)
								this->rthis.PushMode(&this->rthis.paramStr, [this](void* pvalue)
									{ sawType = std::move(*static_cast<std::string*>(pvalue)); });
							else if (nodekey == "ampexpression" || nodekey == "ampexpress" || nodekey == "ampexp")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ ampExpression = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "filtexpression" || nodekey == "filtexpress" || nodekey == "filtexp")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ filtExpression = static_cast<float>(*static_cast<double*>(pvalue)); });
							else
								this->InvalidKeyError(std::move(nodekey));
						}

					private:
						virtual void OnPushNode(std::string&& nodekey) override
						{
							Reset();
							OnNode(std::move(nodekey));
						}
						virtual void OnNextNode(std::string&& nodekey) override
						{
							OnNode(std::move(nodekey));
						}
						virtual void OnPopNode() override
						{
							this->up();

							if (preset != "")
							{
								bool bError = true;
								std::ifstream file(std::string("./presets/") + preset + ".json");
								if (file)
								{
									JsonInterpreter presetReader(this->rthis);
									JsonParser p;
									bError = !p.parse(file, presetReader);
								}

								if (bError)
									this->error("Invalid preset");
								return; // Preset will add synths in child interpreter
							}

							constexpr const unsigned int uSawType_Invalid = 0;
							constexpr const unsigned int uSawType_InfiniSaw = 1;
							constexpr const unsigned int uSawType_PWMage1 = 4 | 1;
							constexpr const unsigned int uSawType_PWMage2 = 4 | 2;
							constexpr const unsigned int uSawType_PWMage3 = 4 | 3;

							unsigned int uSawType = uSawType_Invalid;
							if (sawType == "infinisaw")
								uSawType = uSawType_InfiniSaw;
							else if (sawType == "pwmage")
								uSawType = uSawType_PWMage3;
							else if (sawType == "pwmage1")
								uSawType = uSawType_PWMage1;
							else if (sawType == "pwmage2")
								uSawType = uSawType_PWMage2;
							else if (sawType == "pwmage3")
								uSawType = uSawType_PWMage3;

							if (uSawType == uSawType_Invalid)
							{
								this->error("Invalid saw type");
								return;
							}
							
							const size_t pdidx(this->rthis.partdatas.size() - 1);
							for (size_t i = 0, n = this->rthis.partdatas[pdidx].ndups + 1; i < n; ++i)
							{
								switch (uSawType)
								{
								default:
								case uSawType_Invalid: this->error("Invalid saw type"); break;
								case uSawType_InfiniSaw:
								{
									if (eTopo == Filter::ETopo::TDF2)
									{
										SharedPtr<LadderLPComposable<false, 2, Filter::ETopo::TDF2>> filt;
										this->rthis.partdatas[pdidx].instrument.push_back(CreateFilteredSaw<Filter::ETopo::TDF2, bSaw, InfiniSawComposable>(
											this->rthis.ctrls,
											unison,
											freqSpread, phaseSpread, panSpread,
											noiseAmp,
											Envelope(
												ampHiAtt, ampHiDec, ampHiRel,
												ampHiAttLev, ampHiSusLev,
												ampHiAttRamp, ampHiDecRamp, ampHiRelRamp, ampExpression),
											Envelope(
												ampLoAtt, ampLoDec, ampLoRel,
												ampLoAttLev, ampLoSusLev,
												ampLoAttRamp, ampLoDecRamp, ampLoRelRamp, ampExpression),
											Envelope(
												filtAtt, filtDec, filtRel,
												filtAttFreq, filtSusFreq,
												filtAttRamp, filtDecRamp, filtRelRamp, filtExpression),
											nullptr, nullptr, nullptr,
											&filt));
										filt->SetResetVal(filtRestFreq);
									}
									else if (eTopo == Filter::ETopo::DF2)
									{
										SharedPtr<LadderLPComposable<false, 2, Filter::ETopo::DF2>> filt;
										this->rthis.partdatas[pdidx].instrument.push_back(CreateFilteredSaw<Filter::ETopo::DF2, bSaw, InfiniSawComposable>(
											this->rthis.ctrls,
											unison,
											freqSpread, phaseSpread, panSpread,
											noiseAmp,
											Envelope(
												ampHiAtt, ampHiDec, ampHiRel,
												ampHiAttLev, ampHiSusLev,
												ampHiAttRamp, ampHiDecRamp, ampHiRelRamp, ampExpression),
											Envelope(
												ampLoAtt, ampLoDec, ampLoRel,
												ampLoAttLev, ampLoSusLev,
												ampLoAttRamp, ampLoDecRamp, ampLoRelRamp, ampExpression),
											Envelope(
												filtAtt, filtDec, filtRel,
												filtAttFreq, filtSusFreq,
												filtAttRamp, filtDecRamp, filtRelRamp, filtExpression),
											nullptr, nullptr, nullptr,
											&filt));
										filt->SetResetVal(filtRestFreq);
									}
									else
										this->error("Invalid filter topology");
								} break;
								case uSawType_PWMage1:
								{
									if (eTopo == Filter::ETopo::TDF2)
									{
										SharedPtr<LadderLPComposable<false, 2, Filter::ETopo::TDF2>> filt;
										this->rthis.partdatas[pdidx].instrument.push_back(CreateFilteredSaw<Filter::ETopo::TDF2, bSaw, PWMageComposable<1>>(
											this->rthis.ctrls,
											unison,
											freqSpread, phaseSpread, panSpread,
											noiseAmp,
											Envelope(
												ampHiAtt, ampHiDec, ampHiRel,
												ampHiAttLev, ampHiSusLev,
												ampHiAttRamp, ampHiDecRamp, ampHiRelRamp, ampExpression),
											Envelope(
												ampLoAtt, ampLoDec, ampLoRel,
												ampLoAttLev, ampLoSusLev,
												ampLoAttRamp, ampLoDecRamp, ampLoRelRamp, ampExpression),
											Envelope(
												filtAtt, filtDec, filtRel,
												filtAttFreq, filtSusFreq,
												filtAttRamp, filtDecRamp, filtRelRamp, filtExpression),
											nullptr, nullptr, nullptr,
											&filt));
										filt->SetResetVal(filtRestFreq);
									}
									else if (eTopo == Filter::ETopo::DF2)
									{
										SharedPtr<LadderLPComposable<false, 2, Filter::ETopo::DF2>> filt;
										this->rthis.partdatas[pdidx].instrument.push_back(CreateFilteredSaw<Filter::ETopo::DF2, bSaw, PWMageComposable<1>>(
											this->rthis.ctrls,
											unison,
											freqSpread, phaseSpread, panSpread,
											noiseAmp,
											Envelope(
												ampHiAtt, ampHiDec, ampHiRel,
												ampHiAttLev, ampHiSusLev,
												ampHiAttRamp, ampHiDecRamp, ampHiRelRamp, ampExpression),
											Envelope(
												ampLoAtt, ampLoDec, ampLoRel,
												ampLoAttLev, ampLoSusLev,
												ampLoAttRamp, ampLoDecRamp, ampLoRelRamp, ampExpression),
											Envelope(
												filtAtt, filtDec, filtRel,
												filtAttFreq, filtSusFreq,
												filtAttRamp, filtDecRamp, filtRelRamp, filtExpression),
											nullptr, nullptr, nullptr,
											&filt));
										filt->SetResetVal(filtRestFreq);
									}
									else
										this->error("Invalid filter topology");
								} break;
								case uSawType_PWMage2:
								{
									if (eTopo == Filter::ETopo::TDF2)
									{
										SharedPtr<LadderLPComposable<false, 2, Filter::ETopo::TDF2>> filt;
										this->rthis.partdatas[pdidx].instrument.push_back(CreateFilteredSaw<Filter::ETopo::TDF2, bSaw, PWMageComposable<2>>(
											this->rthis.ctrls,
											unison,
											freqSpread, phaseSpread, panSpread,
											noiseAmp,
											Envelope(
												ampHiAtt, ampHiDec, ampHiRel,
												ampHiAttLev, ampHiSusLev,
												ampHiAttRamp, ampHiDecRamp, ampHiRelRamp, ampExpression),
											Envelope(
												ampLoAtt, ampLoDec, ampLoRel,
												ampLoAttLev, ampLoSusLev,
												ampLoAttRamp, ampLoDecRamp, ampLoRelRamp, ampExpression),
											Envelope(
												filtAtt, filtDec, filtRel,
												filtAttFreq, filtSusFreq,
												filtAttRamp, filtDecRamp, filtRelRamp, filtExpression),
											nullptr, nullptr, nullptr,
											&filt));
										filt->SetResetVal(filtRestFreq);
									}
									else if (eTopo == Filter::ETopo::DF2)
									{
										SharedPtr<LadderLPComposable<false, 2, Filter::ETopo::DF2>> filt;
										this->rthis.partdatas[pdidx].instrument.push_back(CreateFilteredSaw<Filter::ETopo::DF2, bSaw, PWMageComposable<2>>(
											this->rthis.ctrls,
											unison,
											freqSpread, phaseSpread, panSpread,
											noiseAmp,
											Envelope(
												ampHiAtt, ampHiDec, ampHiRel,
												ampHiAttLev, ampHiSusLev,
												ampHiAttRamp, ampHiDecRamp, ampHiRelRamp, ampExpression),
											Envelope(
												ampLoAtt, ampLoDec, ampLoRel,
												ampLoAttLev, ampLoSusLev,
												ampLoAttRamp, ampLoDecRamp, ampLoRelRamp, ampExpression),
											Envelope(
												filtAtt, filtDec, filtRel,
												filtAttFreq, filtSusFreq,
												filtAttRamp, filtDecRamp, filtRelRamp, filtExpression),
											nullptr, nullptr, nullptr,
											&filt));
										filt->SetResetVal(filtRestFreq);
									}
									else
										this->error("Invalid filter topology");
								} break;
								case uSawType_PWMage3:
								{
									if (eTopo == Filter::ETopo::TDF2)
									{
										SharedPtr<LadderLPComposable<false, 2, Filter::ETopo::TDF2>> filt;
										this->rthis.partdatas[pdidx].instrument.push_back(CreateFilteredSaw<Filter::ETopo::TDF2, bSaw, PWMageComposable<3>>(
											this->rthis.ctrls,
											unison,
											freqSpread, phaseSpread, panSpread,
											noiseAmp,
											Envelope(
												ampHiAtt, ampHiDec, ampHiRel,
												ampHiAttLev, ampHiSusLev,
												ampHiAttRamp, ampHiDecRamp, ampHiRelRamp, ampExpression),
											Envelope(
												ampLoAtt, ampLoDec, ampLoRel,
												ampLoAttLev, ampLoSusLev,
												ampLoAttRamp, ampLoDecRamp, ampLoRelRamp, ampExpression),
											Envelope(
												filtAtt, filtDec, filtRel,
												filtAttFreq, filtSusFreq,
												filtAttRamp, filtDecRamp, filtRelRamp, filtExpression),
											nullptr, nullptr, nullptr,
											&filt));
										filt->SetResetVal(filtRestFreq);
									}
									else if (eTopo == Filter::ETopo::DF2)
									{
										SharedPtr<LadderLPComposable<false, 2, Filter::ETopo::DF2>> filt;
										this->rthis.partdatas[pdidx].instrument.push_back(CreateFilteredSaw<Filter::ETopo::DF2, bSaw, PWMageComposable<3>>(
											this->rthis.ctrls,
											unison,
											freqSpread, phaseSpread, panSpread,
											noiseAmp,
											Envelope(
												ampHiAtt, ampHiDec, ampHiRel,
												ampHiAttLev, ampHiSusLev,
												ampHiAttRamp, ampHiDecRamp, ampHiRelRamp, ampExpression),
											Envelope(
												ampLoAtt, ampLoDec, ampLoRel,
												ampLoAttLev, ampLoSusLev,
												ampLoAttRamp, ampLoDecRamp, ampLoRelRamp, ampExpression),
											Envelope(
												filtAtt, filtDec, filtRel,
												filtAttFreq, filtSusFreq,
												filtAttRamp, filtDecRamp, filtRelRamp, filtExpression),
											nullptr, nullptr, nullptr,
											&filt));
										filt->SetResetVal(filtRestFreq);
									}
									else
										this->error("Invalid filter topology");
								} break;
								}
							}
						}

					private:
						std::string preset;

						Filter::ETopo eTopo;
						size_t unison;
						float freqSpread;
						double phaseSpread;
						float panSpread;
						float noiseAmp;

						float ampHiAtt;
						float ampHiDec;
						float ampHiRel;
						float ampHiAttLev;
						float ampHiSusLev;
						ERampShape ampHiAttRamp;
						ERampShape ampHiDecRamp;
						ERampShape ampHiRelRamp;

						float ampLoAtt;
						float ampLoDec;
						float ampLoRel;
						float ampLoAttLev;
						float ampLoSusLev;
						ERampShape ampLoAttRamp;
						ERampShape ampLoDecRamp;
						ERampShape ampLoRelRamp;

						float ampExpression;

						float filtAtt;
						float filtDec;
						float filtRel;
						float filtAttFreq;
						float filtSusFreq;
						float filtRestFreq;
						ERampShape filtAttRamp;
						ERampShape filtDecRamp;
						ERampShape filtRelRamp;

						float filtExpression;

						std::string sawType;

					};

					template<typename HitSynth_t>
					class HitSynth : public NonErrorMode
					{
					public:
						HitSynth(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
							: NonErrorMode(rthisInit, pupInit)
						{
							Reset(false);
						}

					private:
						virtual std::string ModeName() const override
						{
							return HitSynthModeName<HitSynth_t>::name;
						}

					private:
						virtual SharedPtr<HitSynth_t> CreateSynth() = 0;
						virtual void OnReset() {}
						virtual bool HandleOnNode(std::string&& nodekey) { return false; }

					private:
						void Reset(const bool bCallVirtualFuncs = true)
						{
							preset = "";

							freq = 100.0f;
							if (bCallVirtualFuncs)
								OnReset();

							strenToAmp = 0.25f;
							transientTime = 0.00025;
							transientShape = ERampShape::SCurve;
							decayDelay = 0.1;
							decayAmount = 0.001f;
							decayTime = 2.0;
							decayShape = ERampShape::LogScaleLinear;
							detuneDelay = 0.00075;
							detuneAmount = 0.9f;
							detuneTime = 1.0;
							detuneShape = ERampShape::LogScaleLinear;
							filt0freq = 8000.0f;
							filt0res = 0.5f;
							filt1freq = 2500.0f;
							filt1res = 0.5f;
							filt2freq = 800.0f;
							filt2res = 0.7f;
							filt3freq = 0.0f; // 0.0f indicates use freq
							filt3res = 0.7f;
							env0att = 0.00125f;
							env0dec = 0.0125f;
							env0rel = 0.0625f;
							env0attlev = 48.0f;
							env0suslev = 36.0f;
							env0attshape = ERampShape::SCurve;
							env0decshape = ERampShape::Linear;
							env0relshape = ERampShape::Linear;
							env1att = 0.001875f;
							env1dec = 0.01875f;
							env1rel = 0.09375f;
							env1attlev = 24.0f;
							env1suslev = 18.0f;
							env1attshape = ERampShape::SCurve;
							env1decshape = ERampShape::Linear;
							env1relshape = ERampShape::Linear;
							env2att = 0.00375f;
							env2dec = 0.0375f;
							env2rel = 0.1875f;
							env2attlev = 9.0f;
							env2suslev = 6.0f;
							env2attshape = ERampShape::SCurve;
							env2decshape = ERampShape::Linear;
							env2relshape = ERampShape::Linear;
							env3att = 0.005f;
							env3dec = 0.05f;
							env3rel = 0.25f;
							env3attlev = 9.0f;
							env3suslev = 6.0f;
							env3attshape = ERampShape::SCurve;
							env3decshape = ERampShape::Linear;
							env3relshape = ERampShape::Linear;
							filt0del = 0.0f;
							filt1del = 0.0f;
							filt2del = 0.0f;
							filt3del = 0.005f;

						}

						void OnNode(std::string&& nodekey)
						{
							if (HandleOnNode(std::move(nodekey)))
								return;

							if (nodekey == "preset")
								this->rthis.PushMode(&this->rthis.paramStr, [this](void* pvalue)
									{ preset = *static_cast<std::string*>(pvalue); });
							else if (nodekey == "freq")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ freq = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "stren2amp")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ strenToAmp = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "transient_time")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ transientTime = *static_cast<double*>(pvalue); });
							else if (nodekey == "transient_shape")
								this->rthis.PushMode(&this->rthis.paRamp, [this](void* pvalue)
									{ transientShape = *static_cast<ERampShape*>(pvalue); });
							else if (nodekey == "decay_delay")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ decayDelay = *static_cast<double*>(pvalue); });
							else if (nodekey == "decay_amt")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ decayAmount = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "decay_time")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ decayTime = *static_cast<double*>(pvalue); });
							else if (nodekey == "decay_shape")
								this->rthis.PushMode(&this->rthis.paRamp, [this](void* pvalue)
									{ decayShape = *static_cast<ERampShape*>(pvalue); });
							else if (nodekey == "detune_delay")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ detuneDelay = *static_cast<double*>(pvalue); });
							else if (nodekey == "detune_amt")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ detuneAmount = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "detune_time")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ detuneTime = *static_cast<double*>(pvalue); });
							else if (nodekey == "detune_shape")
								this->rthis.PushMode(&this->rthis.paRamp, [this](void* pvalue)
									{ detuneShape = *static_cast<ERampShape*>(pvalue); });
							else if (nodekey == "filt0freq")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ filt0freq = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "filt0res")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ filt0res = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "filt1freq")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ filt1freq = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "filt1res")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ filt1res = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "filt2freq")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ filt2freq = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "filt2res")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ filt2res = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "filt3freq")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ filt3freq = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "filt3res")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ filt3res = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "env0att")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ env0att = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "env0dec")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ env0dec = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "env0rel")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ env0rel = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "env0attlev")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ env0attlev = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "env0suslev")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ env0suslev = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "env0attshape")
								this->rthis.PushMode(&this->rthis.paRamp, [this](void* pvalue)
									{ env0attshape = *static_cast<ERampShape*>(pvalue); });
							else if (nodekey == "env0decshape")
								this->rthis.PushMode(&this->rthis.paRamp, [this](void* pvalue)
									{ env0decshape = *static_cast<ERampShape*>(pvalue); });
							else if (nodekey == "env0relshape")
								this->rthis.PushMode(&this->rthis.paRamp, [this](void* pvalue)
									{ env0relshape = *static_cast<ERampShape*>(pvalue); });
							else if (nodekey == "env1att")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ env1att = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "env1dec")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ env1dec = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "env1rel")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ env1rel = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "env1attlev")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ env1attlev = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "env1suslev")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ env1suslev = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "env1attshape")
								this->rthis.PushMode(&this->rthis.paRamp, [this](void* pvalue)
									{ env1attshape = *static_cast<ERampShape*>(pvalue); });
							else if (nodekey == "env1decshape")
								this->rthis.PushMode(&this->rthis.paRamp, [this](void* pvalue)
									{ env1decshape = *static_cast<ERampShape*>(pvalue); });
							else if (nodekey == "env1relshape")
								this->rthis.PushMode(&this->rthis.paRamp, [this](void* pvalue)
									{ env1relshape = *static_cast<ERampShape*>(pvalue); });
							else if (nodekey == "env2att")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ env2att = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "env2dec")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ env2dec = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "env2rel")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ env2rel = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "env2attlev")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ env2attlev = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "env2suslev")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ env2suslev = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "env2attshape")
								this->rthis.PushMode(&this->rthis.paRamp, [this](void* pvalue)
									{ env2attshape = *static_cast<ERampShape*>(pvalue); });
							else if (nodekey == "env2decshape")
								this->rthis.PushMode(&this->rthis.paRamp, [this](void* pvalue)
									{ env2decshape = *static_cast<ERampShape*>(pvalue); });
							else if (nodekey == "env2relshape")
								this->rthis.PushMode(&this->rthis.paRamp, [this](void* pvalue)
									{ env2relshape = *static_cast<ERampShape*>(pvalue); });
							else if (nodekey == "env3att")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ env3att = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "env3dec")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ env3dec = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "env3rel")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ env3rel = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "env3attlev")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ env3attlev = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "env3suslev")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ env3suslev = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "env3attshape")
								this->rthis.PushMode(&this->rthis.paRamp, [this](void* pvalue)
									{ env3attshape = *static_cast<ERampShape*>(pvalue); });
							else if (nodekey == "env3decshape")
								this->rthis.PushMode(&this->rthis.paRamp, [this](void* pvalue)
									{ env3decshape = *static_cast<ERampShape*>(pvalue); });
							else if (nodekey == "env3relshape")
								this->rthis.PushMode(&this->rthis.paRamp, [this](void* pvalue)
									{ env3relshape = *static_cast<ERampShape*>(pvalue); });
							else if (nodekey == "filt0del")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ filt0del = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "filt1del")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ filt1del = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "filt2del")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ filt2del = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "filt3del")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ filt3del = static_cast<float>(*static_cast<double*>(pvalue)); });
							else
								this->InvalidKeyError(std::move(nodekey));
						}

					private:
						virtual void OnPushNode(std::string&& nodekey) override
						{
							Reset();
							OnNode(std::move(nodekey));
						}
						virtual void OnNextNode(std::string&& nodekey) override
						{
							OnNode(std::move(nodekey));
						}
						virtual void OnPopNode() override
						{
							this->up();

							if (preset != "")
							{
								bool bError = true;
								std::ifstream file(std::string("./presets/") + preset + ".json");
								if (file)
								{
									JsonInterpreter presetReader(this->rthis);
									JsonParser p;
									bError = !p.parse(file, presetReader);
								}

								if (bError)
									this->error("Invalid preset");
								return; // Preset will add synths in child interpreter
							}

							const size_t pdidx(this->rthis.partdatas.size() - 1);
							for (size_t i = 0, n = this->rthis.partdatas[pdidx].ndups + 1; i < n; ++i)
							{
								SharedPtr<HitSynth_t> psynth = CreateSynth();
								this->rthis.partdatas[pdidx].instrument.push_back(psynth);
								HitSynth_t& synth = *psynth;
								synth.SetStrengthToAmp(strenToAmp);
								synth.SetTransientTime(transientTime);
								synth.SetTransientShape(transientShape);
								synth.SetDecayDelay(decayDelay);
								synth.SetDecayAmount(decayAmount);
								synth.SetDecayTime(decayTime);
								synth.SetDecayShape(decayShape);
								synth.SetFundamental(freq);
								synth.SetDetuneDelay(detuneDelay);
								synth.SetDetuneAmount(detuneAmount);
								synth.SetDetuneTime(detuneTime);
								synth.SetDetuneShape(detuneShape);
								synth.template SetFilt<0>(filt0freq, filt0res);
								synth.template SetFilt<1>(filt1freq, filt1res);
								synth.template SetFilt<2>(filt2freq, filt2res);
								synth.template SetFilt<3>((filt3freq > 0.0f) ? filt3freq : freq, filt3res);
								synth.template SetEnvelope<0>(env0att, env0dec, env0rel, env0attlev, env0suslev,
									env0attshape, env0decshape, env0relshape);
								synth.template SetEnvelope<1>(env1att, env1dec, env1rel, env1attlev, env1suslev,
									env1attshape, env1decshape, env1relshape);
								synth.template SetEnvelope<2>(env2att, env2dec, env2rel, env2attlev, env2suslev,
									env2attshape, env2decshape, env2relshape);
								synth.template SetEnvelope<3>(env3att, env3dec, env3rel, env3attlev, env3suslev,
									env3attshape, env3decshape, env3relshape);
								synth.template SetFiltDelay<0>(filt0del);
								synth.template SetFiltDelay<1>(filt1del);
								synth.template SetFiltDelay<2>(filt2del);
								synth.template SetFiltDelay<3>(filt3del);
								synth.ActivateFilters();
							}
						}

					protected:
						std::string preset;

						float freq;

						float strenToAmp;
						double transientTime;
						ERampShape transientShape;
						double decayDelay;
						float decayAmount;
						double decayTime;
						ERampShape decayShape;
						double detuneDelay;
						float detuneAmount;
						double detuneTime;
						ERampShape detuneShape;
						float filt0freq;
						float filt0res;
						float filt1freq;
						float filt1res;
						float filt2freq;
						float filt2res;
						float filt3freq;
						float filt3res;
						float env0att;
						float env0dec;
						float env0rel;
						float env0attlev;
						float env0suslev;
						ERampShape env0attshape;
						ERampShape env0decshape;
						ERampShape env0relshape;
						float env1att;
						float env1dec;
						float env1rel;
						float env1attlev;
						float env1suslev;
						ERampShape env1attshape;
						ERampShape env1decshape;
						ERampShape env1relshape;
						float env2att;
						float env2dec;
						float env2rel;
						float env2attlev;
						float env2suslev;
						ERampShape env2attshape;
						ERampShape env2decshape;
						ERampShape env2relshape;
						float env3att;
						float env3dec;
						float env3rel;
						float env3attlev;
						float env3suslev;
						ERampShape env3attshape;
						ERampShape env3decshape;
						ERampShape env3relshape;
						float filt0del;
						float filt1del;
						float filt2del;
						float filt3del;
					};

					class DrumHit : public HitSynth<DrumHitSynth>
					{
					public:
						template<typename... Ts>
						DrumHit(Ts&&... params)
							: HitSynth<DrumHitSynth>(std::forward<Ts>(params)...)
						{
							OnReset();
						}

						virtual std::string ModeKey() const override
						{
							return "drumhit";
						}

					private:
						virtual SharedPtr<DrumHitSynth> CreateSynth() override
						{
							SharedPtr<DrumHitSynth> drum = this->rthis.ctrls.template CreatePtr<DrumHitSynth>(this->freq, mic_r, hit_range_r, 0.0f, false);
							std::function<float(size_t, size_t)> rt60(GetRT60(this->modecay, this->freq));
							for (size_t order = 0; order < ::DrumHit::NumOrders; ++order)
								for (size_t zero = 0; zero < ::DrumHit::NumZeroes; ++zero)
									drum->SetModeDecay441(order, zero, rt60(order, zero));
							return drum;
						}

						virtual void OnReset() override
						{
							mic_r = 0.0f;
							hit_range_r = 0.2f;
						}

						virtual bool HandleOnNode(std::string&& nodekey) override
						{
							if (nodekey == "mic_r")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ mic_r = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "hit_range_r")
								this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
									{ hit_range_r = static_cast<float>(*static_cast<double*>(pvalue)); });
							else if (nodekey == "modecay" || nodekey == "modedecay")
								this->rthis.PushMode(&this->rthis.paramStr, [this](void* pvalue)
									{ modecay = *static_cast<std::string*>(pvalue); });
							else
								return false;
							return true;
						}

					private:
						float mic_r;
						float hit_range_r;
						std::string modecay;
					};

					class AdditiveHit : public HitSynth<AdditiveHitSynth>
					{
					public:
						AdditiveHit(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
							: HitSynth<AdditiveHitSynth>(rthisInit, pupInit)
							, floatlist(rthisInit, this)
						{
						}

						virtual std::string ModeKey() const override
						{
							return "additivehit";
						}

					private:
						virtual SharedPtr<AdditiveHitSynth> CreateSynth() override
						{
							SharedPtr<AdditiveHitSynth> synth(this->rthis.ctrls.template CreatePtr<AdditiveHitSynth>(this->freq, false));
							const size_t numModes(std::min(freqs.size(), amps.size()));
							for (size_t i = 0; i < numModes; ++i)
								synth->AddMode(freqs[i], amps[i]);
							return synth;
						}

						virtual void OnReset() override
						{
							freqs.clear();
							amps.clear();
						}

						virtual bool HandleOnNode(std::string&& nodekey) override
						{
							if (nodekey == "freqs")
							{
								floatlist.SetFloatList(freqs);
								this->rthis.mode = &floatlist;
							}
							else if (nodekey == "amps")
							{
								floatlist.SetFloatList(amps);
								this->rthis.mode = &floatlist;
							}
							else
								return false;
							return true;
						}

					private:
						class FloatList : public NonErrorMode
						{
						public:
							FloatList(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
								: NonErrorMode(rthisInit, pupInit), pfloats(nullptr)
							{
							}

							void SetFloatList(Vector<float>& vfloats)
							{
								pfloats = &vfloats;
							}

						private:
							virtual std::string ModeName() const override
							{
								return "Parts::Part::Instrument::AdditiveHit::FloatList";
							}

						private:
							void OnNode()
							{
								if (pfloats)
									this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
										{ pfloats->push_back(static_cast<float>(*static_cast<double*>(pvalue))); });
							}

						private:
							virtual void OnPushNode() override
							{
								OnNode();
							}
							virtual void OnNextNode() override
							{
								OnNode();
							}
							virtual void OnPopNode() override
							{
								this->up();
							}
							virtual void OnNumber(double value) override
							{
								this->up();
								if (pfloats)
									pfloats->push_back(static_cast<float>(value));
							}

						private:
							Vector<float>* pfloats;
						};

					private:
						FloatList floatlist;

					private:
						Vector<float> freqs;
						Vector<float> amps;
					};

				private:
					FilteredSynth<true> filteredsaw;
					FilteredSynth<false> noisehit;
					DrumHit drumhit;
					AdditiveHit additivehit;
				};

				class Outputs : public NonErrorMode
				{
				public:
					Outputs(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
						: NonErrorMode(rthisInit, pupInit), output(rthisInit, this)
					{
					}

				private:
					virtual std::string ModeName() const override { return "Parts::Part::Outputs"; }

				private:
					void OnNode()
					{
						this->rthis.partdatas.back().outfaders.push_back(MakeShared<Fader<>>());
						this->rthis.partdatas.back().outputs.push_back(nullptr);
						this->rthis.mode = &output;
					}

				private:
					virtual void OnPushNode() override { OnNode(); }
					virtual void OnNextNode() override { OnNode(); }
					virtual void OnPopNode() override { this->up(); }

				private:
					class Output : public NonErrorMode
					{
					public:
						Output(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
							: NonErrorMode(rthisInit, pupInit), path(rthisInit, this)
						{
						}

					private:
						virtual std::string ModeName() const override { return "Parts::Part::Outputs::Output"; }

					private:
						void OnNode(std::string&& nodekey)
						{
							if (nodekey == "path")
							{
								this->rthis.mode = &path;
							}
							else if (nodekey == "volume")
							{
								this->rthis.PushMode(&this->rthis.volume,
									[this](void* pvalue)
									{
										this->rthis.partdatas.back().outfaders.back()->SetGainDB(
											static_cast<float>(*static_cast<double*>(pvalue)));
									});
							}
							else
								this->InvalidKeyError(std::move(nodekey));
						}

					private:
						virtual void OnPushNode(std::string&& nodekey) override { OnNode(std::move(nodekey)); }
						virtual void OnNextNode(std::string&& nodekey) override { OnNode(std::move(nodekey)); }
						virtual void OnPopNode() override
						{
							this->up();
							auto& rpoutput = this->rthis.partdatas.back().outputs.back();
							if (!rpoutput)
								rpoutput = this->rthis.mainout;
						}

					private:
						class Path : public MixerPath
						{
						public:
							Path(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
								: MixerPath(rthisInit, pupInit)//, pathidx(0)
							{
							}

						private:
							virtual std::string ModeName() const override { return "Parts::Part::Outputs::Output::Path"; }

						private:
							virtual void OnPopNode(const SharedPtr<BusData>& poutput) override
							{
								this->rthis.partdatas.back().outputs.back() = poutput;
							}
						};

					private:
						Path path;
					};

				private:
					Output output;
				};

				class Notes : public NonErrorMode
				{
				public:
					Notes(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
						: NonErrorMode(rthisInit, pupInit),
						tuning(rthisInit, this), timing(rthisInit, this), mindur(rthisInit, this),
						db(rthisInit, this), values(rthisInit, this), sidechain(rthisInit, this)
					{
					}

				private:
					virtual std::string ModeName() const override { return "Parts::Part::Notes"; }

				private:
					void OnNode(std::string&& nodekey)
					{
						if (nodekey == "tuning")
							this->rthis.mode = &tuning;
						else if (nodekey == "timing")
							this->rthis.mode = &timing;
						else if (nodekey == "minduration" || nodekey == "mindur")
							this->rthis.mode = &mindur;
						else if (nodekey == "dur" || nodekey == "duration")
							this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
								{
									this->rthis.partdatas.back().dur =
										static_cast<float>(this->rthis.beatlen * (*static_cast<double*>(pvalue)));
								});
						else if (nodekey == "db")
							this->rthis.mode = &db;
						else if (nodekey == "values")
							this->rthis.mode = &values;
						else if (nodekey == "sidechain")
							this->rthis.mode = &sidechain;
						else if (nodekey == "transpose")
							this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
								{
									this->rthis.partdatas.back().transpose =
										static_cast<float>(*static_cast<double*>(pvalue));
								});
						else
							this->InvalidKeyError(std::move(nodekey));
					}

				private:
					virtual void OnPushNode(std::string&& nodekey) override
					{
						values.SetRelativeTime();
						values.SetMinDur(0.001);
						OnNode(std::move(nodekey));
					}
					virtual void OnNextNode(std::string&& nodekey) override { OnNode(std::move(nodekey)); }
					virtual void OnPopNode() override { this->up(); }

				private:
					class Tuning : public NonErrorMode
					{
					public:
						Tuning(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
							: NonErrorMode(rthisInit, pupInit)
						{
						}

					private:
						virtual std::string ModeName() const override { return "Parts::Part::Notes::Tuning"; }

					private:
						virtual void OnString(std::string&& value) override
						{
							this->up();
							if (value.substr(0, 3) == "edo")
							{
								uint_fast16_t edo(0);
								for (const char c : value.substr(3))
								{
									if (c < '0' || c > '9')
									{
										this->error(std::string("Equal temperment must be of the form \"edo19\". Value provided: \"")
											+ std::move(value) + "\"");
										return;
									}
									edo *= 10;
									edo += c - '0';
								}
								if (edo < 2)
								{
									this->error(std::string("Equal temperment must be 2 or greater EDO. Value provided: \"")
										+ std::move(value) + "\"");
									return;
								}
								this->rthis.partdatas.back().edoinv = 1.0 / static_cast<double>(edo);
							}
							else if (value == "just")
								this->rthis.partdatas.back().edoinv = 1.0;
							else if (value == "freq")
								this->rthis.partdatas.back().edoinv = 0.0;
							else
								this->InvalidStringError(std::move(value));
						}
					};

					class Timing : public NonErrorMode
					{
					public:
						Timing(JsonInterpreter& rthisInit, Notes* const pupInit)
							: NonErrorMode(rthisInit, pupInit)
						{
						}

					private:
						Notes* getnotesmode() const
						{
							return static_cast<Notes*>(this->getup());
						}

					private:
						virtual std::string ModeName() const override { return "Parts::Part::Notes::Timing"; }

					private:
						virtual void OnString(std::string&& value) override
						{
							this->up();
							if (value == "absolute")
								getnotesmode()->values.SetAbsoluteTime();
							else if (value == "relative")
								getnotesmode()->values.SetRelativeTime();
							else if (value == "intuitive")
								getnotesmode()->values.SetIntuitiveTime();
							else
								this->InvalidStringError(std::move(value));
						}
					};

					class MinDuration : public NonErrorMode
					{
					public:
						MinDuration(JsonInterpreter& rthisInit, Notes* const pupInit)
							: NonErrorMode(rthisInit, pupInit)
						{
						}

					private:
						Notes* getnotesmode() const
						{
							return static_cast<Notes*>(this->getup());
						}

					private:
						virtual std::string ModeName() const override { return "Parts::Part::Notes::MinDuration"; }

					private:
						virtual void OnNumber(double value) override
						{
							this->up();
							getnotesmode()->values.SetMinDur(value);
						}
					};

					class DB : public NonErrorMode
					{
					public:
						DB(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
							: NonErrorMode(rthisInit, pupInit)
						{
						}

					private:
						virtual std::string ModeName() const override { return "Parts::Part::Notes::DB"; }

					private:
						virtual void OnBool(bool value) override
						{
							this->up();
							this->rthis.partdatas.back().noteampsdb = value;
						}
					};

					class Values : public NonErrorMode
					{
					public:
						Values(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
							: NonErrorMode(rthisInit, pupInit), valueMode(rthisInit, this)
						{
						}

						void SetAbsoluteTime()
						{
							valueMode.SetAbsoluteTime();
						}

						void SetRelativeTime()
						{
							valueMode.SetOldRelativeTime();
						}

						void SetIntuitiveTime()
						{
							valueMode.SetIntuitiveTime();
						}

						void SetMinDur(const double value)
						{
							valueMode.SetMinDur(value);
						}

					private:
						virtual std::string ModeName() const override { return "Parts::Part::Notes::Values"; }

					private:
						void OnNode()
						{
							this->rthis.mode = &valueMode;
							this->rthis.partdatas.back().notes.emplace_back();
						}

					private:
						virtual void OnPushNode() override
						{
							if (this->rthis.partdatas.back().notes.size() > 0)
								this->error(std::string("Note values for a part must be specified only once"));
							else
							{
								valueMode.ResetBeat();
								OnNode();
							}
					   	}
						virtual void OnNextNode() override { OnNode(); }
						virtual void OnPopNode() override { this->up(); }

					private:
						class Value : public NonErrorMode
						{
						public:
							Value(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
								: NonErrorMode(rthisInit, pupInit),
								pitch(rthisInit, this), beat(rthisInit, this), beatrel(rthisInit, this),
								dur(rthisInit, this), art(rthisInit, this), amp(rthisInit, this),
								notevalerr(rthisInit),
								valuemodes{ &pitch, &beat, &dur, &amp }, nextmode(0)
							{
							}

							void SetAbsoluteTime()
							{
								valuemodes[1] = &beat;
								valuemodes[2] = &dur;
								valuemodes[3] = &amp;
							}

							void SetRelativeTime()
							{
								valuemodes[1] = &beatrel;
								valuemodes[2] = &art;
								valuemodes[3] = &amp;
							}

							void SetOldRelativeTime()
							{
								SetRelativeTime();
								art.SetNegativeArticulation(false);
							}

							void SetIntuitiveTime()
							{
								SetRelativeTime();
								art.SetNegativeArticulation(true);
							}

							void ResetBeat()
							{
								beatrel.ResetBeat();
								nextmode = 0;
							}

							void SetMinDur(const double value)
							{
								art.SetMinDur(value);
							}

						private:
							virtual std::string ModeName() const override
							{
								return "Parts::Part::Notes::Values::Value";
							}

						private:
							virtual void OnPushNode() override
							{
								const size_t offset = this->rthis.partdatas.back().isrhythm;
								this->rthis.mode = valuemodes[offset];
								nextmode = 1 + 2*offset;
							}
							virtual void OnNextNode() override
							{
								constexpr const size_t nummodes = (sizeof(valuemodes) / sizeof(InterpreterMode*));
								if (nextmode < nummodes)
								{
									this->rthis.mode = valuemodes[nextmode++];
								}
								else
									this->error(std::string("Note values take max ") + std::to_string(nummodes) + " parameters");
							}
							virtual void OnPopNode() override
							{
								this->up();
								if (nextmode == 0)
								{
									// Empty list of values
									this->rthis.partdatas.back().notes.clear();
									this->rthis.OnPopNode();
									return;
								}
								// Only amplitude has a default value
								if ((valuemodes[1] == &beat && nextmode < 3)
									|| (valuemodes[2] == &art && nextmode < 3)
									|| (valuemodes[1] == &beatrel && nextmode < 2))
									this->error("Note values must have time specified");
							}

						private:
							class Pitch : public NonErrorMode
							{
							public:
								Pitch(JsonInterpreter& rthisInit, Value* const pupInit)
									: NonErrorMode(rthisInit, pupInit), just(rthisInit, this)
								{
								}

							private:
								Value* getvalmode() const
								{
									return static_cast<Value*>(this->getup());
								}

							private:
								virtual std::string ModeName() const override
								{
									return "Parts::Part::Notes::Values::Value::Pitch";
								}

							private:
								virtual void OnPushNode() override
								{
									if (this->rthis.partdatas.back().edoinv != 1.0)
									{
										this->error(std::string("Non-just-intonation pitch values do not take arrays"));
										return;
									}

									// In case previous note was a rest in relative time
									if (getvalmode()->valuemodes[1] == &getvalmode()->beatrel)
										getvalmode()->SetRelativeTime();

									just.Reset();
									this->rthis.mode = &just;
								}
								virtual void OnNextNode() override
								{
									this->rthis.mode = &just;
								}
								virtual void OnPopNode() override
								{
									this->up();
									if (!this->rthis.partdatas.back().isrhythm &&
										this->rthis.partdatas.back().notes.back().freq == 0.0f)
										this->error("Just intonation pitch values must take 2 numbers in an array, and cannot be 0");
								}
								virtual void OnNumber(double value) override
								{
									this->up();

									// In case previous note was a rest in relative time
									if (getvalmode()->valuemodes[1] == &getvalmode()->beatrel)
										getvalmode()->SetRelativeTime();

									const double edoinv(this->rthis.partdatas.back().edoinv);
									if (edoinv == 0.0)
									{
										this->rthis.partdatas.back().notes.back().freq = static_cast<float>(value);
									}
									else if (edoinv == 1.0)
									{
										this->error(std::string("Just intonation pitch values only take arrays"));
									}
									else
									{
										// f = pow(2, value / edo) * key
										const float freq = static_cast<float>(std::pow(2.0, value * edoinv) * this->rthis.key * this->rthis.partdatas.back().transpose);
										this->rthis.partdatas.back().notes.back().freq = freq;
									}
								}
								virtual void OnNull() override
								{
									this->up();
									if (getvalmode()->valuemodes[1] !=
										&getvalmode()->beatrel)
									{
										this->error("Rests indicated by null only allowed in relative time");
										return;
									}
									getvalmode()->beatrel.SetRest();
									getvalmode()->valuemodes[2] =
										&getvalmode()->notevalerr;
									getvalmode()->valuemodes[3] =
										&getvalmode()->notevalerr;
									this->rthis.partdatas.back().notes.pop_back();
								}

							private:
								class Just : public NonErrorMode
								{
								public:
									Just(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
										: NonErrorMode(rthisInit, pupInit), numer(0), denom(0)
									{
									}

									void Reset()
									{
										numer = 0;
										denom = 0;
									}

								private:
									virtual std::string ModeName() const override
									{
										return "Parts::Part::Notes::Values::Value::Pitch::Just";
									}

								private:
									virtual void OnNumber(double value) override
									{
										this->up();
										if (numer == 0)
										{
											numer = static_cast<int_fast16_t>(value);
										}
										else if (denom == 0)
										{
											denom = static_cast<int_fast16_t>(value);
											const float freq = static_cast<float>(
												this->rthis.key *
												this->rthis.partdatas.back().transpose *
												static_cast<double>(numer) / static_cast<double>(denom));
											this->rthis.partdatas.back().notes.back().freq = freq;
										}
										else
										{
											this->error("Just intonation pitch values must take 2 numbers in an array");
										}
									}

								private:
									int_fast16_t numer;
									int_fast16_t denom;
								};

							private:
								Just just;
							};

							class Beat : public NonErrorMode
							{
							public:
								Beat(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
									: NonErrorMode(rthisInit, pupInit)
								{
								}

							private:
								virtual std::string ModeName() const override
								{
									return "Parts::Part::Notes::Values::Value::Beat";
								}

							private:
								virtual void OnNumber(double value) override
								{
									this->up();
									this->rthis.partdatas.back().notes.back().time = static_cast<float>(value * this->rthis.beatlen);
								}
							};

							class BeatRelative : public NonErrorMode
							{
							public:
								BeatRelative(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
									: NonErrorMode(rthisInit, pupInit), currentbeat(0.0), bResting(false)
								{
								}

								void ResetBeat()
								{
									currentbeat = 0.0;
									bResting = false;
								}

								void SetRest()
								{
									bResting = true;
								}

							private:
								virtual std::string ModeName() const override
								{
									return "Parts::Part::Notes::Values::Value::BeatRelative";
								}

							private:
								virtual void OnNumber(double value) override
								{
									this->up();
									if (!bResting)
									{
										auto& note = this->rthis.partdatas.back().notes.back();
										note.time = static_cast<float>(currentbeat * this->rthis.beatlen);
										note.dur = static_cast<float>(value * this->rthis.beatlen);
									}
									else
									{
										bResting = false;
									}
									currentbeat += value;
								}

							private:
								double currentbeat;
								bool bResting;
							};

							class Duration : public NonErrorMode
							{
							public:
								Duration(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
									: NonErrorMode(rthisInit, pupInit)
								{
								}

							private:
								virtual std::string ModeName() const override
								{
									return "Parts::Part::Notes::Values::Value::Duration";
								}

							private:
								virtual void OnNumber(double value) override
								{
									this->up();
									this->rthis.partdatas.back().notes.back().dur = static_cast<float>(value);
								}
							};

							class Articulation : public NonErrorMode
							{
							public:
								Articulation(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
									: NonErrorMode(rthisInit, pupInit), mindur(0.001), bNegativeArticulation(false)
								{
								}

								void SetMinDur(const double value)
								{
									mindur = value;
								}

								double GetMinDur() const
								{
									return mindur;
								}

								void SetNegativeArticulation(const bool b)
								{
									bNegativeArticulation = b;
								}

							private:
								virtual std::string ModeName() const override
								{
									return "Parts::Part::Notes::Values::Value::Articulation";
								}

							private:
								virtual void OnNumber(double value) override
								{
									this->up();
									float& notedur = this->rthis.partdatas.back().notes.back().dur;
									const float newdur = (bNegativeArticulation)
										? (static_cast<float>(value) + ((value < 0.0) ? notedur : 0.0f))
										: (notedur - static_cast<float>(value));
									notedur = (newdur < mindur) ? mindur : newdur;
								}

							private:
								double mindur;
								bool bNegativeArticulation;
							};

							class Amplitude : public NonErrorMode
							{
							public:
								Amplitude(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
									: NonErrorMode(rthisInit, pupInit)
								{
								}

							private:
								virtual std::string ModeName() const override
								{
									return "Parts::Part::Notes::Values::Value::Amplitude";
								}

							private:
								virtual void OnNumber(double value) override
								{
									this->up();
									this->rthis.partdatas.back().notes.back().amp = static_cast<float>(value);
								}

								virtual void OnNull() override
								{
									this->up();
									this->rthis.partdatas.back().notes.back().amp = -std::numeric_limits<float>::infinity();
								}
							};

							class NoteValueError : public NonErrorMode // Transitioning to error
							{
							public:
								NoteValueError(JsonInterpreter& rthisInit)
									: NonErrorMode(rthisInit, &rthisInit.error)
								{
								}

							private:
								virtual std::string ModeName() const override
								{
									return "Parts::Part::Notes::Values::Value::NoteValueError";
								}
							};

						private:
							Pitch pitch;
							Beat beat;
							BeatRelative beatrel;
							Duration dur;
							Articulation art;
							Amplitude amp;
							NoteValueError notevalerr;
							InterpreterMode* valuemodes[4];
							size_t nextmode;
						};

					private:
						Value valueMode;
					};

					class Sidechain : public NonErrorMode
					{
					public:
						Sidechain(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
							: NonErrorMode(rthisInit, pupInit), path(rthisInit, this)
						{
						}

					private:
						virtual std::string ModeName() const override
						{
							return "Parts::Part::Notes::Sidechain";
						}

					private:
						void OnNode(std::string&& nodekey)
						{
							// TODO: Continue here
						}

					private:
						virtual void OnPushNode(std::string&& nodekey) override
						{
							OnNode(std::move(nodekey));
						}
						virtual void OnNextNode(std::string&& nodekey) override
						{
							OnNode(std::move(nodekey));
						}
						virtual void OnPopNode() override
						{
						}
						
					private:
						class Path : public MixerPath
						{
						public:
							Path(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
								: MixerPath(rthisInit, pupInit)
							{
							}

						private:
							virtual std::string ModeName() const override { return "Parts::Part::Notes::Sidechain::Path"; }

						private:
							virtual void OnPopNode(const SharedPtr<BusData>& poutput) override
							{
								this->rthis.parts.part.notes.sidechain.duckee = poutput;
							}
						};

					private:
						Path path;
						SharedPtr<BusData> duckee;
					};

				private:
					Tuning tuning;
					Timing timing;
					MinDuration mindur;
					DB db;
					Values values;
					Sidechain sidechain;
				};

			private:
				Instrument instrument;
				Outputs outputs;
				Notes notes;
			};

		private:
			Part part;

		private:
			std::unordered_map<std::string, size_t> partdatamap;
			Vector<std::string> partdatakeys;
		};

		class Volume : public NonErrorMode
		{
		public:
			Volume(JsonInterpreter& rthisInit)
				: NonErrorMode(rthisInit, &rthisInit.error)
			{
			}

		private:
			virtual std::string ModeName() const override { return "Volume"; }

		private:
			virtual void OnNumber(double value) override
			{
				this->rthis.PopMode(&value);
			}
		};

		class FX : public NonErrorMode
		{
		public:
			FX(JsonInterpreter& rthisInit)
				: NonErrorMode(rthisInit, &rthisInit.error),
				params(rthisInit, this),
				bTwoPops(false)
			{
			}

		private:
			virtual std::string ModeName() const override { return "FX"; }

		private:
			void OnNode()
			{
				params.Reset();
			}

		private:
			enum class EValidFX
			{
				BiquadLP, BiquadHP, BiquadAP, BiquadNotch, BiquadPeak, BiquadLoShelf, BiquadHiShelf,
				Ladder, BesselLP, Panner, Fader, Delay, Distortion, BusDistortion, RingMod, RingModSum,
				Compressor, Reverb, MSConverter, LRConverter,
				NUM
			};

			static constexpr const uint64_t ParamFreqBit = 0x01;
			static constexpr const uint64_t ParamQBit = 0x02;
			static constexpr const uint64_t ParamGainBit = 0x04;
			static constexpr const uint64_t ParamTopoBit = 0x08;
			static constexpr const uint64_t ParamOrderBit = 0x10;
			static constexpr const uint64_t ParamPanBit = 0x20;
			static constexpr const uint64_t ParamDelayBit = 0x40;
			static constexpr const uint64_t ParamFeedbackBit = 0x80;
			static constexpr const uint64_t ParamThresholdBit = 0x100;
			static constexpr const uint64_t ParamRatioBit = 0x200;
			static constexpr const uint64_t ParamKneeBit = 0x400;
			static constexpr const uint64_t ParamAttackBit = 0x800;
			static constexpr const uint64_t ParamReleaseBit = 0x1000;
			static constexpr const uint64_t ParamStereoLinkBit = 0x2000;
			static constexpr const uint64_t ParamDryVolumeBit = 0x4000;

			static constexpr const uint64_t ParamsNone = 0;
			static constexpr const uint64_t ParamsFreq = ParamFreqBit;
			static constexpr const uint64_t ParamsFreqQ = ParamFreqBit | ParamQBit;
			static constexpr const uint64_t ParamsFreqQGain = ParamFreqBit | ParamQBit | ParamGainBit;
			static constexpr const uint64_t ParamsFreqGain = ParamFreqBit | ParamGainBit;
			static constexpr const uint64_t ParamsTopo = ParamTopoBit;
			static constexpr const uint64_t ParamsFreqTopo = ParamFreqBit | ParamTopoBit;
			static constexpr const uint64_t ParamsFreqQTopo = ParamFreqBit | ParamQBit | ParamTopoBit;
			static constexpr const uint64_t ParamsFreqGainTopo = ParamFreqBit | ParamGainBit | ParamTopoBit;
			static constexpr const uint64_t ParamsFreqQGainTopo = ParamFreqBit | ParamQBit | ParamGainBit | ParamTopoBit;
			static constexpr const uint64_t ParamsOrder = ParamOrderBit;
			static constexpr const uint64_t ParamsFreqOrder = ParamFreqBit | ParamOrderBit;
			static constexpr const uint64_t ParamsFreqQOrder = ParamFreqBit | ParamQBit | ParamOrderBit;
			static constexpr const uint64_t ParamsFreqGainOrder = ParamFreqBit | ParamGainBit | ParamOrderBit;
			static constexpr const uint64_t ParamsFreqQGainOrder = ParamFreqBit | ParamQBit | ParamGainBit | ParamOrderBit;
			static constexpr const uint64_t ParamsTopoOrder = ParamTopoBit | ParamOrderBit;
			static constexpr const uint64_t ParamsFreqTopoOrder = ParamFreqBit | ParamTopoBit | ParamOrderBit;
			static constexpr const uint64_t ParamsFreqQTopoOrder = ParamFreqBit | ParamQBit | ParamTopoBit | ParamOrderBit;
			static constexpr const uint64_t ParamsFreqGainTopoOrder = ParamFreqBit | ParamGainBit | ParamTopoBit | ParamOrderBit;
			static constexpr const uint64_t ParamsFreqQGainTopoOrder = ParamFreqBit | ParamQBit | ParamGainBit | ParamTopoBit | ParamOrderBit;
			static constexpr const uint64_t ParamsGain = ParamGainBit;
			static constexpr const uint64_t ParamsPan = ParamPanBit;
			static constexpr const uint64_t ParamsDelay = ParamDelayBit;
			static constexpr const uint64_t ParamsFeedback = ParamFeedbackBit;
			static constexpr const uint64_t ParamsDelayFeedback = ParamDelayBit | ParamFeedbackBit;
			static constexpr const uint64_t ParamsCompressor = 0x7f00;

			static constexpr const uint64_t FXParams[static_cast<size_t>(EValidFX::NUM)] = {
				ParamFreqBit | ParamQBit | ParamTopoBit, // BiquadLP
				ParamFreqBit | ParamQBit | ParamTopoBit, // BiquadHP
				ParamFreqBit | ParamQBit | ParamTopoBit, // BiquadAP
				ParamFreqBit | ParamQBit | ParamTopoBit, // BiquadNotch
				ParamFreqBit | ParamQBit | ParamGainBit | ParamTopoBit, // BiquadPeak
				ParamFreqBit | ParamQBit | ParamGainBit | ParamTopoBit, // BiquadLoShelf
				ParamFreqBit | ParamQBit | ParamGainBit | ParamTopoBit, // BiquadHiShelf
				ParamFreqBit | ParamQBit | ParamTopoBit | ParamOrderBit, // Ladder
				ParamFreqBit | ParamTopoBit | ParamOrderBit, // BesselLP
				ParamPanBit, // Panner
				ParamGainBit, // Fader
				ParamDelayBit | ParamFeedbackBit | ParamFreqBit | ParamTopoBit | ParamOrderBit, // Delay
				ParamOrderBit, // Distortion
				ParamOrderBit, // BusDistortion
				ParamsNone, // RingMod
				ParamPanBit, // RingModSum
				ParamTopoBit | ParamsCompressor, // Compressor
				ParamDelayBit, // Reverb
				ParamsNone, // MSConverter
				ParamsNone // LRConverter
			};

		private:
			virtual void OnPushNode() override { bTwoPops = false; OnNode(); }
			virtual void OnNextNode() override { OnNode(); }
			virtual void OnPushNode(std::string&& nodekey) override
			{
				bTwoPops = true;
				this->rthis.mode = &params;
				if (nodekey == "bqlopass")
					params.SetEffect(EValidFX::BiquadLP);
				else if (nodekey == "bqhipass")
					params.SetEffect(EValidFX::BiquadHP);
				else if (nodekey == "bqallpass")
					params.SetEffect(EValidFX::BiquadAP);
				else if (nodekey == "bqnotch")
					params.SetEffect(EValidFX::BiquadNotch);
				else if (nodekey == "bqpeak")
					params.SetEffect(EValidFX::BiquadPeak);
				else if (nodekey == "bqloshelf")
					params.SetEffect(EValidFX::BiquadLoShelf);
				else if (nodekey == "bqhishelf")
					params.SetEffect(EValidFX::BiquadHiShelf);
				else if (nodekey == "ladder")
					params.SetEffect(EValidFX::Ladder);
				else if (nodekey == "bessellopass")
					params.SetEffect(EValidFX::BesselLP);
				else if (nodekey == "panner")
					params.SetEffect(EValidFX::Panner);
				else if (nodekey == "fader")
					params.SetEffect(EValidFX::Fader);
				else if (nodekey == "delay")
					params.SetEffect(EValidFX::Delay);
				else if (nodekey == "distortion")
					params.SetEffect(EValidFX::Distortion);
				else if (nodekey == "busdistortion" || nodekey == "busdrive")
					params.SetEffect(EValidFX::BusDistortion);
				else if (nodekey == "ringmod")
					params.SetEffect(EValidFX::RingMod);
				else if (nodekey == "ringmodsum")
					params.SetEffect(EValidFX::RingModSum);
				else if (nodekey == "compressor" || nodekey == "comp")
					params.SetEffect(EValidFX::Compressor);
				else if (nodekey == "reverb" || nodekey == "verb")
					params.SetEffect(EValidFX::Reverb);
				else if (nodekey == "ms")
					params.SetEffect(EValidFX::MSConverter);
				else if (nodekey == "lr")
					params.SetEffect(EValidFX::LRConverter);
				else
					this->InvalidKeyError(std::move(nodekey));
			}
			virtual void OnPopNode() override
			{
				if (bTwoPops)
					bTwoPops = false;
				else
					this->rthis.PopMode(nullptr);
			}

		private:
			class Params : public NonErrorMode
			{
			public:
				Params(JsonInterpreter& rthisInit, InterpreterMode* const pupInit)
					: NonErrorMode(rthisInit, pupInit)
				{
				}

				void Reset() { paramsSet = 0; }

				void SetEffect(const EValidFX eFX) { eEffect = eFX; }

			private:
				virtual std::string ModeName() const override { return "FX::Params"; }

			private:
				void OnNode(std::string&& nodekey)
				{
					if (nodekey == "freq")
					{
						if (FXParams[static_cast<size_t>(eEffect)] & ParamFreqBit)
							this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
								{
									freq = *static_cast<double*>(pvalue);
									paramsSet |= ParamFreqBit;
								});
						else
							this->InvalidKeyError(std::move(nodekey));
					}
					else if (nodekey == "q")
					{
						if (FXParams[static_cast<size_t>(eEffect)] & ParamQBit)
							this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
								{
									q = *static_cast<double*>(pvalue);
									paramsSet |= ParamQBit;
								});
						else
							this->InvalidKeyError(std::move(nodekey));
					}
					else if (nodekey == "gain")
					{
						if (FXParams[static_cast<size_t>(eEffect)] & ParamGainBit)
							this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
								{
									gain = *static_cast<double*>(pvalue);
									paramsSet |= ParamGainBit;
								});
						else
							this->InvalidKeyError(std::move(nodekey));
					}
					else if (nodekey == "topo")
					{
						if (FXParams[static_cast<size_t>(eEffect)] & ParamTopoBit)
							this->rthis.PushMode(&this->rthis.paramStr, [this](void* pvalue)
								{
									topo = std::move(*static_cast<std::string*>(pvalue));
									paramsSet |= ParamTopoBit;
								});
						else
							this->InvalidKeyError(std::move(nodekey));
					}
					else if (nodekey == "order")
					{
						if (FXParams[static_cast<size_t>(eEffect)] & ParamOrderBit)
							this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
								{
									order = *static_cast<double*>(pvalue);
									paramsSet |= ParamOrderBit;
								});
						else
							this->InvalidKeyError(std::move(nodekey));
					}
					else if (nodekey == "pan")
					{
						if (FXParams[static_cast<size_t>(eEffect)] & ParamPanBit)
							this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
								{
									pan = *static_cast<double*>(pvalue);
									paramsSet |= ParamPanBit;
								});
						else
							this->InvalidKeyError(std::move(nodekey));
					}
					else if (nodekey == "delay" || nodekey == "time" || nodekey == "seconds"
						|| nodekey == "delayseconds" || nodekey == "timeseconds" || nodekey == "rt60")
					{
						if (FXParams[static_cast<size_t>(eEffect)] & ParamDelayBit)
							this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
								{
									delay = *static_cast<double*>(pvalue);
									paramsSet |= ParamDelayBit;
								});
						else
							this->InvalidKeyError(std::move(nodekey));
					}
					else if (nodekey == "delayms" || nodekey == "timems" || nodekey == "ms")
					{
						if (FXParams[static_cast<size_t>(eEffect)] & ParamDelayBit)
							this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
								{
									delay = *static_cast<double*>(pvalue) / 1000.0;
									paramsSet |= ParamDelayBit;
								});
						else
							this->InvalidKeyError(std::move(nodekey));
					}
					else if (nodekey == "delaybeats" || nodekey == "timebeats" || nodekey == "beats")
					{
						if (FXParams[static_cast<size_t>(eEffect)] & ParamDelayBit)
							this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
								{
									delay = *static_cast<double*>(pvalue) * this->rthis.beatlen;
									paramsSet |= ParamDelayBit;
								});
						else
							this->InvalidKeyError(std::move(nodekey));
					}
					else if (nodekey == "feedback")
					{
						if (FXParams[static_cast<size_t>(eEffect)] & ParamFeedbackBit)
							this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
								{
									feedback = *static_cast<double*>(pvalue);
									fbt = EFeedbackType::Gain;
									paramsSet |= ParamFeedbackBit;
								});
						else
							this->InvalidKeyError(std::move(nodekey));
					}
					else if (nodekey == "feedbackdb")
					{
						if (FXParams[static_cast<size_t>(eEffect)] & ParamFeedbackBit)
							this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
								{
									feedback = *static_cast<double*>(pvalue);
									fbt = EFeedbackType::DB;
									paramsSet |= ParamFeedbackBit;
								});
						else
							this->InvalidKeyError(std::move(nodekey));
					}
					else if (nodekey == "feedbackdbneg")
					{
						if (FXParams[static_cast<size_t>(eEffect)] & ParamFeedbackBit)
							this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
								{
									feedback = *static_cast<double*>(pvalue);
									fbt = EFeedbackType::DBNeg;
									paramsSet |= ParamFeedbackBit;
								});
						else
							this->InvalidKeyError(std::move(nodekey));
					}
					else if (nodekey == "threshold" || nodekey == "thresholddb")
					{
						if (FXParams[static_cast<size_t>(eEffect)] & ParamThresholdBit)
							this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
								{
									threshold = *static_cast<double*>(pvalue);
									paramsSet |= ParamThresholdBit;
								});
						else
							this->InvalidKeyError(std::move(nodekey));
					}
					else if (nodekey == "ratio")
					{
						if (FXParams[static_cast<size_t>(eEffect)] & ParamRatioBit)
							this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
								{
									ratio = *static_cast<double*>(pvalue);
									paramsSet |= ParamRatioBit;
								});
						else
							this->InvalidKeyError(std::move(nodekey));
					}
					else if (nodekey == "knee" || nodekey == "kneedb")
					{
						if (FXParams[static_cast<size_t>(eEffect)] & ParamKneeBit)
							this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
								{
									knee = *static_cast<double*>(pvalue);
									paramsSet |= ParamKneeBit;
								});
						else
							this->InvalidKeyError(std::move(nodekey));
					}
					else if (nodekey == "attack" || nodekey == "attackms")
					{
						if (FXParams[static_cast<size_t>(eEffect)] & ParamAttackBit)
							this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
								{
									attack_ms = *static_cast<double*>(pvalue);
									paramsSet |= ParamAttackBit;
								});
						else
							this->InvalidKeyError(std::move(nodekey));
					}
					else if (nodekey == "release" || nodekey == "releasems")
					{
						if (FXParams[static_cast<size_t>(eEffect)] & ParamReleaseBit)
							this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
								{
									release_ms = *static_cast<double*>(pvalue);
									paramsSet |= ParamReleaseBit;
								});
						else
							this->InvalidKeyError(std::move(nodekey));
					}
					else if (nodekey == "dryvolume" || nodekey == "drygain" || nodekey == "drygaindb")
					{
						if (FXParams[static_cast<size_t>(eEffect)] & ParamDryVolumeBit)
							this->rthis.PushMode(&this->rthis.paramNum, [this](void* pvalue)
								{
									dryVolume_db = *static_cast<double*>(pvalue);
									paramsSet |= ParamDryVolumeBit;
								});
						else
							this->InvalidKeyError(std::move(nodekey));
					}
					else if (nodekey == "link" || nodekey == "stereolink")
					{
						if (FXParams[static_cast<size_t>(eEffect)] & ParamStereoLinkBit)
							this->rthis.PushMode(&this->rthis.paramBool, [this](void* pvalue)
								{
									bLink = *static_cast<bool*>(pvalue);
									paramsSet |= ParamStereoLinkBit;
								});
						else
							this->InvalidKeyError(std::move(nodekey));
					}
					else if (nodekey == "none")
					{
						if (FXParams[static_cast<size_t>(eEffect)] == ParamsNone)
							this->rthis.PushMode(&this->rthis.paramNum, [](void* pvalue) {});
						else
							this->InvalidKeyError(std::move(nodekey));
					}
					else
						this->InvalidKeyError(std::move(nodekey));
				}

			private:
				virtual void OnPushNode(std::string&& nodekey) override { OnNode(std::move(nodekey)); }
				virtual void OnNextNode(std::string&& nodekey) override { OnNode(std::move(nodekey)); }
				virtual void OnPopNode() override
				{
					this->up();
					if (topo != "df2")
						paramsSet &= ~ParamTopoBit;
					if (eEffect != EValidFX::Distortion && eEffect != EValidFX::BusDistortion &&
						eEffect != EValidFX::BesselLP && static_cast<unsigned int>(order) != 2)
					{
						paramsSet &= ~ParamOrderBit;
					}

					switch (eEffect)
					{
					case EValidFX::BiquadLP:
						switch (paramsSet)
						{
						case ParamsNone: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadLP<>>()); break;
						case ParamsFreq: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadLP<>>(
								static_cast<float>(freq))); break;
						case ParamsFreqQ: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadLP<>>(
								static_cast<float>(freq), static_cast<float>(q))); break;
						case ParamsTopo: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadLP<false, 2, Filter::ETopo::DF2>>()); break;
						case ParamsFreqTopo: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadLP<false, 2, Filter::ETopo::DF2>>(
								static_cast<float>(freq))); break;
						case ParamsFreqQTopo: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadLP<false, 2, Filter::ETopo::DF2>>(
								static_cast<float>(freq), static_cast<float>(q))); break;
						default: this->error("Invalid effect parameters"); break;
						}
						break;
					case EValidFX::BiquadHP:
						switch (paramsSet)
						{
						case ParamsNone: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadHP<>>()); break;
						case ParamsFreq: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadHP<>>(
								static_cast<float>(freq))); break;
						case ParamsFreqQ: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadHP<>>(
								static_cast<float>(freq), static_cast<float>(q))); break;
						case ParamsTopo: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadHP<false, 2, Filter::ETopo::DF2>>()); break;
						case ParamsFreqTopo: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadHP<false, 2, Filter::ETopo::DF2>>(
								static_cast<float>(freq))); break;
						case ParamsFreqQTopo: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadHP<false, 2, Filter::ETopo::DF2>>(
								static_cast<float>(freq), static_cast<float>(q))); break;
						default: this->error("Invalid effect parameters"); break;
						}
						break;
					case EValidFX::BiquadAP:
						switch (paramsSet)
						{
						case ParamsNone: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadAP<>>()); break;
						case ParamsFreq: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadAP<>>(
								static_cast<float>(freq))); break;
						case ParamsFreqQ: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadAP<>>(
								static_cast<float>(freq), static_cast<float>(q))); break;
						case ParamsTopo: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadAP<false, 2, Filter::ETopo::DF2>>()); break;
						case ParamsFreqTopo: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadAP<false, 2, Filter::ETopo::DF2>>(
								static_cast<float>(freq))); break;
						case ParamsFreqQTopo: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadAP<false, 2, Filter::ETopo::DF2>>(
								static_cast<float>(freq), static_cast<float>(q))); break;
						default: this->error("Invalid effect parameters"); break;
						}
						break;
					case EValidFX::BiquadNotch:
						switch (paramsSet)
						{
						case ParamsNone: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadNotch<>>()); break;
						case ParamsFreq: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadNotch<>>(
								static_cast<float>(freq))); break;
						case ParamsFreqQ: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadNotch<>>(
								static_cast<float>(freq), static_cast<float>(q))); break;
						case ParamsTopo: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadNotch<false, 2, Filter::ETopo::DF2>>()); break;
						case ParamsFreqTopo: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadNotch<false, 2, Filter::ETopo::DF2>>(
								static_cast<float>(freq))); break;
						case ParamsFreqQTopo: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadNotch<false, 2, Filter::ETopo::DF2>>(
								static_cast<float>(freq), static_cast<float>(q))); break;
						default: this->error("Invalid effect parameters"); break;
						}
						break;
					case EValidFX::BiquadPeak:
						switch (paramsSet)
						{
						case ParamsNone: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadPeak<>>()); break;
						case ParamsFreq: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadPeak<>>(
								static_cast<float>(freq))); break;
						case ParamsFreqQ: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadPeak<>>(
								static_cast<float>(freq), static_cast<float>(q))); break;
						case ParamsFreqQGain: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadPeak<>>(
								static_cast<float>(freq), static_cast<float>(q), static_cast<float>(gain))); break;
						case ParamsTopo: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadPeak<false, 2, Filter::ETopo::DF2>>()); break;
						case ParamsFreqTopo: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadPeak<false, 2, Filter::ETopo::DF2>>(
								static_cast<float>(freq))); break;
						case ParamsFreqQTopo: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadPeak<false, 2, Filter::ETopo::DF2>>(
								static_cast<float>(freq), static_cast<float>(q))); break;
						case ParamsFreqQGainTopo: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadPeak<false, 2, Filter::ETopo::DF2>>(
								static_cast<float>(freq), static_cast<float>(q), static_cast<float>(gain))); break;
						default: this->error("Invalid effect parameters"); break;
						}
						break;
					case EValidFX::BiquadLoShelf:
						switch (paramsSet)
						{
						case ParamsNone: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadLoShelf<>>()); break;
						case ParamsFreq: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadLoShelf<>>(
								static_cast<float>(freq))); break;
						case ParamsFreqQ: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadLoShelf<>>(
								static_cast<float>(freq), static_cast<float>(q))); break;
						case ParamsFreqQGain: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadLoShelf<>>(
								static_cast<float>(freq), static_cast<float>(q), static_cast<float>(gain))); break;
						case ParamsTopo: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadLoShelf<false, 2, Filter::ETopo::DF2>>()); break;
						case ParamsFreqTopo: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadLoShelf<false, 2, Filter::ETopo::DF2>>(
								static_cast<float>(freq))); break;
						case ParamsFreqQTopo: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadLoShelf<false, 2, Filter::ETopo::DF2>>(
								static_cast<float>(freq), static_cast<float>(q))); break;
						case ParamsFreqQGainTopo: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadLoShelf<false, 2, Filter::ETopo::DF2>>(
								static_cast<float>(freq), static_cast<float>(q), static_cast<float>(gain))); break;
						default: this->error("Invalid effect parameters"); break;
						}
						break;
					case EValidFX::BiquadHiShelf:
						switch (paramsSet)
						{
						case ParamsNone: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadHiShelf<>>()); break;
						case ParamsFreq: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadHiShelf<>>(
								static_cast<float>(freq))); break;
						case ParamsFreqQ: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadHiShelf<>>(
								static_cast<float>(freq), static_cast<float>(q))); break;
						case ParamsFreqQGain: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadHiShelf<>>(
								static_cast<float>(freq), static_cast<float>(q), static_cast<float>(gain))); break;
						case ParamsTopo: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadHiShelf<false, 2, Filter::ETopo::DF2>>()); break;
						case ParamsFreqTopo: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadHiShelf<false, 2, Filter::ETopo::DF2>>(
								static_cast<float>(freq))); break;
						case ParamsFreqQTopo: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadHiShelf<false, 2, Filter::ETopo::DF2>>(
								static_cast<float>(freq), static_cast<float>(q))); break;
						case ParamsFreqQGainTopo: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BiquadHiShelf<false, 2, Filter::ETopo::DF2>>(
								static_cast<float>(freq), static_cast<float>(q), static_cast<float>(gain))); break;
						default: this->error("Invalid effect parameters"); break;
						}
						break;
					case EValidFX::Ladder:
						switch (paramsSet)
						{
						case ParamsNone: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::LadderLP<>>()); break;
						case ParamsFreq: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::LadderLP<>>(
								static_cast<float>(freq))); break;
						case ParamsFreqQ: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::LadderLP<>>(
								static_cast<float>(freq), static_cast<float>(q))); break;
						case ParamsTopo: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::LadderLP<false, 2, Filter::ETopo::DF2>>()); break;
						case ParamsFreqTopo: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::LadderLP<false, 2, Filter::ETopo::DF2>>(
								static_cast<float>(freq))); break;
						case ParamsFreqQTopo: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::LadderLP<false, 2, Filter::ETopo::DF2>>(
								static_cast<float>(freq), static_cast<float>(q))); break;
						case ParamsOrder: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::LadderLP_Custom<false, 2, Filter::ETopo::TDF2, 2>>()); break;
						case ParamsFreqOrder: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::LadderLP_Custom<false, 2, Filter::ETopo::TDF2, 2>>(
								static_cast<float>(freq))); break;
						case ParamsFreqQOrder: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::LadderLP_Custom<false, 2, Filter::ETopo::TDF2, 2>>(
								static_cast<float>(freq), static_cast<float>(q), static_cast<float>(gain))); break;
						case ParamsTopoOrder: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::LadderLP_Custom<false, 2, Filter::ETopo::DF2, 2>>()); break;
						case ParamsFreqTopoOrder: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::LadderLP_Custom<false, 2, Filter::ETopo::DF2, 2>>(
								static_cast<float>(freq))); break;
						case ParamsFreqQTopoOrder: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::LadderLP_Custom<false, 2, Filter::ETopo::DF2, 2>>(
								static_cast<float>(freq), static_cast<float>(q), static_cast<float>(gain))); break;
						default: this->error("Invalid effect parameters"); break;
						}
						break;
					case EValidFX::BesselLP:
						switch (paramsSet)
						{
						case ParamsNone: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BesselLP<2, false, 2, Filter::ETopo::TDF2>>()); break;
						case ParamsFreq: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BesselLP<2, false, 2, Filter::ETopo::TDF2>>(
								static_cast<float>(freq))); break;
						case ParamsTopo: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BesselLP<2, false, 2, Filter::ETopo::DF2>>()); break;
						case ParamsFreqTopo: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Filter::BesselLP<2, false, 2, Filter::ETopo::DF2>>(
								static_cast<float>(freq))); break;
						case ParamsOrder: this->rthis.addEffect(
							CreateBesselLPPtr<Filter::ETopo::TDF2>(this->rthis.ctrls, static_cast<uint_fast8_t>(order))); break;
						case ParamsFreqOrder: this->rthis.addEffect(
							CreateBesselLPPtr<Filter::ETopo::TDF2>(this->rthis.ctrls, static_cast<uint_fast8_t>(order),
								static_cast<float>(freq))); break;
						case ParamsTopoOrder: this->rthis.addEffect(
							CreateBesselLPPtr<Filter::ETopo::DF2>(this->rthis.ctrls, static_cast<uint_fast8_t>(order))); break;
						case ParamsFreqTopoOrder: this->rthis.addEffect(
							CreateBesselLPPtr<Filter::ETopo::DF2>(this->rthis.ctrls, static_cast<uint_fast8_t>(order),
								static_cast<float>(freq))); break;
						default: this->error("Invalid effect parameters"); break;
						}
						break;
					case EValidFX::Panner:
						switch (paramsSet)
						{
						case ParamsNone: this->rthis.addEffect(this->rthis.ctrls.template CreatePtr<Panner<>>()); break;
						case ParamsPan: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Panner<>>(static_cast<float>(pan))); break;
						default: this->error("Invalid effect parameters"); break;
						}
						break;
					case EValidFX::Fader:
						switch (paramsSet)
						{
						case ParamsNone: this->rthis.addEffect(this->rthis.ctrls.template CreatePtr<Fader<>>()); break;
						case ParamsGain: this->rthis.addEffect(
							this->rthis.ctrls.template CreatePtr<Fader<>>(static_cast<float>(gain))); break;
						default: this->error("Invalid effect parameters"); break;
						}
						break;
					case EValidFX::Delay:
						if (!(paramsSet & ParamDelayBit))
						{
							this->error("Must specify delay amount");
						}
						else
						{
							SharedPtr<Delay<>> delayptr;
							if (paramsSet & ParamFeedbackBit)
								delayptr = MakeShared<Delay<>>(
									static_cast<float>(delay),
									static_cast<float>(feedback),
									fbt);
							else
								delayptr = MakeShared<Delay<>>(
									static_cast<float>(delay));
							if (paramsSet & ParamFreqBit)
							{
								uint_fast8_t uOrder = 2;
								Filter::ETopo eTopo = Filter::ETopo::TDF2;
								if (paramsSet & ParamOrderBit)
									uOrder = static_cast<uint_fast8_t>(order);
								if (paramsSet & ParamTopoBit)
									eTopo = Filter::ETopo::DF2;
								delayptr->SetBesselFilter(
									1.0f / static_cast<float>(this->rthis.samplerate),
									static_cast<float>(freq),
									uOrder,
									2,
									eTopo);
							}
							this->rthis.addEffect(std::move(delayptr));
						}
						break;
					case EValidFX::Distortion:
					{
						constexpr const EChebyDistWaveShaper eWaveShaper = EChebyDistWaveShaper::InverseSquareGaussianBoost;
						const int iorder = (paramsSet & ParamOrderBit) ? static_cast<int>(order) : 5;
						switch (iorder)
						{
						case 2: this->rthis.addEffect(MakeShared<ChebyDist<double, 2, sampleChunkNum/2, eWaveShaper>>()); break;
						case 3: this->rthis.addEffect(MakeShared<ChebyDist<double, 3, sampleChunkNum/2, eWaveShaper>>()); break;
						case 4: this->rthis.addEffect(MakeShared<ChebyDist<double, 4, sampleChunkNum/2, eWaveShaper>>()); break;
						case 5: this->rthis.addEffect(MakeShared<ChebyDist<double, 5, sampleChunkNum/2, eWaveShaper>>()); break;
						case 6: this->rthis.addEffect(MakeShared<ChebyDist<double, 6, sampleChunkNum/2, eWaveShaper>>()); break;

						default: this->error("Invalid distortion order (must be 2-6)");
						}
					} break;
					case EValidFX::BusDistortion:
					{
						constexpr const EChebyDistWaveShaper eWaveShaper = EChebyDistWaveShaper::InverseQuart;
						const int iorder = (paramsSet & ParamOrderBit) ? static_cast<int>(order) : 5;
						switch (iorder)
						{
						case 4: this->rthis.addEffect(MakeShared<ChebyDist<double, 4, sampleChunkNum/2, eWaveShaper>>()); break;
						case 5: this->rthis.addEffect(MakeShared<ChebyDist<double, 5, sampleChunkNum/2, eWaveShaper>>()); break;
						case 6: this->rthis.addEffect(MakeShared<ChebyDist<double, 6, sampleChunkNum/2, eWaveShaper>>()); break;

						default: this->error("Invalid bus distortion order (must be 4-6)");
						}
					} break;
					case EValidFX::RingMod:
						switch (paramsSet)
						{
						case ParamsNone: this->rthis.addEffect(MakeShared<BasicRingMod<>>()); break;
						default: this->error("Invalid effect parameters"); break;
						}
					case EValidFX::RingModSum:
						switch (paramsSet)
						{
						case ParamsNone: this->rthis.addEffect(MakeShared<BasicRingModSum<>>()); break;
						case ParamsPan:
							{
								SharedPtr<BasicRingModSum<>> ptr(MakeShared<BasicRingModSum<>>());
								ptr->SetBalance(static_cast<float>(pan));
								this->rthis.addEffect(std::move(ptr));
							} break;
						default: this->error("Invalid effect parameters"); break;
						}
						break;
					case EValidFX::Compressor:
					{
						SharedPtr<Compressor<>> ptr(MakeShared<Compressor<>>());
						CompressorParams comparams;
						if (paramsSet & ParamThresholdBit)
							comparams.threshold_db = threshold;
						else
							comparams.threshold_db = -12.0;
						if (paramsSet & ParamRatioBit)
							comparams.ratio = ratio;
						else
							comparams.ratio = 4.0;
						if (paramsSet & ParamKneeBit)
							comparams.knee_db = knee;
						else
							comparams.knee_db = 1.0;
						if (paramsSet & ParamAttackBit)
							comparams.attackSamples = attack_ms*44.1;
						else
							comparams.attackSamples = 5.0*44.1;
						if (paramsSet & ParamReleaseBit)
							comparams.releaseSamples = release_ms*44.1;
						else
							comparams.releaseSamples = 25.0*44.1;
						if (paramsSet & ParamDryVolumeBit)
							comparams.dryVolume_db = static_cast<float>(dryVolume_db);
						else
							comparams.dryVolume_db = -145.0f;
						if (paramsSet & ParamTopoBit)
							comparams.df2 = true;
						else
							comparams.df2 = false;
						if (!(paramsSet & ParamStereoLinkBit))
							bLink = false;
						ptr->SetParams(comparams, bLink);
						this->rthis.addEffect(std::move(ptr));
					} break;
					case EValidFX::Reverb:
					{
						switch (paramsSet)
						{
						case ParamsNone: this->rthis.addEffect(MakeShared<FDNVerb<>>(1.5)); break;
						case ParamsDelay: this->rthis.addEffect(MakeShared<FDNVerb<>>(delay)); break;
						default: this->error("Invalid effect parameters"); break;
						}
					} break;
					case EValidFX::MSConverter:
						if (paramsSet == ParamsNone)
							this->rthis.addEffect(MakeShared<MSConverter<>>());
						else
							this->error("Invalid effect parameters");
						break;
					case EValidFX::LRConverter:
						if (paramsSet == ParamsNone)
							this->rthis.addEffect(MakeShared<LRConverter<>>());
						else
							this->error("Invalid effect parameters");
						break;
					default: this->error("Invalid effect type"); break;
					}
				}

			private:
				EValidFX eEffect;
				double freq;
				double q;
				double gain;
				std::string topo;
				double order;
				double pan;
				double delay;
				double feedback;
				EFeedbackType fbt;
				double threshold;
				double ratio;
				double knee;
				double attack_ms;
				double release_ms;
				double dryVolume_db;
				bool bLink;
				uint64_t paramsSet;
			};

		private:
			Params params;

		private:
			bool bTwoPops;
		};

		class ParamNumber : public NonErrorMode
		{
		public:
			ParamNumber(JsonInterpreter& rthisInit)
				: NonErrorMode(rthisInit, &rthisInit.error)
			{
			}

		private:
			virtual std::string ModeName() const override { return "ParamNumber"; }

		private:
			virtual void OnNumber(double value) override { this->rthis.PopMode(&value); }
		};

		class ParamString : public NonErrorMode
		{
		public:
			ParamString(JsonInterpreter& rthisInit)
				: NonErrorMode(rthisInit, &rthisInit.error)
			{
			}

		private:
			virtual std::string ModeName() const override { return "ParamString"; }

		private:
			virtual void OnString(std::string&& value) override { this->rthis.PopMode(&value); }
		};

		class ParamBool : public NonErrorMode
		{
		public:
			ParamBool(JsonInterpreter& rthisInit)
				: NonErrorMode(rthisInit, &rthisInit.error)
			{
			}

		private:
			virtual std::string ModeName() const override { return "ParamBool"; }

		private:
			virtual void OnBool(bool value) override { this->rthis.PopMode(&value); }
		};

		class ParamRampShape : public NonErrorMode
		{
		public:
			ParamRampShape(JsonInterpreter& rthisInit)
				: NonErrorMode(rthisInit, &rthisInit.error)
			{
			}

		private:
			virtual std::string ModeName() const override { return "ParamRampShape"; }

		private:
			virtual void OnString(std::string&& value) override
			{
				ERampShape eShape(static_cast<ERampShape>(-1));
				if (value == "instant")
					eShape = ERampShape::Instant;
				else if (value == "linear")
					eShape = ERampShape::Linear;
				else if (value == "quartersin" || value == "qsin")
					eShape = ERampShape::QuarterSin;
				else if (value == "s")
					eShape = ERampShape::SCurve;
				else if (value == "spow")
					eShape = ERampShape::SCurveEqualPower;
				else if (value == "log")
					eShape = ERampShape::LogScaleLinear;
				else if (value == "slog")
					eShape = ERampShape::LogScaleSCurve;
				else if (value == "halfsinlog")
					eShape = ERampShape::LogScaleHalfSin;
				else if (value == "hit")
					eShape = ERampShape::Hit;
				else if (value == "hit262")
					eShape = ERampShape::Hit262;
				else if (value == "hit272")
					eShape = ERampShape::Hit272;
				else if (value == "hit282")
					eShape = ERampShape::Hit282;
				else if (value == "hit292")
					eShape = ERampShape::Hit292;
				else if (value == "hit2a2")
					eShape = ERampShape::Hit2A2;
				else if (value == "hit2624")
					eShape = ERampShape::Hit2624;
				else
					this->InvalidStringError(std::move(value));

				if (eShape != static_cast<ERampShape>(-1))
					this->rthis.PopMode(&eShape);
			}
		};

	private:
		class ModeLogger
		{
		public:
			ModeLogger() : mode(nullptr) {}
			ModeLogger(InterpreterMode* modeInit) : mode(modeInit) {}

			ModeLogger& operator=(InterpreterMode* modeSet)
			{
				mode = modeSet;
				if (mode)
					std::cout << "Mode set to \"" << mode->ModeName() << "\"\n";
				else
					std::cout << "Mode set to null\n";
				return *this;
			}

			operator InterpreterMode*()
			{
				return mode;
			}

			InterpreterMode& operator*()
			{
				return *mode;
			}

			InterpreterMode* operator->()
			{
				return mode;
			}

		private:
			InterpreterMode* mode;
		};

	private:
		Utility::TypeIf_t<bLog, ModeLogger, InterpreterMode*> mode;
		Vector<std::pair<InterpreterMode*, std::function<void(void*)>>> modestack;
		Error error;
		Done done;
		Top top;
		Meta meta;
		Mixer mixer;
		Parts parts;
		Volume volume;
		FX fx;
		ParamNumber paramNum;
		ParamString paramStr;
		ParamBool paramBool;
		ParamRampShape paRamp;

	private:
		std::string name;
		double beatlen;
		double key;
		unsigned long samplerate;
		float timelen;
		UniquePtr<ControlSet> pctrls;
		ControlSet& ctrls;
		AudioFileOut<> wav;
		Vector<PartData> vpartdatas;
		Vector<PartData>& partdatas;
		SharedPtr<BusData> mainout;
		SharedPtr<BusData> currentbus;
		std::function<void(SharedPtr<AudioJoin<>>)> addEffect;
		bool bIsChild;
	};

	template<bool bLog>
	void JsonInterpreter_t<bLog>::NonErrorMode::error()
	{
		this->rthis.mode = &this->rthis.error;
	}

	template<bool bLog>
	void JsonInterpreter_t<bLog>::NonErrorMode::up()
	{
		this->rthis.mode = pup;
	}

	template<> void JsonInterpreter_t<false>::Top::LogWritingWav() const {}
	template<> void JsonInterpreter_t<true>::Top::LogWritingWav() const
	{
		std::cout << "Writing " << this->rthis.name << ".wav...\n";
	}

	using JsonInterpreter = JsonInterpreter_t<false>;
	using JsonInterpreter_Logging = JsonInterpreter_t<true>;
}

