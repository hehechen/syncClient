#include "../socket.h"
#include "../sysutil.h"
#include "../protobuf/filesync.pb.h"
#include <zlib.h>  // adler32
#include <arpa/inet.h>  // htonl, ntohl
#include <string>
#include <sstream>
using namespace std;
using namespace sysutil;

#define kHeaderLen 4
///
/// Encode protobuf Message to transport format defined above
/// returns a std::string.
///
/// returns a empty string if message.AppendToString() fails.
///
//inline std::string encode(const google::protobuf::Message& message)
//{
//  std::string result;

//  result.resize(kHeaderLen);

//  const std::string& typeName = message.GetTypeName();
//  int32_t nameLen = static_cast<int32_t>(typeName.size()+1);
//  int32_t be32 = ::htonl(nameLen);
//  result.append(reinterpret_cast<char*>(&be32), sizeof be32);
//  result.append(typeName.c_str(), nameLen);
//  bool succeed = message.AppendToString(&result);

//  if (succeed)
//  {
//    const char* begin = result.c_str() + kHeaderLen;
//    int32_t checkSum = adler32(1, reinterpret_cast<const Bytef*>(begin), result.size()-kHeaderLen);
//    int32_t be32 = ::htonl(checkSum);
//    result.append(reinterpret_cast<char*>(&be32), sizeof be32);

//    int32_t len = ::htonl(result.size() - kHeaderLen);
//    std::copy(reinterpret_cast<char*>(&len),
//              reinterpret_cast<char*>(&len) + sizeof len,
//              result.begin());
//  }
//  else
//  {
//    result.clear();
//  }

//  return result;
//}


//int main()
//{
//    TcpSocket socket;
//    int con_socket = socket.connect(string("127.0.0.1"),8888);
//    filesync::init msg;
//    msg.set_id(1);
//    msg.set_filename("hahahaha");
//    msg.set_md5("asdhflkhflanvofw");
//    msg.set_size(500);
//    string str;
//    int size = msg.ByteSize();
//    char *ch = (char *)&size;
//    str.append(ch,4);
//    cout<<endl;
//    write(con_socket,str.c_str(),str.size());
//    msg.SerializeToString(&str);
//    write(con_socket,str.c_str(),str.size());
//    return 0;
//}
