#include "IMyService.h"
namespace android
{
    // 服务代理

    // add
    void BpMyService::add(int32_t a, int32_t b) {
        //数据打包
        Parcel data, reply;
        data.writeInterfaceToken(IMyService::getInterfaceDescriptor());
        data.writeInt32(a);
        data.writeInt32(b);

        //发送数据
        remote()->transact(1001, data, &reply, 0);
    }

     // sub
    void BpMyService::sub(int32_t a, int32_t b) {
        Parcel data, reply;
        data.writeInterfaceToken(IMyService::getInterfaceDescriptor());
        data.writeInt32(a);
        data.writeInt32(b);
        remote()->transact(1002, data, &reply, 0);
    }

    
    // 服务

    status_t MyService::onTransact(uint_t code, const Parcel& data, Parcel* reply, uint32_t flags) {
        switch (code) {
        case 1001: {
            CHECK_INTERFACE(IMyService, data, reply);//数据校验

            int32_t a = data.readInt32();//读取参数
            int32_t b= data.readInt32();

            int32_t result = add(a, b);//调用业务方法

            reply->writeInt32(result);//返回结果

            return NO_ERROR;
        }
            break;
        case 1002: {
            CHECK_INTERFACE(IMyService, data, reply);

            int32_t a = data.readInt32();
            int32_t b= data.readInt32();

            int32_t result = sub(a, b);

            reply->writeInt32(result);

            return NO_ERROR;
        }
            break;
        default:
            break;
        }
        return NO_ERROR;
    }

    // add
    void MyService::add(int32_t a, int32_t b)
        return a + b;
    };

    // sub
    void MyService::sub(int32_t a, int32_t b)
        return a - b;
    };
}