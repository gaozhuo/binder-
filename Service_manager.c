int main(int argc, char **argv)
{
    struct binder_state *bs;
    //打开binder驱动，并进行内存映射
    bs = binder_open(128*1024);

    //告诉驱动，我要成为ServiceManager
    binder_become_context_manager(bs);
     
    //进入阻塞状态，等待client发送数据
    binder_loop(bs, svcmgr_handler);

    return 0;
}





int svcmgr_handler(struct binder_state *bs,
                   struct binder_transaction_data *txn,
                   struct binder_io *msg,
                   struct binder_io *reply)
{
    struct svcinfo *si;
    uint16_t *s;//服务名称
    size_t len;
    uint32_t handle;
    uint32_t strict_policy;
    int allow_isolated;
   
    switch(txn->code) {
    case SVC_MGR_GET_SERVICE:  //对应于getService
    case SVC_MGR_CHECK_SERVICE:  //对应于checkService

        s = bio_get_string16(msg, &len);
    
        //根据服务名称去svclist服务列表中查找handle
        handle = do_find_service(bs, s, len, txn->sender_euid, txn->sender_pid);

        //结果放入reply中
        bio_put_ref(reply, handle);

        return 0;

    case SVC_MGR_ADD_SERVICE:  //对应于addService

        s = bio_get_string16(msg, &len);
        handle = bio_get_ref(msg);

        allow_isolated = bio_get_uint32(msg) ? 1 : 0;
        
        //将handle和服务名称添加到svclist服务列表
        do_add_service(bs, s, len, handle, txn->sender_euid, allow_isolated, txn->sender_pid)；
        break;
    default:
        return -1;
    }

    bio_put_uint32(reply, 0);
    return 0;
}