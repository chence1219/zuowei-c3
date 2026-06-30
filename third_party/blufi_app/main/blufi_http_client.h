#ifndef BLUFI_HTTP_CLIENT_H_
#define BLUFI_HTTP_CLIENT_H_

#include <esp_http_client.h>

#include <string>
#include <map>

namespace BlufiHttp {

class HttpClient {
 public:
  HttpClient();
  ~HttpClient();

  void SetHeader(const std::string& key, const std::string& value);
  bool Open(const std::string& method, const std::string& url, const std::string& content = "");
  void Close();

  int GetStatusCode() const;
  std::string GetResponseHeader(const std::string& key) const;
  size_t GetBodyLength() const;
  const std::string& GetBody();
  int Read(char* buffer, size_t buffer_size);

 private:
  esp_http_client_handle_t client_;
  std::map<std::string, std::string> headers_;
  std::string response_body_;
  int status_code_;
  int64_t content_length_;
};

} // namespace blufi

#endif  // BLUFI_HTTP_CLIENT_H_