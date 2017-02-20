/**
*打开binder驱动
*/
static int binder_open(struct inode *nodp, struct file *filp)
{
    struct binder_proc *proc; // 表示进程

    proc = kzalloc(sizeof(*proc), GFP_KERNEL); // 为binder_proc分配内存空间
   
    //下面主要是对binder_proc里面的成员进行初始化
    INIT_LIST_HEAD(&proc->todo); //初始化todo列表
    init_waitqueue_head(&proc->wait); //初始化wait队列

    // ......

    filp->private_data = proc; //将binder_proc对象保存到filp->private_data中

    return 0;
}

/**
*内核中表示进程的结构体
*/
struct binder_proc {
	struct rb_root threads;//binder_thread红黑树的根节点，表示进程中的binder线程

	struct rb_root nodes;//binder_node红黑树的根节点

	struct rb_root refs_by_desc;//binder_ref红黑树的根节点(以handle为key)
	struct rb_root refs_by_node;//binder_ref红黑树的根节点（以binder_node为key）

	void *buffer;//内核缓存区地址
	ptrdiff_t user_buffer_offset;//内核空间与用户空间的地址偏移量
	size_t buffer_size;//内核缓存区大小
	
	struct list_head todo;//todo队列，即该进程需要处理的任务
	wait_queue_head_t wait;//等待队列，进程空闲时在此队列上休眠
};

/**
*内核中表示线程的结构体
*/
struct binder_thread {
	struct binder_proc *proc;//所属进程

	struct binder_transaction *transaction_stack;//正在处理的事务

	struct list_head todo;//todo队列，即该进程需要处理的任务
	wait_queue_head_t wait;//等待队列，进程空闲时在此队列上休眠
};