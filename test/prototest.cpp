#include "../socket.h"
#include "../sysutil.h"
#include "../protobuf/filesync.init.pb.h"
#include <string>
#include <sstream>
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
    int size = msg.ByteSize();
    char *ch = (char *)&size;
    str.append(ch,4);
    const char *hehe = str.c_str();
    for(int i=0;i<4;i++)
    {
        cout<<*hehe<<endl;
        hehe += 4;
    }
    cout<<endl;
    write(con_socket,str.c_str(),str.size());
    msg.SerializeToString(&str);
    write(con_socket,str.c_str(),str.size());
    return 0;
}
