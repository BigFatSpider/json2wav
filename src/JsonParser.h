// Copyright Dan Price. All rights reserved.

//--------------------------------------------
// Simple ASCII-only JSON parser
//--------------------------------------------

#pragma once

#include "Memory.h"
#include <istream>
#include <utility>
#include <string>
#include <iostream>

namespace json2wav
{
	struct JsonNodeLabel
	{
		static const size_t isKey = std::string::npos;
		explicit JsonNodeLabel(const size_t idxInit) noexcept : idx(idxInit) {}
		explicit JsonNodeLabel(const std::string& keyInit) : idx(isKey), key(keyInit) {}
		explicit JsonNodeLabel(std::string&& keyInit) noexcept : idx(isKey), key(std::move(keyInit)) {}
		JsonNodeLabel& operator=(const size_t idxSet) noexcept { idx = idxSet; return *this; }
		JsonNodeLabel& operator=(const std::string& keySet) { idx = isKey; key = keySet; return *this; }
		JsonNodeLabel& operator=(std::string&& keySet) noexcept { idx = isKey; key = std::move(keySet); return *this; }
		JsonNodeLabel(const JsonNodeLabel&) = default;
		JsonNodeLabel(JsonNodeLabel&&) noexcept = default;
		JsonNodeLabel& operator=(const JsonNodeLabel&) = default;
		JsonNodeLabel& operator=(JsonNodeLabel&&) noexcept = default;
		~JsonNodeLabel() noexcept = default;
		bool IsKey() const noexcept { return idx == isKey; }
		bool IsIdx() const noexcept { return idx != isKey; }
		size_t idx;
		std::string key;
	};

	using JsonPath = Vector<JsonNodeLabel>;

	inline std::string JsonPathToStr(const JsonPath& path)
	{
		std::string pathstr;
		bool bdot = false;
		for (const JsonNodeLabel& node : path)
		{
			if (node.IsKey())
				pathstr += (bdot) ? "." + node.key : node.key;
			else
				pathstr += std::string("[") + std::to_string(node.idx) + "]";
			bdot = true;
		}
		return pathstr;
	}

	class IJsonInterpreter
	{
	public:
		virtual ~IJsonInterpreter() noexcept {}
		virtual void OnString(const JsonPath& path, std::string&& value) {}
		virtual void OnNumber(const JsonPath& path, double value) {}
		virtual void OnBool(const JsonPath& path, bool value) {}
		virtual void OnNull(const JsonPath& path) {}
	};

	class IJsonWalker
	{
	public:
		virtual ~IJsonWalker() noexcept {}
		virtual void OnPushNode(std::string&& nodekey) {}
		virtual void OnPushNode() {}
		virtual void OnNextNode(std::string&& nodekey) {}
		virtual void OnNextNode() {}
		virtual void OnPopNode() {}
		virtual void OnString(std::string&& value) {}
		virtual void OnNumber(double value) {}
		virtual void OnBool(bool value) {}
		virtual void OnNull() {}
	};

	class IJsonLogger
	{
	public:
		virtual ~IJsonLogger() noexcept {}

	private:
		virtual void OnPushNode(std::string&& nodekey) {}
		virtual void OnPushNode() {}
		virtual void OnNextNode(std::string&& nodekey) {}
		virtual void OnNextNode() {}
		virtual void OnPopNode() {}
		virtual void OnString(std::string&& value) {}
		virtual void OnNumber(double value) {}
		virtual void OnBool(bool value) {}
		virtual void OnNull() {}

	private:
		class WalkerImpl : public IJsonWalker
		{
		public:
			WalkerImpl(IJsonLogger& loggerInit) : logger(loggerInit) {}

		private:
			virtual void OnPushNode(std::string&& nodekey) override { logger.LogPushNode(std::move(nodekey)); }
			virtual void OnPushNode() override { logger.LogPushNode(); }
			virtual void OnNextNode(std::string&& nodekey) override { logger.LogNextNode(std::move(nodekey)); }
			virtual void OnNextNode() override { logger.LogNextNode(); }
			virtual void OnPopNode() override { logger.LogPopNode(); }
			virtual void OnString(std::string&& value) override { logger.LogString(std::move(value)); }
			virtual void OnNumber(double value) override { logger.LogNumber(value); }
			virtual void OnBool(bool value) override { logger.LogBool(value); }
			virtual void OnNull() override { logger.LogNull(); }

