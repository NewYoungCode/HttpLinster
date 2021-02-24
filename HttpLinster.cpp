#include "HttpLinster.h"

namespace HttpServices {

	HttpLinster::HttpLinster()
	{
		WebRoot = Path::StartPath();
	}
	HttpLinster::~HttpLinster()
	{
	}
	bool HttpLinster::RegexValue(const String & content, const String & regex, String & result)
	{
		std::regex regexExp(regex);
		std::smatch matchResult;
		if (std::regex_search(content, matchResult, regexExp)) {
			if (matchResult.size() >= 2)
			{
				result = (String)matchResult[1];
				return true;
			}
		}
		return false;
	}
	void HttpLinster::Get(const String &url, const HttpHandler &func)
	{
		GetFunc.insert(std::pair<String, HttpHandler>(url, func));
	}
	void HttpLinster::Post(const String &url, const HttpHandler &func)
	{
		PostFunc.insert(std::pair<String, HttpHandler>(url, func));
	}
	bool HttpLinster::Listen(const String & address, size_t port, int backlog)
	{
		this->Address = address;
		this->Port = port;
		Socket s;
		s.Bind(address, port);
		bool b = s.Listen(backlog);
		if (b) {
			printf("%s:%d started!\n", address.c_str(), port);
		}
		for (;;) {
			//单任务
			Receive(s.Accep());
		}
		return b;
	}
	void HttpLinster::Receive(Socket  client) {
		printf("%s:%d\n", client.Address.c_str(), client.Port);
		Request rq;
		String Cookie;
		Response rp;
		rq.client = client;
		bool endHead = false;
		char *buf = new char[UpSize];
		memset(buf, 0, UpSize);
		String RawUrl;
		HttpHandler *handler = NULL;
		for (;;) {
			int len = client.Receive(buf, UpSize);
			if (len == -1 || len == 0) {
				printf("%s连接断开!\n", client.Address.c_str());
				break;
			}
			if (rq.Header.empty()) {
				String tempStr = buf;
				size_t pos1 = tempStr.find("POST /");
				size_t pos2 = tempStr.find("GET /");
				//这里确定是否为http协议
				if (pos1 != 0 && pos2 != 0) {
					delete buf;
					return;
				}
				rq.Method = pos1 == 0 ? Method::Post : Method::Get;
			}
			if (!endHead) {
				rq.Header.append(buf, len);
			}
			else {
				rq.Temp.append(buf, len);
			}
			size_t headPos = rq.Header.find("\r\n\r\n");
			if (!endHead && headPos != String::npos) {
				//处理头部信息
				endHead = true;
				rq.Temp = rq.Header.substr(headPos + 4);
				rq.Header = rq.Header.substr(0, headPos);
				size_t pos = rq.Header.find("/");
				for (size_t i = pos; i < rq.Header.size() - pos; i++)
				{
					if (rq.Header.at(i) == ' ') {
						break;
					}
					RawUrl += rq.Header.at(i);
				}
				size_t what = RawUrl.find("?");
				rq.Url = what != String::npos ? RawUrl.substr(0, what) : RawUrl;
				rq.ParamString = what != String::npos ? RawUrl.substr(what + 1) : "";
				//查找绑定的函数
				if (rq.Method == Method::Get) {
					for (auto&it : GetFunc) {
						if (it.first == rq.Url) {
							handler = &it.second;
							break;
						}
					}
					//如果找不到绑定的函数 就看看是不是在请求文件
					if (handler == NULL) {
						String filename = WebRoot + "\\" + rq.Url;
						if (rq.Url == "/") {//如果什么都没有输入 直接重定向到index.html

							rp.Location = "/index.html";
						}
						if (File::Exists(filename)) {
							String ext = String(Path::GetExtension(filename)).Replace(".", "");
							if (ext == "html" || ext == "htm") {
								rp.ContentType = "text/html";
							}
							else if (ext == "js") {
								rp.ContentType = "application/javascript";
							}
							else if (ext == "jpg" || ext == "png" || ext == "bmp" || ext == "jpeg") {
								rp.ContentType = "image/" + ext;
							}
							else  if (ext == "ico") {
								rp.ContentType = "image/x-icon";
							}
							else {
								rp.ContentType = "application/octet-stream";
							}
							rp.Status = 200;
							rp.fileinfo = new FileSystem::FileInfo(filename);
						}
						else {
							rp.Status = 404;
						}
					}

					//如果是get方法直接跳出循环 不再接收数据
					break;
				}
				else if (rq.Method == Method::Post) {
					for (auto&it : PostFunc) {
						if (it.first == rq.Url) {
							handler = &it.second;
							break;
						}
					}
					if (handler == NULL) {
						rp.Status = 404;
						break;
					}
					//如果是POST方法取出content长度继续接收
					String contentLen;
					if (RegexValue(Text::ReplaceAll(rq.Header, " ", ""), "Content-Length:(\\d+)", contentLen)) {
						try
						{
							rq.ContentLength = std::stoi(contentLen);
						}
						catch (const std::exception&)
						{
							rq.ContentLength = -1;
						}
					}
				}
			}
			else {
				//读取到头部之后就直接返回
				break;
			}
			////如果取完数据之后跳出循环
			//if (rq.Method == Method::Post&&rq.ContentLength == rq.Body->size()) {
			//	break;
			//}
		}
		delete buf;
		if (rq.Method == Method::Get && rp.fileinfo) {
			String v;
			if (rq.GetHeader("Range", v)) {//断点续传
				v = v.Replace("bytes=", "");
				size_t rpos = v.find("-");
				if (rpos != size_t(-1)) {
					unsigned	long long r1 = std::stoll(v.substr(0, rpos));
					unsigned	long long r2 = std::stoll(v.substr(rpos + 1));
					if (r1 <= r2 && r2 <= rp.fileinfo->__stat.st_size) {
						rq.range.IsRange = true;
						rq.range.RangPos = r1;
						rq.range.RangEndPos = r2;
						rq.range.Total = rp.fileinfo->__stat.st_size;
					}
				}
			}
		}
		std::vector<String> vstr;
		rq.Header.Split("\r\n", vstr);
		for (auto kv = vstr.begin() + 1; vstr.size() >= 2 && kv != vstr.end(); kv++)
		{
			size_t mhPos = (*kv).find(":");
			rq.Headers.insert(std::pair<String, String>((*kv).substr(0, mhPos), (*kv).substr(mhPos + 2)));
		}

		//打印出请求头部看看
		printf("%s\n", rq.Header.c_str());
		HandleHeader(rq, rp, handler);
		HandleBody(rq, rp);
		rq.client.Close();
	}
	void HttpLinster::HandleHeader(Request&rq, Response&rp, HttpHandler *handler)
	{
		//先处理接口的响应
		if (handler) {
			//获取浏览器cookie
			rq.GetHeader("Cookie", rq.Cookie);
			//获取请求中的ContentType
			String contentType;
			if (rq.GetHeader("Content-Type", contentType)) {
				String boundaryStr = "boundary=";
				size_t bPos = contentType.find(boundaryStr);
				if (bPos != size_t(-1)) {
					boundaryStr = "--" + contentType.substr(bPos + boundaryStr.size()) + "\r\n";
					std::vector<String> forms;
					//rq.Body->Split(boundaryStr, forms);
					int asdad;
				}
			};

			(*handler)(rq, rp);
		}
		//响应协议头
		String header;
		if (rq.Method == Method::Get) {

			if (rq.range.IsRange &&rp.fileinfo) {
				//断点续传文件
				header.append("HTTP/1.1 206 \r\n");
				header.append("Accept-Ranges: bytes\r\n");
				header.append("Content-Range: bytes " + std::to_string(rq.range.RangPos) + "-" + std::to_string(rq.range.RangEndPos) + "/" + std::to_string(rq.range.Total) + "").append("\r\n");
				header.append("Content-Length: " + std::to_string(rq.range.RangEndPos - rq.range.RangPos) + "\r\n");
			}
			else
			{
				if (rp.fileinfo) {
					//获取文件最后修改的时间
					String lstChange = std::to_string(rp.fileinfo->__stat.st_mtime);
					String inm;
					if (rq.GetHeader("If-None-Match", inm) && inm.Trim() == lstChange) {
						//使用浏览器缓存
						header.append("HTTP/1.1 304 \r\n");
						header.append("ETag: " + lstChange + "\r\n");
						header.append("Accept-Ranges: bytes\r\n");
						rp.UseCache = true;
					}
					else {
						//普通传输文件
						header.append("HTTP/1.1 200 \r\n");
						header.append("Content-Length: " + std::to_string(rp.fileinfo->__stat.st_size) + "\r\n");
						header.append("ETag: " + lstChange + "\r\n");
						header.append("Accept-Ranges: bytes\r\n");
					}
				}
			}
		}
		//通用响应头
		if (!rp.Location.empty()) {
			//重定向响应
			header.append("HTTP/1.1 302 \r\n");
			header.append("Location: " + rp.Location + "\r\n");
		}
		//如果是写的接口使用这样返回
		if (rp.fileinfo == NULL) {
			header.append("HTTP/1.1 " + std::to_string(rp.Status) + " \r\n");
			header.append("Content-Length: " + std::to_string(rp.Body.size()) + "\r\n");
		}
		if (!rp.Cookie.empty()) {
			//path=/;max-age=1000
			header.append("Set-Cookie: " + rp.Cookie + "\r\n");
		}
		header.append("Content-Type: " + rp.ContentType + "\r\n");
		header.append("Server: LSkin Server 1.0 \r\n");
		header.append("\r\n");
		printf("---------------------------------------------------\n%s\n", header.c_str());//输出响应头部
		rq.client.Write(header.c_str(), header.size());//发送响应头部
	}
	void HttpLinster::HandleBody(Request&rq, Response&rp) {
		if (rp.fileinfo) {
			if (!rp.UseCache) {
				//普通响应文件
				char *buf2 = new char[DownSize];
				memset(buf2, 0, DownSize);
				size_t ct = 0;
				if (rq.range.IsRange) {
					rp.fileinfo->StreamPos = rq.range.RangPos;
				}
				//读取文件
				while ((ct = rp.fileinfo->Read(buf2, DownSize)) > 0)
				{
					if (rq.range.IsRange && rp.fileinfo->StreamPos >= rq.range.RangEndPos) {
						break;
					}
					//发送内存块
					if (ct != rq.client.Write(buf2, ct)) {
						printf("文件传输失败!\n");
						break;
					}
					memset(buf2, 0, DownSize);
				}
				delete buf2;
			}
		}
		else {
			rq.client.Write(rp.Body.c_str(), rp.Body.size());//普通响应body
		}
	}
}