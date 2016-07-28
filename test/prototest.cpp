#include "../socket.h"
#include "../sysutil.h"
#include "../protobuf/filesync.init.pb.h"
using namespace std;
using namespace sysutil;

int main()
{
    TcpSocket socket;
    int con_socket = socket.connect(string("127.0.0.1"),8888);
    filesync::init msg;
    msg.set_id(1);
    msg.set_filename("hahahaha");
    msg.set_md5("asdhflkhflanvofw");
    msg.set_size(500);
    string str;
    msg.SerializeToString(&str);
    write(con_socket,str.c_str(),str.size());
    return 0;
}