		private:
			IJsonLogger& logger;
		} wimpl;

	public:
		IJsonWalker& walker;
		IJsonLogger() : wimpl(*this), walker(wimpl) {}

	private:
		Vector<size_t> idxstack;
		void LogPushNode(std::string&& nodekey)
		{
			idxstack.push_back(0);
			std::cout << "Push key \"" << nodekey << "\"\n";
			OnPushNode(std::move(nodekey));
		}

		void LogPushNode()
		{
			idxstack.push_back(0);
			std::cout << "Push idx " << std::to_string(0) << '\n';
			OnPushNode();
		}

		void LogNextNode(std::string&& nodekey)
		{
			std::cout << "Next key \"" << nodekey << "\"\n";
			OnNextNode(std::move(nodekey));
		}

		void LogNextNode()
		{
			std::cout << "Next idx " << std::to_string(++idxstack.back()) << '\n';
			OnNextNode();
		}

		void LogPopNode()
		{
			idxstack.pop_back();
			std::cout << "Pop\n";
			OnPopNode();
		}

		void LogString(std::string&& value)
		{
			std::cout << "String \"" << value << "\"\n";
			OnString(std::move(value));
		}

		void LogNumber(double value)
		{
			std::cout << "Number " << std::to_string(value) << '\n';
			OnNumber(value);
		}

		void LogBool(bool value)
		{
			std::cout << ((value) ? "True\n" : "False\n");
			OnBool(value);
		}

		void LogNull()
		{
			std::cout << "Null\n";
			OnNull();
		}
	};

	class JsonParser
	{
	private:
		class ParseError
		{
		};

	public:
		JsonParser() : i(nullptr), interpret(nullptr), walk(nullptr), c('\0') {}

		JsonParser(std::istream& input, IJsonInterpreter& interpreter) : JsonParser() { parse(input, interpreter); }
		JsonParser(std::istream& input, IJsonWalker& walker) : JsonParser() { parse(input, walker); }
		JsonParser(std::istream& input, IJsonLogger& logger) : JsonParser(input, logger.walker) {}

		bool parse(std::istream& input, IJsonInterpreter& interpreter)
		{
			i = &input;
			setInterpreter(interpreter);
			return parseInternal();
		}

		bool parse(std::istream& input, IJsonWalker& walker)
		{
			i = &input;
			setWalker(walker);
			return parseInternal();
		}

		bool parse(std::istream& input, IJsonLogger& logger)
		{
			return parse(input, logger.walker);
		}

	private:
		bool parseInternal()
		{
			c = '\0';
			bool bValid = true;

			try
			{
				json();
			}
			catch (ParseError& e)
			{
				bValid = false;
			}
			catch (...)
			{
				clear();
				throw;
			}

			clear();

			return bValid;
		}

		void clear() noexcept
		{
			i = nullptr;
			interpret = nullptr;
			walk = nullptr;
			path.clear();
			c = '\0';

			OnPushNodeKey_mf = nullptr;
			OnPushNodeIdx_mf = nullptr;
			OnNextNodeKey_mf = nullptr;
			OnNextNodeIdx_mf = nullptr;
			OnPopNode_mf = nullptr;
			OnString_mf = nullptr;
			OnNumber_mf = nullptr;
			OnBool_mf = nullptr;
			OnNull_mf = nullptr;
		}

		void setInterpreter(IJsonInterpreter& interpreter)
		{
			interpret = &interpreter;

			OnPushNodeKey_mf = &JsonParser::OnPushNodeKey_Interpret;
			OnPushNodeIdx_mf = &JsonParser::OnPushNodeIdx_Interpret;
			OnNextNodeKey_mf = &JsonParser::OnNextNodeKey_Interpret;
			OnNextNodeIdx_mf = &JsonParser::OnNextNodeIdx_Interpret;
			OnPopNode_mf = &JsonParser::OnPopNode_Interpret;
			OnString_mf = &JsonParser::OnString_Interpret;
			OnNumber_mf = &JsonParser::OnNumber_Interpret;
			OnBool_mf = &JsonParser::OnBool_Interpret;
			OnNull_mf = &JsonParser::OnNull_Interpret;
		}

