#pragma once
#include <string>
#include <functional>
#include <map>
#include <vector>
#include <regex>
#include <thread>

#include <Socket.hpp>
#include <Text.hpp>
#include <Common.hpp>
#include <FileSystem.hpp>

namespace HttpServices {
	typedef enum : unsigned char {
		None,
		Get,
		Post
	}Method;
	struct Msg {
		int code = 0;
		const String *data = NULL;
		const String *msg = NULL;
		Msg(const String&data_, const String&msg_="ok", int code_=0) {
			code = code_;
			data = &data_;
			msg = &msg_;
		}
	};
	struct Range
	{
		bool IsRange = false;
		unsigned long long RangPos;
		unsigned long long RangEndPos;
		unsigned long long Total;
	};
	struct Form {
		size_t ContentLength;
		char* DataPos = NULL;
		size_t DataCount;
		String Filed;
		size_t Read(char*buf, size_t count) {
			memset(buf, 0, count);
			for (size_t i = 0; i < DataCount; i++)
			{
				buf[i] = DataPos[i];
			}
		}
	};
	//请求结构体
	struct Request
	{
	private:
		size_t ReadCount = 0;
	public:
		String Temp;
		Range range;
		String Cookie;
		String Url;
		String Header;
		std::vector<Form> Forms;
		std::map<String, String> Headers;
		HttpServices::Method Method = HttpServices::Method::None;
		Socket client;
		size_t ContentLength = 0;
		String ParamString;
		//获取URL中带的值
		String GetParam(const String&key)const {
			std::map<String, String> Params;
			if (!ParamString.empty()) {
				for (auto&it : Text::Split(ParamString, "&")) {
					size_t eq_ = it.find("=");
					String key_ = it.substr(0, eq_);
					String value = it.substr(eq_ + 1);
					if (key == key_) {
						return value;
					}
				}
			}
			return "";
		}
		bool GetHeader(const String&key, String&value) {
			for (auto&it : Headers) {
				if (it.first == key) {
					value = it.second;
					return true;
				}
			}
			return false;
		}
		int ReadStream(String&buf, size_t _Count) const {
			buf.clear();
			Request *ptr = (Request*)this;
			if (ptr->ReadCount >= ContentLength || ptr->ContentLength == -1) {
				return 0;
			}
			if (!Temp.empty()) {
				buf.append(Temp.c_str(), Temp.size());
				ptr->ReadCount += Temp.size();
				ptr->Temp.clear();
				return buf.size();
			}
			char *buf2 = new char[_Count];
			int len = client.Receive(buf2, _Count);
			if (len != -1 && len != 0) {
				buf.append(buf2, len);
				ptr->ReadCount += len;
			}
			delete buf2;
			return len == -1 ? 0 : len;
		}

		size_t ReadStreamToEnd(String&body, size_t _Count)const {
			String *buf = new String;
			while (ReadStream(*buf, _Count) > 0)
			{
				body.append(buf->c_str(), buf->size());
			}
			return body.size();
		}
	};
	struct Response
	{
		bool UseCache = false;
		int Status = 200;
		String Cookie;
		String Body;
		String Location;
		String ContentType = "text/plain";
		FileSystem::FileInfo *fileinfo = NULL;
		void SetContent(const Msg&msg, const String&ContentType_ = "application/json") {
			ContentType = ContentType_;
			Body = "{\"code\":" + std::to_string(msg.code) + ",\"data\":\"" + (msg.data == NULL ? "" : *msg.data) + "\",\"msg\":\"" + (msg.msg == NULL ? "" : *msg.msg) + "\"}";
		}
		~Response() {
			if (fileinfo) {
				delete  fileinfo;
			}
		}
	};
	typedef std::function<void(const Request&, Response&)> HttpHandler;
	class HttpLinster
	{
	public:
		String WebRoot;
		//服务器上行速度
		size_t UpSize = 1024 * 1;
		//服务器下行速度
		size_t DownSize = 1024 * 1024 * 1;
		//绑定GET请求
		void Get(const String&, const HttpHandler&);
		//绑定POST请求
		void Post(const String&, const HttpHandler&);
		//开始监听HTTP协议请求
		bool Listen(const String&address, size_t port, int backlog = SOMAXCONN);
		void Receive(Socket client);
		HttpLinster();
		~HttpLinster();
	private:
		String Address;
		size_t Port;
		std::map<String, HttpHandler> PostFunc;
		std::map<String, HttpHandler> GetFunc;
		void HandleHeader(Request & rq, Response & rp, HttpHandler *handler);
		//处理body部分
		void HandleBody(Request&rq, Response&rp);
		//处理头部信息
		bool RegexValue(const String& content, const String&regex, String&result);
	};
}

