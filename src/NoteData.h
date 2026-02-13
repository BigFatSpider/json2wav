// Copyright Dan Price 2026.

#pragma once

namespace json2wav
{
	struct NoteData
	{
		size_t start;
		size_t end;
		float amp;
		float freq;
		explicit NoteData(const size_t startInit, const size_t endInit, const float ampInit, const float freqInit)
			: start(startInit), end(endInit), amp(ampInit), freq(freqInit)
		{
		}
		NoteData() = delete;
		NoteData(const NoteData&) noexcept = default;
		NoteData(NoteData&&) noexcept = default;
		NoteData& operator=(const NoteData&) noexcept = default;
		NoteData& operator=(NoteData&&) noexcept = default;
		~NoteData() noexcept {}
	};
}