		void setWalker(IJsonWalker& walker)
		{
			walk = &walker;

			OnPushNodeKey_mf = &JsonParser::OnPushNodeKey_Walk;
			OnPushNodeIdx_mf = &JsonParser::OnPushNodeIdx_Walk;
			OnNextNodeKey_mf = &JsonParser::OnNextNodeKey_Walk;
			OnNextNodeIdx_mf = &JsonParser::OnNextNodeIdx_Walk;
			OnPopNode_mf = &JsonParser::OnPopNode_Walk;
			OnString_mf = &JsonParser::OnString_Walk;
			OnNumber_mf = &JsonParser::OnNumber_Walk;
			OnBool_mf = &JsonParser::OnBool_Walk;
			OnNull_mf = &JsonParser::OnNull_Walk;
		}

		void error()
		{
			throw ParseError();
		}

		size_t asciidx() const noexcept
		{
			return static_cast<size_t>(c) & static_cast<size_t>(0x7f);
		}

		void nextraw()
		{
			if (!*i)
				error();
			i->get(c);
		}

		void flushws()
		{
			static const int asciiws[128] = {
				0,0,0,0,0,0,0,0,0,1 ,1,1,1,1,0,0,0,0,0,0
				,0,0,0,0,0,0,0,0,0,0 ,0,0,1,0,0,0,0,0,0,0
				,0,0,0,0,0,0,0,0,0,0 ,0,0,0,0,0,0,0,0,0,0
				,0,0,0,0,0,0,0,0,0,0 ,0,0,0,0,0,0,0,0,0,0
				,0,0,0,0,0,0,0,0,0,0 ,0,0,0,0,0,0,0,0,0,0
				,0,0,0,0,0,0,0,0,0,0 ,0,0,0,0,0,0,0,0,0,0
				,0,0,0,0,0,0,0,0
			};

			while (asciiws[asciidx()] && !i->eof())
				nextraw();
		}

		void next()
		{
			nextraw();
			flushws();
		}

		bool acceptraw(const char ch)
		{
			if (ch == c)
			{
				nextraw();
				return true;
			}
			return false;
		}

		bool accept(const char ch)
		{
			if (ch == c)
			{
				next();
				return true;
			}
			return false;
		}

		void expectraw(const char ch)
		{
			if (!acceptraw(ch))
				error();
		}

		void expect(const char ch)
		{
			if (!accept(ch))
				error();
		}

		void json()
		{
			if (i)
			{
				struct Scope
				{
					Scope(JsonParser& jparse) : j(jparse) {}
					~Scope() noexcept { j.i = nullptr; j.interpret = nullptr; j.walk = nullptr; }
					JsonParser& j;
				} _(*this);

				next();
				if (!value())
					error();
				flushws();
				if (*i)
					error();
			}
		}

		bool value()
		{
			return map() || array() || stringval() || number() || litTrue() || litFalse() || litNull();
		}

		bool map()
		{
			if (accept('{'))
			{
				if (stringkey()) // Push node after key/idx, before value; happens in stringkey()
				{
					expect(':');
					if (!value())
						error();

					while (accept(','))
					{
						if (!nextstringkey())
							error();
						expect(':');
						if (!value())
							error();
					}

					OnPopNode();
				}
				expect('}');
				return true;
			}
			return false;
		}

		bool array()
		{
			if (accept('['))
			{
				OnPushNode(); // Push node after key/idx, before value
				if (value())
				{
					while (accept(','))
					{
						OnNextNode();
						if (!value())
							error();
					}
				}
				expect(']');
				OnPopNode();
				return true;
			}
			return false;
		}

