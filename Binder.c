static int binder_open(struct inode *nodp, struct file *filp)
{
    struct binder_proc *proc; // binder进程 【见附录3.1】

    proc = kzalloc(sizeof(*proc), GFP_KERNEL); // 为binder_proc结构体在分配kernel内存空间
   
    //下面主要是对binder_proc里面的成员进行初始化
    INIT_LIST_HEAD(&proc->todo); //初始化todo列表
    init_waitqueue_head(&proc->wait); //初始化wait队列
    // ......

    filp->private_data = proc; //将binder_proc对象保存到filp->private_data中

    return 0;
}