		template<typename F>
		bool string(F&& onJsonStr)
		{
			if (acceptraw('"'))
			{
				std::string buf;
				while (!accept('"'))
				{
					if (acceptraw('\\'))
					{
						switch (c)
						{
						case '"': buf.push_back('"'); break;
						case '\\': buf.push_back('\\'); break;
						case '/': buf.push_back('/'); break;
						case 'b': buf.push_back('\b'); break;
						case 'f': buf.push_back('\f'); break;
						case 'n': buf.push_back('\n'); break;
						case 'r': buf.push_back('\r'); break;
						case 't': buf.push_back('\t'); break;
						default: error();
						}
					}
					else
					{
						buf.push_back(c);
					}
					nextraw();
				}
				onJsonStr(std::move(buf));
				return true;
			}
			return false;
		}

		bool stringkey()
		{
			return string([this](std::string&& key) { OnPushNode(std::move(key)); });
		}

		bool nextstringkey()
		{
			return string([this](std::string&& key) { OnNextNode(std::move(key)); });
		}

		bool stringval()
		{
			return string([this](std::string&& value) { OnString(std::move(value)); });
		}

		bool number()
		{
			static const int isdig[128] = {
				0,0,0,0,0,0,0,0,0,0 ,0,0,0,0,0,0,0,0,0,0
				,0,0,0,0,0,0,0,0,0,0 ,0,0,0,0,0,0,0,0,0,0
				,0,0,0,0,0,0,0,0,1,1 ,1,1,1,1,1,1,1,1,0,0
				,0,0,0,0,0,0,0,0,0,0 ,0,0,0,0,0,0,0,0,0,0
				,0,0,0,0,0,0,0,0,0,0 ,0,0,0,0,0,0,0,0,0,0
				,0,0,0,0,0,0,0,0,0,0 ,0,0,0,0,0,0,0,0,0,0
				,0,0,0,0,0,0,0,0
			};

			static const int isonenine[128] = {
				0,0,0,0,0,0,0,0,0,0 ,0,0,0,0,0,0,0,0,0,0
				,0,0,0,0,0,0,0,0,0,0 ,0,0,0,0,0,0,0,0,0,0
				,0,0,0,0,0,0,0,0,0,1 ,1,1,1,1,1,1,1,1,0,0
				,0,0,0,0,0,0,0,0,0,0 ,0,0,0,0,0,0,0,0,0,0
				,0,0,0,0,0,0,0,0,0,0 ,0,0,0,0,0,0,0,0,0,0
				,0,0,0,0,0,0,0,0,0,0 ,0,0,0,0,0,0,0,0,0,0
				,0,0,0,0,0,0,0,0
			};

			std::string buf;

			const bool bneg = acceptraw('-');
			bool bnum = false;

			if (bneg)
				buf.push_back('-');

			if (acceptraw('0'))
			{
				if (isdig[asciidx()]) // No leading zeroes
					error();
				buf.push_back('0');
				bnum = true;
			}
			else if (isonenine[asciidx()])
			{
				buf.push_back(c);
				nextraw();
				while (isdig[asciidx()])
				{
					buf.push_back(c);
					nextraw();
				}
				bnum = true;
			}

			if (!bnum)
			{
				if (bneg)
					error();
				return false;
			}

			if (acceptraw('.'))
			{
				buf.push_back('.');
				if (!isdig[asciidx()])
					error();
				while (isdig[asciidx()])
				{
					buf.push_back(c);
					nextraw();
				}
			}

			if (acceptraw('e') || acceptraw('E'))
			{
				buf.push_back('e');
				if (acceptraw('-'))
					buf.push_back('-');
				else if (acceptraw('+'))
					buf.push_back('+');
				if (!isdig[asciidx()])
					error();
				while (isdig[asciidx()])
				{
					buf.push_back(c);
					nextraw();
				}
			}

			OnNumber(std::stod(buf));

			flushws();
			return true;
		}

		bool litTrue()
		{
			if (acceptraw('t'))
			{
				expectraw('r');
				expectraw('u');
				expectraw('e');
				OnBool(true);
				flushws();
				return true;
			}
			return false;
		}

		bool litFalse()
		{
			if (acceptraw('f'))
			{
				expectraw('a');
				expectraw('l');
				expectraw('s');
				expectraw('e');
				OnBool(false);
				flushws();
				return true;
			}
			return false;
		}

		bool litNull()
		{
			if (acceptraw('n'))
			{
				expectraw('u');
				expectraw('l');
				expectraw('l');
				OnNull();
				flushws();
				return true;
			}
			return false;
		}

		// Use the more functional-programming-ish technique of pointer-to-member variables instead of
		// inheritance-based polymorphism, which would require a polymorphic strategy object either on
		// the heap or managed by placement new shenanigans
		void OnPushNode(std::string&& nodekey)
		{
			(this->*OnPushNodeKey_mf)(std::move(nodekey));
		}
		void OnPushNode()
		{
			(this->*OnPushNodeIdx_mf)();
		}
		void OnNextNode(std::string&& nodekey)
		{
			(this->*OnNextNodeKey_mf)(std::move(nodekey));
		}
		void OnNextNode()
		{
			(this->*OnNextNodeIdx_mf)();
		}
		void OnPopNode()
		{
			(this->*OnPopNode_mf)();
		}
		void OnString(std::string&& value)
		{
			(this->*OnString_mf)(std::move(value));
		}
		void OnNumber(double value)
		{
			(this->*OnNumber_mf)(value);
		}
		void OnBool(bool value)
		{
			(this->*OnBool_mf)(value);
		}
		void OnNull()
		{
			(this->*OnNull_mf)();
		}

		void OnPushNodeKey_Interpret(std::string&& nodekey)
		{
			path.emplace_back(std::move(nodekey));
		}
		void OnPushNodeIdx_Interpret()
		{
			path.emplace_back((size_t)0);
		}
		void OnNextNodeKey_Interpret(std::string&& nodekey)
		{
			path.back() = std::move(nodekey);
		}
		void OnNextNodeIdx_Interpret()
		{
			++path.back().idx;
		}
		void OnPopNode_Interpret()
		{
			path.pop_back();
		}
		void OnString_Interpret(std::string&& value)
		{
			interpret->OnString(path, std::move(value));
		}
		void OnNumber_Interpret(double value)
		{
			interpret->OnNumber(path, value);
		}
		void OnBool_Interpret(bool value)
		{
			interpret->OnBool(path, value);
		}
		void OnNull_Interpret()
		{
			interpret->OnNull(path);
		}

		void OnPushNodeKey_Walk(std::string&& nodekey)
		{
			walk->OnPushNode(std::move(nodekey));
		}
		void OnPushNodeIdx_Walk()
		{
			walk->OnPushNode();
		}
		void OnNextNodeKey_Walk(std::string&& nodekey)
		{
			walk->OnNextNode(std::move(nodekey));
		}
		void OnNextNodeIdx_Walk()
		{
			walk->OnNextNode();
		}
		void OnPopNode_Walk()
		{
			walk->OnPopNode();
		}
		void OnString_Walk(std::string&& value)
		{
			walk->OnString(std::move(value));
		}
		void OnNumber_Walk(double value)
		{
			walk->OnNumber(value);
		}
		void OnBool_Walk(bool value)
		{
			walk->OnBool(value);
		}
		void OnNull_Walk()
		{
			walk->OnNull();
		}

	private:
		std::istream* i;
		IJsonInterpreter* interpret;
		IJsonWalker* walk;
		JsonPath path;
		char c;
		
		typedef void (JsonParser::*StrCB_t)(std::string&&);
		typedef void (JsonParser::*NumCB_t)(double);
		typedef void (JsonParser::*BoolCB_t)(bool);
		typedef void (JsonParser::*VoidCB_t)();

		StrCB_t OnPushNodeKey_mf;
		VoidCB_t OnPushNodeIdx_mf;
		StrCB_t OnNextNodeKey_mf;
		VoidCB_t OnNextNodeIdx_mf;
		VoidCB_t OnPopNode_mf;
		StrCB_t OnString_mf;
		NumCB_t OnNumber_mf;
		BoolCB_t OnBool_mf;
		VoidCB_t OnNull_mf;
	};
}

